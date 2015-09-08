/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

typedef struct {
	magic_verify;

	struct MsgPort *port;
	unsigned char *path;
	unsigned char *work;
} split;

#define V_split 29552

boolean split_data(lock *, unsigned char *, split *);
void end_split(split *);
