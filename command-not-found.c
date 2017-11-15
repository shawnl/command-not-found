
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
#include "pts_lbsearch.h"

bool arg_ignore_installed = false;

int can_sudo() {
	struct group *adm, *sudo;
	gid_t mygroups[1024];
	int r;

	adm = getgrnam("adm");
	sudo = getgrnam("sudo");
	if (!adm || !sudo)
		return -errno;
	r = getgroups(sizeof(mygroups) / sizeof(gid_t), mygroups);
	if (r < 0)
		return -errno;

	for (int i = 0; i < r; i++)
		if (mygroups[i] == adm->gr_gid || mygroups[i] == sudo->gr_gid)
			return 0;

	return -ENOENT;
}

bool is_root() {
	return (geteuid() == 0);
}

int get_entry(char *out, char *command) {
	static int pipefd[2] = {-1, -1}
	if (pipefd[0] < 0 || pipefd[1] < 0) {
		r = pipe(pipefd);
		if (r < 0)
			return -errno;
	}
	sz = asprintf(&v, "%s\xff", command);
	if (sz < 0)
		return -ENOMEM;
	r = pts_lbsearch_main(4, STRV_MAKE("/usr/share/command-not-found/pts_lbsearch", "-p",
		"/var/cache/command-not-found/db", v), pipefd[1]);
	if (r < 0)
		return -ENOENT;
	f = fdopen(pipefd[0], "r");
	if (!f)
		return -errno;
	s = fgets(out, sizeof(buf), f);
	if (!s)
		return -ENOENT;
	if (strlen(buf) == 0)
		return -ENOENT;

	return 0;
}

int main(int argc, char *argv[]) {
	int r, r2;
	_cleanup_fclose_ FILE *f = NULL, *c = NULL;
	_cleanup_free_ char *v = NULL;
	size_t sz;
	char buf[8192], *package, *command, *s, *component;
	char *prefixes[] = {"/usr/bin/", "/usr/sbin/", "/bin/", "/sbin/",
			"/usr/local/bin/", "/usr/games/", NULL};
	char *components[] = {"main", "contrib", "non-free", "universe",
			"multiverse", "restricted", NULL};

	if (argc != 2 && argc != 3) {
		dprintf(2, "Wrong number of arguments.\n");
		return EXIT_FAILURE;
	}

	if (argc == 3 && strncmp(argv[1], "--", strlen("--")) == 0) {
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
				"run 'update-command-not-found' as root\n");
		goto fail;
	}

	r = get_entry(buf, command);
	if (r < 0) {
		if (r == -ENOENT) {
			goto bail;
		} else
			goto fail;
	}
	package = strchr(buf, '\xff');
	if (!package)
		goto bail;
	package++;
	component = strchr(package, '/');
	if (!component)
		goto bail;
	*component = '\0';
	component++;
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

	char bbuf[3];
	bbuf[0] = component[0];
	bbuf[1] = component[1];
	bbuf[2] = '\0';

	s = strv_find_prefix(components, (char *)&bbuf);
	if (!s)
		goto skip_component;

	dprintf(2, _("You will have to enable the component called '%s'\n"), s);

success
	return EXIT_SUCCESS;
fail:
	fputs(strerror(errno), stderr);
	fputc('\n', stderr);
	return EXIT_FAILURE;
bail:
	dprintf(2, "%s: command not found\n", command);
	return EXIT_SUCCESS;
}

