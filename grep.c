/*
 * Mini grep implementation for busybox
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
#include "regexp.h"
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

static const char grep_usage[] =
"grep [-ihn]... PATTERN [FILE]...\n"
"Search for PATTERN in each FILE or standard input.\n\n"
"\t-h\tsuppress the prefixing filename on output\n"
"\t-i\tignore case distinctions\n"
"\t-n\tprint line number with output lines\n\n"
#if defined BB_REGEXP
"This version of grep matches full regexps.\n";
#else
"This version of grep matches strings (not full regexps).\n";
#endif


extern int grep_main (int argc, char **argv)
{
    FILE *fp;
    char *needle;
    char *name;
    char *cp;
    int tellName=TRUE;
    int ignoreCase=FALSE;
    int tellLine=FALSE;
    long line;
    char haystack[BUF_SIZE];

    ignoreCase = FALSE;
    tellLine = FALSE;

    argc--;
    argv++;
    if (argc < 1) {
	usage(grep_usage);
    }

    if (**argv == '-') {
	argc--;
	cp = *argv++;

	while (*++cp)
	    switch (*cp) {
	    case 'i':
		ignoreCase = TRUE;
		break;

	    case 'h':
		tellName = FALSE;
		break;

	    case 'n':
		tellLine = TRUE;
		break;

	    default:
		usage(grep_usage);
	    }
    }

    needle = *argv++;
    argc--;

    while (argc-- > 0) {
	name = *argv++;

	fp = fopen (name, "r");
	if (fp == NULL) {
	    perror (name);
	    continue;
	}

	line = 0;

	while (fgets (haystack, sizeof (haystack), fp)) {
	    line++;
	    cp = &haystack[strlen (haystack) - 1];

	    if (*cp != '\n')
		fprintf (stderr, "%s: Line too long\n", name);

	    if (find_match(haystack, needle, ignoreCase) == TRUE) {
		if (tellName==TRUE)
		    printf ("%s: ", name);

		if (tellLine==TRUE)
		    printf ("%ld: ", line);

		fputs (haystack, stdout);
	    }
	}

	if (ferror (fp))
	    perror (name);

	fclose (fp);
    }
    exit( TRUE);
}


/* END CODE */


