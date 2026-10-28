#include "display/touch.h"

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace purr::display {

namespace {

constexpr uint16_t TOUCH_ADDR = 0x14;
constexpr uint16_t TOUCH_PRODUCT_ID = 0x8140, TOUCH_STATUS = 0x814E, TOUCH_POINT = 0x814F;
constexpr auto FINGER_UP_AFTER = std::chrono::milliseconds(300); // without reports

[[noreturn]] void fail(const std::string& what) { throw std::runtime_error(what + ": " + std::strerror(errno)); }

void sleepFor(std::chrono::milliseconds ms) { std::this_thread::sleep_for(ms); }

} // namespace

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

std::optional<TouchEvent> Touch::poll()
{
    uint8_t status = read(TOUCH_STATUS, 1)[0];
    auto now = std::chrono::steady_clock::now();
    if (!(status & 0x80)) {
        if (down_ && now - lastReport_ > FINGER_UP_AFTER) {
            down_ = false;
            return TouchEvent{TouchEvent::Kind::Up, lastX_, lastY_};
        }
        return std::nullopt;
    }
    lastReport_ = now;
    std::optional<TouchEvent> event;
    int count = status & 0x0F;
    if (count >= 1 && count <= 5) {
        auto p = read(TOUCH_POINT, 8);
        lastX_ = p[1] | (p[2] << 8);
        lastY_ = p[3] | (p[4] << 8);
        if (!down_)
            event = TouchEvent{TouchEvent::Kind::Down, lastX_, lastY_};
        down_ = true;
    } else if (down_) {
        down_ = false;
        event = TouchEvent{TouchEvent::Kind::Up, lastX_, lastY_};
    }
    write(TOUCH_STATUS, 0);
    return event;
}

} // namespace purr::display
