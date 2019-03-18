#include "internal.h"
#include <sys/types.h>
#include <utime.h>

const char	touch_usage[] = "touch file [file ...]\n"
"\n"
"\tUpdate the last-modified date on the given file[s].\n";

extern int
touch_fn(const struct FileInfo * i)
{
	if ( utime(i->source, 0) ) {
		name_and_error(i->source);
		return 1;
	}
	return 0;
}
