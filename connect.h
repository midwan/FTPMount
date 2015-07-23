/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

void init_connect(site *);
void disconnect(site *);
boolean change_dir(site *sp, b8 *);
b8 *cd_parent(site *sp, b8 *);
boolean get_list(site *sp, struct info_header *ih);

b32 read_file(site *sp, b8 *, b32 *);
b32 write_file(site *sp, b8 *, b32 *);

b32 open_file(site *sp, b8 *, boolean, b8 *);
void close_file(site *sp, boolean normal_close);

b32 delete_file(site *sp, b8 *);
b32 delete_directory(site *sp, b8 *);
b32 make_directory(site *sp, b8 *);
b32 rename_object(site *sp, b8 *, b8 *);

void interrupt_message(site *sp, tcpmessage *tm);

#define MORE_LINES 10

