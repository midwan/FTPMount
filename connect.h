/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

void init_connect(site *);
void disconnect(site *);
boolean change_dir(site *sp, unsigned char *);
unsigned char *cd_parent(site *sp, unsigned char *);
boolean get_list(site *sp, struct info_header *ih);

unsigned long read_file(site *sp, unsigned char *, unsigned long *);
unsigned long write_file(site *sp, unsigned char *, unsigned long *);

unsigned long open_file(site *sp, unsigned char *, boolean, unsigned char *);
void close_file(site *sp, boolean normal_close);

unsigned long delete_file(site *sp, unsigned char *);
unsigned long delete_directory(site *sp, unsigned char *);
unsigned long make_directory(site *sp, unsigned char *);
unsigned long rename_object(site *sp, unsigned char *, unsigned char *);

void interrupt_message(site *sp, tcpmessage *tm);

#define MORE_LINES 10

