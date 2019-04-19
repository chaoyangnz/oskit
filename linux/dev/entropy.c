#include <oskit/c/assert.h>
#include <oskit/dev/error.h>
#include <oskit/dev/linux.h>
#include <oskit/com/stream.h>
#include <oskit/com/listener.h>
#include <oskit/com/listener_mgr.h>
#include <oskit/io/asyncio.h>
#include <oskit/dev/entropy.h>
#include <oskit/dev/entropy_channel.h>
#include "glue.h"
#include "entropy.h"
#include "entropy_driver.h"
#include "entropy_impl.h"
#include <linux/malloc.h>

/*
  Copyright (c) 2002, 2003 Derek L Davies (ddavies@ddavies.net)
*/

/*#if LINUX_RANDOM_DEVICE*/
#include <linux/random.h>
int random_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
extern ssize_t random_read(struct file *, char *, size_t, loff_t *);
extern ssize_t random_read_unlimited(struct file *, char *, size_t, loff_t *);
extern ssize_t random_write(struct file *, const char *, size_t, loff_t *);
/*#endif*/

struct device *entropy_dev_base;

struct oskit_guid oskit_linux_entropy_iids[ENTROPY_NIIDS] = {
    OSKIT_DEVICE_IID,
    OSKIT_DRIVER_IID,
    OSKIT_ENTROPY_IID
};

typedef struct entropy_noblock_stream_impl {
    oskit_stream_t is;
    unsigned int count;
    entropy_impl_t *ei;
    oskit_osenv_mem_t *mem;
} entropy_noblock_stream_impl_t;

typedef struct entropy_good_stream_impl {
    oskit_stream_t is;
    unsigned int count;
    entropy_impl_t *ei;
    oskit_osenv_mem_t *mem;
} entropy_good_stream_impl_t;

typedef struct entropy_good_asyncio_impl {
    oskit_asyncio_t ia;
    unsigned int count;
    entropy_impl_t *ei;
    oskit_osenv_mem_t *mem;
    oskit_u32_t selmask;
    struct listener_mgr *readers;
} entropy_good_asyncio_impl_t;

static struct oskit_stream_ops noblockstreamops;
static struct oskit_stream_ops goodstreamops;
static struct oskit_asyncio_ops goodasyncioops;

static OSKIT_COMDECL
entropy_query(oskit_entropy_t *e, const oskit_iid_t *iid, void **out_ihandle)
{
    if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	memcmp(iid, &oskit_driver_iid, sizeof(*iid)) == 0 ||
	memcmp(iid, &oskit_device_iid, sizeof(*iid)) == 0 ||
	memcmp(iid, &oskit_entropy_iid, sizeof(*iid)) == 0) {
	*out_ihandle = e;
	oskit_entropy_addref(e);
	return 0;
    }

    *out_ihandle = 0;
    return OSKIT_E_NOINTERFACE;
};

static OSKIT_COMDECL_U
entropy_addref(oskit_entropy_t *e)
{
    entropy_impl_t *ei = (entropy_impl_t *)e;
    return ++(ei->count);
}

static OSKIT_COMDECL_U
entropy_release(oskit_entropy_t *e)
{
    entropy_impl_t *ei = (entropy_impl_t *)e;
    
    /* We never actually free the entropy pool(s),
       it exists for the life of the kernel.
       Memory is allocated in the entropy probe
       function, below.
    */
    if (ei->count > 0) {
	(ei->count)--;
    }
    return ei->count;
}

static OSKIT_COMDECL
entropy_getinfo(oskit_entropy_t *e, oskit_devinfo_t *out_info)
{
    entropy_impl_t* ei = (entropy_impl_t*)e;
    *out_info = ei->ds->info;
    return 0;
}

static OSKIT_COMDECL
entropy_getdriver(oskit_entropy_t *e, oskit_driver_t **out_driver)
{
    entropy_impl_t* ei = (entropy_impl_t*)e;
    *out_driver = (oskit_driver_t*)&(ei->ds->drvi);
    oskit_driver_addref(*out_driver);
    return 0;
}

static OSKIT_COMDECL
entropy_getstats(oskit_entropy_t *e, oskit_entropy_stats_t *out_stats)
{
    entropy_impl_t* ei = (entropy_impl_t*)e;

    /* Update getstats call count.
    */
    (ei->stats.n_calls)++;
    out_stats->n_calls = ei->stats.n_calls;

/*#ifdef LINUX_RANDOM_POOL*/
    {
	/* Obtain estimated entropy.  The first arg, a struct inode *,
	   is never actually used in the ioctl routine.
	*/
	random_ioctl(NULL, &(ei->fs), RNDGETENTCNT,
		     (unsigned long)(&(ei->stats.entropy_count)));
    }
/*#endif*/

    out_stats->entropy_count = ei->stats.entropy_count;
    
    return 0;
}

static OSKIT_COMDECL
entropy_attach_source(oskit_entropy_t *e, char *name,
		      oskit_u32_t type, oskit_u32_t flags,
		      oskit_entropy_channel_t **out_chan)
{
    entropy_impl_t *ei = (entropy_impl_t *)e;
    oskit_entropy_channel_t *c;
    oskit_error_t err;

    err = create_entropy_channel(ei->mem, e, &c);
    if (err) return err;

    *out_chan = c;
    return 0;
}

static OSKIT_COMDECL
entropy_open_nonblocking(oskit_entropy_t *e, oskit_stream_t **out_stream_obj)
{
    entropy_impl_t *ei = (entropy_impl_t *)e;
    entropy_noblock_stream_impl_t *si;
    
    osenv_log(OSENV_LOG_DEBUG, "DLD: open_good ei is %p\n", ei);

    si = (entropy_noblock_stream_impl_t *)oskit_osenv_mem_alloc(ei->mem,
								sizeof(*si),
								0, 0);
    if (si == NULL)
	return NULL;
    memset(si, 0, sizeof(*si));

    osenv_log(OSENV_LOG_DEBUG, "DLD: open_good si is %p\n", si);

    si->count = 1;
    si->is.ops = &noblockstreamops;

    /* FIXME: Do I need to refcount mem too? */
    si->mem = ei->mem;

    /* FIXME: Do I need to refcount ei too? */
    si->ei = ei;
    
    *out_stream_obj = (oskit_stream_t *)si;

    return 0;
}

static OSKIT_COMDECL
entropy_open_good(oskit_entropy_t *e, oskit_stream_t **out_stream_obj,
		  oskit_asyncio_t **out_asyncio_obj)
{
    entropy_impl_t *ei = (entropy_impl_t *)e;
    entropy_good_stream_impl_t *si;
    entropy_good_asyncio_impl_t *ai;

    osenv_log(OSENV_LOG_DEBUG, "DLD: open_good ei is %p\n", ei);
    
    si = (entropy_good_stream_impl_t *)oskit_osenv_mem_alloc(ei->mem,
							     sizeof(*si),
							     0, 0);
    if (si == NULL)
	return NULL;
    memset(si, 0, sizeof(*si));

    osenv_log(OSENV_LOG_DEBUG, "DLD: open_good si is %p\n", si);

    si->count = 1;
    si->is.ops = &goodstreamops;

    /* FIXME: Do I need to refcount mem too? */
    si->mem = ei->mem;

    /* FIXME: Do I need to refcount ei too? */
    si->ei = ei;

    *out_stream_obj = (oskit_stream_t *)si;

    ai = (entropy_good_asyncio_impl_t *)oskit_osenv_mem_alloc(ei->mem,
							      sizeof(*ai),
							      0, 0);
    if (ai == NULL)
    {
	oskit_osenv_mem_t *mem = si->mem;
	oskit_osenv_mem_free(mem, (void*)si, 0, sizeof(*si));
	return NULL;
    }
    memset(ai, 0, sizeof(*ai));

    osenv_log(OSENV_LOG_DEBUG, "DLD: open_good ai is %p\n", ai);
    
    ai->count = 1;
    ai->ia.ops = &goodasyncioops;

    /* Always writable. */
    ai->selmask = OSKIT_ASYNCIO_WRITABLE;

    /* FIXME: Do I need to refcount mem too? */
    ai->mem = ei->mem;

    /* FIXME: Do I need to refcount ei too? */
    ai->ei = ei;

    *out_asyncio_obj = (oskit_asyncio_t *)ai;

    return 0;
}

struct oskit_entropy_ops
oskit_linux_entropy_ops = {
    entropy_query, entropy_addref, entropy_release,
    entropy_getinfo, entropy_getdriver, entropy_getstats,
    entropy_attach_source, entropy_open_nonblocking,
    entropy_open_good
};

static OSKIT_COMDECL_U
entropy_noblock_stream_addref(oskit_stream_t *);

static OSKIT_COMDECL
entropy_noblock_stream_query(oskit_stream_t *s, const oskit_iid_t *iid,
			     void **out_ihandle)
{
    if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	memcmp(iid, &oskit_stream_iid, sizeof(*iid)) == 0) {
	*out_ihandle = s;
	entropy_noblock_stream_addref(s);
	return 0;
    }

    *out_ihandle = 0;
    return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
entropy_noblock_stream_addref(oskit_stream_t *s)
{
    entropy_noblock_stream_impl_t *si = (entropy_noblock_stream_impl_t *)s;
    return ++si->count;
}

static OSKIT_COMDECL_U
entropy_noblock_stream_release(oskit_stream_t *s)
{
    entropy_noblock_stream_impl_t *si = (entropy_noblock_stream_impl_t *)s;
    unsigned int newcount;

    if (si == NULL)
	osenv_panic("%s:%d: null entropy_noblock_stream_impl_t",
		    __FILE__, __LINE__);
    if (si->count == 0)
	osenv_panic("%s:%d: bad count", __FILE__, __LINE__);

    if ((newcount = --si->count) == 0) {
	oskit_osenv_mem_t *mem = si->mem;
	oskit_osenv_mem_free(mem, (void*)si, 0, sizeof(*si));
    }
    
    return newcount;
}

static OSKIT_COMDECL
entropy_noblock_stream_read(oskit_stream_t *s, void* buf, oskit_u32_t len,
			    oskit_u32_t *out_actual)
{
    entropy_noblock_stream_impl_t *si = (entropy_noblock_stream_impl_t *)s;
    entropy_impl_t *ei = si->ei;
    
/*#ifdef LINUX_RANDOM_POOL*/
    /* The fourth argument to random_read_unlimited, ppos, a 'loff_t *', is
       unused in the linux 2.2.12 implementation.
    */
    *out_actual = (oskit_u32_t)random_read_unlimited(&(ei->fs), (char *)buf,
						     (size_t)len, NULL);
/*#endif*/

    return 0;
}

static OSKIT_COMDECL
entropy_noblock_stream_write(oskit_stream_t *s, const void *buf,
			     oskit_u32_t len, oskit_u32_t * out_actual)
{
    entropy_noblock_stream_impl_t *si = (entropy_noblock_stream_impl_t *)s;
    entropy_impl_t *ei = si->ei;
    int err;

/*#ifdef LINUX_RANDOM_POOL*/
    /* The fourth argument to random_write, ppos, a 'loff_t *', is unused in
       the linux 2.2.12 implementation.
    */
    *out_actual = (oskit_u32_t)random_write(&(ei->fs), (const char *)buf,
					    (size_t)len, NULL);

    /* Up the entropy bit count.  The linux random device does not do this
       for us.

       FIXME: We give it one bit of entropy for each byte (which is totally
       bogus, of course).
    */
    err = random_ioctl(NULL, &(ei->fs), RNDADDTOENTCNT, (unsigned long)out_actual);

/*#endif*/
    return 0;
}

static OSKIT_COMDECL
entropy_noblock_stream_seek(oskit_stream_t *s, oskit_s64_t ofs,
			    oskit_seek_t whence, oskit_u64_t * out_newpos)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_noblock_stream_setsize(oskit_stream_t *s, oskit_u64_t new_size)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_noblock_stream_copyto(oskit_stream_t *s, oskit_stream_t *dst,
			      oskit_u64_t size, oskit_u64_t *out_read,
			      oskit_u64_t *out_written)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_noblock_stream_commit(oskit_stream_t *s, oskit_u32_t commit_flags)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_noblock_stream_revert(oskit_stream_t *s)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_noblock_stream_lockregion(oskit_stream_t *s, oskit_u64_t offset,
				  oskit_u64_t size, oskit_u32_t lock_type)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_noblock_stream_unlockregion(oskit_stream_t *s, oskit_u64_t offset,
				    oskit_u64_t size, oskit_u32_t lock_type)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_noblock_stream_stat(oskit_stream_t *s, oskit_stream_stat_t *out_stat,
			    oskit_u32_t stat_flags)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_noblock_stream_clone(oskit_stream_t *s, oskit_stream_t **out_stream)
{
    return OSKIT_E_NOTIMPL;
}

static struct oskit_stream_ops noblockstreamops =
{
    entropy_noblock_stream_query,
    entropy_noblock_stream_addref,
    entropy_noblock_stream_release,
    entropy_noblock_stream_read,
    entropy_noblock_stream_write,
    entropy_noblock_stream_seek,
    entropy_noblock_stream_setsize,
    entropy_noblock_stream_copyto,
    entropy_noblock_stream_commit,
    entropy_noblock_stream_revert,
    entropy_noblock_stream_lockregion,
    entropy_noblock_stream_unlockregion,
    entropy_noblock_stream_stat,
    entropy_noblock_stream_clone
};

static OSKIT_COMDECL_U
entropy_good_stream_addref(oskit_stream_t *);

static OSKIT_COMDECL
entropy_good_stream_query(oskit_stream_t *s, const oskit_iid_t *iid,
			  void **out_ihandle)
{
    if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	memcmp(iid, &oskit_stream_iid, sizeof(*iid)) == 0) {
	*out_ihandle = s;
	entropy_good_stream_addref(s);
	return 0;
    }

    *out_ihandle = 0;
    return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
entropy_good_stream_addref(oskit_stream_t *s)
{
    entropy_good_stream_impl_t *si = (entropy_good_stream_impl_t *)s;
    return ++si->count;
}

static OSKIT_COMDECL_U
entropy_good_stream_release(oskit_stream_t *s)
{
    entropy_good_stream_impl_t *si = (entropy_good_stream_impl_t *)s;
    unsigned int newcount;

    if (si == NULL)
	osenv_panic("%s:%d: null entropy_good_stream_impl_t",
		    __FILE__, __LINE__);
    if (si->count == 0)
	osenv_panic("%s:%d: bad count",
		    __FILE__, __LINE__);

    if ((newcount = --si->count) == 0) {
	oskit_osenv_mem_t *mem = si->mem;
	oskit_osenv_mem_free(mem, (void*)si, 0, sizeof(*si));
    }
    
    return newcount;
}

static OSKIT_COMDECL
entropy_good_stream_read(oskit_stream_t *s, void* buf, oskit_u32_t len,
			 oskit_u32_t *out_actual)
{
    entropy_good_stream_impl_t *si = (entropy_good_stream_impl_t *)s;
    entropy_impl_t *ei = si->ei;

/*#ifdef LINUX_RANDOM_POOL*/
    /* The fourth argument to random_read, ppos, a 'loff_t *', is unused in the
       linux 2.2.12 implementation.
    */
    *out_actual = (oskit_u32_t)random_read(&(ei->fs), (char *)buf,
					   (size_t)len, NULL);
/*#endif*/

    return 0;
}

static OSKIT_COMDECL
entropy_good_stream_write(oskit_stream_t *s, const void *buf, oskit_u32_t len,
			  oskit_u32_t * out_actual)
{
    entropy_good_stream_impl_t *si = (entropy_good_stream_impl_t *)s;
    entropy_impl_t *ei = si->ei;
    int err;

/*#ifdef LINUX_RANDOM_POOL*/
    /* The fourth argument to random_write, ppos, a 'loff_t *', is unused in
       the linux 2.2.12 implementation.
    */
    *out_actual = (oskit_u32_t)random_write(&(ei->fs), (char *)buf,
					    (size_t)len, NULL);

    /* Up the entropy bit count.  The linux random device does not do this
       for us.

       FIXME: We give it one bit of entropy for each byte (which is totally
       bogus, of course).
    */
    err = random_ioctl(NULL, &(ei->fs), RNDADDTOENTCNT, (unsigned long)out_actual);
/*#endif*/

    return 0;
}

static OSKIT_COMDECL
entropy_good_stream_seek(oskit_stream_t *s, oskit_s64_t ofs,
			 oskit_seek_t whence, oskit_u64_t * out_newpos)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_good_stream_setsize(oskit_stream_t *s, oskit_u64_t new_size)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_good_stream_copyto(oskit_stream_t *s, oskit_stream_t *dst,
			   oskit_u64_t size, oskit_u64_t *out_read,
			   oskit_u64_t *out_written)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_good_stream_commit(oskit_stream_t *s, oskit_u32_t commit_flags)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_good_stream_revert(oskit_stream_t *s)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_good_stream_lockregion(oskit_stream_t *s, oskit_u64_t offset,
			       oskit_u64_t size, oskit_u32_t lock_type)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_good_stream_unlockregion(oskit_stream_t *s, oskit_u64_t offset,
				 oskit_u64_t size, oskit_u32_t lock_type)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_good_stream_stat(oskit_stream_t *s, oskit_stream_stat_t *out_stat,
			 oskit_u32_t stat_flags)
{
    return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
entropy_good_stream_clone(oskit_stream_t *s, oskit_stream_t **out_stream)
{
    return OSKIT_E_NOTIMPL;
}

static struct oskit_stream_ops goodstreamops =
{
    entropy_good_stream_query,
    entropy_good_stream_addref,
    entropy_good_stream_release,
    entropy_good_stream_read,
    entropy_good_stream_write,
    entropy_good_stream_seek,
    entropy_good_stream_setsize,
    entropy_good_stream_copyto,
    entropy_good_stream_commit,
    entropy_good_stream_revert,
    entropy_good_stream_lockregion,
    entropy_good_stream_unlockregion,
    entropy_good_stream_stat,
    entropy_good_stream_clone
};

static OSKIT_COMDECL_U
entropy_good_asyncio_addref(oskit_asyncio_t *);

static OSKIT_COMDECL
entropy_good_asyncio_query(oskit_asyncio_t *a, const oskit_iid_t *iid,
			   void **out_ihandle)
{
    if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	memcmp(iid, &oskit_asyncio_iid, sizeof(*iid)) == 0) {
	*out_ihandle = a;
	entropy_good_asyncio_addref(a);
	return 0;
    }

    *out_ihandle = 0;
    return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
entropy_good_asyncio_addref(oskit_asyncio_t *a)
{
    entropy_good_asyncio_impl_t *ai = (entropy_good_asyncio_impl_t *)a;
    return ++ai->count;
}

static OSKIT_COMDECL_U
entropy_good_asyncio_release(oskit_asyncio_t *a)
{
    entropy_good_asyncio_impl_t *ai = (entropy_good_asyncio_impl_t *)a;
    unsigned int newcount;

    if (ai == NULL)
	osenv_panic("%s:%d: null entropy_good_asyncio_impl_t",
		    __FILE__, __LINE__);
    if (ai->count == 0)
	osenv_panic("%s:%d: bad count", __FILE__, __LINE__);

    if ((newcount = --ai->count) == 0) {
	oskit_osenv_mem_t *mem = ai->mem;
	oskit_osenv_mem_free(mem, (void*)ai, 0, sizeof(*ai));
    }
    
    return newcount;
}

static int
entropy_is_readable(oskit_entropy_t *e)
{
    oskit_entropy_stats_t stats;
    oskit_entropy_getstats(e, &stats);
    return stats.entropy_count > 0;
}

static OSKIT_COMDECL
entropy_good_asyncio_poll(oskit_asyncio_t *a)
{
    entropy_good_asyncio_impl_t *ai = (entropy_good_asyncio_impl_t *)a;
    oskit_entropy_t *e = (oskit_entropy_t *)ai->ei;
    oskit_error_t mask = OSKIT_ASYNCIO_WRITABLE;
    int enabled;

    enabled = osenv_intr_save_disable();
    
    if (entropy_is_readable(e))
	mask |= OSKIT_ASYNCIO_READABLE;
    
    if (enabled)
	osenv_intr_enable();
    
    return mask;
}

static OSKIT_COMDECL
entropy_good_asyncio_add_listener(oskit_asyncio_t *a, struct oskit_listener *l,
				  oskit_s32_t mask)
{
    entropy_good_asyncio_impl_t *ai = (entropy_good_asyncio_impl_t *)a;
    oskit_error_t retmask = OSKIT_ASYNCIO_WRITABLE;
    int enabled;
#if 0 /* UNIX */
    unsigned int iotype = 0;
#endif

    enabled = osenv_intr_save_disable();

    retmask = entropy_good_asyncio_poll(a);

    oskit_listener_mgr_add(ai->readers, l);    

#if 0 /* UNIX */
    if (mask & OSKIT_ASYNCIO_READABLE) {
	iotype |= IOTYPE_READ;
    }
    if (mask & OSKIT_ASYNCIO_WRITABLE) {
	iotype |= IOTYPE_WRITE;
    }
#endif

    ai->selmask |= mask;
#if 0 /* UNIX */
    oskitunix_register_async_fd(ei->fd, iotype, asyncio_callback, ai);
#endif
    
    if (enabled)
	osenv_intr_enable();
    return retmask;
}

static OSKIT_COMDECL
entropy_good_asyncio_remove_listener(oskit_asyncio_t *a,
				     struct oskit_listener *l0)
{
    entropy_good_asyncio_impl_t *ai = (entropy_good_asyncio_impl_t *)a;
    int enabled;
    oskit_error_t rc;

    enabled = osenv_intr_save_disable();

    rc = oskit_listener_mgr_remove(ai->readers, l0);
    if (oskit_listener_mgr_count(ai->readers) == 0)
	ai->selmask &= ~OSKIT_ASYNCIO_READABLE;

#if 0 /* UNIX */
    if (! ai->selmask)
	oskitunix_unregister_async_fd(ai->fd);
#endif

    if (enabled)
	osenv_intr_enable();

    return 0;
}

static OSKIT_COMDECL
entropy_good_asyncio_readable(oskit_asyncio_t *a)
{
    entropy_good_asyncio_impl_t *ai = (entropy_good_asyncio_impl_t *)a;
    oskit_entropy_t *e = (oskit_entropy_t *)ai->ei;
    oskit_entropy_stats_t stats;

    oskit_entropy_getstats(e, &stats);

#if 0 /* UNIX */
    rc = NATIVEOS(ioctl)(si->fd, FIONREAD, &b);
#endif
    
    /* The idea is to return the number of full
       good entropy bytes that can be read.
    */
    return stats.entropy_count / 8;
}

static struct oskit_asyncio_ops goodasyncioops =
{
    entropy_good_asyncio_query,
    entropy_good_asyncio_addref,
    entropy_good_asyncio_release,
    entropy_good_asyncio_poll,
    entropy_good_asyncio_add_listener,
    entropy_good_asyncio_remove_listener,
    entropy_good_asyncio_readable,
};

void
oskit_linux_entropy_add_data(oskit_entropy_t *e, void *data, oskit_u32_t len,
			     oskit_u32_t entropy)
{
    entropy_impl_t *ei = (entropy_impl_t *)e;
    
/*#ifdef LINUX_RANDOM_POOL*/
    {
	oskit_u32_t buf[3];
	/* The first arg, a struct inode *, is never actually used.
	 */
	buf[0] = entropy;
	buf[1] = len;
	buf[2] = (oskit_u32_t)data;
	random_ioctl(NULL, &(ei->fs), RNDADDENTROPY, (unsigned long)buf);
    }
/*#endif*/
}

oskit_error_t
oskit_linux_entropy_probe(struct driver_struct *ds)
{
    struct entropy_driver *drv = (struct entropy_driver*)ds;
    struct device *ldev;
    int found = 0;
    struct device **dp;
    int devnum;
    static int already_probed = 0;

    if (!already_probed)
    {
	already_probed = 1;

	/*
	  The general idea here was snarfed from net.c's
	  oskit_linux_netdev_probe_raw()
	*/

	oskit_linux_init_entropy();

	osenv_log(OSENV_LOG_DEBUG, "Probing %s\n", ds->info.name);

	/* The magic "8" is appently the maximum size of the
	   string pointed to by name.  FIXME: check this out
	   and equate the magic number with some meaningful
	   symbol.
	*/
	ldev = kmalloc(sizeof(*ldev) + 8, GFP_KERNEL);
	if (ldev == NULL)
	{
	    return OSKIT_E_OUTOFMEMORY;
	}
	osenv_log(OSENV_LOG_DEBUG, "DLD: Created ldev at %p\n", ldev);
	memset(ldev, 0, sizeof(*ldev));
	ldev->name = (char*)(ldev + 1);
	devnum = 0;
    retry:
	/* We only have 8 bytes for this name (base + unit num)
	 */
	osenv_log(OSENV_LOG_DEBUG, "DLD: basename is %s\n", drv->basename);
	/*FIXME assert(strlen(drv->basename) < (8 - 1));*/
	assert(devnum < 10);
	sprintf(ldev->name, "%s%d", drv->basename, devnum);
	for (dp = &entropy_dev_base; *dp; dp = &(*dp)->next)
	    if (strcmp(ldev->name, (*dp)->name) == 0) {
		devnum++;
		goto retry;
	    }
	*dp = ldev;
	
	drv->probe(ldev);

	for (ldev = entropy_dev_base; ldev; ldev = ldev->next) {
	    oskit_osenv_t *osenv;
	    oskit_osenv_mem_t *mem;
	    entropy_impl_t *entropy;
	    
	    if (ldev->my_alias != NULL)
	    continue;
	    
	    assert(ldev->name != NULL);
	    
	    entropy = kmalloc(sizeof(*entropy), GFP_KERNEL);
	    if (entropy == NULL)
	    {
		return OSKIT_E_OUTOFMEMORY;
	    }
	    osenv_log(OSENV_LOG_DEBUG,
		      "DLD: Created entropy pool at %p\n", entropy);
	    memset(entropy, 0, sizeof(*entropy));
	    entropy->ie.ops = &oskit_linux_entropy_ops;
	    entropy->count = 1;
	    ldev->my_alias = entropy;
	    entropy->ds = ds;
	    entropy->fs.f_dentry = &(entropy->des);
	    entropy->des.d_inode = &(entropy->in);
	    entropy->stats.n_calls = 0;
	    entropy->stats.entropy_count = 0;
	    
	    /* Now we need a way to get memory for entropy channels.
	     */
	    oskit_lookup_first(&oskit_osenv_iid, (void *) &osenv);
	    if (osenv) {
		oskit_osenv_lookup(osenv, &oskit_osenv_mem_iid, (void *)&mem);
		oskit_osenv_release(osenv);
		entropy->mem = mem;
		
		/* And register the entropy pool device.
		 */
		osenv_device_register((oskit_device_t*)entropy,
				      drv->dev_iids,
				      drv->dev_niids);

		found++;

	    }
	}
    }
    
    return found;
}

/* End of entropy.c */
