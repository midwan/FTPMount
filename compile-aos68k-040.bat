REM compile for 040
vc -c99 -cpu=68040 -I%RS_INC% -I%POSIX_INC% -lamiga -lposix startup.c connect.c ftpinfo.c listen.c local.c site.c split.c tcp.c request.c decrypt_password.c  -o bin\FTPMount-Hanlder-040

REM vc -cpu=68040 -c99 -c -I%Roadshow%\netinclude -o obj-040\connect.o connect.c
REM vc -cpu=68040 -c99 -c -I%Roadshow%\netinclude -o obj-040\startup.o startup.c
REM vc -cpu=68040 -c99 -c -I%Roadshow%\netinclude -o obj-040\ftpinfo.o ftpinfo.c
REM vc -cpu=68040 -c99 -c -I%Roadshow%\netinclude -o obj-040\listen.o listen.c
REM vc -cpu=68040 -c99 -c -I%Roadshow%\netinclude -o obj-040\local.o local.c
REM vc -cpu=68040 -c99 -c -I%Roadshow%\netinclude -o obj-040\site.o site.c
REM vc -cpu=68040 -c99 -c -I%Roadshow%\netinclude -o obj-040\split.o split.c
REM vc -cpu=68040 -c99 -c -I%Roadshow%\netinclude -o obj-040\tcp.o tcp.c
REM vc -cpu=68040 -c99 -c -I%Roadshow%\netinclude -o obj-040\request.o request.c
REM vc -cpu=68040 -c99 -c -I%Roadshow%\netinclude -o obj-040\decrypt_password.o decrypt_password.c

REM cd obj-040

REM vc -o ..\FTPHandler-040 decrypt_password.o connect.o ftpinfo.o listen.o local.o request.o site.o split.o startup.o tcp.o -lamiga -lposix
