
#ifndef _LINUX_DEV_ENTROPY_H_
#define _LINUX_DEV_ENTROPY_H_

#include <linux/version.h>
#include "glue.h"
#include "entropy_driver.h"

extern struct oskit_entropy_ops oskit_linux_entropy_ops;

/* oskit_unknown, oskit_device_t, oskit_driver_t */
#define ENTROPY_NIIDS 3
extern struct oskit_guid oskit_linux_entropy_iids[ENTROPY_NIIDS];

#define entropy(name, description, vendor, author, filename, probe) \
        extern void rand_initialize(void); \
        static struct entropy_driver fdev_drv = { \
                { { &oskit_linux_driver_ops }, \
                  oskit_linux_entropy_probe, \
		  { #name, description " Entropy driver", vendor, \
		    author, "Linux "UTS_RELEASE } }, \
                { #name, description " Entropy device", vendor, \
                  author, "Linux "UTS_RELEASE }, \
                (struct oskit_entropy_driver_ops*)&oskit_linux_entropy_ops, \
                "entropy", \
                oskit_linux_entropy_iids, ENTROPY_NIIDS, \
                rand_initialize }; \
        oskit_error_t oskit_linux_init_entropy_##name(void) { \
                return oskit_linux_driver_register(&fdev_drv.ds); \
        }

#endif /*  _LINUX_DEV_ENTROPY_H_ */
