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
#include "request.h"

struct DosPacket* fh_listen(void)
{
	struct DosPacket* dp;
	struct MsgPort* reply;
	struct Message* msg;
	struct InfoData* id;
	struct FileHandle* fh;
	file_info* fi;
	unsigned long signals;
	split sd, sd2;
	lock *my_lock, *lock2;
	site* my_site;
	status_message* sm;
	unsigned char *s, *name;

#ifdef __amigaos4__
	struct ExecIFace * IExec = (struct ExecIFace *)((*((struct ExecBase **) 4))->MainInterface);
#endif

	unsigned long pass_key = 0;
	boolean write_protect = false, disabled = false;

	signals = (1 << ftp_port->mp_SigBit) | (1 << status_control->mp_SigBit);

	while (1)
	{
		Wait(signals);

		while (sm = (status_message *)GetMsg(status_control))
		{
			verify(sm, V_status_message);

			switch (sm->command)
			{
			case SM_KILL:
				ReplyMsg(&sm->header);
				return nil;
			case SM_SUSPEND:
				disabled = true;

				suspend_sites();

				break;
			case SM_RESUME:
				disabled = false;
				break;
			}
			ReplyMsg(&sm->header);
		}

		while (msg = GetMsg(ftp_port))
		{
			dp = (struct DosPacket *)msg->mn_Node.ln_Name;

			reply = dp->dp_Port;

			truth(dp->dp_Link == msg);

			if (disabled && dp->dp_Type != action_IDLE)
			{
				dp->dp_Res1 = DOSFALSE;
				dp->dp_Res2 = ERROR_DEVICE_NOT_MOUNTED;

				dp->dp_Port = ftp_port;
				PutMsg(reply, dp->dp_Link);
				continue;
			}

			switch (dp->dp_Type)
			{
			case ACTION_NIL:
				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;
				break;
			case ACTION_DIE:
				return dp;
			case ACTION_LOCATE_OBJECT: /* Lock() */
				if (dp->dp_Arg3 != SHARED_LOCK && dp->dp_Arg3 != EXCLUSIVE_LOCK)
				{
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_BAD_NUMBER;
					break;
				}

				if (!split_data((lock *)(dp->dp_Arg1 << 2),
				                (unsigned char *)(dp->dp_Arg2 << 2), &sd))
				{
					/* might be ERROR_NO_FREE_STORE, but hey */
					dp->dp_Res1 = 0;
					dp->dp_Res2 = ERROR_INVALID_COMPONENT_NAME;
					break;
				}

				PutMsg(sd.port, dp->dp_Link);

				end_split(&sd);

				continue;
			case ACTION_RENAME_DISK: /* Relabel() */
				name = (unsigned char *)(dp->dp_Arg1 << 2);
				s = (unsigned char *)allocate(name[0] + 2, V_bstr);
				if (!s)
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_NO_FREE_STORE;
					break;
				}

				s[0] = name[0];
				memcpy(&s[1], &name[1], s[0]);
				s[1 + s[0]] = 0;

				/* perhaps should do some mutual exclusion here ... */
				ftp_volume->dol_Name = (unsigned long)s >> 2;

				deallocate(volume_name, V_bstr);
				volume_name = s;

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;

				break;
			case ACTION_FREE_LOCK: /* UnLock() */
			case ACTION_COPY_DIR: /* DupLock() */
			case ACTION_EXAMINE_OBJECT: /* Examine() */
			case ACTION_EXAMINE_NEXT: /* ExNext() */
			case ACTION_PARENT: /* ParentDir() */
			case ACTION_SET_DATE: /* SetFileDate() */
				my_lock = (lock *)(dp->dp_Arg1 << 2);

				if (!my_lock)
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_LOCK;
					break;
				}

				verify(my_lock, V_lock);

				PutMsg(my_lock->port, dp->dp_Link);
				continue;
			case ACTION_SET_PROTECT:
			case ACTION_SET_COMMENT:
				if (!split_data((lock *)(dp->dp_Arg2 << 2),
				                (unsigned char *)(dp->dp_Arg3 << 2), &sd))
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_COMPONENT_NAME;
					break;
				}

				PutMsg(sd.port, dp->dp_Link);

				end_split(&sd);

				continue;
			case ACTION_RENAME_OBJECT:
				if (!split_data((lock *)(dp->dp_Arg1 << 2),
				                (unsigned char *)(dp->dp_Arg2 << 2), &sd))
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_COMPONENT_NAME;
					break;
				}

				if (!split_data((lock *)(dp->dp_Arg3 << 2),
				                (unsigned char *)(dp->dp_Arg4 << 2), &sd2))
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_COMPONENT_NAME;

					end_split(&sd);
					break;
				}

				if (sd.port == local_port &&
					!sd2.path)
				{ /* special case */

					PutMsg(local_port, dp->dp_Link);

					end_split(&sd);
					end_split(&sd2);
					continue;
				}

				if (sd.port != sd2.port)
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_RENAME_ACROSS_DEVICES;

					end_split(&sd);
					end_split(&sd2);
					break;
				}

				PutMsg(sd.port, dp->dp_Link);

				end_split(&sd);
				end_split(&sd2);

				continue;
			case ACTION_DELETE_OBJECT:
				if (!split_data((lock *)(dp->dp_Arg1 << 2),
				                (unsigned char *)(dp->dp_Arg2 << 2), &sd))
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_COMPONENT_NAME;
					break;
				}

				PutMsg(sd.port, dp->dp_Link);

				end_split(&sd);

				continue;
			case ACTION_CREATE_DIR:
				if (!split_data((lock *)(dp->dp_Arg1 << 2),
				                (unsigned char *)(dp->dp_Arg2 << 2), &sd))
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_COMPONENT_NAME;
					break;
				}

				if (!sd.path)
				{
					PutMsg(local_port, dp->dp_Link);
				}
				else
				{
					PutMsg(sd.port, dp->dp_Link);
				}

				end_split(&sd);

				continue;
			case ACTION_DISK_INFO:
				id = (struct InfoData *)(dp->dp_Arg1 << 2);
				truth(id != nil);

				id->id_NumSoftErrors = 0;
				id->id_UnitNumber = 0;
				if (write_protect)
					id->id_DiskState = ID_WRITE_PROTECTED ;
				else
					id->id_DiskState = ID_VALIDATED ;
				id->id_NumBlocks = 1;
				id->id_NumBlocksUsed = 1;
				id->id_BytesPerBlock = 1024;
				id->id_DiskType = ID_DOS_DISK ;
				id->id_VolumeNode = (unsigned long)ftp_volume >> 2;
				id->id_InUse = 0;

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;

				break;
			case ACTION_INFO:
				id = (struct InfoData *)(dp->dp_Arg2 << 2);
				truth(id != nil);

				id->id_NumSoftErrors = 0;
				id->id_UnitNumber = 0;
				if (write_protect)
					id->id_DiskState = ID_WRITE_PROTECTED ;
				else
					id->id_DiskState = ID_VALIDATED ;
				id->id_NumBlocks = 1;
				id->id_NumBlocksUsed = 1;
				id->id_BytesPerBlock = 1024;
				id->id_DiskType = ID_DOS_DISK ;
				id->id_VolumeNode = (unsigned long)ftp_volume >> 2;
				id->id_InUse = 0;

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;

				break;
			case ACTION_SAME_LOCK:
				my_lock = (lock *)(dp->dp_Arg1 << 2);
				lock2 = (lock *)(dp->dp_Arg2 << 2);

				verify(my_lock, V_lock);
				verify(lock2, V_lock);

#ifdef SLDFKJ
				if (my_lock->port == local_port) {
					if (my_lock->rfsl == ftphosts_lock)
					{
						show_string("lock 1 is ROOT");
					}
					else
					{
						show_string("lock 1 is local lock");
					}
				}
				else {
					show_string(my_lock->fname);
				}

				if (lock2->port == local_port) {
					if (lock2->rfsl == ftphosts_lock)
					{
						show_string("lock 2 is ROOT");
					}
					else
					{
						show_string("lock 2 is local lock");
					}
				}
				else {
					show_string(lock2->fname);
				}
#endif

				if (my_lock->port != lock2->port)
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_LOCK;
					break;
				}

				PutMsg(my_lock->port, dp->dp_Link);
				continue;
			case ACTION_READ:
			case ACTION_WRITE:
				fi = (file_info *)dp->dp_Arg1;

				verify(fi, V_file_info);

				PutMsg(fi->port, dp->dp_Link);

				continue;
			case ACTION_FINDUPDATE:
			case ACTION_FINDINPUT:
			case ACTION_FINDOUTPUT:
				if (!split_data((lock *)(dp->dp_Arg2 << 2),
				                (unsigned char *)(dp->dp_Arg3 << 2), &sd))
				{
					/* might be ERROR_NO_FREE_STORE, but hey */
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_COMPONENT_NAME;
					break;
				}

				PutMsg(sd.port, dp->dp_Link);

				end_split(&sd);

				continue;
			case ACTION_END:
				fi = (file_info *)dp->dp_Arg1;

				verify(fi, V_file_info);

				PutMsg(fi->port, dp->dp_Link);

				continue;
			case ACTION_SEEK:
				fi = (file_info *)dp->dp_Arg1;

				verify(fi, V_file_info);

				PutMsg(fi->port, dp->dp_Link);

				continue;
			case ACTION_WRITE_PROTECT:
				if (write_protect && dp->dp_Arg1)
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_DISK_WRITE_PROTECTED;
					break;
				}
				else if (!write_protect && !dp->dp_Arg1)
				{
					dp->dp_Res1 = DOSTRUE;
					dp->dp_Res2 = 0;
					break;
				}
				else if (!write_protect && dp->dp_Arg1)
				{
					dp->dp_Res1 = DOSTRUE;
					dp->dp_Res2 = 0;
					write_protect = 1;
					pass_key = dp->dp_Arg2;
					break;
				}
				else
				{
					if (pass_key == 0 || pass_key == dp->dp_Arg2)
					{
						dp->dp_Res1 = DOSTRUE;
						dp->dp_Res2 = 0;
						write_protect = 0;
						pass_key = 0;
						break;
					}
					else
					{
						dp->dp_Res1 = DOSFALSE;
						dp->dp_Res2 = ERROR_DISK_WRITE_PROTECTED;
						break;
					}
				}
				break;
			case ACTION_FH_FROM_LOCK:
				my_lock = (lock *)(dp->dp_Arg2 << 2);

				if (!my_lock)
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_LOCK;
					break;
				}

				verify(my_lock, V_lock);

				PutMsg(my_lock->port, dp->dp_Link);
				continue;
			case ACTION_IS_FILESYSTEM:
				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;
				break;
			case ACTION_COPY_DIR_FH:
			case ACTION_PARENT_FH:
				//       case ACTION_EXAMINE_FH:
				//ABA, old            fh = (struct FileHandle *)(dp->dp_Arg1);

				//ABA, old            fi = (file_info *)fh->fh_Args;
				fi = (file_info *)(dp->dp_Arg1); /* 2006-11-27 ABA, bug fixing dospacket argument */
				verify(fi, V_file_info);
				PutMsg(fi->port, dp->dp_Link);

				continue;
			case ACTION_EXAMINE_ALL:
				my_lock = (lock *)(dp->dp_Arg1 << 2);

				if (!my_lock)
				{
					dp->dp_Res1 = DOSFALSE;
					dp->dp_Res2 = ERROR_INVALID_LOCK;
					break;
				}

				verify(my_lock, V_lock);

				PutMsg(my_lock->port, dp->dp_Link);
				continue;
			case action_IDLE:
				my_site = (site *)dp->dp_Arg1;

				dp->dp_Port = ftp_port;
				PutMsg(reply, dp->dp_Link); /* send the IDLE back */

				dp = &my_site->death_packet->sp_Pkt;
				dp->dp_Type = action_IDLE_DEATH;

				dp->dp_Port = startup_sync;
				PutMsg(my_site->port, dp->dp_Link);
				WaitPort(startup_sync);
				GetMsg(startup_sync);

				if (dp->dp_Res1)
				{
					my_lock = (lock *)dp->dp_Res2;
					while (my_lock)
					{
						adopt(my_lock, V_lock);
						my_lock = my_lock->next;
					}

					remove_site((site *)my_site);
				}

				continue;
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

void fh_ignore(void)
/* sits on our message port and cancels all actions */
{
	struct Message* msg;
	struct MsgPort* reply;
	struct DosPacket* dp;
	unsigned long signals;
	lock* l;

	signals = (1 << ftp_port->mp_SigBit);

	while (1)
	{
		Wait(signals);

		while (msg = GetMsg(ftp_port))
		{
			dp = (struct DosPacket *)msg->mn_Node.ln_Name;

			if (dp->dp_Type == ACTION_FREE_LOCK)
			{
				l = (lock *)(dp->dp_Arg1 << 2);
				verify(l, V_lock);

				deallocate(l, V_lock);

				dp->dp_Res1 = DOSTRUE;
				dp->dp_Res2 = 0;
			}
			else
			{
				dp->dp_Res1 = DOSFALSE;
				dp->dp_Res2 = ERROR_ACTION_NOT_KNOWN;
			}

			reply = dp->dp_Port;
			dp->dp_Port = ftp_port;

			PutMsg(reply, dp->dp_Link);
		}
	}
}
