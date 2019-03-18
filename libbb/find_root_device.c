/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell 
 * Permission has been granted to redistribute this code under the GPL.
 *
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "libbb.h"



extern int find_real_root_device_name(char* name)
{
	DIR *dir;
	struct dirent *entry;
	struct stat statBuf, rootStat;
	char fileName[BUFSIZ];

	if (stat("/", &rootStat) != 0) {
		error_msg("could not stat '/'");
		return( FALSE);
	}

	dir = opendir("/dev");
	if (!dir) {
		error_msg("could not open '/dev'");
		return( FALSE);
	}

	while((entry = readdir(dir)) != NULL) {

		/* Must skip ".." since that is "/", and so we 
		 * would get a false positive on ".."  */
		if (strcmp(entry->d_name, "..") == 0)
			continue;

		snprintf( fileName, strlen(name)+1, "/dev/%s", entry->d_name);

		if (stat(fileName, &statBuf) != 0)
			continue;
		/* Some char devices have the same dev_t as block
		 * devices, so make sure this is a block device */
		if (! S_ISBLK(statBuf.st_mode))
			continue;
		if (statBuf.st_rdev == rootStat.st_rdev) {
			strcpy(name, fileName); 
			return ( TRUE);
		}
	}

	return( FALSE);
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
