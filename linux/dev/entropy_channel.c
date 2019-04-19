#include <oskit/dev/entropy_channel.h>
#include <oskit/dev/entropy.h>
#include "entropy_impl.h"

/*
  This code copyright (c) 2002, 2003 Derek L Davies (ddavies@ddavies.net)
*/

void oskit_linux_entropy_add_data(oskit_entropy_t *, void *,
				  oskit_u32_t, oskit_u32_t);

typedef struct entropy_channel_impl {
	oskit_entropy_channel_t iec;
	oskit_entropy_t *ep;
	unsigned count;
} entropy_channel_impl_t;

static OSKIT_COMDECL
entropy_channel_query(oskit_entropy_channel_t *e,
		      const oskit_iid_t *iid,
		      void **out_ihandle)
{
    entropy_channel_impl_t *ei = (entropy_channel_impl_t *)e;

    osenv_assert(ei != NULL);
    osenv_assert(ei->count != 0);
	
    if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	memcmp(iid, &oskit_entropy_channel_iid, sizeof(*iid)) == 0) {
	*out_ihandle = e;
	++ei->count;
	return 0;
    }

    *out_ihandle = NULL;
    return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
entropy_channel_addref(oskit_entropy_channel_t *e)
{
    entropy_channel_impl_t *ei = (entropy_channel_impl_t *)e;

    osenv_assert(ei != NULL);
    osenv_assert(ei->count != 0);

    return ++(ei->count);
}

static OSKIT_COMDECL_U
entropy_channel_release(oskit_entropy_channel_t *e)
{
	entropy_channel_impl_t *ei = (entropy_channel_impl_t *)e;
	unsigned newcount;

	osenv_assert(ei != NULL);
	osenv_assert(ei->count != 0);

	newcount = --ei->count;
	if (newcount == 0) {
	    oskit_osenv_mem_t *mem = ((entropy_impl_t*)ei->ep)->mem;
	    oskit_osenv_mem_free(mem, (void*)ei, 0, sizeof(*ei));
	}
	return newcount;
}

static OSKIT_COMDECL_V
entropy_channel_add_data(oskit_entropy_channel_t *ec, void *data,
			 oskit_u32_t len, oskit_u32_t entropy)
{
    entropy_channel_impl_t *eci = (entropy_channel_impl_t *)ec;
    entropy_impl_t *ei = (entropy_impl_t *)(eci->ep);
    oskit_linux_entropy_add_data((oskit_entropy_t *)ei, data, len, entropy);
    return;
}

static OSKIT_COMDECL_V
entropy_channel_get_stats(oskit_entropy_channel_t *e,
			  oskit_entropy_stats_t *stats)
{
    entropy_channel_impl_t *ei = (entropy_channel_impl_t *)e;
    oskit_entropy_getstats(ei->ep, stats);
    return;
}

static struct oskit_entropy_channel_ops entropy_channel_ops = {
    entropy_channel_query, entropy_channel_addref, entropy_channel_release,
    entropy_channel_add_data, entropy_channel_get_stats
};

oskit_error_t create_entropy_channel(oskit_osenv_mem_t *mem,
				     oskit_entropy_t *ep,
				     oskit_entropy_channel_t **out_chan)
{
    entropy_channel_impl_t *c;

    c = (entropy_channel_impl_t *)oskit_osenv_mem_alloc(mem, sizeof(*c), 0, 0);

    if (c == NULL)
	return OSKIT_E_OUTOFMEMORY;

    memset(c, 0, sizeof(*c));
    c->iec.ops = &entropy_channel_ops;
    c->ep = ep;
    c->count = 1;
    *out_chan = (oskit_entropy_channel_t *)c;
	
    return 0;
}

/* End of entropy_channel.c */
