/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

#include "FTPMount.h" /* 03-03-02 rri */
#include "tcp.h"
#include "site.h"
#include "split.h"
#include "local.h"

#define DOSBase _local_DOSBase
struct DosLibrary *DOSBase;

#if !defined(__amigaos4__)
#define IDOS _local_IDOS
struct DOSIFace *IDOS;
#endif

BSTR ctobstr(unsigned char *s)
{
	unsigned char *z;
	int len;

	len = strlen((char*)s);

	z = (unsigned char *)allocate(len + 1, V_bstr);
	if (!z) return 0;

	z[0] = len;

	if (len > 0) {
		memcpy(&z[1], s, len);
	}

	return (BSTR)((unsigned long)z >> 2);
}

void free_bstr(BSTR b)
{
	deallocate((void *)(b << 2), V_bstr);
}

void lock_message(BPTR l, struct DosPacket *dp)
{
	struct FileLock *fl;

	fl = (struct FileLock *)(l << 2);

	PutMsg(fl->fl_Task, dp->dp_Link);
}

void SAVEDS local_handler(void)
{
	struct Process *me;
	struct Message *msg;
	struct DosPacket *dp;
	struct MsgPort *local, *reply, *sync;
	unsigned long signals;
	split sd, sd2;
	lock *locks, **slock, *new_lock, *nlock;
	unsigned long rfsl;   /* real file system lock */
	struct FileInfoBlock *fib;
	struct FileHandle *fh;
	file_info *fi;
	unsigned long o1, o2, o3, o4;     /* stores for original dp->dp_Arg1 etc */
	BSTR b, b2;

	locks = 0;

	mem_tracking_on();

	me = (struct Process *)FindTask(0l);

	local = &me->pr_MsgPort;

	WaitPort(local);
	msg = GetMsg(local);
	dp = (struct DosPacket *)msg->mn_Node.ln_Name;
	reply = dp->dp_Port;

	dp->dp_Port = local;

	sync = CreatePort(0, 0);
	if (!sync) {
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = ERROR_NO_FREE_STORE;

		PutMsg(reply, dp->dp_Link);

		return;
	}

	/* do I really need DOSBase open now that I'm packeting? */

	DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 36);
	if (!DOSBase) {
		DeletePort(sync);

		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = ERROR_INVALID_RESIDENT_LIBRARY;

		PutMsg(reply, dp->dp_Link);

		return;
	}
	else {
#ifdef __amigaos4__
		IDOS = (struct DOSIFace*)GetInterface((struct Library*)DOSBase, "main", 1L, 0);
		if (!IDOS) {
			DeletePort(sync);

			dp->dp_Res1 = DOSFALSE;
			dp->dp_Res2 = ERROR_INVALID_RESIDENT_LIBRARY;

			PutMsg(reply, dp->dp_Link);

			return;
		}
#endif
		dp->dp_Res1 = DOSTRUE;
	}
	dp->dp_Res2 = 0;

	PutMsg(reply, dp->dp_Link);

	signals = (1 << local->mp_SigBit);

	while (1) {
		Wait(signals);

		while (msg = GetMsg(local)) {
			dp = (struct DosPacket *)msg->mn_Node.ln_Name;
			reply = dp->dp_Port;

			truth(dp->dp_Link == msg);

			switch (dp->dp_Type) {
			case ACTION_DIE:
				/* close all locks */
				nlock = locks;
				while (nlock) {
					new_lock = nlock->next;

					if (nlock->rfsl != ftphosts_lock)
						UnLock(nlock->rfsl);

					disown(nlock, V_lock);
					nlock = new_lock;
				}

#ifdef __amigaos4__
				DropInterface((struct Interface*)IDOS);
#endif
				CloseLibrary((struct Library *)DOSBase);
				DeletePort(sync);

				Forbid();
				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = (unsigned long)locks;  /* so they can adopt them */

				PutMsg(reply, dp->dp_Link);

				check_memory();

				return;
			case ACTION_LOCATE_OBJECT:
				if (!split_data((lock *)(dp->dp_Arg1 << 2),
					(unsigned char *)(dp->dp_Arg2 << 2), &sd)) {
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				new_lock = (lock *)allocate(sizeof(*new_lock), V_lock);
				if (!new_lock) {
					show_string("XX2");
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;

					end_split(&sd);
					break;
				}

				if (!sd.path) {   /* they want the root */
					if (dp->dp_Arg3 == EXCLUSIVE_LOCK) {
						deallocate(new_lock, V_lock);

						end_split(&sd);

						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_OBJECT_IN_USE;
						break;
					}

					rfsl = ftphosts_lock;
				}
				else {
					o1 = dp->dp_Arg1;
					o2 = dp->dp_Arg2;

					dp->dp_Arg1 = ftphosts_lock;
					b = ctobstr(sd.path);
					if (!b) {
						deallocate(new_lock, V_lock);

						end_split(&sd);

						dp->dp_Res1 = 0;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;
						break;
					}

					dp->dp_Arg2 = b;
					dp->dp_Port = sync;

					lock_message(ftphosts_lock, dp);
					WaitPort(sync); GetMsg(sync);

					dp->dp_Arg1 = o1;
					dp->dp_Arg2 = o2;

					free_bstr(b);

					rfsl = dp->dp_Res1;
				}

				if (!rfsl) {
					deallocate(new_lock, V_lock);

					end_split(&sd);
					break;
				}

				ensure(new_lock, V_lock);

				new_lock->port = local;

				new_lock->next = locks;
				locks = new_lock;

				new_lock->rfsl = rfsl;
				new_lock->fl.fl_Access = dp->dp_Arg3;
				new_lock->fl.fl_Task = ftp_port;
				new_lock->fl.fl_Volume = (unsigned long)ftp_volume >> 2;

				end_split(&sd);

				dp->dp_Res1 = (unsigned long)new_lock >> 2;
				dp->dp_Res2 = 0;

				break;
			case ACTION_FREE_LOCK:
				slock = &locks;

				new_lock = (lock *)(dp->dp_Arg1 << 2);

				while (*slock && *slock != new_lock) {
					slock = &(*slock)->next;
				}
				if (!*slock) {
					show_string("Free lock failed");
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_LOCK;
					break;
				}

				verify(new_lock, V_lock);

				*slock = new_lock->next;

				if (new_lock->rfsl != ftphosts_lock) {
					dp->dp_Port = sync;

					o1 = dp->dp_Arg1;
					dp->dp_Arg1 = new_lock->rfsl;

					lock_message(new_lock->rfsl, dp);
					WaitPort(sync); GetMsg(sync);

					dp->dp_Arg1 = o1;
				}

				deallocate(new_lock, V_lock);

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;

				break;
			case ACTION_DELETE_OBJECT:
				if (!split_data((lock *)(dp->dp_Arg1 << 2),
					(unsigned char *)(dp->dp_Arg2 << 2), &sd)) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				o1 = dp->dp_Arg1;
				o2 = dp->dp_Arg2;

				dp->dp_Arg1 = ftphosts_lock;
				b = ctobstr(sd.path);
				if (!b) {
					end_split(&sd);
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}
				dp->dp_Arg2 = b;

				dp->dp_Port = sync;

				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg1 = o1;
				dp->dp_Arg2 = o2;

				free_bstr(b);

				end_split(&sd);

				break;
			case ACTION_RENAME_OBJECT:
				if (!split_data((lock *)(dp->dp_Arg1 << 2),
					(unsigned char *)(dp->dp_Arg2 << 2), &sd)) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				if (!split_data((lock *)(dp->dp_Arg3 << 2),
					(unsigned char *)(dp->dp_Arg4 << 2), &sd2)) {
					end_split(&sd);
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				o1 = dp->dp_Arg1;
				o2 = dp->dp_Arg2;
				o3 = dp->dp_Arg3;
				o4 = dp->dp_Arg4;

				if (!sd2.path) {  /* this is rename Unnamed1 to "ucc.gu.uwa..." */
					if (sd2.work) deallocate(sd2.work, V_cstr);
					sd2.work = (unsigned char *)allocate(strlen(sd2.port->mp_Node.ln_Name) + 1, V_cstr);
					if (!sd2.work) {
						dp->dp_Res1 = DOSFALSE;
						dp->dp_Res2 = ERROR_NO_FREE_STORE;

						end_split(&sd);
						end_split(&sd2);
						break;
					}
					strcpy((char*)sd2.work, sd2.port->mp_Node.ln_Name);
					sd2.path = sd2.work;
				}

				b = ctobstr(sd.path);
				b2 = ctobstr(sd2.path);

				if (!b || !b2) {
					if (b) free_bstr(b);
					if (b2) free_bstr(b2);

					end_split(&sd);
					end_split(&sd2);

					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;

					break;
				}

				dp->dp_Arg1 = ftphosts_lock;
				dp->dp_Arg2 = b;
				dp->dp_Arg3 = ftphosts_lock;
				dp->dp_Arg4 = b2;

				dp->dp_Port = sync;

				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg1 = o1;
				dp->dp_Arg2 = o2;
				dp->dp_Arg3 = o3;
				dp->dp_Arg4 = o4;

				free_bstr(b);
				free_bstr(b2);

				end_split(&sd);
				end_split(&sd2);

				break;
			case ACTION_COPY_DIR:
				new_lock = (lock *)(dp->dp_Arg1 << 2);
				verify(new_lock, V_lock);

				rfsl = new_lock->rfsl;

				new_lock = (lock *)allocate(sizeof(*new_lock), V_lock);
				if (!new_lock) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					show_string("DupLock failed 1");
					break;
				}

				if (rfsl != ftphosts_lock) {
					o1 = dp->dp_Arg1;
					dp->dp_Arg1 = rfsl;

					dp->dp_Port = sync;

					lock_message(rfsl, dp);
					WaitPort(sync); GetMsg(sync);

					rfsl = dp->dp_Res1;
					dp->dp_Arg1 = o1;
				}

				if (!rfsl) {
					deallocate(new_lock, V_lock);
					show_string("DupLock failed 2");
					break;
				}

				ensure(new_lock, V_lock);

				new_lock->port = local;
				new_lock->next = locks;
				locks = new_lock;

				new_lock->rfsl = rfsl;
				new_lock->fl.fl_Access = SHARED_LOCK;
				new_lock->fl.fl_Task = ftp_port;
				new_lock->fl.fl_Volume = (unsigned long)ftp_volume >> 2;

				dp->dp_Res1 = (unsigned long)new_lock >> 2;
				dp->dp_Res2 = 0;

				break;
			case ACTION_SET_PROTECT:
				if (!split_data((lock *)(dp->dp_Arg2 << 2),
					(unsigned char *)(dp->dp_Arg3 << 2), &sd)) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				o2 = dp->dp_Arg2;
				o3 = dp->dp_Arg3;

				dp->dp_Arg2 = ftphosts_lock;
				b = ctobstr(sd.path);
				if (!b) {
					end_split(&sd);

					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				dp->dp_Arg3 = b;
				dp->dp_Port = sync;

				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg2 = o2;
				dp->dp_Arg3 = o3;

				free_bstr(b);

				end_split(&sd);

				break;
			case ACTION_CREATE_DIR:
				if (!split_data((lock *)(dp->dp_Arg1 << 2),
					(unsigned char *)(dp->dp_Arg2 << 2), &sd)) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				o1 = dp->dp_Arg1;
				o2 = dp->dp_Arg2;

				dp->dp_Arg1 = ftphosts_lock;
				b = ctobstr(sd.path);
				if (!b) {
					end_split(&sd);

					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				dp->dp_Arg2 = b;

				dp->dp_Port = sync;

				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg1 = o1;
				dp->dp_Arg2 = o2;

				free_bstr(b);

				end_split(&sd);

				break;
			case ACTION_EXAMINE_OBJECT:
				new_lock = (lock *)(dp->dp_Arg1 << 2);
				fib = (struct FileInfoBlock *)(dp->dp_Arg2 << 2);

				verify(new_lock, V_lock);
				truth(fib != nil);

				o1 = dp->dp_Arg1;
				dp->dp_Arg1 = new_lock->rfsl;
				dp->dp_Port = sync;

				lock_message(new_lock->rfsl, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg1 = o1;

				if (!dp->dp_Res1) break;

				if (new_lock->rfsl == ftphosts_lock) {
					strcpy(fib->fib_FileName, (char*)volume_name);
				}

				break;
			case ACTION_EXAMINE_NEXT:
				new_lock = (lock *)(dp->dp_Arg1 << 2);
				fib = (struct FileInfoBlock *)(dp->dp_Arg2 << 2);

				verify(new_lock, V_lock);
				truth(fib != nil);

				o1 = dp->dp_Arg1;
				dp->dp_Arg1 = new_lock->rfsl;
				dp->dp_Port = sync;

				lock_message(new_lock->rfsl, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg1 = o1;

				break;
			case ACTION_SET_COMMENT:
				if (!split_data((lock *)(dp->dp_Arg2 << 2),
					(unsigned char *)(dp->dp_Arg3 << 2), &sd)) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				o2 = dp->dp_Arg2;
				o3 = dp->dp_Arg3;

				dp->dp_Arg2 = ftphosts_lock;
				b = ctobstr(sd.path);
				if (!b) {
					end_split(&sd);

					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				dp->dp_Arg3 = b;
				dp->dp_Port = sync;

				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg2 = o2;
				dp->dp_Arg3 = o3;

				free_bstr(b);

				end_split(&sd);

				break;
			case ACTION_PARENT:
				new_lock = (lock *)(dp->dp_Arg1 << 2);

				verify(new_lock, V_lock);

				if (new_lock->rfsl == ftphosts_lock) {
					dp->dp_Res1 = 0;
					dp->dp_Res2 = 0;
					break;
				}
				/* fall through */
			case ACTION_PARENT_FH:
				new_lock = (lock *)allocate(sizeof(*new_lock), V_lock);
				if (!new_lock) {
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;

					break;
				}

				ensure(new_lock, V_lock);

				new_lock->port = local;

				new_lock->next = locks;
				locks = new_lock;

				new_lock->rfsl = ftphosts_lock;
				new_lock->fl.fl_Access = SHARED_LOCK;
				new_lock->fl.fl_Task = ftp_port;
				new_lock->fl.fl_Volume = (unsigned long)ftp_volume >> 2;

				dp->dp_Res1 = (unsigned long)new_lock >> 2;
				dp->dp_Res2 = 0;

				break;
			case ACTION_SET_DATE:
				new_lock = (lock *)(dp->dp_Arg1 << 2);

				verify(new_lock, V_lock);

				o1 = dp->dp_Arg1;
				dp->dp_Arg1 = new_lock->rfsl;

				dp->dp_Port = sync;
				lock_message(new_lock->rfsl, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg1 = o1;

				break;
			case ACTION_SAME_LOCK:
				new_lock = (lock *)(dp->dp_Arg1 << 2);

				verify(new_lock, V_lock);

				if (new_lock->rfsl == ftphosts_lock) {
					new_lock = (lock *)(dp->dp_Arg2 << 2);

					verify(new_lock, V_lock);

					if (new_lock->rfsl == ftphosts_lock) {
						dp->dp_Res1 = DOSTRUE;
					}
					else {
						dp->dp_Res1 = DOSFALSE;
					}
					dp->dp_Res2 = 0;
					break;
				}

				rfsl = new_lock->rfsl;

				new_lock = (lock *)(dp->dp_Arg2 << 2);

				verify(new_lock, V_lock);

				o1 = dp->dp_Arg1;
				o2 = dp->dp_Arg2;

				dp->dp_Arg1 = rfsl;
				dp->dp_Arg2 = new_lock->rfsl;

				dp->dp_Port = sync;
				lock_message(rfsl, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg1 = o1;
				dp->dp_Arg2 = o2;

				break;
			case ACTION_READ:
			case ACTION_WRITE:
				fi = (file_info *)dp->dp_Arg1;
				verify(fi, V_file_info);

				o1 = dp->dp_Arg1;
				dp->dp_Arg1 = fi->rfarg;
				dp->dp_Port = sync;

				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg1 = o1;

				break;
			case ACTION_FINDUPDATE:
			case ACTION_FINDINPUT:
			case ACTION_FINDOUTPUT:
				if (!split_data((lock *)(dp->dp_Arg2 << 2),
					(unsigned char *)(dp->dp_Arg3 << 2), &sd)) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				fi = (file_info *)allocate(sizeof(*fi), V_file_info);
				if (!fi) {
					end_split(&sd);

					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				ensure(fi, V_file_info);

				fh = (struct FileHandle *)(dp->dp_Arg1 << 2);

				truth(fh != nil);

				o2 = dp->dp_Arg2;
				o3 = dp->dp_Arg3;

				dp->dp_Arg2 = ftphosts_lock;
				b = ctobstr(sd.path);
				if (!b) {
					deallocate(fi, V_file_info);
					end_split(&sd);

					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}
				dp->dp_Arg3 = b;

				dp->dp_Port = sync;

				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg2 = o2;
				dp->dp_Arg3 = o3;

				free_bstr(b);

				if (dp->dp_Res1) {
					fh->fh_Type = ftp_port;
					fi->rfarg = fh->fh_Args;
					fh->fh_Args = (unsigned long)fi;
					fi->port = local;
					fi->type = dp->dp_Type;
				}
				else {
					deallocate(fi, V_file_info);
				}

				end_split(&sd);

				break;
			case ACTION_END:
				fi = (file_info *)dp->dp_Arg1;

				verify(fi, V_file_info);

				o1 = dp->dp_Arg1;
				dp->dp_Arg1 = fi->rfarg;

				dp->dp_Port = sync;
				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg1 = o1;

				deallocate(fi, V_file_info);

				break;
			case ACTION_SEEK:
				fi = (file_info *)dp->dp_Arg1;

				verify(fi, V_file_info);

				o1 = dp->dp_Arg1;

				dp->dp_Arg1 = fi->rfarg;

				dp->dp_Port = sync;
				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg1 = o1;

				break;
			case ACTION_FH_FROM_LOCK:
				fh = (struct FileHandle *)(dp->dp_Arg1 << 2);
				nlock = (lock *)(dp->dp_Arg2 << 2);

				verify(nlock, V_lock);
				truth(fh != nil);

				fi = (file_info *)allocate(sizeof(*fi), V_file_info);
				if (!fi) {
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				ensure(fi, V_file_info);

				o2 = dp->dp_Arg2;

				dp->dp_Arg2 = nlock->rfsl;

				dp->dp_Port = sync;
				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				dp->dp_Arg2 = o2;

				if (dp->dp_Res1) {
					fh->fh_Type = ftp_port;
					fi->rfarg = fh->fh_Args;
					fh->fh_Args = (unsigned long)fi;
					fi->port = local;
					fi->type = dp->dp_Type;
				}
				else {
					deallocate(fi, V_file_info);
				}

				break;
			case ACTION_COPY_DIR_FH:
				fh = (struct FileHandle *)(dp->dp_Arg1);
				fi = (file_info *)fh->fh_Args;

				new_lock = (lock *)allocate(sizeof(*new_lock), V_lock);
				if (!new_lock) {
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;

					break;
				}

				ensure(new_lock, V_lock);

				new_lock->port = local;

				new_lock->fl.fl_Access = SHARED_LOCK;
				new_lock->fl.fl_Task = ftp_port;
				new_lock->fl.fl_Volume = (unsigned long)ftp_volume >> 2;

				verify(fi, V_file_info);

				fh->fh_Args = fi->rfarg;

				dp->dp_Port = sync;
				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				fh->fh_Args = (unsigned long)fi;

				if (dp->dp_Res1) {
					new_lock->rfsl = dp->dp_Res1;

					new_lock->next = locks;
					locks = new_lock;

					dp->dp_Res1 = (unsigned long)new_lock >> 2;
					dp->dp_Res2 = 0;
				}
				else {
					deallocate(new_lock, V_lock);
				}

				break;
			case ACTION_EXAMINE_FH:
				fh = (struct FileHandle *)(dp->dp_Arg1 << 2);

				fi = (file_info *)fh->fh_Args;

				verify(fi, V_file_info);

				fh->fh_Args = fi->rfarg;

				dp->dp_Port = sync;
				lock_message(ftphosts_lock, dp);
				WaitPort(sync); GetMsg(sync);

				fh->fh_Args = (unsigned long)fi;

				break;
			default:
				show_int(dp->dp_Type);
				dp->dp_Res1 = DOSFALSE;
				dp->dp_Res2 = ERROR_ACTION_NOT_KNOWN;
				break;
			}

			dp->dp_Port = ftp_port;
			PutMsg(reply, dp->dp_Link);
		}
	}
}
