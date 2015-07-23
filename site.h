/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

#define READ_BUFFER_LENGTH 200

typedef struct site_s {
	struct site_s *next;

	magic_verify;

	struct my_lock *lock_list;
	struct my_file_info *file_list;
	struct tcpm *control, *intr, *cfile;
	struct StandardPacket *death_packet;
	struct MsgPort *port, *sync, *rank;
	struct info_header *infos;
	b8 *user, *password;
	b8 *cwd, *root;
	b8 *host;
	b32 cfile_type;
	b32 abort_signals, disconnect_signals;
	struct Window *status_window;
	struct Gadget *abort_gadget, *disconnect_gadget;

	struct IntuitionBase *IBase;
	struct GfxBase *GBase;
	struct Library *GTBase;
#ifdef __amigaos4__
	struct IntuitionIFace * pIIntuition;
	struct GraphicsIFace  * pIGraphics;
	struct GadToolsIFace  * pIGadTools;
#endif

	boolean connected, read_banners, unix_paths, open_status, quick;
	boolean needs_user, needs_password, case_sensitive, all_messages, error_messages;
	b16 port_number;
	boolean comment;
	b16 no_lock_conn_idle;

	b8 site_state;
	b8 read_buffer[READ_BUFFER_LENGTH];

	b8 name[0];
} site;

#define V_site 29545

struct MsgPort *get_site(b8 *s);
void SAVEDS site_handler(void);
void remove_site(site *);
void shutdown_sites(void);
void suspend_sites(void);

void state_change(site *, b16);

#define IDLE_INTERVAL 20   /* 20 second interval */

/* Vielfache von IDLE_INTERVAL */
#define NO_LOCK_NO_CONN_IDLE 1      /* ??, wenn keine Locks und keine Connection */
#define NO_LOCK_CONN_IDLE 6         /* disconnect, wenn keine Locks mehr */
#define LOCK_CONN_IDLE 15           /* ??, wenn Locks vorhanden */

#define SS_DISCONNECTED 1
#define SS_CONNECTING 2
#define SS_IDLE 3
#define SS_DISCONNECTING 4
#define SS_LISTING 5
#define SS_CWD 6
#define SS_OPENING 7
#define SS_CLOSING 8
#define SS_READING 9
#define SS_WRITING 10
#define SS_LOGIN 11
#define SS_ABORTING 12
#define SS_DELETING 13
#define SS_MAKEDIR 14
#define SS_RENAMING 15

