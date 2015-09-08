/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

#include "FTPMount.h" /* 03-03-02 rri */
#include "tcp.h"
#include "site.h"
#include "split.h"

boolean collapse(unsigned char *s)
{
	unsigned char *t, *u;

	t = s;
	u = s;

	/* . & .. are illegal */
	if (u[0] == '.') {
		if (u[1] == 0) return false;
		if (u[1] == '/') return false;
		if (u[1] == '.') {
			if (u[2] == 0) return false;
			if (u[2] == '/') return false;
		}
	}

	while (*u) {
		if (t == s && u[0] == '/') {
			// return false; // used to do this, but people didn't want it to error
			u++;  /* ignore it */
			continue;
		}

		if (u[0] == '/' && u[1] == '.') {
			if (u[2] == 0) return false;
			if (u[2] == '/') return false;
			if (u[2] == '.') {
				if (u[3] == 0) return false;
				if (u[3] == '/') return false;
			}
		}

		if (u[0] == '/' && u[1] == '/') {
			while (--t > s) {
				if (*t == '/') break;
			}
			u++;
			if (t == s) u++;
		}
		else {
			*t++ = *u++;
		}
	}

	if (t > s && t[-1] == '/') t[-1] = 0;  /* cull the trailing '/' */

	*t = 0;

	return true;
}

boolean split_data(lock *l, unsigned char *z, split *sd)
{
	unsigned char *s, *t;
	int len1, len2, len3;
	/*
	 * sigh ... all that work to separate the libs to processes ... just
	 * given up and used the global DosBase here
	 */
	struct DevProc *dp;

	truth(z != nil);

	if (!l && z[0] == 0) {

		sd->port = local_port;
		sd->path = nil;

		sd->work = nil;

		return true;
	}

	s = (unsigned char *)allocate(z[0] + 1, V_cstr);
	if (!s) {
		sd->work = nil;
		return false;
	}

	if (z[0])
		memcpy(s, &z[1], z[0]);

	s[z[0]] = 0;

	sd->work = s;

	for (;; s++) {
		if (!*s) {
			z = sd->work;
			break;
		}

		if (*s == ':') {
			s++;
			dp = GetDeviceProc(sd->work, nil);
			if (!dp) {
				deallocate(sd->work, V_cstr);
				sd->work = nil;
				return false;
			}

			l = (lock *)(dp->dvp_Lock << 2);

			FreeDeviceProc(dp);

			z = s;

			break;
		}
	}

	/* have to look at lock to see where we are relative to */

	if (!l || (l->port == local_port && l->rfsl == ftphosts_lock)) {
		s = z;
		goto resolve;
	}

	verify(l, V_lock);

	if (l->port == local_port) {  /* most handlers might allow this, but there's no point */
		show_string("Split Fail B");
		deallocate(sd->work, V_cstr);
		sd->work = nil;
		return false;
	}

	/* construct a full path name so we can collapse it sensibly */

	if (l->fname[0] == 0) {
		len1 = strlen(l->port->mp_Node.ln_Name);
		len2 = strlen(z);

		s = (unsigned char *)allocate(len1 + len2 + 2, V_cstr);
		if (!s) {
			deallocate(sd->work, V_cstr);
			sd->work = nil;
			return false;
		}

		strcpy(s, l->port->mp_Node.ln_Name);

		if (sd->work[0]) {
			s[len1] = '/';
			strcpy(&s[len1 + 1], z);
		}
		else {
			s[len1] = 0;
		}

		deallocate(sd->work, V_cstr);
		sd->work = s;
	}
	else {
		len1 = strlen(l->port->mp_Node.ln_Name);
		len2 = strlen(l->fname);
		len3 = strlen(z);

		s = (unsigned char *)allocate(len1 + len2 + len3 + 3, V_cstr);
		if (!s) {
			deallocate(sd->work, V_cstr);
			sd->work = nil;
			return false;
		}

		strcpy(s, l->port->mp_Node.ln_Name);
		s[len1] = '/';
		strcpy(&s[len1 + 1], l->fname);
		if (sd->work[0]) {
			s[len1 + len2 + 1] = '/';
			strcpy(&s[len1 + len2 + 2], z);
		}
		else {
			s[len1 + len2 + 1] = 0;
		}
		deallocate(sd->work, V_cstr);
		sd->work = s;
	}

resolve:
	if (!collapse(s)) {
		deallocate(sd->work, V_cstr);
		sd->work = nil;
		return false;
	}

	for (t = s; *t; t++) {
		if (*t == '/') {
			*t++ = 0;
			sd->port = get_site(s);
			if (!sd->port) {
				deallocate(sd->work, V_cstr);
				sd->work = nil;
				return false;
			}
			if (t[0])
				sd->path = t;
			else
				sd->path = nil;
			return true;
		}
	}

	if (s[0] == 0) {
		deallocate(sd->work, V_cstr);
		sd->work = nil;
		sd->path = nil;
		sd->port = local_port;
		return true;
	}

	/* ok, we don't have a '/' so check for .info or Unnamed or
	   Disk or Default ... they are "special" */
	if (strcasecmp(s, "Disk") == 0 ||
		strcasecmp(s, "Unnamed", 7) == 0 ||
		strcasecmp(s, ".backdrop") == 0 ||
		strcasecmp(s, "Default") == 0 ||
		strcasecmp(&s[strlen(s) - 5], ".info") == 0) {
		sd->port = local_port;
		sd->path = s;
		return true;
	}

	sd->port = get_site(s);
	deallocate(sd->work, V_cstr);
	sd->work = nil;
	sd->path = nil;

	if (!sd->port) return false;
	return true;
}

void end_split(split *sd)
{
	if (sd->work) {
		deallocate(sd->work, V_cstr);
		sd->work = nil;
	}
}
