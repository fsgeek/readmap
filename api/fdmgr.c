/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include <pthread.h>
#include <string.h>
#include <assert.h>
#include "api-internal.h"
#include "list.h"

#if !defined(offsetof)
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif // offsetof

/*
 * The purpose of the file descriptor manager is to provide a mechanism for mapping file descriptors to paths, since
 * the FUSE layer requires paths and not FDs.
 *
 * We do this via a table (of course!)
 */
#if !defined(container_of)
#define container_of(ptr, type, member) (type *)((char *)(ptr)-offsetof(type, member))
#endif // container_of

typedef uint32_t (*lookup_table_hash_t)(void *key, size_t length);

/*
 * Note: at the present time this is set up for a single lock on the table
 *       this should be sufficient for single process use, but if not,
 *       this could be split into per-bucket locks for greater parallelism.
 */
typedef struct lookup_table
{
    unsigned char EntryCountShift;
    unsigned char Name[7];
    pthread_rwlock_t TableLock; /* protects table against changes */
    lookup_table_hash_t Hash;
    size_t KeySize;
    struct list TableBuckets[1];
} lookup_table_t;

typedef struct lookup_table_entry
{
    struct list ListEntry;
    void *Object;
    unsigned char Key[1];
} lookup_table_entry_t, *plookup_table_entry_t;

static lookup_table_entry_t *lookup_table_entry_create(void *key, size_t key_size, void *object)
{
    size_t size = (offsetof(lookup_table_entry_t, Key) + key_size + 0x7) & ~0x7;
    lookup_table_entry_t *new_entry = malloc(size);

    while (NULL != new_entry)
    {
        new_entry->Object = object;
        memcpy(new_entry->Key, key, key_size);
        break;
    }

    return new_entry;
}

static void lookup_table_entry_destroy(lookup_table_entry_t *DeadEntry)
{
    free(DeadEntry);
}

/* simple generic hash */
static uint32_t default_hash(void *key, size_t length)
{
    uint32_t hash = ~0;
    const char *blob = (const char *)key;

    for (unsigned char index = 0; index < length; index += sizeof(uint32_t))
    {
        hash ^= *(const uint32_t *)&blob[index];
    }

    return hash;
}

static uint32_t lookup_table_hash(lookup_table_t *Table, void *Key)
{
    return (Table->Hash(Key, Table->KeySize) & ((1 << Table->EntryCountShift) - 1));
}

static lookup_table_t *lookup_table_create(unsigned int SizeHint, const char *Name, lookup_table_hash_t Hash, size_t KeySize)
{
    lookup_table_t *table = NULL;
    unsigned char entry_count_shift = 0;
    unsigned entrycount;

    if (SizeHint > 65536)
    {
        SizeHint = 65536;
    }

    while (((unsigned int)(1 << entry_count_shift)) < SizeHint)
    {
        entry_count_shift++;
    }

    entrycount = 1 << entry_count_shift;

    table = malloc(offsetof(struct lookup_table, TableBuckets) + (sizeof(struct list) * entrycount));

    while (NULL != table)
    {
        table->EntryCountShift = entry_count_shift;
        memcpy(table->Name, Name, 7);
        table->Hash = Hash ? Hash : default_hash;
        table->KeySize = KeySize;
        pthread_rwlock_init(&table->TableLock, NULL);

        for (unsigned index = 0; index < entrycount; index++)
        {
            table->TableBuckets[index].prv = table->TableBuckets[index].nxt = &table->TableBuckets[index];
        }
        break;
    }

    return table;
}

static void lookup_table_destroy(lookup_table_t *Table)
{
    unsigned bucket_index = 0;
    lookup_table_entry_t *table_entry;

    pthread_rwlock_wrlock(&Table->TableLock);

    for (bucket_index = 0; bucket_index < (unsigned)(1 << Table->EntryCountShift); bucket_index++)
    {
        while (!list_is_empty(&Table->TableBuckets[bucket_index]))
        {
            table_entry = container_of(list_head(&Table->TableBuckets[bucket_index]), struct lookup_table_entry, ListEntry);
            list_remove(&table_entry->ListEntry);
            lookup_table_entry_destroy(table_entry);
        }
    }

    pthread_rwlock_unlock(&Table->TableLock);

    free(Table);

    return;
}

static struct lookup_table_entry *lookup_table_locked(lookup_table_t *Table, void *Key)
{
    uint32_t bucket_index = lookup_table_hash(Table, Key);
    struct lookup_table_entry *table_entry = NULL;
    struct list *le;

    list_for_each(&Table->TableBuckets[bucket_index], le)
    {
        table_entry = container_of(le, struct lookup_table_entry, ListEntry);
        if (0 == memcmp(Key, table_entry->Key, Table->KeySize))
        {
            return table_entry;
        }
    }

    return NULL;
}

static int lookup_table_insert(lookup_table_t *Table, void *Key, void *Object)
{
    lookup_table_entry_t *entry = lookup_table_entry_create(Key, Table->KeySize, Object);
    int status = ENOMEM;
    uint32_t bucket_index = lookup_table_hash(Table, Key);
    struct lookup_table_entry *table_entry = NULL;

    while (NULL != entry)
    {
        pthread_rwlock_wrlock(&Table->TableLock);
        table_entry = lookup_table_locked(Table, Key);

        if (table_entry)
        {
            status = EEXIST;
        }
        else
        {
            list_insert_tail(&Table->TableBuckets[bucket_index], &entry->ListEntry);
            status = 0;
        }
        pthread_rwlock_unlock(&Table->TableLock);

        break;
    }

    if (0 != status)
    {
        if (NULL != entry)
        {
            free(entry);
            entry = NULL;
        }
    }

    return status;
}

static int lookup_table_lookup(lookup_table_t *Table, void *Key, void **Object)
{
    struct lookup_table_entry *entry;

    pthread_rwlock_rdlock(&Table->TableLock);
    entry = lookup_table_locked(Table, Key);
    pthread_rwlock_unlock(&Table->TableLock);

    if (entry)
    {
        *Object = entry->Object;
    }
    else
    {
        *Object = NULL;
    }

    return NULL == entry ? ENODATA : 0;
}

static int lookup_table_remove(lookup_table_t *Table, void *Key)
{
    struct lookup_table_entry *entry;
    int status = ENODATA;

    pthread_rwlock_wrlock(&Table->TableLock);
    entry = lookup_table_locked(Table, Key);

    if (entry)
    {
        list_remove(&entry->ListEntry);
    }
    pthread_rwlock_unlock(&Table->TableLock);

    if (entry)
    {
        lookup_table_entry_destroy(entry);
        status = 0;
    }

    return status;
}

/* local lookup table based on file descriptors */
/* static */ lookup_table_t *fd_lookup_table;

/*
 * Note: at the present time, the readmap_file_state_t structure is **Not** reference counted.
 *       Instead, it relies upon the open/close management logic to know when it is time
 *       to delete the state.
 */

readmap_file_state_t *readmap_create_file_state(int fd, const char *pathname, int flags)
{
    readmap_file_state_t *file_state = NULL;
    size_t size = sizeof(readmap_file_state_t);
    int status;
    struct stat st;

    (void)pathname;

    assert(fd_lookup_table);

    status = fstat(fd, &st);
    if (!S_ISREG(st.st_mode))
    {
        return NULL;
    }

    file_state = malloc(size);
    while (NULL != file_state)
    {
        file_state->fd = fd;
        file_state->check_size = 0;
        file_state->mapped = 0;
        file_state->flags = flags;
        file_state->mode = st.st_mode;
        file_state->map_location = 0;
        pthread_rwlock_init(&file_state->lock, NULL);
        file_state->hash = 0; // TODO

        file_state->cached_size = 0;
        status = clock_gettime(CLOCK_REALTIME_COARSE, &file_state->check_time);
        assert(0 == status);

        // Try to insert it
        status = lookup_table_insert(fd_lookup_table, &fd, file_state);

        if (0 != status)
        {
            // note: we *could* do a lookup in case of a collision, but we shouldn't need to do so
            free(file_state);
            file_state = NULL;
            break;
        }

        /* done */
        break;
    }

    return file_state;
}

readmap_file_state_t *readmap_lookup_file_state(int fd)
{
    readmap_file_state_t *file_state;
    int status;

    if (NULL == fd_lookup_table)
    {
        // This can happen during shutdown.
        return NULL;
    }

    assert(fd_lookup_table);
    status = lookup_table_lookup(fd_lookup_table, &fd, (void **)&file_state);

    if (0 != status)
    {
        file_state = NULL;
    }

    return file_state;
}

static inline void timespec_diff(struct timespec *begin, struct timespec *end, struct timespec *diff)
{
    struct timespec result = {.tv_sec = 0, .tv_nsec = 0};
    assert((end->tv_sec > begin->tv_sec) || ((end->tv_sec == begin->tv_sec) && end->tv_nsec >= begin->tv_nsec));
    result.tv_sec = end->tv_sec - begin->tv_sec;
    if (end->tv_nsec < begin->tv_nsec)
    {
        result.tv_sec--;
        result.tv_nsec = (long)1000000000 + end->tv_nsec - begin->tv_nsec;
    }
    *diff = result;
}

static void readmap_update_size(readmap_file_state_t *file_state)
{
    struct stat st;
    int status;

    assert(S_ISREG(file_state->mode)); // shouldn't be handling anything but files

    status = fstat(file_state->fd, &st);
    assert(0 == status);

    file_state->check_size = 0;
    file_state->cached_size = st.st_size;
    status = clock_gettime(CLOCK_REALTIME, &file_state->check_time);
    assert(0 == status);
}

size_t readmap_get_size(readmap_file_state_t *file_state)
{
    struct timespec now, diff;
    int status = clock_gettime(CLOCK_REALTIME, &now);
    size_t size;

    assert(0 == status);

    pthread_rwlock_rdlock(&file_state->lock);
    timespec_diff(&now, &file_state->check_time, &diff);
    size = file_state->cached_size;
    pthread_rwlock_unlock(&file_state->lock);

    if (diff.tv_sec > 0) // at least 1 second
    {
        pthread_rwlock_wrlock(&file_state->lock);
        readmap_update_size(file_state);
        size = file_state->cached_size;
        pthread_rwlock_unlock(&file_state->lock);
    }

    return size;
}

void readmap_delete_file_state(readmap_file_state_t *file_state)
{
    int fd;
    int status;

    assert(fd_lookup_table);
    fd = file_state->fd;
    status = lookup_table_remove(fd_lookup_table, &fd);
    assert(0 == status);
    if (0 != status)
    {
        return;
    }

    // cleanup the file state
    free(file_state);
}

int readmap_init_file_state_mgr(void)
{
    lookup_table_t *new_table = NULL;
    int status = 0;

    while (NULL == fd_lookup_table)
    {
        /*
         * table size: this is a speed/space trade-off.  I did a quick run with various power-of-two values with
         * a test of 64K file descriptors (which is the max on my linux box at this time for a single process).
         * I don't want to add dynamic resizing into the mix, though someone _could_ do so, if they thought it
         * worth the cost (I've seldom found it to be worthwhile).  Another approach would be to use a more
         * efficient secondary scheme, such as b-tree or AVL tree.  That still gives good parallelism across
         * buckets.
         *
         * My test iteratively created 64K entries, then looked each of them up 100 times.  I picked the value
         * that seemed to be a reasonable compromise between space and efficiency.
         *
         * 8192 -  2.420 seconds
         * 4096 -  3.022 seconds
         * 2048 -  4.050 seconds
         * 1024 -  7.654 seconds
         *  512 - 14.118 seconds
         *
         */
        new_table = lookup_table_create(4096, "readmapFD", NULL, sizeof(int));

        if (NULL == new_table)
        {
            status = ENOMEM;
            break;
        }

        if (!__sync_bool_compare_and_swap(&fd_lookup_table, NULL, new_table))
        {
            // presumably a race that we've lost
            lookup_table_destroy(new_table);
            new_table = NULL;
            status = EINVAL;
            break;
        }

        break;
    }

    return status;
}

void readmap_terminate_file_state_mgr(void)
{
    lookup_table_t *existing_table = fd_lookup_table;

    if (NULL != existing_table)
    {
        if (__sync_bool_compare_and_swap(&fd_lookup_table, existing_table, NULL))
        {
            lookup_table_destroy(existing_table);
        }
        /* else: don't do anything because it changed and someone else must be doing something */
    }
}
