/* vi: set sw=4 ts=4: */
/*
 * Mini logname implementation for busybox
 *
 * Copyright (C) 2000  Edward Betts <edward@debian.org>.
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

static const char logname_usage[] = "logname\n\n"

	"Print the name of the current user.\n";

extern int logname_main(int argc, char **argv)
{
	char *cp;

	if (argc > 1)
		usage(logname_usage);

	cp = getlogin();
	if (cp) {
		puts(cp);
		exit(TRUE);
	}
	fprintf(stderr, "%s: no login name\n", argv[0]);
	exit(FALSE);
}