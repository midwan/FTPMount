/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */


typedef union {
	struct {
		b8 a;
		b8 b;
		b8 c;
		b8 d;
	} dot;
	b32 l;
} ipnum;

typedef union {
	struct {
		b8 a;
		b8 b;
	} dot;
	b16 w;
} portnum;

/* something like an iorequest ... probably a real exec device
   implementation would be good, but for now just DIY */

typedef struct tcpm {
	struct   Message header;

	magic_verify;

	b32   command;
	void  *ident;     /* stores pointer to context information */

	ipnum address;
	portnum  port;

	void  *data;
	struct   tcpm  *interrupt;
	b32   length;

	b32   result;
	b32   error;
	b32   flags;
	b32   userid;
	void  *userdata;
} tcpmessage;

typedef struct {
	magic_verify;

	sb32  fd;
	void  *connecting_port;
	boolean  eof;
} tcpident;

#define V_tcpmessage 4159
#define  V_tcpident 7592

/* commands */
#define TCP_NOOP  0
#define  TCP_CONNECT 1
#define  TCP_LISTEN  2
#define TCP_READ  3
#define TCP_WRITE 4
#define TCP_CLOSE 5
#define TCP_INTERRUPT   6

/* hmmm, keep these??? */
#define TCP_NEWMESSAGE  7
#define TCP_DISPOSE  8

/* internal */
#define TCP_CONNECTED   9  /* CONNECT reply */
#define TCP_ACCEPTED 10 /* Async ACCEPT */
#define TCP_WRITTEN  11 /* WRITE reply (not used) */
#define TCP_GOT      12 /* READ reply (not used) */
#define TCP_STARTUP  13 /* STARTUP was successful/unsuccessful */

#define TCP_DIE      14

/* info */
#define TCP_PEERNAME 15
#define TCP_SERVICE  16

/* errors */
#define  NO_ERROR    0
#define ERROR_OOM    1
#define ERROR_UNKNOWN_COMMAND 2
#define ERROR_NO_CONNECTION   3
#define ERROR_LOST_CONNECTION 4
#define ERROR_ALREADY_CONNECTED  5
#define ERROR_ACCESS_DENIED   6
#define ERROR_EOF    7
#define ERROR_INTERRUPTED  8
#define ERROR_UNKNOWN_HOST 9
#define ERROR_CANT_CONNECT 10
#define ERROR_UNREACHABLE  11
#define ERROR_CONNECT_REFUSED 12

/* flags */
#define FLAG_READLINE      1

#ifndef	__MORPHOS__
void SAVEDS ASM tcp_handler(REG(a0, b8 *parent_port));
#else
void tcp_handler(b8 *parent_port);
#endif
void unique_name(void *, b8 *, b8 *);

