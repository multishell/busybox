/* vi: set sw=4 ts=4: */
/*
 * public domain -- Dave 'Kill a Cop' Cinege <dcinege@psychosis.com>
 * 
 * dutmp
 * Takes utmp formated file on stdin and dumps it's contents 
 * out in colon delimited fields. Easy to 'cut' for shell based 
 * versions of 'who', 'last', etc. IP Addr is output in hex, 
 * little endian on x86.
 * 
 * Modified to support all sort of libcs by 
 * Erik Andersen <andersen@lineo.com>
 */

#include "internal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#define BB_DECLARE_EXTERN
#define bb_need_io_error
#include "messages.c"
#include <utmp.h>


static const char dutmp_usage[] = "dutmp [FILE]\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nDump utmp file format (pipe delimited) from FILE\n"
	"or stdin to stdout.  (i.e. 'dutmp /var/run/utmp')\n"
#endif
	;

extern int dutmp_main(int argc, char **argv)
{

	int file;
	struct utmp ut;

	if (argc<2) {
		file = fileno(stdin);
	} else if (*argv[1] == '-' ) {
		usage(dutmp_usage);
	} else  {
		file = open(argv[1], O_RDONLY);
		if (file < 0) {
			fatalError(io_error, argv[1], strerror(errno));
		}
	}

	while (read(file, (void*)&ut, sizeof(struct utmp))) {
		printf("%d|%d|%s|%s|%s|%s|%s|%lx\n",
				ut.ut_type, ut.ut_pid, ut.ut_line,
				ut.ut_id, ut.ut_user, ut.ut_host,
				ctime(&(ut.ut_time)), 
				(long)ut.ut_addr);
	}

	return(TRUE);
}
