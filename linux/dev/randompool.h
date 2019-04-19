
#ifndef _LINUX_RANDOMPOOL_H_
#define _LINUX_RANDOMPOOL_H_

#include "glue.h"

struct randompool_driver {
    struct driver_struct              ds;
    oskit_devinfo_t                   dev_info;
    struct oskit_osenv_randompool_ops *dev_ops;
    const char                        *basename;
    oskit_guid_t                      *dev_iids;
    int                               dev_niids;
    void                              (*init)(void);
};

#endif
