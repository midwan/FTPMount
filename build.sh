vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$Roadshow_Netinclude -o obj/connect.o connect.c
vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$Roadshow_Netinclude -o obj/startup.o startup.c
vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$Roadshow_Netinclude -o obj/ftpinfo.o ftpinfo.c
vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$Roadshow_Netinclude -o obj/listen.o listen.c
vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$Roadshow_Netinclude -o obj/local.o local.c
vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$Roadshow_Netinclude -o obj/site.o site.c
vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$Roadshow_Netinclude -o obj/split.o split.c
vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$Roadshow_Netinclude -o obj/tcp.o tcp.c
vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$Roadshow_Netinclude -o obj/request.o request.c
vc +aos68k -c99 -cpu=68040 -c -I$NDK_INC -I$Roadshow_Netinclude -o obj/decrypt_password.o decrypt_password.c

cd obj/

vc +aos68k -c99 -cpu=68040 -L$NDK_LIB -o FTPHandler-040 decrypt_password.o connect.o ftpinfo.o listen.o local.o request.o site.o split.o startup.o tcp.o -lamiga
