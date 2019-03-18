/*
 * sed.c - very minimalist version of sed
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by Mark Whitley <markw@lineo.com>, <markw@enol.com>
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
	Supported features and commands in this version of sed:

	 - comments ('#')
	 - Address matching: num|/matchstr/[,num|/matchstr/|$]command
	 - Commands: p, d, s/match/replace/[g]
	 
	 (Note: Specifying an address (range) to match is *optional*; commands
	 default to the whole pattern space if no specific address match was
	 requested.)

	Unsupported features:

	 - transliteration (y/source-chars/dest-chars/) (use 'tr')
	 - no support for characters other than the '/' character for regex matches
	 - no pattern space hold space storing / swapping (x, etc.)
	 - no labels / branching (: label, b, t, and friends)
	 - and lots, lots more.

*/

#include <stdio.h>
#include <stdlib.h> /* for realloc() */
#include <unistd.h> /* for getopt() */
#include <regex.h>
#include <string.h> /* for strdup() */
#include <errno.h>
#include <ctype.h> /* for isspace() */
#include "internal.h"


/* externs */
extern int optind; /* in unistd.h */
extern char *optarg; /* ditto */

/* options */
static int be_quiet = 0;

struct sed_cmd {

	/* address storage */
	int beg_line; /* 'sed 1p'   0 == no begining line, apply commands to all lines */
	int end_line; /* 'sed 1,3p' 0 == no end line, use only beginning. -1 == $ */
	regex_t *beg_match; /* sed -e '/match/cmd' */
	regex_t *end_match; /* sed -e '/match/,/end_match/cmd' */

	/* the command */
	char cmd; /* p,d,s (add more at your leisure :-) */

	/* substitution command specific fields */
	regex_t *sub_match; /* sed -e 's/sub_match/replace/' */
	char *replace; /* sed -e 's/sub_match/replace/' XXX: who will hold the \1 \2 \3s? */
	unsigned int sub_g:1; /* sed -e 's/foo/bar/g' (global) */
};

/* globals */
static struct sed_cmd *sed_cmds = NULL; /* growable arrary holding a sequence of sed cmds */
static int ncmds = 0; /* number of sed commands */

/*static char *cur_file = NULL;*/ /* file currently being processed XXX: do I need this? */

static const char sed_usage[] =
	"sed [-Vhnef] pattern [files...]\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n"
	"-n\tsuppress automatic printing of pattern space\n"
	"-e script\tadd the script to the commands to be executed\n"
	"-f scriptfile\tadd the contents of script-file to the commands to be executed\n"
	"-h\tdisplay this help message\n"
	"-V\toutput version information and exit\n"
	"\n"
	"If no -e or -f is given, the first non-option argument is taken as the\n"
	"sed script to interpret. All remaining arguments are names of input\n"
	"files; if no input files are specified, then the standard input is read.\n"
#endif
	;

static void destroy_cmd_strs()
{
	if (sed_cmds == NULL)
		return;

	/* destroy all the elements in the array */
	while (--ncmds >= 0) {

		if (sed_cmds[ncmds].beg_match) {
			regfree(sed_cmds[ncmds].beg_match);
			free(sed_cmds[ncmds].beg_match);
		}
		if (sed_cmds[ncmds].end_match) {
			regfree(sed_cmds[ncmds].end_match);
			free(sed_cmds[ncmds].end_match);
		}
		if (sed_cmds[ncmds].sub_match) {
			regfree(sed_cmds[ncmds].sub_match);
			free(sed_cmds[ncmds].sub_match);
		}
		if (sed_cmds[ncmds].replace)
			free(sed_cmds[ncmds].replace);
	}

	/* destroy the array */
	free(sed_cmds);
	sed_cmds = NULL;
}

static void exit_sed(int retcode, const char *message)
{
	destroy_cmd_strs();
	if (message)
		fputs(message, stderr);
	exit(retcode);
}

/*
 * trim_str - trims leading and trailing space from a string
 * 
 * Note: This returns a malloc'ed string so you must store and free it
 * XXX: This should be in the utility.c file.
 */
static char *trim_str(const char *str)
{
	int i;
	char *retstr = strdup(str);

	/* trim leading whitespace */
	memmove(retstr, &retstr[strspn(retstr, " \n\t\v")], strlen(retstr));

	/* trim trailing whitespace */
	i = strlen(retstr) - 1;
	while (isspace(retstr[i]))
		i--;
	retstr[++i] = 0;

	/* Aside: 
	 *
	 * you know, a strrspn() would really be nice cuz then we could say:
	 * 
	 * retstr[strlen(retstr) - strrspn(retstr, " \n\t\v") + 1] = 0;
	 */
	
	return retstr;
}

/*
 * index_of_unescaped_slash - walks left to right through a string beginning
 * at a specified index and returns the index of the next unescaped slash.
 */
static int index_of_next_unescaped_slash(int idx, const char *str)
{
	do {
		idx++;
		/* test if we've hit the end */
		if (str[idx] == 0)
			return -1;
	} while (str[idx] != '/' && str[idx - 1] != '\\');

	return idx;
}

/*
 * returns the index in the string just past where the address ends.
 */
static int get_address(const char *str, int *line, regex_t **regex)
{
	char *my_str = strdup(str);
	int idx = 0;

	if (isdigit(my_str[idx])) {
		do {
			idx++;
		} while (isdigit(my_str[idx]));
		my_str[idx] = 0;
		*line = atoi(my_str);
		*regex = NULL;
	}
	else if (my_str[idx] == '$') {
		*line = -1;
		*regex = NULL;
		idx++;
	}
	else if (my_str[idx] == '/') {
		idx = index_of_next_unescaped_slash(idx, my_str);
		if (idx == -1)
			exit_sed(1, "sed: unterminated match expression\n");
		my_str[idx] = '\0';
		*regex = (regex_t *)xmalloc(sizeof(regex_t));
		if (bb_regcomp(*regex, my_str+1, REG_NEWLINE) != 0) {
			free(my_str);
			exit_sed(1, NULL);
		}
	}
	else {
		fprintf(stderr, "sed.c:get_address: no address found in string\n");
		fprintf(stderr, "\t(you probably didn't check the string you passed me)\n");
		idx = -1;
	}

	free(my_str);
	return idx;
}

static char *strdup_substr(const char *str, int start, int end)
{
	int size = end - start + 1;
	char *newstr = xmalloc(size);
	memcpy(newstr, str+start, size-1);
	newstr[size-1] = '\0';
	return newstr;
}

static void parse_cmd_str(struct sed_cmd *sed_cmd, const char *cmdstr)
{
	int idx = 0;

	/* parse the command
	 * format is: [addr][,addr]cmd
	 *            |----||-----||-|
	 *            part1 part2  part3
	 */

	/* first part (if present) is an address: either a number or a /regex/ */
	if (isdigit(cmdstr[idx]) || cmdstr[idx] == '/')
		idx = get_address(cmdstr, &sed_cmd->beg_line, &sed_cmd->beg_match);

	/* second part (if present) will begin with a comma */
	if (cmdstr[idx] == ',')
		idx += get_address(&cmdstr[++idx], &sed_cmd->end_line, &sed_cmd->end_match);

	/* last part (mandatory) will be a command */
	if (cmdstr[idx] == '\0')
		exit_sed(1, "sed: missing command\n");
	if (!strchr("pds", cmdstr[idx])) /* <-- XXX add new commands here */
		exit_sed(1, "sed: invalid command\n");
	sed_cmd->cmd = cmdstr[idx];
	/* special-case handling for 's' */
	if (sed_cmd->cmd == 's') {
		int oldidx, cflags = REG_NEWLINE;
		char *match;
		/* format for substitution is:
		 *    s/match/replace/gI
		 *    |               ||
		 *    mandatory       optional
		 */

		/* verify that we have an 's' followed by a 'slash' */
		if (cmdstr[++idx] != '/')
			exit_sed(1, "sed: bad format in substitution expression\n");

		/* save the match string */
		oldidx = idx+1;
		idx = index_of_next_unescaped_slash(idx, cmdstr);
		if (idx == -1)
			exit_sed(1, "sed: bad format in substitution expression\n");
		match = strdup_substr(cmdstr, oldidx, idx);

		/* save the replacement string */
		oldidx = idx+1;
		idx = index_of_next_unescaped_slash(idx, cmdstr);
		if (idx == -1)
			exit_sed(1, "sed: bad format in substitution expression\n");
		sed_cmd->replace = strdup_substr(cmdstr, oldidx, idx);

		/* process the flags */
		while (cmdstr[++idx]) {
			switch (cmdstr[idx]) {
			case 'g':
				sed_cmd->sub_g = 1;
				break;
			case 'I':
				cflags |= REG_ICASE;
				break;
			default:
				exit_sed(1, "sed: bad option in substitution expression\n");
			}
		}
			
		/* compile the regex */
		sed_cmd->sub_match = (regex_t *)xmalloc(sizeof(regex_t));
		if (bb_regcomp(sed_cmd->sub_match, match, cflags) != 0) {
			free(match);
			exit_sed(1, NULL);
		}
		free(match);
	}
}

static void add_cmd_str(const char *cmdstr)
{
	char *my_cmdstr = trim_str(cmdstr);

	/* if this is a comment, don't even bother */
	if (my_cmdstr[0] == '#') {
		free(my_cmdstr);
		return;
	}

	/* grow the array */
	sed_cmds = realloc(sed_cmds, sizeof(struct sed_cmd) * (++ncmds));
	/* zero new element */
	memset(&sed_cmds[ncmds-1], 0, sizeof(struct sed_cmd));
	/* load command string into new array element */
	parse_cmd_str(&sed_cmds[ncmds-1], my_cmdstr);
}


static void load_cmd_file(char *filename)
{
	FILE *cmdfile;
	char *line;

	cmdfile = fopen(filename, "r");
	if (cmdfile == NULL)
		exit_sed(1, strerror(errno));

	while ((line = get_line_from_file(cmdfile)) != NULL) {
		line[strlen(line)-1] = 0; /* eat newline */
		add_cmd_str(line);
		free(line);
	}
}


static int do_sed_command(const struct sed_cmd *sed_cmd, const char *line) 
{
	int altered = 0;

	switch (sed_cmd->cmd) {

		case 'p':
			fputs(line, stdout);
			break;

		case 'd':
			altered++;
			break;

		case 's': /* oo, a fun one :-) */

			/* we only substitute if the substitution 'search' expression matches */
			if (regexec(sed_cmd->sub_match, line, 0, NULL, 0) == 0) {
				regmatch_t regmatch;
				int i;
				char *ptr = (char *)line;

				while (*ptr) {
					/* if we can match the search string... */
					if (regexec(sed_cmd->sub_match, ptr, 1, &regmatch, 0) == 0) {
						/* print everything before the match, */
						for (i = 0; i < regmatch.rm_so; i++)
							fputc(ptr[i], stdout);
						/* then print the substitution in its place */
						fputs(sed_cmd->replace, stdout);
						/* then advance past the match */
						ptr += regmatch.rm_eo;
						/* and let the calling function know that something
						 * has been changed */
						altered++;

						/* if we're not doing this globally... */
						if (!sed_cmd->sub_g)
							break;
					}
					/* if we COULD NOT match the search string (meaning we've gone past
					 * all previous instances), get out */
					else
						break;
				}

				/* is there anything left to print? */
				if (*ptr) 
					fputs(ptr, stdout);
			}

			break;
	}

	return altered;
}

static void process_file(FILE *file)
{
	char *line = NULL;
	static int linenum = 0; /* GNU sed does not restart counting lines at EOF */
	unsigned int still_in_range = 0;
	int line_altered;
	int i;

	/* go through every line in the file */
	while ((line = get_line_from_file(file)) != NULL) {

		linenum++;
		line_altered = 0;

		/* for every line, go through all the commands */
		for (i = 0; i < ncmds; i++) {

			/* are we acting on a range of matched lines? */
			if (sed_cmds[i].beg_match && sed_cmds[i].end_match) {
				if (still_in_range || regexec(sed_cmds[i].beg_match, line, 0, NULL, 0) == 0) {
					line_altered += do_sed_command(&sed_cmds[i], line);
					still_in_range = 1; 
					if (regexec(sed_cmds[i].end_match, line, 0, NULL, 0) == 0)
						still_in_range = 0;
				}
			}

			/* are we trying to match a single line? */
			else if (sed_cmds[i].beg_match) {
				if (regexec(sed_cmds[i].beg_match, line, 0, NULL, 0) == 0)
					line_altered += do_sed_command(&sed_cmds[i], line);
			}

			/* are we acting on a range of line numbers? */
			else if (sed_cmds[i].beg_line > 0 && sed_cmds[i].end_line > 0) {
				if (linenum >= sed_cmds[i].beg_line && linenum <= sed_cmds[i].end_line)
					line_altered += do_sed_command(&sed_cmds[i], line);
			}

			/* are we acting on a specified line number */
			else if (sed_cmds[i].beg_line > 0) {
				if (linenum == sed_cmds[i].beg_line)
					line_altered += do_sed_command(&sed_cmds[i], line);
			}

			/* not acting on matches or line numbers. act on every line */
			else 
				line_altered += do_sed_command(&sed_cmds[i], line);

		}

		/* we will print the line unless we were told to be quiet or if the
		 * line was altered (via a 'd'elete or 's'ubstitution) */
		if (!be_quiet && !line_altered)
			fputs(line, stdout);

		free(line);
	}
}

extern int sed_main(int argc, char **argv)
{
	int opt;

    /* do special-case option parsing */
	if (argv[1] && (strcmp(argv[1], "--help") == 0))
		usage(sed_usage);

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "Vhne:f:")) > 0) {
		switch (opt) {
			case 'V':
				printf("Print Busybox version here\n");
				exit(0);
				break;
			case 'h':
				usage(sed_usage);
				break;
			case 'n':
				be_quiet++;
				break;
			case 'e':
				add_cmd_str(optarg);
				break;
			case 'f': 
				load_cmd_file(optarg);
				break;
		}
	}

	/* if we didn't get a pattern from a -e and no command file was specified,
	 * argv[optind] should be the pattern. no pattern, no worky */
	if (ncmds == 0) {
		if (argv[optind] == NULL)
			usage(sed_usage);
		else {
			add_cmd_str(argv[optind]);
			optind++;
		}
	}


	/* argv[(optind)..(argc-1)] should be names of file to process. If no
	 * files were specified or '-' was specified, take input from stdin.
	 * Otherwise, we process all the files specified. */
	if (argv[optind] == NULL || (strcmp(argv[optind], "-") == 0)) {
		process_file(stdin);
	}
	else {
		int i;
		FILE *file;
		for (i = optind; i < argc; i++) {
			file = fopen(argv[i], "r");
			if (file == NULL) {
				fprintf(stderr, "sed: %s: %s\n", argv[i], strerror(errno));
			} else {
				process_file(file);
				fclose(file);
			}
		}
	}
	
	exit_sed(0, NULL);

	/* not reached */
	return 0;
}
