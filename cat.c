#include "internal.h"
#include <stdio.h>

const char	cat_usage[] = "cat [file ...]";

extern int
cat_fn(const struct FileInfo * i)
{
	FILE *	f = stdin;
	int		c;

	if ( i ) {
		f = fopen(i->source, "r");
		if ( f == NULL ) {
			name_and_error(i->source);
			return 1;
		}
	}
	while ( (c = getc(f)) != EOF )
		putc(c, stdout);
	if ( i )
		fclose(f);
	fflush(stdout);
	return 0;
}

extern int
cat_more_main(struct FileInfo * i, int argc, char * * argv)
{
	if ( argc == 1 )
		return (i->applet->function)(0);
	else
		return monadic_main(i, argc, argv);
}
