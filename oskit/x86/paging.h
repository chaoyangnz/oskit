/*
 * Copyright (c) 1995-1998 University of Utah and the Flux Group.
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
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	Definitions relating to x86 page directories and page tables.
 */
#ifndef	_OSKIT_X86_PAGING_H_
#define _OSKIT_X86_PAGING_H_

#include <oskit/page.h>

#define INTEL_OFFMASK	0xfff	/* offset within page */
#define PDESHIFT	22	/* page descriptor shift */
#define PDEMASK		0x3ff	/* mask for page descriptor index */
#define PTESHIFT	12	/* page table shift */
#define PTEMASK		0x3ff	/* mask for page table index */

/*
 *	Convert linear offset to page descriptor/page table index
 */
#define lin2pdenum(a)	(((a) >> PDESHIFT) & PDEMASK)
#define lin2ptenum(a)	(((a) >> PTESHIFT) & PTEMASK)

/*
 *	Convert page descriptor/page table index to linear address
 */
#define pdenum2lin(a)	((oskit_addr_t)(a) << PDESHIFT)
#define ptenum2lin(a)	((oskit_addr_t)(a) << PTESHIFT)

/*
 *	Number of ptes/pdes in a page table/directory.
 */
#define NPTES	(ptoa(1)/sizeof(pt_entry_t))
#define NPDES	(ptoa(1)/sizeof(pt_entry_t))

/*
 *	Hardware pte bit definitions (to be used directly on the ptes
 *	without using the bit fields).
 */
#define INTEL_PTE_VALID		0x00000001
#define INTEL_PTE_WRITE		0x00000002
#define INTEL_PTE_USER		0x00000004
#define INTEL_PTE_WTHRU		0x00000008
#define INTEL_PTE_NCACHE 	0x00000010
#define INTEL_PTE_REF		0x00000020
#define INTEL_PTE_MOD		0x00000040
#define INTEL_PTE_GLOBAL	0x00000100
#define INTEL_PTE_AVAIL		0x00000e00
#define INTEL_PTE_PFN		0xfffff000

#define INTEL_PDE_VALID		0x00000001
#define INTEL_PDE_WRITE		0x00000002
#define INTEL_PDE_USER		0x00000004
#define INTEL_PDE_WTHRU		0x00000008
#define INTEL_PDE_NCACHE 	0x00000010
#define INTEL_PDE_REF		0x00000020
#define INTEL_PDE_MOD		0x00000040	/* only for superpages */
#define INTEL_PDE_SUPERPAGE	0x00000080
#define INTEL_PDE_GLOBAL	0x00000100	/* only for superpages */
#define INTEL_PDE_AVAIL		0x00000e00
#define INTEL_PDE_PFN		0xfffff000

/*
 *	Macros to translate between page table entry values
 *	and physical addresses.
 */
#define	pa_to_pte(a)		((a) & INTEL_PTE_PFN)
#define	pte_to_pa(p)		((p) & INTEL_PTE_PFN)
#define	pte_increment_pa(p)	((p) += INTEL_OFFMASK+1)

#define	pa_to_pde(a)		((a) & INTEL_PDE_PFN)
#define	pde_to_pa(p)		((p) & INTEL_PDE_PFN)
#define	pde_increment_pa(p)	((p) += INTEL_OFFMASK+1)

/*
 *	Superpage-related macros.
 */
#define SUPERPAGE_SHIFT		PDESHIFT
#define SUPERPAGE_SIZE		(1 << SUPERPAGE_SHIFT)
#define SUPERPAGE_MASK		(SUPERPAGE_SIZE - 1)

#define round_superpage(x)	((oskit_addr_t)((((oskit_addr_t)(x))	\
				+ SUPERPAGE_MASK) & ~SUPERPAGE_MASK))
#define trunc_superpage(x)	((oskit_addr_t)(((oskit_addr_t)(x))	\
				& ~SUPERPAGE_MASK))

#define	superpage_aligned(x)	((((oskit_addr_t)(x)) & SUPERPAGE_MASK) == 0)


#ifndef ASSEMBLER

#include <oskit/compiler.h>
#include <oskit/x86/types.h>
#include <oskit/x86/proc_reg.h>

/*
 *	i386/i486/i860 Page Table Entry
 */
typedef unsigned int	pt_entry_t;
#define PT_ENTRY_NULL	((pt_entry_t *) 0)

typedef unsigned int	pd_entry_t;
#define PD_ENTRY_NULL	((pt_entry_t *) 0)

/*
 * Read and write the page directory base register (PDBR).
 */
#define set_pdbr(pdir)	set_cr3(pdir)
#define get_pdbr()	get_cr3()

/*
 * Invalidate the entire TLB.
 */
#define inval_tlb()	set_pdbr(get_pdbr())

OSKIT_INLINE void paging_enable(oskit_addr_t pdir);
OSKIT_INLINE void paging_disable(void);

/*
 * Load page directory 'pdir' and turn paging on.
 * Assumes that 'pdir' equivalently maps the physical memory
 * that contains the currently executing code,
 * the currently loaded GDT and IDT, etc.
 */
OSKIT_INLINE void paging_enable(oskit_addr_t pdir)
{
	/* Load the page directory.  */
	set_cr3(pdir);

	/* Turn on paging.  */
	asm volatile(
	"	movl	%0,%%cr0\n"
	"	jmp	1f\n"
	"1:\n"
	: : "r" (get_cr0() | CR0_PG));
}

/*
 * Turn paging off.
 * Assumes that the currently loaded page directory
 * equivalently maps the physical memory
 * that contains the currently executing code,
 * the currently loaded GDT and IDT, etc.
 */
OSKIT_INLINE void paging_disable(void)
{
	/* Turn paging off.  */
	asm volatile(
	"	movl	%0,%%cr0\n"
	"	jmp	1f\n"
	"1:\n"
	: : "r" (get_cr0() & ~CR0_PG));

	/* Flush the TLB.  */
	set_cr3(0);
}

#endif /* !ASSEMBLER */

#endif	/* _OSKIT_X86_PAGING_H_ */
