/*
 * Entropy Pool Interface.
 *
 * Copyright (c) 2002, 2003 Derek L Davies (ddavies@ddavies.net)
 *
 */

#ifndef _OSKIT_DEV_ENTROPY_H_
#define _OSKIT_DEV_ENTROPY_H_

#include <oskit/dev/device.h>
#include <oskit/com/stream.h>
#include <oskit/io/asyncio.h>
#include <oskit/dev/entropy_channel.h>

struct oskit_entropy {
	struct oskit_entropy_ops *ops;
};
typedef struct oskit_entropy oskit_entropy_t;

struct oskit_entropy_ops {

	/*** COM-specified IUnknown interface operations ***/
	OSKIT_COMDECL_IUNKNOWN(oskit_entropy_t)

	/*** For both oskit_device_t and oskit_driver_t.  device needs
	     just getinfo, and driver needs both getinfo and getdriver. ***/
	OSKIT_COMDECL	(*getinfo)(oskit_entropy_t *r,
				   oskit_devinfo_t *out_info);
	OSKIT_COMDECL	(*getdriver)(oskit_entropy_t *r,
				     oskit_driver_t **out_driver);

	OSKIT_COMDECL (*getstats)(oskit_entropy_t *e,
				  oskit_entropy_stats_t *out_stats);

	/*** This is how device drivers attach to the entropy driver(s).
	     They add entropy via the entropy channel. ***/
	OSKIT_COMDECL (*attach_source)(oskit_entropy_t *e,
				       char *name,
				       oskit_u32_t type,
				       oskit_u32_t flags,
				       oskit_entropy_channel_t **out_chan);

	/*** This is an interface for non blocking, read only, calls.
	     It roughly corresponds to /dev/urandom . ***/
	OSKIT_COMDECL (*open_nonblocking)(oskit_entropy_t *e,
					  oskit_stream_t **out_stream_obj);

	/*** This is an interface for extracting good randomness and, as such,
	     it may block.  Entropy is both readable and writeable via this
	     interface.  It roughly corresponds to /dev/random . ***/
	OSKIT_COMDECL (*open_good)(oskit_entropy_t *e,
				   oskit_stream_t **out_stream_obj,
				   oskit_asyncio_t **out_async_obj);
};

/* GUID for oskit_entropy interface */
extern const struct oskit_guid oskit_entropy_iid;
#define OSKIT_ENTROPY_IID OSKIT_GUID(0x4aa7e00a, 0x7c74, 0x11cf, \
				     0xb5, 0x00, 0x08, 0x00, \
                                     0x09, 0x53, 0xad, 0xc2)

#define oskit_entropy_query(e, iid, out_ihandle) \
	((e)->ops->query((oskit_entropy_t *)(e), (iid), (out_ihandle)))
#define oskit_entropy_addref(e) \
	((e)->ops->addref((oskit_entropy_t *)(e)))
#define oskit_entropy_release(e) \
	((e)->ops->release((oskit_entropy_t *)(e)))

#define oskit_entropy_getinfo(e, inf) \
	((e)->ops->getinfo((oskit_entropy_t *)(e), (inf)))
#define oskit_entropy_getdriver(e, dev, drv) \
	((e)->ops->getdriver((oskit_entropy_t *)(e), (dev), (drv)))

#define oskit_entropy_getstats(e, stats) \
	((e)->ops->getstats((oskit_entropy_t *)(e), (stats)))
#define oskit_entropy_attach_source(e, name, type, flags, chan) \
	((e)->ops->attach_source((oskit_entropy_t *)(e), \
				 (name), (type), (flags), (chan)))
#define oskit_entropy_open_nonblocking(e, stream) \
	((e)->ops->open_nonblocking((oskit_entropy_t *)(e), (stream)))
#define oskit_entropy_open_good(e, stream, asyncio) \
	((e)->ops->open_good((oskit_entropy_t *)(e), (stream), (asyncio)))
#define oskit_entropy_add_data(e, data, len, entropy) \
	((e)->ops->add_data((oskit_entropy_t *)(e), (data), (len), (entropy)))

oskit_error_t create_entropy_channel(oskit_osenv_mem_t *mem,
				     oskit_entropy_t *ep,
				     oskit_entropy_channel_t **out_chan);

#endif /* _OSKIT_DEV_ENTROPY_H_ */
