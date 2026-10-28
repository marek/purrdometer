// purrdometer-setup --selftest: the parts that need no root and no radio, and
// the state machine over a NetworkManager that is made to fail.

#include "setup/selftest.h"

int selftest()
{
    purr::Check check;
    selftestParsing(check);
    selftestCommand(check);
    selftestAuthorizedKeys(check);
    selftestApply(check);
    selftestSetupMode(check);
    return check.finish();
}
