/* Copyright (C) 2017-2019  Shawn Landden <shawn@git.icu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
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
#include <assert.h>
#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#define _ gettext

#include "update-command-not-found.h"
#include "util.h"

static int collect_contents(FILE *db) {
	Cleanup(fclosep) FILE *contents_cat = NULL, *commands_cat = NULL, *apt = NULL, *snap = NULL;
	char buf[8192], *bin, *pkg, cmn, *t, *t2, *t3;
	Cleanup(freep) char *indextargets = NULL, *contents = NULL, *commands = NULL,
		*popens = NULL, *snap_db = NULL, *popens2 = NULL;
	int r;

	if (!(apt = popen("apt-get indextargets", "r")))
		return -ENOENT;

	r = read_full_stream(apt, &indextargets, NULL);
	if (r < 0)
		return r;

	t = indextargets;
	contents = realloc(contents, 1);
	*contents = '\0';
	commands = realloc(commands, 1);
	*commands = '\0';
	while ((t = strstr(t, "\n\n"))) {
		bool is_commands = false;
		t += 2;
		t = strchrnul(t, '\n'); t++;/*MetaKey:*/
		if (strncmp(t, "ShortDesc: Contents-", strlen("ShortDesc: Contents-")) != 0) {
			if (strncmp(t, "ShortDesc: Commands-", strlen("ShortDesc: Commands-")) != 0)
				continue;
			is_commands = true;
		}
		t = strchrnul(t, '\n'); t++;/*ShortDesc::*/
		t = strchrnul(t, '\n'); t++;/*Description:*/
		t = strchrnul(t, '\n'); t++;/*URI:*/
		if (strncmp(t, "Filename: ", strlen("Filename: ")) != 0)
			continue;
		t += strlen("Filename: ");
		t2 = strchrnul(t, '\n');
		*t2 = '\0';
		if (is_commands) {
			// These fds are leaked, but this this process is short lived, so it doesn't matter.
			int memfd = -1;
			char memfd_buf[4096];
			char memfd_name[1024];

			if (strspn(t, " \n"))
				return -EINVAL;
			memfd = memfd_create(t, 0);
			if (memfd < 0)
				return -errno;
			// We can safely ignore truncation here.
			r = snprintf((char *)&memfd_buf, sizeof(memfd_buf), "Section: %s\n", t);
			if (r < 0)
				return -errno;
			r = write(memfd, &memfd_buf, strlen((char *)&memfd_buf));
			if (r < 0)
				return -errno;

			// This is a hack to store the Section that the files don't have, for use below.
			r = snprintf((char *)&memfd_name, sizeof(memfd_name), "/proc/self/fd/%u", memfd);
			if (r < 0)
				return -errno;
			if (r > sizeof(memfd_name))
				return -E2BIG;
			commands = realloc(commands, strlen(commands) + 1 + strlen(t) + 1 + strlen((char *)&memfd_name) + 1);
			strcat(commands, " ");
			strcat(commands, t);
			strcat(commands, " ");
			strcat(commands, (char *)&memfd_name);
		} else {
			contents = realloc(contents, strlen(contents) + strlen(t) + 1 + 1);
			strcat(contents, " ");
			strcat(contents, t);
		}
		*t2 = '\n';
	};

	r = access("/usr/lib/apt/apt-helper", X_OK);
	if (r < 0)
		return -errno;
	if (strlen(contents) == 0)
		printf(_("Warning: Full contents of packages not available.%s\n"),
			strlen(commands) ? _(" You have Commands files, however, so this is not a problem.") : "");

	if (strlen(commands) == 0)
		puts(_("Warning: No commands listings downloaded via apt."));

	if (access("/usr/bin/snap", X_OK))
		snap = popen("/usr/bin.snap advise-snap --dump-db", "r");
	else
		errno = ENOENT;
	if (!snap)
		printf(_("Warning: not including snaps: %m\n"));

	if (strlen(commands) == 0 && strlen(contents) == 0 && !snap) {
		puts(_("No data! If 'apt update' does not work, install 'apt-file' and then run 'apt update'."));
		return -ENOENT;
	}

	r = asprintf(&popens, "/usr/lib/apt/apt-helper cat-file %s", contents);
	if (r < 0)
		return -errno;

	r = asprintf(&popens2, "/usr/lib/apt/apt-helper cat-file %s", commands);
	if (r < 0)
		return -errno;

	if (!((contents_cat = popen(popens, "r")) || (commands_cat = popen(popens2, "r"))))
		return -ENOENT;

	int round = 0;
	while (true) {
		struct binary node;
		bool got_component = false;
		buf[0] = '\0';

		if (round == 0 && contents_cat && fgets((char *)&buf, sizeof(buf), contents_cat)) {
			if (!memcmp("usr/bin/", &buf, strlen("usr/bin/")) ||
			    !memcmp("usr/sbin/", &buf, strlen("usr/sbin/")) ||
			    !memcmp("bin/", &buf, strlen("bin/")) ||
			    !memcmp("sbin/", &buf, strlen("sbin/")) ||
			    !memcmp("usr/games/", &buf, strlen("usr/games/")))
				/*go below*/;
			else
				continue;

			t = &buf[strcspn((char *)&buf, " \t")];
			if (!t)
				return -EINVAL;
			*t++ = '\0';
			t2 = strrchr((char *)&buf, '/');
			if (!t2)
				t2 = (char *)&buf - 1;
			bin = t2 + 1;

			/*get package name*/
			t3 = strrchr(t + 1, '/');
			if (!t3)
				t3 = t + strspn(t, " \t");
			pkg = t3 + 1;
			*(t3 + strlen(t3) - 1) = '\0'; /*kill the newline*/

			/* get component (if it doesn't exist, package is in main) */
			if ((t2 = strchr(t + 1, ','))) {
				t = t2 + 1;
				t2 = strchr(t2 + 1, '/');
			} else
				t2 = strchr(t + 1, '/');
			if (t2 && strchr(t2 + 1, '/')) {
				t2[0] = '\0';
				cmn = *(t + strspn(t, " \t"));
				got_component = true;
			}
		} else if (round == 1 && snap && fgets((char *)&buf, sizeof(buf), snap)) {
			/* We modify buf *in place* */
			bin = buf;
			pkg = strchr(buf, ' ');
			if (!pkg)
				return -EINVAL;
			pkg = '\0';
			pkg++;
			t = strchr(pkg, ' ');
			if (!t)
				return -EINVAL;
			cmn = 's';
		} else if (round == 2 && commands_cat) {
			do {
				size_t buflen = strlen(buf);
				char *c = buf + buflen, *t, *t2;
				if (buflen == 0) {
					if (!fgets((char *)&buf + buflen, sizeof(buf) - buflen, commands_cat))
						break;
					if (memcmp(c, "name: ", strlen("name: ")) == 0) {
						node.pkg = c;
						continue;
					} else if (node.pkg && memcmp(c, "commands: ", strlen("commands: ")) == 0) {
						c += strlen("commands: ");
						break;
					} else
						continue;
					break;
				}
				t = strchr(c, ',');
				if (!t) {
					t = strchr(c, '\n');
					if (!t)
						return -EINVAL;
					buf[0] = '\0';
				}
				*t = '\0';
			} while (false);
			break;
		} else if (round > 2)
			break;
		else {
			round++;
			continue;
		}

		node.bin = bin;
		node.pkg = pkg;

		if (got_component) {
			node.cmn = cmn;
		} else
			node.cmn = 'm';

		if (node.cmn == 'm')
			r = fprintf(db, "%s\xff%s\n", node.bin, node.pkg);
		else
			r = fprintf(db, "%s\xff%s/%c\n", node.bin, node.pkg, node.cmn);
		if (r < 0)
			return -errno;
	};

	return 0;
}

int main(int argc, char *argv[]) {
	int r;
	Cleanup(fclosep) FILE *db = NULL;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	db = fopen("/var/cache/command-not-found/db-unsorted", "w+");
	if (!db) {
		r = -errno;
		goto fail;
	}

	if ((r = collect_contents(db)) < 0)
		goto fail;

	FILE *foo = popen("LC_ALL=C sort -u /var/cache/command-not-found/db-unsorted > /var/cache/command-not-found/db-sorted;" \
			  "mv /var/cache/command-not-found/db-sorted /var/cache/command-not-found/db;"\
			  "rm /var/cache/command-not-found/db-unsorted;", "r");
	if (!foo) {
		r = -errno;
		goto fail;
	}

	return EXIT_SUCCESS;
fail:
	dprintf(2, "%s", strerror(-r));
	dprintf(2, "\n");
	return EXIT_FAILURE;
}
