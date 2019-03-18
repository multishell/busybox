#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern void
usage(const char * message)
{
	if ( message == 0 || *message == 0 )
		return;
	fprintf(stderr, "Usage:\t%s\n", message);
}

extern void
name_and_error(const char * name)
{
	fprintf(stderr, "%s: %s\n", name, strerror(errno));
}


extern int
is_a_directory(const char * name)
{
	struct stat	s;

	return ( stat(name, &s) == 0 && (s.st_mode & S_IFMT) == S_IFDIR );
}

extern char *
join_paths(char * buffer, const char * a, const char * b)
{
	int	length = 0;

	if ( a && *a ) {
		length = strlen(a);
		memcpy(buffer, a, length);
	}
	if ( b && *b ) {
		if ( length > 0 && buffer[length - 1] != '/' )
			buffer[length++] = '/';
		if ( *b == '/' )
			b++;
		strcpy(&buffer[length], b);
	}
	return buffer;
}
