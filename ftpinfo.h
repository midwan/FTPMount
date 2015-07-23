/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

struct info_header {
   struct info_header *next;
   struct info_header **prev;

   magic_verify;

   b32   unique;

   struct   my_info *infos;
   struct  my_info **last_info_p;

   boolean  case_sensitive;
   b8 name[0];
};

typedef struct my_info {
   struct   my_info *next;

   magic_verify;

   b32   size;
   b32   blocks;
   b32   flags;
   struct   DateStamp modified;
   b8 name[0];    // speichert "Name\0Kommentar"
} ftpinfo;

#define MYFLAG_LINK     0x100000 /* for indicating links with our normal protection bits */
#define MYFLAG_DIR      0x080000 /* for indicating directories with our normal protection bits */
#define MYFLAG_DELETED  0x040000

#define V_ftpinfo 6033
#define V_info_header   9412

void add_ftpinfo(struct info_header *ih, b8 *name, b8 *comment, struct DateStamp ds, b32 size, b32 blocks, b32 flags);
void free_info_header(struct info_header *ih);
struct info_header *new_info_header(site *sp, b8 *name);
struct info_header *find_info_header(site *sp, b8 *name);
ftpinfo *find_info(struct info_header *ih, b8 *name);

