#include "internal.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* ED: sparc termios is broken: revert back to old termio handling. */
#if #cpu(sparc)
#  define USE_OLD_TERMIO
#endif

#define BB_MORE_TERM

#ifdef BB_MORE_TERM
#  ifdef USE_OLD_TERMIO
#	include <termio.h>
#	define termios termio
#	define stty(fd,argp) ioctl(fd,TCSETAF,argp)
#  else
#	include <termios.h>
#	define stty(fd,argp) tcsetattr(fd,TCSANOW,argp)
#  endif
#	include <signal.h>

	FILE *cin;
	struct termios initial_settings, new_settings;

	void gotsig(int sig) { 
		stty(fileno(cin), &initial_settings);
		exit(0);
	}
#endif

const char	more_usage[] = "more [file]\n"
"\n"
"\tDisplays a file, one page at a time.\n"
"\tIf there are no arguments, the standard input is displayed.\n";

extern int
more_fn(const struct FileInfo * i)
{
	FILE *	f = stdin;
	int	c;
	int	lines = 0, tlines = 0;
	int	next_page = 0;
	
	if ( i ) 
		if (! (f = fopen(i->source, "r"))) {
			name_and_error(i->source);
			return 1;
		}
		
#ifdef BB_MORE_TERM
	cin = fopen("/dev/tty", "r");
	if (!cin)
		cin = fopen("/dev/console", "r");
#ifdef USE_OLD_TERMIO
	ioctl(fileno(cin),TCGETA,&initial_settings);
#else
	tcgetattr(fileno(cin),&initial_settings);
#endif
	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	stty(fileno(cin), &new_settings);
	
	(void) signal(SIGINT, gotsig);
#endif

	while ( (c = getc(f)) != EOF ) {
		if ( next_page ) {
			char	garbage;
			int	len;
			tlines += lines;
			lines = 0;
			next_page = 0;
			if ( i && i->source )
				len = printf("%s - line: %d", i->source, tlines);
			else
				len = printf("line: %d", tlines);
				
			fflush(stdout);
#ifndef BB_MORE_TERM		
			read(2, &garbage, 1);
#else				
			do {
				fread(&garbage, 1, 1, cin);	
			} while ((garbage != ' ') && (garbage != '\n'));
			
			if (garbage == '\n') {
				lines = 22;
				tlines -= 22;
			}					
			garbage = 0;				
			//clear line, since tabs don't overwrite.
			while(len-- > 0)	putchar('\b');
			while(len++ < 79)	putchar(' ');
			while(len-- > 0)	putchar('\b');
			fflush(stdout);
#endif								
		}
		putchar(c);
		if ( c == '\n' && ++lines == 23 )
			next_page = 1;
	}
	if ( f != stdin )
		fclose(f);
#ifdef BB_MORE_TERM
	gotsig(0);
#endif	
	return 0;
}
