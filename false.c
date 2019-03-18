#include "internal.h"

const char	false_usage[] = "false\n"
"\n\t"
"\treturn \"false\" status for use by shell programs.\n";

extern int
false_main(struct FileInfo * i, int argc, char * * argv)
{
	return 1;
}
