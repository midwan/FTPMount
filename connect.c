/*
 * This source file is Copyright 1995 and 1996 by Evan Scott, apart from those sections which are
 * explicitly described as otherwise.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

#include "FTPMount.h" /* 03-03-02 rri */

#include <ctype.h>                     // for isdigit()  (RJF)
#include "tcp.h"
#include "site.h"
#include "split.h"
#include "ftpinfo.h"
#include "connect.h"
#include "request.h"
#include "strings.h"

#define ERROR_GARBAGE_RECEIVED 15

extern ftpinfo *get_info(site *, unsigned char *);

unsigned char *grow_info(unsigned char *a, unsigned char *b, int n)
/*
 * concatenates a and b (not null terminated of length n) and returns
 * an allocated string with the result.  a is freed.
 *    a may be nil
 *    b may not
 */
{
	unsigned char *c, *d;
	int len;

	if (a)
		len = strlen(a);
	else
		len = 0;

	len += n + 1;

	c = (unsigned char *)allocate(len, V_cstr);
	if (!c)
	{
		if (a)
			deallocate(a, V_cstr);
		return nil;
	}

	if (a)
	{
		strcpy(c, a);
		d = c + strlen(c);
	}
	else
	{
		d = c;
	}

	if (n)
		memcpy(d, b, n);
	d[n] = 0;

	if (a)
	{
		deallocate(a, V_cstr);
	}

	a = c;
	while (*a)
	{
		if (*a == '\t') *a = ' ';  /* hmmm */
		if (*a == '\r') *a = '\n';
		a++;
	}

	return c;
}

boolean substr(unsigned char *s, unsigned char *ss)
/*
 * returns true if s contains ss (non-case-sensitive)
 * s may be nil, in which case false is returned
 */
{
	int len;

	if (s == nil) return false;

	len = strlen(ss);

	while (*s) {
		if (strncasecmp(s, ss, len) == 0) return true;
		s++;
	}

	return false;
}

void inform(struct IntuitionBase *IntuitionBase, unsigned char *title, unsigned char *text, unsigned char *site, unsigned long errno)
{
	struct EasyStruct es;

	es.es_StructSize = sizeof(struct EasyStruct);
	es.es_Flags = 0;
	es.es_Title = title;
	es.es_GadgetFormat = strings[MSG_OK];
	es.es_TextFormat = text;

	EasyRequest(nil, &es, nil, site, errno);
}

tcpmessage *new_message(site *sp)
/*
 * get a new tcpmessage
 */
{
	tcpmessage *intr;
	struct MsgPort *sync;

	verify(sp, V_site);

	intr = sp->intr;
	verify(intr, V_tcpmessage);

	sync = sp->sync;

	/* "re-use" intr to get a new tcpmessage */
	intr->command = TCP_NEWMESSAGE;
	intr->header.mn_ReplyPort = sync;

	PutMsg(tcp, &intr->header);
	WaitPort(sync); GetMsg(sync); /* this _should_ never block */

	return (tcpmessage *)intr->data;
}

void interrupt_message(site *sp, tcpmessage *tm)
/*
 * interrupt tm
 */
{
	tcpmessage *intr;
	struct MsgPort *sync;

	verify(sp, V_site);
	verify(tm, V_tcpmessage);

	intr = sp->intr;
	verify(intr, V_tcpmessage);

	sync = sp->sync;

	intr->command = TCP_INTERRUPT;
	intr->header.mn_ReplyPort = sync;
	intr->interrupt = tm;

	PutMsg(tcp, &intr->header);

	/*
	 * NB: I could probably assume tm->header.mn_ReplyPort == sync safely, but
	 * this seems a bit more "correctly generic" (although potentially more buggy)
	 */
	WaitPort(tm->header.mn_ReplyPort); GetMsg(tm->header.mn_ReplyPort);  /* aborted tm coming back */
	WaitPort(sync); GetMsg(sync); /* intr coming back successful */

	return;
}

unsigned long control_write(site *sp, unsigned char *command, unsigned long csig)
/*
 * writes the string command to the control connection
 * Inputs:
 *    sp : site pointer
 * command : null terminated command string
 * csig  : additional cancel signals, 0 is ok
 *
 * returns the tcp error
 */
{
	tcpmessage *tm;
	struct MsgPort *sync;
	unsigned long signals, rsigs;

	verify(sp, V_site);
	truth(command != nil);

	// truth(sp->connected);

	tm = sp->control;
	verify(tm, V_tcpmessage);

	sync = sp->sync;

	// kein \n am Ende, da Kommando dies enthält
	// Achtung: hier wird auch "PASS xx" ausgegeben!
	DS(kprintf("TCP control_write (site %s) %s", sp->host, command))

		tm->command = TCP_WRITE;
	tm->length = strlen(command);
	tm->flags = 0;
	tm->data = command;
	tm->header.mn_ReplyPort = sync;

	csig |= sp->abort_signals | sp->disconnect_signals;
	signals = (1 << sync->mp_SigBit) | csig;

	PutMsg(tcp, &tm->header);
	do
	{
		rsigs = Wait(signals);
		if (rsigs & csig)
		{
			interrupt_message(sp, tm);

			if (rsigs & sp->disconnect_signals)
			{
				disconnect(sp);
			}

			return ERROR_INTERRUPTED;
		}
	} while (!GetMsg(sync));

	return tm->error;
}

unsigned long make_connection(site *sp, tcpmessage *tm, unsigned char *addr, unsigned short port, unsigned long csig)
/*
 * make a connection to a remote host
 * Inputs:
 * sp : site pointer
 * tm : an unused tcpmessage
 * addr  : null terminated string address
 * port  : port number to connect to
 * csig  : additional cancel signals (may be 0)
 *
 * Returns:
 *    standard tcp error
 */
{
	struct MsgPort *sync;
	unsigned long signals, rsigs, asigs;

	verify(sp, V_site);
	verify(tm, V_tcpmessage);
	truth(addr != nil);

	sync = sp->sync;

	tm->header.mn_ReplyPort = sync;
	tm->command = TCP_CONNECT;
	tm->data = addr;
	tm->port.w = port;
	tm->flags = 0;

	asigs = csig | sp->abort_signals | sp->disconnect_signals;
	signals = asigs | (1 << sync->mp_SigBit);

	PutMsg(tcp, &tm->header);
	do
	{
		rsigs = Wait(signals);
		if (rsigs & asigs)
		{
			interrupt_message(sp, tm);

			if (rsigs & sp->disconnect_signals)
			{
				disconnect(sp);
			}

			return ERROR_INTERRUPTED;
		}
	} while (!GetMsg(sync));

	return tm->error;
}

void break_connection(site *sp, tcpmessage *tm)
/*
 * do a TCP_CLOSE on tm
 */
{
	struct MsgPort *sync;

	verify(sp, V_site);
	verify(tm, V_tcpmessage);

	sync = sp->sync;

	tm->command = TCP_CLOSE;
	tm->header.mn_ReplyPort = sync;

	PutMsg(tcp, &tm->header);
	WaitPort(sync); GetMsg(sync);

	return;
}

boolean passive_response(unsigned char *s, unsigned char *addr, unsigned short *portp)
/*
 * parse the response to a PASV command
 * Inputs:
 * s  : the response to the PASV (null terminated)
 * addr  : a buffer to hold the address (should be as long as s)
 * portp : where to put the port number
 *
 * Returns:
 * true if it was a valid PASV response
 */
{
	unsigned char *t;
	unsigned short ncommas, portn;

	truth(s != nil);
	truth(addr != nil);
	truth(portp != nil);

	// Antwortstring enthält \n
	DS(kprintf("passive_response %s", s))

		while (*s && *s != '(')
		s++;
	if (!*s)
		return false;

	/* first calculate port number ... skip the first 4 commas */

	ncommas = 0;
	t = s;
	while (*t && *t != ')' && ncommas < 4)
	{
		if (*t == ',')
			ncommas++;
		t++;
	}

	portn = atoi(t) * 256;  /* possibly a more thorough check of whether these are legit numbers */
	while (*t && *t != ',' && *t != ')')
		t++;
	if (*t == ',')
		portn += atoi(t + 1);

	/*
	 * now copy the first 4 fields to addr, changing commas to periods
	 * (hopefully making a legitimate ip address)
	 */

	DS(t = addr)

		ncommas = 0;
	s++;     /* move s past the '(' */
	while (*s && ncommas < 4)
	{
		if (*s == ',')
		{
			ncommas++;
			if (ncommas == 4)
				*addr = 0;
			else
				*addr++ = '.';
			s++;
		}
		else
		{
			*addr++ = *s++;
		}
	}

	*portp = portn;

	DS(kprintf("  -> %s:%ld\n", t, (int)portn))

		return true;
}

unsigned long response(site *sp, unsigned long csig, unsigned char **infop, unsigned char *code)
/*
 * reads response from remote server on sp->control
 * Inputs:
 * sp : site pointer
 * csig  : cancel signals (in addition to standard sp->abort etc ... usually a window) 0 is ok.
 * infop : pointer to a string pointer to store the servers response message
 * code  : 3 byte array for the response code (eg 257 "/usr/dm/pathname" directory created)
 *
 * Returns:
 * standard tcp error code with the additional error ERROR_GARBAGE_RECEIVED
 */
{
	tcpmessage *tm;
	struct MsgPort *sync;
	unsigned long signals, rsigs, asigs;
	unsigned char *info, *z;

	verify(sp, V_site);

	truth(code != nil);
	truth(infop != nil);

	*infop = nil;

	tm = sp->control;
	sync = sp->sync;

	verify(tm, V_tcpmessage);

	asigs = csig | sp->disconnect_signals | sp->abort_signals;  /* abort signals */
	signals = asigs | (1 << sync->mp_SigBit);

	tm->command = TCP_READ;
	tm->flags = FLAG_READLINE;
	tm->data = sp->read_buffer;
	tm->length = READ_BUFFER_LENGTH;
	tm->header.mn_ReplyPort = sync;

	PutMsg(tcp, &tm->header);
	do
	{
		rsigs = Wait(signals);
		if (rsigs & asigs)
		{
			state_change(sp, SS_ABORTING);

			interrupt_message(sp, tm);

			/* cancelling the read of the response is guaranteed fatal anyway ... but ... */

			if (rsigs & sp->disconnect_signals) {
				disconnect(sp);
			}

			return ERROR_INTERRUPTED;
		}
	} while (!GetMsg(sync));

	if (tm->error != NO_ERROR) {
		show_int(tm->error);
		return tm->error;
	}

	z = sp->read_buffer;

	if (tm->result < 4 ||
		z[0] < '0' || z[0] > '9' ||
		z[1] < '0' || z[1] > '9' ||
		z[2] < '0' || z[2] > '9')
	{
		show_int(tm->result);
		return ERROR_GARBAGE_RECEIVED;
	}

	code[0] = z[0];
	code[1] = z[1];
	code[2] = z[2];

	info = grow_info(nil, &z[4], tm->result - 4);
	if (z[3] == '-') {   /* we have a continuation message */
		while (1) {
			PutMsg(tcp, &tm->header);
			do {
				rsigs = Wait(signals);
				if (rsigs & asigs) {
					state_change(sp, SS_ABORTING);

					if (info)
						deallocate(info, V_cstr);

					interrupt_message(sp, tm);

					if (rsigs & sp->disconnect_signals) {
						disconnect(sp);
					}

					return ERROR_INTERRUPTED;
				}
			} while (!GetMsg(sync));

			if (tm->error != NO_ERROR) {  /* tell them about the error */
				if (info)
					deallocate(info, V_cstr);

				return tm->error;
			}

			if (tm->result < 4) {      /* not enough to even check if codes are equal */
				info = grow_info(info, z, tm->result);
				continue;
			}

			if (z[0] == code[0] &&
				z[1] == code[1] &&
				z[2] == code[2]) {
				info = grow_info(info, &z[4], tm->result - 4);
				if (z[3] == ' ') break;    /* end of continuation */
			}
			else {
				info = grow_info(info, z, tm->result);
			}
		}
	}

	// Antwort enthält \n
	DS(kprintf("response = %s", info))

		*infop = info;
	return NO_ERROR;
}



unsigned short numeric_reply(unsigned char *s)
{
	return (unsigned short)((s[0] - '0') * 100 + (s[1] - '0') * 10 + (s[2] - '0'));
}



boolean retry_cancel(struct IntuitionBase *IntuitionBase, unsigned char *title, unsigned char *info)
/*
 * paged information with retry/cancel buttons
 * returns true for retry
 *    info may be nil (well, sortof)
 */
{
	unsigned char *z, *s, tmp;
	struct EasyStruct es;
	int nlines;

	es.es_StructSize = sizeof(struct EasyStruct);
	es.es_Flags = 0;
	es.es_Title = title;
	es.es_TextFormat = "%s";

	if (info)
		z = info;
	else
		z = strings[MSG_UNKNOWN];

more:
	s = z;
	nlines = 0;

	while (*z && nlines < MORE_LINES) {
		if (*z == '\n') nlines++;
		z++;
	}

	if (*z) {
		es.es_GadgetFormat = strings[MSG_RETRY_MORE_CANCEL];
	}
	else {
		es.es_GadgetFormat = strings[MSG_RETRY_CANCEL];
	}

	tmp = *z;
	*z = 0;

	switch (EasyRequest(nil, &es, nil, s)) {
	case 0:  /* cancel */
		*z = tmp;
		return false;
	case 1: /* retry */
		*z = tmp;
		return true;
	case 2: /* more */
		*z = tmp;
		if (!*z) return true;
		goto more;
	}

	/* 2005-05-14 ABA : unreachable */
	return false;
}



void ok(struct IntuitionBase *IntuitionBase, unsigned char *title, unsigned char *info)
/*
 * paged information with ok button
 *    info may be nil (sortof)
 */
{
	unsigned char *z, *s, tmp;
	struct EasyStruct es;
	int nlines;

	es.es_StructSize = sizeof(struct EasyStruct);
	es.es_Flags = 0;
	es.es_Title = title;
	es.es_TextFormat = "%s";

	if (info)
		z = info;
	else
		z = strings[MSG_UNKNOWN];

more:
	s = z;

	nlines = 0;
	while (*z && nlines < MORE_LINES) {
		if (*z == '\n') nlines++;
		z++;
	}

	if (*z) {
		es.es_GadgetFormat = strings[MSG_MORE_OK];
	}
	else {
		es.es_GadgetFormat = strings[MSG_OK];
	}

	tmp = *z;
	*z = 0;

	if (EasyRequest(nil, &es, nil, s) && tmp) {
		*z = tmp;
		goto more;
	}

	*z = tmp;
	return;
}



void disconnect(site *sp)
/*
 * rudely close control connection and clean up state information on site sp
 */
{
	tcpmessage *tm;
	struct MsgPort *sync;

	verify(sp, V_site);

	if (!sp->connected) return;

	sync = sp->sync;

	DS(kprintf("** disconnect site %s\n", sp->host))

		state_change(sp, SS_DISCONNECTING);

	tm = sp->cfile;      /* file open, have to close it */
	if (tm)
	{
		verify(tm, V_tcpmessage);
		verify(sp->file_list, V_file_info);

		tm->command = TCP_CLOSE;
		tm->header.mn_ReplyPort = sync;

		PutMsg(tcp, &tm->header);  /* send CLOSE on file tm */
		WaitPort(sync); GetMsg(sync);

		tm->command = TCP_DISPOSE;
		PutMsg(tcp, &tm->header);

		sp->cfile = nil;

		deallocate(sp->file_list, V_file_info);
		sp->file_list = nil;
	}

	tm = sp->control;
	verify(tm, V_tcpmessage);

	tm->command = TCP_CLOSE;
	tm->header.mn_ReplyPort = sync;

	PutMsg(tcp, &tm->header);  /* send CLOSE */

	sp->connected = false;

	if (sp->cwd)
	{
		deallocate(sp->cwd, V_cstr);
		sp->cwd = nil;
	}

	while (sp->infos)
		free_info_header(sp->infos);

	WaitPort(sync); GetMsg(sync); /* wait for CLOSE to come back */

	/* it shouldn't really fail ... not sure if we can do anything if it has */

	state_change(sp, SS_DISCONNECTED);

	return;
}



unsigned long read_file(site *sp, unsigned char *s, unsigned long *length)
/*
 * read *length bytes from open file
 * Inputs:
 * sp : site pointer
 * s  : data buffer
 * length   : pointer to length to read, changed to length actually read
 *
 * Result:
 * tcp error is returned, *length is modified to be actual length read
 */
{
	tcpmessage *tm;
	struct MsgPort *sync;
	unsigned long signals, asigs, rsigs;

	verify(sp, V_site);
	truth(s != nil);
	truth(length != nil);

	tm = sp->cfile;
	verify(tm, V_tcpmessage);

	sync = sp->sync;

	tm->command = TCP_READ;
	tm->flags = 0;
	tm->data = s;
	tm->length = *length;
	tm->header.mn_ReplyPort = sync;

	asigs = sp->abort_signals | sp->disconnect_signals;
	signals = asigs | (1 << sync->mp_SigBit);

	PutMsg(tcp, &tm->header);
	do
	{
		rsigs = Wait(signals);
		if (rsigs & asigs)
		{
			state_change(sp, SS_ABORTING);

			interrupt_message(sp, tm);

			if (rsigs & sp->disconnect_signals) {
				disconnect(sp);
			}
			else {
				close_file(sp, false);
			}

			*length = 0;
			return ERROR_INTERRUPTED;
		}
	} while (!GetMsg(sync));

	if (tm->result > 0)
	{
		*length = tm->result;
		return NO_ERROR;
	}
	else
	{
		*length = 0;
		return tm->error;
	}
}



unsigned long write_file(site *sp, unsigned char *s, unsigned long *length)
/*
 * write *length bytes to an open file (almost identical copy to read_file above)
 * Inputs:
 * sp : site pointer
 * s  : data buffer
 * length   : pointer to length to write, changed to length actually written
 *
 * Result:
 * tcp error is returned, *length is modified to be actual length written
 */
{
	tcpmessage *tm;
	struct MsgPort *sync;
	unsigned long signals, asigs, rsigs;

	verify(sp, V_site);
	truth(s != nil);
	truth(length != nil);

	tm = sp->cfile;
	verify(tm, V_tcpmessage);

	sync = sp->sync;

	tm->command = TCP_WRITE;
	tm->flags = 0;
	tm->data = s;
	tm->length = *length;
	tm->header.mn_ReplyPort = sync;

	asigs = sp->abort_signals | sp->disconnect_signals;
	signals = asigs | (1 << sync->mp_SigBit);

	PutMsg(tcp, &tm->header);
	do
	{
		rsigs = Wait(signals);
		if (rsigs & asigs)
		{
			state_change(sp, SS_ABORTING);

			interrupt_message(sp, tm);

			if (rsigs & sp->disconnect_signals)
			{
				disconnect(sp);
			}
			else {
				close_file(sp, false);
			}

			*length = 0;
			return ERROR_INTERRUPTED;
		}
	} while (!GetMsg(sync));

	if (tm->result > 0)
	{
		*length = tm->result;
		return NO_ERROR;
	}
	else {
		*length = 0;
		return tm->error;
	}
}



unsigned long open_file(site *sp, unsigned char *s, boolean writing, unsigned char *leaf_name)
/*
 * open file with name in s
 * Inputs:
 * sp : site pointer
 * s  : file name
 * writing  : true if opened for writing, false if for reading
 *
 * Returns:
 * 0  : no error
 * non-0 : standard file system errors (ERROR_OBJECT_NOT_FOUND etc)
 */
{
	tcpmessage *tm, *newtm;
	struct MsgPort *sync;
	unsigned char reply[4], *info;
	unsigned char *leaf;
	unsigned long error;
	unsigned short port_number;
	file_info *fi;

	verify(sp, V_site);
	truth(s != nil);

	/* a few conditions we are assuming */
	truth(sp->connected);
	truth(sp->cfile == nil);
	truth(sp->file_list == nil);

	if (s[0] == 0)
	{
		/* they are trying to open the root of this site as a file ... */
		return ERROR_OBJECT_WRONG_TYPE;
	}

	tm = sp->control;
	verify(tm, V_tcpmessage);

	sync = sp->sync;

	tm->header.mn_ReplyPort = sync;

	leaf = cd_parent(sp, s);
	if (!leaf)
	{
		/* there are other possible reasons here, but I'm being lazy ... */
		return ERROR_DIR_NOT_FOUND;
	}

	if (leaf_name)
		leaf = leaf_name;

	DS(kprintf("** open_file(\"%s\", %s) (site %s)\n", s, (writing ? "write" : "read"), sp->host))

		state_change(sp, SS_OPENING);

	newtm = new_message(sp);
	if (newtm)
	{
		fi = (file_info *)allocate(sizeof(*fi) + strlen(s) + 1, V_file_info);
		if (fi)
		{
			strcpy(fi->fname, s);

			if (control_write(sp, "PASV\r\n", 0) == NO_ERROR) {
				if (writing)
				{ /* yes, I do this twice :( */
					sprintf(sp->read_buffer, "STOR %s\r\n", leaf);
				}
				else {
					sprintf(sp->read_buffer, "RETR %s\r\n", leaf);
				}

				if (!sp->quick || control_write(sp, sp->read_buffer, 0) == NO_ERROR)
				{
					if (response(sp, 0, &info, reply) == NO_ERROR)
					{
						if (reply[0] == '2')
						{
							if (info)
							{
								if (passive_response(info, sp->read_buffer, &port_number))
								{
									deallocate(info, V_cstr);

									if (make_connection(sp, newtm, sp->read_buffer, port_number, 0) == NO_ERROR)
									{
										if (writing)
										{ /* and again */
											sprintf(sp->read_buffer, "STOR %s\r\n", leaf);
										}
										else {
											sprintf(sp->read_buffer, "RETR %s\r\n", leaf);
										}

										if (sp->quick || control_write(sp, sp->read_buffer, 0) == NO_ERROR)
										{
											/* this next response will be to the RETR/STOR */
											if (response(sp, 0, &info, reply) == NO_ERROR)
											{
												if (info)
												{
#ifdef VERIFY
													if (reply[0] != '1')
													{
														reply[3] = 0;
														show_string(reply);
														show_string(info);
													}
#endif
													deallocate(info, V_cstr);
												}
												if (reply[0] == '1')
												{
													ensure(fi, V_file_info);

													fi->rfarg = 0;
													fi->rpos = 0;
													fi->vpos = 0;
													fi->end = 0;
													fi->closed = false;
													fi->seek_end = false;
													fi->eof = false;
													fi->port = nil;
													fi->type = (writing) ? ACTION_FINDOUTPUT : ACTION_FINDINPUT;

													sp->cfile = newtm;
													sp->file_list = fi;
													fi->next = nil;

													return 0;
												}
												else
												{
													switch (numeric_reply(reply))
													{
													case 450:
													case 520:
													case 550:
														if (writing)
														{
															error = ERROR_INVALID_COMPONENT_NAME;
														}
														else {
															error = ERROR_OBJECT_NOT_FOUND;
														}
														break;
													case 521:
													case 532:
													case 533:
														if (writing)
														{
															error = ERROR_WRITE_PROTECTED;
														}
														else {
															error = ERROR_READ_PROTECTED;
														}
														break;
													case 452:
													case 552:
														error = ERROR_DISK_FULL;
														break;
													case 553:
														if (writing)
														{
															error = ERROR_WRITE_PROTECTED;
														}
														else {
															error = ERROR_INVALID_COMPONENT_NAME;
														}
														break;
													default:
														error = ERROR_OBJECT_NOT_FOUND;
														break;
													}
												}

												/* no need to disconnect sp */

												deallocate(fi, V_file_info);

												break_connection(sp, newtm);

												newtm->command = TCP_DISPOSE;
												PutMsg(tcp, &newtm->header);

												return error;
											}
											else
											{
												show_string("Error reading response to RETR/STOR");
												error = ERROR_OBJECT_NOT_FOUND;
											}
											break_connection(sp, newtm);
										}
										else
										{
											show_string("error writing RETR/STOR");
											error = ERROR_OBJECT_NOT_FOUND;
										}
									}
									else
									{
										show_string("Error making connection");
										error = ERROR_OBJECT_NOT_FOUND;
									}
								}
								else
								{
									show_string("Bad PASV response");
									deallocate(info, V_cstr);
									error = ERROR_OBJECT_NOT_FOUND;
								}
							}
							else
							{
								show_string("no info");
								error = ERROR_NO_FREE_STORE;
							}
						}
						else
						{
							show_string("non-'2' response to PASV");
							error = ERROR_OBJECT_NOT_FOUND;
						}
					}
					else
					{
						show_string("error reading response to PASV");
						error = ERROR_OBJECT_NOT_FOUND;
					}
				}
				else
				{
					show_string("error writing RETR/STOR");
					error = ERROR_OBJECT_NOT_FOUND;
				}
			}
			else
			{
				show_string("error writing PASV");
				error = ERROR_OBJECT_NOT_FOUND;
			}

			deallocate(fi, V_file_info);
		}
		else
			error = ERROR_NO_FREE_STORE;

		newtm->command = TCP_DISPOSE;
		PutMsg(tcp, &newtm->header);

		disconnect(sp);
	}
	else
		error = ERROR_NO_FREE_STORE;

	return error;
}



/* this is how large our flushing buffer is when attempting an abort */
#define FLUSH_SIZE 100

void close_file(site *sp, boolean normal_close)
/*
 * close currently open file for site
 * Inputs:
 * sp : site pointer
 * normal_close : true if closed normally, false if closed by async abort
 */
{
	tcpmessage *tm, *filetm, *ret;
	file_info *fi;
	struct MsgPort *sync;
	unsigned char *info, reply[4], flush[FLUSH_SIZE];
	unsigned long signals, asigs, rsigs;

	verify(sp, V_site);

	tm = sp->control;
	filetm = sp->cfile;
	fi = sp->file_list;

	verify(tm, V_tcpmessage);
	verify(filetm, V_tcpmessage);
	verify(fi, V_file_info);

	if (normal_close)
	{
		sp->cfile = nil;
		sp->cfile_type = 0;
		sp->file_list = 0;
	}

	DS(kprintf("** close_file (site %s)\n", sp->host))

		state_change(sp, SS_CLOSING);

	sync = sp->sync;

	signals = (1 << sync->mp_SigBit) | sp->disconnect_signals | sp->abort_signals;
	asigs = sp->disconnect_signals | sp->abort_signals;

#ifdef VERIFY
	if (fi->eof && fi->rpos < fi->end)
	{
		show_string("Closing : EOF before fi->end");
		show_int(fi->rpos);
		show_int(fi->end);
	}
#endif

	if (fi->type == ACTION_FINDINPUT && fi->rpos < fi->end && !fi->eof)
	{
		if (normal_close)
		{
			deallocate(fi, V_file_info);
		}
		else {
			fi->eof = true;
		}

		/* have to ABOR :( */
		show_string("Attempting ABOR");

		if (control_write(sp, "ABOR\r\n", 0) != NO_ERROR)
		{
			show_string("close file failed X1");

			break_connection(sp, filetm);

			disconnect(sp);

			filetm->command = TCP_DISPOSE;
			PutMsg(tcp, &filetm->header);

			return;
		}

		/* can't use response because we need to flush filetm at the same time */

		filetm->command = TCP_READ;
		filetm->flags = 0;
		filetm->data = flush;
		filetm->length = FLUSH_SIZE;
		filetm->header.mn_ReplyPort = sync;

		tm->command = TCP_READ;
		tm->flags = FLAG_READLINE;
		tm->data = sp->read_buffer;
		tm->length = READ_BUFFER_LENGTH;
		tm->header.mn_ReplyPort = sync;

		PutMsg(tcp, &tm->header);
		PutMsg(tcp, &filetm->header);

		while (1)
		{
			rsigs = Wait(signals);
			if (rsigs & asigs)
			{
				state_change(sp, SS_ABORTING);

				interrupt_message(sp, tm);
				interrupt_message(sp, filetm);

				break;
			}

			ret = (tcpmessage *)GetMsg(sync);
			if (ret == tm)
			{
				/* wait for filetm */
				WaitPort(sync); GetMsg(sync);
				break;
			}
			if (ret->error != NO_ERROR)
			{ /* filetm is done */
			   /* wait for tm */
				WaitPort(sync); GetMsg(sync);
				break;
			}
			PutMsg(tcp, &filetm->header);
		}

		break_connection(sp, filetm);

		if (normal_close)
		{
			filetm->command = TCP_DISPOSE;
			PutMsg(tcp, &filetm->header);
		}

		if (tm->error != NO_ERROR)
		{
			show_string("close file failed X2");
			disconnect(sp);

			return;
		}

		show_string("First ABOR response");
#ifdef VERIFY
		if (sp->read_buffer[3] == '-')
			show_string("continuation reply on ABOR");
#endif
		if (!normal_close)
		{
			/* leave the close response until we do the real close later */
			return;
		}

		switch (response(sp, 0, &info, reply))
		{
		case NO_ERROR:
			break;
		default:
			show_string("close file failed X3");
			disconnect(sp);

			return;
		}

		show_string("Second ABOR response");

#ifdef VERIFY
		if (reply[0] != '2')
		{
			reply[3] = 0;
			show_string(reply);
			show_string(info);
		}
#endif

		if (info)
			deallocate(info, V_cstr);

		show_string("ABOR completed");
		return;
	}

	if (normal_close)
	{
		deallocate(fi, V_file_info);
	}
	else {
		fi->eof = true;
	}

	break_connection(sp, filetm);

	if (!normal_close)
	{
		/* leave the final close response until we do the real close later */
		return;
	}

	filetm->command = TCP_DISPOSE;
	PutMsg(tcp, &filetm->header);

	switch (response(sp, 0, &info, reply))
	{
	case NO_ERROR:
		break;
	default:
		show_string("close failed 1");

		disconnect(sp);
		return;
	}

	if (info)
		deallocate(info, V_cstr);

	/* we don't really care what they returned here */

#ifdef VERIFY
	if (reply[0] != '2')
	{
		show_string("Non '2' close of file");
		sp->read_buffer[0] = reply[0];
		sp->read_buffer[1] = reply[1];
		sp->read_buffer[2] = reply[2];
		sp->read_buffer[3] = 0;
		show_string(sp->read_buffer);
	}
#endif

	return;
}



unsigned long delete_file(site *sp, unsigned char *s)
/*
 * delete file with name in s
 * Inputs:
 * sp : site pointer
 * s  : full path name
 *
 * Returns:
 * standard file system errors
 */
{
	unsigned char *leaf;
	unsigned long error;
	boolean perm;
	unsigned char *info, reply[3];
	ftpinfo *fi;

	verify(sp, V_site);
	truth(s != nil);

	if (s[0] == 0)
	{
		/* trying to delete root */
		return ERROR_OBJECT_WRONG_TYPE;
	}

	leaf = cd_parent(sp, s);
	if (!leaf)
	{
		/* again, being lazy here */
		return ERROR_DIR_NOT_FOUND;
	}

	DS(kprintf("** delete_file(\"%s\") (site %s)\n", s, sp->host))

		state_change(sp, SS_DELETING);

	fi = get_info(sp, s);
	if (fi)
	{
		leaf = fi->name;
	}

	sprintf(sp->read_buffer, "DELE %s\r\n", leaf);

	if (control_write(sp, sp->read_buffer, 0) == NO_ERROR)
	{
		if (response(sp, 0, &info, reply) == NO_ERROR)
		{
			perm = substr(info, "perm");
			if (info) deallocate(info, V_cstr);

			switch (reply[0])
			{
			case '2':
				return 0;   /* success */
			case '4':
				/* temp failure ... */
				/* most likely reason */
				return ERROR_OBJECT_IN_USE;
			default:
				if (numeric_reply(reply) == 502)
					return ERROR_ACTION_NOT_KNOWN;

				if (perm)
					return ERROR_DELETE_PROTECTED;
				else
					return ERROR_OBJECT_NOT_FOUND;
			}
		}
		else
		{
			disconnect(sp);
			error = ERROR_OBJECT_NOT_FOUND;
		}
	}
	else
	{
		disconnect(sp);
		error = ERROR_OBJECT_NOT_FOUND;
	}

	return error;
}



unsigned long delete_directory(site *sp, unsigned char *s)
/*
 * delete directory with name in s
 * Inputs:
 * sp : site pointer
 * s  : full path name
 *
 * Returns:
 * standard file system errors
 */
{
	unsigned char *leaf;
	unsigned long error;
	boolean perm, no_such;
	unsigned char *info, reply[3];
	ftpinfo *fi;

	verify(sp, V_site);
	truth(s != nil);

	if (s[0] == 0)
	{
		/* trying to delete root */
		return ERROR_OBJECT_WRONG_TYPE;
	}

	leaf = cd_parent(sp, s);
	if (!leaf)
	{
		/* again, being lazy here */
		return ERROR_DIR_NOT_FOUND;
	}

	DS(kprintf("** delete_dir(\"%s\") (site %s)\n", s, sp->host))

		state_change(sp, SS_DELETING);

	fi = get_info(sp, s);
	if (fi)
	{
		leaf = fi->name;
	}

	sprintf(sp->read_buffer, "RMD %s\r\n", leaf);
	if (control_write(sp, sp->read_buffer, 0) == NO_ERROR)
	{
		if (response(sp, 0, &info, reply) == NO_ERROR)
		{
			perm = substr(info, "perm");
			no_such = substr(info, "no such");
			if (info) deallocate(info, V_cstr);

			switch (reply[0])
			{
			case '2':
				return 0;   /* success */
			case '4':
				/* temp failure ... */
				/* most likely reason */
				return ERROR_OBJECT_IN_USE;
			default:
				if (numeric_reply(reply) == 502)
					return ERROR_ACTION_NOT_KNOWN;

				if (perm)
				{
					return ERROR_DELETE_PROTECTED;
				}
				else if (no_such)
				{
					return ERROR_OBJECT_NOT_FOUND;
				}
				else {
					return ERROR_DIRECTORY_NOT_EMPTY;
				}
			}
		}
		else {
			disconnect(sp);
			error = ERROR_OBJECT_NOT_FOUND;
		}
	}
	else {
		disconnect(sp);
		error = ERROR_OBJECT_NOT_FOUND;
	}

	return error;
}



unsigned long make_directory(site *sp, unsigned char *s)
/*
 * make directory with name in s
 * Inputs:
 * sp : site pointer
 * s  : full path name
 *
 * Returns:
 * standard file system errors
 */
{
	unsigned char *leaf;
	unsigned long error;
	boolean exists;
	unsigned char *info, reply[3];

	verify(sp, V_site);
	truth(s != nil);

	if (s[0] == 0) {
		/* trying to mkd root */
		return ERROR_OBJECT_WRONG_TYPE;
	}

	leaf = cd_parent(sp, s);
	if (!leaf) {
		/* again, being lazy here */
		return ERROR_DIR_NOT_FOUND;
	}

	DS(kprintf("** make_dir(\"%s\") (site %s)\n", s, sp->host))

		state_change(sp, SS_MAKEDIR);

	sprintf(sp->read_buffer, "MKD %s\r\n", leaf);
	if (control_write(sp, sp->read_buffer, 0) == NO_ERROR)
	{
		if (response(sp, 0, &info, reply) == NO_ERROR)
		{
			exists = substr(info, "exist");

			if (info)
				deallocate(info, V_cstr);

			switch (reply[0])
			{
			case '2':
				return 0;   /* success */
			case '4':
				/* temp failure ... */
				/* most likely reason */
				return ERROR_OBJECT_IN_USE;
			default:
				if (numeric_reply(reply) == 502)
					return ERROR_ACTION_NOT_KNOWN;

				if (exists)
					return ERROR_OBJECT_EXISTS;
				else
					return ERROR_WRITE_PROTECTED;
			}
		}
		else
		{
			disconnect(sp);
			error = ERROR_OBJECT_NOT_FOUND;
		}
	}
	else
	{
		disconnect(sp);
		error = ERROR_OBJECT_NOT_FOUND;
	}

	return error;
}



unsigned long rename_object(site *sp, unsigned char *from, unsigned char *to)
/*
 * renames file 'from' to 'to'
 * Inputs:
 * sp : site pointer
 * from, to: null terminated file names
 *
 * Returns:
 * file system error, or 0 indicating success
 */
{
	unsigned char *leaf1, *leaf2;
	unsigned char *info, reply[3];
	boolean perm, exist;

	if (sp->unix_paths)
	{
		if (!change_dir(sp, ""))
		{
			return ERROR_DIR_NOT_FOUND;
		}

		leaf1 = from;
		leaf2 = to;
	}
	else
	{
		leaf1 = cd_parent(sp, from);
		if (!leaf1)
		{
			return ERROR_DIR_NOT_FOUND;
		}

		leaf2 = to + strlen(to) - 1;
		while (leaf2 > to && *leaf2 != '/')
			leaf2--;

		if (leaf2 > to)
			*leaf2 = 0;

		if (strcmp(to, sp->cwd) == 0)
		{  /* they are in the same directory, we can do it */
			if (leaf2 > to)
				*leaf2 = '/';
		}
		else
		{
			return ERROR_ACTION_NOT_KNOWN;
		}
	}

	DS(kprintf("** rename(\"%s\" to \"%s\") (site %s)\n", from, to, sp->host))

		state_change(sp, SS_RENAMING);

	sprintf(sp->read_buffer, "RNFR %s\r\n", leaf1);
	if (control_write(sp, sp->read_buffer, 0) != NO_ERROR)
	{
		disconnect(sp);
		return ERROR_OBJECT_NOT_FOUND;
	}

	if (sp->quick)
	{
		sprintf(sp->read_buffer, "RNTO %s\r\n", leaf2);
		if (control_write(sp, sp->read_buffer, 0) != NO_ERROR)
		{
			disconnect(sp);
			return ERROR_OBJECT_NOT_FOUND;
		}
	}

	/* response to RNFR */
	if (response(sp, 0, &info, reply) != NO_ERROR)
	{
		disconnect(sp);
		return ERROR_OBJECT_NOT_FOUND;
	}

	perm = substr(info, "perm");

	if (info) deallocate(info, V_cstr);

	if (reply[0] != '3')
	{
		if (perm)
		{
			return ERROR_WRITE_PROTECTED;
		}
		return ERROR_OBJECT_NOT_FOUND;
	}

	if (!sp->quick)
	{
		sprintf(sp->read_buffer, "RNTO %s\r\n", leaf2);
		if (control_write(sp, sp->read_buffer, 0) != NO_ERROR)
		{
			disconnect(sp);
			return ERROR_OBJECT_NOT_FOUND;
		}
	}

	/* response to RNTO */
	if (response(sp, 0, &info, reply) != NO_ERROR)
	{
		disconnect(sp);
		return ERROR_OBJECT_NOT_FOUND;
	}

	perm = substr(info, "perm");
	exist = substr(info, "exist");

	if (info) deallocate(info, V_cstr);

	if (reply[0] != '2')
	{
		if (perm)
		{
			return ERROR_WRITE_PROTECTED;
		}
		if (exist)
		{
			return ERROR_OBJECT_EXISTS;
		}

		return ERROR_INVALID_COMPONENT_NAME;
	}

	return 0;
}



boolean change_dir(site *sp, unsigned char *new_dir)
/*
 * change directory to new_dir
 * Inputs:
 * sp : site pointer
 * new_dir  : null terminated path name
 *
 * Returns:
 * true if change_dir was successful
 */
{
	tcpmessage *tm;
	struct MsgPort *sync;
	unsigned char *info, reply[4];
	unsigned char *z, *s;

	verify(sp, V_site);
	truth(new_dir != nil);

	tm = sp->control;
	sync = sp->sync;

	verify(tm, V_tcpmessage);

	/* check to see if we are already there */

	if (!sp->cwd && new_dir[0] == 0)
		return true;
	if (sp->cwd && strcmp(sp->cwd, new_dir) == 0)
		return true;

	/* have to explicitly change there */

	if (sp->cfile)
	{  /* can't do _anything_ while we have a file opened */
		if (sp->file_list->closed)
		{
			close_file(sp, true);
		}
		else {
			return false;
		}
	}

	if (!sp->connected)
	{
		return false;
	}

	DS(kprintf("** change_dir(\"%s\") (site %s)\n", new_dir, sp->host))

		state_change(sp, SS_CWD);

	if (sp->cwd)
	{
		deallocate(sp->cwd, V_cstr);
		sp->cwd = nil;
	}

	/* first we change to the root */

	if (sp->root)
	{
		sprintf(sp->read_buffer, "CWD %s\r\n", sp->root);
	}
	else {
		strcpy(sp->read_buffer, "CWD\r\n");
	}

	if (control_write(sp, sp->read_buffer, 0) != NO_ERROR)
	{
		show_string("change dir failed 1");

		disconnect(sp);

		return false;
	}

	/* this CWD is vital */

	if (response(sp, 0, &info, reply) != NO_ERROR)
	{
		show_string("change dir failed 2");

		disconnect(sp);

		return false;
	}

	if (info)
		deallocate(info, V_cstr);

	if (reply[0] != '2')
	{
		show_string("change dir failed 3");

		disconnect(sp);

		return false;
	}

	/*
	 * ok, we are at (should be at :) the root (or at least what
	 * we consider to be the root) of the FS
	 */

	if (new_dir[0] == 0)
	{
		/* they wanted to change to the root ... so nothing further need be done */
		return true;
	}

	if (sp->unix_paths)
	{
		sprintf(sp->read_buffer, "CWD %s\r\n", new_dir);

		if (control_write(sp, sp->read_buffer, 0) != NO_ERROR)
		{
			show_string("change dir failed 4");

			disconnect(sp);

			return false;
		}

		if (response(sp, 0, &info, reply) != NO_ERROR)
		{
			show_string("change dir failed 5");

			disconnect(sp);

			return false;
		}

		if (info) deallocate(info, V_cstr);

		if (reply[0] == '2')
		{
			sp->cwd = (unsigned char *)allocate(strlen(new_dir) + 1, V_cstr);
			if (sp->cwd)
			{
				strcpy(sp->cwd, new_dir);
				return true;
			}
			goto fail_to_root;
		}

		/* ok, our clumped cwd didn't work, lets try it the slow way */
	}

	s = z = new_dir;
	while (*z)
	{
		while (*s && *s != '/') s++;

		if (*s == '/')
		{
			*s = 0;
			sprintf(sp->read_buffer, "CWD %s\r\n", z);
			*s++ = '/';
		}
		else
		{
			sprintf(sp->read_buffer, "CWD %s\r\n", z);
		}

		if (control_write(sp, sp->read_buffer, 0) != NO_ERROR)
		{
			show_string("change dir failed 6");

			disconnect(sp);

			return false;
		}

		if (response(sp, 0, &info, reply) != NO_ERROR)
		{
			show_string("change dir failed 7");

			disconnect(sp);

			return false;
		}

		if (info)
			deallocate(info, V_cstr);

		if (reply[0] != '2')
		{
			goto fail_to_root;
		}

		z = s;
	}

	/* we've succeeded where unix_paths failed, so ... */

	sp->unix_paths = false;

	sp->cwd = (unsigned char *)allocate(strlen(new_dir) + 1, V_cstr);
	if (sp->cwd)
	{
		strcpy(sp->cwd, new_dir);
		return true;
	}

fail_to_root:
	/* something went wrong ... who knows where we are? ... go back to the root */

	if (sp->root)
	{
		sprintf(sp->read_buffer, "CWD %s\r\n", sp->root);
	}
	else {
		strcpy(sp->read_buffer, "CWD\r\n");
	}

	if (control_write(sp, sp->read_buffer, 0) != NO_ERROR)
	{
		show_string("change dir failed 8");

		disconnect(sp);

		return false;
	}

	if (response(sp, 0, &info, reply) != NO_ERROR)
	{
		show_string("change dir failed 9");

		disconnect(sp);

		return false;
	}

	if (info)
		deallocate(info, V_cstr);

	if (reply[0] == '2')
	{
		return false;
	}

	show_string("change dir failed 10");

	disconnect(sp);

	return false;
}



unsigned char *cd_parent(site *sp, unsigned char *path)
/*
 * change to the parent dir of the object described by path
 * Inputs:
 * sp : site pointer
 * path  : string describing object
 *
 * Returns:
 * pointer to leaf name (last component of path)
 * or nil ... generally indicates gross error or dir not found
 */
{
	unsigned char *leaf;
	boolean cd;

	verify(sp, V_site);
	truth(path != nil);

	/* start at end of pathname and work back til we find a / */

	leaf = path + strlen(path) - 1;
	while (leaf > path && *leaf != '/')
		leaf--;

	if (leaf == path)
	{
		/* no /, so we are talking about an object in the root dir */

		cd = change_dir(sp, "");
	}
	else
	{
		/* temporarily knock out / to get parent path */

		*leaf = 0;
		cd = change_dir(sp, path);

		/* then restore the / and move over it */
		*leaf++ = '/';
	}

	if (cd)
		return leaf;
	else
		return nil;
}



// Großbuchstaben für einfachere Vergleiche
static char *months[] = { "JAN ", "FEB ", "MAR ", "APR ", "MAY ", "JUN ",
			   "JUL ", "AUG ", "SEP ", "OCT ", "NOV ", "DEC " };


void convert_nt_entry(unsigned char *is)          // Input String from add_info()
/*
 *  if this is a WinNT entry, convert (in place) to a
 *  *nix style record. (Unix, Linux, Xenix...)
 *
 *  03/09/96 Ron Flory (RJF)
 *
 *  This function kindly donated by Ron Flory (), and remain Copyright 1996,
 *  all rights reserved by him.
 */
{
	static unsigned char   *nt_buf = 0;
 /* jetzt global, da auch in add_info() benötigt wird
	static char *months[] = {  "Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ",
				"Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec " };
 */
	unsigned char    *c1, *c2, tbuf[32], c;
	short i;

	if (!nt_buf)                        // if our conversion buf not alloc'd
		nt_buf = (unsigned char*)allocate(READ_BUFFER_LENGTH, V_cstr);

	// ***** verify inbuffer viable, MsDos-like, conversion buf alloc'd *****

	if ((!is) || (!nt_buf) || (!isdigit(*is)) || (strlen(is) < 41))
		return;

	*nt_buf = 0;                        // term NT->Unix conversion buffer
	c1 = strchr(is, 0x0d);              // look for MsDos CR
	if (c1)
		*c1 = 0;                         // then, remove CR

	 // ***** avoid altering 'is' unless this really is an MsDos entry *****


	strcpy(nt_buf, "-rwxrwxrwx 9 x x ");   // assume this is a file
				   // plug dummy fields (ignored)
				   // Note: execute flags set

	if ((strstr(is, "<DIR>")) || (strstr(is, "<dir>"))) // directory ?
	{
		*nt_buf = 'd';                   // force directory flag in return buf
		strcat(nt_buf, "size ");         // dummy 'size' (ignored on dirs)
	}
	else
	{
		// ***** extract filesize *****

		c1 = is + 30;                     // start of length field
		c2 = nt_buf + strlen(nt_buf);

		while (*c1 == ' ')               // skip leading spaces
			c1++;

		while (((c = *(c1++)) != 0) && (c <= '9') && (c > ' '))
			if (c != ',')
			*(c2++) = c;               // copy filesize digits (skip commas)

		*(c2++) = ' ';                   // padd
		*c2 = 0;                     // term
	}

	// ***** extract creation date *****

	memcpy(tbuf, is, 2);                // month digits (01..12)
	tbuf[2] = 0;

	i = atol(tbuf);                     // convert digits to 1..12 month index

	if ((i) && (i < 13))              // valid month index ??
		strcat(nt_buf, months[i - 1]);   // insert month string
	memcpy(tbuf, is + 3, 2);            // day digits (01..31)
	tbuf[2] = 0;

	if (*tbuf == '0')                   // leading space on 'day' ??
		*tbuf = ' ';                     // need leading space, not zero

	strcat(nt_buf, tbuf);               // append day of month
	strcat(nt_buf, " ");

	// ***** get creation time *****

	c1 = strchr(is, ':');               // find creation time field
	if (!c1)
		return;                          // abort, not an MsDos entry

	memcpy(tbuf, c1 - 2, 2);             // extract hour
	tbuf[2] = 0;                        // term

	i = atoi(tbuf);                     // month digits to index (01..12)

	if (toupper(*(c1 + 3)) == 'P')       // PM ???
		i += 12;

	stci_d(tbuf, i);                    // itoa()
	strcat(nt_buf, tbuf);               // hour
	strcat(nt_buf, ":");

	memcpy(tbuf, c1 + 1, 2);             // minutes
	tbuf[2] = 0;
	strcat(nt_buf, tbuf);               // minutes
	strcat(nt_buf, " ");                // padd

	// **** extract directory/file name *****

	c1 = is + 39;                        // start of fid (I hate hard offsets)
	c2 = nt_buf + strlen(nt_buf);

	while ((c = *c1) > ' ')             // (might want to skip leading spaces)
		*(c2++) = *(c1++);               // copy entire dir/filename

	*c2 = 0;                            // term buffer

	// ***** copy translated MsDos entry over orig Unix entry *****

	strcpy(is, nt_buf);                 // if we get here, should be OK (?!*)
}



void add_info(struct info_header *ih, unsigned char *s)
/*
 * parses s and adds the information to header ih
 * Inputs:
 * ih : info_header
 * s  : line returned from LIST
 */
{
	unsigned long perm;   // permission bits
	unsigned long size;   // file size
	unsigned char *ende, *linkname = nil, *user, *group;
	int tag, monat, jahr;
	unsigned char tempd[15], tempt[10];
	struct DateTime dtime;

	convert_nt_entry(s);          // if NT entry, convert to Unix format (RJF)

	// evt. Steuerzeichen (ASCII < 32) am Zeilenende löschen
	ende = s + strlen(s) - 1;
	while ((ende > s) && (*ende < ' '))
		ende--;
	ende[1] = 0;
	// ende zeigt jetzt auf letztes Zeichen der Zeile

	// skip leading blanks
	while ((*s > 0) && (*s <= ' '))
		s++;

	// gültige Zeilen von "ls" beginnen mit "-", "d" oder "l"; devices mit "c"
	// oder "b" werden auch ausgefiltert, da sie für ftp nicht interessieren
	if ((*s != '-') && (*s != 'd') && (*s != 'l'))
		return;


	// Protection-Bits; erstmal auf 0 setzen
	perm = 0;

	// erstes Zeichen gibt Typ an
	if (*s == 'd')
		perm |= MYFLAG_DIR;
	else if (*s == 'l')
	{
		perm |= MYFLAG_LINK;
		perm |= MYFLAG_DIR;     /* assume its a directory ... it _may_ be a file ... */
		// links sind gekennzeichnet wie folgt: "name -> orig.name";
		// Zeichen "->" von hinten suchen
		linkname = ende - 2;
		while ((linkname > s) && !((linkname[0] == '-') && (linkname[1] == '>')))
			linkname--;
		// Link gefunden?
		if (linkname > s)
		{
			// ja; nach dem Dateinamen ein \0 einfügen, dabei tolerant gegenüber
			// einem Leerzeichen vor "->" sein
			*linkname = 0;
			if (linkname[-1] == ' ')
				linkname[-1] = 0;

			// Linkname merken, dabei tolerant gegenüber einem Leerzeichen nach "->" sein
			linkname += 2;
			if (*linkname == ' ')
				linkname++;
		}
		else
			linkname = nil;
	} // else if (link)
	s++;

	// prüfe, ob 9 gültige Rechtebits vorhanden sind; nur diese können geparst werden
#define PERM_BIT_OK(n)  ( (s[n] >= '-') && (s[n] <= 'z') )
	if (PERM_BIT_OK(0) && PERM_BIT_OK(1) && PERM_BIT_OK(2) &&
		PERM_BIT_OK(3) && PERM_BIT_OK(4) && PERM_BIT_OK(5) &&
		PERM_BIT_OK(6) && PERM_BIT_OK(7) && PERM_BIT_OK(8))
	{
#undef PERM_BIT_OK
		// protection bits user (Bits setzen, wenn auf "-" gesetzt, da Bits low-aktiv)
		if (*s < 'A') perm |= FIBF_READ;
		s++;
		if (*s < 'A') perm |= FIBF_WRITE | FIBF_DELETE;
		s++;
		if (*s < 'A') perm |= FIBF_EXECUTE;
		s++;
		// protection bits group (Bits setzen, wenn nicht "-", da diese Bits high-aktiv)
		if (*s >= 'A') perm |= FIBF_GRP_READ;
		s++;
		if (*s >= 'A') perm |= FIBF_GRP_WRITE | FIBF_GRP_DELETE;
		s++;
		if (*s >= 'A') perm |= FIBF_GRP_EXECUTE;
		s++;
		// protection bits other
		if (*s >= 'A') perm |= FIBF_OTR_READ;
		s++;
		if (*s >= 'A') perm |= FIBF_OTR_WRITE | FIBF_OTR_DELETE;
		s++;
		if (*s >= 'A') perm |= FIBF_OTR_EXECUTE;
		s++;
	} // if

	// falls keine gültigen Rechtebits vorhanden, Standardwert "rwed" setzen
	// (heißt hier nichts tun); überspringen der Bits in folgender while-Schleife

	// überspringe noch nicht untersuchte Bits
	while (*s > ' ')
		s++;

	// skip blanks
	while ((*s > 0) && (*s <= 32))
		s++;


	// evt. link count überspringen
	if (isdigit(*s))
	{
		while (*s > ' ')
			s++;
	} // if

	// skip blanks
	while ((*s > 0) && (*s <= 32))
		s++;

	// ungültige Zeile abfangen
	if (*s == 0)
		return;

	// s steht jetzt (normalerweise!) auf dem user-Namen

 /*
	// jetzt von hinten anfangen und Dateilänge, Datum und Dateiname
	// heraussuchen (es sind die 5 hintersten Felder)

	erstmal noch nicht
 */


 // jetzt kommt der user-Name
	user = s;
	while (*s > ' ')
		s++;
	*s = 0;     // user-Name abschließen
	s++;

	// skip blanks
	while ((*s > 0) && (*s <= 32))
		s++;


	// jetzt kommt der group-Name
	group = s;
	while (*s > ' ')
		s++;
	*s = 0;     // group-Name abschließen
	s++;

	// skip blanks
	while ((*s > 0) && (*s <= 32))
		s++;


	// Dateigröße auslesen und überspringen
	size = atoi(s);

	while (*s > ' ')
		s++;

	// skip blanks
	while ((*s > 0) && (*s <= 32))
		s++;

	// ungültige Zeile abfangen
	if (*s == 0)
		return;


	// Monat (1..12) aus "mmm"-String ermitteln (geht nicht mit StrToDate(),
	// da es Locale nutzt und ftp immer englische Monatskürzel liefert)
	monat = 0;
	while ((monat < 12) && !(((s[0] & ~32) == months[monat][0]) &&
		((s[1] & ~32) == months[monat][1]) &&
		((s[2] & ~32) == months[monat][2])))
		monat++;

	monat++;    // Monat auf 1..12 bringen (falls oben keiner gefunden wurde,
				// wird monat auf 13 gesetzt, was ein ungültiges Datum erzeugt

	s += 3;
	// skip blanks
	while ((*s > 0) && (*s <= 32))
		s++;

	tag = atoi(s);      // Tag auslesen
	while (*s > ' ')
		s++;
	// skip blanks
	while ((*s > 0) && (*s <= 32))
		s++;

	if ((s[1] == ':') || (s[2] == ':'))
	{  // Uhrzeit und kein Jahr angegeben
		jahr = year;
		memset(tempt, 0, sizeof(tempt));
		strncpy(tempt, s, 5);
		if (tempt[4] == ' ')
			tempt[4] = 0;
		strcat(tempt, ":00");
	}
	else
	{  // Jahr und keine Uhrzeit angegeben
		jahr = atoi(s);
		strcpy(tempt, "12:00:00");
	} // else

	dtime.dat_Format = FORMAT_USA;   // "mm-dd-yy"
	dtime.dat_Flags = 0;
	dtime.dat_StrDay = nil;
	dtime.dat_StrDate = tempd;
	dtime.dat_StrTime = tempt;

	tempd[0] = (monat / 10) % 10 + '0';
	tempd[1] = monat % 10 + '0';
	tempd[2] = '-';
	tempd[3] = (tag / 10) % 10 + '0';
	tempd[4] = tag % 10 + '0';
	tempd[5] = '-';
	tempd[6] = (jahr / 10) % 10 + '0';
	tempd[7] = jahr % 10 + '0';
	tempd[8] = 0;

	StrToDate(&dtime);
	if ((s[1] == ':') || (s[2] == ':'))
	{
		// Uhrzeit und kein Jahr ist angegeben; prüfe, ob erzeugtes Datum in der
		// Zukunft liegt; falls ja, dann Jahr um eins verringern
		struct DateStamp jetzt;

		DateStamp(&jetzt);
		if (dtime.dat_Stamp.ds_Days > jetzt.ds_Days)
		{
			jahr = year - 1;
			tempd[6] = (jahr / 10) % 10 + '0';
			tempd[7] = jahr % 10 + '0';
			StrToDate(&dtime);
		} // if
	} // if


	while (*s > ' ')
		s++;
	s++;
	// s steht jetzt auf Dateiname

	if (*s == '.')
	{
		// "." und ".." herausfiltern
		if ((s[1] == 0) || ((s[1] == '.') && (s[2] == 0)))
			return;

		// es gibt kein "hidden"-Flag; H steht für "hold" und wird nicht mehr
		// unterstützt
	} // if

	{
		unsigned char kommentar[256];

		strcpy(kommentar, user);
		strcat(kommentar, " / ");
		strcat(kommentar, group);
		if (linkname)
		{
			strcat(kommentar, ", -> ");
			strcat(kommentar, linkname);
		}

		add_ftpinfo(ih, s, kommentar, dtime.dat_Stamp, size, (size + 1023) / 1024, perm);
	} // sub block
} // add_info()




boolean get_list(site *sp, struct info_header *ih)
/*
 * gets LIST in cwd and puts it in ih
 * Inputs:
 * sp : site pointer
 * ih : info_header to hold list information
 *
 * Returns:
 * true if LIST was successful
 */
{
	tcpmessage *tm, *listm;
	struct MsgPort *sync;
	unsigned char reply[3], *info;
	unsigned short portn;
	unsigned long signals, asigs, rsigs;

	verify(sp, V_site);
	verify(ih, V_info_header);

	truth(sp->connected);
	truth(sp->cfile == nil);

	DS(kprintf("** get_list (site %s)\n", sp->host))

		state_change(sp, SS_LISTING);

	tm = sp->control;
	verify(tm, V_tcpmessage);

	sync = sp->sync;

	asigs = sp->disconnect_signals | sp->abort_signals;
	signals = (1 << sync->mp_SigBit) | asigs;

	listm = new_message(sp);
	if (!listm) return false;

	if (control_write(sp, "PASV\r\n", 0) == NO_ERROR)
	{
		if (!sp->quick || control_write(sp, "LIST -la\r\n", 0) == NO_ERROR)
		{
			if (response(sp, 0, &info, reply) == NO_ERROR)
			{
				if (reply[0] == '2' && info)
				{
					if (passive_response(info, sp->read_buffer, &portn))
					{
						deallocate(info, V_cstr);

						if (make_connection(sp, listm, sp->read_buffer, portn, 0) == NO_ERROR)
						{
							if (sp->quick || control_write(sp, "LIST -la\r\n", 0) == NO_ERROR)
							{
								/* this next response will be to the LIST */
								if (response(sp, 0, &info, reply) == NO_ERROR)
								{
									if (info) deallocate(info, V_cstr);

									if (reply[0] == '1')
									{
										/* list should be coming through listm now */

										goto read_list;
									}
								}
							}
							break_connection(sp, listm);
						}
					}
					else
					{
						if (info)
							deallocate(info, V_cstr);
					}
				}
				else
				{
					if (info)
						deallocate(info, V_cstr);
				}
			}
		}
	}

	listm->command = TCP_DISPOSE;
	PutMsg(tcp, &listm->header);

	disconnect(sp);

	return false;


read_list:
	listm->command = TCP_READ;
	listm->data = sp->read_buffer;
	listm->flags = FLAG_READLINE;
	listm->length = READ_BUFFER_LENGTH;
	listm->header.mn_ReplyPort = sync;

	do
	{
		PutMsg(tcp, &listm->header);
		rsigs = Wait(signals);
		if (rsigs & asigs)
		{
			state_change(sp, SS_ABORTING);

			interrupt_message(sp, listm);

			if (rsigs & sp->disconnect_signals)
			{
				break_connection(sp, listm);

				listm->command = TCP_DISPOSE;
				PutMsg(tcp, &listm->header);

				disconnect(sp);
				return false;
			}
		}
		else
		{
			GetMsg(sync);
		}

		if (listm->result > 0)
		{
			sp->read_buffer[listm->result] = 0;
			add_info(ih, sp->read_buffer);
		}
	} while (listm->error == NO_ERROR);

	break_connection(sp, listm);

	listm->command = TCP_DISPOSE;
	PutMsg(tcp, &listm->header);

	if (response(sp, 0, &info, reply) != NO_ERROR)
	{
		show_string("get list failed 8");

		disconnect(sp);

		return true;
	}

	if (info)
		deallocate(info, V_cstr);

#ifdef VERIFY
	if (reply[0] != '2')
	{
		show_string("received non-2 for end of LIST");
	}
#endif
	return true;
}



boolean prelim(site *sp, struct Window *w)
/*
 * once logged in, does preliminary setup stuff ... for now
 * sets TYPE I and figures out where the root of the fs is
 * Inputs:
 *    sp : site pointer
 *    w  : the connection cancel window
 *
 * Returns:
 * true if setup was successful
 */
{
	unsigned long csig;
	unsigned char *info, reply[3];
	unsigned char *s, *z;

	DS(kprintf("** preliminary setup stuff (site %s)\n", sp->host))

		csig = (1 << w->UserPort->mp_SigBit);

	if (control_write(sp, "TYPE I\r\n", csig) == NO_ERROR)
	{
		/* we either need to change to root, or work out where root is */
		if (sp->root)
		{
			sprintf(sp->read_buffer, "CWD %s\r\n", sp->root);
		}
		else {
			strcpy(sp->read_buffer, "PWD\r\n");
		}

		if (!sp->quick || control_write(sp, sp->read_buffer, csig) == NO_ERROR)
		{
			/* first response is to TYPE I */
			if (response(sp, csig, &info, reply) == NO_ERROR)
			{
				/* we don't really care what they replied */
				if (info)
					deallocate(info, V_cstr);

				if (sp->root)
				{
					sprintf(sp->read_buffer, "CWD %s\r\n", sp->root);
				}
				else {
					strcpy(sp->read_buffer, "PWD\r\n");
				}

				if (sp->quick || control_write(sp, sp->read_buffer, csig) == NO_ERROR)
				{
					/* ... next response is to CWD/PWD */
					if (response(sp, csig, &info, reply) == NO_ERROR)
					{
						if (reply[0] == '2')
						{
							if (sp->root)
							{
								/* was the CWD ... was successful */
								if (info)
									deallocate(info, V_cstr);

								return true;
							}
							else if (info)
							{
								/* was the PWD ... have to extract the root path */
								s = info;
								while (*s && *s != '"')
									s++;
								if (*s)
								{
									s++;
									z = s;
									while (*z && *z != '"')
										z++;
									if (*z)
									{
										sp->root = (unsigned char *)allocate(z - s + 1, V_cstr);
										if (sp->root)
										{
											if (z != s)
												memcpy(sp->root, s, z - s);

											sp->root[z - s] = 0;

											deallocate(info, V_cstr);
											return true;
										}
										else if (sp->error_messages)
											inform(sp->IBase, strings[MSG_OPERATIONAL_ERROR], strings[MSG_OOM_ROOT], nil, 0);
									}
									else if (sp->error_messages)
										inform(sp->IBase, strings[MSG_OPERATIONAL_ERROR], strings[MSG_PWD_GARBAGE], nil, 0);
								}
								else if (sp->error_messages)
									inform(sp->IBase, strings[MSG_OPERATIONAL_ERROR], strings[MSG_PWD_GARBAGE], nil, 0);
								deallocate(info, V_cstr);
							}
							else
							{
								if (sp->error_messages)
									inform(sp->IBase, strings[MSG_OPERATIONAL_ERROR], strings[MSG_FAILED_PWD], nil, 0);
							}
						}
						else {
							if (sp->error_messages)
								ok(sp->IBase, strings[MSG_OPERATIONAL_ERROR], info);
							if (info) deallocate(info, V_cstr);
						}
					}
					else if (sp->error_messages)
						inform(sp->IBase, strings[MSG_OPERATIONAL_ERROR], strings[MSG_ERROR_READING_PWD], nil, 0);
				}
				else if (sp->error_messages)
					inform(sp->IBase, strings[MSG_OPERATIONAL_ERROR], strings[MSG_ERROR_REQUESTING_PWD], nil, 0);
			}
			else if (sp->error_messages)
				inform(sp->IBase, strings[MSG_OPERATIONAL_ERROR], strings[MSG_ERROR_READING_TYPE], nil, 0);
		}
		else if (sp->error_messages)
			inform(sp->IBase, strings[MSG_OPERATIONAL_ERROR], strings[MSG_ERROR_REQUESTING_PWD], nil, 0);
	}
	else if (sp->error_messages)
		inform(sp->IBase, strings[MSG_OPERATIONAL_ERROR], strings[MSG_ERROR_SETTING_TYPE], nil, 0);

	return false;
}


void login(site *sp, struct Window *w)
/*
 * goes through the login sequence once a successful connection has been established
 * Inputs:
 * sp : site pointer
 * w  : the connection cancel window
 */
{
	tcpmessage *tm;
	struct MsgPort *sync;
	unsigned char reply[4], *info;
	unsigned long csig;
	boolean early_success = false;

	tm = sp->control;
	sync = sp->sync;

	DS(kprintf("** login (site %s)\n", sp->host))

		state_change(sp, SS_LOGIN);

	csig = 1 << w->UserPort->mp_SigBit;

retry_login:
	// falls nötig, per Requester nach Login und Passwort fragen
	if (sp->needs_user || sp->needs_password)
	{
		if (!sp->error_messages || !user_pass_request(sp, w))
		{
			tm->command = TCP_CLOSE;
			PutMsg(tcp, &tm->header);
			WaitPort(sync); GetMsg(sync);

			close_req(sp, w);

			state_change(sp, SS_DISCONNECTED);
			return;
		}
	}

	if (sp->user)
	{
		sprintf(sp->read_buffer, "USER %s\r\n", sp->user);
	}
	else {
		strcpy(sp->read_buffer, "USER ftp\r\n");
	}

	if (control_write(sp, sp->read_buffer, csig) == NO_ERROR)
	{
		if (sp->password)
		{
			sprintf(sp->read_buffer, "PASS %s\r\n", sp->password);
		}
		else {
			sprintf(sp->read_buffer, "PASS %s\r\n", anon_login);
		}

		if (control_write(sp, sp->read_buffer, csig) == NO_ERROR)
		{
			/* first response should be to the USER */

			switch (response(sp, csig, &info, reply))
			{
			case NO_ERROR:
				switch (reply[0])
				{
				case '2':
					// kein Passwort nötig; dies merken
					early_success = true;

					/* the welcome banner will come here I guess */
					if (sp->all_messages && !sp->read_banners)
					{
						ok(sp->IBase, strings[MSG_LOGIN_SUCCEEDED_NO_PASS], info);
						sp->read_banners = true;
					}
					/* fall through */

				case '3':
					/* ignore the banner here ... usually its just "Anonymous login ok, send ident ..." */
					if (info)
						deallocate(info, V_cstr);

					/* now read pass response */
					switch (response(sp, csig, &info, reply))
					{
					case NO_ERROR:
						/* if we succeeded early (d. h. kein Passwort ist erforderlich),
						   we don't care what they tell us */
						if (!early_success)
						{
							if (reply[0] == '2')
							{
								if (sp->all_messages && !sp->read_banners)
								{
									ok(sp->IBase, strings[MSG_LOGIN_SUCCEEDED], info);
									sp->read_banners = true;
								}
							}
							else if (reply[0] == '3')
							{
								/* they want an ACCT ... fuck 'em */

								if (sp->error_messages)
									inform(sp->IBase, strings[MSG_LOGIN_FAILED], strings[MSG_ACCT_REQUESTED], nil, 0);
								if (info)
									deallocate(info, V_cstr);
								break;
							}
							else
							{
								if (reply[0] == '5' && reply[1] == '3' && reply[2] == '0')
								{
									/* this is login incorrect */
									if (sp->error_messages && retry_cancel(sp->IBase, strings[MSG_LOGIN_INCORRECT], info))
									{
										if (info)
											deallocate(info, V_cstr);
										// Site erfordert also Passwort; dies merken und evt.
										// aktuelles wegschmeissen, da es falsch ist
										sp->needs_password = true;
										if (sp->password)
											deallocate(sp->password, V_cstr);
										sp->password = nil;
										goto retry_login;
									}
									if (info)
										deallocate(info, V_cstr);
									break;
								}

								if (sp->error_messages)
									ok(sp->IBase, strings[MSG_LOGIN_FAILED_PASS], info);
								if (info)
									deallocate(info, V_cstr);
								break;
							}
						}

						if (info)
							deallocate(info, V_cstr);

						if (prelim(sp, w))
						{
							close_req(sp, w);
							sp->connected = true;

							state_change(sp, SS_IDLE);
							return;
						}

						break;
					case ERROR_INTERRUPTED:
						break;
					case ERROR_LOST_CONNECTION:
					case ERROR_EOF:
					case ERROR_UNREACHABLE:
						if (sp->error_messages)
							inform(sp->IBase, strings[MSG_LOGIN_ERROR], strings[MSG_LOST_CONN_DURING_LOGIN_PASS], nil, 0);
						break;
					case ERROR_GARBAGE_RECEIVED:
						if (sp->error_messages)
							inform(sp->IBase, strings[MSG_LOGIN_ERROR], strings[MSG_GARBAGE_RECEIVED_PASS], nil, 0);
						break;
					default:
						if (sp->error_messages)
							inform(sp->IBase, strings[MSG_LOGIN_ERROR], strings[MSG_ERROR_RESPONSE_PASS], nil, 0);
						break;
					}
					break;
				case '4':
					if (sp->error_messages && retry_cancel(sp->IBase, strings[MSG_TEMP_LOGIN_FAILURE_USER], info))
					{
						if (info)
							deallocate(info, V_cstr);
						goto retry_login;
					}
					if (info)
						deallocate(info, V_cstr);
					break;
				default:
					if (sp->error_messages)
						ok(sp->IBase, strings[MSG_LOGIN_FAILED_USER], info);
					if (info)
						deallocate(info, V_cstr);
					break;
				}
				break;
			case ERROR_INTERRUPTED:
				break;
			case ERROR_LOST_CONNECTION:
			case ERROR_EOF:
			case ERROR_UNREACHABLE:
				if (sp->error_messages)
					inform(sp->IBase, strings[MSG_LOGIN_ERROR], strings[MSG_LOST_CONN_DURING_LOGIN], nil, 0);
				break;
			case ERROR_GARBAGE_RECEIVED:
				if (sp->error_messages)
					inform(sp->IBase, strings[MSG_LOGIN_ERROR], strings[MSG_GARBAGE_RECEIVED_USER], nil, 0);
				break;
			default:
				if (sp->error_messages)
					inform(sp->IBase, strings[MSG_LOGIN_ERROR], strings[MSG_ERROR_USER_RESPONSE], nil, 0);
				break;
			}
		}
		else if (sp->error_messages)
			inform(sp->IBase, strings[MSG_LOGIN_ERROR], strings[MSG_ERROR_WRITING_PASS], nil, 0);
	}
	else if (sp->error_messages)
		inform(sp->IBase, strings[MSG_LOGIN_ERROR], strings[MSG_ERROR_WRITING_USER], nil, 0);

	tm->command = TCP_CLOSE;
	PutMsg(tcp, &tm->header);
	WaitPort(sync); GetMsg(sync);

	close_req(sp, w);

	state_change(sp, SS_DISCONNECTED);

	return;
}



void init_connect(site *sp)
{
	struct Window *w;
	unsigned char *z;
	tcpmessage *tm, *intr;
	struct MsgPort *sync;
	unsigned char reply[3], *info;
	unsigned long signals, csig;

	verify(sp, V_site);

	z = sp->host;

	while (sp->infos)
		free_info_header(sp->infos);

	w = connect_req(sp, z);
	if (!w)
	{
		show_string("connect req failed");
		return;
	}

	DS(kprintf("** init_connect (site %s)\n", sp->host))

		state_change(sp, SS_CONNECTING);

	tm = sp->control;
	sync = sp->sync;
	intr = sp->intr;

	csig = (1 << w->UserPort->mp_SigBit) | sp->abort_signals | sp->disconnect_signals;
	signals = (1 << sync->mp_SigBit) | csig;

	// falls keine Port-Nummer angegeben ist, diese besorgen
	if (sp->port_number == 0)
	{
		tm->command = TCP_SERVICE;
		tm->data = strings[MSG_SERVICE];
		tm->header.mn_ReplyPort = sync;

		PutMsg(tcp, &tm->header);
		WaitPort(sync); GetMsg(sync);

		if (tm->result)
		{
			sp->port_number = ftp_port_number = tm->port.w;
		}
		else if (tm->error == ERROR_NO_CONNECTION)
		{
			close_req(sp, w);

			if (sp->error_messages)
				inform(sp->IBase, strings[MSG_CONNECT_ERROR], strings[MSG_AMITCP_NOT_RUNNING], nil, 0);

			state_change(sp, SS_DISCONNECTED);

			return;
		}
		else
		{
			sp->port_number = 21;
		}
	}


	tm->command = TCP_CONNECT;
	tm->header.mn_ReplyPort = sync;
	tm->data = z;
	tm->port.w = sp->port_number;

	PutMsg(tcp, &tm->header);

	do
	{
		if (Wait(signals) & csig)
		{
			intr->interrupt = tm;
			PutMsg(tcp, &intr->header);
			WaitPort(sync); GetMsg(sync);
			WaitPort(sync); GetMsg(sync);

			if (tm->result)
			{  /* it succeeded in connecting */
				tm->command = TCP_CLOSE;
				PutMsg(tcp, &tm->header);
				WaitPort(sync); GetMsg(sync);
			}

			close_req(sp, w);

			state_change(sp, SS_DISCONNECTED);

			return;
		}
	} while (!GetMsg(sync));

	if (!tm->result)
	{   /* the connect failed ... tell the user why */
		close_req(sp, w);

		switch (tm->error)
		{
		case ERROR_NO_CONNECTION:
			if (sp->error_messages)
				inform(sp->IBase, strings[MSG_CONNECT_ERROR], strings[MSG_AMITCP_NOT_RUNNING], nil, 0);
			break;
		case ERROR_UNKNOWN_HOST:
			if (sp->error_messages)
				inform(sp->IBase, strings[MSG_CONNECT_ERROR], strings[MSG_HOST_UNKNOWN], z, 0);
			break;
		case ERROR_UNREACHABLE:
			if (sp->error_messages)
				inform(sp->IBase, strings[MSG_CONNECT_ERROR], strings[MSG_HOST_UNREACHABLE], z, 0);
			break;
		case ERROR_CONNECT_REFUSED:
			if (sp->error_messages)
				inform(sp->IBase, strings[MSG_CONNECT_ERROR], strings[MSG_FTP_REFUSED], z, 0);
			break;
		default:
			if (sp->error_messages)
				inform(sp->IBase, strings[MSG_CONNECT_ERROR], strings[MSG_CANT_CONNECT], z, tm->error);
			break;
		}

		state_change(sp, SS_DISCONNECTED);

		return;
	}

	/* ok, we've connected ... look at the greeting */

retry_intro:
	switch (response(sp, csig, &info, reply))
	{
	case NO_ERROR:
		break;
	case ERROR_INTERRUPTED:
		close_req(sp, w);
		goto close_and_exit;
	case ERROR_LOST_CONNECTION:
	case ERROR_EOF:
	case ERROR_UNREACHABLE:
		close_req(sp, w);
		if (sp->error_messages)
			inform(sp->IBase, strings[MSG_CONNECT_ERROR], strings[MSG_LOST_CONN_DURING_INTRO], nil, 0);
		goto close_and_exit;
	case ERROR_GARBAGE_RECEIVED:
		close_req(sp, w);
		if (sp->error_messages)
			inform(sp->IBase, strings[MSG_CONNECT_ERROR], strings[MSG_GARBAGE_DURING_INTRO], z, 0);
		goto close_and_exit;
	default:
		close_req(sp, w);
		if (sp->error_messages)
			inform(sp->IBase, strings[MSG_CONNECT_ERROR], strings[MSG_ERROR_DURING_INTRO], nil, 0);
		goto close_and_exit;
	}

	switch (reply[0])
	{
	case '1':
		if (sp->error_messages && retry_cancel(sp->IBase, strings[MSG_CONN_DELAY], info))
		{
			goto retry_intro;
		}
		close_req(sp, w);
		if (info)
			deallocate(info, V_cstr);

		goto close_and_exit;
	case '2':
	case '3':
		/* This banner appears to be generally pretty dull, but if
		 * you really want to see it then remove the comments ...
		 * if (!sp->read_banners)
		 * {
		 *    ok(sp->IBase, "Connected", info);
		 * }
		 */

		if (info)
			deallocate(info, V_cstr);

		login(sp, w);
		return;
	case '4':
		if (retry_cancel(sp->IBase, strings[MSG_TEMP_CONN_FAILURE], info))
		{
			goto retry_intro;
		}
		close_req(sp, w);
		if (info)
			deallocate(info, V_cstr);

		goto close_and_exit;
	case '5':
	default:
		close_req(sp, w);

		if (sp->error_messages)
			ok(sp->IBase, strings[MSG_CONN_FAILED], info);

		if (info)
			deallocate(info, V_cstr);

		break;
	}

close_and_exit:
	tm->command = TCP_CLOSE;
	PutMsg(tcp, &tm->header);
	WaitPort(sync); GetMsg(sync);

	state_change(sp, SS_DISCONNECTED);

	return;
}

