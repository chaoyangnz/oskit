#ifndef _OSKIT_DEV_ENTROPY_CHANNEL_H_
#define _OSKIT_DEV_ENTROPY_CHANNEL_H_

#include <oskit/com.h>
#include <oskit/error.h>
#include <oskit/dev/osenv_mem.h>
#include <oskit/dev/entropy_stats.h>

/*
 * Entropy channel interface.  This is how drivers
 * add entropy to the entropy device(s).
 *
 * IID: 4aa7e00b-7c74-11cf-b500-08000953adc2
 *
 * Copyright (c) 2002, 2003 Derek L Davies (ddavies@ddavies.net)
 *
 */
struct oskit_entropy_channel {
    struct oskit_entropy_channel_ops *ops;
};
typedef struct oskit_entropy_channel oskit_entropy_channel_t;

struct oskit_entropy_channel_ops {

    /*** COM-specified IUnknown interface operations ***/
    OSKIT_COMDECL_IUNKNOWN(oskit_entropy_channel_t);

    /*** Entropy channel stuff ***/
    OSKIT_COMDECL_V (*add_data)(oskit_entropy_channel_t *c, void *data,
				oskit_u32_t len, oskit_u32_t entropy);

    OSKIT_COMDECL_V (*get_stats)(oskit_entropy_channel_t *c,
				 oskit_entropy_stats_t *stats);
};

/* GUID for oskit_entropy_channel interface */
extern const struct oskit_guid oskit_entropy_channel_iid;
#define OSKIT_ENTROPY_CHANNEL_IID OSKIT_GUID(0x4aa7e00b, 0x7c74, 0x11cf, \
				             0xb5, 0x00, 0x08, 0x00, \
                                             0x09, 0x53, 0xad, 0xc2)

#define oskit_entropy_channel_query(c, iid, out_ihandle) \
	((c)->ops->query((oskit_entropy_channel_t *)(c), (iid), (out_ihandle)))
#define oskit_entropy_channel_addref(c) \
	((c)->ops->addref((oskit_entropy_channel_t *)(c)))
#define oskit_entropy_channel_release(c) \
	((c)->ops->release((oskit_entropy_channel_t *)(c)))

#define oskit_entropy_channel_add_data(c, data, len, entropy) \
	((c)->ops->add_data((oskit_entropy_channel_t *)(c), (data), (len), \
			    (entropy)))

#define oskit_entropy_channel_get_stats(c, stats) \
	((c)->ops->get_stats((oskit_entropy_channel_t *)(c), (stats)))

#endif /* _OSKIT_DEV_ENTROPY_CHANNEL_H_ */
