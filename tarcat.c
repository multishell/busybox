#include "internal.h"
#include "tarfn.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

static int
IOError(TarInfo * i)
{
    int error = errno;  /* fflush() could cause errno to change */
    fflush(stdout);
    fprintf(stderr, "%s: %s\n", i->Name, strerror(error));

    /*
     * The status returned by a coroutine of TarExtractor(), if it
     * is non-zero, will be returned to the caller of TarExtractor().
     */
    return -2;
}

static int
catFile(TarInfo * i, int do_write)
{
#define BUFF 1024 * 128
    size_t  size = i->Size;
        
    while ( (int)size > 0 ) {
        char    buffer[BUFF];
        size_t  writeSize;
        size_t  readSize;

        if ( size > BUFF )
            writeSize = readSize = BUFF;
        else {
            int mod = size % 512;
            if ( mod != 0 )
                readSize = size + (512 - mod);
            else
	        readSize = size;
            writeSize = size;
        }

        if ( (readSize = read(0, buffer, readSize)) < 0) {
	    fprintf(stderr,"Error reading data: %d\n",readSize);
            return -1;  /* Something wrong with archive */
	}

/*        if ( readSize < writeSize ) */
            writeSize = readSize;

        if (do_write) {
	    if ( write(1, buffer, writeSize) != writeSize ) {
		fprintf(stderr,"Error writing data\n");
                return IOError(i);  /* Write failure. */
	    }
        }

        size -= writeSize;
    }
    return 0;
}

const char  tarcat_usage[] = "tarcat filename\n"
"\n"
"\tExtracts a file to stdout from a tar archive on the standard input.\n"
"\n";

int
tarcat_main(struct FileInfo * i, int argc, char * * argv)
{
    int     status;
    char    buffer[512];
    TarInfo h;
    char *filename = NULL;

    filename = argv[1];

    while ( (status = read(0, buffer, 512)) == 512) {
        int     nameLength;
        if ( !DecodeTarHeader(buffer, &h) ) {
            if ( h.Name[0] == '\0' ) {
                return 0;       /* End of tape */
            } else {
                errno = 0;      /* Indicates broken tarfile */
                fprintf(stderr, "Error in archive format.\n");
                return 1;      /* Header checksum error */
            }
        }
        if ( h.Name[0] == '\0' ) {
            errno = 0;      /* Indicates broken tarfile */
            fprintf(stderr, "Error in archive format.\n");
            return 1;      /* Bad header data */
        }
        nameLength = strlen(h.Name);
        switch ( h.Type ) {
	    case NormalFile0:
	    case NormalFile1:
        	if ( h.Name[nameLength - 1] != '/' ) {
		    if ( 0 == strcmp(h.Name,filename) ) {
                        status = catFile(&h,1);
                    } else {
                        status = catFile(&h,0);
                    }
                }
            case Directory:
            case HardLink:
            case SymbolicLink:
            case CharacterDevice:
            case BlockDevice:
            case FIFO:
                break;
            default:
                errno = 0;      /* Indicates broken tarfile */
                fprintf(stderr, "Error in archive format.\n");
                return 1;      /* Bad header field */
        }
    }
    if ( status != 0 ) {     /* Read partial header record */
        errno = 0;      /* Indicates broken tarfile */
        fprintf(stderr, "Error in archive format.\n");
        return 1;
    }
    return 0;
}
