
#include <oskit/dev/linux.h>

#include "glue.h"

oskit_error_t oskit_linux_init_entropy(void)
{
#define entropy(name, description, vendor, author, filename, probe) \
    INIT_DRIVER(entropy_##name, description " Entropy driver");
#include <oskit/dev/linux_entropy.h>
    return 0;
}
