/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */

/* This file is modified from the original for the purposes of the
limbo project. The original sources can be found at
https://datatracker.ietf.org/doc/html/rfc1321 */

#include <stdint.h>

/* MD5 context. */
typedef struct {
  uint32_t state[4];                                /* state (ABCD) */
  uint32_t count[2];     /* number of bits, modulo 2^64 (lsb first) */
  uint8_t buffer[64];                               /* input buffer */
} MD5_CTX;

void MD5Init(MD5_CTX *);
void MD5Update(MD5_CTX *, const unsigned char *, unsigned int);
void MD5Final(unsigned char [16], MD5_CTX *);
