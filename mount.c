/* vi: set sw=4 ts=4: */
/*
 * Mini mount implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 3/21/1999	Charles P. Wright <cpwright@cpwright.com>
 *		searches through fstab when -a is passed
 *		will try mounting stuff with all fses when passed -t auto
 *
 * 1999-04-17	Dave Cinege...Rewrote -t auto. Fixed ro mtab.
 *
 * 1999-10-07	Erik Andersen <andersen@lineo.com>, <andersee@debian.org>.
 *              Rewrite of a lot of code. Removed mtab usage (I plan on
 *              putting it back as a compile-time option some time), 
 *              major adjustments to option parsing, and some serious 
 *              dieting all around.
 *
 * 1999-11-06	mtab suppport is back - andersee
 *
 * 2000-01-12   Ben Collins <bcollins@debian.org>, Borrowed utils-linux's
 *              mount to add loop support.
 */

#include "internal.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <sys/mount.h>
#include <ctype.h>
#if defined BB_FEATURE_USE_DEVPS_PATCH
#include <linux/devmtab.h>
#endif


#if defined BB_FEATURE_MOUNT_LOOP
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/loop.h>


static int use_loop = FALSE;
#endif

extern const char mtab_file[];	/* Defined in utility.c */

static const char mount_usage[] = "\tmount [flags]\n"
	"\tmount [flags] device directory [-o options,more-options]\n"
	"\n" "Flags:\n" "\t-a:\tMount all file systems in fstab.\n"
#ifdef BB_MTAB
	"\t-f:\t\"Fake\" mount. Add entry to mount table but don't mount it.\n"
	"\t-n:\tDon't write a mount table entry.\n"
#endif
	"\t-o option:\tOne of many filesystem options, listed below.\n"
	"\t-r:\tMount the filesystem read-only.\n"
	"\t-t filesystem-type:\tSpecify the filesystem type.\n"
	"\t-w:\tMount for reading and writing (default).\n"
	"\n"
	"Options for use with the \"-o\" flag:\n"
	"\tasync / sync:\tWrites are asynchronous / synchronous.\n"
	"\tdev / nodev:\tAllow use of special device files / disallow them.\n"
	"\texec / noexec:\tAllow use of executable files / disallow them.\n"
#if defined BB_FEATURE_MOUNT_LOOP
	"\tloop: Mounts a file via loop device.\n"
#endif
	"\tsuid / nosuid:\tAllow set-user-id-root programs / disallow them.\n"
	"\tremount: Re-mount a currently-mounted filesystem, changing its flags.\n"
	"\tro / rw: Mount for read-only / read-write.\n"
	"There are EVEN MORE flags that are specific to each filesystem.\n"
	"You'll have to see the written documentation for those.\n";


struct mount_options {
	const char *name;
	unsigned long and;
	unsigned long or;
};

static const struct mount_options mount_options[] = {
	{"async", ~MS_SYNCHRONOUS, 0},
	{"defaults", ~0, 0},
	{"dev", ~MS_NODEV, 0},
	{"exec", ~MS_NOEXEC, 0},
	{"nodev", ~0, MS_NODEV},
	{"noexec", ~0, MS_NOEXEC},
	{"nosuid", ~0, MS_NOSUID},
	{"remount", ~0, MS_REMOUNT},
	{"ro", ~0, MS_RDONLY},
	{"rw", ~MS_RDONLY, 0},
	{"suid", ~MS_NOSUID, 0},
	{"sync", ~0, MS_SYNCHRONOUS},
	{0, 0, 0}
};

static int
do_mount(char *specialfile, char *dir, char *filesystemtype,
		 long flags, void *string_flags, int useMtab, int fakeIt,
		 char *mtab_opts)
{
	int status = 0;
	char *lofile = NULL;

#if defined BB_MTAB
	if (fakeIt == FALSE)
#endif
	{
#if defined BB_FEATURE_MOUNT_LOOP
		if (use_loop==TRUE) {
			int loro = flags & MS_RDONLY;
			char *lofile = specialfile;

			specialfile = find_unused_loop_device();
			if (specialfile == NULL) {
				fprintf(stderr, "Could not find a spare loop device\n");
				return (FALSE);
			}
			if (set_loop(specialfile, lofile, 0, &loro)) {
				fprintf(stderr, "Could not setup loop device\n");
				return (FALSE);
			}
			if (!(flags & MS_RDONLY) && loro) {	/* loop is ro, but wanted rw */
				fprintf(stderr, "WARNING: loop device is read-only\n");
				flags &= ~MS_RDONLY;
			}
		}
#endif
		status =
			mount(specialfile, dir, filesystemtype, flags, string_flags);
	}


	/* If the mount was sucessful, do anything needed, then return TRUE */
	if (status == 0) {

#if defined BB_MTAB
		if (useMtab == TRUE) {
			write_mtab(specialfile, dir, filesystemtype, flags, mtab_opts);
		}
#endif
		return (TRUE);
	}

	/* Bummer.  mount failed.  Clean up */
#if defined BB_FEATURE_MOUNT_LOOP
	if (lofile != NULL) {
		del_loop(specialfile);
	}
#endif
	return (FALSE);
}



/* Seperate standard mount options from the nonstandard string options */
static void
parse_mount_options(char *options, unsigned long *flags, char *strflags)
{
	while (options) {
		int gotone = FALSE;
		char *comma = strchr(options, ',');
		const struct mount_options *f = mount_options;

		if (comma)
			*comma = '\0';

		while (f->name != 0) {
			if (strcasecmp(f->name, options) == 0) {

				*flags &= f->and;
				*flags |= f->or;
				gotone = TRUE;
				break;
			}
			f++;
		}
#if defined BB_FEATURE_MOUNT_LOOP
		if (gotone == FALSE && !strcasecmp("loop", options)) {	/* loop device support */
			use_loop = TRUE;
			gotone = TRUE;
		}
#endif
		if (*strflags && strflags != '\0' && gotone == FALSE) {
			char *temp = strflags;

			temp += strlen(strflags);
			*temp++ = ',';
			*temp++ = '\0';
		}
		if (gotone == FALSE)
			strcat(strflags, options);
		if (comma) {
			*comma = ',';
			options = ++comma;
		} else {
			break;
		}
	}
}

int
mount_one(char *blockDevice, char *directory, char *filesystemType,
		  unsigned long flags, char *string_flags, int useMtab, int fakeIt,
		  char *mtab_opts, int whineOnErrors)
{
	int status = 0;

#if defined BB_FEATURE_USE_PROCFS
	char buf[255];
	if (strcmp(filesystemType, "auto") == 0) {
		FILE *f = fopen("/proc/filesystems", "r");

		if (f == NULL)
			return (FALSE);

		while (fgets(buf, sizeof(buf), f) != NULL) {
			filesystemType = buf;
			if (*filesystemType == '\t') {	// Not a nodev filesystem

				// Add NULL termination to each line
				while (*filesystemType && *filesystemType != '\n')
					filesystemType++;
				*filesystemType = '\0';

				filesystemType = buf;
				filesystemType++;	// hop past tab

				status = do_mount(blockDevice, directory, filesystemType,
								  flags | MS_MGC_VAL, string_flags,
								  useMtab, fakeIt, mtab_opts);
				if (status == TRUE)
					break;
			}
		}
		fclose(f);
	} else
#endif
#if defined BB_FEATURE_USE_DEVPS_PATCH
	if (strcmp(filesystemType, "auto") == 0) {
		int fd, i, numfilesystems;
		char device[] = "/dev/mtab";
		struct k_fstype *fslist;

		/* open device */ 
		fd = open(device, O_RDONLY);
		if (fd < 0)
			fatalError("open failed for `%s': %s\n", device, strerror (errno));

		/* How many filesystems?  We need to know to allocate enough space */
		numfilesystems = ioctl (fd, DEVMTAB_COUNT_FILESYSTEMS);
		if (numfilesystems<0)
			fatalError("\nDEVMTAB_COUNT_FILESYSTEMS: %s\n", strerror (errno));
		fslist = (struct k_fstype *) calloc ( numfilesystems, sizeof(struct k_fstype));

		/* Grab the list of available filesystems */
		status = ioctl (fd, DEVMTAB_GET_FILESYSTEMS, fslist);
		if (status<0)
			fatalError("\nDEVMTAB_GET_FILESYSTEMS: %s\n", strerror (errno));

		/* Walk the list trying to mount filesystems 
		 * that do not claim to be nodev filesystems */
		for( i = 0 ; i < numfilesystems ; i++) {
			if (fslist[i].mnt_nodev)
				continue;
			status = do_mount(blockDevice, directory, fslist[i].mnt_type,
							  flags | MS_MGC_VAL, string_flags,
							  useMtab, fakeIt, mtab_opts);
			if (status == TRUE)
				break;
		}
		free( fslist);
		close(fd);
	} else
#endif
	{
		status = do_mount(blockDevice, directory, filesystemType,
						  flags | MS_MGC_VAL, string_flags, useMtab,
						  fakeIt, mtab_opts);
	}

	if (status == FALSE && whineOnErrors == TRUE) {
		if (whineOnErrors == TRUE) {
			fprintf(stderr, "Mounting %s on %s failed: %s\n",
					blockDevice, directory, strerror(errno));
		}
		return (FALSE);
	}
	return (TRUE);
}

extern int mount_main(int argc, char **argv)
{
	char string_flags_buf[1024] = "";
	char *string_flags = string_flags_buf;
	char *extra_opts = string_flags_buf;
	unsigned long flags = 0;
	char *filesystemType = "auto";
	char *device = NULL;
	char *directory = NULL;
	int all = FALSE;
	int fakeIt = FALSE;
	int useMtab = TRUE;
	int i;

#if defined BB_FEATURE_USE_DEVPS_PATCH
	if (argc == 1) {
		int fd, i, numfilesystems;
		char device[] = "/dev/mtab";
		struct k_mntent *mntentlist;

		/* open device */ 
		fd = open(device, O_RDONLY);
		if (fd < 0)
			fatalError("open failed for `%s': %s\n", device, strerror (errno));

		/* How many mounted filesystems?  We need to know to 
		 * allocate enough space for later... */
		numfilesystems = ioctl (fd, DEVMTAB_COUNT_MOUNTS);
		if (numfilesystems<0)
			fatalError( "\nDEVMTAB_COUNT_MOUNTS: %s\n", strerror (errno));
		mntentlist = (struct k_mntent *) calloc ( numfilesystems, sizeof(struct k_mntent));
		
		/* Grab the list of mounted filesystems */
		if (ioctl (fd, DEVMTAB_GET_MOUNTS, mntentlist)<0)
			fatalError( "\nDEVMTAB_GET_MOUNTS: %s\n", strerror (errno));

		for( i = 0 ; i < numfilesystems ; i++) {
			fprintf( stdout, "%s %s %s %s %d %d\n", mntentlist[i].mnt_fsname,
					mntentlist[i].mnt_dir, mntentlist[i].mnt_type, 
					mntentlist[i].mnt_opts, mntentlist[i].mnt_freq, 
					mntentlist[i].mnt_passno);
		}
		/* Don't bother to close files or free memory.  Exit 
		 * does that automagically, so we can save a few bytes */
#if 0
		free( mntentlist);
		close(fd);
#endif
		exit(TRUE);
	}
#else
	if (argc == 1) {
		FILE *mountTable = setmntent(mtab_file, "r");

		if (mountTable) {
			struct mntent *m;

			while ((m = getmntent(mountTable)) != 0) {
				char *blockDevice = m->mnt_fsname;
				if (strcmp(blockDevice, "/dev/root") == 0) {
					find_real_root_device_name( blockDevice);
				}
				printf("%s on %s type %s (%s)\n", blockDevice, m->mnt_dir,
					   m->mnt_type, m->mnt_opts);
			}
			endmntent(mountTable);
		} else {
			perror(mtab_file);
		}
		exit(TRUE);
	}
#endif

	/* Parse options */
	i = --argc;
	argv++;
	while (i > 0 && **argv) {
		if (**argv == '-') {
			char *opt = *argv;

			while (i > 0 && *++opt)
				switch (*opt) {
				case 'o':
					if (--i == 0) {
						goto goodbye;
					}
					parse_mount_options(*(++argv), &flags, string_flags);
					break;
				case 'r':
					flags |= MS_RDONLY;
					break;
				case 't':
					if (--i == 0) {
						goto goodbye;
					}
					filesystemType = *(++argv);
					break;
				case 'w':
					flags &= ~MS_RDONLY;
					break;
				case 'a':
					all = TRUE;
					break;
				case 'f':
					fakeIt = TRUE;
					break;
#ifdef BB_MTAB
				case 'n':
					useMtab = FALSE;
					break;
#endif
				case 'v':
					break; /* ignore -v */
				case 'h':
				case '-':
					goto goodbye;
				}
		} else {
			if (device == NULL)
				device = *argv;
			else if (directory == NULL)
				directory = *argv;
			else {
				goto goodbye;
			}
		}
		i--;
		argv++;
	}

	if (all == TRUE) {
		struct mntent *m;
		FILE *f = setmntent("/etc/fstab", "r");

		if (f == NULL)
			fatalError( "\nCannot read /etc/fstab: %s\n", strerror (errno));

		while ((m = getmntent(f)) != NULL) {
			// If the file system isn't noauto, 
			// and isn't swap or nfs, then mount it
			if ((!strstr(m->mnt_opts, "noauto")) &&
				(!strstr(m->mnt_type, "swap")) &&
				(!strstr(m->mnt_type, "nfs"))) {
				flags = 0;
				*string_flags = '\0';
				parse_mount_options(m->mnt_opts, &flags, string_flags);
				/* If the directory is /, try to remount
				 * with the options specified in fstab */
				if (m->mnt_dir[0] == '/' && m->mnt_dir[1] == '\0') {
					flags |= MS_REMOUNT;
				}
				if (mount_one(m->mnt_fsname, m->mnt_dir, m->mnt_type,
						  flags, string_flags, useMtab, fakeIt,
						  extra_opts, FALSE)) 
				{
					/* Try again, but this time try a remount */
					mount_one(m->mnt_fsname, m->mnt_dir, m->mnt_type,
							  flags|MS_REMOUNT, string_flags, useMtab, fakeIt,
							  extra_opts, TRUE);
				}
			}
		}
		endmntent(f);
	} else {
		if (device && directory) {
#ifdef BB_NFSMOUNT
			if (strcmp(filesystemType, "nfs") == 0) {
				if (nfsmount
					(device, directory, &flags, &extra_opts, &string_flags,
					 1) != 0)
					exit(FALSE);
			}
#endif
			exit(mount_one(device, directory, filesystemType,
						   flags, string_flags, useMtab, fakeIt,
						   extra_opts, TRUE));
		} else {
			goto goodbye;
		}
	}
	exit(TRUE);

  goodbye:
	usage(mount_usage);
}
