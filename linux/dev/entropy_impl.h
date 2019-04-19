
#ifndef _LINUX_DEV_ENTROPY_IMPL_H_
#define _LINUX_DEV_ENTROPY_IMPL_H_

/*
 * This code copyright (c) 2002, 2003 Derek L Davies (ddavies@ddavies.net)
 */

#include <oskit/dev/osenv_mem.h>
#include <oskit/dev/entropy.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include "glue.h"

typedef struct entropy_impl {
    /* COM interface */
    oskit_entropy_t ie;
    unsigned int count;
    oskit_entropy_stats_t stats;
    oskit_stream_t noblock;
    oskit_stream_t good;
    struct file fs;
    struct dentry des;
    struct inode in;
    struct driver_struct *ds;
#ifndef KNIT
    oskit_osenv_mem_t *mem;
#endif
} entropy_impl_t;

#endif
