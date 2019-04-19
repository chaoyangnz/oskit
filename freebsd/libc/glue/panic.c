/*
 * Copyright (c) 1995-1996, 1998, 2000 University of Utah and the Flux Group.
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

/*
 * This function is called by the assert() macro defined in assert.h;
 * it's also a nice simple general-purpose panic function.
 */
void panic(const char *fmt, ...)
{
	va_list vl;
	static int alreadypaniced = 0;
	extern void dump_stack_trace(void);

	if (alreadypaniced)
		exit(69);
	alreadypaniced = 1;

	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);

	printf("\n");

	dump_stack_trace();

	exit(1);
}

