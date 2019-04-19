/*
 * Copyright (c) 1997-1998,2002 University of Utah and the Flux Group.
 * All rights reserved.
 * @OSKIT-FLUX-GPLUS@
 */
/*
 * Definition of the oskit_streamdev interface representing
 * stream devices.
 */
#ifndef _OSKIT_DEV_STREAM_H_
#define _OSKIT_DEV_STREAM_H_

#include <oskit/dev/device.h>

struct oskit_stream;


/*
 * Standard stream device node interface, derived from oskit_device_t,
 * IID 4aa7dfa6-7c74-11cf-b500-08000953adc2.
 * Various tidbits of information can be obtained through this interface;
 * however, in order to read and write the device itself,
 * the open() method in this interface must be called
 * to obtain a per-open stream object (see com/stream.h).
 * Opening the device indicates that it is about to be used;
 * this typically causes the driver to allocate additional resources
 * necessary to access the device rather than merely "know about" it.
 * The device driver isn't obligated to do anything in particular, however;
 * in fact, it may simply export the stream interface
 * as part of the device node object itself,
 * and return a reference to that as the result of the open() operation.
 */
struct oskit_streamdev
{
	struct oskit_streamdev_ops *ops;
};
typedef struct oskit_streamdev oskit_streamdev_t;

struct oskit_streamdev_ops
{
	/* COM-specified IUnknown interface operations */
	OSKIT_COMDECL_IUNKNOWN(oskit_streamdev_t)

	/* Base fdev device interface operations */
	OSKIT_COMDECL	(*getinfo)(oskit_streamdev_t *fdev,
				   oskit_devinfo_t *out_info);
	OSKIT_COMDECL	(*getdriver)(oskit_streamdev_t *fdev,
				     oskit_driver_t **out_driver);

	/* Stream device interface operations */

	/*
	 * Open the device, returning a stream to be used.
	 * The flags are not necessarily used for anything,
	 * but using this method signature makes this interface
	 * a common prefix of oskit_ttydev_ops.
	 */
	OSKIT_COMDECL	(*open)(oskit_streamdev_t *dev, oskit_u32_t flags,
				struct oskit_stream **out_stream);
};

/* GUID for fdev block device interface */
extern const struct oskit_guid oskit_streamdev_iid;
#define OSKIT_STREAMDEV_IID OSKIT_GUID(0x4aa7dfad, 0x7c74, 0x11cf, \
		0xb5, 0x00, 0x08, 0x00, 0x09, 0x53, 0xad, 0xc2)

#define oskit_streamdev_query(dev, iid, out_ihandle) \
	((dev)->ops->query((oskit_streamdev_t *)(dev), (iid), (out_ihandle)))
#define oskit_streamdev_addref(dev) \
	((dev)->ops->addref((oskit_streamdev_t *)(dev)))
#define oskit_streamdev_release(dev) \
	((dev)->ops->release((oskit_streamdev_t *)(dev)))
#define oskit_streamdev_getinfo(fdev, out_info) \
	((fdev)->ops->getinfo((oskit_streamdev_t *)(fdev), (out_info)))
#define oskit_streamdev_getdriver(fdev, out_driver) \
	((fdev)->ops->getdriver((oskit_streamdev_t *)(fdev), (out_driver)))
#define oskit_streamdev_open(dev, flags, out_stream) \
	((dev)->ops->open((oskit_streamdev_t *)(dev), (flags), (out_stream)))

#endif /* _OSKIT_DEV_TTY_H_ */
