#
# main
#
CC = vc
CFLAGS =-c -c99 -IC:/VBCC/targets/m68k-amigaos/include/ -IC:/VBCC/include_h/
BIN = head
LIBS =-LC:/VBCC/targets/m68k-amigaos/lib -lvc -lamiga
LINK = vlink -bamigahunk -Bstatic -CVBCC -nostdlib -s -x
c:/VBCC/targets/m68k-amigaos/lib/startup.o
OBJ = head.o
LINKOBJ = head.o
RM = DEL
.PHONY: all clean
all: $(BIN)
clean:
$(RM) $(OBJ) $(BIN)
$(BIN):	$(OBJ)
$(LINK) $(LINKOBJ) -o $(BIN) $(LIBS)
head.o:	head.c
$(CC) $(CFLAGS) head.c -o head.o
