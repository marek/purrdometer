// purrdometer-logger --selftest: accounting, parsing, the store and the link's
// ownership, with no Bluetooth.

#include "logger/selftest.h"

int selftest()
{
    purr::Check check;
    selftestAccounting(check);
    selftestStore(check);
    selftestLink(check);
    selftestRawLog(check);
    return check.finish();
}
