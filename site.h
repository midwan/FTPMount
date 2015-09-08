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
	unsigned char *user, *password;
	unsigned char *cwd, *root;
	unsigned char *host;
	unsigned long cfile_type;
	unsigned long abort_signals, disconnect_signals;
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
	unsigned short port_number;
	boolean comment;
	unsigned short no_lock_conn_idle;

	unsigned char site_state;
	unsigned char read_buffer[READ_BUFFER_LENGTH];

	unsigned char name[1];
} site;

#define V_site 29545

struct MsgPort *get_site(unsigned char *s);
void SAVEDS site_handler(void);
void remove_site(site *);
void shutdown_sites(void);
void suspend_sites(void);

void state_change(site *, unsigned short);

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

