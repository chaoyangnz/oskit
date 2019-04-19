#include <oskit/dev/dev.h>
#include <oskit/dev/bus.h>
#include <oskit/dev/stream.h>
#include <oskit/dev/isa.h>
#include <oskit/dev/native.h>
#include <oskit/dev/osenv.h>
#include <oskit/dev/osenv_irq.h>
#include <oskit/dev/osenv_intr.h>
#include <oskit/machine/pc/keyboard.h>
#include <oskit/machine/pio.h>
#include <oskit/c/malloc.h>
#include <oskit/c/string.h>
#include <oskit/com/stream.h>
#include <oskit/io/asyncio.h>
#include <oskit/com/listener_mgr.h>

extern oskit_osenv_t *oskit_dev_osenv; /* XXX */

#define INIT_CMDBYTE (KC8_CPU|KC8_MDISABLE|KC8_KDISABLE)
#define KBD_DELAY 	60

#define KBD_IRQ		1
#define AUX_IRQ		12


/*	$NetBSD: i8042reg.h,v 1.8 2002/01/31 13:25:20 uwe Exp $	*/

#define	KBSTATP		4	/* kbd controller status port (I) */
#define	 KBS_DIB	0x01	/* kbd data in buffer */
#define	 KBS_IBF	0x02	/* kbd input buffer low */
#define	 KBS_WARM	0x04	/* kbd system flag */
#define	 KBS_OCMD	0x08	/* kbd output buffer has command */
#define	 KBS_NOSEC	0x10	/* kbd security lock not engaged */
#define	 KBS_TERR	0x20	/* kbd transmission error */
#define	 KBS_RERR	0x40	/* kbd receive error */
#define	 KBS_PERR	0x80	/* kbd parity error */

#define	KBCMDP		4	/* kbd controller port (O) */
#define	 KBC_RAMREAD	0x20	/* read from RAM */
#define	 KBC_RAMWRITE	0x60	/* write to RAM */
#define	 KBC_AUXDISABLE	0xa7	/* disable auxiliary port */
#define	 KBC_AUXENABLE	0xa8	/* enable auxiliary port */
#define	 KBC_AUXTEST	0xa9	/* test auxiliary port */
#define	 KBC_KBDECHO	0xd2	/* echo to keyboard port */
#define	 KBC_AUXECHO	0xd3	/* echo to auxiliary port */
#define	 KBC_AUXWRITE	0xd4	/* write to auxiliary port */
#define	 KBC_SELFTEST	0xaa	/* start self-test */
#define	 KBC_KBDTEST	0xab	/* test keyboard port */
#define	 KBC_KBDDISABLE	0xad	/* disable keyboard port */
#define	 KBC_KBDENABLE	0xae	/* enable keyboard port */
#define	 KBC_PULSE0	0xfe	/* pulse output bit 0 */
#define	 KBC_PULSE1	0xfd	/* pulse output bit 1 */
#define	 KBC_PULSE2	0xfb	/* pulse output bit 2 */
#define	 KBC_PULSE3	0xf7	/* pulse output bit 3 */

#define	KBDATAP		0	/* kbd data port (I) */
#define	KBOUTP		0	/* kbd data port (O) */

#define	K_RDCMDBYTE	0x20
#define	K_LDCMDBYTE	0x60

#define	KC8_TRANS	0x40	/* convert to old scan codes */
#define	KC8_MDISABLE	0x20	/* disable mouse */
#define	KC8_KDISABLE	0x10	/* disable keyboard */
#define	KC8_IGNSEC	0x08	/* ignore security lock */
#define	KC8_CPU		0x04	/* exit from protected mode reset */
#define	KC8_MENABLE	0x02	/* enable mouse interrupt */
#define	KC8_KENABLE	0x01	/* enable keyboard interrupt */
#define	CMDBYTE		(KC8_TRANS|KC8_CPU|KC8_MENABLE|KC8_KENABLE)

#define KBC_DEVCMD_ACK 0xfa
#define KBC_DEVCMD_RESEND 0xfe


static oskit_error_t
i8042_wait(void)
{
	unsigned int i;
	for (i = 100000; i; i--)
		if ((inb(K_STATUS) & K_IBUF_FUL) == 0) {
			osenv_timer_spin(KBD_DELAY);
			return 0;
		}
	return OSKIT_EIO;
}

static oskit_error_t
i8042_send(oskit_u8_t cmd)
{
	oskit_error_t rc = i8042_wait();
	if (rc == 0)
		outb(K_CMD, cmd);
	return rc;
}

static oskit_error_t
i8042_send_data(oskit_bool_t aux, oskit_u8_t data)
{
	oskit_error_t rc;

	if (aux) {
		rc = i8042_send(KBC_AUXWRITE);
		if (rc)
			return rc;
	}

	rc = i8042_wait();
	if (rc == 0)
		outb(K_RDWR, data);

	return rc;
}

static oskit_error_t
i8042_cmdbyte(oskit_u8_t byte)
{
	oskit_error_t rc = i8042_send(KBC_RAMWRITE);
	if (rc == 0)
		rc = i8042_wait();
	if (rc == 0)
		outb(K_RDWR, byte);
	return rc;
}

static void
i8042_flush(void)
{
	int i = 100;
	while ((inb(K_STATUS) & K_OBUF_FUL) && i--)
		(void) inb(K_RDWR);
}

struct i8042_slot {
	oskit_streamdev_t devi;

	struct i8042_bus *bus;	/* back-pointer, set iff probe found it */
	int irq;
	oskit_u8_t enable_cmdbits, disable_cmdbits;

	struct open_slot *volatile open;
};

#define NSLOTS	2
struct i8042_bus {
	oskit_bus_t busi;
	unsigned int refs;
	struct i8042_slot slots[NSLOTS];

	oskit_u8_t cmdbyte;	/* i8042 command byte value.  */

	/* Environment hooks.  */
	oskit_osenv_irq_t *irq;
	oskit_osenv_intr_t *intr;
};


static void i8042_interrupt(void *arg);	/* below */

static struct oskit_streamdev_ops slot_device_ops; /* below */
static struct oskit_bus_ops bus_ops; /* below */


static OSKIT_COMDECL
driver_query(oskit_isa_driver_t *drv, const struct oskit_guid *iid,
	     void **out_ihandle)
{
	if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_driver_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_isa_driver_iid, sizeof(*iid)) == 0) {
		*out_ihandle = drv;
		return 0;
	}

	*out_ihandle = 0;
	return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U driver_addref(oskit_isa_driver_t *drv)
{
	/* No reference counting for static driver nodes */
	return 1;
}

static OSKIT_COMDECL_U driver_release(oskit_isa_driver_t *drv)
{
	/* No reference counting for static driver nodes */
	return 1;
}

static OSKIT_COMDECL
driver_getinfo(oskit_isa_driver_t *drv, oskit_devinfo_t *out_info)
{
	out_info->name = "i8042";
	out_info->description = "i8042 PC Keyboard Controller";
	out_info->vendor = "Intel";
	out_info->author = "OSKit";
	out_info->version = NULL;
	return 0;
}

static OSKIT_COMDECL
driver_probe(oskit_isa_driver_t *drv, oskit_isabus_t *bus)
{
	oskit_error_t rc;
	struct i8042_bus *newb;

	rc = osenv_io_alloc(K_RDWR, 1);
	if (rc)
		return rc;
	rc = osenv_io_alloc(K_CMD, 1);
	if (rc) {
		osenv_io_free(K_RDWR, 1);
		return rc;
	}

	i8042_flush();
	rc = i8042_cmdbyte(INIT_CMDBYTE);
	if (rc == 0) {
		newb = smalloc(sizeof *newb);
		if (newb == NULL) {
			osenv_io_free(K_RDWR, 1);
			osenv_io_free(K_CMD, 1);
			rc = OSKIT_E_OUTOFMEMORY;
		}
		else {
			newb->busi.ops = &bus_ops;
			newb->refs = 1;
			memset(newb->slots, 0, sizeof newb->slots);
			newb->cmdbyte = INIT_CMDBYTE;
			rc = osenv_isabus_addchild(
				K_RDWR, (oskit_device_t*)&newb->busi);
			if (rc == 0)
				rc = GET_OSENV_IFACE(oskit_dev_osenv,
						     irq, &newb->irq);
			if (rc == 0)
				rc = GET_OSENV_IFACE(oskit_dev_osenv,
						     intr,
						     &newb->intr);
			if (rc)
				sfree(newb, sizeof *newb);
		}
	}

	if (rc) {
		osenv_io_free(K_RDWR, 1);
		osenv_io_free(K_CMD, 1);
	}

	return rc ?: 1;
}

static struct oskit_isa_driver_ops driver_ops = {
	driver_query,
	driver_addref,
	driver_release,
	driver_getinfo,
	driver_probe
};

static struct oskit_isa_driver i8042_driver = { &driver_ops };

#define OSENV(if, call, args...) \
	oskit_osenv_##if##_##call(b->if ,##args)


static OSKIT_COMDECL
bus_query(oskit_bus_t *bus, const struct oskit_guid *iid, void **out_ihandle)
{
	struct i8042_bus *b = (struct i8042_bus *) bus;

	if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_device_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_bus_iid, sizeof(*iid)) == 0) {
		++b->refs;
		*out_ihandle = &b->busi;
		return 0;
	}

	*out_ihandle = NULL;
	return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
bus_addref(oskit_bus_t *bus)
{
	struct i8042_bus *b = (struct i8042_bus *) bus;
	return ++b->refs;
}

static OSKIT_COMDECL_U
bus_release(oskit_bus_t *bus)
{
	struct i8042_bus *b = (struct i8042_bus *) bus;
	if (--b->refs > 0)
		return b->refs;

	/* We know that the interrupts are disabled because
	   that happens on closing the slots, and open slots
	   would keep us alive.  */

	osenv_isabus_remchild(K_RDWR);
	osenv_io_free(K_CMD, 1);
	osenv_io_free(K_RDWR, 1);

	sfree(b, sizeof *b);
	return 0;
}

static OSKIT_COMDECL
bus_getinfo(oskit_bus_t *bus, oskit_devinfo_t *out_info)
{
	return driver_getinfo(NULL, out_info);
}

/* Every bus needs a driver.  Heh.  */
static OSKIT_COMDECL
bus_getdriver(oskit_bus_t *bus, oskit_driver_t **out_driver)
{
	*out_driver = (oskit_driver_t *)&i8042_driver;
	return 0;
}

static OSKIT_COMDECL
bus_getchild(oskit_bus_t *bus, oskit_u32_t idx,
	     struct oskit_device **out_fdev, char *out_pos)
{
	struct i8042_bus *b = (struct i8042_bus *) bus;

	while (idx < NSLOTS && b->slots[idx].bus == NULL)
		++idx;

	if (idx < NSLOTS) {
		++b->refs;
		*out_fdev = (oskit_device_t *)&b->slots[idx].devi;
		strcpy (out_pos, "slot0");
		out_pos[4] = '0' + idx;
		return 0;
	}

	return OSKIT_E_DEV_NOMORE_CHILDREN;
}

static OSKIT_COMDECL
bus_probe(oskit_bus_t *bus)
{
	struct i8042_bus *b = (struct i8042_bus *) bus;
	int nfound = 0;

	/* Probe each slot that we don't already know is there,
	   and count how many new ones we saw.  */

	if (b->slots[0].bus == NULL) {
		/* Following NetBSD and Linux, we don't probe for the keyboard
		   port but just always assume it is there.  */
		b->slots[0].bus = b;
		b->slots[0].devi.ops = &slot_device_ops;
		b->slots[0].irq = KBD_IRQ;
		b->slots[0].enable_cmdbits = KC8_KENABLE;
		b->slots[0].disable_cmdbits = KC8_KDISABLE;
		osenv_device_register((oskit_device_t *)&b->slots[0].devi,
				      &oskit_streamdev_iid, 1);
		++nfound;
	}

	if (b->slots[1].bus == NULL) {
		oskit_error_t rc;
		/* Enable the port.  */
		rc = i8042_cmdbyte(b->cmdbyte &~ (KC8_MENABLE|KC8_MDISABLE));
		if (rc == 0)
			rc = i8042_send(KBC_AUXECHO);
		if (rc == 0)
			rc = i8042_send_data(0, 0x5a);
		/* NetBSD 1.6 /sys/dev/ic/pckbc.c comment says that some
		   older controllers return 0xfe here, and so it accepts
		   anything at all as ok.  Linux 2.5.41 requires 0xa5 and
		   does several other checks after that as well to rule out
		   false positives.  */
		if (rc == 0) {
			oskit_u8_t status, data;
			int i;
			rc = OSKIT_EIO;
			for (i = 100000; i; i--) {
				status = inb(K_STATUS);
				if (status & K_OBUF_FUL) {
					osenv_timer_spin(KBD_DELAY);
					data = inb(K_RDWR);
					if (status & K_AUX_OBUF_FUL) {
						rc = 0;
						break;
					}
					/* XXX
					   Really should do i8042_interrupt
					   guts here, but too hairy for now.
					*/
					osenv_log(OSENV_LOG_WARNING,
						  "lost keyboard data 0x%x",
						  data);
				}
			}
		}
		if (rc == 0) {
			b->slots[1].bus = b;
			b->slots[1].devi.ops = &slot_device_ops;
			b->slots[1].irq = AUX_IRQ;
			b->slots[1].enable_cmdbits = KC8_MENABLE;
			b->slots[1].disable_cmdbits = KC8_MDISABLE;
			osenv_device_register(
				(oskit_device_t *)&b->slots[1].devi,
				&oskit_streamdev_iid, 1);
			++nfound;
		}
		/* Restore the cmdbyte, disabling the port again
		   until it's opened.  */
		i8042_cmdbyte(b->cmdbyte);
	}

	return nfound;
}

static struct oskit_bus_ops bus_ops = {
	bus_query,
	bus_addref,
	bus_release,
	bus_getinfo,
	bus_getdriver,
	bus_getchild,
	bus_probe
};


/* Open slots have a simple stream/asyncio interface.

   We support nonblocking reads and writes only.
*/

#define SLOT_BUFSIZE	256	/* ??? */
#define SLOT_CMDSIZE	8

struct open_slot {
	oskit_stream_t streami;
	oskit_asyncio_t asyncioi;

	struct i8042_slot *slot; /* back-pointer */

	unsigned int refs;

	struct listener_mgr *read_listeners, *write_listeners;
	oskit_size_t head, tail;
	unsigned char buf[SLOT_BUFSIZE];

	unsigned char cmdbuf[SLOT_CMDSIZE];
	oskit_size_t cmdidx, cmdlen;
};
#define queue_init(q) 	((q)->head = (q)->tail = 0)
#define queue_next(q,i)	(((i) + 1) % SLOT_BUFSIZE)
#define queue_full(q) 	(queue_next((q),(q)->tail) == q->head)
#define queue_empty(q)	((q)->head == (q)->tail)

#undef OSENV
#define OSENV(if, call, args...) \
	oskit_osenv_##if##_##call(s->slot->bus->if ,##args)


/* Stream interface.  */

static OSKIT_COMDECL
stream_query(oskit_stream_t *si, const struct oskit_guid *iid,
	     void **out_ihandle)
{
	struct open_slot *s = (struct open_slot *)si;

	if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_stream_iid, sizeof(*iid)) == 0) {
		s->refs++;
		*out_ihandle = &s->streami;
		return 0;
	}
	if (memcmp(iid, &oskit_asyncio_iid, sizeof(*iid)) == 0) {
		s->refs++;
		*out_ihandle = &s->asyncioi;
		return 0;
	}

	*out_ihandle = 0;
	return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
stream_addref(oskit_stream_t *si)
{
	struct open_slot *s = (struct open_slot *)si;
	return ++s->refs;
}

static OSKIT_COMDECL_U
stream_release(oskit_stream_t *si)
{
	struct i8042_bus *bus;
	struct open_slot *s = (struct open_slot *)si;
	if (--s->refs > 0)
		return s->refs;

	bus = s->slot->bus;
	s->slot->open = NULL;	/* Make the interrupt handler ignore it. */

	/* Disable the device and its interrupt.
	   Linux 2.5.41 uses this method.  NetBSD 1.6 never touches
	   the DISABLE bits this way, but instead only uses the KBC_*
	   commands to do it.  I don't know if it matters.  */
	{
		oskit_u8_t cmdbyte = bus->cmdbyte;
		cmdbyte &= s->slot->enable_cmdbits;
		cmdbyte |= s->slot->disable_cmdbits;
		if (i8042_cmdbyte(cmdbyte) == 0)
			bus->cmdbyte = cmdbyte;
	}

	oskit_osenv_irq_free(bus->irq, s->slot->irq, &i8042_interrupt, bus);

	if (s->read_listeners)
		oskit_destroy_listener_mgr(s->read_listeners);
	if (s->write_listeners)
		oskit_destroy_listener_mgr(s->write_listeners);
	sfree(s, sizeof *s);

	/* We held a ref on the bus while we had the slot open.  */
	bus_release((oskit_bus_t *)bus);

	return 0;
}

static OSKIT_COMDECL
stream_read(oskit_stream_t *si,
	    void *buf, oskit_u32_t len, oskit_u32_t *out_actual)
{
	struct open_slot *s = (struct open_slot *)si;
	oskit_error_t rc;

	OSENV(intr, disable);
	if (s->tail == s->head)
		rc = OSKIT_EWOULDBLOCK;
	else {
		if (s->head > s->tail) {
			oskit_size_t copy = len;
			if (copy > SLOT_BUFSIZE - s->head)
				copy = SLOT_BUFSIZE - s->head;
			memcpy(buf, &s->buf[s->head], copy);
			s->head = (s->head + copy) % SLOT_BUFSIZE;
			*out_actual = copy;
		}
		else
			*out_actual = 0;

		if (s->head < s->tail) {
			oskit_size_t copy = len - *out_actual;
			if (copy > s->tail - s->head)
				copy = s->tail - s->head;
			memcpy(buf, &s->buf[*out_actual + s->head], copy);
			s->head += copy;
			*out_actual += copy;
		}

		rc = 0;
	}
	OSENV(intr, enable);

	return rc;
}

static OSKIT_COMDECL
stream_write(oskit_stream_t *si, const void *buf, oskit_u32_t len,
	     oskit_u32_t *out_actual)
{
	struct open_slot *s = (struct open_slot *)si;
	const unsigned char *p = buf;

	while (s->cmdlen < sizeof s->cmdbuf && len-- > 0)
		s->cmdbuf[s->cmdlen++] = *p++;

	if (s->cmdidx == 0 && s->cmdlen != 0) {
		/* We are not already sending a command, so start now.  */
		oskit_error_t rc = i8042_send_data(
			s->slot == &s->slot->bus->slots[1],
			s->cmdbuf[0]);
		if (rc)
			return rc;
		/* The interrupt handler will get ACK or NAK.  */
	}

	*out_actual = p - (const unsigned char *)buf;
	return *out_actual == 0 ? OSKIT_EWOULDBLOCK : 0;
}

static OSKIT_COMDECL
stream_seek(oskit_stream_t *si, oskit_s64_t ofs, oskit_seek_t whence,
	    oskit_u64_t *out_newpos)
{
	return OSKIT_ESPIPE;
}

static OSKIT_COMDECL
stream_setsize(oskit_stream_t *si, oskit_u64_t new_size)
{
	return OSKIT_EINVAL;
}

static OSKIT_COMDECL
stream_copyto(oskit_stream_t *si, oskit_stream_t *dst, oskit_u64_t size,
	      oskit_u64_t *out_read, oskit_u64_t *out_written)
{
	return OSKIT_E_NOTIMPL;	/*XXX*/
}

static OSKIT_COMDECL
stream_commit(oskit_stream_t *si, oskit_u32_t commit_flags)
{
	return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
stream_revert(oskit_stream_t *si)
{
	return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
stream_lock_region(oskit_stream_t *si, oskit_u64_t offset,
		   oskit_u64_t size, oskit_u32_t lock_type)
{
	return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
stream_unlock_region(oskit_stream_t *si, oskit_u64_t offset,
		     oskit_u64_t size, oskit_u32_t lock_type)
{
	return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
stream_stat(oskit_stream_t *si, oskit_stream_stat_t *out_stat,
	    oskit_u32_t stat_flags)
{
	return OSKIT_E_NOTIMPL;	/*XXX*/
}

static OSKIT_COMDECL
stream_clone(oskit_stream_t *si, oskit_stream_t **out_stream)
{
	return OSKIT_E_NOTIMPL;	/*XXX*/
}

static struct oskit_stream_ops slot_stream_ops = {
	stream_query, stream_addref, stream_release,
	stream_read, stream_write, stream_seek, stream_setsize,
	stream_copyto, stream_commit, stream_revert,
	stream_lock_region, stream_unlock_region,
	stream_stat, stream_clone
};


/*
 **********************************************************************
 * Async IO interface,
 */
static OSKIT_COMDECL
asyncio_query(oskit_asyncio_t *f,
		   const struct oskit_guid *iid, void **out_ihandle)
{
	struct open_slot *s = (void *) (f - 1);

	return stream_query(&s->streami, iid, out_ihandle);
}

static OSKIT_COMDECL_U
asyncio_addref(oskit_asyncio_t *f)
{
	struct open_slot *s = (void *) (f - 1);

	return stream_addref(&s->streami);
}

static OSKIT_COMDECL_U
asyncio_release(oskit_asyncio_t *f)
{
	struct open_slot *s = (void *) (f - 1);

	return stream_release(&s->streami);
}


static OSKIT_COMDECL
asyncio_poll(oskit_asyncio_t *f)
{
	struct open_slot *s = (void *) (f - 1);
	int ret = 0;

	/* Don't bother disabling interrupts because an interrupt can only
	   perturb us in the way that gives us a positive.  */
	if (!queue_empty(s))
		ret |= OSKIT_ASYNCIO_READABLE;
	if (s->cmdlen < sizeof s->cmdbuf)
		ret |= OSKIT_ASYNCIO_WRITABLE;

	return ret;
}

/*
 * Add a callback object (a "listener" for async I/O events).
 * When an event of interest occurs on this I/O object
 * (i.e., when one of the three I/O conditions becomes true),
 * all registered listeners will be called.
 * Also, if successful, this method returns a mask
 * describing which of the OSKIT_ASYNC_IO_* conditions are already true,
 * which the caller must check in order to avoid missing events
 * that occur just before the listener is registered.
 */
static OSKIT_COMDECL
asyncio_add_listener(oskit_asyncio_t *f,
		     struct oskit_listener *l, oskit_s32_t mask)
{
	struct open_slot *s = (void *) (f - 1);

	if (mask & OSKIT_ASYNCIO_READABLE) {
		/* We don't need to bother disabling interrupts yet,
		   because the handler won't do anything until the
		   read_listeners store happens and then what it does
		   will be ok.  */
		if (!s->read_listeners) {
			s->read_listeners = oskit_create_listener_mgr
				((oskit_iunknown_t *)f);
			if (!s->read_listeners)
				return OSKIT_E_OUTOFMEMORY;
		}
		/* Now we interact with the handler.  */
		OSENV(intr, disable);
		oskit_listener_mgr_add(s->read_listeners, l);
		OSENV(intr, enable);
	}

	if (mask & OSKIT_ASYNCIO_WRITABLE) {
		if (!s->write_listeners) {
			s->write_listeners = oskit_create_listener_mgr
				((oskit_iunknown_t *)f);
			if (!s->write_listeners)
				return OSKIT_E_OUTOFMEMORY;
		}
		OSENV(intr, disable);
		oskit_listener_mgr_add(s->write_listeners, l);
		OSENV(intr, enable);
	}

        return asyncio_poll(f);
}

/*
 * Remove a previously registered listener callback object.
 * Returns an error if the specified callback has not been registered.
 */
static OSKIT_COMDECL
asyncio_remove_listener(oskit_asyncio_t *f, struct oskit_listener *l)
{
	struct open_slot *s = (void *) (f - 1);
	oskit_error_t rc = 0;

	OSENV(intr, disable);
	/*
	 * we don't know where was added - if at all - so let's check
	 * both lists
	 */
	if (oskit_listener_mgr_remove(s->read_listeners, l) &&
	    oskit_listener_mgr_remove(s->write_listeners, l))
		rc = OSKIT_E_INVALIDARG;
	OSENV(intr, enable);

	return rc;
}

static OSKIT_COMDECL
asyncio_readable(oskit_asyncio_t *f)
{
	struct open_slot *s = (void *) (f - 1);
	int n;

	OSENV(intr, disable);
	if (s->head == s->tail)
		n = 0;
	else if (s->head < s->tail)
		n = s->tail - s->head;
	else
		n = SLOT_BUFSIZE - s->head + s->tail;
	OSENV(intr, disable);

	return n;
}

static struct oskit_asyncio_ops slot_asyncio_ops =
{
	asyncio_query,
	asyncio_addref,
	asyncio_release,
	asyncio_poll,
	asyncio_add_listener,
	asyncio_remove_listener,
	asyncio_readable
};

/* This is the main magilla, interrupt handler for either IRQ from the i8042.

   We handle both ports if they have data.  If in the middle of sending
   a command, we handle ACK/NAK and send the next byte (or resend the last).

 */

static void
i8042_interrupt(void *arg)
{
	struct i8042_bus *const bus = arg;
	oskit_u8_t status, data;
	struct open_slot *s;
	struct listener_mgr *notify[NSLOTS];
	int got[NSLOTS];
	int slot;

	for (slot = 0; slot < NSLOTS; ++slot) {
		got[slot] = 0;
		if (bus->slots[slot].bus == NULL)
			notify[slot] = NULL;
		else {
			s = bus->slots[slot].open;
			notify[slot] =
				(s && s->read_listeners && queue_empty(s))
				? s->read_listeners : NULL;
		}
	}

	/* Read the status byte and see whether there is input.  */
	while ((status = inb(K_STATUS)) & K_OBUF_FUL) {
		osenv_timer_spin(KBD_DELAY);
		data = inb(K_RDWR);

		slot = bus->slots[1].bus && (status & K_AUX_OBUF_FUL);
		s = bus->slots[slot].open;

		if (s == NULL)
			/* Stray interrupt when a slot is shutting down.  */
			continue;

		if (s->cmdidx < s->cmdlen) {
			switch (data) {
			case KBC_DEVCMD_ACK:
				++s->cmdidx;
				if (s->cmdidx == sizeof s->cmdbuf) {
					/* Buffer was full, is empty now.  */
					s->cmdidx = s->cmdlen = 0;
					if (s->write_listeners)
						oskit_listener_mgr_notify(
							s->write_listeners);
					break;
				}
				else if (s->cmdidx == s->cmdlen) {
					/* Fully drained.  */
					s->cmdidx = s->cmdlen = 0;
					break;
				}
				/* FALLTHROUGH */
			case KBC_DEVCMD_RESEND:
				/* Note that this resends forever.  The
				   Linux 2.5.41 does not resend at all, and
				   NetBSD 1.6 resends 5 times before
				   failing.  Since our commands are always
				   nonblocking, we just keep resending.
				   The user can notice eventually that the
				   (very small) cmdbuf fills up and that
				   the stream never becomes writable for a
				   long time.  Then it needs to release
				   refs to close the slot and then reopen
				   it to try to make new progress.  */
				i8042_send_data(slot, s->cmdbuf[s->cmdidx]);
				continue;
			}
		}

		s->buf[s->tail] = data;
		s->tail = (s->tail + 1) % SLOT_BUFSIZE;
		if (s->tail == s->head)
			s->head = (s->head + 1) % SLOT_BUFSIZE;
		got[slot] = 1;
	}

	for (slot = 0; slot < NSLOTS; ++slot)
		if (notify[slot] && got[slot])
			oskit_listener_mgr_notify(notify[slot]);
}

/* Device interface for slots.

   The slot devices really refer to the i8042 bus device itself,
   and just add references to that.  */

static OSKIT_COMDECL
slot_query(oskit_streamdev_t *dev,
	   const struct oskit_guid *iid, void **out_ihandle)
{
	struct i8042_slot *slot = (struct i8042_slot *) dev;
	struct i8042_bus *b = slot->bus;

	if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_driver_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_device_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_streamdev_iid, sizeof(*iid)) == 0) {
		++b->refs;
		*out_ihandle = &slot->devi;
		return 0;
	}

	*out_ihandle = NULL;
	return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
slot_addref(oskit_streamdev_t *dev)
{
	struct i8042_slot *slot = (struct i8042_slot *) dev;
	return bus_addref ((oskit_bus_t *)slot->bus);
}

static OSKIT_COMDECL_U
slot_release(oskit_streamdev_t *dev)
{
	struct i8042_slot *slot = (struct i8042_slot *) dev;
	return bus_release ((oskit_bus_t *)slot->bus);
}

static OSKIT_COMDECL
slot_getinfo(oskit_streamdev_t *dev, oskit_devinfo_t *out_info)
{
	struct i8042_slot *slot = (struct i8042_slot *) dev;
	struct i8042_bus *b = slot->bus;

	if (slot == &b->slots[0]) {
		out_info->name = "kbd";
		out_info->description = "PC Keyboard port";
	}
	else {
		out_info->name = "aux";
		out_info->description = "PS/2 Mouse port";
	}
	out_info->vendor = NULL;
	out_info->author = "OSKit";
	out_info->version = NULL;
	return 0;
}

static OSKIT_COMDECL
slot_getdriver(oskit_streamdev_t *dev, oskit_driver_t **out_driver)
{
	struct i8042_slot *slot = (struct i8042_slot *) dev;
	return bus_getdriver ((oskit_bus_t *)slot->bus, out_driver);
}

static OSKIT_COMDECL
slot_open(oskit_streamdev_t *dev, oskit_s32_t flags,
	  oskit_stream_t **out_stream)
{
	struct i8042_slot *slot = (struct i8042_slot *) dev;
	struct open_slot *s;
	oskit_error_t rc;

	if (slot->open)
		/* Or return another ref?  */
		return OSKIT_EBUSY;

	s = smalloc (sizeof *s);
	if (s == NULL)
		return OSKIT_E_OUTOFMEMORY;

	s->streami.ops = &slot_stream_ops;
	s->asyncioi.ops = &slot_asyncio_ops;
	s->refs = 1;
	s->read_listeners = s->write_listeners = NULL;
	queue_init(s);
	s->cmdidx = s->cmdlen = 0;

	s->slot = slot;
	++slot->bus->refs;	/* open slot holds a ref */
	slot->open = s;		/* no ref */

	rc = oskit_osenv_irq_alloc(slot->bus->irq,
				   slot->irq, &i8042_interrupt, slot->bus, 0);
	if (rc == 0) {
		oskit_u8_t cmdbyte = slot->bus->cmdbyte;
		cmdbyte &= ~slot->disable_cmdbits;
		cmdbyte |= slot->enable_cmdbits;
		rc = i8042_cmdbyte(cmdbyte);
		if (rc) {
			oskit_osenv_irq_free(slot->bus->irq,
					     slot->irq, &i8042_interrupt,
					     slot->bus);
		}
		else
			slot->bus->cmdbyte = cmdbyte;
	}

	if (rc) {
		sfree (s, sizeof *s);
		return rc;
	}

	*out_stream = &s->streami; /* only ref */
	return 0;
}

static struct oskit_streamdev_ops slot_device_ops = {
	slot_query,
	slot_addref,
	slot_release,
	slot_getinfo,
	slot_getdriver,
	slot_open
};


oskit_error_t
oskit_dev_init_i8042(void)
{
#ifndef KNIT
	/* Make sure the ISA bus tree is initialized. */
	osenv_isabus_init();
#endif

	return osenv_driver_register((oskit_driver_t*)&i8042_driver,
				     &oskit_isa_driver_iid, 1);
}
