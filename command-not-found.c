/* command-not-found, finds programs
 * Copyright (C) 2017  Shawn Landden <slandden@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <grp.h>
#include <libintl.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#define _ gettext

#include "command-not-found.h"

bool arg_ignore_installed = false;

int can_sudo() {
	struct group *grp;
	gid_t mygroups[1024];
	int r;

	grp = getgrnam("adm");
	r = getgroups(sizeof(mygroups) / sizeof(gid_t), mygroups);
	if (r < 0)
		return -errno;

	for (int i = 0; i < r; i++)
		if (mygroups[i] == grp->gr_gid)
			return 0;
	return -ENOENT;
}

bool is_root() {
	return (geteuid() == 0);
}

int main(int argc, char *argv[]) {
	int r, r2;
	FILE *f = NULL;
	_cleanup_free_ char *v = NULL;
	size_t sz;
	char buf[8192], *package, *command, *s;
	char *prefixes[] = {"/usr/bin/", "/usr/sbin/", "/bin/", "/sbin/",
			"/usr/local/bin/", "/usr/games/", NULL};

	if (strncmp(argv[1], "--", strlen("--")) == 0) {
		if (strcmp(argv[1], "--ignore-installed") == 0)
			arg_ignore_installed = true;
		command = argv[2];
	} else
		command = argv[1];

	STRV_FOREACH(s, *prefixes) {
		_cleanup_free_ char *w = NULL;
		char *path;

		r = asprintf(&w, "%s%s", s, command);
		if (r < 0)
			goto fail;
		r = access(w, X_OK);
		if (r != 0)
			continue;
		path = getenv("PATH");
		do {
			if (*path == '\0')
				break;
			r = strncmp(path, s, strlen(s));
			if (r != 0)
				continue;
			dprintf(2, _("The command could not be "\
"located because '%s' is not included in the PATH environment variable.\n"), s);
			if (strcmp(s + strlen(s) - 5, "sbin/") == 0)
				dprintf(2, _("This is most likely caused by the"\
" lack of administrative priviledges associated with your user account.\n"));
			return EXIT_SUCCESS;
		} while ((path = strchrnul(path, ':') + 1));

		if (arg_ignore_installed)
			break;

		dprintf(2, _("Command '%s' is available in '%s'\n"), command,
			w);
		return EXIT_SUCCESS;
	}

	r = access("/usr/bin/apt", X_OK);
	r2= access("/usr/bin/aptitude", X_OK);
	if (r != 0 && r2 != 0)
		goto bail;

	r = access("/var/cache/command-not-found/db", R_OK);
	if (r != 0) {
		if (errno == ENOENT)
			dprintf(2, "/var/cache/command-not-found/db not found."
				"run update-command-not-found as root\n");
		goto fail;
	}
	sz = asprintf(&v, "/usr/share/command-not-found/pts_lbsearch -p "\
			"/var/cache/command-not-found/db "\
			"%s\0", command);
	if (sz < 0)
		goto fail;
	f = popen(v, "r");
	if (!f)
		goto fail;
	fgets((char *)&buf, sizeof(buf), f);
	r = pclose(f);
	if (r != 0)
		goto bail;
	if (strlen(buf) == 0)
		goto bail;
	package = &buf[strlen(buf) + 1];
	dprintf(2, _("The program '%s' is currently not installed. "), command);
	if (is_root()) {
		dprintf(2, _("You can install it by typing:\n"));
		dprintf(2, _("apt install %s\n"), package);
	} else if (can_sudo() == 0) {
		dprintf(2, _("You can install it by typing:\n"));
		dprintf(2, _("sudo apt install %s\n"), package);
	} else
		dprintf(2, _("To run '%s' please ask your "
			"administrator to install the package '%s'\n"),
			command, package);

	return EXIT_SUCCESS;
fail:
	fputs(strerror(errno), stderr);
	return EXIT_FAILURE;
bail:
	dprintf(2, "%s: command not found\n", command);
	return EXIT_SUCCESS;
}

