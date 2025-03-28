/*
 * charset.c - Character set conversions.
 *
 * Written by
 *  Jouko Valta <jopi@stekt.oulu.fi>
 *  Andreas Boose <viceteam@t-online.de>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "charset.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "util.h"

/*
    test for line ending, return number of bytes to skip if found.

    FIXME: although covering probably the vast majority of common
           cases, this function does not yet work for the general
           case (exotic platforms, unicode text).
*/
static int test_lineend(uint8_t *s)
{
    if ((s[0] == '\r') && (s[1] == '\n')) {
        /* CRLF (Windows, DOS) */
        return 2;
    } else if (s[0] == '\n') {
        /* LF (*nix) */
        return 1;
    } else if (s[0] == '\r') {
        /* CR (MacOS9) */
        return 1;
    }
    return 0;
}

uint8_t *charset_petconvstring(uint8_t *c, int mode)
{
    uint8_t *s = c, *d = c;
    int ch;

    switch (mode) {
        case CONVERT_TO_PETSCII: /* To petscii.  */
            while (*s) {
                if ((ch = test_lineend(s))) {
                    *d++ = 0x0d; /* petscii CR */
                    s += ch;
                } else {
                    *d++ = charset_p_topetscii(*s);
                    s++;
                }
            }
            break;

        case CONVERT_TO_ASCII: /* To ascii. */
            while (*s) {
                *d++ = charset_p_toascii(*s, CONVERT_WITHOUT_CTRLCODES);
                s++;
            }
            break;

        case CONVERT_TO_ASCII_WITH_CTRLCODES: /* To ascii, convert also screencodes. */
            while (*s) {
                *d++ = charset_p_toascii(*s, CONVERT_WITH_CTRLCODES);
                s++;
            }
            break;
        case CONVERT_TO_UTF8:
        default:
            log_error(LOG_DEFAULT, "Unkown conversion rule.");
    }

    *d = 0;

    return c;
}

/*
   replace the CHROUT duplicates by the proper petcii codes

   FIXME: this one doesn't work correct yet for a bunch of codes. luckily
          these are all codes that can not be converted between ascci and
          petscii anyway, so that isn't a real problem.
*/
static uint8_t petcii_fix_dupes(uint8_t c)
{
    if ((c >= 0x60) && (c <= 0x7f)) {
        return ((c - 0x60) + 0xc0);
    } else if (c >= 0xe0) {
        return ((c - 0xe0) + 0xa0);
    }
    return c;
}

/*
    when mapping to ascii, unmapable characters are NOT mapped to '?',
    but '.' instead, because:
    - filenames will be eventually translated by this function and then
      used on the host filesystem. adding wildcards into those is probably
      not a good idea at this point.
*/

#define ASCII_UNMAPPED  '.'

uint8_t charset_p_toascii(uint8_t c, int mode)
{
    if (mode) {
        /* convert ctrl chars to "screencodes" (used by monitor) */
        if (c <= 0x1f) {
            c += 0x40;
        }
    }

    c = petcii_fix_dupes(c);

    /* map petscii to ascii */
    if (c == 0x0d) {  /* petscii "return" */
        return '\n';
    } else if (c == 0x0a) {
        return '\r';
    } else if (c <= 0x1f) {
        /* unhandled ctrl codes */
        return ASCII_UNMAPPED;
    } else if (c == 0xa0) { /* petscii Shifted Space */
        return ' ';
    } else if ((c >= 0xc1) && (c <= 0xda)) {
        /* uppercase (petscii 0xc1 -) */
        return (uint8_t)((c - 0xc1) + 'A');
    } else if ((c >= 0x41) && (c <= 0x5a)) {
        /* lowercase (petscii 0x41 -) */
        return (uint8_t)((c - 0x41) + 'a');
    }

    return ((isprint((unsigned char)c) ? c : ASCII_UNMAPPED));
}

/*
    when mapping ascii to petscii, mapping unmapable to '.' breaks
    loading files with certain names, in particulare foobar~1.prg style
    names. mapping them to '?' instead will allow this (and other)
    stuff to work.
*/

/* #define PETSCII_UNMAPPED 0x2e */     /* petscii "." */
#define PETSCII_UNMAPPED 0x3f     /* petscii "?" */

uint8_t charset_p_topetscii(uint8_t c)
{
    /* map ascii to petscii */
    if (c == '\n') {
        return 0x0d; /* petscii "return" */
    } else if (c == '\r') {
        return 0x0a;
    } else if (c <= 0x1f) {
        /* unhandled ctrl codes */
        return PETSCII_UNMAPPED;
    } else if (c == '`') {
        return 0x27; /* petscii "'" */
    } else if ((c >= 'a') && (c <= 'z')) {
        /* lowercase (petscii 0x41 -) */
        return (uint8_t)((c - 'a') + 0x41);
    } else if ((c >= 'A') && (c <= 'Z')) {
        /* uppercase (petscii 0xc1 -)
           (don't use duplicate codes 0x61 - ) */
        return (uint8_t)((c - 'A') + 0xc1);
    } else if (c >= 0x7b) {
        /* last not least, ascii codes >= 0x7b can not be
           represented properly in petscii */
        return PETSCII_UNMAPPED;
    }

    return petcii_fix_dupes(c);
}

uint8_t charset_screencode_to_petscii(uint8_t code)
{
    code &= 0x7f; /* mask inverse bit */
    if (code <= 0x1f) {
        return (uint8_t)(code + 0x40);
    } else if (code >= 0x40 && code <= 0x5f) {
        return (uint8_t)(code + 0x20);
    }
    return code;
}

uint8_t charset_petscii_to_screencode(uint8_t code, unsigned int reverse_mode)
{
    uint8_t rev = (reverse_mode ? 0x80 : 0x00);

    if (code >= 0x40 && code <= 0x5f) {
        return (uint8_t)(code - 0x40) | rev;
    } else if (code >= 0x60 && code <= 0x7f) {
        return (uint8_t)(code - 0x20) | rev;
    } else if (code >= 0xa0 && code <= 0xbf) {
        return (uint8_t)(code - 0x40) | rev;
    } else if (code >= 0xc0 && code <= 0xfe) {
        return (uint8_t)(code - 0x80) | rev;
    } else if (code == 0xff) {
        return 0x5e | rev;
    }
    return code | rev;
}

void charset_petscii_to_screencode_line(const uint8_t *line, uint8_t **buf,
                                       unsigned int *len)
{
    size_t linelen, i;

    linelen = strlen((const char *)line);
    *buf = lib_malloc(linelen);

    for (i = 0; i < linelen; i++) {
        (*buf)[i] = charset_petscii_to_screencode(line[i], 0);
    }
    *len = (unsigned int)linelen;
}

int charset_petscii_to_ucs(uint8_t c)
{
    switch (c) {
        case 0x5c:
            return 0xa3; /* Pound sign */
        case 0x5e: /* PETSCII Up arrow */
            return 0x2191;
        case 0x5f: /* PETSCII Left arrow */
            return 0x2190;

        case 0xa0: /* PETSCII Shifted Space */
        case 0xe0:
            return 0xa0;

        case 0xc0:
            return 0x2500;

        case 0xde: /* PETSCII Pi */
        case 0xff:
            return 0x3c0;

        default:
            return (int)charset_p_toascii(c, CONVERT_WITHOUT_CTRLCODES);
    }
}


/* FIXME: `len` should be size_t */
int charset_ucs_to_utf8(uint8_t *out, int code, size_t len)
{
    if (code >= 0x00 && code <= 0x7f) {
        if (len >= 1) {
            *out = (uint8_t)(code);
        }
        return 1;
    } else if (code >= 0x80 && code <= 0x7ff) {
        if (len >= 2) {
            *(out) = 0xc0 | (uint8_t)(code >> 6);
            *(out + 1) = 0x80 | (uint8_t)(code & 0x3f);
        }
        return 2;
    } else if (code >= 0x800 && code <= 0xffff) {
        if (len >= 3) {
            *(out) = 0xe0 | (uint8_t)(code >> 12);
            *(out + 1) = 0x80 | (uint8_t)((code >> 6) & 0x3f);
            *(out + 2) = 0x80 | (uint8_t)(code & 0x3f);
        }
        return 3;
    } else if (code >= 0x10000 && code <= 0x10ffff) {
        if (len >= 4) {
            *(out) = 0xe0 | (uint8_t)(code >> 18);
            *(out + 1) = 0x80 | (uint8_t)((code >> 12) & 0x3f);
            *(out + 2) = 0x80 | (uint8_t)((code >> 6) & 0x3f);
            *(out + 3) = 0x80 | (uint8_t)(code & 0x3f);
        }
        return 4;
    }
    log_error(LOG_DEFAULT, "Out-of-range code point U+%04x.", (unsigned int)code);
    return 0;
}

/* Convert a string from ASCII to PETSCII, or from PETSCII to ASCII/UTF-8 and
   return it in a malloc'd buffer. */
uint8_t *charset_petconv_stralloc(uint8_t *in, int mode)
{
    uint8_t *s = in;
    uint8_t *d;
    uint8_t *buf;
    int ch;
    size_t len;

    len = strlen((const char *)in);
    buf = lib_malloc(len + 1);
    d = buf;

    switch (mode) {
        case CONVERT_TO_PETSCII: /* UTF-8 not implemented. */
            while (*s) {
                if ((ch = test_lineend(s))) {
                    *d++ = 0x0d; /* PETSCII CR */
                    s += ch;
                } else {
                    *d++ = charset_p_topetscii(*s);
                    s++;
                }
            }
            break;

        case CONVERT_TO_ASCII:
            while (*s) {
                *d++ = charset_p_toascii(*s, CONVERT_WITHOUT_CTRLCODES);
                s++;
            }
            break;

        case CONVERT_TO_UTF8:
            while (1) {
                while (*s) {
                    int code = charset_petscii_to_ucs(*s);
                    d += charset_ucs_to_utf8(d, code, len - (d - buf));
                    s++;
                }
                if (d - buf > len) {
                    /* UTF-8 form is longer than the PETSCII form. */
                    len = d - buf;
                    buf = lib_realloc(buf, len + 1);
                    d = buf;
                    s = in;
                } else {
                    break;
                }
            }
            break;
        case CONVERT_TO_ASCII_WITH_CTRLCODES:
        default:
            log_error(LOG_DEFAULT, "Unkown conversion rule.");
    }

    *d = 0;

    return buf;
}

/* These are a helper function for the `-autostart' command-line option.  It
   replaces all the $[0-9A-Z][0-9A-Z] patterns in `string' and returns it.  */
char * charset_hexstring_to_byte(char *source, char *destination)
{
    char * next = source + 1;
    char c;
    uint8_t value = 0;
    int digit = 0;

    while (*next && digit++ < 2) {
        value <<= 4;

        c = util_toupper( *next++ );

        if (c >= 'A' && c <= 'F') {
            value += c - 'A';
        } else if (isdigit((unsigned char)c)) {
            value += c - '0';
        } else {
            break;
        }
    }

    if (digit < 2) {
        value = *source;
        next = source + 1;
    }

    *destination = value;

    return next;
}

char *charset_replace_hexcodes(char *source)
{
    char * destination = lib_strdup(source ? source : "");

    if (destination) {
        char * pread = destination;
        char * pwrite = destination;

        while (*pread != 0) {
            if (*pread == '$') {
                pread = charset_hexstring_to_byte( pread, pwrite++ );
            } else {
                *pwrite++ = *pread++;
            }
        }
        *pwrite = 0;
    }

    return destination;
}
