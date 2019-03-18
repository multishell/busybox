/*
 * Mini grep implementation for busybox using libc regex.
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Mark Whitley <markw@lineo.com>, <markw@codepoet.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <regex.h>
#include <string.h> /* for strerror() */
#include <errno.h>
#include "busybox.h"

extern void xregcomp(regex_t *preg, const char *regex, int cflags);

extern int optind; /* in unistd.h */
extern int errno;  /* for use with strerror() */

/* options */
static int ignore_case       = 0;
static int print_filename    = 0;
static int print_line_num    = 0;
static int print_count_only  = 0;
static int be_quiet          = 0;
static int invert_search     = 0;
static int suppress_err_msgs = 0;

/* globals */
static regex_t regex; /* storage space for compiled regular expression */
static int matched; /* keeps track of whether we ever matched */
static char *cur_file = NULL; /* the current file we are reading */


static void print_matched_line(char *line, int linenum)
{
	if (print_count_only)
		return;

	if (print_filename)
		printf("%s:", cur_file);
	if (print_line_num)
		printf("%i:", linenum);

	puts(line);
}

static void grep_file(FILE *file)
{
	char *line = NULL;
	int ret;
	int linenum = 0;
	int nmatches = 0;

	while ((line = get_line_from_file(file)) != NULL) {
		if (line[strlen(line)-1] == '\n')
			line[strlen(line)-1] = '\0';
		linenum++;
		ret = regexec(&regex, line, 0, NULL, 0);
		if (ret == 0 && !invert_search) { /* match */

			/* if we found a match but were told to be quiet, stop here and
			 * return success */
			if (be_quiet) {
				regfree(&regex);
				exit(0);
			}

			nmatches++;
			print_matched_line(line, linenum);

		}
		else if (ret == REG_NOMATCH && invert_search) {
			if (be_quiet) {
				regfree(&regex);
				exit(0);
			}

			nmatches++;
			print_matched_line(line, linenum);
		}

		free(line);
	}

	/* special-case post processing */
	if (print_count_only) {
		if (print_filename)
			printf("%s:", cur_file);
		printf("%i\n", nmatches);
	}

	/* record if we matched */
	if (nmatches != 0)
		matched = 1;
}

extern int grep_main(int argc, char **argv)
{
	int opt;
	int reflags;

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "iHhnqvsc")) > 0) {
		switch (opt) {
			case 'i':
				ignore_case++;
				break;
			case 'H':
				print_filename++;
				break;
			case 'h':
				print_filename--;
				break;
			case 'n':
				print_line_num++;
				break;
			case 'q':
				be_quiet++;
				break;
			case 'v':
				invert_search++;
				break;
			case 's':
				suppress_err_msgs++;
				break;
			case 'c':
				print_count_only++;
				break;
		}
	}

	/* argv[optind] should be the regex pattern; no pattern, no worky */
	if (argv[optind] == NULL)
		usage(grep_usage);

	/* compile the regular expression
	 * we're not going to mess with sub-expressions, and we need to
	 * treat newlines right. */
	reflags = REG_NOSUB; 
	if (ignore_case)
		reflags |= REG_ICASE;
	xregcomp(&regex, argv[optind], reflags);

	/* argv[(optind+1)..(argc-1)] should be names of file to grep through. If
	 * there is more than one file to grep, we will print the filenames */
	if ((argc-1) - (optind+1) > 0)
		print_filename++;

	/* If no files were specified, or '-' was specified, take input from
	 * stdin. Otherwise, we grep through all the files specified. */
	if (argv[optind+1] == NULL || (strcmp(argv[optind+1], "-") == 0)) {
		grep_file(stdin);
	}
	else {
		int i;
		FILE *file;
		for (i = optind + 1; i < argc; i++) {
			cur_file = argv[i];
			file = fopen(cur_file, "r");
			if (file == NULL) {
				if (!suppress_err_msgs)
					perror_msg("%s", cur_file);
			}
			else {
				grep_file(file);
				fclose(file);
			}
		}
	}

	regfree(&regex);

	if (!matched)
		return 1;

	return 0;
}
