/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

struct info_header
{
	struct info_header* next;
	struct info_header** prev;

	magic_verify;

	unsigned long unique;

	struct my_info* infos;
	struct my_info** last_info_p;

	boolean case_sensitive;
	unsigned char name[1];
};

typedef struct my_info
{
	struct my_info* next;

	magic_verify;

	unsigned long size;
	unsigned long blocks;
	unsigned long flags;
	struct DateStamp modified;
	unsigned char name[1]; // speichert "Name\0Kommentar"
} ftpinfo;

#define MYFLAG_LINK     0x100000 /* for indicating links with our normal protection bits */
#define MYFLAG_DIR      0x080000 /* for indicating directories with our normal protection bits */
#define MYFLAG_DELETED  0x040000

#define V_ftpinfo 6033
#define V_info_header   9412

void add_ftpinfo(struct info_header* ih, unsigned char* name, unsigned char* comment, struct DateStamp ds, unsigned long size, unsigned long blocks, unsigned long flags);
void free_info_header(struct info_header* ih);
struct info_header* new_info_header(site* sp, unsigned char* name);
struct info_header* find_info_header(site* sp, unsigned char* name);
ftpinfo* find_info(struct info_header* ih, unsigned char* name);
