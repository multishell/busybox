#include <stdlib.h>
#include <string.h>
#include "libbb.h"

/*
 * Returns a [multi-line] package field
 */
extern char *read_package_field(const char *package_buffer)
{
	char *field = NULL;
	int field_length = 0;	
	int buffer_length = 0;

	buffer_length = strlen(package_buffer);

	while ((field = strchr(&package_buffer[field_length], '\n')) != NULL) {
		field_length = buffer_length - strlen(field);
		if (package_buffer[field_length + 1] != ' ') {
			break;
		} else {
			field_length++;
		}		
	}
	if (field_length == 0) {
		return(NULL);
	} else {
		return(xstrndup(package_buffer, field_length));
	}
}

