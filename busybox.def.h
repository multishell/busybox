/* vi: set sw=4 ts=4: */
// This file defines the feature set to be compiled into busybox.
// When you turn things off here, they won't be compiled in at all.
//
//// This file is parsed by sed. You MUST use single line comments.
//   i.e.  //#define BB_BLAH
//
//
// BusyBox Applications
#define BB_AR
#define BB_BASENAME
#define BB_CAT
#define BB_CHMOD_CHOWN_CHGRP
#define BB_CHROOT
#define BB_CHVT
#define BB_CLEAR
#define BB_CP_MV
#define BB_CUT
#define BB_DATE
#define BB_DC
#define BB_DD
#define BB_DEALLOCVT
#define BB_DF
#define BB_DIRNAME
#define BB_DMESG
#define BB_DUTMP
#define BB_DU
#define BB_ECHO
#define BB_FBSET
#define BB_FDFLUSH
#define BB_FIND
#define BB_FREE
#define BB_FREERAMDISK
#define BB_FSCK_MINIX
#define BB_GREP
#define BB_GUNZIP
#define BB_GZIP
#define BB_HALT
#define BB_HEAD
#define BB_HOSTID
#define BB_HOSTNAME
#define BB_ID
#define BB_INIT
#define BB_INSMOD
#define BB_KILL
#define BB_KILLALL
#define BB_KLOGD
#define BB_LENGTH
#define BB_LN
#define BB_LOADACM
#define BB_LOADFONT
#define BB_LOADKMAP
#define BB_LOGGER
#define BB_LOGNAME
#define BB_LS
#define BB_LSMOD
#define BB_MAKEDEVS
//#define BB_MD5SUM
#define BB_MKDIR
#define BB_MKFIFO
#define BB_MKFS_MINIX
#define BB_MKNOD
#define BB_MKSWAP
#define BB_MKTEMP
#define BB_NC
#define BB_MORE
#define BB_MOUNT
#define BB_MT
#define BB_NSLOOKUP
#define BB_PING
#define BB_POWEROFF
#define BB_PRINTF
#define BB_PS
#define BB_PWD
#define BB_REBOOT
#define BB_RM
#define BB_RMDIR
#define BB_RMMOD
#define BB_SED
#define BB_SETKEYCODES
#define BB_SFDISK
#define BB_SH
#define BB_SLEEP
#define BB_SORT
#define BB_SWAPONOFF
#define BB_SYNC
#define BB_SYSLOGD
#define BB_TAIL
#define BB_TAR
#define BB_TEE
#define BB_TEST
#define BB_TELNET
#define BB_TOUCH
#define BB_TR
#define BB_TRUE_FALSE
#define BB_TTY
#define BB_UPTIME
#define BB_USLEEP
#define BB_WC
#define BB_WHICH
#define BB_WHOAMI
#define BB_UUENCODE
#define BB_UUDECODE
#define BB_UMOUNT
#define BB_UNIQ
#define BB_UNAME
#define BB_UPDATE
#define BB_YES
// End of Applications List
//
//
//
// ---------------------------------------------------------
// This is where feature definitions go.  Generally speaking,
// turning this stuff off makes things a bit smaller (and less 
// pretty/useful).
//
//
// Turn this on to use Erik's very cool devps, devmtab, 
// etc kernel drivers, thereby eliminating the need for 
// the /proc filesystem and thereby saving lots and lots 
// memory for more important things.
// You can't use this and USE_PROCFS at the same time...
// (BTW, I emailed Linus and this patch will not be going into the stock kernel)
//#define BB_FEATURE_USE_DEVPS_PATCH
//
// enable features that use the /proc filesystem (apps that 
// break without this will tell you on compile)...
// You can't use this and BB_FEATURE_USE_DEVPS_PATCH 
// at the same time...
#define BB_FEATURE_USE_PROCFS
//
// Enable full regular expressions.  This adds about 
// 4k.  When this is off, things that would normally
// use regualr expressions (like grep) will just use
// normal strings.
#define BB_FEATURE_FULL_REGULAR_EXPRESSIONS
//
// This compiles out everything but the most 
// trivial --help usage information (i.e. reduces binary size)
//#define BB_FEATURE_TRIVIAL_HELP
//
// Use termios to manipulate the screen ('more' is prettier with this on)
#define BB_FEATURE_USE_TERMIOS
//
// calculate terminal & column widths (for more and ls)
#define BB_FEATURE_AUTOWIDTH
//
// show username/groupnames (bypasses libc6 NSS) for ls
#define BB_FEATURE_LS_USERNAME
//
// show file timestamps in ls
#define BB_FEATURE_LS_TIMESTAMPS
//
// enable ls -p and -F
#define BB_FEATURE_LS_FILETYPES
//
// enable ls -R
#define BB_FEATURE_LS_RECURSIVE
//
// Change ping implementation -- simplified, featureless, but really small.
//#define BB_SIMPLE_PING
//
// Make init use a simplified /etc/inittab file (recommended).
#define BB_FEATURE_USE_INITTAB
//
//Enable init being called as /linuxrc
//#define BB_FEATURE_LINUXRC
//
//Have init enable core dumping for child processes (for debugging only) 
//#define BB_FEATURE_INIT_COREDUMPS
//
// Allow init to permenently chroot, and umount the old root fs
// just like an initrd does.  Requires a kernel patch by Werner Almesberger. 
// ftp://icaftp.epfl.ch/pub/people/almesber/misc/umount-root-*.tar.gz
//#define BB_FEATURE_INIT_CHROOT
//
//Make sure nothing is printed to the console on boot
#define BB_FEATURE_EXTRA_QUIET
//
//Simple tail implementation (2k vs 6k for the full one).  Still
//provides 'tail -f' support -- but for only one file at a time.
#define BB_FEATURE_SIMPLE_TAIL
//
// Enable support for loop devices in mount
#define BB_FEATURE_MOUNT_LOOP
//
// Enable support for a real /etc/mtab file instead of /proc/mounts
//#define BB_FEATURE_MOUNT_MTAB_SUPPORT
//
// Enable support forced filesystem unmounting 
// (i.e. in case of an unreachable NFS system).
#define BB_FEATURE_MOUNT_FORCE
//
// Enable support for mounting remote NFS volumes
// (This does not yet work with Linux 2.[34].x kernels)
//#define BB_FEATURE_NFSMOUNT
//
// Enable support for creation of tar files.
#define BB_FEATURE_TAR_CREATE
//
// Enable support for "--exclude" for excluding files
#define BB_FEATURE_TAR_EXCLUDE
//
//// Enable reverse sort
#define BB_FEATURE_SORT_REVERSE
//
// Enable command line editing in the shell
#define BB_FEATURE_SH_COMMAND_EDITING
//
//Allow the shell to invoke all the compiled in BusyBox commands as if they
//were shell builtins.  Nice for staticly linking an emergency rescue shell
//amoung other thing.
#define BB_FEATURE_STANDALONE_SHELL
//
// Enable tab completion in the shell (not yet 
// working very well -- so don't turn this on)
//#define BB_FEATURE_SH_TAB_COMPLETION
//
//Turn on extra fbset options
//#define BB_FEATURE_FBSET_FANCY
//
// You must enable one or both of these features
// Support installing modules from pre 2.1 kernels
//#define BB_FEATURE_INSMOD_OLD_KERNEL
// Support installing modules from kernel versions after 2.1.18
#define BB_FEATURE_INSMOD_NEW_KERNEL
//
// Support module version checking
//#define BB_FEATURE_INSMOD_VERSION_CHECKING
//
//
// End of Features List
//
//
//
//
//
//
//---------------------------------------------------
// Nothing beyond this point should ever be touched by 
// mere mortals so leave this stuff alone.
//
#ifdef BB_FEATURE_MOUNT_MTAB_SUPPORT
#define BB_MTAB
#endif
//
#if defined BB_FEATURE_FULL_REGULAR_EXPRESSIONS && (defined BB_SED || defined BB_GREP )
#define BB_REGEXP
#endif
//
#if defined BB_FEATURE_SH_COMMAND_EDITING && defined BB_SH
#define BB_CMDEDIT
#endif
//
#ifdef BB_KILLALL
#ifndef BB_KILL
#define BB_KILL
#endif
#endif
//
#ifdef BB_FEATURE_LINUXRC
#ifndef BB_INIT
#define BB_INIT
#define BB_LINUXRC
#endif
#endif
//
#ifdef BB_GZIP
#ifndef BB_GUNZIP
#define BB_GUNZIP
#endif
#endif
//
#if defined BB_MOUNT && defined BB_FEATURE_NFSMOUNT
#define BB_NFSMOUNT
#endif
//
#if defined BB_FEATURE_SH_COMMAND_EDITING
#ifndef BB_FEATURE_USE_TERMIOS
#define BB_FEATURE_USE_TERMIOS
#endif
#endif
//
#if defined BB_FEATURE_AUTOWIDTH
#ifndef BB_FEATURE_USE_TERMIOS
#define BB_FEATURE_USE_TERMIOS
#endif
#endif
//
#if defined BB_INSMOD
#ifndef BB_FEATURE_INSMOD_OLD_KERNEL
#define BB_FEATURE_INSMOD_NEW_KERNEL
#endif
#endif
