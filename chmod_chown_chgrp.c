/* vi: set sw=4 ts=4: */
/*
 * Mini chown/chmod/chgrp implementation for busybox
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
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
#define BB_DECLARE_EXTERN
#define bb_need_invalid_option
#include "messages.c"

#include <stdio.h>
#include <grp.h>
#include <pwd.h>


static uid_t uid = -1;
static gid_t gid = -1;
static int whichApp;
static char *invocationName = NULL;
static char *theMode = NULL;


#define CHGRP_APP   1
#define CHOWN_APP   2
#define CHMOD_APP   3

static const char chgrp_usage[] = "chgrp [OPTION]... GROUP FILE...\n\n"
	"Change the group membership of each FILE to GROUP.\n"

	"\nOptions:\n\t-R\tchange files and directories recursively\n";
static const char chown_usage[] =
	"chown [OPTION]...  OWNER[.[GROUP] FILE...\n\n"
	"Change the owner and/or group of each FILE to OWNER and/or GROUP.\n"

	"\nOptions:\n\t-R\tchange files and directories recursively\n";
static const char chmod_usage[] =
	"chmod [-R] MODE[,MODE]... FILE...\n\n"
	"Each MODE is one or more of the letters ugoa, one of the symbols +-= and\n"

	"one or more of the letters rwxst.\n\n"
	"\nOptions:\n\t-R\tchange files and directories recursively.\n";


static int fileAction(const char *fileName, struct stat *statbuf)
{
	switch (whichApp) {
	case CHGRP_APP:
	case CHOWN_APP:
#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)
		if (lchown
			(fileName, (whichApp == CHOWN_APP) ? uid : statbuf->st_uid,
			 (gid == -1) ? statbuf->st_gid : gid) == 0)
#else
		if (chown
			(fileName, (whichApp == CHOWN_APP) ? uid : statbuf->st_uid,
			 (gid == -1) ? statbuf->st_gid : gid) == 0)
#endif
		{
			return (TRUE);
		}
		break;
	case CHMOD_APP:
		/* Parse the specified modes */
		if (parse_mode(theMode, &(statbuf->st_mode)) == FALSE) {
			fatalError( "%s: unknown mode: %s\n", invocationName, theMode);
		}
		if (chmod(fileName, statbuf->st_mode) == 0)
			return (TRUE);
		break;
	}
	perror(fileName);
	return (FALSE);
}

int chmod_chown_chgrp_main(int argc, char **argv)
{
	int recursiveFlag = FALSE;
	char *groupName;
	char *p;
	const char *appUsage;

	whichApp =
		(strcmp(*argv, "chown") == 0)? 
			CHOWN_APP : (strcmp(*argv, "chmod") == 0)? 
				CHMOD_APP : CHGRP_APP;

	appUsage =
		(whichApp == CHOWN_APP)? 
			chown_usage : (whichApp == CHMOD_APP) ? chmod_usage : chgrp_usage;

	if (argc < 2)
		usage(appUsage);
	invocationName = *argv;
	argc--;
	argv++;

	/* Parse options */
	while (**argv == '-') {
		while (*++(*argv))
			switch (**argv) {
			case 'R':
				recursiveFlag = TRUE;
				break;
			default:
				fprintf(stderr, invalid_option, invocationName, **argv);
				usage(appUsage);
			}
		argc--;
		argv++;
	}

	if (whichApp == CHMOD_APP) {
		theMode = *argv;
	} else {

		/* Find the selected group */
		if (whichApp == CHGRP_APP) {
			groupName = *argv;
			gid = strtoul(groupName, &p, 10);	/* maybe it's already numeric */
			if (groupName == p)
				gid = my_getgrnam(groupName);
			if (gid == -1)
				goto bad_group;
		} else {
			groupName = strchr(*argv, '.');
			if (groupName) {
				*groupName++ = '\0';
				gid = strtoul(groupName, &p, 10);
				if (groupName == p)
					gid = my_getgrnam(groupName);
				if (gid == -1)
					goto bad_group;
			} else
				gid = -1;
		}


		/* Find the selected user (if appropriate)  */
		if (whichApp == CHOWN_APP) {
			uid = strtoul(*argv, &p, 10);	/* if numeric ... */
			if (*argv == p)
				uid = my_getpwnam(*argv);
			if (uid == -1) {
				fatalError( "%s: unknown user name: %s\n", 
						invocationName, *argv);
			}
		}
	}

	/* Ok, ready to do the deed now */
	if (argc <= 1) {
		fatalError( "%s: too few arguments\n", invocationName);
	}
	while (argc-- > 1) {
		if (recursiveAction
			(*(++argv), recursiveFlag, TRUE, FALSE, fileAction,
			 fileAction) == FALSE)
			exit(FALSE);
	}
	exit(TRUE);

  bad_group:
	fatalError( "%s: unknown group name: %s\n", invocationName, groupName);
}
