#include "display/epaper.h"

#include <linux/spi/spidev.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace purr::display {

namespace {

constexpr uint32_t SPI_HZ = 10'000'000;
constexpr size_t SPI_CHUNK = 4096; // spidev's default buffer size
constexpr auto BUSY_TIMEOUT = std::chrono::seconds(10);

[[noreturn]] void fail(const std::string& what) { throw std::runtime_error(what + ": " + std::strerror(errno)); }

void sleepFor(std::chrono::milliseconds ms) { std::this_thread::sleep_for(ms); }

} // namespace

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

} // namespace purr::display
