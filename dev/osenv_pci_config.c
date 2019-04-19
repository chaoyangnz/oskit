/*
 * Copyright (c) 1996, 1998, 1999 University of Utah and the Flux Group.
 * All rights reserved.
 * 
 * This file is part of the Flux OSKit.  The OSKit is free software, also known
 * as "open source;" you can redistribute it and/or modify it under the terms
 * of the GNU General Public License (GPL), version 2, as published by the Free
 * Software Foundation (FSF).  To explore alternate licensing terms, contact
 * the University of Utah at csl-dist@cs.utah.edu or +1-801-585-3271.
 * 
 * The OSKit is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GPL for more details.  You should have
 * received a copy of the GPL along with the OSKit; see the file COPYING.  If
 * not, write to the FSF, 59 Temple Place #330, Boston, MA 02111-1307, USA.
 */

/*
 * Default pci config object.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <oskit/com.h>
#include <oskit/dev/dev.h>
#include <oskit/dev/osenv_pci_config.h>

/*
 * There is one and only one pci config interface in this implementation.
 */
static struct oskit_osenv_pci_config_ops	osenv_pci_config_ops;
#ifdef KNIT
       struct oskit_osenv_pci_config		osenv_pci_config_object =
#else
static struct oskit_osenv_pci_config		osenv_pci_config_object =
#endif
						    {&osenv_pci_config_ops};

static OSKIT_COMDECL
pci_config_query(oskit_osenv_pci_config_t *s,
		 const oskit_iid_t *iid, void **out_ihandle)
{
        if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
            memcmp(iid, &oskit_osenv_pci_config_iid, sizeof(*iid)) == 0) {
                *out_ihandle = s;
                return 0;
        }

        *out_ihandle = 0;
        return OSKIT_E_NOINTERFACE;
};

static OSKIT_COMDECL_U
pci_config_addref(oskit_osenv_pci_config_t *s)
{
	/* Only one object */
	return 1;
}

static OSKIT_COMDECL_U
pci_config_release(oskit_osenv_pci_config_t *s)
{
	/* Only one object */
	return 1;
}

static OSKIT_COMDECL
pci_config_init(oskit_osenv_pci_config_t *o)
{
	return osenv_pci_config_init();
}

static OSKIT_COMDECL_U
pci_config_read(oskit_osenv_pci_config_t *o,
		char bus, char device, char function,
		char port, unsigned *data)
{
	return osenv_pci_config_read(bus, device, function, port, data);
}

static OSKIT_COMDECL_U
pci_config_write(oskit_osenv_pci_config_t *o,
		 char bus, char device, char function,
		 char port, unsigned data)
{
	return osenv_pci_config_write(bus, device, function, port, data);
}

static struct oskit_osenv_pci_config_ops osenv_pci_config_ops = {
	pci_config_query,
	pci_config_addref,
	pci_config_release,
	pci_config_init,
	pci_config_read,
	pci_config_write,
};

#ifndef KNIT
/*
 * Return a reference to the one and only interrupt object.
 */
oskit_osenv_pci_config_t *
oskit_create_osenv_pci_config(void)
{
	return &osenv_pci_config_object;
}
#endif
