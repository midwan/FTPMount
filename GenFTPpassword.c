;/* execute to compile with SAS/C
;SC GenFTPpassword.c decrypt_password.o IGNORE 73 DEBUG=SF NOCHKABORT NOSTKCHK LINK
;SC GenFTPpassword.c IGNORE 73 DEBUG=SF NOCHKABORT NOSTKCHK LINK
SC GenFTPpassword.c IGNORE 73 OPT CPU=68000 NOCHKABORT NOSTKCHK STRIPDEBUG LINK
Delete GenFTPpassword.(o|lnk)
Quit
*/

/**************************************************************************

GenFTPpassword.c  --  Erzeuge verschlüsseltes Passwort für FTPMount

AUTOR(EN):        Thies Wellpott
ERSTELLUNG:       09.04.1998
COPYRIGHT:        (c) 1998 Thies Wellpott
BETRIEBSSYSTEM:   AmigaOS >= 2.04
COMPILER:         SAS/C >= 6.50

BESCHREIBUNG:

Verschlüsselt das als Programmargument im Klartext übergebene Passwort
für die Verwendung im PASSWORDCRYPT-Tooltype von FTPMount >= 1.1.


FEHLER/EINSCHRÄNKUNGEN:

keine


ENTWICKLUNGSGESCHICHTE:

V1.000   09.04.1998   Thies Wellpott
- Erstellung der Datei
- Fertigstellung

V1.001   20.04.1998   Thies Wellpott
- srand() benutzt argv zusätzlich

V1.002   12.05.1998   Thies Wellpott
- decrypt_password() für Testdekodierung benutzt
- Klartextpasswort wird ausgegeben (um Fehler bei Sonderzeichen zu erkennen)

**************************************************************************/


#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <stdio.h>


typedef unsigned char UBYTE;


const char *version = "\0$VER: GenFTPpassword 1.1 " __AMIGADATE__;

#define ZEICHEN1(l)  ( (char)((l >> 4) & 0xf) * 4 + 42)
#define ZEICHEN2(l)  ( (char)((l) & 0xf) * 3 + 47)



int main(int argc, char *argv[])
{
   short i;
   long l, zufallszahl;
   UBYTE passwort[64], verschluesseltes[256], *c;

   if ( (argc != 2) || ((argv[1][0] == '?') && (argv[1][1] == 0)) ||
        (strlen(argv[1]) >= sizeof(passwort)) )
   {
      printf("%s - FreeWare (c) 1998 Thies Wellpott\n\nUsage: %s password/A  (max. %d chars)\n"
             "\nPut resulting string (via copy & paste) into PASSWORDCRYPT\ntooltype of your ftp-sites icon.\n",
            &version[7], argv[0], sizeof(passwort) - 1);
      return 5;
   } // if

   srand(time(NULL) + (int)argv);

   // Passwortpuffer mit Zufallsbyte init. und Passwort kopieren
   zufallszahl = rand() >> 5;
   memset(passwort, zufallszahl, sizeof(passwort));
   strcpy(passwort, argv[1]);

   // Verschlüsselungspuffer mit 0 init.
   memset(verschluesseltes, 0, sizeof(verschluesseltes));

   c = verschluesseltes;
   // alle Zeichen (auch das 0-Byte!) in 3er-Blöcken durchlaufen
   for (i = 0;  i <= strlen(passwort);  i += 3)
   {
      // Zufallszahl < 256 besorgen;
      zufallszahl = (rand() >> 9) & 0xff;
      // 3 Zeichen in long kombinieren
      //printf("z1: %d  z2: %d  z3: %d\n", (int)passwort[i], (int)passwort[i+1], (int)passwort[i+2]);
      l = ((long)passwort[i] << 8) | ((long)passwort[i + 1] << 16) | ((long)passwort[i + 2]);
      l = (l * zufallszahl) ^ (zufallszahl * zufallszahl * zufallszahl * zufallszahl);

      // und 10 Zeichen in Verschlüsselpungspuffer
      *c++ = ZEICHEN1(l >> 24);
      *c++ = ZEICHEN2(l >> 24);
      *c++ = ZEICHEN1(zufallszahl);
      *c++ = ZEICHEN2(zufallszahl);
      *c++ = ZEICHEN1(l >> 16);
      *c++ = ZEICHEN2(l >> 16);
      *c++ = ZEICHEN1(l >> 8);
      *c++ = ZEICHEN2(l >> 8);
      *c++ = ZEICHEN1(l);
      *c++ = ZEICHEN2(l);
   } // for (i)

   printf("Plain text password: \"%s\" (without quotes)\n   Crypted password: %s\n", passwort, verschluesseltes);

/**
   {
      #include <proto/exec.h>

      UBYTE *decrypt_password(UBYTE *s);
      UBYTE *dp;

      dp = decrypt_password(verschluesseltes);
      if (dp)
      {
         printf("Decrypted: \"%s\" (without quotes)\n", dp);
         FreeVec(dp);      // FreeVec() ist korrekt, siehe verify.h
      } // if
   } // sub block
**/

   return 0;
} // main()

