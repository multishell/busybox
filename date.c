#include "internal.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>

const char			date_usage[] = "date [+%s]";

int
date_main(struct FileInfo * i, int argc, char * * argv)
{
	time_t		t;

	time(&t);
	if (argc == 2)
		printf("%ld\n", t);
	else
		printf("%s\n", ctime(&t));
	return 0;
}
