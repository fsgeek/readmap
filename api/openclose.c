/*
 * (C) Copyright 2018 Tony Mason
 * All Rights Reserved
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>

#include "api-internal.h"
#include "callstats.h"

/*
 * REF:
 * https://rafalcieslak.wordpress.com/2013/04/02/dynamic-linker-tricks-using-ld_preload-to-cheat-inject-features-and-investigate-programs/
 *      https://github.com/poliva/ldpreloadhook/blob/master/hook.c
 */

struct map_name_args
{
    const char *mapfile_name;
    uuid_t *uuid;
    int *status;
};

static int fin_open(const char *pathname, int flags, ...)
{
    typedef int (*orig_open_t)(const char *pathname, int flags, ...);
    static orig_open_t orig_open = NULL;
    va_list args;
    mode_t mode;

    if (NULL == orig_open)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_open = (orig_open_t)dlsym(RTLD_NEXT, "open");
#pragma GCC diagnostic pop

        assert(NULL != orig_open);
        if (NULL == orig_open)
        {
            errno = EACCES;
            return -1;
        }
    }

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    return orig_open(pathname, flags, mode);
}

static int fin_openat(int dirfd, const char *pathname, int flags, ...)
{
    typedef int (*orig_openat_t)(int dirfd, const char *pathname, int flags, ...);
    static orig_openat_t orig_openat = NULL;
    va_list args;
    mode_t mode;

    if (NULL == orig_openat)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_openat = (orig_openat_t)dlsym(RTLD_NEXT, "openat");
#pragma GCC diagnostic pop

        assert(NULL != orig_openat);
        if (NULL == orig_openat)
        {
            errno = EACCES;
            return -1;
        }
    }

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    return orig_openat(dirfd, pathname, flags, mode);
}

static int fin_close(int fd)
{
    typedef int (*orig_close_t)(int fd);
    static orig_close_t orig_close = NULL;

    if (NULL == orig_close)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_close = (orig_close_t)dlsym(RTLD_NEXT, "close");
#pragma GCC diagnostic pop

        assert(NULL != orig_close);
        if (NULL == orig_close)
        {
            errno = EACCES;
            return -1;
        }
    }

    /* TODO: add the remove/delete logic for this file descriptor to path mapping */

    return orig_close(fd);
}

static int internal_open(const char *pathname, int flags, mode_t mode)
{
    int fd;

    fd = fin_open(pathname, flags, mode);

    if (fd < 0)
    {
        return fd;
    }

    // Note that if this failed (file_state is null) we don't care - that
    // just turns this into a fallback case.
    (void)readmap_create_file_state(fd, pathname, flags);

    return fd;
}

int readmap_open(const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    int result = -1;
    va_list args;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    result = internal_open(pathname, flags, mode);

    return result;
}

int readmap_creat(const char *pathname, mode_t mode)
{
    return readmap_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

static int internal_openat(int dirfd, const char *pathname, int flags, mode_t mode)
{
    int fd = -1;

    fd = fin_openat(dirfd, pathname, flags, mode);

    return fd;
}

int readmap_openat(int dirfd, const char *pathname, int flags, ...)
{
    va_list args;
    mode_t mode;

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    return internal_openat(dirfd, pathname, flags, mode);
}

int readmap_close(int fd)
{
    readmap_file_state_t *file_state = NULL;
    int status = 0;

    status = fin_close(fd);

    if (0 != status)
    {
        // call failed
        return status;
    }

    file_state = readmap_lookup_file_state(fd);

    if (NULL != file_state)
    {
        readmap_delete_file_state(file_state);
    }

    return 0;
}

static int fopen_mode_to_flags(const char *mode)
{
    int flags = -1;

    // first character is the primary mode
    switch (*mode)
    {
    case 'r':
        flags = O_RDONLY;
        break;
    case 'w':
        flags = O_WRONLY;
        break;
    case 'a':
        flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
    default:
        errno = EINVAL;
        return flags;
    }

    for (unsigned index = 0; index < 7; index++)
    { // 7 is a magic value from glibc
        if ('\0' == *(mode + index))
        {
            break; // NULL terminated
        }
        switch (*(mode + index))
        {
        default: // ignore
            continue;
            break;
        case '+':
            flags |= O_RDWR;
            break;
        case 'x':
            flags |= O_EXCL;
            break;
        case 'b':
        case 't':
            // no-op on UNIX/Linux
            break;
        case 'm':
            // memory mapped - doesn't matter
            break;
        case 'c':
            // internal to libc
            break;
        case 'e':
            flags |= O_CLOEXEC;
            break;
        }
    }

    return flags;
}

static FILE *fin_fopen(const char *pathname, const char *mode)
{
    typedef FILE *(*orig_fopen_t)(const char *pathname, const char *mode);
    static orig_fopen_t orig_fopen = NULL;

    if (NULL == orig_fopen)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_fopen = (orig_fopen_t)dlsym(RTLD_NEXT, "fopen");
#pragma GCC diagnostic pop

        assert(NULL != orig_fopen);
        if (NULL == orig_fopen)
        {
            errno = EACCES;
            return NULL;
        }
    }

    return orig_fopen(pathname, mode);
}

static FILE *fin_fdopen(int fd, const char *mode)
{
    typedef FILE *(*orig_fdopen_t)(int fd, const char *mode);
    static orig_fdopen_t orig_fdopen = NULL;

    if (NULL == orig_fdopen)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_fdopen = (orig_fdopen_t)dlsym(RTLD_NEXT, "fdopen");
#pragma GCC diagnostic pop

        assert(NULL != orig_fdopen);
        if (NULL == orig_fdopen)
        {
            errno = EACCES;
            return NULL;
        }
    }

    return orig_fdopen(fd, mode);
}

static FILE *fin_freopen(const char *pathname, const char *mode, FILE *stream)
{
    typedef FILE *(*orig_freopen_t)(const char *pathname, const char *mode, FILE *stream);
    static orig_freopen_t orig_freopen = NULL;

    if (NULL == orig_freopen)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_freopen = (orig_freopen_t)dlsym(RTLD_NEXT, "freopen");
#pragma GCC diagnostic pop

        assert(NULL != orig_freopen);
        if (NULL == orig_freopen)
        {
            errno = EACCES;
            return NULL;
        }
    }

    return orig_freopen(pathname, mode, stream);
}

FILE *readmap_fopen(const char *pathname, const char *mode)
{
    // TODO: add the readmap integration here
    // (1) send the request (just like open)
    // (2) if the open succeeds to the library, match it up with
    //     the response from FUSE
    // (3) Update the mapping table, if necessary - keep using the file descriptor (its inside FILE *)
    //
    // Something to consider: glibc supports options for these files to be _memory mapped_
    // Not sure if we need to handle that differently, or not
    //
    FILE *file;
    readmap_file_state_t *rms = NULL;
    int flags;

    flags = fopen_mode_to_flags(mode);
    assert(-1 != flags); // otherwise, it's an error

    // Let's do the open
    file = fin_fopen(pathname, mode);

    if (NULL == file)
    {
        return file;
    }

    // Open + lookup both worked, so we need to track the file descriptor
    // to uuid mapping.
    rms = readmap_create_file_state(fileno(file), pathname, flags);
    assert(NULL != rms); // if it failed, we'd need to release the name map

    return fin_fopen(pathname, mode);
}

FILE *readmap_fdopen(int fd, const char *mode)
{
    // TODO: do we need to trap this call at all?  The file descriptor already exists.
    return fin_fdopen(fd, mode);
}

static FILE *internal_freopen(const char *pathname, const char *mode, FILE *stream)
{
    readmap_file_state_t *rms = NULL;
    FILE *file = NULL;
    int fd = -1;
    int flags = 0;

    // TODO: this is going to change the file descriptor; I'm not sure we are going to see the
    // close call here, or if it will bypass us.  So, that needs to be determined.
    // (1) keep track of the existing fd;
    // (2) send the new name request (if appropriate)
    // (3) Match up the results with the tracking table.
    //
    // Keep in mind that we have four cases here:
    //   old readmap name  -> new readmap name
    //   old readmap name  -> new non-readmap name
    //   old non-readmap name -> new readmap name
    //   old non-readmap name -> new non-readmap name
    //
    // We care about all but the last of those cases
    // Need the fd, since this is an implicit close of the underlying file.
    fd = fileno(stream);

    // Get the existing state
    rms = readmap_lookup_file_state(fd);

    if (NULL == rms)
    {
        // pass-through
        file = fin_freopen(pathname, mode, stream);

        return file;
    }

    // save the original flags
    flags = rms->flags;

    // we have to tear this down
    readmap_delete_file_state(rms);
    rms = NULL;

    // invoke the underlying library implementation
    file = fin_freopen(pathname, mode, stream);

    if (NULL != file)
    {
        // create state for this file
        rms = readmap_create_file_state(fileno(file), pathname, flags);
        assert(NULL != rms); // if it failed, we'd need to release the name map
    }

    return file;
}

FILE *readmap_freopen(const char *pathname, const char *mode, FILE *stream)
{
    return internal_freopen(pathname, mode, stream);
}
