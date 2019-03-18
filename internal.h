#ifndef	_INTERNAL_H_
#define	_INTERNAL_H_

#include "busybox.def.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

struct FileInfo {
	unsigned int	complainInPostProcess:1;
	unsigned int	changeUserID:1;
	unsigned int	changeGroupID:1;
	unsigned int	changeMode:1;
	unsigned int	create:1;
	unsigned int	force:1;
	unsigned int	recursive:1;
	unsigned int	processDirectoriesAfterTheirContents;
	unsigned int	makeParentDirectories:1;
	unsigned int	didOperation:1;
	unsigned int	isSymbolicLink:1;
	unsigned int	makeSymbolicLink:1;
	unsigned int	dyadic:1;
	const char *	source;
	const char *	destination;
	int				directoryLength;
	uid_t			userID;
	gid_t			groupID;
	mode_t			andWithMode;
	mode_t			orWithMode;
	struct stat		stat;
	const struct Applet *
					applet;
};

struct Applet {
	const char *	name;
	int				(*main)(struct FileInfo * i, int argc, char * * argv);
	int				(*function)(const struct FileInfo * i);
	const char *	usage;
	int				minimumArgumentCount;
	int				maximumArgumentCount;
};

extern void	name_and_error(const char *);
extern int	is_a_directory(const char *);
extern char *	join_paths(char *, const char *, const char *);

extern int	descend(
		 struct FileInfo *o
		,int 		(*function)(const struct FileInfo * i));

extern struct mntent *
		findMountPoint(const char *, const char *);

extern void	usage(const char *);

#ifdef INCLUDE_DINSTALL
	extern int dinstall_main(void);
#endif

extern int block_device_main(struct FileInfo * i, int argc, char * * argv);
extern int cat_more_main(struct FileInfo * i, int argc, char * * argv);
extern int chgrp_main(struct FileInfo * i, int argc, char * * argv);
extern int chmod_main(struct FileInfo * i, int argc, char * * argv);
extern int chown_main(struct FileInfo * i, int argc, char * * argv);
extern int clear_main(struct FileInfo * i, int argc, char * * argv);
extern int date_main(struct FileInfo * i, int argc, char * * argv);
extern int dd_main(struct FileInfo * i, int argc, char * * argv);
extern int df_main(struct FileInfo * i, int argc, char * * argv);
extern int dmesg_main(struct FileInfo * i, int argc, char * * argv);
extern int dyadic_main(struct FileInfo * i, int argc, char * * argv);
extern int false_main(struct FileInfo * i, int argc, char * * argv);
extern int fdisk_main(struct FileInfo * i, int argc, char * * argv);
extern int find_main(struct FileInfo * i, int argc, char * * argv);
extern int halt_main(struct FileInfo * i, int argc, char * * argv);
extern int init_main(struct FileInfo * i, int argc, char * * argv);
extern int kill_main(struct FileInfo * i, int argc, char * * argv);
extern int length_main(struct FileInfo * i, int argc, char * * argv);
extern int ln_main(struct FileInfo * i, int argc, char * * argv);
extern int loadkmap_main(struct FileInfo * i, int argc, char * * argv);
extern int losetup_main(struct FileInfo * i, int argc, char * * argv);
extern int ls_main(struct FileInfo * i, int argc, char * * argv);
extern int makedevs_main(struct FileInfo * i, int argc, char * * argv);
extern int math_main(struct FileInfo * i, int argc, char * * argv);
extern int mknod_main(struct FileInfo * i, int argc, char * * argv);
extern int mkswap_main(struct FileInfo * i, int argc, char * * argv);
extern int mnc_main(struct FileInfo * i, int argc, char * * argv);
extern int monadic_main(struct FileInfo * i, int argc, char * * argv);
extern int mount_main(struct FileInfo * i, int argc, char * * argv);
extern int mt_main(struct FileInfo * i, int argc, char * * argv);
extern int printf_main(struct FileInfo * i, int argc, char * * argv);
extern int pwd_main(struct FileInfo * i, int argc, char * * argv);
extern int reboot_main(struct FileInfo * i, int argc, char * * argv);
extern int rm_main(struct FileInfo * i, int argc, char * * argv);
extern int scan_partitions_main(struct FileInfo * i, int argc, char * * argv);
extern int sh_main(struct FileInfo * i, int argc, char * * argv);
extern int sleep_main(struct FileInfo * i, int argc, char * * argv);
extern int star_main(struct FileInfo * i, int argc, char * * argv);
extern int sync_main(struct FileInfo * i, int argc, char * * argv);
extern int tarcat_main(struct FileInfo * i, int argc, char * * argv);
extern int tput_main(struct FileInfo * i, int argc, char * * argv);
extern int true_main(struct FileInfo * i, int argc, char * * argv);
extern int tryopen_main(struct FileInfo * i, int argc, char * * argv);
extern int umount_main(struct FileInfo * i, int argc, char * * argv);
extern int update_main(struct FileInfo * i, int argc, char * * argv);
extern int zcat_main(struct FileInfo * i, int argc, char * * argv);
extern int gzip_main(struct FileInfo * i, int argc, char * * argv);

extern int cat_fn(const struct FileInfo * i);
extern int cp_fn(const struct FileInfo * i);
extern int dutmp_fn(const struct FileInfo * i);
extern int fdflush_fn(const struct FileInfo * i);
extern int find_fn(const struct FileInfo * i);
extern int ln_fn(const struct FileInfo * i);
extern int mkdir_fn(const struct FileInfo * i);
extern int more_fn(const struct FileInfo * i);
extern int mv_fn(const struct FileInfo * i);
extern int post_process(const struct FileInfo * i);
extern int rm_fn(const struct FileInfo * i);
extern int rmdir_fn(const struct FileInfo * i);
extern int swapoff_fn(const struct FileInfo * i);
extern int swapon_fn(const struct FileInfo * i);
extern int touch_fn(const struct FileInfo * i);
extern int update_fn(const struct FileInfo * i);

extern int mkswap(char *device_name, int pages, int check);
extern int do_umount(const char * name, int noMtab);
extern char *block_device(const char * name, struct FileInfo * f);

extern int
parse_mode(
 const char *	s
,mode_t *		or
,mode_t *		and
,int *			group_execute);

extern int		parse_user_name(const char * string, struct FileInfo * i);

extern const char	block_device_usage[];
extern const char	cat_usage[];
extern const char	chgrp_usage[];
extern const char	chmod_usage[];
extern const char	chown_usage[];
extern const char	clear_usage[];
extern const char	cp_usage[];
extern const char	date_usage[];
extern const char	dd_usage[];
extern const char	df_usage[];
extern const char	dmesg_usage[];
extern const char	dutmp_usage[];
extern const char	false_usage[];
extern const char	fdflush_usage[];
extern const char	find_usage[];
extern const char	halt_usage[];
extern const char	init_usage[];
extern const char	kill_usage[];
extern const char	length_usage[];
extern const char	ln_usage[];
extern const char	loadkmap_usage[];
extern const char	losetup_usage[];
extern const char	ls_usage[];
extern const char	math_usage[];
extern const char	makedevs_usage[];
extern const char	mkdir_usage[];
extern const char	mknod_usage[];
extern const char	mkswap_usage[];
extern const char	mnc_usage[];
extern const char	more_usage[];
extern const char	mount_usage[];
extern const char	mt_usage[];
extern const char	mv_usage[];
extern const char	printf_usage[];
extern const char	pwd_usage[];
extern const char	reboot_usage[];
extern const char	rm_usage[];
extern const char	rmdir_usage[];
extern const char	scan_partitions_usage[];
extern const char	sleep_usage[];
extern const char	star_usage[];
extern const char	swapoff_usage[];
extern const char	swapon_usage[];
extern const char	sync_usage[];
extern const char	tarcat_usage[];
extern const char	touch_usage[];
extern const char	tput_usage[];
extern const char	true_usage[];
extern const char	tryopen_usage[];
extern const char	umount_usage[];
extern const char	update_usage[];
extern const char	zcat_usage[];
extern const char	gzip_usage[];

#endif
