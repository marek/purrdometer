// Every file and directory the programs use, from the install directories the
// build sets (Makefile: PREFIX, SYSCONFDIR, LOCALSTATEDIR, RUNSTATEDIR,
// DATADIR, FONTDIR). The defaults are the Debian layout the package installs.
#pragma once

#ifndef PURR_SYSCONFDIR
#define PURR_SYSCONFDIR "/etc"
#endif
#ifndef PURR_LOCALSTATEDIR
#define PURR_LOCALSTATEDIR "/var"
#endif
#ifndef PURR_RUNSTATEDIR
#define PURR_RUNSTATEDIR "/run"
#endif
#ifndef PURR_DATADIR
#define PURR_DATADIR "/usr/share"
#endif
#ifndef PURR_FONTDIR
#define PURR_FONTDIR PURR_DATADIR "/fonts/truetype/dejavu"
#endif

namespace purr {

constexpr const char* CONFIG_DIR = PURR_SYSCONFDIR "/purrdometer";
constexpr const char* STATE_DIR = PURR_LOCALSTATEDIR "/lib/purrdometer"; // the purrdometer user's
constexpr const char* DATA_DIR = PURR_DATADIR "/purrdometer";
constexpr const char* SETUP_RUN_DIR = PURR_RUNSTATEDIR "/purrdometer-setup";
constexpr const char* FONT_DIR = PURR_FONTDIR;

constexpr const char* CONFIG_PATH = PURR_SYSCONFDIR "/purrdometer/purrdometer.conf";
constexpr const char* WHEELS_PATH = PURR_DATADIR "/purrdometer/wheels.conf";
constexpr const char* OVERRIDES_PATH = PURR_LOCALSTATEDIR "/lib/purrdometer/settings.conf"; // written by the web UI
constexpr const char* PROFILES_PATH = PURR_LOCALSTATEDIR "/lib/purrdometer/profiles.conf";  // written by the web UI
constexpr const char* DB_PATH = PURR_LOCALSTATEDIR "/lib/purrdometer/purrdometer.db";
constexpr const char* SENSOR_LOG_DIR = PURR_LOCALSTATEDIR "/lib/purrdometer/sensor-log";
constexpr const char* SENSOR_PATH = PURR_LOCALSTATEDIR "/lib/purrdometer/sensor";             // the first sensor heard
constexpr const char* SCAN_REQUEST_PATH = PURR_LOCALSTATEDIR "/lib/purrdometer/scan-request"; // touched by the web UI
constexpr const char* SETUP_REQUEST_PATH = PURR_LOCALSTATEDIR "/lib/purrdometer/setup-request";
constexpr const char* SETUP_APPLY_PATH = PURR_LOCALSTATEDIR "/lib/purrdometer/setup-apply";
constexpr const char* SETUP_STATE_PATH = PURR_RUNSTATEDIR "/purrdometer-setup/state";
constexpr const char* SETUP_NETWORKS_PATH = PURR_RUNSTATEDIR "/purrdometer-setup/networks";

} // namespace purr
