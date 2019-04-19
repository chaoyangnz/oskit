/*
 * Copyright (c) 1997-2000 University of Utah and the Flux Group.
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

#include <unistd.h>
#include <fs.h>
#include <fd.h>
#include "posix.h"

int
fchdir(int fd)
{
	oskit_file_t *f;
	oskit_dir_t *d;
	oskit_error_t rc;

	if (fd_check_openfile(fd))
		return -1;

	/* Find the underlying file referred to by this openfile */
	rc = oskit_openfile_getfile(fd_array[fd].openfile, &f);
	if (rc) {
		fd_unlock(fd);
		errno = rc;
		return -1;
	}

	/* Make sure it's actually a directory */
	rc = oskit_file_query(f, &oskit_dir_iid, (void **)&d);
	oskit_file_release(f);
	fd_unlock(fd);
	if (rc) {
		errno = ENOTDIR;
		return -1;
	}

	rc = fs_chdir(d);
	if (!rc)
		oskit_dir_release(d);
	
	return rc;
}

