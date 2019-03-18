#include "internal.h"
#include "tarfn.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>
#include <string.h>
#include <time.h>

/*
 * Define this in terms of vfprintf() when I have time.
 */
static void
verbose(const char * pattern, ...)
{
}

static int
Read(void * userData, char * buffer, int length)
{
    /*
     * If the status of the read function is < 0, it will be returned to
     * the caller of TarExtractor().
     */
    return read((int)userData, buffer, length);
}

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
ExtractFile(TarInfo * i)
{
    /*
     * If you don't want to extract the file, you must advance the tape
     * by the file size rounded up to the next 512-byte boundary and
     * return 0.
     */

    int fd = open(i->Name, O_CREAT|O_TRUNC|O_WRONLY, i->Mode & ~S_IFMT);
    size_t  size = i->Size;
    struct utimbuf t;
        
    if ( fd < 0 ) {
	if (errno == EEXIST)
            unlink(i->Name);
	else if (errno == ENOENT) {
            char path[1024];
            char *s = path;
            strcpy(path, i->Name);
            s++;
            while ( *s != '\0' ) {
                if ( *s == '/' ) {
                    *s = '\0';
                    if ( mkdir(path, 0777) != 0 && !is_a_directory(path) ) {
                        name_and_error(path);
                        return 1;
                    }
                    *s = '/';
                }
                s++;
            }
        }
        fd = open(i->Name, O_CREAT|O_TRUNC|O_WRONLY, i->Mode & ~S_IFMT);
        if ( fd < 0 )
            return IOError(i);
    }

    verbose("File: %s\n", i->Name);

    while ( size > 0 ) {
        char    buffer[1024 * 128];
        size_t  writeSize;
        size_t  readSize;

        if ( size > sizeof(buffer) )
            writeSize = readSize = sizeof(buffer);
        else {
            int mod = size % 512;
            if ( mod != 0 )
                readSize = size + (512 - mod);
            else
                readSize = size;
            writeSize = size;
        }

        if ( (readSize = Read(i->UserData, buffer, readSize)) <= 0 )
            return -1;  /* Something wrong with archive */

        if ( readSize < writeSize )
            writeSize = readSize;

        if ( write(fd, buffer, writeSize) != writeSize )
            return IOError(i);  /* Write failure. */

        size -= writeSize;
    }
    /* fchown() and fchmod() are cheaper than chown() and chmod(). */
    fchown(fd, i->UserID, i->GroupID);
    fchmod(fd, i->Mode & ~S_IFMT);
    close(fd);
    t.actime = time(0);
    t.modtime = i->ModTime;
    utime(i->Name, &t);
    return 0;
}

static int
SetModes(TarInfo * i)
{
    struct utimbuf t;
    chown(i->Name, i->UserID, i->GroupID);
    chmod(i->Name, i->Mode & ~S_IFMT);
    t.actime = time(0);
    t.modtime = i->ModTime;
    utime(i->Name, &t);
    return 0;
}

static int
MakeDirectory(TarInfo * i)
{
	char	path[1024];
	char *	s = path;

    verbose("Directory: %s\n", i->Name);

	strcpy(path, i->Name);
	s++;
	while ( *s != 0 ) {
		if ( *s == '/' ) {
			*s = '\0';
			if ( mkdir(path, 0777) != 0 && !is_a_directory(path) ) {
				name_and_error(path);
				return 1;
			}
			*s = '/';
		}
		s++;
	}
	if ( mkdir(i->Name, i->Mode) != 0
	 && ( errno != EEXIST || !is_a_directory(i->Name) ) ) {
			name_and_error(i->Name);
			return 1;
	}
    SetModes(i);
    return 0;
}

static int
MakeHardLink(TarInfo * i)
{
    verbose("Hard Link: %s\n", i->Name);

    if ( link(i->LinkName, i->Name) != 0 ) {
        unlink(i->Name);
        if ( link(i->LinkName, i->Name) != 0 )
            return IOError(i);
    }
    return 0;
}

static int
MakeSymbolicLink(TarInfo * i)
{
    verbose("Symbolic Link: %s\n", i->Name);

    if ( symlink(i->LinkName, i->Name) != 0 ) {
        unlink(i->Name);
        if ( symlink(i->LinkName, i->Name) != 0 )
        return IOError(i);
    }
    return 0;
}

static int
MakeSpecialFile(TarInfo * i)
{
    verbose("Special File: %s\n", i->Name);

    if ( mknod(i->Name, i->Mode, i->Device) != 0 ) {
        unlink(i->Name);
        if ( mknod(i->Name, i->Mode, i->Device) != 0 )
            return IOError(i);
    }
    SetModes(i);
    return 0;
}

static const TarFunctions   functions = {
    Read,
    ExtractFile,
    MakeDirectory,
    MakeHardLink,
    MakeSymbolicLink,
    MakeSpecialFile
};

const char  star_usage[] = "star\n"
"\n"
"\tExtracts a tar archive from the standard input.\n"
"\n";

int
star_main(struct FileInfo * i, int argc, char * * argv)
{
    int status = TarExtractor((void *)0, &functions);

    if ( status == -1 ) {
        fflush(stdout);
        fprintf(stderr, "Error in archive format.\n");
        return 1;
    }
    else
        return status;
}
