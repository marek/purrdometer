// The logger's --selftest cases, one function per area (selftest_*.cpp).
#pragma once

#include "common/selftest.h"

void selftestAccounting(purr::Check& check);
void selftestStore(purr::Check& check);
void selftestLink(purr::Check& check);
void selftestRawLog(purr::Check& check);
