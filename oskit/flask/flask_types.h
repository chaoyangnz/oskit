/*
 * Copyright (c) 1999 The University of Utah and the Flux Group.
 * All rights reserved.
 * 
 * Contributed by the Computer Security Research division,
 * INFOSEC Research and Technology Office, NSA.
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
#ifndef _OSKIT_FLASK_TYPES_H_
#define _OSKIT_FLASK_TYPES_H_

#include <oskit/types.h>

typedef char* oskit_security_context_t;

typedef oskit_u32_t oskit_security_id_t;
#define OSKIT_SECSID_NULL			0x00000000
#define OSKIT_SECSID_WILD			0xFFFFFFFF		

typedef oskit_u32_t oskit_access_vector_t;

typedef oskit_u16_t oskit_security_class_t;
#define OSKIT_SECCLASS_NULL			0x0000

#endif
