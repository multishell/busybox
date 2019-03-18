#include <ctype.h>
#include <dirent.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <search.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "busybox.h"

#define DEPENDSMAX	64	/* maximum number of depends we can handle */

/* Should we do full dependency checking? */
//#define DODEPENDS 0

/* Should we do debugging? */
//#define DODEBUG 0

#ifdef DODEBUG
#define SYSTEM(x) do_system(x)
#define DPRINTF(fmt,args...) fprintf(stderr, fmt, ##args)
#else
#define SYSTEM(x) system(x)
#define DPRINTF(fmt,args...) /* nothing */
#endif

/* from dpkg-deb.c */

static const char statusfile[] = "/var/lib/dpkg/status.udeb";
static const char new_statusfile[] = "/var/lib/dpkg/status.udeb.new";
static const char bak_statusfile[] = "/var/lib/dpkg/status.udeb.bak";

static const char infodir[] = "/var/lib/dpkg/info/";
static const char udpkg_quiet[] = "UDPKG_QUIET";

//static const int status_want_unknown	= 1;
static const int state_want_install	= 2;
//static const int state_want_hold	= 3;
//static const int state_want_deinstall	= 4;
//static const int state_want_purge	= 5;

static const int state_flag_ok	= 1;
//static const int state_flag_reinstreq = 2; 
//static const int state_flag_hold	= 3;
//static const int state_flag_holdreinstreq = 4;

//static const int state_statusnoninstalled = 1;
static const int state_status_unpacked	= 2;
static const int state_status_halfconfigured = 3; 
static const int state_status_installed	= 4;
static const int state_status_halfinstalled	= 5;
//static const int state_statusconfigfiles	= 6;
//static const int state_statuspostinstfailed	= 7;
//static const int state_statusremovalfailed	= 8;

static const char *state_words_want[] = { "unknown", "install", "hold", "deinstall", "purge", 0 };
static const char *state_words_flag[] = { "ok", "reinstreq", "hold", "hold-reinstreq", 0 };
static const char *state_words_status[] = { "not-installed", "unpacked", "half-configured", "installed", 
		"half-installed", "config-files", "post-inst-failed", "removal-failed", 0 };

static const int color_white	= 0;
static const int color_grey	= 1;
static const int color_black	= 2;

/* data structures */ 
typedef struct package_s {
	char *filename;
	char *package;
	unsigned char state_want;
	unsigned char state_flag;
	unsigned char state_status;
	char *depends;
	char *provides;
	char *description;
	char *priority;
	char *section;
	char *installed_size;
	char *maintainer;
	char *source;
	char *version;
	char *pre_depends;
	char *replaces;
	char *recommends;
	char *suggests;
	char *conflicts;
	char *conffiles;
	char *long_description;
	char *architecture;
	char *md5sum;
	int installer_menu_item;
	char color; /* for topo-sort */
	struct package_s *requiredfor[DEPENDSMAX]; 
	unsigned short requiredcount;
	struct package_s *next;
} package_t;

#ifdef DODEBUG
static int do_system(const char *cmd)
{
	DPRINTF("cmd is %s\n", cmd);
	return system(cmd);
}
#else
#define do_system(cmd) system(cmd)
#endif

static int package_compare(const void *p1, const void *p2)
{
	return strcmp(((package_t *)p1)->package, 
		((package_t *)p2)->package);
}

#ifdef DODEPENDS

static char **depends_split(const char *dependsstr)
{
	static char *dependsvec[DEPENDSMAX];
	char *p;
	int i = 0;

	dependsvec[0] = 0;
	if (dependsstr == 0) {
		goto end;
	}

	p = xstrdup(dependsstr);
	while (*p != 0 && *p != '\n') {
		if (*p != ' ') {
			if (*p == ',') {
				*p = 0;
				dependsvec[++i] = 0;
			} else {
				if (dependsvec[i] == 0) {
					dependsvec[i] = p;
				}
			}
		} else {
			*p = 0; /* eat the space... */
		}
		p++;
	}
	*p = 0;

end:
	dependsvec[i+1] = 0;
	return dependsvec;
}

/* Topological sort algorithm:
 * ordered is the output list, pkgs is the dependency graph, pkg is 
 * the current node
 *
 * recursively add all the adjacent nodes to the ordered list, marking
 * each one as visited along the way
 *
 * yes, this algorithm looks a bit odd when all the params have the
 * same type :-)
 */
static void depends_sort_visit(package_t **ordered, package_t *pkgs,
		package_t *pkg)
{
	unsigned short i;

	/* mark node as processing */
	pkg->color = color_grey;

	/* visit each not-yet-visited node */
	for (i = 0; i < pkg->requiredcount; i++)
		if (pkg->requiredfor[i]->color == color_white)
			depends_sort_visit(ordered, pkgs, pkg->requiredfor[i]);

#if 0
	/* add it to the list */
	newnode = (struct package_t *)xmalloc(sizeof(struct package_t));
	/* make a shallow copy */
	*newnode = *pkg;
	newnode->next = *ordered;
	*ordered = newnode;
#endif

	pkg->next = *ordered;
	*ordered = pkg;

	/* mark node as done */
	pkg->color = color_black;
}

static package_t *depends_sort(package_t *pkgs)
{
	/* TODO: it needs to break cycles in the to-be-installed package 
	 * graph... */
	package_t *ordered = NULL;
	package_t *pkg;

	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		pkg->color = color_white;
	}
	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		if (pkg->color == color_white) {
			depends_sort_visit(&ordered, pkgs, pkg);
		}
	}

	/* Leaks the old list... return the new one... */
	return ordered;
}


/* resolve package dependencies -- 
 * for each package in the list of packages to be installed, we parse its 
 * dependency info to determine if the dependent packages are either 
 * already installed, or are scheduled to be installed. If both tests fail
 * than bail.
 *
 * The algorithm here is O(n^2*m) where n = number of packages to be 
 * installed and m is the # of dependencies per package. Not a terribly
 * efficient algorithm, but given that at any one time you are unlikely
 * to install a very large number of packages it doesn't really matter
 */
static package_t *depends_resolve(package_t *pkgs, void *status)
{
	package_t *pkg, *chk;
	package_t dependpkg;
	char **dependsvec;
	int i;
	void *found;

	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		dependsvec = depends_split(pkg->depends);
		i = 0;
		while (dependsvec[i] != 0) {
			/* Check for dependencies; first look for installed packages */
			dependpkg.package = dependsvec[i];
			if (((found = tfind(&dependpkg, &status, package_compare)) == 0) ||
			    ((chk = *(package_t **)found) && (chk->state_flag & state_flag_ok) &&
				(chk->state_status & state_status_installed))) {

				/* if it fails, we look through the list of packages we are going to 
				 * install */
				for (chk = pkgs; chk != 0; chk = chk->next) {
					if (strcmp(chk->package, dependsvec[i]) == 0 || (chk->provides && 
					     strncmp(chk->provides, dependsvec[i], strlen(dependsvec[i])) == 0)) {
						if (chk->requiredcount >= DEPENDSMAX) {
							error_msg("Too many dependencies for %s", chk->package);
							return 0;
						}
						if (chk != pkg) {
							chk->requiredfor[chk->requiredcount++] = pkg;
						}
						break;
					}
				}
				if (chk == 0) {
					error_msg("%s depends on %s, but it is not going to be installed", pkg->package, dependsvec[i]);
					return 0;
				}
			}
			i++;
		}
	}

	return depends_sort(pkgs);
}
#endif

/* Status file handling routines
 * 
 * This is a fairly minimalistic implementation. there are two main functions 
 * that are supported:
 * 
 * 1) reading the entire status file:
 *    the status file is read into memory as a binary-tree, with just the 
 *    package and status info preserved
 *
 * 2) merging the status file
 *    control info from (new) packages is merged into the status file, 
 *    replacing any pre-existing entries. when a merge happens, status info 
 *    read using the status_read function is written back to the status file
 */
static unsigned char status_parse(const char *line, const char **status_words)
{
	unsigned char status_num;
	int i = 0;

	while (status_words[i] != 0) {
		if (strncmp(line, status_words[i], strlen(status_words[i])) == 0) {
			status_num = (char)i;
			return(status_num);
		}
		i++;
	}
	/* parse error */
	error_msg("Invalid status word");
	return(0);
}

/*
 * Read the buffered control file and parse it,
 * filling parsed fields into the package structure
 */
static int fill_package_struct(package_t *package, const char *package_buffer)
{
	char *field = NULL;
	int field_start = 0;
	int field_length = 0;

	while ((field = read_package_field(&package_buffer[field_start])) != NULL) {
		field_length = strlen(field);
		field_start += (field_length + 1);

		if (strlen(field) == 0) {
			printf("empty line: *this shouldnt happen i dont think*\n");
			break;
		}

		/* these are common to both installed and uninstalled packages */
		if (strstr(field, "Package: ") == field) {
			package->package = strdup(field + 9);
		}
		else if (strstr(field, "Depends: ") == field) {
			package->depends = strdup(field + 9);
		}
		else if (strstr(field, "Provides: ") == field) {
			package->provides = strdup(field + 10);
		}
		/* This is specific to the Debian Installer. Ifdef? */
		else if (strstr(field, "installer-menu-item: ") == field) {
			package->installer_menu_item = atoi(field + 21);
		}
		else if (strstr(field, "Description: ") == field) {
			package->description = strdup(field + 13);
		}
		else if (strstr(field, "Priority: ") == field) {
			package->priority = strdup(field + 10);
		}
		else if (strstr(field, "Section: ") == field) {
			package->section = strdup(field + 9);
		}
		else if (strstr(field, "Installed-Size: ") == field) {
			package->installed_size = strdup(field + 16);
		}
		else if (strstr(field, "Maintainer: ") == field) {
			package->maintainer = strdup(field + 12);
		}
		else if (strstr(field, "Version: ") == field) {
			package->version = strdup(field + 9);
		}
		else if (strstr(field, "Suggests: ") == field) {
			package->suggests = strdup(field + 10);
		}
		else if (strstr(field, "Recommends: ") == field) {
			package->recommends = strdup(field + 12);
		}
/*		else if (strstr(field, "Conffiles: ") == field) {
			package->conffiles = read_block(file);
				package->conffiles = xcalloc(1, 1);
				while ((field = strtok(NULL, "\n")) != NULL) {
					package->long_description = xrealloc(package->conffiles, 
					strlen(package->conffiles) + strlen(field) + 1);
					strcat(package->conffiles, field);
				}
			}
*/
		/* These are only in available file */
		else if (strstr(field, "Architecture: ") == field) {
			package->architecture = strdup(field + 14);
		}
		else if (strstr(field, "Filename: ") == field) {
			package->filename = strdup(field + 10);
		}
		else if (strstr(field, "MD5sum ") == field) {
			package->md5sum = strdup(field + 7);
		}

		/* This is only needed for status file */
		if (strstr(field, "Status: ") == field) {
			char *word_pointer;

			word_pointer = strchr(field, ' ') + 1;
			package->state_want = status_parse(word_pointer, state_words_want);
			word_pointer = strchr(word_pointer, ' ') + 1;
			package->state_flag = status_parse(word_pointer, state_words_flag);
			word_pointer = strchr(word_pointer, ' ') + 1;
			package->state_status = status_parse(word_pointer, state_words_status);
		} else {
			package->state_want = status_parse("purge", state_words_want);
			package->state_flag = status_parse("ok", state_words_flag);
			package->state_status = status_parse("not-installed", state_words_status);
		}

		free(field);
	}
	return EXIT_SUCCESS;
}

extern void write_package(FILE *out_file, package_t *pkg)
{
	if (pkg->package) {
		fprintf(out_file, "Package: %s\n", pkg->package);
	}
	if ((pkg->state_want != 0) || (pkg->state_flag != 0)|| (pkg->state_status != 0)) {
		fprintf(out_file, "Status: %s %s %s\n", 
			state_words_want[pkg->state_want - 1],
			state_words_flag[pkg->state_flag - 1],
			state_words_status[pkg->state_status - 1]);
	}
	if (pkg->depends) {
		fprintf(out_file, "Depends: %s\n", pkg->depends);
	}
	if (pkg->provides) {
		fprintf(out_file, "Provides: %s\n", pkg->provides);
	}
	if (pkg->priority) {
		fprintf(out_file, "Priority: %s\n", pkg->priority);
	}
	if (pkg->section) {
		fprintf(out_file, "Section: %s\n", pkg->section);
	}
	if (pkg->section) {
		fprintf(out_file, "Installed-Size: %s\n", pkg->installed_size);
	}
	if (pkg->maintainer) {
		fprintf(out_file, "Maintainer: %s\n", pkg->maintainer);
	}
	if (pkg->source) {
		fprintf(out_file, "Source: %s\n", pkg->source);
	}
	if (pkg->version) {
		fprintf(out_file, "Version: %s\n", pkg->version);
	}
	if (pkg->pre_depends) {
		fprintf(out_file, "Pre-depends: %s\n", pkg->pre_depends);
	}
	if (pkg->replaces) {
		fprintf(out_file, "Replaces: %s\n", pkg->replaces);
	}
	if (pkg->recommends) {
		fprintf(out_file, "Recommends: %s\n", pkg->recommends);
	}
	if (pkg->suggests) {
		fprintf(out_file, "Suggests: %s\n", pkg->suggests);
	}
	if (pkg->conflicts) {
		fprintf(out_file, "Conflicts: %s\n", pkg->conflicts);
	}
	if (pkg->conffiles) {
		fprintf(out_file, "Conf-files: %s\n", pkg->conffiles);
	}
	if (pkg->architecture) {
		fprintf(out_file, "Architecture: %s\n", pkg->architecture);
	}
	if (pkg->filename) {
		fprintf(out_file, "Filename: %s\n", pkg->filename);
	}
	if (pkg->md5sum) {
		fprintf(out_file, "MD5sum: %s\n", pkg->md5sum);
	}
	if (pkg->installer_menu_item) {
		fprintf(out_file, "installer-main-menu %d\n", pkg->installer_menu_item);
	}
	if (pkg->description) {
		fprintf(out_file, "Description: %s\n", pkg->description);
	}
	fputc('\n', out_file);
	pkg = pkg->next;
}

static void *status_read(void)
{
	FILE *f;
	void *status = 0;
	package_t *m = 0, *p = 0, *t = 0;
	char *package_control_buffer = NULL;

	if (getenv(udpkg_quiet) == NULL) {
		printf("(Reading database...)\n");
	}

	if ((f = wfopen(statusfile, "r")) == NULL) {
		return(NULL);
	}

	while ( (package_control_buffer = fgets_str(f, "\n\n")) != NULL) {
		m = (package_t *)xcalloc(1, sizeof(package_t));
		fill_package_struct(m, package_control_buffer);
		if (m->package) {
			/*
			 * If there is an item in the tree by this name,
			 * it must be a virtual package; insert real
			 * package in preference.
			 */
			tdelete(m, &status, package_compare);
			tsearch(m, &status, package_compare);
			if (m->provides) {
				/* 
				 * A "Provides" triggers the insertion
				 * of a pseudo package into the status
				 * binary-tree.
				 */
				p = (package_t *)xcalloc(1, sizeof(package_t));
				p->package = xstrdup(m->provides);
				t = *(package_t **)tsearch(p, &status, package_compare);
				if (t != p) {
					free(p->package);
					free(p);
				} else {
					/*
					 * Pseudo package status is the
					 * same as the status of the
					 * package providing it 
					 * FIXME: (not quite right, if 2
					 * packages of different statuses
				 	 * provide it).
				 	 */
					t->state_want = m->state_want;
					t->state_flag = m->state_flag;
					t->state_status = m->state_status;
				}
			}
		}
		else {
			free(m);
		}
	}
	fclose(f);
	return status;
}

static int status_merge(void *status, package_t *pkgs)
{
	FILE *fin, *fout;
	char *line = NULL;
	package_t *pkg = 0, *statpkg = 0;
	package_t locpkg;

	if ((fout = wfopen(new_statusfile, "w")) == NULL) {
		return 0;
	}
	if (getenv(udpkg_quiet) == NULL) {
		printf("(Updating database...)\n");
	}

	/*
	 * Dont use wfopen here, handle errors ourself
	 */
	if ((fin = fopen(statusfile, "r")) != NULL) {
		while (((line = get_line_from_file(fin)) != NULL) && !feof(fin)) { 
			chomp(line); /* trim newline */
			/* If we see a package header, find out if it's a package
			 * that we have processed. if so, we skip that block for
			 * now (write it at the end).
			 *
			 * we also look at packages in the status cache and update
			 * their status fields
	 		 */
			if (strstr(line, "Package: ") == line) {
				for (pkg = pkgs; pkg != 0 && strcmp(line + 9,
						pkg->package) != 0; pkg = pkg->next) ;

				locpkg.package = line + 9;
				statpkg = tfind(&locpkg, &status, package_compare);
	
				/* note: statpkg should be non-zero, unless the status
				 * file was changed while we are processing (no locking
				 * is currently done...
				 */
				if (statpkg != 0) {
					statpkg = *(package_t **)statpkg;
				}
			}
			if (pkg != 0) {
				continue;
			}
			if (strstr(line, "Status: ") == line && statpkg != 0) {
				snprintf(line, sizeof(line), "Status: %s %s %s",
					state_words_want[statpkg->state_want - 1], 
					state_words_flag[statpkg->state_flag - 1], 
					state_words_status[statpkg->state_status - 1]);
			}
			fprintf(fout, "%s\n", line);
		}
		fclose(fin);
	}
	free(line);

	// Print out packages we processed.
	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		write_package(fout, pkg);
	}
	fclose(fout);

	/*
	 * Its ok if renaming statusfile fails becasue it doesnt exist
	 */
	if (rename(statusfile, bak_statusfile) == -1) {
		struct stat stat_buf;	
		if (stat(statusfile, &stat_buf) == 0) {
			error_msg("Couldnt create backup status file");
			return(EXIT_FAILURE);
		}
		error_msg("No status file found, creating new one");
	}

	if (rename(new_statusfile, statusfile) == -1) {
		error_msg("Couldnt create status file");
		return(EXIT_FAILURE);
	}
	return(EXIT_SUCCESS);
}

static int is_file(const char *fn)
{
	struct stat statbuf;

	if (stat(fn, &statbuf) < 0) {
		return 0;
	}
	return S_ISREG(statbuf.st_mode);
}

static int dpkg_doconfigure(package_t *pkg)
{
	int r;
	char postinst[1024];
	char buf[1024];

	DPRINTF("Configuring %s\n", pkg->package);
	pkg->state_status = 0;
	snprintf(postinst, sizeof(postinst), "%s%s.postinst", infodir, pkg->package);

	if (is_file(postinst)) {
		snprintf(buf, sizeof(buf), "%s configure", postinst);
		if ((r = do_system(buf)) != 0) {
			error_msg("postinst exited with status %d\n", r);
			pkg->state_status = state_status_halfconfigured;
			return 1;
		}
	}
	pkg->state_status = state_status_installed;
	
	return 0;
}

static int dpkg_dounpack(package_t *pkg)
{
	FILE *out_stream;
	char *info_prefix;
	int status = TRUE;
	int r = 0;

	DPRINTF("Unpacking %s\n", pkg->package);

	/* extract the data file */
	deb_extract(pkg->filename, stdout, (extract_data_tar_gz | extract_all_to_fs), "/", NULL);

	/* extract the control files */
	info_prefix = (char *) malloc(strlen(pkg->package) + strlen(infodir) + 2 + 5 + 1);
	sprintf(info_prefix, "%s/%s.", infodir, pkg->package);
	deb_extract(pkg->package, stdout, (extract_control_tar_gz | extract_all_to_fs), info_prefix, NULL);

	/* Create the list file */
	strcat(info_prefix, "list");
	out_stream = wfopen(info_prefix, "w");			
	deb_extract(pkg->package, out_stream, (extract_data_tar_gz | extract_list), NULL, NULL);
	fclose(out_stream);

	pkg->state_want = state_want_install;
	pkg->state_flag = state_flag_ok;
 
	if (status == TRUE) {
		pkg->state_status = state_status_unpacked;
	} else {
		pkg->state_status = state_status_halfinstalled;
	}

	return r;
}

/*
 * Extract and parse the control file from control.tar.gz
 */
static int dpkg_read_control(package_t *pkg)
{
	FILE *pkg_file;
	char *control_buffer = NULL;

	if ((pkg_file = wfopen(pkg->filename, "r")) == NULL) {
		return EXIT_FAILURE;
	}
	control_buffer = deb_extract(pkg->filename, stdout, (extract_control_tar_gz | extract_one_to_buffer), NULL, "./control");
	fill_package_struct(pkg, control_buffer);
	return EXIT_SUCCESS;
}

static int dpkg_unpack(package_t *pkgs, void *status)
{
	int r = 0;
	package_t *pkg;

	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		dpkg_read_control(pkg);
		if ((r = dpkg_dounpack(pkg)) != 0 ) {
			break;
		}
	}
	status_merge(status, pkgs);

	return r;
}

static int dpkg_configure(package_t *pkgs, void *status)
{
	int r = 0;
	void *found;
	package_t *pkg;

	for (pkg = pkgs; pkg != 0 && r == 0; pkg = pkg->next) {
		found = tfind(pkg, &status, package_compare);

		if (found == 0) {
			error_msg("Trying to configure %s, but it is not installed", pkg->package);
			r = 1;
		} 
		/* configure the package listed in the status file;
		 * not pkg, as we have info only for the latter
		 */
		else {
			r = dpkg_doconfigure(*(package_t **)found);
		}
	}
	status_merge(status, 0);

	return r;
}

static int dpkg_install(package_t *pkgs, void *status)
{
	package_t *p, *ordered = 0;

	/* Stage 1: parse all the control information */
	for (p = pkgs; p != 0; p = p->next) {
		dpkg_read_control(p);
	}

	/* Stage 2: resolve dependencies */
#ifdef DODEPENDS
	ordered = depends_resolve(pkgs, status);
#else
	ordered = pkgs;
#endif
	
	/* Stage 3: install */
	for (p = ordered; p != 0; p = p->next) {
		p->state_want = state_want_install;

		/* for now the flag is always set to ok... this is probably
		 * not what we want
		 */
		p->state_flag = state_flag_ok;

		DPRINTF("Installing %s\n", p->package);
		if (dpkg_dounpack(p) != 0) {
			perror_msg(p->filename);
		}

		if (dpkg_doconfigure(p) != 0) {
			perror_msg(p->filename);
		}
	}

	if (ordered != 0) {
		status_merge(status, pkgs);
	}

	return 0;
}

/*
 * Not implemented yet
 *
static int dpkg_remove(package_t *pkgs, void *status)
{
	package_t *p;

	for (p = pkgs; p != 0; p = p->next)
	{
	}
	status_merge(status, 0);

	return 0;
}
*/

extern int dpkg_main(int argc, char **argv)
{
	const int arg_install = 1;
	const int arg_unpack = 2;
	const int arg_configure = 4;

	package_t *p, *packages = NULL;
	void *status = NULL;
	int opt = 0;
	int optflag = 0;

	while ((opt = getopt(argc, argv, "iruc")) != -1) {
		switch (opt) {
			case 'i':
				optflag |= arg_install;
				break;
			case 'u':
				optflag |= arg_unpack;
				break;
			case 'c':
				optflag |= arg_configure;
				break;
			default:
				show_usage();
		}
	}

	while (optind < argc) {
		p = (package_t *) xcalloc(1, sizeof(package_t));
		if (optflag & arg_configure) {
			p->package = xstrdup(argv[optind]);
		} else {
			p->filename = xstrdup(argv[optind]);
		}
		p->next = packages;
		packages = p;

		optind++;
	}

	make_directory((char *)infodir, S_IRWXU, FILEUTILS_RECUR);

	status = status_read();

	if (optflag & arg_install) {
		return dpkg_install(packages, status);
	}
	else if (optflag & arg_unpack) {
		return dpkg_unpack(packages, status);
	}
	else if (optflag & arg_configure) {
		return dpkg_configure(packages, status);
	}
	return(EXIT_FAILURE);
}
