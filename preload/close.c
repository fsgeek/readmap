/*
 * Copyright (c) 2021, Tony Mason. All rights reserved.
 */

#include "preload.h"

int close(int fd)
{
    return readmap_close(fd);
}
