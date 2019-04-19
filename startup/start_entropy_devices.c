
/*
 * start_entropy_devices.c
 */

#include <oskit/startup.h>
#include <oskit/dev/linux.h>

void
start_entropy_devices(void)
{
	start_devices();
}

/*
 * Having this initialization here makes `start_devices' (start_devices.c)
 * initialize the random pool drivers, since linking in this file means
 * the program is using the entropy store(s).
 */

#ifdef HAVE_CONSTRUCTOR
/*
 * Place initilization in the init section to be called at startup
 */
static void initme(void) __attribute__ ((constructor));

static void
initme(void)
{
	extern void (*_init_devices_entropy)(void);
	_init_devices_entropy = (void (*)(void))oskit_linux_init_entropy;
}
#else
/*
 * Use a (dreaded) common symbol
 */
void (*_init_devices_entropy)(void) = oskit_linux_init_entropy;
#endif
