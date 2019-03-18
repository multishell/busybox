/* vi: set sw=4 ts=4: */
/*
 * Mini du implementation for busybox
 *
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by John Beppu <beppu@lineo.com>
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
#define BB_DECLARE_EXTERN
#define bb_need_name_too_long
#include "messages.c"

#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>

typedef void (Display) (long, char *);

static int du_depth = 0;
static int count_hardlinks = 0;

static Display *print;

static void print_normal(long size, char *filename)
{
	fprintf(stdout, "%ld\t%s\n", size, filename);
}

static void print_summary(long size, char *filename)
{
	if (du_depth == 1) {
		print_normal(size, filename);
	}
}

/* tiny recursive du */
static long du(char *filename)
{
	struct stat statbuf;
	long sum;
	int len;

	if ((lstat(filename, &statbuf)) != 0) {
		printf("du: %s: %s\n", filename, strerror(errno));
		return 0;
	}

	du_depth++;
	sum = (statbuf.st_blocks >> 1);

	/* Don't add in stuff pointed to by symbolic links */
	if (S_ISLNK(statbuf.st_mode)) {
		sum = 0L;
		if (du_depth == 1)
			print(sum, filename);
	}
	if (S_ISDIR(statbuf.st_mode)) {
		DIR *dir;
		struct dirent *entry;

		dir = opendir(filename);
		if (!dir) {
			du_depth--;
			return 0;
		}

		len = strlen(filename);
		if (filename[len - 1] == '/')
			filename[--len] = '\0';

		while ((entry = readdir(dir))) {
			char newfile[BUFSIZ + 1];
			char *name = entry->d_name;

			if ((strcmp(name, "..") == 0)
				|| (strcmp(name, ".") == 0)) {
				continue;
			}

			if (len + strlen(name) + 1 > BUFSIZ) {
				errorMsg(name_too_long);
				du_depth--;
				return 0;
			}
			sprintf(newfile, "%s/%s", filename, name);

			sum += du(newfile);
		}
		closedir(dir);
		print(sum, filename);
	}
	else if (statbuf.st_nlink > 1 && !count_hardlinks) {
		/* Add files with hard links only once */
		if (is_in_ino_dev_hashtable(&statbuf, NULL)) {
			sum = 0L;
			if (du_depth == 1)
				print(sum, filename);
		}
		else {
			add_to_ino_dev_hashtable(&statbuf, NULL);
		}
	}
	du_depth--;
	return sum;
}

int du_main(int argc, char **argv)
{
	int i;
	int c;

	/* default behaviour */
	print = print_normal;

	/* parse argv[] */
	while ((c = getopt(argc, argv, "sl")) != EOF) {
			switch (c) {
			case 's':
					print = print_summary;
					break;
			case 'l':
					count_hardlinks = 1;
					break;
			default:
					usage(du_usage);
			}
	}

	/* go through remaining args (if any) */
	if (optind >= argc) {
		du(".");
	} else {
		long sum;

		for (i=optind; i < argc; i++) {
			sum = du(argv[i]);
			if (sum && isDirectory(argv[i], FALSE, NULL)) {
				print_normal(sum, argv[i]);
			}
			reset_ino_dev_hashtable();
		}
	}

	return(0);
}

/* $Id: du.c,v 1.25 2000/09/25 21:45:57 andersen Exp $ */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
