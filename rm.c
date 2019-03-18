/* vi: set sw=4 ts=4: */
/*
 * Mini rm implementation for busybox
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

#include "internal.h"
#include <stdio.h>
#include <time.h>
#include <utime.h>
#include <dirent.h>
#include <errno.h>

static const char *rm_usage = "rm [OPTION]... FILE...\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nRemove (unlink) the FILE(s).  You may use '--' to\n"
	"indicate that all following arguments are non-options.\n\n"
	"Options:\n"
	"\t-f\t\tremove existing destinations, never prompt\n"
	"\t-r or -R\tremove the contents of directories recursively\n"
#endif
	;


static int recursiveFlag = FALSE;
static int forceFlag = FALSE;
static const char *srcName;


static int fileAction(const char *fileName, struct stat *statbuf, void* junk)
{
	if (unlink(fileName) < 0) {
		perror(fileName);
		return (FALSE);
	}
	return (TRUE);
}

static int dirAction(const char *fileName, struct stat *statbuf, void* junk)
{
	if (rmdir(fileName) < 0) {
		perror(fileName);
		return (FALSE);
	}
	return (TRUE);
}

extern int rm_main(int argc, char **argv)
{
	int stopIt=FALSE;
	struct stat statbuf;

	argc--;
	argv++;

	/* Parse any options */
	while (argc > 0 && stopIt == FALSE) {
		if (**argv == '-') {
			while (*++(*argv))
				switch (**argv) {
					case 'R':
					case 'r':
						recursiveFlag = TRUE;
						break;
					case 'f':
						forceFlag = TRUE;
						break;
					case '-':
						stopIt = TRUE;
						break;
					default:
						usage(rm_usage);
				}
			argc--;
			argv++;
		}
		else
			break;
	}

	if (argc < 1 && forceFlag == FALSE) {
		usage(rm_usage);
	}

	while (argc-- > 0) {
		srcName = *(argv++);
		if (forceFlag == TRUE && lstat(srcName, &statbuf) != 0
			&& errno == ENOENT) {
			/* do not reports errors for non-existent files if -f, just skip them */
		} else {
			if (recursiveAction(srcName, recursiveFlag, FALSE,
								TRUE, fileAction, dirAction, NULL) == FALSE) {
				exit(FALSE);
			}
		}
	}
	return(TRUE);
}
