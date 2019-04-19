/*
 * Copyright (c) 1994-1998 University of Utah and the Flux Group.
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
#ifndef _OSKIT_C_WCHAR_H_
#define _OSKIT_C_WCHAR_H_

#include <oskit/types.h>
#include <oskit/compiler.h>

#ifndef _WCHAR_T
#define _WCHAR_T
typedef oskit_wchar_t wchar_t;
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef oskit_size_t size_t;
#endif

OSKIT_BEGIN_DECLS

size_t wcslen(const wchar_t *ws);
wchar_t *wcscpy(wchar_t *ws1, const wchar_t *ws2);
wchar_t *wcscat(wchar_t *ws1, const wchar_t *ws2);

OSKIT_END_DECLS

#endif _OSKIT_C_WCHAR_H_
