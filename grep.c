/* vi: set sw=4 ts=4: */
/*
 * Mini grep implementation for busybox
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

/*
	18-Dec-1999	Konstantin Boldyshev <konst@voshod.com>

	+ -q option (be quiet) 
	+ exit code depending on grep result (TRUE or FALSE)
	  (useful for scripts)
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
#define BB_DECLARE_EXTERN
#define bb_need_too_few_args
#include "messages.c"

static const char grep_usage[] =
	"grep [OPTIONS]... PATTERN [FILE]...\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nSearch for PATTERN in each FILE or standard input.\n\n"
	"OPTIONS:\n"
	"\t-h\tsuppress the prefixing filename on output\n"
	"\t-i\tignore case distinctions\n"
	"\t-n\tprint line number with output lines\n"
	"\t-q\tbe quiet. Returns 0 if result was found, 1 otherwise\n"
	"\t-v\tselect non-matching lines\n\n"
#if defined BB_REGEXP
	"This version of grep matches full regular expressions.\n";
#else
	"This version of grep matches strings (not regular expressions).\n"
#endif
#endif
	;

static int match = FALSE, beQuiet = FALSE;

static void do_grep(FILE * fp, char *needle, char *fileName, int tellName,
					int ignoreCase, int tellLine, int invertSearch)
{
	long line = 0;
	char *haystack;
	int  truth = !invertSearch;

	while ((haystack = cstring_lineFromFile(fp))) {
		line++;
		if (find_match(haystack, needle, ignoreCase) == truth) {
			if (tellName == TRUE)
				printf("%s:", fileName);

			if (tellLine == TRUE)
				printf("%ld:", line);

			if (beQuiet == FALSE)
				fputs(haystack, stdout);

			match = TRUE;
		}
		free(haystack);
	}
}


extern int grep_main(int argc, char **argv)
{
	FILE *fp;
	char *needle;
	char *fileName;
	int tellName     = TRUE;
	int ignoreCase   = FALSE;
	int tellLine     = FALSE;
	int invertSearch = FALSE;

	if (argc < 1) {
		usage(grep_usage);
	}
	argv++;

	while (--argc >= 0 && *argv && (**argv == '-')) {
		while (*++(*argv)) {
			switch (**argv) {
			case 'i':
				ignoreCase = TRUE;
				break;

			case 'h':
				tellName = FALSE;
				break;

			case 'n':
				tellLine = TRUE;
				break;

			case 'q':
				beQuiet = TRUE;
				break;

			case 'v':
				invertSearch = TRUE;
				break;

			default:
				usage(grep_usage);
			}
		}
		argv++;
	}

	if (argc == 0 || *argv == NULL) {
		fatalError(too_few_args, "grep");
	}

	needle = *argv++;
	argc--;

	if (argc == 0) {
		do_grep(stdin, needle, "stdin", FALSE, ignoreCase, tellLine, invertSearch);
	} else {
		/* Never print the filename for just one file */
		if (argc == 1)
			tellName = FALSE;
		while (argc-- > 0) {
			fileName = *argv++;

			fp = fopen(fileName, "r");
			if (fp == NULL) {
				perror(fileName);
				continue;
			}

			do_grep(fp, needle, fileName, tellName, ignoreCase, tellLine, invertSearch);

			if (ferror(fp))
				perror(fileName);
			fclose(fp);
		}
	}
	return(match);
}


/* END CODE */
