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
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.
 *
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
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

#ifndef	_OSKIT_X86_SEG_H_
#define	_OSKIT_X86_SEG_H_


/*
 * x86 segmentation.
 */

#ifndef ASSEMBLER

/*
 * Real segment descriptor.
 */
struct x86_desc {
	unsigned int	limit_low:16,	/* limit 0..15 */
			base_low:16,	/* base  0..15 */
			base_med:8,	/* base  16..23 */
			access:8,	/* access byte */
			limit_high:4,	/* limit 16..19 */
			granularity:4,	/* granularity */
			base_high:8;	/* base 24..31 */
};

/*
 * Trap, interrupt, or call gate.
 */
struct x86_gate {
	unsigned int	offset_low:16,	/* offset 0..15 */
			selector:16,
			word_count:8,
			access:8,
			offset_high:16;	/* offset 16..31 */
};

#endif /* !ASSEMBLER */

#define	SZ_32		0x4			/* 32-bit segment */
#define SZ_16		0x0			/* 16-bit segment */
#define	SZ_G		0x8			/* 4K limit field */

#define	ACC_A		0x01			/* accessed */
#define	ACC_TYPE	0x1e			/* type field: */

#define	ACC_TYPE_SYSTEM	0x00			/* system descriptors: */

#define	ACC_LDT		0x02			    /* LDT */
#define	ACC_CALL_GATE_16 0x04			    /* 16-bit call gate */
#define	ACC_TASK_GATE	0x05			    /* task gate */
#define	ACC_TSS		0x09			    /* task segment */
#define	ACC_CALL_GATE	0x0c			    /* call gate */
#define	ACC_INTR_GATE	0x0e			    /* interrupt gate */
#define	ACC_TRAP_GATE	0x0f			    /* trap gate */

#define	ACC_TSS_BUSY	0x02			    /* task busy */

#define	ACC_TYPE_USER	0x10			/* user descriptors */

#define	ACC_DATA	0x10			    /* data */
#define	ACC_DATA_W	0x12			    /* data, writable */
#define	ACC_DATA_E	0x14			    /* data, expand-down */
#define	ACC_DATA_EW	0x16			    /* data, expand-down,
							     writable */
#define	ACC_CODE	0x18			    /* code */
#define	ACC_CODE_R	0x1a			    /* code, readable */
#define	ACC_CODE_C	0x1c			    /* code, conforming */
#define	ACC_CODE_CR	0x1e			    /* code, conforming,
						       readable */
#define	ACC_PL		0x60			/* access rights: */
#define	ACC_PL_K	0x00			/* kernel access only */
#define	ACC_PL_U	0x60			/* user access */
#define	ACC_P		0x80			/* segment present */

/*
 * Components of a selector
 */
#define	SEL_LDT		0x04			/* local selector */
#define	SEL_PL		0x03			/* privilege level: */
#define	SEL_PL_K	0x00			    /* kernel selector */
#define	SEL_PL_U	0x03			    /* user selector */

/*
 * Convert selector to descriptor table index.
 */
#define	sel_idx(sel)	((sel)>>3)


#ifndef ASSEMBLER

#include <oskit/compiler.h>

/* Return the privilege level of a segment selector */
#define ISPL(s)         ((s) & SEL_PL)

#define USERMODE(s, f)          (ISPL(s) == SEL_PL_U || ((f) & EFL_VM) != 0)
#define KERNELMODE(s, f)        (ISPL(s) == SEL_PL_K && ((f) & EFL_VM) == 0)


/* Format of a "pseudo-descriptor", used for loading the IDT and GDT.  */
struct pseudo_descriptor
{
	short pad;
	unsigned short limit;
	unsigned long linear_base;
};

OSKIT_INLINE void
fill_descriptor(struct x86_desc *desc, unsigned base, unsigned limit,
		unsigned char access, unsigned char sizebits);
OSKIT_INLINE void
fill_descriptor_base(struct x86_desc *desc, unsigned base);
OSKIT_INLINE void
fill_descriptor_limit(struct x86_desc *desc, unsigned limit);
OSKIT_INLINE void
fill_gate(struct x86_gate *gate, unsigned offset, unsigned short selector,
	  unsigned char access, unsigned char word_count);

/* Fill a segment descriptor.  */
OSKIT_INLINE void
fill_descriptor(struct x86_desc *desc, unsigned base, unsigned limit,
		unsigned char access, unsigned char sizebits)
{
	if (limit > 0xfffff)
	{
		limit >>= 12;
		sizebits |= SZ_G;
	}
	desc->limit_low = limit & 0xffff;
	desc->base_low = base & 0xffff;
	desc->base_med = (base >> 16) & 0xff;
	desc->access = access | ACC_P;
	desc->limit_high = limit >> 16;
	desc->granularity = sizebits;
	desc->base_high = base >> 24;
}

/* Set the base address in a segment descriptor.  */
OSKIT_INLINE void
fill_descriptor_base(struct x86_desc *desc, unsigned base)
{
	desc->base_low = base & 0xffff;
	desc->base_med = (base >> 16) & 0xff;
	desc->base_high = base >> 24;
}

/* Set the limit in a segment descriptor.  */
OSKIT_INLINE void
fill_descriptor_limit(struct x86_desc *desc, unsigned limit)
{
	if (limit > 0xfffff)
	{
		limit >>= 12;
		desc->granularity |= SZ_G;
	}
	else
		desc->granularity &= ~SZ_G;
	desc->limit_low = limit & 0xffff;
	desc->limit_high = limit >> 16;
}

/* Fill a gate with particular values.  */
OSKIT_INLINE void
fill_gate(struct x86_gate *gate, unsigned offset, unsigned short selector,
	  unsigned char access, unsigned char word_count)
{
	gate->offset_low = offset & 0xffff;
	gate->selector = selector;
	gate->word_count = word_count;
	gate->access = access | ACC_P;
	gate->offset_high = (offset >> 16) & 0xffff;
}

#ifdef CODE16
#define i16_fill_descriptor fill_descriptor
#define i16_fill_gate fill_gate
#endif

#endif /* !ASSEMBLER */

#endif	/* _OSKIT_X86_SEG_H_ */
