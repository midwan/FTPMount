
#include "FTPMount.h" /* 03-03-08 rri */

b8 *decrypt_password(b8 *s)
{
	b8 *password, *pz;
	long l, zufallszahl;
#define DECODEBYTE(z1, z2)  ( (((z1) - 42) / 4 << 4) | ((z2) - 47) / 3 )

	// Länge muß Vielfaches von 10 sein
	if (strlen((char*)s) % 10 != 0) return nil;
	if ((password = (b8 *)allocate(strlen((char*)s) / 10 * 3, V_cstr)) == nil) return nil;

	pz = password;

	while (*s)
	{
		zufallszahl = DECODEBYTE(s[2], s[3]);
		l = (DECODEBYTE(s[0], s[1]) << 24) |
			(DECODEBYTE(s[4], s[5]) << 16) |
			(DECODEBYTE(s[6], s[7]) << 8) |
			DECODEBYTE(s[8], s[9]);
		l = (l ^ (zufallszahl * zufallszahl * zufallszahl * zufallszahl)) / zufallszahl;
		*pz++ = (l >> 8) & 0xff;
		*pz++ = (l >> 16) & 0xff;
		*pz++ = l & 0xff;
		s += 10;
	} // while
	return password;
}
