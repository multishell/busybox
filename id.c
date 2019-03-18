/* vi: set sw=4 ts=4: */
/*
 * Mini id implementation for busybox
 *
 *
 * Copyright (C) 2000 by Randolph Chung <tausq@debian.org>
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
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>

static const char id_usage[] =
	"id [OPTIONS]... [USERNAME]\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nPrint information for USERNAME or the current user\n\n"
	"Options:\n"
	"\t-g\tprints only the group ID\n"
	"\t-u\tprints only the user ID\n"
	"\t-r\tprints the real user ID instead of the effective ID (with -ug)\n\n"
#endif
	;

extern int id_main(int argc, char **argv)
{
	int no_user = 0, no_group = 0, print_real = 0;
	char *cp, *user, *group;
	unsigned long gid;
	
	cp = user = group = NULL;

	argc--; argv++;

	while (argc > 0) {
		cp = *argv;
		if (*cp == '-') {
			switch (*++cp) {
			case 'u': no_group = 1; break;
			case 'g': no_user = 1; break;
			case 'r': print_real = 1; break;
			default: usage(id_usage);
			}
		} else {
			user = cp;			
		}
		argc--; argv++;
	}

	if (no_user && no_group) usage(id_usage);

	if (user == NULL) {
		user = xmalloc(9);
		group = xmalloc(9);
		if (print_real) {
			my_getpwuid(user, getuid());
			my_getgrgid(group, getgid());
		} else {
			my_getpwuid(user, geteuid());
			my_getgrgid(group, getegid());
		}
	} else {
		group = xmalloc(9);
	    gid = my_getpwnamegid(user);
		my_getgrgid(group, gid);
	}

	if (no_group) printf("%lu\n", my_getpwnam(user));
	else if (no_user) printf("%lu\n", my_getgrnam(group));
	else
		printf("uid=%lu(%s) gid=%lu(%s)\n",
			   my_getpwnam(user), user, my_getgrnam(group), group);
	

	return(0);
}


/* END CODE */
