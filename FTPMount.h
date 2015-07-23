/* All includes for FTPMount in one file - R.Riedel 2003-03-08
* AmigaOS4 port (c) 2005-2006 Alexandre BALABAN (alexandre (at) free (dot) fr)
*/

/* activate this to build debug-versions! */
/*
#define DEBUGSTATUS 1
*/

#define __USE_INLINE__ /* 2005-05-14 ABA : OS4 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define __USE_BASETYPE__ /* 2005-05-14 ABA : OS4 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dostags.h>
#include <intuition/sghooks.h>
#include <devices/timer.h>
#include <clib/alib_protos.h>

/*
#ifndef	__MORPHOS__
#include <clib/alib_stdio_protos.h>
#endif
*/

#include <clib/debug_protos.h>
#include <proto/commodities.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <proto/locale.h>
#include <proto/wb.h>

#ifndef __SASC
#define INLINES_AS_MACROS 1	/* SAS doesn't seem to do inlines properly */
#endif

#ifndef	__MORPHOS__
#include <proto/bsdsocket.h> /* 2003-03-02 rri */
#else
#include	 <proto/socket.h>
#endif

#include <sys/errno.h>
#include <sys/ioccom.h> /* 2003-03-02 rri */
#include <netdb.h>

#define FIONBIO _IOW('f', 126, long) /* set/clear non-blocking i/o */  /* 2003-03-02 rri */
#define stci_d(out, ival)     	   sprintf((out),"%d", (ival))

#ifdef __amigaos4__
#undef CreateExtIO
#define CreateExtIO( mp, size ) AllocSysObjectTags(  ASOT_IOREQUEST, \
															ASOIOR_Size, (size), \
															ASOIOR_ReplyPort, (mp), \
                                                            ASO_NoTrack, 0, \
															TAG_DONE)
#undef DeleteExtIO
#define DeleteExtIO( io )       FreeSysObject(ASOT_IOREQUEST, (io));
int atoi(const char *string);
#endif

#ifdef __MORPHOS__
#define	sprintf(buf, fmt, tags...)	NewRawDoFmt(fmt, 0, buf, tags)
int atoi(const char *string);
static inline int atol(const char *string)
{
	return atoi(string);
}

static inline void *malloc(int size)
{
	return AllocVecTaskPooled(size);
}

static inline void free(void *p)
{
	FreeVecTaskPooled(p);
}

#endif

/* twdebug.h */

#ifdef DEBUGSTATUS
/* void kprintf(UBYTE *fmt,...); */
#define DS(x)  x;
#else
#define DS(x)
#endif

#if !defined(__amigaos4__) /* 2005-05-14 ABA : OS4 already defines what's needed */
# if !defined(__MORPHOS__)
#ifdef __SASC
# define SAVEDS __saveds
# define ASM __asm
# define REG(x,y) register __##x y
#elif defined(__GNUC__)
# define SAVEDS __saveds
# define ASM
# define REG(x,y) y __asm(#x)
#elif defined(_DCC)
# define SAVEDS __geta4
# define ASM
# define REG(x,y) __##x y
#elif defined(__STORM__)
# define SAVEDS __saveds
# define ASM
# define REG(x,y) register __##x y
#elif defined(__VBCC__)
# define SAVEDS __saveds
# define ASM
# define REG(x,y) __reg(#x) y
#else
# error add #defines for your compiler...
#endif
#endif
#endif

/* evtypes.h */
typedef unsigned long b32;
typedef unsigned char b8;
typedef unsigned short b16;

typedef signed long sb32;
typedef signed char sb8;
typedef signed short sb16;

typedef void(*ptf)();

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#ifdef NULL
#undef NULL
#endif

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#ifdef NULL
#undef NULL
#endif

static void * const nil = 0;
static void * const NULL = 0;

typedef enum { false = (1 == 0), true = (1 == 1) } boolean;

#if !defined(__amigaos4__)
/*#ifdef AMIGA */
/*#define strcasecmp strncmp */
#define strnicmp strncasecmp
#define stricmp strcasecmp
/*#endif */
#endif



/* NB: this only works for y a power of 2 */
#define ROUND_UP(x, y) ((x + y - 1) & (~(y - 1)))

#include "verify.h"

#define V_DosList 17519
#define V_StandardPacket 21364

#define V_bstr 25203
#define V_cstr 25458

#define V_lock 27759

typedef struct my_lock {
	struct FileLock fl;

	magic_verify;

	struct my_lock *next;
	struct MsgPort *port;
	b32 rfsl, lastkey;
	b8 fname[0];
} lock;

#define V_file_info 26217

#define FIRST_BLOCK_SIZE 512
#define MAX_READ_SIZE 2048

typedef struct my_file_info
{
	magic_verify;
	struct my_file_info *next;
	b32 rfarg; /* real file arg */
	b32 rpos, vpos, end; /* real file position, virtual file position, file end */
	struct MsgPort *port;
	struct tcpm *tm;
	b16 type;
	boolean seek_end, eof, closed;
	b8 first_block[FIRST_BLOCK_SIZE];
	b8 fname[0];
} file_info;

/* special internal packets */

#define action_IDLE 2050
#define action_IDLE_DEATH 2051
#define action_SUSPEND 2052

#define VERSION "1"
#define REVISION "5"

#ifdef DECLARE_GLOBALS_HERE
#define global
#else
#define global extern
#endif

global struct ExecBase *SysBase;
global struct DosLibrary *DOSBase;
global struct IntuitionBase *IntuitionBase;
global struct GfxBase *GfxBase;
global struct Library *GadToolsBase;
global struct Library *IconBase;

#if defined(__MORPHOS__) || defined(__amigaos4__)
global struct Library *LocaleBase; /* 12-04-04 itix */
#else
global struct LocaleBase *LocaleBase; /* 03-03-01 rri */
#endif

#ifdef __amigaos4__
global struct ExecIFace         *IExec;
global struct DOSIFace          *IDOS;
global struct IntuitionIFace	 *IIntuition;
global struct GraphicsIFace 	 *IGraphics;
global struct GadToolsIFace 	 *IGadTools;
global struct IconIFace 		 *IIcon;
global struct LocaleIFace 		 *ILocale;

#define IntuitionParamDef struct IntuitionIFace * IIntuition
#define IntuitionParam IIntuition
#define GraphicsParamDef struct GraphicsIFace * IGraphics
#define GraphicsParam IGraphics
#define GadToolsParamDef struct GadToolsIFace * IGadTools
#define GadToolsParam IGadTools
#else
#define IntuitionParamDef struct IntuitionBase *IntuitionBase
#define IntuitionParam IntuitionBase
#define GraphicsParamDef struct GfxBase *GfxBase
#define GraphicsParam GfxBase
#define GadToolsParamDef struct Library *GadToolsBase
#define GadToolsParam GadToolsBase
#endif

global struct MsgPort *ftp_port, *tcp, *startup_sync, *broker_port;
global struct DosList *ftp_device, *ftp_volume;

global BPTR ftpdir_lock, ftphosts_lock;

global struct tcpm *prime; /* the numero uno tcp message */

global unsigned char unique_buffer[25];

global unsigned short ftp_port_number;
global struct site_s *sites;

global unsigned char *anon_login;

global struct Message *local_msg;
global struct MsgPort *local_port;

global struct gim *cancel_gim, *abort_gim, *disconnect_gim, *login_gim;

global struct status_message *status_mess;
global struct MsgPort *status_control, *status_port;

global unsigned long darkpen, lightpen, textpen, fillpen;

global struct my_lock *orphaned_locks;

global unsigned short year;

global struct Locale *my_locale;
global struct Catalog *cat;

global unsigned char *volume_name;

#ifdef DECLARE_GLOBALS_HERE
unsigned char *ver = (unsigned char*)"$VER: FTPMount " VERSION "." REVISION " (2006-11-27)";  /* 03-03-09 */
#endif
