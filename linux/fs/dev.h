/*
 * Copyright (c) 1997, 1998 The University of Utah and the Flux Group.
 * 
 * This file is part of the OSKit Linux Glue Libraries, which are free
 * software, also known as "open source;" you can redistribute them and/or
 * modify them under the terms of the GNU General Public License (GPL),
 * version 2, as published by the Free Software Foundation (FSF).
 * 
 * The OSKit is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GPL for more details.  You should have
 * received a copy of the GPL along with the OSKit; see the file COPYING.  If
 * not, write to the FSF, 59 Temple Place #330, Boston, MA 02111-1307, USA.
 */
#ifndef _LINUX_FS_DEV_H_
#define _LINUX_FS_DEV_H_

#include <oskit/types.h>
#include <oskit/io/blkio.h>

#include <linux/fs.h>

extern void fs_linux_dev_init();

extern oskit_error_t fs_linux_devtab_insert(oskit_blkio_t *bio, kdev_t *devp);
extern void fs_linux_devtab_delete(kdev_t dev);

#endif /* _LINUX_FS_DEV_H_ */
