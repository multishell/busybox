#include "internal.h"
#include <errno.h>
#include <sys/param.h>

const char mkdir_usage[] = "mkdir [-m mode] directory [directory ...]\n"
"\tCreate directories.\n"
"\n"
"\t-m mode:\tSpecifiy the mode for the new directory\n"
"\t\tunder the argument directory.";

int
mkdir_fn(const struct FileInfo * i)
{
	if ( i->makeParentDirectories ) {

		char	path[PATH_MAX];
		char *	s = path;

		strcpy(path, i->source);

		if ( s[0] == '\0' && s[1] == '\0' ) {
			usage(mkdir_usage);
			return 1;
		}
		s++;
		while ( *s != '\0' ) {
			if ( *s == '/' ) {
				int	status;

				*s = '\0';
				status = mkdir(path, i->orWithMode);
				*s = '/';

				if ( status != 0 ) {
					if ( errno != EEXIST ) {
						name_and_error(i->source);
						return 1;
					}
				}

			}
			s++;
		}
	}

	if ( mkdir(i->source, i->orWithMode) != 0 && errno != EEXIST ) {
		name_and_error(i->source);
		return 1;
	}
	else
		return 0;
}
