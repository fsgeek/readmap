/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include "preload.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int open(const char *pathname, int flags, ...);
int creat(const char *pathname, mode_t mode);
int openat(int dirfd, const char *pathname, int flags, ...);
FILE *fopen(const char *pathname, const char *mode);
FILE *fdopen(int fd, const char *mode);
FILE *freopen(const char *pathname, const char *mode, FILE *stream);

int open(const char *pathname, int flags, ...)
{
    va_list args;
    mode_t mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    // readmap_preload_init();

    return readmap_open(pathname, flags, mode);
}

int creat(const char *pathname, mode_t mode)
{
    return readmap_creat(pathname, mode);
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    va_list args;
    mode_t mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    return readmap_openat(dirfd, pathname, flags, mode);
}

FILE *fopen(const char *pathname, const char *mode)
{
    return readmap_fopen(pathname, mode);
}

FILE *fdopen(int fd, const char *mode)
{
    return readmap_fdopen(fd, mode);
}

FILE *freopen(const char *pathname, const char *mode, FILE *stream)
{
    return readmap_freopen(pathname, mode, stream);
}
