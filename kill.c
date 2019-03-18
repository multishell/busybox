/*
 * Mini kill implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
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
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

const char kill_usage[] = "kill [-signal] process-id [process-id ...]\n";

struct signal_name {
    const char *name;
    int number;
};

const struct signal_name signames[] = {
    {"HUP", SIGHUP},
    {"INT", SIGINT},
    {"QUIT", SIGQUIT},
    {"ILL", SIGILL},
    {"TRAP", SIGTRAP},
    {"ABRT", SIGABRT},
#ifndef __alpha__
    {"IOT", SIGIOT},
#endif
#if defined(__sparc__) || defined(__alpha__)
    {"EMT", SIGEMT},
#else
    {"BUS", SIGBUS},
#endif
    {"FPE", SIGFPE},
    {"KILL", SIGKILL},
#if defined(__sparc__) || defined(__alpha__)
    {"BUS", SIGBUS},
#else
    {"USR1", SIGUSR1},
#endif
    {"SEGV", SIGSEGV},
#if defined(__sparc__) || defined(__alpha__)
    {"SYS", SIGSYS},
#else
    {"USR2", SIGUSR2},
#endif
    {"PIPE", SIGPIPE},
    {"ALRM", SIGALRM},
    {"TERM", SIGTERM},
#if defined(__sparc__) || defined(__alpha__)
    {"URG", SIGURG},
    {"STOP", SIGSTOP},
    {"TSTP", SIGTSTP},
    {"CONT", SIGCONT},
    {"CHLD", SIGCHLD},
    {"TTIN", SIGTTIN},
    {"TTOU", SIGTTOU},
    {"IO", SIGIO},
# ifndef __alpha__
    {"POLL", SIGIO},
# endif
    {"XCPU", SIGXCPU},
    {"XFSZ", SIGXFSZ},
    {"VTALRM", SIGVTALRM},
    {"PROF", SIGPROF},
    {"WINCH", SIGWINCH},
# ifdef __alpha__
    {"INFO", SIGINFO},
# else
    {"LOST", SIGLOST},
# endif
    {"USR1", SIGUSR1},
    {"USR2", SIGUSR2},
#else
    {"STKFLT", SIGSTKFLT},
    {"CHLD", SIGCHLD},
    {"CONT", SIGCONT},
    {"STOP", SIGSTOP},
    {"TSTP", SIGTSTP},
    {"TTIN", SIGTTIN},
    {"TTOU", SIGTTOU},
    {"URG", SIGURG},
    {"XCPU", SIGXCPU},
    {"XFSZ", SIGXFSZ},
    {"VTALRM", SIGVTALRM},
    {"PROF", SIGPROF},
    {"WINCH", SIGWINCH},
    {"IO", SIGIO},
    {"POLL", SIGPOLL},
    {"PWR", SIGPWR},
    {"UNUSED", SIGUNUSED},
#endif
    {0, 0}
};

extern int kill_main (int argc, char **argv)
{
    int had_error = 0;
    int sig = SIGTERM;



    if (argv[1][0] == '-') {
	if (argv[1][1] >= '0' && argv[1][1] <= '9') {
	    sig = atoi (&argv[1][1]);
	    if (sig < 0 || sig >= NSIG)
		goto end;
	} else {
	    const struct signal_name *s = signames;
	    for (;;) {
		if (strcmp (s->name, &argv[1][1]) == 0) {
		    sig = s->number;
		    break;
		}
		s++;
		if (s->name == 0)
		    goto end;
	    }
	}
	argv++;
	argc--;

    }
    while (argc > 1) {
	int pid;
	if (argv[1][0] < '0' || argv[1][0] > '9')
	    goto end;
	pid = atoi (argv[1]);
	if (kill (pid, sig) != 0) {
	    had_error = 1;
	    perror (argv[1]);
	}
	argv++;
	argc--;
    }
    if (had_error) {
end:
	usage (kill_usage);
    }
    exit (TRUE);
}
