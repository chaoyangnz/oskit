/*
 * linux/include/asm-arm/arch-nexuspci/system.h
 *
 * Copyright (c) 1996,1997,1998 Russell King.
 */
#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

#define arch_do_idle()		processor.u.armv3v4._do_idle()

extern __inline__ void arch_reset(char mode)
{
	/*
	 * loop endlessly - the watchdog will reset us if it's enabled.
	 */
	cli();
}

#endif
