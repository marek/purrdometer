// The setup helper's --selftest cases, one function per area (selftest_*.cpp).
#pragma once

#include "common/selftest.h"

#include <cstdlib>
#include <string>
#include <sys/stat.h>

// A scratch folder for the cases: PURR_TMP, or tmp in the checkout.
inline std::string selftestTmp()
{
    const char* env = std::getenv("PURR_TMP");
    std::string tmp = env ? env : "tmp";
    mkdir(tmp.c_str(), 0755);
    return tmp;
}

void selftestParsing(purr::Check& check);
void selftestCommand(purr::Check& check);
void selftestAuthorizedKeys(purr::Check& check);
void selftestApply(purr::Check& check);
void selftestSetupMode(purr::Check& check);
