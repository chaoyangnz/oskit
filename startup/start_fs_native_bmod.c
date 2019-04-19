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

/*
 * start_fs_native_bmod.c
 *
 * start up bmod in native mode, copying in from OSKITROOT.
 */
#include <oskit/dev/error.h>
#include <oskit/fs/dir.h>
#include <oskit/fs/fsnamespace.h>
#include <oskit/fs/memfs.h>
#include <oskit/clientos.h>
#include <oskit/startup.h>
#include <oskit/config.h>

#include <oskit/c/stdio.h>
#include <oskit/c/stdlib.h>
#include <oskit/c/string.h>
#include <oskit/c/assert.h>

/* Defined in the unix library */
int	oskit_unix_bmod_copydir(oskit_dir_t *rootdir, const char *path);

static void		start_fs_native_bmod_cleanup(void *arg);

void
start_fs_native_bmod(void)
{
	oskit_filesystem_t *memfs;
	oskit_fsnamespace_t *fsn;
	oskit_dir_t        *root;
	int 	rc;
	char		*fsname;
	char		buf[BUFSIZ], *bp = buf;

#ifdef KNIT
	rc = oskit_memfs_init(&memfs);
#else
	rc = oskit_memfs_init(start_osenv(), &memfs);
#endif
	if (rc) {
		printf("start_fs_native_bmod:  Could not allocate a bmod!\n");
		return;
	}

	rc = oskit_filesystem_getroot(memfs, &root);
	if (rc) {
		printf("start_fs_native_bmod:  Could not getroot the bmod!\n");
		return;
	}
	oskit_filesystem_release(memfs);

	/*
	 * Populate the bmodfs if OSKITROOT is set. The format is a colon
	 * separated list of directories to be copied in.
	 */
        fsname = (char *)getenv("OSKITROOT");
	
        if (fsname) {	
		printf("OSKITROOT=%s\n", fsname);
	
		while (*fsname) {
			while (*fsname && *fsname != ':')
				*bp++ = *fsname++;
			*bp = '\0';

			oskit_unix_bmod_copydir(root, buf);

			if (*fsname == ':')
				fsname++;
			bp = buf;
		}
	}

	/*
	 * Create the filesystem namespace and set it.
	 */
	if (oskit_create_fsnamespace(root, root, &fsn)) {
		printf("start_fs_native_bmod: "
		       "Could not create filesystem namespace!\n");
		exit(69);
	}

	/*
	 * Release our reference since the fsnamespace took a couple.
	 */
	oskit_dir_release(root);

	/*
	 * Initialize the filesystem namespace for the program.
	 */
	oskit_clientos_setfsnamespace(fsn);

	/*
	 * And release our reference since the clientos took one.
	 */
	oskit_fsnamespace_release(fsn);

	/*
	 * Set up to clear out the clientos fsname handle at exit.
	 */
	startup_atexit(start_fs_native_bmod_cleanup, NULL);
}

/*
 * Must release the filesystem namespace upon exit so that everything
 * gets cleared away. Of course, this is not strictly required in a
 * memory filesystem, but this code is supposed to be demonstration code,
 * so might as well be complete.
 */
static void
start_fs_native_bmod_cleanup(void *arg)
{
	oskit_clientos_setfsnamespace(NULL);
}

