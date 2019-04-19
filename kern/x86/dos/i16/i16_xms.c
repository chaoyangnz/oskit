/*
 * Copyright (c) 1994-1995, 1998 University of Utah and the Flux Group.
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
#if 0

#include <mach/machine/code16.h>
#include <mach/machine/vm_types.h>
#include <mach/machine/far_ptr.h>
#include <mach/machine/asm.h>

#include "i16_a20.h"
#include "phys_mem.h"
#include "debug.h"


struct far_pointer_16 xms_control;

#define CALL_XMS "lcallw "SEXT(xms_control)


static oskit_addr_t xms_phys_free_mem;
static oskit_size_t xms_phys_free_size;

static short free_handle;
static char free_handle_allocated;
static char free_handle_locked;


CODE32

void xms_mem_collect(void)
{
	if (xms_phys_free_mem)
	{
		phys_mem_add(xms_phys_free_mem, xms_phys_free_size);
		xms_phys_free_mem = 0;
	}
}

CODE16

static void i16_xms_enable_a20(void)
{
	short success;
	asm volatile(CALL_XMS : "=a" (success) : "a" (0x0500) : "ebx");
	if (!success)
		i16_die("XMS error: can't enable A20 line");
}

static void i16_xms_disable_a20(void)
{
	short success;
	asm volatile(CALL_XMS : "=a" (success) : "a" (0x0600) : "ebx");
	if (!success)
		i16_die("XMS error: can't disable A20 line");
}

void i16_xms_check()
{
	unsigned short rc;
	unsigned short free_k;

	/* Check for an XMS server.  */
	asm volatile(
	"	int $0x2f\n"
	: "=a" (rc)
	: "a" (0x4300));
	if ((rc & 0xff) != 0x80)
		return;

	/* Get XMS driver's control function.  */
	asm volatile(
	"	pushl	%%ds\n"
	"	pushl	%%es\n"
	"	int	$0x2f\n"
	"	movw	%%es,%0\n"
	"	popl	%%es\n"
	"	popl	%%ds\n"
	: "=r" (xms_control.seg), "=b" (xms_control.ofs)
	: "a" (0x4310));

	/* See how much memory is available.  */
	asm volatile(CALL_XMS
	  : "=a" (free_k)
	  : "a" (0x0800)
	  : "ebx", "edx");
	if (free_k * 1024 == 0)
		return;

	xms_phys_free_size = (unsigned)free_k * 1024;

	/* Grab the biggest memory block we can get.  */
	asm volatile(CALL_XMS
	  : "=a" (rc), "=d" (free_handle)
	  : "a" (0x0900), "d" (free_k)
	  : "ebx");
	if (!rc)
		i16_die("XMS error: can't allocate extended memory");

	free_handle_allocated = 1;

	/* Lock it down.  */
	asm volatile(CALL_XMS "\n"
	"	shll	$16,%%edx\n"
	"	movw	%%bx,%%dx\n"
	: "=a" (rc), "=d" (xms_phys_free_mem)
	: "a" (0x0c00), "d" (free_handle)
	: "ebx");
	if (!rc)
		i16_die("XMS error: can't lock down extended memory");

	free_handle_locked = 1;

	/* We need to update phys_mem_max here
	   instead of just letting phys_mem_add() do it
	   when the memory is collected with phys_mem_collect(),
	   because VCPI initialization needs to know the top of physical memory
	   before phys_mem_collect() is called.
	   See i16_vcpi.c for the gross details.  */
	if (phys_mem_max < xms_phys_free_mem + xms_phys_free_size)
		phys_mem_max = xms_phys_free_mem + xms_phys_free_size;

	i16_enable_a20 = i16_xms_enable_a20;
	i16_disable_a20 = i16_xms_disable_a20;

	do_debug(i16_puts("XMS detected"));
}

void i16_xms_shutdown()
{
	unsigned short rc;

	if (free_handle_locked)
	{
		/* Unlock our memory block.  */
		asm volatile(CALL_XMS
		  : "=a" (rc)
		  : "a" (0x0d00), "d" (free_handle)
		  : "ebx");
		free_handle_locked = 0;
		if (!rc)
			i16_die("XMS error: can't unlock extended memory");
	}

	if (free_handle_allocated)
	{
		/* Free the memory block.  */
		asm volatile(CALL_XMS
		  : "=a" (rc)
		  : "a" (0x0a00), "d" (free_handle)
		  : "ebx");
		free_handle_allocated = 0;
		if (!rc)
			i16_die("XMS error: can't free extended memory");
	}
}

#endif /* 0 */
