/* vi: set sw=4 ts=4: */
/*
 * Mini init implementation for busybox
 *
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Adjusted by so many folks, it's impossible to keep track.
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

/* Turn this on to disable all the dangerous 
   rebooting stuff when debugging.
#define DEBUG_INIT
*/

#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef BB_SYSLOGD
# include <sys/syslog.h>
#endif

/* From <linux/vt.h> */
struct vt_stat {
	unsigned short v_active;        /* active vt */
	unsigned short v_signal;        /* signal to send */
	unsigned short v_state;         /* vt bitmask */
};
#define VT_GETSTATE     0x5603  /* get global vt state info */

/* From <linux/serial.h> */
struct serial_struct {
	int     type;
	int     line;
	int     port;
	int     irq;
	int     flags;
	int     xmit_fifo_size;
	int     custom_divisor;
	int     baud_base;
	unsigned short  close_delay;
	char    reserved_char[2];
	int     hub6;
	unsigned short  closing_wait; /* time to wait before closing */
	unsigned short  closing_wait2; /* no longer used... */
	int     reserved[4];
};



#ifndef RB_HALT_SYSTEM
#define RB_HALT_SYSTEM  0xcdef0123
#define RB_ENABLE_CAD   0x89abcdef
#define RB_DISABLE_CAD  0
#define RB_POWER_OFF    0x4321fedc
#define RB_AUTOBOOT     0x01234567
#if defined(__GLIBC__)
#include <sys/reboot.h>
  #define init_reboot(magic) reboot(magic)
#else
  #define init_reboot(magic) reboot(0xfee1dead, 672274793, magic)
#endif
#endif

#ifndef _PATH_STDPATH
#define _PATH_STDPATH	"/usr/bin:/bin:/usr/sbin:/sbin"
#endif


#if defined BB_FEATURE_INIT_COREDUMPS
/*
 * When a file named CORE_ENABLE_FLAG_FILE exists, setrlimit is called 
 * before processes are spawned to set core file size as unlimited.
 * This is for debugging only.  Don't use this is production, unless
 * you want core dumps lying about....
 */
#define CORE_ENABLE_FLAG_FILE "/.init_enable_core"
#include <sys/resource.h>
#include <sys/time.h>
#endif

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#if defined(__GLIBC__)
//#include <sys/kdaemon.h>
#else
static _syscall2(int, bdflush, int, func, int, data);
#endif							/* __GLIBC__ */


#define VT_PRIMARY   "/dev/tty1"     /* Primary virtual console */
#define VT_SECONDARY "/dev/tty2"     /* Virtual console */
#define VT_LOG       "/dev/tty3"     /* Virtual console */
#define SERIAL_CON0  "/dev/ttyS0"    /* Primary serial console */
#define SERIAL_CON1  "/dev/ttyS1"    /* Serial console */
#define SHELL        "/bin/sh"	     /* Default shell */
#define INITTAB      "/etc/inittab"  /* inittab file location */
#ifndef INIT_SCRIPT
#define INIT_SCRIPT  "/etc/init.d/rcS"   /* Default sysinit script. */
#endif

#define LOG     0x1
#define CONSOLE 0x2

/* Allowed init action types */
typedef enum {
	SYSINIT = 1,
	RESPAWN,
	ASKFIRST,
	WAIT,
	ONCE,
	CTRLALTDEL
} initActionEnum;

/* A mapping between "inittab" action name strings and action type codes. */
typedef struct initActionType {
	const char *name;
	initActionEnum action;
} initActionType;

static const struct initActionType actions[] = {
	{"sysinit", SYSINIT},
	{"respawn", RESPAWN},
	{"askfirst", ASKFIRST},
	{"wait", WAIT},
	{"once", ONCE},
	{"ctrlaltdel", CTRLALTDEL},
	{0}
};

/* Set up a linked list of initActions, to be read from inittab */
typedef struct initActionTag initAction;
struct initActionTag {
	pid_t pid;
	char process[256];
	char console[256];
	initAction *nextPtr;
	initActionEnum action;
};
initAction *initActionList = NULL;


static char *secondConsole = VT_SECONDARY;
static char *log           = VT_LOG;
static int  kernelVersion  = 0;
static char termType[32]   = "TERM=linux";
static char console[32]    = _PATH_CONSOLE;

static void delete_initAction(initAction * action);


/* Print a message to the specified device.
 * Device may be bitwise-or'd from LOG | CONSOLE */
static void message(int device, char *fmt, ...)
		   __attribute__ ((format (printf, 2, 3)));
static void message(int device, char *fmt, ...)
{
	va_list arguments;
	int fd;

#ifdef BB_SYSLOGD

	/* Log the message to syslogd */
	if (device & LOG) {
		char msg[1024];

		va_start(arguments, fmt);
		vsnprintf(msg, sizeof(msg), fmt, arguments);
		va_end(arguments);
		openlog("init", 0, LOG_USER);
		syslog(LOG_USER|LOG_INFO, msg);
		closelog();
	}
#else
	static int log_fd = -1;

	/* Take full control of the log tty, and never close it.
	 * It's mine, all mine!  Muhahahaha! */
	if (log_fd < 0) {
		if (log == NULL) {
			/* don't even try to log, because there is no such console */
			log_fd = -2;
			/* log to main console instead */
			device = CONSOLE;
		} else if ((log_fd = device_open(log, O_RDWR|O_NDELAY)) < 0) {
			log_fd = -2;
			fprintf(stderr, "Bummer, can't write to log on %s!\r\n", log);
			fflush(stderr);
			log = NULL;
			device = CONSOLE;
		}
	}
	if ((device & LOG) && (log_fd >= 0)) {
		va_start(arguments, fmt);
		vdprintf(log_fd, fmt, arguments);
		va_end(arguments);
	}
#endif

	if (device & CONSOLE) {
		/* Always send console messages to /dev/console so people will see them. */
		if (
			(fd =
			 device_open(_PATH_CONSOLE,
						 O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0) {
			va_start(arguments, fmt);
			vdprintf(fd, fmt, arguments);
			va_end(arguments);
			close(fd);
		} else {
			fprintf(stderr, "Bummer, can't print: ");
			va_start(arguments, fmt);
			vfprintf(stderr, fmt, arguments);
			fflush(stderr);
			va_end(arguments);
		}
	}
}

/* Set terminal settings to reasonable defaults */
void set_term(int fd)
{
	struct termios tty;

	tcgetattr(fd, &tty);

	/* set control chars */
	tty.c_cc[VINTR]  = 3;	/* C-c */
	tty.c_cc[VQUIT]  = 28;	/* C-\ */
	tty.c_cc[VERASE] = 127; /* C-? */
	tty.c_cc[VKILL]  = 21;	/* C-u */
	tty.c_cc[VEOF]   = 4;	/* C-d */
	tty.c_cc[VSTART] = 17;	/* C-q */
	tty.c_cc[VSTOP]  = 19;	/* C-s */
	tty.c_cc[VSUSP]  = 26;	/* C-z */

	/* use line dicipline 0 */
	tty.c_line = 0;

	/* Make it be sane */
	tty.c_cflag &= CBAUD|CBAUDEX|CSIZE|CSTOPB|PARENB|PARODD;
	tty.c_cflag |= HUPCL|CLOCAL;

	/* input modes */
	tty.c_iflag = ICRNL | IXON | IXOFF;

	/* output modes */
	tty.c_oflag = OPOST | ONLCR;

	/* local modes */
	tty.c_lflag =
		ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;

	tcsetattr(fd, TCSANOW, &tty);
}

/* How much memory does this machine have? */
static int check_free_memory()
{
	struct sysinfo info;

	/* Pre initialize mem_unit in case this kernel is something prior to
	 * the linux 2.4 kernel (which will actually fill in mem_unit... */
	sysinfo(&info);
	if (sysinfo(&info) != 0) {
		printf("Error checking free memory: %s\n", strerror(errno));
		return -1;
	}
	if (info.mem_unit==0) {
		/* Looks like we have a kernel prior to Linux 2.4.x */
		info.mem_unit=1024;
		info.totalram/=info.mem_unit;
		info.totalswap/=info.mem_unit;
	} else {
		/* Bah. Linux 2.4.x completely changed sysinfo. This can in theory
		overflow a 32 bit unsigned long, but who puts more then 4GiB ram+swap
		on an embedded system? */
		info.mem_unit/=1024;
		info.totalram*=info.mem_unit;
		info.totalswap*=info.mem_unit;
	}

	return(info.totalram+info.totalswap);
}

static void console_init()
{
	int fd;
	int tried_devcons = 0;
	int tried_vtprimary = 0;
	struct vt_stat vt;
	struct serial_struct sr;
	char *s;

	if ((s = getenv("TERM")) != NULL) {
		snprintf(termType, sizeof(termType) - 1, "TERM=%s", s);
	}

	if ((s = getenv("CONSOLE")) != NULL) {
		snprintf(console, sizeof(console) - 1, "%s", s);
	}
#if #cpu(sparc)
	/* sparc kernel supports console=tty[ab] parameter which is also 
	 * passed to init, so catch it here */
	else if ((s = getenv("console")) != NULL) {
		/* remap tty[ab] to /dev/ttyS[01] */
		if (strcmp(s, "ttya") == 0)
			snprintf(console, sizeof(console) - 1, "%s", SERIAL_CON0);
		else if (strcmp(s, "ttyb") == 0)
			snprintf(console, sizeof(console) - 1, "%s", SERIAL_CON1);
	}
#endif
	else {
		/* 2.2 kernels: identify the real console backend and try to use it */
		if (ioctl(0, TIOCGSERIAL, &sr) == 0) {
			/* this is a serial console */
			snprintf(console, sizeof(console) - 1, "/dev/ttyS%d", sr.line);
		} else if (ioctl(0, VT_GETSTATE, &vt) == 0) {
			/* this is linux virtual tty */
			snprintf(console, sizeof(console) - 1, "/dev/tty%d",
					 vt.v_active);
		} else {
			snprintf(console, sizeof(console) - 1, "%s", _PATH_CONSOLE);
			tried_devcons++;
		}
	}

	while ((fd = open(console, O_RDONLY | O_NONBLOCK)) < 0) {
		/* Can't open selected console -- try /dev/console */
		if (!tried_devcons) {
			tried_devcons++;
			snprintf(console, sizeof(console) - 1, "%s", _PATH_CONSOLE);
			continue;
		}
		/* Can't open selected console -- try vt1 */
		if (!tried_vtprimary) {
			tried_vtprimary++;
			snprintf(console, sizeof(console) - 1, "%s", VT_PRIMARY);
			continue;
		}
		break;
	}
	if (fd < 0) {
		/* Perhaps we should panic here? */
		snprintf(console, sizeof(console) - 1, "/dev/null");
	} else {
		/* check for serial console and disable logging to tty3 & running a
		   * shell to tty2 */
		if (ioctl(0, TIOCGSERIAL, &sr) == 0) {
			log = NULL;
			secondConsole = NULL;
			/* Force the TERM setting to vt102 for serial console --
			 * iff TERM is set to linux (the default) */
			if (strcmp( termType, "TERM=linux" ) == 0)
				snprintf(termType, sizeof(termType) - 1, "TERM=vt102");
			message(LOG | CONSOLE,
					"serial console detected.  Disabling virtual terminals.\r\n");
		}
		close(fd);
	}
	message(LOG, "console=%s\n", console);
}

static pid_t run(char *command, char *terminal, int get_enter)
{
	int i, fd;
	pid_t pid;
	char *tmpCmd;
	char *cmd[255];
	char buf[255];
	static const char press_enter[] =

		"\nPlease press Enter to activate this console. ";
	char *environment[] = {
		"HOME=/",
		"PATH=/usr/bin:/bin:/usr/sbin:/sbin",
		"SHELL=/bin/sh",
		termType,
		"USER=root",
		0
	};


	if ((pid = fork()) == 0) {
		/* Clean up */
		close(0);
		close(1);
		close(2);
		setsid();

		/* Reset signal handlers set for parent process */
		signal(SIGUSR1, SIG_DFL);
		signal(SIGUSR2, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		signal(SIGHUP, SIG_DFL);

		if ((fd = device_open(terminal, O_RDWR)) < 0) {
			message(LOG | CONSOLE, "Bummer, can't open %s\r\n", terminal);
			exit(1);
		}
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		ioctl(0, TIOCSCTTY, 0);
		tcsetpgrp(0, getpgrp());
		set_term(0);

		if (get_enter == TRUE) {
			/*
			 * Save memory by not exec-ing anything large (like a shell)
			 * before the user wants it. This is critical if swap is not
			 * enabled and the system has low memory. Generally this will
			 * be run on the second virtual console, and the first will
			 * be allowed to start a shell or whatever an init script 
			 * specifies.
			 */
			char c;
#ifdef DEBUG_INIT
			pid_t shell_pgid = getpid();
			message(LOG, "Waiting for enter to start '%s' (pid %d, console %s)\r\n",
					command, shell_pgid, terminal);
#endif
			write(fileno(stdout), press_enter, sizeof(press_enter) - 1);
			read(fileno(stdin), &c, 1);
		}

#ifdef DEBUG_INIT
		/* Log the process name and args */
		message(LOG, "Starting pid %d, console %s: '%s'\r\n",
				shell_pgid, terminal, command);
#endif

		/* See if any special /bin/sh requiring characters are present */
		if (strpbrk(command, "~`!$^&*()=|\\{}[];\"'<>?") != NULL) {
			cmd[0] = SHELL;
			cmd[1] = "-c";
			strcpy(buf, "exec ");
			strncat(buf, command, sizeof(buf) - strlen(buf) - 1);
			cmd[2] = buf;
			cmd[3] = NULL;
		} else {
			/* Convert command (char*) into cmd (char**, one word per string) */
			for (tmpCmd = command, i = 0;
				 (tmpCmd = strsep(&command, " \t")) != NULL;) {
				if (*tmpCmd != '\0') {
					cmd[i] = tmpCmd;
					tmpCmd++;
					i++;
				}
			}
			cmd[i] = NULL;
		}

#if defined BB_FEATURE_INIT_COREDUMPS
		{
			struct stat sb;
			if (stat (CORE_ENABLE_FLAG_FILE, &sb) == 0) {
				struct rlimit limit;
				limit.rlim_cur = RLIM_INFINITY;
				limit.rlim_max = RLIM_INFINITY;
				setrlimit(RLIMIT_CORE, &limit);
			}
		}
#endif

		/* Now run it.  The new program will take over this PID, 
		 * so nothing further in init.c should be run. */
		execve(cmd[0], cmd, environment);

		/* We're still here?  Some error happened. */
		message(LOG | CONSOLE, "Bummer, could not run '%s': %s\n", cmd[0],
				strerror(errno));
		exit(-1);
	}
	return pid;
}

static int waitfor(char *command, char *terminal, int get_enter)
{
	int status, wpid;
	int pid = run(command, terminal, get_enter);

	while (1) {
		wpid = wait(&status);
		if (wpid > 0 && wpid != pid) {
			continue;
		}
		if (wpid == pid)
			break;
	}
	return wpid;
}

/* Make sure there is enough memory to do something useful. *
 * Calls "swapon -a" if needed so be sure /etc/fstab is present... */
static void check_memory()
{
	struct stat statBuf;

	if (check_free_memory() > 1000)
		return;

	if (stat("/etc/fstab", &statBuf) == 0) {
		/* swapon -a requires /proc typically */
		waitfor("mount proc /proc -t proc", console, FALSE);
		/* Try to turn on swap */
		waitfor("swapon -a", console, FALSE);
		if (check_free_memory() < 1000)
			goto goodnight;
	} else
		goto goodnight;
	return;

  goodnight:
	message(CONSOLE,
			"Sorry, your computer does not have enough memory.\r\n");
	while (1)
		sleep(1);
}

/* Run all commands to be run right before halt/reboot */
static void run_lastAction(void)
{
	initAction *a;
	for (a = initActionList; a; a = a->nextPtr) {
		if (a->action == CTRLALTDEL) {
			waitfor(a->process, a->console, FALSE);
			delete_initAction(a);
		}
	}
}


#ifndef DEBUG_INIT
static void shutdown_system(void)
{

	/* first disable our SIGHUP signal */
	signal(SIGHUP, SIG_DFL);

	/* Allow Ctrl-Alt-Del to reboot system. */
	init_reboot(RB_ENABLE_CAD);

	message(CONSOLE|LOG, "\r\nThe system is going down NOW !!\r\n");
	sync();

	/* Send signals to every process _except_ pid 1 */
	message(CONSOLE|LOG, "Sending SIGTERM to all processes.\r\n");
	kill(-1, SIGTERM);
	sleep(1);
	sync();

	message(CONSOLE|LOG, "Sending SIGKILL to all processes.\r\n");
	kill(-1, SIGKILL);
	sleep(1);

	/* run everything to be run at "ctrlaltdel" */
	run_lastAction();

	sync();
	if (kernelVersion > 0 && kernelVersion <= KERNEL_VERSION(2,2,11)) {
		/* bdflush, kupdate not needed for kernels >2.2.11 */
		//bdflush(1, 0);
		sync();
	}
}

static void halt_signal(int sig)
{
	shutdown_system();
	message(CONSOLE|LOG,
			"The system is halted. Press %s or turn off power\r\n",
			(secondConsole == NULL)	/* serial console */
			? "Reset" : "CTRL-ALT-DEL");
	sync();

	/* allow time for last message to reach serial console */
	sleep(2);

	if (sig == SIGUSR2 && kernelVersion >= KERNEL_VERSION(2,2,0))
		init_reboot(RB_POWER_OFF);
	else
		init_reboot(RB_HALT_SYSTEM);
	exit(0);
}

static void reboot_signal(int sig)
{
	shutdown_system();
	message(CONSOLE|LOG, "Please stand by while rebooting the system.\r\n");
	sync();

	/* allow time for last message to reach serial console */
	sleep(2);

	init_reboot(RB_AUTOBOOT);
	exit(0);
}

#if defined BB_FEATURE_INIT_CHROOT

#if ! defined BB_FEATURE_USE_PROCFS
#error Sorry, I depend on the /proc filesystem right now.
#endif

static void check_chroot(int sig)
{
	char *argv_init[2] = { "init", NULL, };
	char *envp_init[3] = { "HOME=/", "TERM=linux", NULL, };
	char rootpath[256], *tc;
	int fd;

	if ((fd = open("/proc/sys/kernel/init-chroot", O_RDONLY)) == -1) {
		message(CONSOLE,
				"SIGHUP recived, but could not open proc file\r\n");
		sleep(2);
		return;
	}
	if (read(fd, rootpath, sizeof(rootpath)) == -1) {
		message(CONSOLE,
				"SIGHUP recived, but could not read proc file\r\n");
		sleep(2);
		return;
	}
	close(fd);

	if (rootpath[0] == '\0') {
		message(CONSOLE,
				"SIGHUP recived, but new root is not valid: %s\r\n",
				rootpath);
		sleep(2);
		return;
	}

	tc = strrchr(rootpath, '\n');
	*tc = '\0';

	/* Ok, making it this far means we commit */
	message(CONSOLE, "Please stand by, changing root to `%s'.\r\n",
			rootpath);

	/* kill all other programs first */
	message(CONSOLE, "Sending SIGTERM to all processes.\r\n");
	kill(-1, SIGTERM);
	sleep(2);
	sync();

	message(CONSOLE, "Sending SIGKILL to all processes.\r\n");
	kill(-1, SIGKILL);
	sleep(2);
	sync();

	/* ok, we don't need /proc anymore. we also assume that the signaling
	 * process left the rest of the filesystems alone for us */
	umount("/proc");

	/* Ok, now we chroot. Hopefully we only have two things mounted, the
	 * new chroot'd mount point, and the old "/" mount. s,
	 * we go ahead and unmount the old "/". This should trigger the kernel
	 * to set things up the Right Way(tm). */

	if (!chroot(rootpath))
		umount("/dev/root");

	/* If the chroot fails, we are already too far to turn back, so we
	 * continue and hope that executing init below will revive the system */

	/* close all of our descriptors and open new ones */
	close(0);
	close(1);
	close(2);
	open("/dev/console", O_RDWR, 0);
	dup(0);
	dup(0);

	message(CONSOLE, "Executing real init...\r\n");
	/* execute init in the (hopefully) new root */
	execve("/sbin/init", argv_init, envp_init);

	message(CONSOLE,
			"ERROR: Could not exec new init. Press %s to reboot.\r\n",
			(secondConsole == NULL)	/* serial console */
			? "Reset" : "CTRL-ALT-DEL");
	return;
}
#endif							/* BB_FEATURE_INIT_CHROOT */

#endif							/* ! DEBUG_INIT */

void new_initAction(initActionEnum action, char *process, char *cons)
{
	initAction *newAction;

	if (*cons == '\0')
		cons = console;

	/* If BusyBox detects that a serial console is in use, 
	 * then entries not refering to the console or null devices will _not_ be run.
	 * The exception to this rule is the null device.
	 */
	if (secondConsole == NULL && strcmp(cons, console)
		&& strcmp(cons, "/dev/null"))
		return;

	newAction = calloc((size_t) (1), sizeof(initAction));
	if (!newAction) {
		message(LOG | CONSOLE, "Memory allocation failure\n");
		while (1)
			sleep(1);
	}
	newAction->nextPtr = initActionList;
	initActionList = newAction;
	strncpy(newAction->process, process, 255);
	newAction->action = action;
	strncpy(newAction->console, cons, 255);
	newAction->pid = 0;
//    message(LOG|CONSOLE, "process='%s' action='%d' console='%s'\n",
//      newAction->process, newAction->action, newAction->console);
}

static void delete_initAction(initAction * action)
{
	initAction *a, *b = NULL;

	for (a = initActionList; a; b = a, a = a->nextPtr) {
		if (a == action) {
			if (b == NULL) {
				initActionList = a->nextPtr;
			} else {
				b->nextPtr = a->nextPtr;
			}
			free(a);
			break;
		}
	}
}

/* NOTE that if BB_FEATURE_USE_INITTAB is NOT defined,
 * then parse_inittab() simply adds in some default
 * actions(i.e runs INIT_SCRIPT and then starts a pair 
 * of "askfirst" shells).  If BB_FEATURE_USE_INITTAB 
 * _is_ defined, but /etc/inittab is missing, this 
 * results in the same set of default behaviors.
 * */
void parse_inittab(void)
{
#ifdef BB_FEATURE_USE_INITTAB
	FILE *file;
	char buf[256], lineAsRead[256], tmpConsole[256];
	char *id, *runlev, *action, *process, *eol;
	const struct initActionType *a = actions;
	int foundIt;


	file = fopen(INITTAB, "r");
	if (file == NULL) {
		/* No inittab file -- set up some default behavior */
#endif
		/* Swapoff on halt/reboot */
		new_initAction(CTRLALTDEL, "/sbin/swapoff -a > /dev/null 2>&1", console);
		/* Umount all filesystems on halt/reboot */
		new_initAction(CTRLALTDEL, "/bin/umount -a -r > /dev/null 2>&1", console);
		/* Askfirst shell on tty1 */
		new_initAction(ASKFIRST, SHELL, console);
		/* Askfirst shell on tty2 */
		if (secondConsole != NULL)
			new_initAction(ASKFIRST, SHELL, secondConsole);
		/* sysinit */
		new_initAction(SYSINIT, INIT_SCRIPT, console);

		return;
#ifdef BB_FEATURE_USE_INITTAB
	}

	while (fgets(buf, 255, file) != NULL) {
		foundIt = FALSE;
		/* Skip leading spaces */
		for (id = buf; *id == ' ' || *id == '\t'; id++);

		/* Skip the line if it's a comment */
		if (*id == '#' || *id == '\n')
			continue;

		/* Trim the trailing \n */
		eol = strrchr(id, '\n');
		if (eol != NULL)
			*eol = '\0';

		/* Keep a copy around for posterity's sake (and error msgs) */
		strcpy(lineAsRead, buf);

		/* Separate the ID field from the runlevels */
		runlev = strchr(id, ':');
		if (runlev == NULL || *(runlev + 1) == '\0') {
			message(LOG | CONSOLE, "Bad inittab entry: %s\n", lineAsRead);
			continue;
		} else {
			*runlev = '\0';
			++runlev;
		}

		/* Separate the runlevels from the action */
		action = strchr(runlev, ':');
		if (action == NULL || *(action + 1) == '\0') {
			message(LOG | CONSOLE, "Bad inittab entry: %s\n", lineAsRead);
			continue;
		} else {
			*action = '\0';
			++action;
		}

		/* Separate the action from the process */
		process = strchr(action, ':');
		if (process == NULL || *(process + 1) == '\0') {
			message(LOG | CONSOLE, "Bad inittab entry: %s\n", lineAsRead);
			continue;
		} else {
			*process = '\0';
			++process;
		}

		/* Ok, now process it */
		a = actions;
		while (a->name != 0) {
			if (strcmp(a->name, action) == 0) {
				if (*id != '\0') {
					struct stat statBuf;

					strcpy(tmpConsole, "/dev/");
					strncat(tmpConsole, id, 200);
					if (stat(tmpConsole, &statBuf) != 0) {
						message(LOG | CONSOLE,
								"device '%s' does not exist.  Did you read the directions?\n",
								tmpConsole);
						break;
					}
					id = tmpConsole;
				}
				new_initAction(a->action, process, id);
				foundIt = TRUE;
			}
			a++;
		}
		if (foundIt == TRUE)
			continue;
		else {
			/* Choke on an unknown action */
			message(LOG | CONSOLE, "Bad inittab entry: %s\n", lineAsRead);
		}
	}
	return;
#endif /* BB_FEATURE_USE_INITTAB */
}



extern int init_main(int argc, char **argv)
{
	initAction *a;
	pid_t wpid;
	int status;

#ifndef DEBUG_INIT
	/* Expect to be invoked as init with PID=1 or be invoked as linuxrc */
	if (getpid() != 1
#ifdef BB_FEATURE_LINUXRC
			&& strstr(argv[0], "linuxrc") == NULL
#endif
	                  )
	{
			usage("init\n\nInit is the parent of all processes.\n\n"
				  "This version of init is designed to be run only "
				  "by the kernel.\n");
	}
	/* Set up sig handlers  -- be sure to
	 * clear all of these in run() */
	signal(SIGUSR1, halt_signal);
	signal(SIGUSR2, halt_signal);
	signal(SIGINT, reboot_signal);
	signal(SIGTERM, reboot_signal);
#if defined BB_FEATURE_INIT_CHROOT
	signal(SIGHUP, check_chroot);
#endif

	/* Turn off rebooting via CTL-ALT-DEL -- we get a 
	 * SIGINT on CAD so we can shut things down gracefully... */
	init_reboot(RB_DISABLE_CAD);
#endif

	/* Figure out what kernel this is running */
	kernelVersion = get_kernel_revision();

	/* Figure out where the default console should be */
	console_init();

	/* Close whatever files are open, and reset the console. */
	close(0);
	close(1);
	close(2);
	set_term(0);
	chdir("/");
	setsid();

	/* Make sure PATH is set to something sane */
	putenv(_PATH_STDPATH);

	/* Hello world */
#ifndef DEBUG_INIT
	message(
#if ! defined BB_FEATURE_EXTRA_QUIET
			CONSOLE|
#endif
			LOG,
			"init started:  BusyBox v%s (%s) multi-call binary\r\n",
			BB_VER, BB_BT);
#else
	message(
#if ! defined BB_FEATURE_EXTRA_QUIET
			CONSOLE|
#endif
			LOG,
			"init(%d) started:  BusyBox v%s (%s) multi-call binary\r\n",
			getpid(), BB_VER, BB_BT);
#endif


	/* Make sure there is enough memory to do something useful. */
	check_memory();

	/* Check if we are supposed to be in single user mode */
	if (argc > 1 && (!strcmp(argv[1], "single") ||
					 !strcmp(argv[1], "-s") || !strcmp(argv[1], "1"))) {
		/* Ask first then start a shell on tty2 */
		if (secondConsole != NULL)
			new_initAction(ASKFIRST, SHELL, secondConsole);
		/* Start a shell on tty1 */
		new_initAction(RESPAWN, SHELL, console);
	} else {
		/* Not in single user mode -- see what inittab says */

		/* NOTE that if BB_FEATURE_USE_INITTAB is NOT defined,
		 * then parse_inittab() simply adds in some default
		 * actions(i.e runs INIT_SCRIPT and then starts a pair 
		 * of "askfirst" shells */
		parse_inittab();
	}

	/* Fix up argv[0] to be certain we claim to be init */
	strncpy(argv[0], "init", strlen(argv[0])+1);
	if (argc > 1)
		strncpy(argv[1], "\0", strlen(argv[1])+1);

	/* Now run everything that needs to be run */

	/* First run the sysinit command */
	for (a = initActionList; a; a = a->nextPtr) {
		if (a->action == SYSINIT) {
			waitfor(a->process, a->console, FALSE);
			/* Now remove the "sysinit" entry from the list */
			delete_initAction(a);
		}
	}
	/* Next run anything that wants to block */
	for (a = initActionList; a; a = a->nextPtr) {
		if (a->action == WAIT) {
			waitfor(a->process, a->console, FALSE);
			/* Now remove the "wait" entry from the list */
			delete_initAction(a);
		}
	}
	/* Next run anything to be run only once */
	for (a = initActionList; a; a = a->nextPtr) {
		if (a->action == ONCE) {
			run(a->process, a->console, FALSE);
			/* Now remove the "once" entry from the list */
			delete_initAction(a);
		}
	}
	/* If there is nothing else to do, stop */
	if (initActionList == NULL) {
		message(LOG | CONSOLE,
				"No more tasks for init -- sleeping forever.\n");
		while (1)
			sleep(1);
	}

	/* Now run the looping stuff for the rest of forever */
	while (1) {
		for (a = initActionList; a; a = a->nextPtr) {
			/* Only run stuff with pid==0.  If they have
			 * a pid, that means they are still running */
			if (a->pid == 0) {
				switch (a->action) {
				case RESPAWN:
					/* run the respawn stuff */
					a->pid = run(a->process, a->console, FALSE);
					break;
				case ASKFIRST:
					/* run the askfirst stuff */
					a->pid = run(a->process, a->console, TRUE);
					break;
					/* silence the compiler's incessant whining */
				default:
					break;
				}
			}
		}
		/* Wait for a child process to exit */
		wpid = wait(&status);
		if (wpid > 0) {
			/* Find out who died and clean up their corpse */
			for (a = initActionList; a; a = a->nextPtr) {
				if (a->pid == wpid) {
					a->pid = 0;
					message(LOG,
							"Process '%s' (pid %d) exited.  Scheduling it for restart.\n",
							a->process, wpid);
				}
			}
		}
		sleep(1);
	}
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
