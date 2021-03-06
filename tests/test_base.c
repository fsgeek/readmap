/*
 * Copyright (c) 2017-2020, Tony Mason. All rights reserved.
 */

#include "readmap_test.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif //

MunitResult
test_null(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    return MUNIT_OK;
}

extern MunitSuite *SetupMunitSuites(void);

int main(
    int argc,
    char **argv)
{
    MunitSuite suite;

    suite.prefix = (char *)(uintptr_t) "/readmap";
    suite.tests = NULL;
    suite.suites = SetupMunitSuites();
    suite.iterations = 1;
    suite.options = MUNIT_SUITE_OPTION_NONE;

    return munit_suite_main(&suite, NULL, argc, argv);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
