/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 * AmigaOS4 port (c) 2005-2006 Alexandre BALABAN (alexandre (at) free (dot) fr)
 */

/*
 * a message passing API for amitcp
 */

#include	<dos/dos.h>

#include    <sys/select.h> /* 22-11-06 ABA : OS4/up4 */

#include "FTPMount.h" /* 03-03-02 rri */
#include "tcp.h"

#ifdef __MORPHOS__
// This is missing from the MorphOS SDK
#ifndef UNIQUE_ID
#define UNIQUE_ID	(-1)
#endif
#endif

tcpmessage *new_tcpmessage(struct MsgPort *reply_port)
{
tcpmessage *z;

z = (tcpmessage *)allocate(sizeof(*z), V_tcpmessage);
if (!z) return nil;

ensure(z, V_tcpmessage);

z->header.mn_Node.ln_Type = NT_MESSAGE;
z->header.mn_Node.ln_Pri = 0;
z->header.mn_Node.ln_Name = "TCPMessage";
z->header.mn_ReplyPort = reply_port;
z->header.mn_Length = sizeof(*z);

z->command = TCP_NOOP;
z->ident = nil;
z->address.l = 0;
z->port.w = 0;

z->data = nil;
z->interrupt = nil;
z->length = 0;
z->result = 0;
z->error = NO_ERROR;
z->flags = 0;

return z;
}


void free_tcpmessage(tcpmessage *tm)
{
verify(tm, V_tcpmessage);
ensure(tm, 0);
deallocate(tm, V_tcpmessage);
return;
}


tcpident *new_tcpident(sb32 fd)
{
tcpident *ti;

ti = (tcpident *)allocate(sizeof(*ti), V_tcpident);
if (!ti) return nil;
ensure(ti, V_tcpident);
ti->fd = fd;
ti->eof = false;
return ti;
}


void free_tcpident(tcpident *ti)
{
verify(ti, V_tcpident);
ensure(ti, 0);
deallocate(ti, V_tcpident);
return;
}


void fix_read_set(struct List *wait_list, fd_set *reads, sb32 *max_fd)
// was macht dies??
{
tcpmessage *tm;
tcpident *ti;

FD_ZERO(reads);
*max_fd = -1;

for (tm = (tcpmessage *)wait_list->lh_Head;
	  tm->header.mn_Node.ln_Succ;
	  tm = (tcpmessage *)tm->header.mn_Node.ln_Succ)
 {
  ti = tm->ident;
  verify(ti, V_tcpident);
  if (ti->fd > *max_fd)
  *max_fd = ti->fd;
  if (tm->command == TCP_LISTEN || tm->command == TCP_READ)
	{
	 FD_SET(ti->fd, reads);
	 break;
	}
 }
}


void fix_write_set(struct List *wait_list, fd_set *writes, sb32 *max_fd)
// was macht dies??
{
tcpmessage *tm;
tcpident *ti;

FD_ZERO(writes);
*max_fd = -1;

for (tm = (tcpmessage *)wait_list->lh_Head;
	  tm->header.mn_Node.ln_Succ;
	  tm = (tcpmessage *)tm->header.mn_Node.ln_Succ)
 {
  ti = tm->ident;
  verify(ti, V_tcpident);
  if (ti->fd > *max_fd) *max_fd = ti->fd;
  if (tm->command == TCP_WRITE)
	{
	 FD_SET(ti->fd, writes);
	 break;
	}
 }
}

#ifdef __amigaos4__
void non_blocking(struct Library *SocketBase, struct SocketIFace * ISocket, sb32 fd)
#else
void non_blocking(struct Library *SocketBase, sb32 fd)
#endif
{
long one;

one = 1;
IoctlSocket(fd, FIONBIO, (void *)&one);
}


void unique_name(void *tp, b8 *s, b8 *buffer)
{
	b32 task;

	task = (b32)tp;

	buffer[0] = (task >> 28) & 0xf;
	buffer[1] = (task >> 24) & 0xf;
	buffer[2] = (task >> 20) & 0xf;
	buffer[3] = (task >> 16) & 0xf;
	buffer[4] = (task >> 12) & 0xf;
	buffer[5] = (task >> 8) & 0xf;
	buffer[6] = (task >> 4) & 0xf;
	buffer[7] = task & 0xf;

	if (buffer[0] > 9) buffer[0] += 'a' - 10; else buffer[0] += '0';
	if (buffer[1] > 9) buffer[1] += 'a' - 10; else buffer[1] += '0';
	if (buffer[2] > 9) buffer[2] += 'a' - 10; else buffer[2] += '0';
	if (buffer[3] > 9) buffer[3] += 'a' - 10; else buffer[3] += '0';
	if (buffer[4] > 9) buffer[4] += 'a' - 10; else buffer[4] += '0';
	if (buffer[5] > 9) buffer[5] += 'a' - 10; else buffer[5] += '0';
	if (buffer[6] > 9) buffer[6] += 'a' - 10; else buffer[6] += '0';
	if (buffer[7] > 9) buffer[7] += 'a' - 10; else buffer[7] += '0';

	strcpy(&buffer[8], s);
}

#ifdef __amigaos4__
void tcp_read(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm, struct List *wait_list, fd_set *reads, sb32 *max_fd)
#else
void tcp_read(struct Library *SocketBase, tcpmessage *tm, struct List *wait_list, fd_set *reads, sb32 *max_fd)
#endif
{
tcpident *ti;
sb32 result=0; /* 2003-03-08 rri */
b8 *s;

truth(SocketBase != nil);
#ifdef __amigaos4__
truth(ISocket != nil);
#endif
truth(max_fd != nil);
truth(reads != nil);
truth(wait_list != nil);

verify(tm, V_tcpmessage);

DS(kprintf("  TCP_READ %ld bytes", tm->length))

ti = tm->ident;

if (!ti)
 {
  tm->result = 0;
  tm->error = ERROR_NO_CONNECTION;

  DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
  ReplyMsg(&tm->header);
  return;
 }

verify(ti, V_tcpident);

if (tm->length == 0)
 {
  tm->result = 0;
  if (ti->eof) tm->error = ERROR_EOF;
  else			tm->error = NO_ERROR;

  DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
  ReplyMsg(&tm->header);
  return;
 }

/* socket has got to be set to non-blocking */

if (tm->flags & FLAG_READLINE)
 {
  s = tm->data;
  tm->result = 0;

  while (1)
	{
			result = recv(ti->fd, s, 1, 0);
			if (result == 1)
			{
				tm->result++;
				if (*s == '\r' || *s == '\n')
				{
					if (tm->result == 1)
					{  /* blank line ... skip it */
						tm->result--;
						continue;
					}
					tm->error = NO_ERROR;

					DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
					ReplyMsg(&tm->header);
					return;
				}
				s++;
				if (tm->result == tm->length) {
					tm->error = NO_ERROR;
					DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
					ReplyMsg(&tm->header);
					return;
				}
				continue;
			} else if (result == 0)
			{  /* got to be EOF */
				ti->eof = true;
				tm->error = ERROR_EOF;

				DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
				ReplyMsg(&tm->header);
				return;
			} else
			{
				switch (Errno())
				{
				case EINTR:
				case EWOULDBLOCK: /* nothing there to read yet */
					DS(kprintf("  -> (wait_list) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
					AddTail(wait_list, (struct Node *)tm);
					FD_SET(ti->fd, reads);
					if (ti->fd > *max_fd)
						*max_fd = ti->fd;
					return;

				default: 	/* something went wrong */
					tm->error = ERROR_LOST_CONNECTION;

					DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
					ReplyMsg(&tm->header);
				}
				return;
			}
		}
	} else
	{
		result = recv(ti->fd, tm->data, tm->length, 0);
		if (result == tm->length)
		{	/* satisfied immediately */
			tm->result = tm->length;
			tm->error = NO_ERROR;

			DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
			ReplyMsg(&tm->header);
			return;
		}
	}

	if (result == 0)
	{   /* got to be EOF */
		ti->eof = true;
		tm->result = 0;
		tm->error = ERROR_EOF;

		DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
		ReplyMsg(&tm->header);
		return;
	}

	/* from here we have the stuff we have to wait for */

	if (result < 0)
	{
		switch (Errno())
		{
		case EINTR:
		case EWOULDBLOCK: /* nothing there to read yet */
			tm->result = 0;
			DS(kprintf("  -> (wait_list) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
			AddTail(wait_list, (struct Node *)tm);
			FD_SET(ti->fd, reads);
			if (ti->fd > *max_fd)
				*max_fd = ti->fd;
			return;

		default: 	/* something went wrong */
			tm->result = 0;
			tm->error = ERROR_LOST_CONNECTION;

			DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
			ReplyMsg(&tm->header);
			return;
		}
	}

	truth(result < tm->length);

	tm->result = result;
	DS(kprintf("  -> (wait_list) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
	AddTail(wait_list, (struct Node *)tm);
	FD_SET(ti->fd, reads);
	if (ti->fd > *max_fd)
		*max_fd = ti->fd;
	return;
}

#ifdef __amigaos4__
void tcp_write(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm, struct List *wait_list, fd_set *writes, sb32 *max_fd)
#else
void tcp_write(struct Library *SocketBase, tcpmessage *tm, struct List *wait_list, fd_set *writes, sb32 *max_fd)
#endif
{
	tcpident *ti;
	sb32 result;

	truth(SocketBase != nil);
    #ifdef __amigaos4__
    truth(ISocket != nil);
    #endif
	truth(max_fd != nil);
	truth(writes != nil);
	truth(wait_list != nil);

	verify(tm, V_tcpmessage);

	DS(kprintf("  TCP_WRITE %ld bytes", tm->length))

	ti = tm->ident;

	if (!ti)
	{
		tm->result = 0;
		tm->error = ERROR_NO_CONNECTION;

		DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
		ReplyMsg(&tm->header);
		return;
	}

	verify(ti, V_tcpident);

	if (tm->length == 0)
	{
		tm->result = 0;
		tm->error = NO_ERROR;

		DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
		ReplyMsg(&tm->header);
		return;
	}

	/* socket has got to be set to non-blocking */

	result = send(ti->fd, tm->data, tm->length, 0);
	if (result == tm->length)
	{   /* satisfied immediately */
		tm->result = tm->length;
		tm->error = NO_ERROR;

		DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
		ReplyMsg(&tm->header);
		return;
	}

	/* from here we have the stuff we have to wait for */

	if (result < 0)
	{
		switch (Errno())
		{
		case EWOULDBLOCK:
		case EINTR: 	/* write couldn't get through immediately */
			tm->result = 0;
			DS(kprintf("  -> (wait_list) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
			AddTail(wait_list, (struct Node *)tm);
			FD_SET(ti->fd, writes);
			if (ti->fd > *max_fd)
				*max_fd = ti->fd;
			return;

		default: 	/* something has gone wrong */
			tm->result = 0;
			tm->error = ERROR_LOST_CONNECTION;
			DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
			ReplyMsg(&tm->header);
			return;
		}
	}

	truth(result < tm->length);

	tm->result = result;
	DS(kprintf("  -> (wait_list) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
	AddTail(wait_list, (struct Node *)tm);
	FD_SET(ti->fd, writes);
	if (ti->fd > *max_fd) *max_fd = ti->fd;
	return;
}

#ifdef __amigaos4__
void tcp_read_more(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm, struct List *wait_list, fd_set *reads, sb32 *max_fd)
#else
void tcp_read_more(struct Library *SocketBase, tcpmessage *tm, struct List *wait_list, fd_set *reads, sb32 *max_fd)
#endif
{
	tcpident *ti;
	sb32 result;
	b8 *s;

	truth(SocketBase != nil);
	#ifdef __amigaos4__
	truth(ISocket != nil);
	#endif
	truth(max_fd != nil);
	truth(reads != nil);
	truth(wait_list != nil);

	verify(tm, V_tcpmessage);

	DS(kprintf("  TCP_READ more %ld bytes", tm->length))

	ti = tm->ident;
	verify(ti, V_tcpident);

	if (tm->flags & FLAG_READLINE)
	{
		s = (b8 *)tm->data + tm->result;

		while (1)
		{
			result = recv(ti->fd, s, 1, 0);
			if (result == 1)
			{
				tm->result++;
				if (*s == '\r' || *s == '\n')
				{
					if (tm->result == 1)
					{  /* blank line ... skip it */
						tm->result--;
						continue;
					}
					tm->error = NO_ERROR;

					Remove((struct Node *)tm);
					DS(kprintf("  -> (remove) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
					ReplyMsg(&tm->header);

					fix_read_set(wait_list, reads, max_fd);
					return;
				}
				s++;
				if (tm->result == tm->length)
				{
					tm->error = NO_ERROR;

					Remove((struct Node *)tm);
					DS(kprintf("  -> (remove) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
					ReplyMsg(&tm->header);

					fix_read_set(wait_list, reads, max_fd);
					return;
				}
				continue;
			} else if (result == 0)
			{  /* got to be EOF */
				ti->eof = true;
				tm->error = ERROR_EOF;

				Remove((struct Node *)tm);
				DS(kprintf("  -> (remove) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
				ReplyMsg(&tm->header);

				fix_read_set(wait_list, reads, max_fd);
				return;
			} else
			{
				switch (Errno())
				{
				case EINTR:
				case EWOULDBLOCK: /* nothing there to read yet */
					return;

				default: 	/* something went wrong */
					tm->error = ERROR_LOST_CONNECTION;

					Remove((struct Node *)tm);
					DS(kprintf("  -> (remove) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
					ReplyMsg(&tm->header);

					fix_read_set(wait_list, reads, max_fd);
					return;
				}
				return;
			}
		}
	} else
	{
		result = recv(ti->fd, (b8 *)tm->data + tm->result, tm->length - tm->result, 0);
		if (result == tm->length - tm->result) {  /* satisfied! */
			tm->result = tm->length;
			tm->error = NO_ERROR;

			Remove((struct Node *)tm); /* remove from wait_list */
			DS(kprintf("  -> (remove) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
			ReplyMsg(&tm->header);  	/* send it back completed */

			fix_read_set(wait_list, reads, max_fd);

			return;
		}

		if (result == 0) {	/* got to be EOF */
			ti->eof = true;
			tm->error = ERROR_EOF;

			Remove((struct Node *)tm); /* as above */
			DS(kprintf("  -> (remove) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
			ReplyMsg(&tm->header);

			fix_read_set(wait_list, reads, max_fd);

			return;
		}

		/* have to wait some more */

		if (result < 0)
		{
			switch (Errno())
			{
			case EINTR:
			case EWOULDBLOCK: /* nothing more to read yet */
				return;

			default: 	/* something went wrong */
				tm->error = ERROR_LOST_CONNECTION;

				Remove((struct Node *)tm);
				DS(kprintf("  -> (remove) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
				ReplyMsg(&tm->header);

				fix_read_set(wait_list, reads, max_fd);

				return;
			}
		}

		truth(result < tm->length - tm->result);

		tm->result += result;
		DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
		return;  			/* keep waiting */
	}
}

#ifdef __amigaos4__
void tcp_write_more(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm, struct List *wait_list, fd_set *writes, sb32 *max_fd)
#else
void tcp_write_more(struct Library *SocketBase, tcpmessage *tm, struct List *wait_list, fd_set *writes, sb32 *max_fd)
#endif
{
	tcpident *ti;
	sb32 result;

	truth(SocketBase != nil);
	#ifdef __amigaos4__
	truth(ISocket != nil);
	#endif
	truth(max_fd != nil);
	truth(writes != nil);
	truth(wait_list != nil);

	verify(tm, V_tcpmessage);

	DS(kprintf("  TCP_WRITE more %ld bytes", tm->length))

	ti = tm->ident;
	verify(ti, V_tcpident);

	result = send(ti->fd, (b8 *)tm->data + tm->result, tm->length - tm->result, 0);
	if (result == tm->length - tm->result)
	{  /* satisfied! */
		tm->result = tm->length;
		tm->error = NO_ERROR;

		Remove((struct Node *)tm);
		DS(kprintf("  -> (remove) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
		ReplyMsg(&tm->header);

		fix_write_set(wait_list, writes, max_fd);

		return;
	}

	/* from here we have the stuff we have to wait some more for */

	if (result < 0)
	{
		switch (Errno())
		{
		case EWOULDBLOCK:
		case EINTR: 	/* write blocked again */
			return;

		default: 	/* something has gone wrong */
			tm->error = ERROR_LOST_CONNECTION;

			Remove((struct Node *)tm);
			DS(kprintf("  -> (remove) result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
			ReplyMsg(&tm->header);

			fix_write_set(wait_list, writes, max_fd);

			return;
		}
	}

	truth(result < tm->length - tm->result);

	tm->result += result;
	DS(kprintf("  -> result = %ld ($%08lx), error = %ld\n", tm->result, tm->result, tm->error))
	return;
}

#ifdef __amigaos4__
void tcp_listen(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm, struct List *wait_list, fd_set *reads, sb32 *max_fd)
#else
void tcp_listen(struct Library *SocketBase, tcpmessage *tm, struct List *wait_list, fd_set *reads, sb32 *max_fd)
#endif
{
	tcpident *ti;
	sb32 result, socklen;
	struct sockaddr_in sin;
	struct hostent *he;
	tcpmessage *wait_tm;

	truth(SocketBase != nil);
	#ifdef __amigaos4__
	truth(ISocket != nil);
	#endif
	truth(max_fd != nil);
	truth(reads != nil);
	truth(wait_list != nil);

	verify(tm, V_tcpmessage);

	DS(kprintf("  TCP_LISTEN\n"))

	if (tm->ident)
	{
		tm->result = false;
		tm->error = ERROR_ALREADY_CONNECTED;

		ReplyMsg(&tm->header);
		return;
	}

	memset(&sin, 0, sizeof(sin));

	he = gethostbyname("localhost");
	if (!he) {
		tm->result = false;
		tm->error = ERROR_UNKNOWN_HOST;

		ReplyMsg(&tm->header);
		return;
	}

	sin.sin_family = he->h_addrtype;
	sin.sin_port = tm->port.w;
	memcpy(&sin.sin_addr.s_addr, he->h_addr_list[0], he->h_length);

	ti = new_tcpident(0);	/* filled in later */
	if (ti)
	{
		wait_tm = new_tcpmessage(nil);
		if (wait_tm)
		{
			result = socket(AF_INET, SOCK_STREAM, 0);
			if (result >= 0)
			{
				ti->fd = result;

				result = bind(ti->fd, (struct sockaddr *)&sin, sizeof(sin));
				if (result == 0)
				{
					result = listen(ti->fd, 5);	/* random msq queue length */
					if (result == 0)
					{
						/*
						 * strictly, we shouldn't need this
						 * ... but, we might (better safe etc)
						 */
                        #ifdef __amigaos4__
						non_blocking(SocketBase, ISocket, ti->fd);
                        #else
						non_blocking(SocketBase, ti->fd);
                        #endif

						socklen = sizeof(sin);
/* removed 2003-03-08 rri result = */
						getsockname(ti->fd, (struct sockaddr *)&sin, &socklen);

						/* we are listening! */
						tm->ident = ti;
						tm->port.w = sin.sin_port;

						wait_tm->ident = ti;

						wait_tm->command = TCP_LISTEN;
						wait_tm->port.w = sin.sin_port;  /* just for curiosity */

						/* note the reply port so we can send future accepts
							to the same place */
						wait_tm->header.mn_ReplyPort = tm->header.mn_ReplyPort;

						if (ti->fd > *max_fd) *max_fd = ti->fd;
						FD_SET(ti->fd, reads);
						AddTail(wait_list, (struct Node *)wait_tm);

						tm->result = true;
						tm->error = NO_ERROR;
						ReplyMsg(&tm->header);
						return;
					} else {
						tm->error = ERROR_ACCESS_DENIED;
					}
				} else {
					tm->error = ERROR_ACCESS_DENIED;
				}
				CloseSocket(ti->fd);
			} else tm->error = ERROR_OOM;
			free_tcpmessage(wait_tm);
		} else tm->error = ERROR_OOM;
		free_tcpident(ti);
	} else tm->error = ERROR_OOM;

	tm->result = false;

	ReplyMsg(&tm->header);
	return;
}


#ifdef __amigaos4__
void tcp_accept(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm, struct List *wait_list, struct MsgPort *replies)
#else
void tcp_accept(struct Library *SocketBase, tcpmessage *tm, struct List *wait_list, struct MsgPort *replies)
#endif
{
	tcpident *ti, *accept_ti;
	tcpmessage  *accept_tm, *wait_tm;
	struct sockaddr_in sin;
	sb32  	sin_len, result;

	truth(SocketBase != nil);
	#ifdef __amigaos4__
	truth(ISocket != nil);
	#endif
	truth(wait_list != nil);
	truth(replies != nil);

	verify(tm, V_tcpmessage);

	DS(kprintf("  TCP_ACCEPT\n"))

	ti = tm->ident;
	verify(ti, V_tcpident);

	sin_len = sizeof(sin);

	result = accept(ti->fd, (struct sockaddr *)&sin, &sin_len);
	if (result < 0)
	{ /* this should never really happen I don't think, but ... */
		/* nothing need be done */
		return;
	}

    #ifdef __amigaos4__
	non_blocking(SocketBase, ISocket, result);
    #else
	non_blocking(SocketBase, result);
    #endif

	accept_tm = new_tcpmessage(tm->header.mn_ReplyPort);
	if (accept_tm) {
		wait_tm = new_tcpmessage(nil);
		if (wait_tm) {
			accept_ti = new_tcpident(result);
			if (accept_ti) {
				/* all systems go! */

				/* tell the parent that we have a new connection ... */
				accept_tm->command = TCP_ACCEPTED;
				accept_tm->address.l = sin.sin_addr.s_addr;
				accept_tm->port.w = sin.sin_port;

				accept_tm->ident = accept_ti;
				accept_tm->result = true;
				accept_tm->error = NO_ERROR;

				PutMsg(tm->header.mn_ReplyPort, &accept_tm->header);

				/* keep a copy on our files for book keeping purposes */
				wait_tm->command = TCP_CONNECTED;
				wait_tm->address.l = sin.sin_addr.s_addr;
				wait_tm->port.w = sin.sin_port;

				wait_tm->ident = accept_ti;

				/* this is a bit nasty ... it has to be on the head because
					we are traversing the list forward */
				AddHead(wait_list, (struct Node *)wait_tm);
				return;
			}
			free_tcpmessage(wait_tm);
		}
		free_tcpmessage(accept_tm);
	}

	/* this is a bit sad ... it accepts and then it just closes for no
		apparent reason ... I don't see any alternative however */

	CloseSocket(result);
	return;
}

#ifdef __amigaos4__
void tcp_close(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm, struct List *wait_list, fd_set *reads, fd_set *writes, sb32 *max_fd)
#else
void tcp_close(struct Library *SocketBase, tcpmessage *tm, struct List *wait_list, fd_set *reads, fd_set *writes, sb32 *max_fd)
#endif
{
	tcpident *ti, *iti;
	tcpmessage *itm, *nitm;
	int ncons = 0, nlis = 0;

	truth(SocketBase != nil);
	#ifdef __amigaos4__
	truth(ISocket != nil);
	#endif
	truth(max_fd != nil);
	truth(reads != nil);
	truth(wait_list != nil);

	verify(tm, V_tcpmessage);

	DS(kprintf("  TCP_CLOSE\n"))

	ti = tm->ident;
	if (!ti || ti->connecting_port)
	{
		tm->result = false;
		tm->error = ERROR_NO_CONNECTION;

		ReplyMsg(&tm->header);
		return;
	}

	verify(ti, V_tcpident);

	/*
	 * search through the waiting list and:
	 * 	abort reads and writes that match
	 * 	close TCP_CONNECTEDs
	 * close TCP_LISTENs
	 */

	for (itm = (tcpmessage *)wait_list->lh_Head;
			nitm = (tcpmessage *)itm->header.mn_Node.ln_Succ;
			itm = nitm) {

		verify(itm, V_tcpmessage);

		switch (itm->command) {
		case TCP_READ:
		case TCP_WRITE:
			iti = itm->ident;
			verify(iti, V_tcpident);

			if (iti == ti) {
				itm->error = ERROR_INTERRUPTED;
				itm->ident = nil;

				Remove((struct Node *)itm);
				ReplyMsg(&itm->header);
			}
			break;
		case TCP_LISTEN:
			iti = itm->ident;
			verify(iti, V_tcpident);

			if (iti == ti)
			 {
			  nlis++;
			  Remove((struct Node *)itm);
			  free_tcpmessage(itm);
			 }
			break;
		case TCP_CONNECTED:
			iti = itm->ident;
			verify(iti, V_tcpident);

			if (iti == ti)
			 {
			  ncons++;
			  Remove((struct Node *)itm);
			  free_tcpmessage(itm);
			 }
			break;
		}
	}

truth(ncons + nlis == 1);  /* one, and only one connection!!! */

	if (ti->fd >= 0)
		CloseSocket(ti->fd);

	free_tcpident(ti);

	tm->ident = nil;
	tm->result = true;
	tm->error = NO_ERROR;

	ReplyMsg(&tm->header);

	fix_read_set(wait_list, reads, max_fd);
	fix_write_set(wait_list, writes, max_fd);

	return;
}

#ifdef __amigaos4__
void do_connect(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *mess, struct MsgPort *pport, struct MsgPort *myport)
#else
void do_connect(struct Library *SocketBase, tcpmessage *mess, struct MsgPort *pport, struct MsgPort *myport)
#endif
{
	/* bits of the following inspired by the source to NcFTP ... thanks go to Mike Gleason */
	struct sockaddr_in sin;
	struct hostent *he;
	sb32 result, s;

	truth(SocketBase != nil);
	#ifdef __amigaos4__
	truth(ISocket != nil);
	#endif
	truth(pport != nil);
	truth(myport != nil);
	verify(mess, V_tcpmessage);

	memset((void *)&sin, 0, sizeof(&sin)); /* not sure this is necessary ... ncftp does this.  JIC */

	sin.sin_port = mess->port.w;
	sin.sin_family = AF_INET;

	if (mess->data)
	{ /* if they don't send a string, their address must be right */

		sin.sin_addr.s_addr = inet_addr(mess->data);
		if (sin.sin_addr.s_addr == -1)
		{ /* not a direct dotted IP number */
			he = gethostbyname(mess->data);
			if (!he)
			{  /* oh well ... */
				mess->result = 0;
				mess->error = ERROR_UNKNOWN_HOST;

				PutMsg(pport, &mess->header);

				WaitPort(myport);
				GetMsg(myport);
				return;
			}

			sin.sin_family = he->h_addrtype;
			memcpy(&sin.sin_addr, he->h_addr_list[0], he->h_length);
		}
	} else
	{
		sin.sin_addr.s_addr = mess->address.l;
	}

	/* we have the address, now try the connect */

	s = socket(sin.sin_family, SOCK_STREAM, 0);  /* 0 for protocol legal? ncftp does it */
	if (s < 0)
	{
		mess->result = 0;
		mess->error = ERROR_OOM;

		PutMsg(pport, &mess->header);

		WaitPort(myport);
		GetMsg(myport);
		return;
	}

	result = connect(s, (struct sockaddr *)&sin, sizeof(sin));
	if (result < 0)
	{
		/* perhaps we should try the backup addresses, but nahhhh */
		switch (Errno())
		{
		case ENETDOWN:
		case ENETUNREACH:
			mess->error = ERROR_UNREACHABLE;
			break;
		case ECONNREFUSED:
			mess->error = ERROR_CONNECT_REFUSED;
			break;
		default:
			mess->error = ERROR_CANT_CONNECT;
			break;
		}

		CloseSocket(s);

		mess->result = 0;

		PutMsg(pport, &mess->header);

		WaitPort(myport);
		GetMsg(myport);
		return;
	}

	/* hurrah */

	/* now we have to transfer our socket to our parents "library domain" */

	result = ReleaseSocket(s, UNIQUE_ID);
	if (result < 0)
	{ /* BUGGER!  just as everything was going so well too :( */
		mess->result = 0;
		mess->error = ERROR_OOM;

		CloseSocket(s);
	} else {
		mess->result = result;
		mess->error = NO_ERROR;
	}

	PutMsg(pport, &mess->header);

	WaitPort(myport);
	GetMsg(myport);
	return;
}


#ifndef	__MORPHOS__
void SAVEDS ASM connect_child( REG(a0, b8 *parent_port))
#else
void connect_child(b8 *parent_port)
#endif
{
struct Library *SocketBase;
#ifdef __amigaos4__
struct SocketIFace *ISocket;
#endif
struct MsgPort *pport, *myport;
tcpmessage *mess;

Forbid(); /* 2003-03-02 rri */
pport = FindPort(parent_port);
Permit(); /* 2003-03-02 rri */
if (!pport)
 {
  /* not a fuck of a lot we can do */
  return;
 }

myport = CreatePort(0, 0);
if (myport)
 {
  mess = new_tcpmessage(myport);
  if (mess)
	{
	 SocketBase = OpenLibrary("bsdsocket.library", 0);
     #ifdef __amigaos4__
     if (SocketBase)
     {
        ISocket = (struct SocketIFace*)GetInterface( (struct Library*)SocketBase, "main", 1L, 0 );
        if (!ISocket)
        {
            CloseLibrary( SocketBase );
            SocketBase = 0;
        }
     }
     #endif
	 if (SocketBase)
	  {
		/* tell them we are going */
		mess->command = TCP_STARTUP;
		mess->result = true;
		mess->error = NO_ERROR;
		PutMsg(pport, &mess->header);
		WaitPort(myport);
		GetMsg(myport);		/* should be guaranteed to be mess back */
		pport = mess->header.mn_ReplyPort;  /* may be a different port */
		mess->command = TCP_CONNECTED;
		mess->header.mn_ReplyPort = myport; /* GOT TO BE THE SAME !!! (for ident purposes) */
        #ifdef __amigaos4__
		do_connect(SocketBase, ISocket, mess, pport, myport);
        DropInterface( (struct Interface*)ISocket );
        #else
		do_connect(SocketBase, mess, pport, myport);
        #endif
		CloseLibrary(SocketBase);
	  }
	 else
	  {
		/* tell them we are NOT going */
		mess->command = TCP_STARTUP;
		mess->result = false;
		mess->error = ERROR_NO_CONNECTION;
		PutMsg(pport, &mess->header);
		WaitPort(myport);
		GetMsg(myport);
	  }
	 free_tcpmessage(mess);
	 DeletePort(myport);
	 return;
	}
  DeletePort(myport);
 }
/* our half-hearted way of telling the parent something is wrong */
Signal(pport->mp_SigTask, 1 << pport->mp_SigBit);
return;
}

#ifdef __amigaos4__
void tcp_connect(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm, struct List *wait_list, struct MsgPort *replies)
#else
void tcp_connect(struct Library *SocketBase, tcpmessage *tm, struct List *wait_list, struct MsgPort *replies)
#endif
{
	struct Process *child;
	b8 buffer[30];
	struct MsgPort *tmp_port;
	tcpmessage *child_tm;

	unique_name(FindTask(0), ": Evans TCP Handler", buffer);

	DS(kprintf("  TCP_CONNECT %s:%ld\n", tm->data, (int)tm->port.w))

	if (tm->ident)
	{
		tm->result = false;
		tm->error = ERROR_ALREADY_CONNECTED;

		ReplyMsg(&tm->header);
		return;
	}

	tm->ident = new_tcpident(-1);
	if (!tm->ident)
	{
		tm->result = false;
		tm->error = ERROR_OOM;

		ReplyMsg(&tm->header);
		return;
	}

	tmp_port = CreatePort(buffer, 0);
	if (!tmp_port)
	{
		free_tcpident(tm->ident);
		tm->ident = nil;

		tm->result = false;
		tm->error = ERROR_OOM;

		ReplyMsg(&tm->header);
		return;
	}

	/* start child */

#ifndef	__MORPHOS__
	child = CreateNewProcTags(
		NP_Entry,	connect_child,
		NP_Name, "TCP Connect Handler",
		NP_Arguments,  buffer,
		TAG_END, 0
	);
#else
	child = CreateNewProcTags(
		NP_CodeType, CODETYPE_PPC,
		NP_Entry, (ULONG)connect_child,
		NP_Name, (ULONG)"TCP Connect Handler",
		NP_PPC_Arg1, (ULONG)buffer,
		TAG_END
	);
#endif

	if (!child)
	{
		tm->result = false;
		tm->error = ERROR_OOM;

		DeletePort(tmp_port);
		free_tcpident(tm->ident);
		tm->ident = nil;

		ReplyMsg(&tm->header);
		return;
	}

	/* the child should get back to us immediately ... so
		synchronously wait for the startup message */

	Wait(1 << tmp_port->mp_SigBit);

	child_tm = (tcpmessage *)GetMsg(tmp_port);
	if (!child_tm)
	{  /* they had a problem, and failed */
		tm->result = false;
		tm->error = ERROR_OOM;

		DeletePort(tmp_port);
		free_tcpident(tm->ident);
		tm->ident = nil;

		ReplyMsg(&tm->header);
		return;
	}

	if (!child_tm->result)
	{   /* ditto */
		tm->error = child_tm->error;

		ReplyMsg((struct Message *)child_tm);

		tm->result = false;

		DeletePort(tmp_port);
		free_tcpident(tm->ident);
		tm->ident = nil;

		ReplyMsg(&tm->header);
		return;
	}

	/* well, all should be ok now */

	DeletePort(tmp_port);

	tmp_port = child_tm->header.mn_ReplyPort;

	child_tm->header.mn_ReplyPort = replies;
	child_tm->data = tm->data;
	child_tm->flags = tm->flags;
	child_tm->port = tm->port;
	child_tm->address = tm->address;

	PutMsg(tmp_port, &child_tm->header);

	/* that will come back when the child has resolved the connect */

	((tcpident *)tm->ident)->connecting_port = tmp_port;  /* note: this is a special case for TCP_CONNECT ONLY */

	AddTail(wait_list, (struct Node *)tm);

	return;
}

#ifdef __amigaos4__
void tcp_connected(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm, struct List *wait_list)
#else
void tcp_connected(struct Library *SocketBase, tcpmessage *tm, struct List *wait_list)
#endif
{
	tcpident *ti;
	tcpmessage *itm, *nitm, *wait_tm;
	struct MsgPort *their_port;	/* to identify which child */
	sb32 s;

	truth(SocketBase != nil);
	#ifdef __amigaos4__
	truth(ISocket != nil);
	#endif
	truth(wait_list != nil);
	verify(tm, V_tcpmessage);

	DS(kprintf("  TCP_CONNECTED\n"))

	their_port = tm->header.mn_ReplyPort;

	if (tm->error == NO_ERROR)
	{
		s = ObtainSocket(tm->result, AF_INET, SOCK_STREAM, 0);	/* shudder */
	} else {
		s = -1;
	}

	/* it may have failed ... but we have to check after we have found the TCP_CONNECT */

	/* see which TCP_CONNECT in the wait list it corresponds to ... */

	for (itm = (tcpmessage *)wait_list->lh_Head;
		  nitm = (tcpmessage *)itm->header.mn_Node.ln_Succ;
		  itm = nitm)
	{
		if (itm->command == TCP_CONNECT)
		{
			ti = itm->ident;
			verify(ti, V_tcpident);

			if (ti->connecting_port == (void *)their_port)
			{
				Remove((struct Node *)itm);

				if ((tm->error != NO_ERROR) || (s < 0))
				{
					if (tm->error != NO_ERROR)
					{
						itm->result = false;
						itm->error = tm->error;
					} else
					{	 /* this case is our Obtain failing */
						itm->result = false;
						/* bizarre error for bizarre case */
						itm->error = ERROR_LOST_CONNECTION;
					}

					ReplyMsg(&tm->header);

					/* NB: reusing tm */
					tm = itm->interrupt;
					itm->interrupt = nil;

					free_tcpident(ti);
					itm->ident = nil;

					ReplyMsg(&itm->header);
					if (tm)
					{
						verify(tm, V_tcpmessage);
						ReplyMsg(&tm->header);
					}
					return;
				}

				#ifdef __amigaos4__
                non_blocking(SocketBase, ISocket, s);
                #else
                non_blocking(SocketBase, s);
                #endif

				/* ok, have to make a tcpmessage to wait here */

				wait_tm = new_tcpmessage(nil);
				if (wait_tm)
				{
					ti->connecting_port = nil;
					ti->fd = s;

					wait_tm->ident = ti;
					wait_tm->command = TCP_CONNECTED;

					wait_tm->address = tm->address;
					wait_tm->port = tm->port;

					itm->address = tm->address;
					itm->port = tm->port;

					itm->result = true;
					itm->error = NO_ERROR;

					ReplyMsg(&tm->header);

					tm = itm->interrupt;
					itm->interrupt = nil;

					ReplyMsg(&itm->header);

					if (tm)
					{
						verify(tm, V_tcpmessage);
						ReplyMsg(&tm->header);
					}

					AddTail(wait_list, (struct Node *)wait_tm);

					return;
				} else
					itm->error = ERROR_OOM;

				free_tcpident(ti);
				itm->ident = nil;

				CloseSocket(s);
				itm->result = false;

				ReplyMsg(&tm->header);

				tm = itm->interrupt;
				itm->interrupt = nil;

				ReplyMsg(&itm->header);
				if (tm)
				{
					verify(tm, V_tcpmessage);
					ReplyMsg(&tm->header);
				}

				return;
			}
		}
	}

	/* hmmm, strange things are happening */
	if (s >= 0) CloseSocket(s);

	ReplyMsg(&tm->header);
	return;
}



void tcp_interrupt(tcpmessage *tm, struct List *wait_list, fd_set *reads, fd_set *writes, sb32 *max_fd)
{
	tcpident *ti;
	tcpmessage *itm, *nitm;
	struct MsgPort *port;

	verify(tm, V_tcpmessage);

	DS(kprintf("  TCP_INTERRUPT\n"))

	for (itm = (tcpmessage *)wait_list->lh_Head;
		  nitm = (tcpmessage *)itm->header.mn_Node.ln_Succ;
		  itm = nitm)
	{

		verify(itm, V_tcpmessage);

		if (itm == tm->interrupt)
		{
			switch (itm->command)
			{
			case TCP_READ:
				Remove((struct Node *)itm);

				itm->error = ERROR_INTERRUPTED;
				ReplyMsg(&itm->header);

				tm->result = true;
				tm->error = NO_ERROR;

				ReplyMsg(&tm->header);

				fix_read_set(wait_list, reads, max_fd);
				return;

			case TCP_WRITE:
				Remove((struct Node *)itm);

				itm->error = ERROR_INTERRUPTED;
				ReplyMsg(&itm->header);

				tm->result = true;
				tm->error = NO_ERROR;

				ReplyMsg(&tm->header);

				fix_write_set(wait_list, writes, max_fd);
				return;

			case TCP_CONNECT:
				/* this is the fun one */
				ti = itm->ident;
				verify(ti, V_tcpident);

				port = ti->connecting_port;

				itm->interrupt = tm;
				Signal(port->mp_SigTask, SIGBREAKF_CTRL_C);
				return;
			}
		}
	}

	tm->result = false;
	tm->error = ERROR_NO_CONNECTION;

	ReplyMsg(&tm->header);
	return;
}

#ifdef __amigaos4__
void tcp_peername(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm)
#else
void tcp_peername(struct Library *SocketBase, tcpmessage *tm)
#endif
/*
 * this one is a strange one ... it may have the potential to block
 * I don't really want to write a bloody child process thingy just for
 * this trivial bloody function
 */
{
	tcpident *ti;
	struct sockaddr_in sin;
	struct hostent *he;
	sb32 sin_len, len;

	truth(SocketBase != nil);
	#ifdef __amigaos4__
	truth(ISocket != nil);
	#endif
	verify(tm, V_tcpmessage);

	DS(kprintf("  TCP_PEERNAME\n"))

	ti = tm->ident;
	if (!ti)
	{
		tm->result = false;
		tm->error = ERROR_NO_CONNECTION;

		ReplyMsg(&tm->header);
		return;
	}

	verify(ti, V_tcpident);

	sin_len = sizeof(sin);

	if (getpeername(ti->fd, (struct sockaddr *)&sin, &sin_len) < 0)
	{
		if (Errno() == ENOTCONN)
		{
			tm->error = ERROR_NO_CONNECTION;
		} else {
			tm->error = ERROR_OOM;
		}
		tm->result = false;

		ReplyMsg(&tm->header);
		return;
	}

	he = gethostbyaddr((void *)&sin.sin_addr.s_addr, 4, AF_INET);
	if (!he)
	{
		tm->error = ERROR_UNKNOWN_HOST;
		tm->result = false;

		ReplyMsg(&tm->header);
		return;
	}

	len = strlen(he->h_name);
	if (len >= tm->length)
	{
		len = tm->length - 1;
	}

	memcpy(tm->data, he->h_name, len);
	((b8 *)tm->data)[len] = 0;

	tm->error = NO_ERROR;
	tm->result = true;

	ReplyMsg(&tm->header);
	return;
}

#ifdef __amigaos4__
void tcp_service(struct Library *SocketBase, struct SocketIFace * ISocket, tcpmessage *tm)
#else
void tcp_service(struct Library *SocketBase, tcpmessage *tm)
#endif
{
	struct servent *se;

	truth(SocketBase != nil);
	#ifdef __amigaos4__
	truth(ISocket != nil);
	#endif
	verify(tm, V_tcpmessage);

	se = getservbyname(tm->data, "tcp");
	if (!se)
	{
		tm->result = false;
		tm->error = ERROR_UNKNOWN_COMMAND;
	} else
	{
		tm->result = true;
		tm->error = NO_ERROR;

		tm->port.w = se->s_port;
	}

	DS(kprintf("  TCP_SERVICE %s -> %ld\n", tm->data, tm->port.w))

	ReplyMsg(&tm->header);
	return;
}



struct MsgPort *running_running(tcpmessage *emergency, struct MsgPort *commands)
{
	b32		tcp_signal, signal_tmp;
	tcpmessage  *tm, *tm2;
	tcpident 	*ti;
	struct Library *SocketBase = nil;
    #ifdef __amigaos4__
    struct SocketIFace * ISocket = nil;
    #endif
	struct List waiting;
	fd_set	read_set, write_set, read_tmp, write_tmp;
	sb32  	max_fd, n;
	struct MsgPort *death_port;

	NewList(&waiting);	/* list of waiting requests */

	FD_ZERO(&read_set);
	FD_ZERO(&write_set);

	max_fd = -1;

	tcp_signal = (1 << commands->mp_SigBit);

	while (1)
	{
        #ifdef __amigaos4__
        if ( ISocket )
        #else
		if (SocketBase)
        #endif
		{
			read_tmp = read_set;
			write_tmp = write_set;
			signal_tmp = tcp_signal;

			n = WaitSelect(max_fd + 1, &read_tmp, &write_tmp, nil, nil, &signal_tmp);
		} else
		{
			n = 0;
			Wait(tcp_signal);
		}

		while (tm = (tcpmessage *)GetMsg(commands))
		{
			/* a message from our parent (sponsor?) */

			verify(tm, V_tcpmessage);

			if (tm->header.mn_ReplyPort == commands)
			{
				/* its been ReplyMsg'd to us */
				free_tcpmessage(tm);
				continue;
			}

			switch (tm->command)
			{
			case TCP_NOOP:
				DS(kprintf("  TCP_NOOP\n"))
				tm->result = true;
				ReplyMsg(&tm->header);
				break;

			case TCP_CONNECT:
				if (!SocketBase)
				{
					SocketBase = OpenLibrary("bsdsocket.library", 0);
                    #ifdef __amigaos4__
                    if (SocketBase)
                    {
                        ISocket = (struct SocketIFace*)GetInterface( (struct Library*)SocketBase, "main", 1L, 0 );
                        if (!ISocket)
                        {
                            CloseLibrary( SocketBase );
                            SocketBase = 0;
                        }
                    }
                    #endif
					if (!SocketBase)
					{
						tm->result = false;
						tm->error = ERROR_NO_CONNECTION;

						ReplyMsg(&tm->header);
						break;
					}
				}
                #ifdef __amigaos4__
				tcp_connect(SocketBase, ISocket, tm, &waiting, commands);
				#else
                tcp_connect(SocketBase, tm, &waiting, commands);
                #endif
				break;

			case TCP_LISTEN:
				if (!SocketBase)
				{
					SocketBase = OpenLibrary("bsdsocket.library", 0);
                    #ifdef __amigaos4__
                    if (SocketBase)
                    {
                        ISocket = (struct SocketIFace*)GetInterface( (struct Library*)SocketBase, "main", 1L, 0 );
                        if (!ISocket)
                        {
                            CloseLibrary( SocketBase );
                            SocketBase = 0;
                        }
                    }
                    #endif
					if (!SocketBase)
					{
						tm->result = false;
						tm->error = ERROR_NO_CONNECTION;

						ReplyMsg(&tm->header);
						break;
					}
				}
                #ifdef __amigaos4__
				tcp_listen(SocketBase, ISocket, tm, &waiting, &read_set, &max_fd);
				#else
                tcp_listen(SocketBase, tm, &waiting, &read_set, &max_fd);
				#endif
                break;

			case TCP_READ:
				if (!SocketBase)
				{   /* someone is frigging us around */
					tm->result = 0;
					tm->error = ERROR_NO_CONNECTION;

					ReplyMsg(&tm->header);
					break;
				}
				#ifdef __amigaos4__
                tcp_read(SocketBase, ISocket, tm, &waiting, &read_set, &max_fd);
                #else
                tcp_read(SocketBase, tm, &waiting, &read_set, &max_fd);
                #endif
				break;

			case TCP_WRITE:
				if (!SocketBase)
				{   /* someone is frigging us around */
					tm->result = 0;
					tm->error = ERROR_NO_CONNECTION;

					ReplyMsg(&tm->header);
					break;
				}
				#ifdef __amigaos__
                tcp_write(SocketBase, ISocket, tm, &waiting, &write_set, &max_fd);
                #else
                tcp_write(SocketBase, tm, &waiting, &write_set, &max_fd);
                #endif
				break;

			case TCP_CLOSE:
				if (!SocketBase)
				{   /* someone is frigging us around */
					tm->result = false;
					tm->error = ERROR_NO_CONNECTION;

					ReplyMsg(&tm->header);
					break;
				}
				#ifdef __amigaos4__
                tcp_close(SocketBase, ISocket, tm, &waiting, &read_set, &write_set, &max_fd);
				#else
                tcp_close(SocketBase, tm, &waiting, &read_set, &write_set, &max_fd);
                #endif

				if (IsListEmpty(&waiting))
				{
					/* absolutely everything has closed.  We can close
						the socket library now in safety */
					#ifdef __amigaos4__
                    DropInterface( (struct Interface*)ISocket);
                    ISocket = nil;
                    #endif
                    CloseLibrary(SocketBase);
					SocketBase = nil;
				}
				break;

			case TCP_CONNECTED:
				/* ahhh, a child is reporting back! */

				if (!SocketBase)
				{   /* someone is frigging us around */
					tm->result = false;
					tm->error = ERROR_NO_CONNECTION;

					ReplyMsg(&tm->header);
					break;
				}

                #ifdef __amigaos4__
				tcp_connected(SocketBase, ISocket, tm, &waiting);
				#else
                tcp_connected(SocketBase, tm, &waiting);
                #endif

				if (IsListEmpty(&waiting))
				{
					/* absolutely everything has closed.  We can close
						the socket library now in safety */
					#ifdef __amigaos4__
                    DropInterface( (struct Interface*)ISocket);
                    ISocket = nil;
                    #endif
					CloseLibrary(SocketBase);
					SocketBase = nil;
				}
				break;

			case TCP_INTERRUPT:
				tcp_interrupt(tm, &waiting, &read_set, &write_set, &max_fd);
				break;

			case TCP_NEWMESSAGE:
				tm2 = new_tcpmessage(tm->header.mn_ReplyPort);
				if (!tm2)
				{
					tm->result = false;
					tm->error = ERROR_OOM;
					ReplyMsg(&tm->header);
					break;
				}

				tm2->ident = tm->ident;

				tm->data = tm2;
				tm->result = true;
				tm->error = NO_ERROR;

				ReplyMsg(&tm->header);
				break;

			case TCP_DISPOSE:
				free_tcpmessage(tm);
				break;

			case TCP_DIE:
				/* we have to assume they've done the right thing and
					disposed of all messages (closing all connections) */
				/* all we have to do is to free whatever is left over
					and exit */

				death_port = tm->header.mn_ReplyPort;

				free_tcpmessage(tm);

				if (SocketBase)
				{
					#ifdef __amigaos4__
                    DropInterface( (struct Interface*)ISocket);
                    ISocket = nil;
                    #endif
					CloseLibrary(SocketBase);
					SocketBase = nil;
				}

				while (tm = (tcpmessage *)RemHead(&waiting))
				{
					if (tm->ident && tm->command != TCP_CONNECT)
						free_tcpident(tm->ident);
					free_tcpmessage(tm);
				}

				/* perhaps should do a little more ... */

				return death_port;

			case TCP_PEERNAME:
				if (!SocketBase)
				{   /* someone is yankin' our chain */
					tm->result = false;
					tm->error = ERROR_NO_CONNECTION;

					ReplyMsg(&tm->header);
					break;
				}
                #ifdef __amigaos4__
				tcp_peername(SocketBase, ISocket, tm);
                #else
				tcp_peername(SocketBase, tm);
                #endif
				break;

			case TCP_SERVICE:
				if (!SocketBase)
				{
					SocketBase = OpenLibrary("bsdsocket.library", 0);
                    #ifdef __amigaos4__
                    if (SocketBase)
                    {
                        ISocket = (struct SocketIFace *)GetInterface( SocketBase, "main", 1L, 0 );
                        if (!ISocket)
                        {
                            CloseLibrary( SocketBase );
                            SocketBase = 0;
                        }
                    }
                    #endif
					if (!SocketBase)
					{
						tm->result = false;
						tm->error = ERROR_NO_CONNECTION;

						ReplyMsg(&tm->header);
						break;
					}
				}
				#ifdef __amigaos4__
                tcp_service(SocketBase, ISocket, tm);
                #else
                tcp_service(SocketBase, tm);
                #endif
				if (IsListEmpty(&waiting))
				{
					/* absolutely everything has closed.  We can close
						the socket library now in safety */
		 			#ifdef __amigaos4__
                    DropInterface((struct Interface*)ISocket);
                    ISocket = nil;
                    #endif
					CloseLibrary(SocketBase);
					SocketBase = nil;
				}
				break;

			default:
				tm->result = false;
				tm->error = ERROR_UNKNOWN_COMMAND;
				ReplyMsg(&tm->header);
				break;
			} // switch
		} // while

		if (n < 0)
		{	/* hmmm ... this seems to happen when they close the library underneath us */
			/* should send back any waiting reads/writes/listens */
		 	#ifdef __amigaos4__
            DropInterface((struct Interface*)ISocket);
            ISocket = nil;
            #endif
			CloseLibrary(SocketBase);
			SocketBase = nil;

			continue;
		}

#ifdef SDLFKJ
		truth (n >= 0);		/* I don't _think_ we should ever get SIGINTR ... */
#endif

		if (n <= 0)
			continue;	/* no fds are ready, so don't look through list */

		for (tm = (tcpmessage *)waiting.lh_Head;
				tm2 = (tcpmessage *)tm->header.mn_Node.ln_Succ;
				tm = tm2)
		{

			switch (tm->command)
			{
			case TCP_LISTEN:
				ti = tm->ident;
				verify(ti, V_tcpident);

				if (FD_ISSET(ti->fd, &read_tmp))
				{  /* ready to accept */
                    #ifdef __amigaos4__
					tcp_accept(SocketBase, ISocket, tm, &waiting, commands);
                    #else
					tcp_accept(SocketBase, tm, &waiting, commands);
                    #endif
				}
				break;

			case TCP_READ:
				ti = tm->ident;
				verify(ti, V_tcpident);

				if (FD_ISSET(ti->fd, &read_tmp))
				{  /* ready to continue reading */
					#ifdef __amigaos4__
                    tcp_read_more(SocketBase, ISocket, tm, &waiting, &read_set, &max_fd);
					#else
                    tcp_read_more(SocketBase, tm, &waiting, &read_set, &max_fd);
                    #endif
				}
				break;

			case TCP_WRITE:
				ti = tm->ident;
				verify(ti, V_tcpident);

				if (FD_ISSET(ti->fd, &write_tmp))
				{ /* ready to continue writing */
					#ifdef __amigaos4__
                    tcp_write_more(SocketBase, ISocket, tm, &waiting, &write_set, &max_fd);
                    #else
                    tcp_write_more(SocketBase, tm, &waiting, &write_set, &max_fd);
                    #endif
				}
				break;
			} // switch
		} // for (tm)
	} // while (1)
}


#ifndef	__MORPHOS__
void SAVEDS ASM tcp_handler( REG(a0, b8 *parent_port))
#else
void tcp_handler(b8 *parent_port)
#endif
{
	struct MsgPort *mp, *tcp_commands;
	tcpmessage *tm, *new_tm;

	DS(kprintf("--- tcp_handler startup (parent port %s)\n", parent_port))

	mem_tracking_on();

	Forbid(); /* 2003-03-02 rri */
	mp = FindPort(parent_port);
	Permit(); /* 2003-03-02 rri */

	if (!mp)
	{
		Forbid(); /* 2003-03-02 rri */
		truth(FindPort(parent_port) != nil);	/* will display an alert */
		Permit(); /* 2003-03-02 rri */
		return;  /* OOOPS - not overly much we can do here */
	}

	DS(kprintf("--- tcp_handler - parent port found\n")) /* 2003-03-09 rri */

	tcp_commands = CreatePort(0, 0);

	if (!tcp_commands)
	{
		/* this is a signal jobbie because we can't send a message so when they
			wait for the signal, and then can't GetMsg they should guess
			that something has gone wrong */

		Signal(mp->mp_SigTask, 1 << mp->mp_SigBit);
		return;
	}

	tm = new_tcpmessage(tcp_commands);
	if (!tm)
	{
		/* again ... our message doesn't exist, so just signal */

		Signal(mp->mp_SigTask, 1 << mp->mp_SigBit);
		DeletePort(tcp_commands);
		return;
	}

	new_tm = new_tcpmessage(tcp_commands);

	if (!new_tm)
	{
		Signal(mp->mp_SigTask, 1 << mp->mp_SigBit);
		DeletePort(tcp_commands);
		free_tcpmessage(tm);
		return;
	}

	new_tm->command = TCP_NEWMESSAGE;
	new_tm->header.mn_ReplyPort = mp;
	/* just a guess ... if they want something different, they'se gonna haveta do it themselves */
	/* ok, we have enough to communicate ... tell them we started ok */

	tm->command = TCP_STARTUP;
	tm->data = tcp_commands;	/* this tells them where to send commands */
	tm->result = true;			/* not necessary but wth */

	PutMsg(mp, &tm->header);

	/* wait for it to come back */

	Wait(1 << tcp_commands->mp_SigBit);
	GetMsg(tcp_commands);	/* should be guaranteed to be tm back ;) */

	/* ok, time to carry on */
	/* send them a message to bootstrap their message system */

	PutMsg(mp, &new_tm->header);

	/* Reusing mp here for the death port */

	mp = running_running(tm, tcp_commands);

	DeletePort(tcp_commands);
	free_tcpmessage(tm);

	check_memory();

	Forbid();	/* once we are RemTask'd the Forbid will be lifted */

	Signal(mp->mp_SigTask, 1 << mp->mp_SigBit);  /* signal parent we are dead */

	return;
}
