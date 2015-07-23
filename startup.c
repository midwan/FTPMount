/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 * AmigaOS4 port (c) 2005-2006 Alexandre BALABAN (alexandre (at) free (dot) fr)
 */

#define DECLARE_GLOBALS_HERE 1

#include "FTPMount.h" /* 03-03-02 rri */
#include "tcp.h"
#include "site.h"
#include "local.h"
#include "request.h"
#include "strings.h"

 /* [ABA, 23/05/2005 : add support for debug checking] */
#if defined(VERIFY) || defined(MINI_VERIFY)
#include "verify_code.h"
#endif
/* [END ABA, 23/05/2005 : add support for debug checking] */

struct DosPacket *fh_listen(void);
void fh_ignore(void);

boolean launch_tcp_handler(void);
boolean launch_local(void);
void shutdown_tcp_handler(void);
void shutdown_local(void);

boolean open_libraries(void);
void close_libraries(void);

boolean make_gims(void);
void free_gims(void);

void startup_error(b8 *s);

boolean get_anon_login(void);

boolean create_volume(void);
void destroy_volume(void);

boolean launch_status(void);
void shutdown_status(void);

void setup_strings(void);
void cleanup_strings(void);

void SAVEDS start(void)
{
	struct Process *me;
	struct Message *msg;
	struct DosPacket *dp;
	struct MsgPort *reply;
	struct DateTime dtime;
	b8 temp[15];

	SysBase = *(struct ExecBase **)4;

#ifdef __amigaos4__
	IExec = (struct ExecIFace *)((struct ExecBase *)SysBase)->MainInterface;
#endif

	me = (struct Process *)FindTask(0l);

	ftp_port = &me->pr_MsgPort;

	WaitPort(ftp_port);  /* wait for startup packet */
	msg = GetMsg(ftp_port);

	dp = (struct DosPacket *)msg->mn_Node.ln_Name;
	reply = dp->dp_Port;
	dp->dp_Port = ftp_port;

	ftp_device = (struct DosList *)(dp->dp_Arg3 << 2);
	ftp_device->dol_Task = ftp_port; /* fill in our message port */

	/* get down to initializing everything */

	sites = nil;
	orphaned_locks = nil;

	if (open_libraries())
	{
		ftpdir_lock = Lock("FTPMountDir:", SHARED_LOCK);
		if (ftpdir_lock)
		{
			UnLock(CurrentDir(ftpdir_lock));
			/* setup PROGDIR: so we can open the catalog asap */
			UnLock(SetProgramDir(ftpdir_lock));

			setup_strings();

			/* get current year */
			DateStamp(&dtime.dat_Stamp);

			dtime.dat_StrDate = temp;
			dtime.dat_StrDay = nil;
			dtime.dat_StrTime = nil;
			dtime.dat_Flags = 0;
			dtime.dat_Format = FORMAT_INT;

			DateToStr(&dtime);
			year = atoi(temp);

			if (year >= 78) year += 1900;
			else year += 2000;

			/* set globale Var. "anon_login" to string "user@host" */
			if (get_anon_login())
			{
				/* create Buttons (Abort, Disconnect, etc.) */
				if (make_gims())
				{
					if (launch_tcp_handler())
					{
						if (launch_local())
						{
							ftphosts_lock = Lock("Hosts", SHARED_LOCK);
							if (ftphosts_lock)
							{
								if (launch_status())
								{
									if (create_volume())
									{
										/* initialization is complete */
										dp->dp_Res1 = DOSTRUE;
										dp->dp_Res2 = 0;
										PutMsg(reply, dp->dp_Link);
										dp = fh_listen();

										/* only comes back on a DIE */

										shutdown_sites();
										shutdown_status();
										shutdown_local();
										shutdown_tcp_handler();
										UnLock(ftphosts_lock);
										UnLock(ftpdir_lock);
										free_gims();
										if (anon_login) FreeVec(anon_login); /* 2003-03-09 rri */
										destroy_volume();
										close_libraries();

										if (dp)
										{
											dp->dp_Res1 = DOSTRUE;
											dp->dp_Res2 = 0;
											reply = dp->dp_Port;
											dp->dp_Port = ftp_port;
											PutMsg(reply, dp->dp_Link);
										}

										fh_ignore();	/* never comes back */
									}
									else dp->dp_Res2 = ERROR_NO_FREE_STORE;
									shutdown_status();
								}
								else dp->dp_Res2 = ERROR_NO_FREE_STORE;
								UnLock(ftphosts_lock);
							}
							else
							{
								startup_error(strings[MSG_CANT_FIND_HOSTS]);
								dp->dp_Res2 = ERROR_DIR_NOT_FOUND;
							}
							shutdown_local();
						}
						else dp->dp_Res2 = ERROR_NO_FREE_STORE;
						shutdown_tcp_handler();
					}
					else dp->dp_Res2 = ERROR_NO_FREE_STORE;
					free_gims();
				}
				else dp->dp_Res2 = ERROR_NO_FREE_STORE;
				if (anon_login) FreeVec(anon_login); /* 2003-03-09 rri */
			}
			else dp->dp_Res2 = ERROR_REQUIRED_ARG_MISSING;
			cleanup_strings();
			SetProgramDir(0);
			CurrentDir(0);
			UnLock(ftpdir_lock);
		}
		else dp->dp_Res2 = ERROR_DIR_NOT_FOUND;
		close_libraries();
	}
	else dp->dp_Res2 = ERROR_INVALID_RESIDENT_LIBRARY;
	ftp_device->dol_Task = 0;
	dp->dp_Res1 = DOSFALSE;
	Forbid();	/* this is so they can't unloadseg us until we have finished */
	PutMsg(reply, dp->dp_Link);
	return;
}


void startup_error(b8 *s)
{
	struct EasyStruct es;

	es.es_StructSize = sizeof(struct EasyStruct);
	es.es_Flags = 0;
	es.es_Title = strings[MSG_FTPM_STARTUP_ERROR];
	es.es_GadgetFormat = strings[MSG_OK];
	es.es_TextFormat = s;

	if (IntuitionBase) EasyRequest(nil, &es, nil);
}


boolean launch_tcp_handler(void)
{
	struct Process *child;
	tcpmessage *tm;

	unique_name(FindTask(0), ": FTPMount", unique_buffer);

#ifdef __amigaos4__
	startup_sync = (struct MsgPort*)AllocSysObjectTags(ASOT_PORT,
		ASO_NoTrack, 0,
		ASOPORT_Name, unique_buffer,
		ASOPORT_Pri, 0,
		TAG_DONE);

#else
	startup_sync = CreatePort(unique_buffer, 0);
#endif

	if (startup_sync)
	{
#ifndef	__MORPHOS__
		child = CreateNewProcTags
			(
				NP_Entry, (ULONG)tcp_handler,
				NP_Arguments, (ULONG)unique_buffer,
				NP_Name, (ULONG)strings[MSG_TCP_HANDLER],
				NP_StackSize, 6000,
				TAG_END, 0
				);
#else
		child = CreateNewProcTags
			(
				NP_CodeType, CODETYPE_PPC,
				NP_Entry, (ULONG)tcp_handler,
				NP_PPC_Arg1, (ULONG)unique_buffer,
				NP_Name, (ULONG)strings[MSG_TCP_HANDLER],
				TAG_END
				);
#endif

		if (child)
		{
			Wait(1 << startup_sync->mp_SigBit);
			tm = (tcpmessage *)GetMsg(startup_sync);

			if (tm)
			{
				if (tm->result)
				{
					tcp = tm->data;
					ReplyMsg(&tm->header);
					WaitPort(startup_sync);
					prime = (tcpmessage *)GetMsg(startup_sync);

					/* ok!  off we trundle */

					prime->command = TCP_SERVICE;
					prime->data = "ftp";

					PutMsg(tcp, &prime->header);
					WaitPort(startup_sync); GetMsg(startup_sync);

					if (prime->result)
					{
						ftp_port_number = prime->port.w;
					}
					else
					{
						ftp_port_number = 0; /* fill it in at connect time */
					}
					return true;
				}
				ReplyMsg(&tm->header);
				/* whether child is still alive here??? */
			}
		}
#ifdef __amigaos4__
		FreeSysObject(ASOT_PORT, startup_sync);
#else
		DeletePort(startup_sync);
#endif
	}
	startup_error(strings[MSG_CANT_LAUNCH_TCP]);
	return false;
}


void shutdown_tcp_handler(void)
{
	prime->command = TCP_DIE;
	prime->header.mn_ReplyPort = startup_sync;
	PutMsg(tcp, &prime->header);
	Wait(1 << startup_sync->mp_SigBit); /* Wait til the child signals it is dead */
	DeletePort(startup_sync);
	return;
}


boolean launch_local(void)
{
	struct StandardPacket *sp;
	struct Process *child;

	sp = (struct StandardPacket *) AllocVec(sizeof(*sp), MEMF_PUBLIC); /* 2003-03-09 rri */
	if (!sp) return false;

	local_msg = &sp->sp_Msg;
	local_msg->mn_Node.ln_Name = (char *)&sp->sp_Pkt;
	sp->sp_Pkt.dp_Link = local_msg;
	sp->sp_Pkt.dp_Type = ACTION_DIE; /* for startup it should ignore this :) */
	sp->sp_Pkt.dp_Port = startup_sync;  /* this is bad programming ... increases linkage */

#ifndef	__MORPHOS__
	child = CreateNewProcTags(NP_Entry, (ULONG)local_handler,
		NP_Name, (ULONG)strings[MSG_LOCAL_HANDLER],
		NP_StackSize, 6000,
		TAG_END, 0
		);
#else
	child = CreateNewProcTags(NP_Entry, (ULONG)local_handler,
		NP_CodeType, CODETYPE_PPC,
		NP_Name, (ULONG)strings[MSG_LOCAL_HANDLER],
		TAG_END
		);
#endif

	if (child)
	{
		local_port = &child->pr_MsgPort;
		PutMsg(local_port, local_msg);
		WaitPort(startup_sync); GetMsg(startup_sync);
		if (sp->sp_Pkt.dp_Res1) return true;
	}
	FreeVec(sp); /* 2003-03-09 rri */
	startup_error(strings[MSG_CANT_LAUNCH_LOCAL]);
	return false;
}


void shutdown_local(void)
{
	struct StandardPacket *sp;

	sp = (struct StandardPacket *)local_msg;
	sp->sp_Pkt.dp_Port = startup_sync;
	PutMsg(local_port, local_msg);
	WaitPort(startup_sync); GetMsg(startup_sync);
	FreeVec(sp); /* 2003-03-09 rri */
	return;
}


boolean open_libraries(void)
{
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
	if (IntuitionBase)
	{
#ifdef __amigaos4__
		IIntuition = (struct IntuitionIFace *) GetInterface((struct Library*)IntuitionBase, "main", 1L, 0);
		if (IIntuition)
		{
#endif
			DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 36);
			if (DOSBase)
			{
#ifdef __amigaos4__
				IDOS = (struct DOSIFace *) GetInterface((struct Library*)DOSBase, "main", 1L, 0);
				if (IDOS)
				{
#endif
					GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0);
					if (GfxBase)
					{
#ifdef __amigaos4__
						IGraphics = (struct GraphicsIFace*) GetInterface((struct Library*)GfxBase, "main", 1L, 0);
						if (IGraphics)
						{
#endif
							/* don't care since it works without these, too */
							IconBase = OpenLibrary("icon.library", 0);
#if !defined(__MORPHOS__) && !defined(__amigaos4__)
							LocaleBase = (struct LocaleBase *) OpenLibrary("locale.library", 0); /* 03-03-02 rri */
#else
							LocaleBase = OpenLibrary("locale.library", 0); /* 12-04-04 itix */
#endif
#ifdef __amigaos4__
							if (IconBase)
								IIcon = (struct IconIFace*)GetInterface((struct Library*)IconBase, "main", 1L, 0);
							if (LocaleBase)
								ILocale = (struct LocaleIFace*)GetInterface((struct Library*)LocaleBase, "main", 1L, 0);
#endif
							return true;
#ifdef __amigaos4__
						}
						else startup_error("FTPMount cannot get graphics.library interface");
#endif
					}
					else startup_error("FTPMount cannot open graphics.library");
#ifdef __amigaos4__
					DropInterface((struct Interface*)IDOS);
				}
				else startup_error("FTPMount cannot get dos.library interface");
#endif
				CloseLibrary((struct Library *)DOSBase);
			}
			else startup_error("FTPMount requires V36 dos.library");
#ifdef __amigaos4__
			DropInterface((struct Interface*)IIntuition);
		}
		else startup_error("FTPMount cannot get intuition.library interface");
#endif
		CloseLibrary((struct Library *)IntuitionBase);
	} /* don't call startup_error() since it needs Intuitionbase! */
	return false;
}


void close_libraries(void)
{
#ifdef __amigaos4
	DropInterface((struct Interface*)ILocale);
	DropInterface((struct Interface*)IIcon);
	DropInterface((struct Interface*)IGraphics);
	DropInterface((struct Interface*)IDOS);
	DropInterface((struct Interface*)IIntuition);
#endif

#if !defined(__MORPHOS__) && !defined(__amigaos4__)
	CloseLibrary((struct Library *) LocaleBase); /* 03-03-02 rri */
#else
	CloseLibrary(LocaleBase); /* 12-04-04 itix */
#endif
	CloseLibrary(IconBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseLibrary((struct Library *)DOSBase);
	CloseLibrary((struct Library *)IntuitionBase);
}


boolean make_gims(void)
{
	struct Screen *s;
	struct DrawInfo *drawinfo;

	s = LockPubScreen(nil);
	if (!s) return false;
	lightpen = 2;
	darkpen = 1;
	textpen = 1;
	fillpen = 3;

	drawinfo = GetScreenDrawInfo(s);
	if (drawinfo)
	{
		if (drawinfo->dri_NumPens > SHADOWPEN)
		{
			lightpen = drawinfo->dri_Pens[SHINEPEN];
			darkpen = drawinfo->dri_Pens[SHADOWPEN];
			textpen = drawinfo->dri_Pens[TEXTPEN];
			fillpen = drawinfo->dri_Pens[FILLPEN];
		}
		FreeScreenDrawInfo(s, drawinfo);
	}

	/* 25/05/2005
	#ifdef __amigaos4__
	cancel_gim = make_gim(strings[MSG_CANCEL], textpen, lightpen, darkpen, s, IntuitionBase, IIntuition, GfxBase, IGraphics);
	#else
	cancel_gim = make_gim(strings[MSG_CANCEL], textpen, lightpen, darkpen, s, IntuitionBase, GfxBase);
	#endif
	*/
	cancel_gim = make_gim(strings[MSG_CANCEL], textpen, lightpen, darkpen, s, IntuitionParam, GraphicsParam);
	if (cancel_gim)
	{
		/* 25/05/2005
		#ifdef __amigaos4__
		abort_gim = make_gim(strings[MSG_ABORT], textpen, lightpen, darkpen, s, IntuitionBase, IIntuition, GfxBase, IGraphics);
		#else
		abort_gim = make_gim(strings[MSG_ABORT], textpen, lightpen, darkpen, s, IntuitionBase, GfxBase);
		#endif
		*/
		abort_gim = make_gim(strings[MSG_ABORT], textpen, lightpen, darkpen, s, IntuitionParam, GraphicsParam);
		if (abort_gim)
		{
			/* 25/05/2005
			#ifdef __amigaos4__
			disconnect_gim = make_gim(strings[MSG_DISCONNECT], textpen, lightpen, darkpen, s, IntuitionBase, IIntuition, GfxBase, IGraphics);
			#else
			disconnect_gim = make_gim(strings[MSG_DISCONNECT], textpen, lightpen, darkpen, s, IntuitionBase, GfxBase);
			#endif
			*/
			disconnect_gim = make_gim(strings[MSG_DISCONNECT], textpen, lightpen, darkpen, s, IntuitionParam, GraphicsParam);
			if (disconnect_gim)
			{
				/* 25/05/2005
				#ifdef __amigaos4__
				login_gim = make_gim(strings[MSG_LOGIN], textpen, lightpen, darkpen, s, IntuitionBase, IIntuition, GfxBase, IGraphics);
				#else
				login_gim = make_gim(strings[MSG_LOGIN], textpen, lightpen, darkpen, s, IntuitionBase, GfxBase);
				#endif
				*/
				login_gim = make_gim(strings[MSG_LOGIN], textpen, lightpen, darkpen, s, IntuitionParam, GraphicsParam);
				if (login_gim)
				{
					UnlockPubScreen(nil, s);
					return true;
				}
				/* 25/05/2005
				#ifdef __amigaos4__
				free_gim(disconnect_gim, IntuitionBase, IIntuition, GfxBase, IGraphics);
				#else
				free_gim(disconnect_gim, IntuitionBase, GfxBase);
				#endif
				*/
				free_gim(disconnect_gim, IntuitionParam, GraphicsParam);
			}
			/* 25/05/2005
			#ifdef __amigaos4__
			free_gim(abort_gim, IntuitionBase, IIntuition, GfxBase, IGraphics);
			#else
			free_gim(abort_gim, IntuitionBase, GfxBase);
			#endif
			*/
			free_gim(abort_gim, IntuitionParam, GraphicsParam);
		}
		/* 25/05/2005
		#ifdef __amigaos4__
		free_gim(cancel_gim, IntuitionBase, IIntuition, GfxBase, IGraphics);
		#else
		free_gim(cancel_gim, IntuitionBase, GfxBase);
		#endif
		*/
		free_gim(cancel_gim, IntuitionParam, GraphicsParam);
	}
	UnlockPubScreen(nil, s);
	return false;
}


void free_gims(void)
{
	/* 25/05/2005
	#ifdef __amigaos4__
		free_gim(login_gim, IntuitionBase, IIntuition, GfxBase, IGraphics);
		free_gim(disconnect_gim, IntuitionBase, IIntuition, GfxBase, IGraphics);
		free_gim(abort_gim, IntuitionBase, IIntuition, GfxBase, IGraphics);
		free_gim(cancel_gim, IntuitionBase, IIntuition, GfxBase, IGraphics);
	#else
		free_gim(login_gim, IntuitionBase, GfxBase);
		free_gim(disconnect_gim, IntuitionBase, GfxBase);
		free_gim(abort_gim, IntuitionBase, GfxBase);
		free_gim(cancel_gim, IntuitionBase, GfxBase);
	#endif
	*/
	free_gim(login_gim, IntuitionParam, GraphicsParam);
	free_gim(disconnect_gim, IntuitionParam, GraphicsParam);
	free_gim(abort_gim, IntuitionParam, GraphicsParam);
	free_gim(cancel_gim, IntuitionParam, GraphicsParam);
}


boolean get_anon_login(void)
{
#define BUFF_SIZE  100
	b8 user[BUFF_SIZE], host[BUFF_SIZE];
	sb32 i, j;
	struct EasyStruct es;

	es.es_StructSize = sizeof(struct EasyStruct);
	es.es_Flags = 0;
	es.es_Title = strings[MSG_FTPM_STARTUP_ERROR];
	es.es_GadgetFormat = strings[MSG_CONTINUE_EXIT];

	i = GetVar("USER", (STRPTR)user, BUFF_SIZE, 0);
	j = GetVar("HOST", (STRPTR)host, BUFF_SIZE, 0);

	/* four cases here */
	if (i >= 0 && j >= 0)
	{
		anon_login = (b8 *)AllocVec(i + j + 2, 0); /* 2003-03-09 rri */
		if (!anon_login) return false;

		strcpy(anon_login, user);
		anon_login[i] = '@';
		strcpy(anon_login + i + 1, host);
	}
	else if (i < 0 && j >= 0)
	{
		anon_login = (b8 *)AllocVec(j + 9, 0); /* 2003-03-09 rri */
		if (!anon_login) return false;

		strcpy((char*)anon_login, "unknown@");
		strcat((char*)anon_login, (const char*)host);

		es.es_TextFormat = strings[MSG_USER_NOT_SET];
		if (!EasyRequest(nil, &es, 0, (ULONG)anon_login))
		{
			FreeVec(anon_login); /* 2003-03-09 rri */
			return false;
		}
	}
	else if (i >= 0 && j < 0)
	{
		anon_login = (b8 *)AllocVec(i + 9, 0); /* 2003-03-09 rri */
		if (!anon_login) return false;

		strcpy((char*)anon_login, (char*)user);
		strcat((char*)anon_login, "@unknown");

		es.es_TextFormat = strings[MSG_HOST_NOT_SET];
		if (!EasyRequest(nil, &es, 0, (ULONG)anon_login))
		{
			FreeVec(anon_login); /* 2003-03-09 rri */
			return false;
		}
	}
	else
	{
		anon_login = (b8 *)AllocVec(16, 0); /* 2003-03-09 rri */
		if (!anon_login)
			return false;
		strcpy((char*)anon_login, "unknown@unknown");
		es.es_TextFormat = strings[MSG_USER_HOST_NOT_SET];
		if (!EasyRequest(nil, &es, 0, (ULONG)anon_login))
		{
			FreeVec(anon_login); /* 2003-03-09 rri */
			return false;
		}
	}
	return true;
}


boolean create_volume(void)
{
	b32 vlen;

	ftp_volume = (struct DosList *) AllocVec(sizeof(struct DosList), MEMF_PUBLIC); /* 2003-03-09 rri */
	if (ftp_volume)
	{
		ftp_volume->dol_Type = DLT_VOLUME;
		ftp_volume->dol_Task = ftp_port;
		ftp_volume->dol_Lock = 0;
		DateStamp(&ftp_volume->dol_misc.dol_volume.dol_VolumeDate);
		ftp_volume->dol_misc.dol_volume.dol_LockList = 0;
		ftp_volume->dol_misc.dol_volume.dol_DiskType = ID_DOS_DISK;

		vlen = strlen("FTPMount");
		volume_name = (b8 *)AllocVec(vlen + 2, MEMF_PUBLIC); /* 2003-03-09 rri */
		if (volume_name)
		{
			volume_name[0] = vlen;
			strcpy((char*)&volume_name[1], "FTPMount");

			ftp_volume->dol_Name = (b32)volume_name >> 2;
			if (AddDosEntry(ftp_volume)) return true;
			FreeVec(volume_name); /* 2003-03-09 rri */
		}
		FreeVec(ftp_volume); /* 2003-03-09 rri */
	}
	return false;
}


void destroy_volume(void)
{
	LockDosList(LDF_VOLUMES | LDF_DELETE | LDF_WRITE);

	if (RemDosEntry(ftp_volume))
	{
		FreeVec(volume_name); /* 2003-03-09 rri */
		FreeVec(ftp_volume); /* 2003-03-09 rri */
	}

	UnLockDosList(LDF_VOLUMES | LDF_DELETE | LDF_WRITE);
	return;
}


boolean launch_status(void)
{
	struct Process *child;

	status_mess = (status_message *)AllocVec(sizeof(*status_mess), 0); /* 2003-03-09 rri */
	if (!status_mess) return false;

	status_mess->header.mn_Length = sizeof(*status_mess);
	status_mess->header.mn_Node.ln_Name = "status startup message";
	status_mess->header.mn_Node.ln_Type = NT_MESSAGE;
	status_mess->header.mn_Node.ln_Pri = 0;

	status_control = CreatePort(0, 0);
	if (status_control)
	{
#ifndef	__MORPHOS__
		child = CreateNewProcTags(NP_Entry, (ULONG)status_handler,
			NP_Name, (ULONG)strings[MSG_STATUS_HANDLER],
			NP_StackSize, 6000,
			TAG_END, 0
			);
#else
		child = CreateNewProcTags(NP_Entry, (ULONG)status_handler,
			NP_CodeType, CODETYPE_PPC,
			NP_Name, (ULONG)strings[MSG_STATUS_HANDLER],
			TAG_END
			);
#endif

		if (child)
		{
			status_port = &child->pr_MsgPort;
			status_mess->header.mn_ReplyPort = startup_sync;
			PutMsg(status_port, &status_mess->header);
			WaitPort(startup_sync); GetMsg(startup_sync);
			if (status_mess->command != SM_KILL) return true;
		}
		DeletePort(status_control);
	}

	FreeVec(status_mess); /* 2003-03-09 rri */
	startup_error(strings[MSG_CANT_LAUNCH_STATUS]);
	return false;
}


void shutdown_status(void)
{
	status_message *sm;

	status_mess->command = SM_KILL;
	status_mess->header.mn_ReplyPort = startup_sync;

	PutMsg(status_port, &status_mess->header);

	while (1)
	{
		Wait((1 << status_control->mp_SigBit) | (1 << startup_sync->mp_SigBit));
		while (sm = (status_message *)GetMsg(status_control))
		{
			ReplyMsg(&sm->header);
		}
		if (sm = (status_message *)GetMsg(startup_sync))
		{
			FreeVec(status_mess); /* 2003-03-09 rri */
			DeletePort(status_control);
			return;
		}
	}
}


void setup_strings(void)
/* sync strings[] with .catlog provided */
{
	int i;

	my_locale = nil;
	cat = nil;

	if (LocaleBase)
	{
		my_locale = OpenLocale(0);
		if (my_locale)
		{
			cat = OpenCatalog(my_locale, "FTPMount.catalog",
				OC_BuiltInLanguage, (ULONG)"english",
				TAG_END
				);
		}
	}

	if (cat)
	{
		for (i = 0; i < NUM_MSGS; i++)
		{
			strings[i] = (b8*)GetCatalogStr(cat, i, (STRPTR)strings[i]);
		}
	}
}


void cleanup_strings(void)
{
	if (cat) CloseCatalog(cat);
	if (my_locale) CloseLocale(my_locale);
}

#ifdef __amigaos4__
void _start(void)
{
	start();
}
#endif

#ifdef	__MORPHOS__
const ULONG __abox__ = 1;

int atoi(const char *string)
{
	LONG	ret;

	StrToLong((STRPTR)string, &ret);

	return (int)ret;
}
#endif
