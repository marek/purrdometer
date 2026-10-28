#include "display/gpio.h"

#include <gpiod.h>

#include <cerrno>
#include <dirent.h>
#include <cstring>
#include <stdexcept>
#include <string>

namespace purr::display {

namespace {

constexpr const char* GPIO_CHIP_LABEL = "pinctrl-bcm2835";

[[noreturn]] void fail(const std::string& what) { throw std::runtime_error(what + ": " + std::strerror(errno)); }

} // namespace

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

} // namespace purr::display
