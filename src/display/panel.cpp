#include "display/panel.h"

#include <gpiod.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/spi/spidev.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace purr::display {

namespace {

constexpr const char* GPIO_CHIP_LABEL = "pinctrl-bcm2835";
constexpr uint32_t SPI_HZ = 10'000'000;
constexpr size_t SPI_CHUNK = 4096;            // spidev's default buffer size
constexpr uint16_t TOUCH_ADDR = 0x14;
constexpr uint16_t TOUCH_PRODUCT_ID = 0x8140, TOUCH_STATUS = 0x814E, TOUCH_POINT = 0x814F;
constexpr auto BUSY_TIMEOUT = std::chrono::seconds(10);
constexpr auto FINGER_UP_AFTER = std::chrono::milliseconds(300); // without reports

[[noreturn]] void fail(const std::string& what) { throw std::runtime_error(what + ": " + std::strerror(errno)); }

void sleepFor(std::chrono::milliseconds ms) { std::this_thread::sleep_for(ms); }

} // namespace

std::vector<uint8_t> packPanel(const Canvas& c, bool rotate180)
{
    std::vector<uint8_t> ram(size_t(PANEL_ROW_BYTES) * PANEL_H, 0xFF);
    for (int py = 0; py < PANEL_H; ++py)
        for (int px = 0; px < PANEL_W; ++px) {
            // Landscape is the panel turned a quarter clockwise (anticlockwise when flipped).
            int lx = rotate180 ? PANEL_H - 1 - py : py;
            int ly = rotate180 ? px : PANEL_W - 1 - px;
            if (c.black(lx, ly))
                ram[size_t(py) * PANEL_ROW_BYTES + px / 8] &= uint8_t(~(0x80 >> (px % 8)));
        }
    return ram;
}

std::pair<int, int> touchToScreen(int tx, int ty, bool rotate180)
{
    // Landscape is PANEL_H wide and PANEL_W tall.
    int x = PANEL_H - 1 - ty, y = tx;
    if (rotate180)
        return {PANEL_H - 1 - x, PANEL_W - 1 - y};
    return {x, y};
}

// -- GPIO --------------------------------------------------------------------

Gpio::Gpio()
{
    if (DIR* dir = opendir("/dev")) {
        while (dirent* e = readdir(dir)) {
            if (std::strncmp(e->d_name, "gpiochip", 8) != 0)
                continue;
            gpiod_chip* chip = gpiod_chip_open(("/dev/" + std::string(e->d_name)).c_str());
            if (!chip)
                continue;
            gpiod_chip_info* info = gpiod_chip_get_info(chip);
            bool match = info && std::strcmp(gpiod_chip_info_get_label(info), GPIO_CHIP_LABEL) == 0;
            gpiod_chip_info_free(info);
            if (match) {
                chip_ = chip;
                break;
            }
            gpiod_chip_close(chip);
        }
        closedir(dir);
    }
    if (!chip_)
        throw std::runtime_error(std::string("no GPIO chip labelled ") + GPIO_CHIP_LABEL);

    gpiod_line_settings* out = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(out, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(out, GPIOD_LINE_VALUE_ACTIVE);
    gpiod_line_settings* busy = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(busy, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(busy, GPIOD_LINE_BIAS_PULL_DOWN);
    gpiod_line_settings* irq = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(irq, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(irq, GPIOD_LINE_BIAS_PULL_DOWN);
    gpiod_line_settings_set_edge_detection(irq, GPIOD_LINE_EDGE_FALLING); // INT pulses low on touch

    const unsigned outs[] = {Pins::RST, Pins::DC, Pins::TOUCH_RST};
    const unsigned busyPin[] = {Pins::BUSY}, irqPin[] = {Pins::TOUCH_INT};
    gpiod_line_config* lines = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(lines, outs, 3, out);
    gpiod_line_config_add_line_settings(lines, busyPin, 1, busy);
    gpiod_line_config_add_line_settings(lines, irqPin, 1, irq);
    gpiod_request_config* req = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req, "purrdometer-display");
    lines_ = gpiod_chip_request_lines(chip_, req, lines);
    gpiod_request_config_free(req);
    gpiod_line_config_free(lines);
    gpiod_line_settings_free(out);
    gpiod_line_settings_free(busy);
    gpiod_line_settings_free(irq);
    if (!lines_)
        fail("cannot request the HAT's GPIO lines");
    events_ = gpiod_edge_event_buffer_new(16);
}

Gpio::~Gpio()
{
    if (events_)
        gpiod_edge_event_buffer_free(events_);
    if (lines_)
        gpiod_line_request_release(lines_);
    if (chip_)
        gpiod_chip_close(chip_);
}

void Gpio::set(unsigned pin, bool high)
{
    gpiod_line_request_set_value(lines_, pin, high ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

bool Gpio::get(unsigned pin) { return gpiod_line_request_get_value(lines_, pin) == GPIOD_LINE_VALUE_ACTIVE; }

int Gpio::touchFd() { return gpiod_line_request_get_fd(lines_); }

void Gpio::drainTouchEvents() { gpiod_line_request_read_edge_events(lines_, events_, 16); }

// -- panel -------------------------------------------------------------------

EPaper::EPaper(Gpio& gpio, const std::string& spidev) : gpio_(gpio)
{
    fd_ = open(spidev.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0)
        fail("cannot open " + spidev);
    uint8_t mode = SPI_MODE_0, bits = 8;
    uint32_t hz = SPI_HZ;
    if (ioctl(fd_, SPI_IOC_WR_MODE, &mode) < 0 || ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &hz) < 0)
        fail("cannot configure " + spidev);
}

EPaper::~EPaper()
{
    try {
        sleep();
    } catch (const std::exception&) {
    }
    if (fd_ >= 0)
        close(fd_);
}

void EPaper::data(const uint8_t* bytes, size_t n)
{
    gpio_.set(Pins::DC, true);
    for (size_t off = 0; off < n; off += SPI_CHUNK) {
        size_t len = std::min(SPI_CHUNK, n - off);
        if (::write(fd_, bytes + off, len) != ssize_t(len))
            fail("SPI write");
    }
}

void EPaper::command(uint8_t c, std::initializer_list<uint8_t> args)
{
    gpio_.set(Pins::DC, false);
    if (::write(fd_, &c, 1) != 1)
        fail("SPI write");
    if (args.size())
        data(args.begin(), args.size());
}

void EPaper::waitIdle()
{
    auto deadline = std::chrono::steady_clock::now() + BUSY_TIMEOUT;
    while (gpio_.get(Pins::BUSY)) {
        if (std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("e-paper stayed busy");
        sleepFor(std::chrono::milliseconds(10));
    }
}

void EPaper::reset(std::chrono::milliseconds hold)
{
    gpio_.set(Pins::RST, true);
    sleepFor(std::chrono::milliseconds(20));
    gpio_.set(Pins::RST, false);
    sleepFor(hold);
    gpio_.set(Pins::RST, true);
    sleepFor(std::chrono::milliseconds(20));
}

void EPaper::window()
{
    command(0x01, {0xF9, 0x00, 0x00});                       // driver output control
    command(0x11, {0x03});                                   // data entry mode
    command(0x44, {0x00, (PANEL_W - 1) >> 3});               // RAM x range
    command(0x45, {0x00, 0x00, (PANEL_H - 1) & 0xFF, (PANEL_H - 1) >> 8});
    command(0x4E, {0x00});                                   // RAM x counter
    command(0x4F, {0x00, 0x00});                             // RAM y counter
}

void EPaper::full(const std::vector<uint8_t>& ram)
{
    reset(std::chrono::milliseconds(2));
    waitIdle();
    command(0x12);                                           // software reset
    waitIdle();
    window();
    command(0x3C, {0x05});                                   // border waveform
    command(0x21, {0x00, 0x80});                             // display update control
    command(0x18, {0x80});                                   // internal temperature sensor
    waitIdle();
    command(0x24);
    data(ram.data(), ram.size());
    command(0x26);                                           // the base partial updates compare against
    data(ram.data(), ram.size());
    command(0x22, {0xF7});
    command(0x20);
    waitIdle();
    asleep_ = false;
}

void EPaper::partial(const std::vector<uint8_t>& ram)
{
    if (asleep_)
        return full(ram);
    reset(std::chrono::milliseconds(1));
    window();
    command(0x3C, {0x80});
    command(0x24);
    data(ram.data(), ram.size());
    command(0x22, {0xFF});
    command(0x20);
    waitIdle();
}

void EPaper::sleep()
{
    if (asleep_)
        return;
    command(0x10, {0x01});                                   // deep sleep; the image stays
    asleep_ = true;
}

// -- touch -------------------------------------------------------------------

Touch::Touch(Gpio& gpio, const std::string& i2cdev) : gpio_(gpio)
{
    fd_ = open(i2cdev.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0)
        fail("cannot open " + i2cdev);
    if (ioctl(fd_, I2C_SLAVE, TOUCH_ADDR) < 0)
        fail("cannot address the touch controller");
    for (bool level : {true, false, true}) {
        gpio_.set(Pins::TOUCH_RST, level);
        sleepFor(std::chrono::milliseconds(100));
    }
}

Touch::~Touch()
{
    if (fd_ >= 0)
        close(fd_);
}

std::vector<uint8_t> Touch::read(uint16_t reg, size_t n)
{
    uint8_t addr[2] = {uint8_t(reg >> 8), uint8_t(reg & 0xFF)};
    std::vector<uint8_t> out(n);
    i2c_msg msgs[2] = {{TOUCH_ADDR, 0, 2, addr}, {TOUCH_ADDR, I2C_M_RD, uint16_t(n), out.data()}};
    i2c_rdwr_ioctl_data xfer{msgs, 2};
    if (ioctl(fd_, I2C_RDWR, &xfer) < 0)
        fail("touch read");
    return out;
}

void Touch::write(uint16_t reg, uint8_t value)
{
    uint8_t buf[3] = {uint8_t(reg >> 8), uint8_t(reg & 0xFF), value};
    if (::write(fd_, buf, 3) != 3)
        fail("touch write");
}

std::string Touch::productId()
{
    auto id = read(TOUCH_PRODUCT_ID, 4);
    return {id.begin(), id.end()};
}

std::optional<std::pair<int, int>> Touch::poll()
{
    uint8_t status = read(TOUCH_STATUS, 1)[0];
    auto now = std::chrono::steady_clock::now();
    if (!(status & 0x80)) {
        if (down_ && now - lastReport_ > FINGER_UP_AFTER)
            down_ = false;
        return std::nullopt;
    }
    lastReport_ = now;
    std::optional<std::pair<int, int>> tap;
    int count = status & 0x0F;
    if (count >= 1 && count <= 5) {
        auto p = read(TOUCH_POINT, 8);
        if (!down_)
            tap = std::pair{p[1] | (p[2] << 8), p[3] | (p[4] << 8)};
        down_ = true;
    } else {
        down_ = false;
    }
    write(TOUCH_STATUS, 0);
    return tap;
}

} // namespace purr::display
