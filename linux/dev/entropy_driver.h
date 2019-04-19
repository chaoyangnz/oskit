
#ifndef _LINUX_ENTROPY_DRIVER_H_
#define _LINUX_ENTROPY_DRIVER_H_

#include "glue.h"

struct device
{
    char *name;
    struct device *next;
    int (*open)(struct device *dev);
    void *my_alias;
};

struct entropy_driver {
    struct driver_struct              ds;
    oskit_devinfo_t                   dev_info;
    struct oskit_osenv_entropy_ops    *dev_ops;
    const char                        *basename;
    oskit_guid_t                      *dev_iids;
    int                               dev_niids;
    int                               (*probe)(struct device *dev);
};

oskit_error_t oskit_linux_entropy_init(void);
oskit_error_t oskit_linux_entropy_probe(struct driver_struct *);

#endif /* _LINUX_ENTROPY_DRIVER_H_ */
