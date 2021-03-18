/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "munit.h"
#include "readmap_test.h"

#if !defined(__notused)
#define __notused __attribute__((unused))
#endif  //

#define TEMPDIR "/tmp/readmap_test/"
const char dir_template[] = TEMPDIR "/XXXXXX";

static MunitResult test_open(const MunitParameter params[] __notused, void *prv __notused)
{
    char        tempdir[sizeof(dir_template) + 1];
    char *      tmpname;
    struct stat st;
    int         status;
    int         fd    = -1;
    int         apifd = -1;

    readmap_init();
    status = stat(TEMPDIR, &st);

    if ((status < 0) && (ENOENT == errno)) {
        status = mkdir(TEMPDIR, 0700);
        munit_assert(0 == status);
    }
    strcpy(tempdir, dir_template);
    tmpname = mkdtemp(tempdir);

    fprintf(stderr, "temp name is %s\n", tempdir);
    munit_assert(NULL != tmpname);

    tmpname = malloc(strlen(tempdir) + 8);
    munit_assert(NULL != tmpname);
    strcpy(tmpname, tempdir);
    strcat(tmpname, "/XXXXXX");

    fd = mkstemp(tmpname);
    fprintf(stderr, "opened file %s\n", tmpname);
    munit_assert(fd >= 0);

    status = posix_fallocate(fd, 0, 100 * 1024 * 1024);  // 100MB
    munit_assert(0 == status);

    apifd = readmap_open(tmpname, O_RDWR);
    munit_assert(apifd >= 0);

    readmap_shutdown();

    return MUNIT_OK;
}

static const MunitTest perf_tests[] = {
    TEST("/null", test_null, NULL),
    TEST("/open", test_open, NULL),
    TEST(NULL, NULL, NULL),
};

const MunitSuite readmap_suite = {
    .prefix     = (char *)(uintptr_t) "/basic",
    .tests      = (MunitTest *)(uintptr_t)perf_tests,
    .suites     = NULL,
    .iterations = 1,
    .options    = MUNIT_SUITE_OPTION_NONE,
};

static MunitSuite readmap_test_suites[10];

MunitSuite *SetupMunitSuites()
{
    memset(readmap_test_suites, 0, sizeof(readmap_test_suites));
    readmap_test_suites[0] = readmap_suite;
    return readmap_test_suites;
}
