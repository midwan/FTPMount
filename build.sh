# Compile for 68000
vc +aos68k -c99 -I$NDK_INC -I$RS_INC -I$POSIX_INC -lamiga -lposix *.c -o bin/FTPMount-Handler

# Compile for 68060
vc +aos68k -c99 -cpu=68060 -I$NDK_INC -I$RS_INC -I$POSIX_INC -lamiga -lposix *.c -o bin/FTPMount-Handler-060

# Compile files only (don't link)
#vc +aos68k -c99 -I$NDK_INC -I$RS_INC -I$POSIX_INC -lamiga -c *.c

# Manually link all files
#vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib $VBCC/targets/m68k-amigaos/lib/startup.o obj/*.o -lamiga -lposix -s -Rshort -L$VBCC/targets/m68k-amigaos/lib -lvc -o FTPMountHandler

# Compile each file separately
#vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$RS_INC -o obj/connect.o connect.c
#vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$RS_INC -o obj/startup.o startup.c
#vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$RS_INC -o obj/ftpinfo.o ftpinfo.c
#vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$RS_INC -o obj/listen.o listen.c
#vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$RS_INC -o obj/local.o local.c
#vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$RS_INC -o obj/site.o site.c
#vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$RS_INC -o obj/split.o split.c
#vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$RS_INC -o obj/tcp.o tcp.c
#vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$RS_INC -o obj/request.o request.c
#vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$RS_INC -o obj/decrypt_password.o decrypt_password.c

#cd obj/

#vc +aos68k -c99 -cpu=68040 -L$NDK_LIB -o FTPHandler-040 decrypt_password.o connect.o ftpinfo.o listen.o local.o request.o site.o split.o startup.o tcp.o -lamiga
