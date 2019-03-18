#include "internal.h"
#include <grp.h>
#include <stdio.h>

const char	chgrp_usage[] = "chgrp [-R] group-name file [file ...]\n"
"\n\tThe group list is kept in the file /etc/groups.\n\n"
"\t-R:\tRecursively change the group of all files and directories\n"
"\t\tunder the argument directory.";

extern int
chgrp_main(struct FileInfo * i, int argc, char * * argv)
{
	struct group *	g;

	while ( argc >= 3 && strcmp("-R", argv[1]) == 0 ) {
		i->recursive = 1;
		argc--;
		argv++;
	}

	if ( (g = getgrnam(argv[1])) == 0 ) {
		fprintf(stderr, "%s: no such group.\n", argv[1]);
		return 1;
	}

	argv++;
	argc--;

	i->groupID = g->gr_gid;
	i->changeGroupID = 1;
	i->complainInPostProcess = 1;

	return monadic_main(i, argc, argv);
}
