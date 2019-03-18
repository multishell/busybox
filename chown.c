#include "internal.h"
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdio.h>

int my_getid(const char *filename, const char *name, uid_t *id) 
{
	FILE *stream;
	uid_t rid;
	char *rname, *start, *end, buf[128];

	stream=fopen(filename,"r");

	while (fgets (buf, 128, stream) != NULL) {
		if (buf[0] == '#')
			continue;

		start = buf;
		end = strchr (start, ':');
		if (end == NULL)
			continue;
		*end = '\0';
		rname = start;

		start = end + 1;
		end = strchr (start, ':');
		if (end == NULL)
			continue;

		start = end + 1;
		rid = (uid_t) strtol (start, &end, 10);
		if (end == start)
			continue;

		if (name) {
			if (0 == strcmp(rname, name)) {
				*id=rid;
				return 0;
			}
		} else {
			if ( *id == rid )
				return 0;
		}
	}
	fclose(stream);
	return -1;
}

int 
my_getpwuid(uid_t *uid) 
{
	return my_getid("/etc/passwd", NULL, uid);
}

int 
my_getpwnam(char *name, uid_t *uid) 
{
	return my_getid("/etc/passwd", name, uid);
}

int 
my_getgrgid(gid_t *gid) 
{
	return my_getid("/etc/group", NULL, gid);
}

int 
my_getgrnam(char *name, gid_t *gid) 
{
	return my_getid("/etc/group", name, gid);
}

const char	chown_usage[] = "chown [-R] user-name file [file ...]\n"
"\n\tThe group list is kept in the file /etc/groups.\n\n"
"\t-R:\tRecursively change the mode of all files and directories\n"
"\t\tunder the argument directory.";

int
parse_user_name(const char * s, struct FileInfo * i)
{
	char *				dot = strchr(s, '.');
	char * end = NULL;
	uid_t id = 0;

	if (! dot )
		dot = strchr(s, ':');

	if ( dot )
		*dot = '\0';

	if ( my_getpwnam(s,&id) == -1 ) {
		id = strtol(s,&end,10);
		if ((*end != '\0') || ( my_getpwuid(&id) == -1 )) {
			fprintf(stderr, "%s: no such user.\n", s);
			return 1;
		}
	}
	i->userID = id;

	if ( dot ) {
		if ( my_getgrnam(++dot,&id) == -1 ) {
			id = strtol(dot,&end,10);
			if ((*end != '\0') || ( my_getgrgid(&id) == -1 )) {
				fprintf(stderr, "%s: no such group.\n", dot);
				return 1;
			}
		}
		i->groupID = id;
		i->changeGroupID = 1;
	}
	return 0;
}

extern int
chown_main(struct FileInfo * i, int argc, char * * argv)
{
	int					status;

	while ( argc >= 3 && strcmp("-R", argv[1]) == 0 ) {
		i->recursive = 1;
		argc--;
		argv++;
	}

	if ( (status = parse_user_name(argv[1], i)) != 0 )
		return status;

	argv++;
	argc--;

	i->changeUserID = 1;
	i->complainInPostProcess = 1;

	return monadic_main(i, argc, argv);
}
