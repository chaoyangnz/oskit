/*
 * Copyright (c) 1994-1996 Sleepless Software
 * Copyright (c) 1997-1998 University of Utah and the Flux Group.
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
 * Remote serial-line source-level debugging for the Flux OS Toolkit.
 */
/*
 * Generic definitions for remote GDB debugging.
 */
#ifndef _OSKIT_GDB_H_
#define _OSKIT_GDB_H_

#include <oskit/compiler.h>
#include <oskit/machine/types.h>

/*
 * This structure must be defined in <oskit/machine/gdb.h>
 * to represent the processor register state frame as GDB wants it.
 */
struct gdb_state;

/*
 * This structure must be defined in <oskit/machine/base_trap.h>
 * to represent the processor register state
 * as saved by the default trap entrypoint code.
 */
struct trap_state;

OSKIT_BEGIN_DECLS

/*
 * This function provides the necessary glue
 * between the base trap handling mechanism provided by the toolkit
 * and a machine-independent GDB stub such as gdb_serial.
 * This function is intended to be plugged into 'trap_handler_func'
 * (see oskit/machine/base_trap.h).
 */
int gdb_trap(struct trap_state *inout_trap_state);

/*
 * Before the above function is called for the first time,
 * this variable must be set to point to the GDB stub it should call.
 */
extern void (*gdb_signal)(int *inout_signo, struct gdb_state *inout_gdb_state);

/*
 * These functions are provided to copy data
 * into and out of the address space of the program being debugged.
 * Machine-dependent code typically provides default implementations
 * that simply copy data into and out of the kernel's own address space.
 * These functions return zero if the copy succeeds,
 * or nonzero if the memory region couldn't be accessed for some reason.
 */
int gdb_copyin(oskit_addr_t src_va, void *dest_buf, oskit_size_t size);
int gdb_copyout(const void *src_buf, oskit_addr_t dest_va, oskit_size_t size);

/*
 * The GDB stub calls this architecture-specific function
 * to modify the trace flag in the processor state.
 */
void gdb_set_trace_flag(int trace_enable, struct gdb_state *inout_state);

OSKIT_END_DECLS

/* Bring in machine-dependent definitions. */
#include <oskit/machine/gdb.h>

#endif /* _OSKIT_GDB_H_ */
