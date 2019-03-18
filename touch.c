/* vi: set sw=4 ts=4: */
/*
 * Mini touch implementation for busybox
 *
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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
 */

#include "busybox.h"
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>

extern int touch_main(int argc, char **argv)
{
	int fd;
	int create = TRUE;

	/* Parse options */
	while (--argc > 0 && **(++argv) == '-') {
		while (*(++(*argv))) {
			switch (**argv) {
			case 'c':
				create = FALSE;
				break;
			default:
				usage(touch_usage);
				exit(FALSE);
			}
		}
	}

	if (argc < 1) {
		usage(touch_usage);
	}

	while (argc > 0) {
		fd = open(*argv, (create == FALSE) ? O_RDWR : O_RDWR | O_CREAT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (fd < 0) {
			if (create == FALSE && errno == ENOENT)
				exit(TRUE);
			else {
				fatalError("%s", strerror(errno));
			}
		}
		close(fd);
		if (utime(*argv, NULL)) {
			fatalError("%s", strerror(errno));
		}
		argc--;
		argv++;
	}

	return(TRUE);
}
