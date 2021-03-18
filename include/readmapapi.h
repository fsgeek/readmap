//
// This is the readmap API
//
// (C) Copyright 2021 Tony Mason
// All Rights Reserved
//

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <errno.h>
#include <stdio.h>

void readmap_init(void);
int readmap_open(const char *pathname, int flags, ...);
int readmap_creat(const char *pathname, mode_t mode);
int readmap_openat(int dirfd, const char *pathname, int flags, ...);
int readmap_close(int fd);
FILE *readmap_fopen(const char *pathname, const char *mode);
FILE *readmap_fdopen(int fd, const char *mode);
FILE *readmap_freopen(const char *pathname, const char *mode, FILE *stream);
