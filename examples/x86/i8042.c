/*
 * Copyright (c) 1997-1999, 2002 University of Utah and the Flux Group.
 * All rights reserved.
 * @OSKIT-FLUX-GPLUS@
 */
/*
 * Example program to test the function of i8042 device driver.
 */

#include <stdio.h>
#include <stdlib.h>

#include <oskit/types.h>
#include <oskit/com/stream.h>
#include <oskit/io/asyncio.h>
#include <oskit/fs/file.h>      /* XXX OSKIT_O_RDWR */
#include <oskit/dev/dev.h>
#include <oskit/dev/stream.h>
#include <oskit/com/stream.h>
#include <oskit/clientos.h>
#include <oskit/startup.h>
#include <oskit/dev/osenv.h>

#include <oskit/debug.h>


int
main(int argc, char **argv)
{
	oskit_streamdev_t **streamdev;
	oskit_stream_t *kbd, *aux;
	oskit_asyncio_t *kbdio, *auxio;
	int ndev, rc;
	oskit_osenv_t *osenv;
	oskit_u32_t wrote;

	oskit_clientos_init();
	osenv = start_osenv();

	oskit_dev_init(osenv);
        oskit_dev_init_i8042();
        oskit_dump_drivers();
        oskit_dev_probe();
        oskit_dump_devices();

	ndev = osenv_device_lookup(&oskit_streamdev_iid, (void***)&streamdev);
        if (ndev <= 0)
                panic("no devices found!");

        rc = oskit_streamdev_open(streamdev[0], OSKIT_O_RDWR, &kbd);
        if (rc)
                panic("unable to open kbd: %x", rc);
	else {
		rc = oskit_stream_query(kbd, &oskit_asyncio_iid,
					(void **)&kbdio);
		if (rc) {
			printf("no asyncio for kbd: %x\n", rc);
			kbdio = NULL;
		}
	}
	if (ndev > 1) {
		rc = oskit_streamdev_open(streamdev[1], OSKIT_O_RDWR, &aux);
		if (rc) {
			printf("unable to open aux: %x\n", rc);
			aux = NULL;
		}
		else {
			rc = oskit_stream_query(aux, &oskit_asyncio_iid,
						(void **)&auxio);
			if (rc) {
				printf("no asyncio for aux: %x\n", rc);
				auxio = NULL;
			}
		}
	}

	rc = oskit_stream_write(kbd, "\xff\xed\x07", 3, &wrote);
	if (rc)
		printf("write of 0xff failed: %x\n", rc);
	else
		printf("wrote %d (ff ed 07)\n", wrote);

	do {
		rc = oskit_asyncio_poll(kbdio);
		if (OSKIT_FAILED(rc))
			panic("kbdio poll: %x", rc);
		if (rc & OSKIT_ASYNCIO_READABLE)
			while (1) {
				oskit_u8_t buf[256];
				oskit_u32_t len;
				rc = oskit_stream_read(kbd, buf, sizeof buf,
						       &len);
				if (rc == OSKIT_EWOULDBLOCK)
					break;
				if (OSKIT_FAILED(rc))
					printf ("read failed with %x\n", rc);
				else {
					int i;
					printf ("kbd %u: ", len);
					for (i = 0; i < len; ++i)
						printf ("%02x ", buf[i]);
					printf ("\n");
				}
			}
		rc = auxio ? oskit_asyncio_poll(auxio) : 0;
		if (OSKIT_FAILED(rc))
			panic("auxio poll: %x", rc);
		if (rc & OSKIT_ASYNCIO_READABLE)
			while (1) {
				oskit_u8_t buf[256];
				oskit_u32_t len;
				rc = oskit_stream_read(aux, buf, sizeof buf,
						       &len);
				if (rc == OSKIT_EWOULDBLOCK)
					break;
				if (OSKIT_FAILED(rc))
					printf ("read failed with %x\n", rc);
				else {
					int i;
					printf ("aux %u: ", len);
					for (i = 0; i < len; ++i)
						printf ("%02x ", buf[i]);
					printf ("\n");
				}
			}
	} while (1);

	exit(1);
}
