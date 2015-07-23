/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 * AmigaOS4 port (c) 2005-2006 Alexandre BALABAN (alexandre (at) free (dot) fr)
 */

#include "FTPMount.h" /* 03-03-02 rri */
#include "tcp.h"
#include "site.h"
#include "split.h"
#include "ftpinfo.h"
#include "connect.h"
#include "request.h"
#include "strings.h"


struct MsgPort *get_site(b8 *s)
	// ftp-site mit Namen s zurückgeben; entweder schon aktive nehmen
	// oder neue erstellen (inkl. Icon auslesen, Process erstellen, etc.)
{
	site *sp;
	struct Process *child;
	struct StandardPacket *std_pkt;
	struct DiskObject *dobj;
	BPTR ocd;
	b8 *tmp;


	// falls die site noch im RAM: ist, diese nehmen und zurückgeben
	sp = sites;
	while (sp)
	{
		if (stricmp(sp->name, s) == 0)
			return sp->port;
		sp = sp->next;
	} // while


	// sonst neue erstellen
	sp = (site *)allocate(sizeof(*sp) + strlen(s) + 1, V_site);
	if (!sp)
		return nil;

	ensure(sp, V_site);


	/* first sort out default login information */
	sp->needs_user = false;
	sp->needs_password = false;

	sp->root = nil;
	sp->user = nil;
	sp->password = nil;
	sp->host = nil;

	sp->open_status = false;
	sp->quick = true;
	sp->case_sensitive = false;
	sp->all_messages = true;
	sp->error_messages = true;

	// erstmal auf 0 setzen als Zeichen, daß nichts angegeben ist
	sp->port_number = 0;
	//alt: sp->port_number = ftp_port_number;


	// gucke, ob "," vorkommt, d. h. eine port-Nummer angegeben ist
	tmp = s;
	while (*tmp && (*tmp != ','))
		tmp++;

	if ((*tmp == ',') && isdigit(tmp[1]))
	{
		sp->port_number = atoi(tmp + 1);
		*tmp = 0;               // Hostname geht nur bis Zeichen vor ','
	} // if (',')


	// gucke ob "@" vorkommt, d. h. ein user-Name angegeben ist bzw.
	// immer eine Anfrage danach gestellt werden soll
	tmp = s;
	while (*tmp && (*tmp != '@'))
		tmp++;

	if (*tmp == '@')
	{
		sp->host = (b8 *)allocate(strlen(tmp + 1) + 1, V_cstr);
		if (sp->host)
			strcpy(sp->host, tmp + 1);

		// erstmal kein user-Name angegeben, nur Angabe, daß nach einem
		// gefragt werden soll
		sp->needs_user = true;
		sp->needs_password = true;

		// ist ein user-Name angegeben?
		if (tmp != s)
		{
			sp->user = (b8 *)allocate(tmp - s + 1, V_cstr);
			if (sp->user)
			{
				strncpy(sp->user, s, tmp - s);
				sp->user[tmp - s] = 0;
				sp->needs_user = false;
			} // if
		} // if
	}
	else
	{
		sp->host = (b8 *)allocate(strlen(s) + 1, V_cstr);
		if (sp->host)
			strcpy(sp->host, s);
	} // else


	/* now try to get info from icon */
	if (IconBase)
	{
		ocd = CurrentDir(ftphosts_lock);
		dobj = GetDiskObject(s);
		if (!dobj)
			dobj = GetDiskObject(strings[MSG_DEFAULT]);
		CurrentDir(ocd);
	}
	else
	{
		dobj = nil;
	}


	// Port-Nummer-String wieder an Namen anhängen (aus 0-Byte wieder ',' machen)
	if (sp->port_number != 0)
		s[strlen(s)] = ',';


	if (dobj)
	{
		/*
		 * HOST overrides the "title", whereas USER doesn't ...
		 * is this inconsistent or is it valid?
		 */
		if (!sp->user)
		{
			tmp = FindToolType(dobj->do_ToolTypes, "USER");
			if (tmp)
			{
				sp->user = (b8 *)allocate(strlen(tmp) + 1, V_cstr);
				if (sp->user)
				{
					strcpy(sp->user, tmp);
					sp->needs_user = false;
					sp->needs_password = true;
				}
			}
		} // if


		// verschlüsseltes Passwort hat Priorität über normalem Passwort
		tmp = FindToolType(dobj->do_ToolTypes, "PASSWORDCRYPT");
		if (tmp)
		{
			// Passwort dekodieren
			b8 *decrypt_password(b8 *s);
			sp->password = decrypt_password(tmp);
			if (sp->password)
			{
				sp->needs_password = false;
			} // if
		}
		else
		{
			tmp = FindToolType(dobj->do_ToolTypes, "PASSWORD");
			if (tmp)
			{
				sp->password = (b8 *)allocate(strlen(tmp) + 1, V_cstr);
				if (sp->password)
				{
					strcpy(sp->password, tmp);
					sp->needs_password = false;
				}
			} // if
		} // else

		tmp = FindToolType(dobj->do_ToolTypes, "STATUS");
		if (tmp)
		{
			if (stricmp(tmp, strings[MSG_OFF]) != 0 &&
				stricmp(tmp, strings[MSG_FALSE]) != 0)
			{
				sp->open_status = true;
			}
		}

		tmp = FindToolType(dobj->do_ToolTypes, "QUICK");
		if (tmp)
		{
			if (stricmp(tmp, strings[MSG_OFF]) == 0 ||
				stricmp(tmp, strings[MSG_FALSE]) == 0)
			{
				sp->quick = false;
			}
			else {
				sp->quick = true;
			}
		}

		tmp = FindToolType(dobj->do_ToolTypes, "SLOW");
		if (tmp)
		{
			if (stricmp(tmp, strings[MSG_OFF]) != 0 &&
				stricmp(tmp, strings[MSG_FALSE]) != 0)
			{
				sp->quick = false;
			}
			else {
				sp->quick = true;
			}
		}

		tmp = FindToolType(dobj->do_ToolTypes, "CASE");
		if (tmp) {
			if (stricmp(tmp, strings[MSG_OFF]) != 0 &&
				stricmp(tmp, strings[MSG_FALSE]) != 0)
			{
				sp->case_sensitive = true;
			}
		}

		tmp = FindToolType(dobj->do_ToolTypes, "HOST");
		if (tmp)
		{
			if (sp->host)
				deallocate(sp->host, V_cstr);
			sp->host = allocate(strlen(tmp) + 1, V_cstr);
			if (sp->host)
			{
				strcpy(sp->host, tmp);
			}
		} // if

		tmp = FindToolType(dobj->do_ToolTypes, "ROOT");
		if (tmp)
		{
			sp->root = allocate(strlen(tmp) + 1, V_cstr);
			if (sp->root)
			{
				strcpy(sp->root, tmp);
			}
		}

		tmp = FindToolType(dobj->do_ToolTypes, "MESSAGES");
		if (tmp)
		{
			if (stricmp(tmp, "ALL") == 0)
			{
				sp->error_messages = true;
				sp->all_messages = true;
			}
			else if (stricmp(tmp, "ERROR") == 0)
			{
				sp->all_messages = false;
				sp->error_messages = true;
			}
			else if (stricmp(tmp, "NONE") == 0)
			{
				sp->all_messages = false;
				sp->error_messages = false;
			}
		}

		// wenn keine port-Nummer im Hostnamen angegeben ist, Tooltype
		// heranziehen
		if (sp->port_number == 0)
		{
			tmp = FindToolType(dobj->do_ToolTypes, "PORT");
			if (tmp)
			{
				sp->port_number = atoi(tmp);
			}
		} // if



		sp->comment = false;
		tmp = FindToolType(dobj->do_ToolTypes, "COMMENT");
		if (tmp)
		{
			sp->comment = (stricmp(tmp, strings[MSG_OFF]) != 0 &&
				stricmp(tmp, strings[MSG_FALSE]) != 0);
		}

		tmp = FindToolType(dobj->do_ToolTypes, "TIMEOUT");
		if (tmp)
		{
			sp->no_lock_conn_idle = atoi(tmp) / IDLE_INTERVAL;
		}
		else
			sp->no_lock_conn_idle = NO_LOCK_CONN_IDLE;   // default values

		FreeDiskObject(dobj);
	} // if (dobj)


	// wenn keine port-Nummer angegeben wurde, auf Defaultwert setzen
	if (sp->port_number == 0)
		sp->port_number = ftp_port_number;


	if (!sp->host)
	{
		if (sp->user)
			deallocate(sp->user, V_cstr);
		if (sp->password)
			deallocate(sp->password, V_cstr);
		if (sp->root)
			deallocate(sp->root, V_cstr);

		deallocate(sp, V_site);

		return nil;
	} // if

	std_pkt = (struct StandardPacket *)allocate(sizeof(*std_pkt), V_StandardPacket);
	if (!std_pkt)
	{
		deallocate(sp->host, V_cstr);

		if (sp->user) deallocate(sp->user, V_cstr);
		if (sp->password) deallocate(sp->password, V_cstr);
		if (sp->root) deallocate(sp->root, V_cstr);

		deallocate(sp, V_site);

		return nil;
	} // if

	std_pkt->sp_Msg.mn_Node.ln_Name = (void *)&std_pkt->sp_Pkt;
	std_pkt->sp_Pkt.dp_Link = &std_pkt->sp_Msg;

	std_pkt->sp_Pkt.dp_Type = ACTION_DIE;  /* ignored when used at startup */
	std_pkt->sp_Pkt.dp_Arg1 = (b32)sp;
	std_pkt->sp_Pkt.dp_Port = startup_sync;

	strcpy(sp->name, s);

	sp->lock_list = nil;
	sp->file_list = nil;

	sp->infos = nil;

	sp->control = nil;
	sp->intr = nil;

	sp->cwd = nil;

	sp->connected = false;
	sp->read_banners = false;
	sp->unix_paths = true;

	sp->cfile = nil;
	sp->cfile_type = 0;

	sp->abort_signals = sp->disconnect_signals = 0;
	sp->site_state = SS_DISCONNECTED;

	sp->status_window = nil;

#ifndef	__MORPHOS__
	child = CreateNewProcTags(
		NP_Entry, site_handler,
		NP_Name, sp->name,
		NP_StackSize, 6000,
		NP_Priority, 0,
		TAG_END, 0
		);
#else
	child = CreateNewProcTags(
		NP_CodeType, CODETYPE_PPC,
		NP_Entry, (ULONG)site_handler,
		NP_Name, (ULONG)sp->name,
		NP_Priority, 0,
		TAG_END
		);
#endif

	if (!child)
	{
		deallocate(sp->host, V_cstr);

		if (sp->user)
			deallocate(sp->user, V_cstr);
		if (sp->password)
			deallocate(sp->password, V_cstr);
		if (sp->root)
			deallocate(sp->root, V_cstr);

		deallocate(std_pkt, V_StandardPacket);
		deallocate(sp, V_site);
		return nil;
	}

	sp->port = &child->pr_MsgPort;

	PutMsg(sp->port, &std_pkt->sp_Msg);
	WaitPort(startup_sync); GetMsg(startup_sync);

	if (std_pkt->sp_Pkt.dp_Res1)
	{
		sp->death_packet = std_pkt;

		sp->next = sites;
		sites = sp;

		return sp->port;
	}
	else
	{
		deallocate(sp->host, V_cstr);

		if (sp->user)
			deallocate(sp->user, V_cstr);
		if (sp->password)
			deallocate(sp->password, V_cstr);
		if (sp->root)
			deallocate(sp->root, V_cstr);

		deallocate(std_pkt, V_StandardPacket);
		deallocate(sp, V_site);

		return nil;
	}
}



void remove_site(site *sp)
{
	site **sps;

	verify(sp, V_site);

	sps = &sites;
	while (*sps && *sps != sp)
	{
		sps = &(*sps)->next;
	}

	if (*sps)
	{
		*sps = sp->next;

		if (sp->host) deallocate(sp->host, V_cstr);
		if (sp->root) deallocate(sp->root, V_cstr);

		deallocate(sp->death_packet, V_StandardPacket);
		deallocate(sp, V_site);
	}

	return;
}



void shutdown_sites(void)
{
	site *sp, *spn;
	struct DosPacket *dp;
	lock *l;

	sp = sites;
	while (sp)
	{
		verify(sp, V_site);

		spn = sp->next;

		dp = &sp->death_packet->sp_Pkt;
		dp->dp_Type = ACTION_DIE;
		dp->dp_Port = startup_sync;

		PutMsg(sp->port, dp->dp_Link);
		WaitPort(startup_sync); GetMsg(startup_sync);

		l = (lock *)dp->dp_Res2;
		while (l)
		{
			adopt(l, V_lock);
			l = l->next;
		}

		if (sp->host) deallocate(sp->host, V_cstr);
		if (sp->root) deallocate(sp->root, V_cstr);

		deallocate(sp->death_packet, V_StandardPacket);
		deallocate(sp, V_site);
		sp = spn;
	}

	sites = nil;
}



void suspend_sites(void)
{
	site *sp;
	struct DosPacket *dp;

	sp = sites;
	while (sp)
	{
		verify(sp, V_site);

		dp = &sp->death_packet->sp_Pkt;
		dp->dp_Type = action_SUSPEND;
		dp->dp_Port = startup_sync;

		PutMsg(sp->port, dp->dp_Link);
		WaitPort(startup_sync); GetMsg(startup_sync);

		sp = sp->next;
	}
}



struct info_header *get_dir(site *sp, b8 *name)
	// übergebenes Verzeichnis auslesen
{
	struct info_header *ih;

	verify(sp, V_site);
	truth(name != nil);

	// erstmal im Cache gucken
	ih = find_info_header(sp, name);
	if (ih)
		return ih;

	/* oh kay */

	if (sp->cfile)
	{
		verify(sp->file_list, V_file_info);

		if (sp->file_list->closed)
		{
			close_file(sp, true);
		}
		else {
			return nil;
		}
	}

	if (!change_dir(sp, name))
		return nil;

	ih = new_info_header(sp, name);
	if (!ih)
		return nil; /* just out of memory ... but wth */

	get_list(sp, ih);

	return ih;
}



ftpinfo *get_info(site *ftp_site, b8 *name)
{
	b8 *s;
	struct info_header *ih;

	verify(ftp_site, V_site);
	truth(name != nil);

	/* get parent dir name */

	s = name + strlen(name) - 1;
	while (s >= name && *s != '/')
		s--;

	if (s >= name)
	{
		*s = 0;
		ih = get_dir(ftp_site, name);
		*s++ = '/';
	}
	else
	{
		s = name;
		ih = get_dir(ftp_site, "");
	}

	if (!ih)
		return nil;

	return find_info(ih, s);
}



void flush_info(site *ftp_site, b8 *name)
{
	b8 *s;
	struct info_header *ih;

	verify(ftp_site, V_site);
	truth(name != nil);

	/* get parent dir name */

	s = name + strlen(name) - 1;
	while (s >= name && *s != '/')
		s--;

	if (s >= name)
	{
		*s = 0;
		ih = find_info_header(ftp_site, name);
		*s++ = '/';
	}
	else {
		s = name;
		ih = find_info_header(ftp_site, "");
	}

	if (ih)
		free_info_header(ih);

	return;
}



status_message *get_sm(site *sp)
// "sm" = "status message"
{
	status_message *sm;

	verify(sp, V_site);

	// entweder vorhandene Status Message nehmen
	if (sm = (status_message *)GetMsg(sp->rank))
		return sm;

	// oder neue allokieren und init.
	sm = (status_message *)allocate(sizeof(*sm), V_status_message);
	if (!sm)
		return nil;

	ensure(sm, V_status_message);

	sm->header.mn_ReplyPort = sp->rank;
	sm->header.mn_Length = sizeof(*sm);
	sm->header.mn_Node.ln_Type = NT_MESSAGE;
	sm->header.mn_Node.ln_Name = "site status message";
	sm->header.mn_Node.ln_Pri = 0;

	sm->this_site = sp;

	return sm;
}



void state_change(site *sp, b16 state)
{
	status_message *sm;

	verify(sp, V_site);

	if (sp->site_state == state)
		return;

	sp->site_state = state;

	sm = get_sm(sp);
	if (sm)
	{
		sm->command = SM_STATE_CHANGE;
		PutMsg(status_port, &sm->header);
	}
}



static void fill_fib(site *site, struct FileInfoBlock *fib, ftpinfo *fi)
// FileInfoBlock mit Daten aus ftpinfo füllen
{
	if (fi->flags & MYFLAG_DIR)
	{
		fib->fib_DirEntryType = ST_USERDIR;
	}
	else {
		fib->fib_DirEntryType = ST_FILE;
	}

	fib->fib_EntryType = fib->fib_DirEntryType;
	fib->fib_DiskKey = 0;
	strncpy(&fib->fib_FileName[1], fi->name, sizeof(fib->fib_FileName) - 1);
	fib->fib_FileName[sizeof(fib->fib_FileName) - 1] = 0;
	fib->fib_FileName[0] = strlen(&fib->fib_FileName[1]);

	fib->fib_Protection = fi->flags & 0xffff;
	fib->fib_Size = fi->size;
	fib->fib_NumBlocks = fi->blocks;
	fib->fib_Date = fi->modified;
	if (site->comment)
	{
		strncpy(&fib->fib_Comment[1], fi->name + strlen(fi->name) + 1,
			sizeof(fib->fib_Comment) - 1);
		fib->fib_Comment[sizeof(fib->fib_Comment) - 1] = 0;
		fib->fib_Comment[0] = strlen(&fib->fib_Comment[1]);
	}
	else
		fib->fib_Comment[0] = 0;
} // fill_fib()


void SAVEDS site_handler(void)
{
	struct Process *me;
	struct Message *msg;
	struct DosPacket *dp;
	struct MsgPort *local, *reply, *sync, *timeport, *rank;
	struct StandardPacket *idle_packet;
	struct timerequest *timer;
	status_message *sm, *tsm;
	lock *new_lock, *slock, **locks;
	site *ftp_site;
	b32 signals;
	int idlecount, n;
	split sd, sd2;
	b8 *s;
	struct info_header *ih;
	ftpinfo *fi;
	file_info *fip;
	struct FileHandle *fh;
	struct FileInfoBlock *fib;
	b32 o1 = 0, o2, o3; /* 2003-03-08 rri */

	me = (struct Process *)FindTask(0);

	local = &me->pr_MsgPort;

	WaitPort(local);
	msg = GetMsg(local);

	dp = (struct DosPacket *)msg->mn_Node.ln_Name;

	reply = dp->dp_Port;
	dp->dp_Port = local;

	ftp_site = (site *)dp->dp_Arg1;

	verify(ftp_site, V_site);

	local->mp_Node.ln_Name = ftp_site->name;

	idlecount = 0;

	mem_tracking_on();

	ftp_site->IBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
	if (ftp_site->IBase)
	{
#ifdef __amigaos4__
		ftp_site->pIIntuition = (struct IntuitionIFace*)GetInterface((struct Library*)ftp_site->IBase, "main", 1L, 0);
		if (ftp_site->pIIntuition)
		{
#endif

			ftp_site->GTBase = OpenLibrary("gadtools.library", 0);
			if (ftp_site->GTBase)
			{
#ifdef __amigaos4__
				ftp_site->pIGadTools = (struct GadToolsIFace*)GetInterface((struct Library*)ftp_site->GTBase, "main", 1L, 0);
				if (ftp_site->pIGadTools)
				{
#endif
					ftp_site->GBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
					if (ftp_site->GBase)
					{
#ifdef __amigaos4__
						ftp_site->pIGraphics = (struct GraphicsIFace*)GetInterface((struct Library*)ftp_site->GBase, "main", 1L, 0);
						if (ftp_site->pIGraphics)
						{
#endif
							sync = CreatePort(0, 0);
							if (sync)
							{
								ftp_site->sync = sync;

								timeport = CreatePort(0, 0);
								if (timeport)
								{
									timer = (struct timerequest *)CreateExtIO(timeport, sizeof(*timer));
									if (timer)
									{
										idle_packet = (struct StandardPacket *)allocate(sizeof(*idle_packet), V_StandardPacket);
										if (idle_packet)
										{
											idle_packet->sp_Msg.mn_Node.ln_Name =
												(void *)&idle_packet->sp_Pkt;
											idle_packet->sp_Pkt.dp_Link = &idle_packet->sp_Msg;
											idle_packet->sp_Pkt.dp_Type = action_IDLE;
											idle_packet->sp_Pkt.dp_Arg1 = (b32)ftp_site;

											prime->header.mn_ReplyPort = sync;
											prime->command = TCP_NEWMESSAGE;
											PutMsg(tcp, &prime->header);
											WaitPort(sync); GetMsg(sync);
											if (prime->result)
											{
												ftp_site->control = prime->data;

												PutMsg(tcp, &prime->header);
												WaitPort(sync); GetMsg(sync);
												if (prime->result)
												{
													ftp_site->intr = prime->data;
													ftp_site->intr->interrupt = ftp_site->control;
													ftp_site->intr->header.mn_ReplyPort = sync;
													ftp_site->intr->command = TCP_INTERRUPT;

													if (OpenDevice("timer.device", UNIT_VBLANK, (struct IORequest *)timer, 0) == 0)
													{
														sm = (status_message *)allocate(sizeof(*sm), V_status_message);
														if (sm)
														{
															rank = CreatePort(0, 0);
															if (rank)
															{
																ftp_site->rank = rank;
																goto begin;
															}
															else
																dp->dp_Res2 = ERROR_NO_FREE_STORE;
															deallocate(sm, V_status_message);
														}
														else
															dp->dp_Res2 = ERROR_NO_FREE_STORE;
														CloseDevice((struct IORequest *)timer);
													}
													else
														dp->dp_Res2 = ERROR_DEVICE_NOT_MOUNTED;
													ftp_site->intr->command = TCP_DISPOSE;
													PutMsg(tcp, &ftp_site->intr->header);
												}
												else
													dp->dp_Res2 = ERROR_NO_FREE_STORE;
												ftp_site->control->command = TCP_DISPOSE;
												PutMsg(tcp, &ftp_site->control->header);
											}
											else
												dp->dp_Res2 = ERROR_NO_FREE_STORE;
											deallocate(idle_packet, V_StandardPacket);
										}
										else
											dp->dp_Res2 = ERROR_NO_FREE_STORE;
										DeleteExtIO((struct IORequest *)timer);
									}
									else
										dp->dp_Res2 = ERROR_NO_FREE_STORE;
									DeletePort(timeport);
								}
								else
									dp->dp_Res2 = ERROR_NO_FREE_STORE;
								DeletePort(sync);
							}
							else
								dp->dp_Res2 = ERROR_NO_FREE_STORE;
#ifdef __amigaos4__
						}
						else
							dp->dp_Res2 = ERROR_INVALID_RESIDENT_LIBRARY;

						DropInterface((struct Interface*)ftp_site->pIGraphics);
#endif
						CloseLibrary((struct Library *)ftp_site->GBase);
					}
					else
						dp->dp_Res2 = ERROR_INVALID_RESIDENT_LIBRARY;
#ifdef __amigaos4__
				}
				else
					dp->dp_Res2 = ERROR_INVALID_RESIDENT_LIBRARY;

				DropInterface((struct Interface*)ftp_site->pIGadTools);
#endif
				CloseLibrary(ftp_site->GTBase);
			}
			else
				dp->dp_Res2 = ERROR_INVALID_RESIDENT_LIBRARY;
#ifdef __amigaos4__
		}
		else
			dp->dp_Res2 = ERROR_INVALID_RESIDENT_LIBRARY;

		DropInterface((struct Interface*)ftp_site->pIIntuition);
#endif
		CloseLibrary((struct Library *)ftp_site->IBase);
	}
	else
		dp->dp_Res2 = ERROR_INVALID_RESIDENT_LIBRARY;

	dp->dp_Res1 = DOSFALSE;

	check_memory();

	Forbid();
	PutMsg(reply, dp->dp_Link);
	return;

begin:
	timer->tr_node.io_Command = TR_ADDREQUEST;
	timer->tr_time.tv_secs = IDLE_INTERVAL;
	timer->tr_time.tv_micro = 0;
	SendIO((struct IORequest *)timer);

	ensure(sm, V_status_message);

	sm->header.mn_ReplyPort = sync;
	sm->header.mn_Length = sizeof(*sm);
	sm->header.mn_Node.ln_Type = NT_MESSAGE;
	sm->header.mn_Node.ln_Pri = 0;
	sm->header.mn_Node.ln_Name = "site status message";

	sm->command = SM_NEW_SITE;
	sm->data = ftp_site->open_status;
	sm->this_site = ftp_site;

	PutMsg(status_port, &sm->header);
	WaitPort(sync); GetMsg(sync);

	ftp_site->abort_signals = (1 << AllocSignal(-1));
	ftp_site->disconnect_signals = (1 << AllocSignal(-1));

	dp->dp_Res1 = DOSTRUE;
	dp->dp_Res2 = 0;

	PutMsg(reply, dp->dp_Link);   /* send it back to signal we are away */

	signals = (1 << local->mp_SigBit) | (1 << timeport->mp_SigBit) | (ftp_site->disconnect_signals);

	while (1)
	{
		if (ftp_site->connected)
		{
			if (!ftp_site->cfile)
			{
				state_change(ftp_site, SS_IDLE);
			}
		}
		else {
			state_change(ftp_site, SS_DISCONNECTED);
		}

		if (Wait(signals) & ftp_site->disconnect_signals)
		{
			disconnect(ftp_site);
		}

		if (GetMsg(timeport))
		{
			timer->tr_node.io_Command = TR_ADDREQUEST;
			timer->tr_time.tv_secs = IDLE_INTERVAL;
			timer->tr_time.tv_micro = 0;

			idlecount++;

			SendIO((struct IORequest *)timer);

			if (ftp_site->lock_list == nil && ftp_site->file_list == nil)
			{
				if (ftp_site->connected)
				{
					if (idlecount > ftp_site->no_lock_conn_idle)    // war NO_LOCK_CONN_IDLE
					{
						idle_packet->sp_Pkt.dp_Port = sync;
						PutMsg(ftp_port, &idle_packet->sp_Msg);
						WaitPort(sync); GetMsg(sync);
					}
				}
				else if (!ftp_site->connected)
				{
					if (idlecount > NO_LOCK_NO_CONN_IDLE)
					{
						idle_packet->sp_Pkt.dp_Port = sync;
						PutMsg(ftp_port, &idle_packet->sp_Msg);
						WaitPort(sync); GetMsg(sync);
					}
				}
			} // if
		} // if (GetMsg(timeport))

		while (msg = GetMsg(local))
		{
			idlecount = 0;

			dp = (struct DosPacket *)msg->mn_Node.ln_Name;

			reply = dp->dp_Port;

			switch (dp->dp_Type)
			{
			case action_IDLE_DEATH:
				DS(kprintf("ACTION_IDLE_DEATH"))

					if (ftp_site->file_list && ftp_site->file_list->closed)
					{
						close_file(ftp_site, true);
					}
				if (ftp_site->lock_list || ftp_site->file_list)
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = 0;

					break;
				}
				/* fall through to DIE */
			case ACTION_DIE:
				DS(kprintf("ACTION_DIE"))
					state_change(ftp_site, SS_DISCONNECTING);

				slock = ftp_site->lock_list;
				while (slock)
				{
					new_lock = slock->next;
					disown(slock, V_lock);
					slock = new_lock;
				}

				deallocate(idle_packet, V_StandardPacket);

				AbortIO((struct IORequest *)timer);
				WaitPort(timeport); GetMsg(timeport);

				CloseDevice((struct IORequest *)timer);

				if (ftp_site->connected)
				{
					ftp_site->control->command = TCP_CLOSE;
					PutMsg(tcp, &ftp_site->control->header);
					WaitPort(sync); GetMsg(sync);

					ftp_site->connected = false;
				}

				ftp_site->control->command = TCP_DISPOSE;
				PutMsg(tcp, &ftp_site->control->header);

				ftp_site->intr->command = TCP_DISPOSE;
				PutMsg(tcp, &ftp_site->intr->header);

				sm->command = SM_DEAD_SITE;

				PutMsg(status_port, &sm->header);
				WaitPort(sync); GetMsg(sync);

				while (tsm = (status_message *)GetMsg(rank))
				{
					verify(tsm, V_status_message);
					deallocate(tsm, V_status_message);
				}

				DeletePort(rank);
				deallocate(sm, V_status_message);

				DeleteExtIO((struct IORequest *)timer);
				DeletePort(timeport);
				DeletePort(sync);

#ifdef __amigaos4__
				DropInterface((struct Interface*)ftp_site->pIGraphics);
				DropInterface((struct Interface*)ftp_site->pIGadTools);
				DropInterface((struct Interface*)ftp_site->pIIntuition);
#endif
				CloseLibrary((struct Library *)ftp_site->GBase);
				CloseLibrary(ftp_site->GTBase);
				CloseLibrary((struct Library *)ftp_site->IBase);

				while (ftp_site->infos)
					free_info_header(ftp_site->infos);

				if (ftp_site->cwd)
					deallocate(ftp_site->cwd, V_cstr);
				if (ftp_site->root)
				{
					/* this one might have been allocated from main task ... but */
					deallocate(ftp_site->root, V_cstr);
					ftp_site->root = nil;
				}

				/* again, these may have been allocated in either the main task or
				   the site task ... too complicated to work it out ... this works for now */

				if (ftp_site->user)
					deallocate(ftp_site->user, V_cstr);
				if (ftp_site->password)
					deallocate(ftp_site->password, V_cstr);

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = (b32)(ftp_site->lock_list); /* so they can adopt the locks */

				check_memory();

				Forbid();
				PutMsg(reply, dp->dp_Link);
				return;

			case ACTION_LOCATE_OBJECT:
				DS(kprintf("ACTION_LOCATE_OBJECT %s mode %ld", (dp->dp_Arg2 << 2) + 1, dp->dp_Arg3))

					if (!split_data((lock *)(dp->dp_Arg1 << 2), (b8 *)(dp->dp_Arg2 << 2), &sd))
					{
						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;

						break;
					}

				if (!ftp_site->connected)
				{
					init_connect(ftp_site);

					if (!ftp_site->connected)
					{
						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;

						end_split(&sd);
						break;
					}
				}

				if (!sd.path || sd.path[0] == 0)
				{
					/* the root ... this is ok */
					if (dp->dp_Arg3 == EXCLUSIVE_LOCK)
					{
						dp->dp_Res1 = 0;
						/* can't exclusive lock root */
						dp->dp_Res2 = ERROR_OBJECT_IN_USE;
						break;
					}

					new_lock = (lock *)allocate(sizeof(*new_lock) + 1, V_lock);
					if (!new_lock)
					{
						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;

						break;
					}

					ensure(new_lock, V_lock);

					new_lock->next = ftp_site->lock_list;
					ftp_site->lock_list = new_lock;

					new_lock->port = local;
					new_lock->rfsl = 0;
					new_lock->lastkey = 0;
					new_lock->fname[0] = 0;

					new_lock->fl.fl_Link = 0;
					new_lock->fl.fl_Key = 0;
					new_lock->fl.fl_Access = SHARED_LOCK;
					new_lock->fl.fl_Task = ftp_port;
					new_lock->fl.fl_Volume = (b32)ftp_volume >> 2;

					dp->dp_Res1 = (b32)new_lock >> 2;
					dp->dp_Res2 = 0;
				}
				else
				{
					new_lock = (lock *)allocate(sizeof(*new_lock) + strlen(sd.path) + 1, V_lock);
					if (!new_lock)
					{
						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;
						break;
					}

					ensure(new_lock, V_lock);

					strcpy(new_lock->fname, sd.path);

					new_lock->port = local;
					new_lock->rfsl = 0;
					new_lock->lastkey = 0;

					new_lock->fl.fl_Link = 0;
					new_lock->fl.fl_Key = 0;
					new_lock->fl.fl_Access = SHARED_LOCK;
					new_lock->fl.fl_Task = ftp_port;
					new_lock->fl.fl_Volume = (b32)ftp_volume >> 2;

					/* search for a conflicting lock */

					slock = ftp_site->lock_list;
					while (slock)
					{
						if (strcmp(sd.path, slock->fname) == 0)
						{
							if (dp->dp_Arg3 == EXCLUSIVE_LOCK || slock->fl.fl_Access == EXCLUSIVE_LOCK)
							{
								dp->dp_Res1 = 0;
								dp->dp_Res2 = ERROR_OBJECT_IN_USE;

								deallocate(new_lock, V_lock);
								end_split(&sd);

								goto reply_msg;
							}

							/* ok, this one is guaranteed to work */

							new_lock->next = ftp_site->lock_list;
							ftp_site->lock_list = new_lock;

							dp->dp_Res1 = (b32)new_lock >> 2;
							dp->dp_Res2 = 0;

							end_split(&sd);

							goto reply_msg;
						}
						slock = slock->next;
					}

					/* ok, it doesn't conflict ... check if it actually exists */

					fi = get_info(ftp_site, sd.path);
					if (!fi)
					{
						deallocate(new_lock, V_lock);
						end_split(&sd);

						dp->dp_Res1 = 0;
						if (ftp_site->cfile)
							dp->dp_Res2 = ERROR_OBJECT_IN_USE;
						else
							dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;

						goto reply_msg;
					}

					/* well, we found it! */

					new_lock->next = ftp_site->lock_list;
					ftp_site->lock_list = new_lock;

					dp->dp_Res1 = (b32)new_lock >> 2;
					dp->dp_Res2 = 0;
				}

				end_split(&sd);
				break;

			case ACTION_FREE_LOCK:
				DS(kprintf("ACTION_FREE_LOCK $%08lx", dp->dp_Arg1 << 2))

					slock = (lock *)(dp->dp_Arg1 << 2);

				if (!slock)
				{
					dp->dp_Res1 = DOSTRUE;
					dp->dp_Res2 = 0;
					break;
				}

				verify(slock, V_lock);

				locks = &ftp_site->lock_list;
				while (*locks && *locks != slock)
				{
					locks = &(*locks)->next;
				}

				if (!*locks)
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_LOCK;
				}
				else
				{
					*locks = slock->next;
					deallocate(slock, V_lock);

					dp->dp_Res1 = DOSTRUE;
					dp->dp_Res2 = 0;
				}

				break;

			case ACTION_DELETE_OBJECT:
				DS(kprintf("ACTION_DELETE_OBJECT %s", (dp->dp_Arg2 << 2) + 1))

					if (!split_data((lock *)(dp->dp_Arg1 << 2), (b8 *)(dp->dp_Arg2 << 2), &sd))
					{
						dp->dp_Res1 = DOSFALSE;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;

						break;
					}

				if (ftp_site->file_list)
				{
					if (ftp_site->file_list->closed)
					{
						close_file(ftp_site, true);
					}
					else
					{
						dp->dp_Res1 = DOSFALSE;
						dp->dp_Res2 = ERROR_OBJECT_IN_USE;

						end_split(&sd);

						goto reply_msg;
					}
				}

				/* search for a conflicting lock */

				slock = ftp_site->lock_list;
				while (slock)
				{
					if (strcmp(sd.path, slock->fname) == 0)
					{
						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_OBJECT_IN_USE;

						end_split(&sd);

						goto reply_msg;
					}
					slock = slock->next;
				}

				fi = get_info(ftp_site, sd.path);
				if (fi && fi->flags & MYFLAG_DIR)
				{
					dp->dp_Res2 = delete_directory(ftp_site, sd.path);
				}
				else {
					dp->dp_Res2 = delete_file(ftp_site, sd.path);
				}

				if (dp->dp_Res2)
				{
					dp->dp_Res1 = DOSFALSE;
				}
				else
				{
					if (fi)
						fi->flags |= MYFLAG_DELETED;
					dp->dp_Res1 = DOSTRUE;
				}

				end_split(&sd);

				break;

			case ACTION_RENAME_OBJECT:
				DS(kprintf("ACTION_RENAME_OBJECT %s to %s", (dp->dp_Arg2 << 2) + 1, (dp->dp_Arg4 << 2) + 1))

					if (!split_data((lock *)(dp->dp_Arg1 << 2), (b8 *)(dp->dp_Arg2 << 2), &sd))
					{
						dp->dp_Res1 = DOSFALSE;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;

						break;
					}

				if (!split_data((lock *)(dp->dp_Arg3 << 2), (b8 *)(dp->dp_Arg4 << 2), &sd2))
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;

					end_split(&sd);

					break;
				}

				fi = get_info(ftp_site, sd.path);

				dp->dp_Res2 = rename_object(ftp_site, sd.path, sd2.path);

				if (dp->dp_Res2)
				{
					dp->dp_Res1 = DOSFALSE;
				}
				else
				{
					if (fi)
						fi->flags |= MYFLAG_DELETED;
					flush_info(ftp_site, sd2.path);
					dp->dp_Res1 = DOSTRUE;
				}

				end_split(&sd);
				end_split(&sd2);

				break;

			case ACTION_COPY_DIR:
				DS(kprintf("ACTION_COPY_DIR (DupLock) $%08lx", dp->dp_Arg1 << 2))

					slock = (lock *)(dp->dp_Arg1 << 2);
				if (!slock)
				{
					dp->dp_Res1 = 0;
					dp->dp_Res2 = 0;
					break;
				}

				verify(slock, V_lock);

				if (slock->fl.fl_Access == EXCLUSIVE_LOCK)
				{
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_OBJECT_IN_USE;
					break;
				}

				new_lock = (lock *)allocate(sizeof(*new_lock) + strlen(slock->fname) + 1, V_lock);
				if (!new_lock)
				{
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				*new_lock = *slock;
				strcpy(new_lock->fname, slock->fname);

				new_lock->next = slock->next;
				slock->next = new_lock;

				dp->dp_Res1 = (b32)new_lock >> 2;
				dp->dp_Res2 = 0;
				break;

			case ACTION_CREATE_DIR:
				DS(kprintf("ACTION_CREATE_DIR %s", (dp->dp_Arg2 << 2) + 1))

					if (!split_data((lock *)(dp->dp_Arg1 << 2), (b8 *)(dp->dp_Arg2 << 2), &sd))
					{
						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;

						break;
					}

				if (ftp_site->file_list)
				{
					if (ftp_site->file_list->closed)
					{
						close_file(ftp_site, true);
					}
					else
					{
						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_OBJECT_IN_USE;

						end_split(&sd);

						goto reply_msg;
					}
				}

				new_lock = (lock *)allocate(sizeof(*new_lock) + strlen(sd.path) + 1, V_lock);
				if (!new_lock)
				{
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;

					end_split(&sd);

					break;
				}

				dp->dp_Res2 = make_directory(ftp_site, sd.path);
				flush_info(ftp_site, sd.path);

				if (dp->dp_Res2)
				{
					dp->dp_Res1 = 0;

					deallocate(new_lock, V_lock);
				}
				else
				{
					dp->dp_Res1 = (b32)new_lock >> 2;

					ensure(new_lock, V_lock);

					new_lock->next = ftp_site->lock_list;
					ftp_site->lock_list = new_lock;

					strcpy(new_lock->fname, sd.path);

					new_lock->port = local;
					new_lock->rfsl = 0;
					new_lock->lastkey = 0;

					new_lock->fl.fl_Link = 0;
					new_lock->fl.fl_Key = 0;
					new_lock->fl.fl_Access = SHARED_LOCK;
					new_lock->fl.fl_Task = ftp_port;
					new_lock->fl.fl_Volume = (b32)ftp_volume >> 2;
				}

				end_split(&sd);

				break;

			case ACTION_EXAMINE_OBJECT:
				DS(kprintf("ACTION_EXAMINE_OBJECT obj $%08lx", dp->dp_Arg1 << 2))

					slock = (lock *)(dp->dp_Arg1 << 2);
				fib = (struct FileInfoBlock *)(dp->dp_Arg2 << 2);

				verify(slock, V_lock);
				truth(fib != nil);

				if (slock->fname[0] == 0)
				{
					/* root of this site */
					fib->fib_DiskKey = 0;
					fib->fib_DirEntryType = ST_USERDIR;
					fib->fib_EntryType = ST_USERDIR;

					strcpy(&fib->fib_FileName[1], ftp_site->name);
					fib->fib_FileName[0] = strlen(ftp_site->name);

					fib->fib_Protection = 0xf;
					fib->fib_Size = 0;
					fib->fib_NumBlocks = 0;
					fib->fib_Date = ftp_volume->dol_misc.dol_volume.dol_VolumeDate;
					fib->fib_Comment[0] = 0;

					dp->dp_Res1 = DOSTRUE;
					dp->dp_Res2 = 0;

					break;
				}

				s = slock->fname + strlen(slock->fname) - 1;
				while (s > slock->fname && *s != '/') s--;

				if (s == slock->fname)
				{
					ih = get_dir(ftp_site, "");
				}
				else {
					*s = 0;
					ih = get_dir(ftp_site, slock->fname);
					*s++ = '/';
				}

				if (!ih) {
					dp->dp_Res1 = DOSFALSE;
					if (ftp_site->cfile)
						dp->dp_Res2 = ERROR_OBJECT_IN_USE;
					else
						dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
					/* general "connection buggered" */
					break;
				}

				fi = find_info(ih, s);
				if (!fi) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
					break;
				}

				slock->lastkey = 0;

				fill_fib(ftp_site, fib, fi);

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;
				break;

			case ACTION_EXAMINE_NEXT:
				DS(kprintf("ACTION_EXAMINE_NEXT dir $%08lx", dp->dp_Arg1 << 2))

					slock = (lock *)(dp->dp_Arg1 << 2);
				fib = (struct FileInfoBlock *)(dp->dp_Arg2 << 2);

				verify(slock, V_lock);
				truth(fib != nil);

				ih = get_dir(ftp_site, slock->fname);
				if (!ih) {
					dp->dp_Res1 = DOSFALSE;
					if (ftp_site->cfile)
						dp->dp_Res2 = ERROR_OBJECT_IN_USE;
					else
						dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
					break;
				}

				// n = fib->fib_DiskKey;
				n = slock->lastkey;

				slock->lastkey++;

				fi = ih->infos;
				while (fi && n) {
					fi = fi->next;
					n--;
				}

				while (fi && fi->flags & MYFLAG_DELETED) {
					fi = fi->next;
					slock->lastkey++;
				}

				fib->fib_DiskKey = slock->lastkey;

				if (!fi) {
					slock->lastkey = 0;
					fib->fib_DiskKey = 0;
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_MORE_ENTRIES;
					break;
				}

				fill_fib(ftp_site, fib, fi);

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;

				break;

				// !!! case ACTION_EXAMINE_ALL

			case ACTION_PARENT:
				DS(kprintf("ACTION_PARENT of $%08lx", dp->dp_Arg1 << 2))

					slock = (lock *)(dp->dp_Arg1 << 2);
				if (!slock) {
					dp->dp_Res1 = 0;
					dp->dp_Res2 = 0;
					break;
				}

				if (slock->fname[0] == 0) {
					/* need root of FTP: (the slimy way) */
					dp->dp_Port = ftp_site->sync;
					dp->dp_Type = ACTION_LOCATE_OBJECT;

					o1 = dp->dp_Arg1;
					o2 = dp->dp_Arg2;
					o3 = dp->dp_Arg3;
					dp->dp_Arg1 = 0;
					dp->dp_Arg2 = (b32)(&("\0\0\0\0"[3])) >> 2;
					dp->dp_Arg3 = SHARED_LOCK;

					PutMsg(local_port, dp->dp_Link);
					WaitPort(ftp_site->sync); GetMsg(ftp_site->sync);

					dp->dp_Arg1 = o1;
					dp->dp_Arg2 = o2;
					dp->dp_Arg3 = o3;
					dp->dp_Type = ACTION_PARENT;
					break;
				}

				s = slock->fname + strlen(slock->fname) - 1;
				while (s > slock->fname && *s != '/') s--;

				if (s == slock->fname) {
					new_lock = (lock *)allocate(sizeof(*new_lock) + 1, V_lock);
					if (!new_lock) {
						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;

						break;
					}

					ensure(new_lock, V_lock);

					new_lock->next = ftp_site->lock_list;
					ftp_site->lock_list = new_lock;

					new_lock->port = local;
					new_lock->rfsl = 0;
					new_lock->fname[0] = 0;

					new_lock->fl.fl_Link = 0;
					new_lock->fl.fl_Key = 0;
					new_lock->fl.fl_Access = SHARED_LOCK;
					new_lock->fl.fl_Task = ftp_port;
					new_lock->fl.fl_Volume = (b32)ftp_volume >> 2;

					dp->dp_Res1 = (b32)new_lock >> 2;
					dp->dp_Res2 = 0;
				}
				else {
					*s = 0;

					new_lock = (lock *)allocate(sizeof(*new_lock) + strlen(slock->fname) + 1, V_lock);
					if (!new_lock) {
						*s = '/';

						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;
						break;
					}

					ensure(new_lock, V_lock);

					strcpy(new_lock->fname, slock->fname);

					*s = '/';

					new_lock->port = local;
					new_lock->rfsl = 0;

					new_lock->fl.fl_Link = 0;
					new_lock->fl.fl_Key = 0;
					new_lock->fl.fl_Access = SHARED_LOCK;
					new_lock->fl.fl_Task = ftp_port;
					new_lock->fl.fl_Volume = (b32)ftp_volume >> 2;

					/* search for a conflicting lock */

					slock = ftp_site->lock_list;
					while (slock) {
						if (strcmp(new_lock->fname, slock->fname) == 0) {
							if (slock->fl.fl_Access == EXCLUSIVE_LOCK) {
								dp->dp_Res1 = 0;
								dp->dp_Res2 = ERROR_OBJECT_IN_USE;

								deallocate(new_lock, V_lock);
								goto reply_msg;
							}

							/* ok, this one is guaranteed to work */

							new_lock->next = ftp_site->lock_list;
							ftp_site->lock_list = new_lock;

							dp->dp_Res1 = (b32)new_lock >> 2;
							dp->dp_Res2 = 0;

							goto reply_msg;
						}
						slock = slock->next;
					}

					/* ok, it doesn't conflict ... it must exist*/

					new_lock->next = ftp_site->lock_list;
					ftp_site->lock_list = new_lock;

					dp->dp_Res1 = (b32)new_lock >> 2;
					dp->dp_Res2 = 0;
				}
				break;

			case ACTION_SAME_LOCK:
				DS(kprintf("ACTION_SAME_LOCK $%08lx and $%08lx", dp->dp_Arg1 << 2, dp->dp_Arg2 << 2))

					slock = (lock *)(dp->dp_Arg1 << 2);
				new_lock = (lock *)(dp->dp_Arg2 << 2);

				verify(slock, V_lock);
				verify(new_lock, V_lock);

				if (strcmp(slock->fname, new_lock->fname) == 0) {
					dp->dp_Res1 = DOSTRUE;
					dp->dp_Res2 = 0;
				}
				else {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = 0;
				}
				break;

			case ACTION_READ:
				DS(kprintf("ACTION_READ fhd $%08lx, %ld bytes", dp->dp_Arg1, dp->dp_Arg3))

					fip = (file_info *)dp->dp_Arg1;
				verify(fip, V_file_info);

				if (!ftp_site->connected || ftp_site->cfile == nil) {
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
					break;
				}

				if (ftp_site->cfile_type != ACTION_FINDINPUT) {
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_READ_PROTECTED;
					break;
				}

				if (fip->seek_end) { /* artificially at end */
					dp->dp_Res1 = 0;
					dp->dp_Res2 = 0;
					break;
				}

				state_change(ftp_site, SS_READING);

				o1 = dp->dp_Arg3;
				s = (b8 *)dp->dp_Arg2;

				if (o1 == 0)
				{
					dp->dp_Res1 = 0;
					dp->dp_Res2 = 0;
					break;
				}

				if (fip->vpos < FIRST_BLOCK_SIZE && fip->vpos < fip->rpos)
				{
					o2 = o1;
					if (o2 > FIRST_BLOCK_SIZE - fip->vpos) o2 = FIRST_BLOCK_SIZE - fip->vpos;
					if (o2 > fip->rpos - fip->vpos) o2 = fip->rpos - fip->vpos;

					memcpy(s, &fip->first_block[fip->vpos], o2);

					fip->vpos += o2;

					if (o2 == o1)
					{
						dp->dp_Res1 = o1;
						dp->dp_Res2 = 0;

						break;
					}
					else {
						s += o2;
						o1 -= o2;
					}
				}

				if (fip->vpos != fip->rpos)
				{
					dp->dp_Res1 = -1;
					dp->dp_Res2 = ERROR_SEEK_ERROR;
					break;
				}

				while (o1)
				{
					if (o1 > MAX_READ_SIZE)
					{
						o2 = MAX_READ_SIZE;
						o1 -= o2;
					}
					else {
						o2 = o1;
						o1 = 0;
					}
					switch (read_file(ftp_site, s, &o2))
					{
					case NO_ERROR:
						s += o2;
						break;
					case ERROR_EOF:
						s += o2;
						dp->dp_Res1 = s - (b8 *)dp->dp_Arg2;
						dp->dp_Res2 = 0;
						o1 = 0;
						fip->eof = true;
						break;
					default:
						dp->dp_Res1 = -1;
						dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
						o1 = 0;
						break;
					}
					fip->rpos += o2;
					fip->vpos = fip->rpos;
					tsm = get_sm(ftp_site);
					if (tsm)
					{
						tsm->command = SM_PROGRESS;
						PutMsg(status_port, &tsm->header);
					}
				}

				dp->dp_Res1 = s - (b8 *)dp->dp_Arg2;
				dp->dp_Res2 = 0;
				break;

			case ACTION_WRITE:
				DS(kprintf("ACTION_WRITE fhd $%08lx, %ld bytes", dp->dp_Arg1, dp->dp_Arg3))

					fip = (file_info *)dp->dp_Arg1;
				verify(fip, V_file_info);

				if (!ftp_site->connected || ftp_site->cfile == nil)
				{
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
					break;
				}

				if (ftp_site->cfile_type != ACTION_FINDOUTPUT)
				{
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_WRITE_PROTECTED;
					break;
				}

				state_change(ftp_site, SS_WRITING);

				o1 = dp->dp_Arg3;
				s = (b8 *)dp->dp_Arg2;

				if (o1 == 0)
				{
					dp->dp_Res1 = 0;
					dp->dp_Res2 = 0;
					break;
				}

				if (fip->vpos != fip->rpos)
				{
					dp->dp_Res1 = -1;
					dp->dp_Res2 = ERROR_SEEK_ERROR;
					break;
				}

				while (o1)
				{
					if (o1 > MAX_READ_SIZE)
					{
						o2 = MAX_READ_SIZE;
						o1 -= o2;
					}
					else {
						o2 = o1;
						o1 = 0;
					}
					switch (write_file(ftp_site, s, &o2))
					{
					case NO_ERROR:
						s += o2;
						break;
					case ERROR_EOF:
						s += o2;
						dp->dp_Res1 = s - (b8 *)dp->dp_Arg2;
						dp->dp_Res2 = 0;
						o1 = 0;
						fip->eof = true;
						break;
					default:
						dp->dp_Res1 = -1;
						dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
						o1 = 0;
						break;
					}
					fip->rpos += o2;
					fip->vpos = fip->rpos;
					tsm = get_sm(ftp_site);
					if (tsm)
					{
						tsm->command = SM_PROGRESS;
						PutMsg(status_port, &tsm->header);
					}
				}

				dp->dp_Res1 = s - (b8 *)dp->dp_Arg2;
				dp->dp_Res2 = 0;
				break;

			case ACTION_FINDINPUT:
			case ACTION_FINDOUTPUT:
				DS(kprintf("ACTION_FINDIN/OUTPUT dir $%08lx, %s", dp->dp_Arg2 << 2, (dp->dp_Arg3 << 2) + 1))

					if (!split_data((lock *)(dp->dp_Arg2 << 2),
						(b8 *)(dp->dp_Arg3 << 2), &sd))
					{
						dp->dp_Res1 = DOSFALSE;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;
						break;
					}

				if (!ftp_site->connected)
					init_connect(ftp_site);

				if (!ftp_site->connected) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;

					end_split(&sd);
					break;
				}

				if (ftp_site->cfile) {
					fip = ftp_site->file_list;
					verify(fip, V_file_info);

					if (fip->closed) {
						if (strcmp(sd.path, fip->fname) == 0 &&
							dp->dp_Type == ACTION_FINDINPUT) {
							/* "reopen" this file */
							fh = (struct FileHandle *)(dp->dp_Arg1 << 2);

							truth(fh != nil);

							fh->fh_Type = ftp_port;
							fh->fh_Args = (b32)fip;

							fip->closed = false;
							fip->vpos = 0;
							fip->seek_end = false;

							dp->dp_Res1 = DOSTRUE;
							dp->dp_Res2 = 0;

							end_split(&sd);
							break;
						}

						close_file(ftp_site, true);
					}
					else {
						/* only one file at a time!  :( */
						dp->dp_Res1 = DOSFALSE;
						dp->dp_Res2 = ERROR_OBJECT_IN_USE;

						end_split(&sd);
						break;
					}
				}

				/* search for a conflicting lock */

				slock = ftp_site->lock_list;
				while (slock) {
					if (strcmp(sd.path, slock->fname) == 0) {
						if (dp->dp_Type == ACTION_FINDOUTPUT || slock->fl.fl_Access == EXCLUSIVE_LOCK) {
							dp->dp_Res1 = 0;
							dp->dp_Res2 = ERROR_OBJECT_IN_USE;

							end_split(&sd);

							goto reply_msg;
						}
					}
					slock = slock->next;
				}

				/* make sure we have information on this file BEFORE we start */

				if (dp->dp_Type == ACTION_FINDINPUT) {
					fi = get_info(ftp_site, sd.path);
					// if (!fi) {
					//    dp->dp_Res1 = 0;
					//    dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
					//
					//    end_split(&sd);
					//
					//    break;
					// }
				}
				else {
					fi = nil;
				}

				if (dp->dp_Type == ACTION_FINDINPUT) {
					if (fi) {
						dp->dp_Res2 = open_file(ftp_site, sd.path, false, fi->name);
					}
					else {
						dp->dp_Res2 = open_file(ftp_site, sd.path, false, nil);
					}
				}
				else {
					dp->dp_Res2 = open_file(ftp_site, sd.path, true, nil);
					flush_info(ftp_site, sd.path);
				}

				if (dp->dp_Res2 != 0) {
					end_split(&sd);

					dp->dp_Res1 = 0;
					break;
				}

				fip = ftp_site->file_list;

				if (fi) {
					fip->end = fi->size;
				}
				else {
					fip->end = 0;
				}

				fip->port = local;
				fip->type = ftp_site->cfile_type = dp->dp_Type;

				end_split(&sd);

				fh = (struct FileHandle *)(dp->dp_Arg1 << 2);

				truth(fh != nil);

				fh->fh_Type = ftp_port;
				fh->fh_Args = (b32)fip;

				if (dp->dp_Type == ACTION_FINDINPUT) {
					o1 = FIRST_BLOCK_SIZE;
					switch (read_file(ftp_site, fip->first_block, &o1)) {
					case ERROR_EOF:
						fip->eof = true;
					case NO_ERROR:
						fip->rpos = o1;

						dp->dp_Res1 = DOSTRUE;
						dp->dp_Res2 = 0;

						break;
					default:
						close_file(ftp_site, true);

						dp->dp_Res1 = DOSFALSE;
						dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;

						break;
					}
				}
				else {
					dp->dp_Res1 = DOSTRUE;
					dp->dp_Res2 = 0;
				}

				break;

			case ACTION_END:
				DS(kprintf("ACTION_END (Close) fhd $%08lx", dp->dp_Arg1))

					fip = (file_info *)dp->dp_Arg1;

				verify(fip, V_file_info);

				if (fip->type == ACTION_FINDINPUT && fip->rpos <= FIRST_BLOCK_SIZE) {
					fip->closed = true;
				}
				else {
					close_file(ftp_site, true);
				}

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;

				break;

			case ACTION_SEEK:
				DS(kprintf("ACTION_SEEK fhd $%08lx, pos. %ld, from %ld", dp->dp_Arg1, dp->dp_Arg2, dp->dp_Arg3))

					fip = (file_info *)dp->dp_Arg1;
				verify(fip, V_file_info);

				if (dp->dp_Arg3 == OFFSET_END && dp->dp_Arg2 == 0) {
					/* go to the end of the file ... really we are :) */

					if (fip->seek_end)
					{
						dp->dp_Res1 = fip->end;
						dp->dp_Res2 = 0;
						break;
					}

					if (fip->type == ACTION_FINDINPUT)
						fip->seek_end = true;

					dp->dp_Res1 = fip->vpos;
					dp->dp_Res2 = 0;

					break;
				}

				if (dp->dp_Arg3 == OFFSET_BEGINNING) {
					o1 = dp->dp_Arg2;
				}
				else if (dp->dp_Arg3 == OFFSET_END) {
					o1 = fip->end + dp->dp_Arg2;
				}
				else if (dp->dp_Arg3 == OFFSET_CURRENT) {
					o1 = fip->vpos + dp->dp_Arg2;
				}

				if (o1 == fip->vpos) {
					/* not actually moving */

					if (fip->seek_end) {
						dp->dp_Res1 = fip->end;
						dp->dp_Res2 = 0;

						fip->seek_end = false;
					}
					else {
						dp->dp_Res1 = fip->vpos;
						dp->dp_Res2 = 0;
					}
					break;
				}

				if (o1 == fip->rpos)
				{
					/* not _really_ moving */

					if (fip->seek_end)
					{
						dp->dp_Res1 = fip->end;
						dp->dp_Res2 = 0;

						fip->seek_end = false;
					}
					else {
						dp->dp_Res1 = fip->vpos;
						dp->dp_Res2 = 0;
					}

					fip->vpos = fip->rpos;
					break;
				}

				if (o1 < fip->rpos && o1 < FIRST_BLOCK_SIZE)
				{
					/* seeking into our stored first block */
					if (fip->seek_end)
					{
						dp->dp_Res1 = fip->end;
						dp->dp_Res2 = 0;

						fip->seek_end = false;
					}
					else {
						dp->dp_Res1 = fip->vpos;
						dp->dp_Res2 = 0;
					}
					fip->vpos = o1;
					break;
				}

				show_string("SEEK:");
				show_int(dp->dp_Arg2);
				show_int(dp->dp_Arg3);
				dp->dp_Res1 = DOSFALSE;
				dp->dp_Res2 = ERROR_ACTION_NOT_KNOWN;
				break;

			case ACTION_FH_FROM_LOCK:
				DS(kprintf("ACTION_FH_FROM_LOCK $%08lx", dp->dp_Arg2 << 2))

					fh = (struct FileHandle *)(dp->dp_Arg1 << 2);
				slock = (lock *)(dp->dp_Arg2 << 2);

				truth(fh != nil);
				verify(slock, V_lock);

				if (!ftp_site->connected)
					init_connect(ftp_site);

				if (!ftp_site->connected) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;

					break;
				}

				if (ftp_site->cfile) {
					fip = ftp_site->file_list;
					verify(fip, V_file_info);

					if (fip->closed) {
						if (strcmp(slock->fname, fip->fname) == 0 &&
							slock->fl.fl_Access == SHARED_LOCK) {
							/* "reopen" this file */

							fh->fh_Type = ftp_port;
							fh->fh_Args = (b32)fip;

							fip->closed = false;
							fip->vpos = 0;
							fip->seek_end = false;

							dp->dp_Res1 = DOSTRUE;
							dp->dp_Res2 = 0;

							break;
						}

						close_file(ftp_site, true);
					}
					else {
						/* only one file at a time!  :( */
						dp->dp_Res1 = DOSFALSE;
						dp->dp_Res2 = ERROR_OBJECT_IN_USE;
						break;
					}
				}

				if (slock->fl.fl_Access == SHARED_LOCK) {
					fi = get_info(ftp_site, slock->fname);
				}
				else {
					fi = nil;
				}

				if (slock->fl.fl_Access == SHARED_LOCK) {
					if (fi) {
						dp->dp_Res2 = open_file(ftp_site, slock->fname, false, fi->name);
					}
					else {
						dp->dp_Res2 = open_file(ftp_site, slock->fname, false, nil);
					}
				}
				else {
					dp->dp_Res2 = open_file(ftp_site, slock->fname, true, nil);
					flush_info(ftp_site, slock->fname);
				}

				if (dp->dp_Res2 != 0) {
					dp->dp_Res1 = 0;
					break;
				}

				fip = ftp_site->file_list;

				if (fi) {
					fip->end = fi->size;
				}
				else {
					fip->end = 0;
				}

				fip->port = local;

				if (slock->fl.fl_Access == SHARED_LOCK) {
					fip->type = ftp_site->cfile_type = ACTION_FINDINPUT;
				}
				else {
					fip->type = ftp_site->cfile_type = ACTION_FINDOUTPUT;
				}

				fh->fh_Type = ftp_port;
				fh->fh_Args = (b32)fip;

				if (slock->fl.fl_Access == SHARED_LOCK) {
					o1 = FIRST_BLOCK_SIZE;
					switch (read_file(ftp_site, fip->first_block, &o1)) {
					case ERROR_EOF:
						fip->eof = true;
					case NO_ERROR:
						fip->rpos = o1;

						dp->dp_Res1 = DOSTRUE;
						dp->dp_Res2 = 0;

						break;
					default:
						close_file(ftp_site, true);

						dp->dp_Res1 = DOSFALSE;
						dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;

						break;
					}
				}
				else {
					dp->dp_Res1 = DOSTRUE;
					dp->dp_Res2 = 0;
				}

				if (dp->dp_Res1) {
					/* close the lock */
					ensure(slock, 0);

					locks = &ftp_site->lock_list;
					while (*locks && *locks != slock) {
						locks = &(*locks)->next;
					}

					if (*locks) {
						*locks = slock->next;
						deallocate(slock, V_lock);
					}
				}

				break;

			case ACTION_COPY_DIR_FH:
				DS(kprintf("ACTION_COPY_DIR_FH (DupLockFH) fhd $%08lx", dp->dp_Arg1))

					/* ABA, OLD
								fh = (struct FileHandle *)(dp->dp_Arg1 << 2);   // !!! "<< 2" muß doch weg?
								if (!fh) {
								   dp->dp_Res1 = 0;
								   dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
								   break;
								}

								fip = (file_info *)fh->fh_Args;
					*/
					fip = (file_info *)(dp->dp_Arg1); /* 2006-11-27 ABA, BUG FIXING DOSPACKET ARGUMENTS */
				verify(fip, V_file_info);

				if (fip->type == ACTION_FINDOUTPUT) {
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_OBJECT_IN_USE;
					break;
				}

				new_lock = (lock *)allocate(sizeof(*new_lock) + strlen(fip->fname) + 1, V_lock);
				if (!new_lock) {
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				new_lock->port = local;
				new_lock->rfsl = 0;
				new_lock->lastkey = 0;

				new_lock->fl.fl_Link = 0;
				new_lock->fl.fl_Key = 0;
				new_lock->fl.fl_Access = SHARED_LOCK;
				new_lock->fl.fl_Task = ftp_port;
				new_lock->fl.fl_Volume = (b32)ftp_volume >> 2;

				strcpy(new_lock->fname, fip->fname);

				new_lock->next = ftp_site->lock_list;
				ftp_site->lock_list = new_lock;

				dp->dp_Res1 = (b32)new_lock >> 2;
				dp->dp_Res2 = 0;

				break;

			case ACTION_PARENT_FH:
				DS(kprintf("ACTION_PARENT_FH fhd $%08lx", dp->dp_Arg1))

					//            fh = (struct FileHandle *)(dp->dp_Arg1 << 2);   // !!! "<< 2" muß doch weg?
					//old            fh = (struct FileHandle *)(dp->dp_Arg1);   // !!! "<< 2" muß doch weg?
					//            if (!fh) {
					//               dp->dp_Res1 = 0;
					//               dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
					//               break;
					//            }
					//
					//            fip = (file_info *)fh->fh_Args;
					fip = (file_info *)(dp->dp_Arg1); /* 2006-11-27 ABA, bug fixing dospacket argument */

				verify(fip, V_file_info);

				s = fip->fname + strlen(fip->fname) - 1;
				while (s > fip->fname && *s != '/') s--;

				if (s == fip->fname) {
					new_lock = (lock *)allocate(sizeof(*new_lock) + 1, V_lock);
					if (!new_lock) {
						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;

						break;
					}

					ensure(new_lock, V_lock);

					new_lock->next = ftp_site->lock_list;
					ftp_site->lock_list = new_lock;

					new_lock->port = local;
					new_lock->rfsl = 0;
					new_lock->fname[0] = 0;

					new_lock->fl.fl_Link = 0;
					new_lock->fl.fl_Key = 0;
					new_lock->fl.fl_Access = SHARED_LOCK;
					new_lock->fl.fl_Task = ftp_port;
					new_lock->fl.fl_Volume = (b32)ftp_volume >> 2;

					dp->dp_Res1 = (b32)new_lock >> 2;
					dp->dp_Res2 = 0;
				}
				else {
					*s = 0;

					new_lock = (lock *)allocate(sizeof(*new_lock) + strlen(fip->fname) + 1, V_lock);
					if (!new_lock) {
						*s = '/';

						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;
						break;
					}

					ensure(new_lock, V_lock);

					strcpy(new_lock->fname, fip->fname);

					*s = '/';

					new_lock->port = local;
					new_lock->rfsl = 0;

					new_lock->fl.fl_Link = 0;
					new_lock->fl.fl_Key = 0;
					new_lock->fl.fl_Access = SHARED_LOCK;
					new_lock->fl.fl_Task = ftp_port;
					new_lock->fl.fl_Volume = (b32)ftp_volume >> 2;

					/* search for a conflicting lock */

					slock = ftp_site->lock_list;
					while (slock) {
						if (strcmp(new_lock->fname, slock->fname) == 0) {
							if (slock->fl.fl_Access == EXCLUSIVE_LOCK) {
								dp->dp_Res1 = 0;
								dp->dp_Res2 = ERROR_OBJECT_IN_USE;

								deallocate(new_lock, V_lock);
								goto reply_msg;
							}

							/* ok, this one is guaranteed to work */

							new_lock->next = ftp_site->lock_list;
							ftp_site->lock_list = new_lock;

							dp->dp_Res1 = (b32)new_lock >> 2;
							dp->dp_Res2 = 0;

							goto reply_msg;
						}
						slock = slock->next;
					}

					/* ok, it doesn't conflict ... it must exist*/

					new_lock->next = ftp_site->lock_list;
					ftp_site->lock_list = new_lock;

					dp->dp_Res1 = (b32)new_lock >> 2;
					dp->dp_Res2 = 0;
				}
				break;

			case ACTION_EXAMINE_FH:
				DS(kprintf("ACTION_EXAMINE_FH fhd $%08lx", dp->dp_Arg1))

					fh = (struct FileHandle *)(dp->dp_Arg1 << 2);   // !!! "<< 2" muß doch weg?
				fip = (file_info *)fh->fh_Args;

				verify(fip, V_file_info);

				fib = (struct FileInfoBlock *)(dp->dp_Arg2 << 2);

				truth(fib != nil);

				s = fip->fname + strlen(fip->fname) - 1;
				while (s > fip->fname && *s != '/') s--;

				if (s == fip->fname) {
					ih = get_dir(ftp_site, "");
				}
				else {
					*s = 0;
					ih = get_dir(ftp_site, fip->fname);
					*s++ = '/';
				}

				if (!ih) {
					dp->dp_Res1 = DOSFALSE;
					if (ftp_site->cfile)
						dp->dp_Res2 = ERROR_OBJECT_IN_USE;
					else
						dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
					/* general "connection buggered" */
					break;
				}

				fi = find_info(ih, s);
				if (!fi) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_OBJECT_NOT_FOUND;
					break;
				}

				fill_fib(ftp_site, fib, fi);

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;
				break;

			case action_SUSPEND:
				DS(kprintf("ACTION_SUPEND"))

					if (ftp_site->connected) {
						disconnect(ftp_site);
					}

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;

				dp->dp_Port = ftp_port;
				PutMsg(reply, dp->dp_Link);

				idle_packet->sp_Pkt.dp_Port = sync;
				PutMsg(ftp_port, &idle_packet->sp_Msg);
				WaitPort(sync); GetMsg(sync);

				continue;

			default:
				// ACTION_LOCK/FREE_RECORD
				// ACTION_SET_FILE_SIZE
				// ACTION_CHANGE_MODE
				// ACTION_FINDUPDATE
				// ACTION_MAKE_LINK
				// ACTION_READ_LINK
				// ACTION_SET_DATE, _SET_COMMENT, _SET_PROTECT
				show_int(dp->dp_Type);
				dp->dp_Res1 = DOSFALSE;
				dp->dp_Res2 = ERROR_ACTION_NOT_KNOWN;
				break;
			}

		reply_msg:
			DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", dp->dp_Res1, dp->dp_Res1, dp->dp_Res2))

				dp->dp_Port = ftp_port;
			PutMsg(reply, dp->dp_Link);
		}
	}
}
