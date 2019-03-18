#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

static const struct Applet	applets[] = {

#ifdef BB_BLOCK_DEVICE	//sbin
{ "block_device", block_device_main, 0, block_device_usage,	1, 1 },
#endif
#ifdef BB_CAT	//bin
{ "cat",	cat_more_main, cat_fn, cat_usage,		0, -1 },
#endif
#ifdef BB_CHGRP	//bin
{ "chgrp",	chgrp_main, 0, chgrp_usage,			2, -1 },
#endif
#ifdef BB_CHMOD	//bin
{ "chmod",	chmod_main, 0, chmod_usage,			2, -1 },
#endif
#ifdef BB_CHOWN	//bin
{ "chown",	chown_main, 0, chown_usage,			2, -1 },
#endif
#ifdef BB_CLEAR	//usr/bin
{ "clear",	clear_main, 0, clear_usage,			0, 0 },
#endif
#ifdef BB_CP	//bin
{ "cp",		dyadic_main, cp_fn, cp_usage,			2, -1 },
#endif
#ifdef BB_DATE	//bin
{ "date",	date_main, 0, date_usage,			0, 1 },
#endif
#ifdef BB_DD	//bin
{ "dd",		dd_main, 0, dd_usage,				0, 2 },
#endif
#ifdef BB_DF	//bin
{ "df",		df_main, 0, df_usage,				0, -1 },
#endif
#ifdef BB_DMESG	//bin
{ "dmesg",	dmesg_main, 0, dmesg_usage,			0, 0 },
#endif
#ifdef BB_DUTMP	//usr/sbin
{ "dutmp",	cat_more_main, dutmp_fn, dutmp_usage,		0, -1 },
#endif
#ifdef BB_FALSE	//bin
{ "false",	false_main, 0, false_usage,			0, 0 },
#endif
#ifdef BB_FDFLUSH	//bin
{ "fdflush",	monadic_main, fdflush_fn, fdflush_usage,	1, -1 },
#endif
#ifdef BB_FIND	//usr/bin
{ "find",	find_main, find_fn, find_usage,			1, -1 },
#endif
#ifdef BB_HALT	//sbin
{ "halt",	halt_main, 0, halt_usage,			0, 0 },
#endif
#ifdef BB_INIT	//sbin
{ "init",	init_main, 0, init_usage,			0, -1 },
#endif
#ifdef BB_KILL	//bin
{ "kill",	kill_main, 0, kill_usage,			1, -1 },
#endif
#ifdef BB_LENGTH	//usr/bin
{ "length",	length_main, 0, length_usage,			1, 1 },
#endif
#ifdef BB_LN	//bin
{ "ln",		dyadic_main, ln_fn, ln_usage,			2, -1 },
#endif
#ifdef BB_LOADFONT	//usr/bin
{ "loadfont",	loadfont_main, 0, loadfont_usage,		0, 0 },
#endif
#ifdef BB_LOADKMAP	//sbin
{ "loadkmap",	loadkmap_main, 0, loadkmap_usage,		0, 0 },
#endif
#ifdef BB_LOSETUP	//sbin
{ "losetup",	losetup_main, 0, losetup_usage,			1, 2 },
#endif
#ifdef BB_LS	//bin
{ "ls",		ls_main, 0, ls_usage,				0, -1 },
#endif
#ifdef BB_MAKEDEVS	//sbin
{ "makedevs",	makedevs_main, 0, makedevs_usage,		6, 7 },
#endif
#ifdef BB_MATH	//usr/bin
{ "math",	math_main, 0, math_usage,			1, -1 },
#endif
#ifdef BB_MKDIR	//bin
{ "mkdir",	monadic_main, mkdir_fn, mkdir_usage,		1, -1 },
#endif
#ifdef BB_MKNOD	//bin
{ "mknod",	mknod_main, 0, mknod_usage,			2, 5 },
#endif
#ifdef BB_MKSWAP	//sbin
{ "mkswap",	mkswap_main, 0, mkswap_usage,			1, -1 },
#endif
#ifdef BB_MNC	//usr/bin
{ "mnc",	mnc_main, 0, mnc_usage,				2, -1 },
#endif
#ifdef BB_MORE	//bin
{ "more",	cat_more_main, more_fn, more_usage,		0, -1 },
#endif
#ifdef BB_MOUNT	//bin
{ "mount",	mount_main, 0, mount_usage,			0, -1 },
#endif
#ifdef BB_MT	//bin
{ "mt",		mt_main, 0, mt_usage,				1, -1 },
#endif
#ifdef BB_MV	//bin
{ "mv",		dyadic_main, mv_fn, mv_usage,			2, -1 },
#endif
#ifdef BB_PWD	//bin
{ "pwd",	pwd_main, 0, pwd_usage,				0, 0 },
#endif
#ifdef BB_REBOOT	//sbin
{ "reboot",	reboot_main, 0, reboot_usage,			0, 0 },
#endif
#ifdef BB_RM	//bin
{ "rm",		rm_main, rm_fn, rm_usage,			1, -1 },
#endif
#ifdef BB_RMDIR	//bin
{ "rmdir",	monadic_main, rmdir_fn, rmdir_usage,		1, -1 },
#endif
#ifdef BB_SLEEP	//bin
{ "sleep",	sleep_main, 0, sleep_usage,			1, -1 },
#endif
#ifdef BB_STAR	//bin
{ "star",	star_main, 0, star_usage,			0, 0 },
{ "untar",	star_main, 0, star_usage,			0, 0 },
#endif
#ifdef BB_SWAPOFF	//sbin
{ "swapoff",	monadic_main, swapoff_fn, swapoff_usage,	1, -1 },
#endif
#ifdef BB_SWAPON	//sbin
{ "swapon",	monadic_main, swapon_fn, swapon_usage,		1, -1 },
#endif
#ifdef BB_SYNC	//bin
{ "sync",	sync_main, 0, sync_usage,			0, 0 },
#endif
#ifdef BB_TARCAT	//bin
{ "tarcat",	tarcat_main, 0, tarcat_usage,			1, 1 },
#endif
#ifdef BB_TOUCH	//usr/bin
{ "touch",	monadic_main, touch_fn, touch_usage,		1, -1 },
#endif
#ifdef BB_TRUE	//bin
{ "true",	true_main, 0, true_usage,			0, 0 },
#endif
#ifdef BB_UMOUNT	//bin
{ "umount",	umount_main, 0, umount_usage,			1, -1 },
#endif
#ifdef BB_UPDATE	//sbin
{ "update",	update_main, 0, update_usage,			0, -1 },
#endif
#ifdef BB_ZCAT	//bin
{ "zcat",	zcat_main, 0, zcat_usage,			0, -1 },
{ "gunzip",	zcat_main, 0, zcat_usage,			0, -1 },
#endif
#ifdef BB_GZIP	//bin
{ "gzip",	gzip_main, 0, gzip_usage,			0, 0 },
#endif
{ 0 }
};

extern int
main(int argc, char * * argv)
{
	char * s = argv[0];
	char * name = argv[0];
	const struct Applet * a = applets;
	struct FileInfo	i;

	while ( *s != '\0' ) {
		if ( *s++ == '/' )
			name = s;
	}

#ifdef INCLUDE_DINSTALL
	if ( strcmp(name,"dbootstrap") == 0) {
		int status;

		status = dbootstrap_main();
		exit (status);
	}
#endif

	while ( a->name != 0 ) {
		if ( strcmp(name, a->name) == 0 ) {
			int	status;

			if ( argc - 1 < a->minimumArgumentCount
			 || (a->maximumArgumentCount > 0
			  && argc - 1 > a->maximumArgumentCount )
			 || (a->usage && argc >= 2 && strcmp(argv[1], "--help") == 0 ) ) {
				usage(a->usage);
				return 1;
			}
			errno = 0;
			memset((void *)&i, 0, sizeof(struct FileInfo));
			i.orWithMode = 0777;
			i.andWithMode = ~0;
			i.applet = a;
			status = ((*(a->main))(&i, argc, argv));
			if ( status < 0 ) {
				fprintf( stderr,"%s: %s\n"
				,a->name ,strerror(errno));
			}
			exit(status);
		}
		a++;
	}
	fprintf(stderr, "BusyBox v%s (%s) multi-call binary -- GPL2\n"
			"\tError: called as %s. No function defined for that.\n",
			BB_VER, BB_BT, argv[0]);
	return -1;
}

