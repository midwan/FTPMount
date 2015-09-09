/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

#if defined(VERIFY) || defined(MINI_VERIFY)
/* #include <exec/lists.h>
	#include <exec/nodes.h> */

#define magic_verify	unsigned long	magic
#define MAGIC 0x49a72fc0
#define verify(x,y)	{ if ((x)->magic!=(y)) verify_alert(__FILE__,__LINE__,#x,#y); }
#define ensure(x,y)	((x)->magic=(y))
#define truth(x)	{ if (!(x)) truth_alert(__FILE__,__LINE__,#x); }
#define falsity(x)	{ if (x) false_alert(__FILE__,__LINE__,#x); }
#define show_int(x)	int_alert(__FILE__,__LINE__,#x, (int)x);
#define show_string(x)	string_alert(__FILE__,__LINE__,#x, x);

void verify_alert(char *, int, char *, char *);
void truth_alert(char *, int, char *);
void false_alert(char *, int, char *);
void int_alert(char *, int, char *, int);
void string_alert(char *, int, char *, char *);

#define allocate(x,y)	track_malloc(x,y,0,__FILE__,__LINE__,#x,#y)
#define allocate_flags(x,f,y)	track_malloc(x,y,f,__FILE__,__LINE__,#x,#y)
#define deallocate(x,y)	track_free(x,y,__FILE__,__LINE__,#x,#y)
#define disown(x,y) track_disown(x,y,__FILE__,__LINE__,#x,#y)
#define adopt(x,y) track_adopt(x,y,__FILE__,__LINE__,#x,#y)
#define mem_tracking_on() track_init()
#define check_memory() track_check()

struct bink {
	struct bink *next;
	struct bink **prev;
	unsigned long size;
	unsigned long type;
	int line;
	char *file, *typename;
};

void *track_malloc(int, unsigned long, unsigned long, char *, int, char *, char *);
void track_free(void *, unsigned long, char *, int, char *, char *);
void track_disown(void *, unsigned long, char *, int, char *, char *);
void track_adopt(void *, unsigned long, char *, int, char *, char *);
void track_init(void);
void track_check(void);

#else
#define magic_verify	char nothing[1]
#define MAGIC 0x49a72fc0
#define verify(x,y)
#define ensure(x,y)
#define truth(x)
#define falsity(x)
#define show_int(x)
#define show_string(x)

#define allocate(x,y)	AllocVec(x, 0)
#define allocate_flags(x,f,y)	AllocVec(x,f)
#define deallocate(x,y)	FreeVec(x)
#define disown(x,y)
#define adopt(x,y)
#define mem_tracking_on()
#define check_memory()
#endif
