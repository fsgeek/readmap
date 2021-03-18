/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include <mntent.h>
#include <pthread.h>
#include <string.h>

static void readmap_dummy_init(void);
static void readmap_real_init(void);
static void readmap_dummy_shutdown(void);
static void readmap_real_shutdown(void);

pthread_once_t readmap_initialized = PTHREAD_ONCE_INIT;

static void readmap_init_internal(void)
{
    readmap_initialized = 1;
    readmap_init_file_state_mgr();
}

void readmap_init(void)
{
    pthread_once(&readmap_initialized, readmap_init_internal);
}

void readmap_shutdown(void)
{
    static unsigned char shutdown_called;
    static pthread_mutex_t shutdown_lock = PTHREAD_MUTEX_INITIALIZER;

    if (0 == shutdown_called)
    {
        pthread_mutex_lock(&shutdown_lock);
        if (0 == shutdown_called)
        {
            readmap_terminate_file_state_mgr();
            shutdown_called = 1;
        }
        pthread_mutex_unlock(&shutdown_lock);
    }
}
