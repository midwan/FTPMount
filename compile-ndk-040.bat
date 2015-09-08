vc -cpu=68040 -c99 -c -I%NDK_INC% -I%Roadshow%\netinclude -o obj-040\connect.o connect.c
vc -cpu=68040 -c99 -c -I%NDK_INC% -I%Roadshow%\netinclude -o obj-040\startup.o startup.c
vc -cpu=68040 -c99 -c -I%NDK_INC% -I%Roadshow%\netinclude -o obj-040\ftpinfo.o ftpinfo.c
vc -cpu=68040 -c99 -c -I%NDK_INC% -I%Roadshow%\netinclude -o obj-040\listen.o listen.c
vc -cpu=68040 -c99 -c -I%NDK_INC% -I%Roadshow%\netinclude -o obj-040\local.o local.c
vc -cpu=68040 -c99 -c -I%NDK_INC% -I%Roadshow%\netinclude -o obj-040\site.o site.c
vc -cpu=68040 -c99 -c -I%NDK_INC% -I%Roadshow%\netinclude -o obj-040\split.o split.c
vc -cpu=68040 -c99 -c -I%NDK_INC% -I%Roadshow%\netinclude -o obj-040\tcp.o tcp.c
vc -cpu=68040 -c99 -c -I%NDK_INC% -I%Roadshow%\netinclude -o obj-040\request.o request.c
vc -cpu=68040 -c99 -c -I%NDK_INC% -I%Roadshow%\netinclude -o obj-040\decrypt_password.o decrypt_password.c

cd obj-040

vc -o ..\FTPHandler-040 decrypt_password.o connect.o ftpinfo.o listen.o local.o request.o site.o split.o startup.o tcp.o -lamiga -lposix
