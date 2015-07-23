# OS4 Makefile copyright 2005-2006 Alexandre Balaban
#

# compilation stuffs
CC 	    = ppc-amigaos-gcc
#CDEFS   = -ggdb -DMINI_VERIFY -DUSE_INLINE_STDARG -DAROS_ALMOST_COMPATIBLE
CDEFS   = -DUSE_INLINE_STDARG -DAROS_ALMOST_COMPATIBLE -ggdb
CWARN   = -Wall -Wno-parentheses -Wno-missing-braces
CFLAGS  = $(CDEFS) $(CWARN) -O2 -fomit-frame-pointer -mcpu=750 -finline-functions -mstring -mmultiple
LDFLAGS = -ggdb
STRIP   = ppc-amigaos-strip
TARGET  = FTPMount-Handler
OBJS	= startup.o tcp.o connect.o listen.o ftpinfo.o split.o site.o local.o request.o decrypt_password.o

# release related stuffs
CP = copy CLONE
RM = delete
DIST_DIR    = release
DIST_ROOT   = $(DIST_DIR)/FTPMount
DIST_L      = $(DIST_ROOT)/L
DIST_FILES  = $(DIST_ROOT)/FTPMountDir
DIST_SRC    = $(DIST_FILES)/Source
ARC_NAME    = FTPMount.lha
DIST_ARC    = $(DIST_DIR)/$(ARC_NAME)

ARC         = lha
AR_FLAGS    = -r u
ECHO        = echo
ED          = ed

# rules
all	: $(TARGET)

dist : $(DIST_ARC)

$(DIST_ARC) : $(TARGET)
	@$(ECHO) Copying Handlers...
	@$(CP) $(TARGET) $(TARGET).db $(DIST_L)
	@$(ECHO) Removing old sources...
	@$(RM) $(DIST_SRC)/#?
	@$(ECHO) Copying new sources...
	@$(CP) *.c *.h $(DIST_SRC)
	@$(CP) Makefile smakefile $(DIST_SRC)
	@$(ECHO) Editing the readme...
	@$(ED) $(DIST_FILES)/FTPMount.readme
	@$(ECHO) Creating the archive...
	@$(ARC) $(AR_FLAGS) $(DIST_ARC) $(DIST_DIR)/ FTPMount

clean :
	$(RM) $(OBJS)

$(TARGET): $(OBJS)
		  $(CC) $(LDFLAGS) -nostdlib -o $@.db $(OBJS) -lc -lgcc
		  $(STRIP) --remove-section=.comment $@.db -o $@

connect.o: connect.c verify.h tcp.h site.h split.h request.h strings.h connect.h ftpinfo.h
decrypt_password.o: decrypt_password.c verify.h
ftpinfo.o: ftpinfo.c verify.h tcp.h site.h split.h ftpinfo.h connect.h
listen.o	: listen.c  verify.h tcp.h site.h split.h request.h
local.o	: local.c	verify.h tcp.h site.h split.h local.h
request.o: request.c verify.h site.h strings.h request.h
site.o	: site.c		verify.h tcp.h site.h split.h request.h strings.h ftpinfo.h
split.o	: split.c	verify.h tcp.h site.h split.h
startup.o: startup.c verify.h tcp.h site.h local.h request.h strings.h verify_code.h
tcp.o		: tcp.c verify.h tcp.h
