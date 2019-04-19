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

#include <stdlib.h>
#include <string.h>

#include <oskit/lmm.h>
#include <oskit/x86/base_paging.h>
#include <oskit/x86/pc/phys_lmm.h>

int ptab_alloc(oskit_addr_t *out_ptab_pa)
{
	void *ptab_va = lmm_alloc_page(&malloc_lmm, 0);
	if (ptab_va == 0)
		return -1;

	/* Clear it out to make sure all entries are invalid.  */
	memset(ptab_va, 0, 4096);

	*out_ptab_pa = kvtophys(ptab_va);
	return 0;
}

