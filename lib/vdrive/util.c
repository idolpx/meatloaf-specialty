/*! \file util.c \n
 *  \author Ettore Perazzoli, Andreas Boose\n
 *  \brief  Miscellaneous utility functions.
 *
 * util.c - Miscellaneous utility functions.
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andreas Boose <viceteam@t-online.de>
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
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

#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif
#define _FILE_OFFSET_BITS 64

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "archdep.h"
#include "lib.h"
#include "log.h"

#include "util.h"


/* #define DBGUTIL */

#ifdef DBGUTIL
#define DBG(x)  printf x
#else
#define DBG(x)
#endif

/* Malloc a new string whose contents concatenate the arguments until the
   first NULL pointer (max `_CONCAT_MAX_ARGS' arguments).  */
char *util_concat(const char *s, ...)
{
#define _CONCAT_MAX_ARGS 128
    const char *arg;
    char *newp, *ptr;
    int num_args;
    size_t arg_len[_CONCAT_MAX_ARGS], tot_len;
    int i;
    va_list ap;

    arg_len[0] = tot_len = strlen(s);

    va_start(ap, s);
    for (i = 1;
         i < _CONCAT_MAX_ARGS && (arg = va_arg(ap, const char *)) != NULL;
         i++) {
        arg_len[i] = strlen(arg);
        tot_len += arg_len[i];
    }
    num_args = i;
    va_end(ap);

    newp = lib_malloc(tot_len + 1);

    if (arg_len[0] > 0) {
        memcpy(newp, s, arg_len[0]);
    }
    ptr = newp + arg_len[0];

    va_start(ap, s);
    for (i = 1; i < num_args; i++) {
        memcpy(ptr, va_arg(ap, const char *), arg_len[i]);
        ptr += arg_len[i];
    }
    *ptr = '\0';
    va_end(ap);

    DBG(("util_concat %p - %s\n", newp, newp));
    return newp;
}


/** \brief  Join the strings in \a list with \a sep as separator
 *
 * \param[in]   list    list of strings, `NULL`-terminated
 * \param[in]   sep     separator string (optional)
 *
 * Example:
 * \code{.c}
 *
 *  const char **list[] = { "foo", "bar", "meloen", NULL };
 *  char *s = util_strjoin(list, ";");
 *  // returns: foo;bar;meloen
 * \endcode
 *
 * \return  heap-allocated string, deallocate with lib_free()
 */
char *util_strjoin(const char **list, const char *sep)
{
    char *result;
    char *p;
    size_t i;
    size_t list_size;
    size_t total_len = 0;
    size_t sep_len;

    for (i = 0; list[i] != NULL; i++) {
        total_len += strlen(list[i]);
        /* NOP */
    }
    list_size = i;
    /* total_len is now all strings in list added together without sep */

    if (list_size == 0) {
        /* no list */
        return NULL;
    } else if (list_size == 1) {
        /* one item, just copy */
        return lib_strdup(*list);
    }

    if (sep != NULL && *sep != '\0') {
        sep_len = strlen(sep);
    } else {
        sep_len = 0;
    }

    total_len += ((list_size - 1) * sep_len) + 1;
    result = lib_malloc(total_len);

    p = result;
    for (i = 0; i < list_size; i++) {
        size_t ilen;
        if (i > 0 && (sep_len > 0)) {
            /* add sepatator */
            memcpy(p, sep, sep_len);
            p += sep_len;
        }
        ilen = strlen(list[i]);
        memcpy(p, list[i], ilen);
        p += ilen;
    }
    *p = '\0';
    return result;
}


/* Add a line to a string.  */
void util_addline(char **list, const char *line)
{
    char *tmp;

    tmp = util_concat(*list, line, NULL);
    lib_free(*list);
    *list = tmp;
}

/* Add a line to a string and free the line.  */
void util_addline_free(char **list, char *line)
{
    util_addline(list, line);
    lib_free(line);
}

/* Add the first `src_size' bytes of `src' to the end of `buf', which is a
   malloc'ed block of `max_buf_size' bytes of which only the first `buf_size'
   ones are used.  If the `buf' is not large enough, realloc it.  Return a
   pointer to the new block.  */
uint8_t *util_bufcat(uint8_t *buf, int *buf_size, size_t *max_buf_size,
                     const uint8_t *src, int src_size)
{
#define BUFCAT_GRANULARITY 0x1000
    if (*buf_size + src_size > (int)(*max_buf_size)) {
        uint8_t *new_buf;

        *max_buf_size = (((*buf_size + src_size) / BUFCAT_GRANULARITY + 1)
                         * BUFCAT_GRANULARITY);
        new_buf = lib_realloc(buf, *max_buf_size);
        buf = new_buf;
    }

    memcpy(buf + *buf_size, src, src_size);
    *buf_size += src_size;
    return buf;
}

/* Remove spaces from start and end of string `s'.  The string is not
   reallocated even if it becomes smaller.  */
void util_remove_spaces(char *s)
{
    char *p;
    size_t l = strlen(s);

    for (p = s; *p == ' '; p++) {
    }

    l -= (p - s);
    memmove(s, p, l + 1);

    if (l > 0) {
        for (p = s + l - 1; l > 0 && *p == ' '; l--, p--) {
        }
        *(p + 1) = '\0';
    }
}

/* Set a new value to the dynamically allocated string *str.
   Returns `-1' if nothing has to be done.  */
int util_string_set(char **str, const char *new_value)
{
    if (*str == NULL) {
        if (new_value != NULL) {
            *str = lib_strdup(new_value);
        }
    } else {
        if (new_value == NULL) {
            lib_free(*str);
            *str = NULL;
        } else {
            /* Skip copy if src and dest are already the same.  */
            if (strcmp(*str, new_value) == 0) {
                return -1;
            }

            *str = lib_realloc(*str, strlen(new_value) + 1);
            strcpy(*str, new_value);
        }
    }
    DBG(("util_string_set %p - %s\n", *str, *str));
    return 0;
}

int util_check_null_string(const char *string)
{
    if (string != NULL && *string != '\0') {
        return 0;
    }

    return -1;
}

int util_check_filename_access(const char *filename)
{
    ADFILE *file;

    file = archdep_fopen(filename, MODE_READ);
    if ( !archdep_fisopen(file)) {
        file = archdep_fopen(filename, MODE_WRITE);
        if( !archdep_fisopen(file) ) {
            return -1;
        } else {
            archdep_fclose(file);
            archdep_remove(filename);
            return 0;
        }
    } else {
        archdep_fclose(file);
        return 0;
    }
}

/* ------------------------------------------------------------------------- */

/* Replace every occurrence of `string' in `s' with `replacement' and return
   the result as a malloc'ed string.  */
char *util_subst(const char *s, const char *string, const char *replacement)
{
    int num_occurrences;
    int total_size;
    size_t s_len = strlen(s);
    size_t string_len = strlen(string);
    size_t replacement_len = strlen(replacement);
    const char *sp;
    char *dp;
    char *result;

    /* First, count the occurrences so that we avoid re-allocating every
       time.  */
    for (num_occurrences = 0, sp = s;
         (sp = strstr(sp, string)) != NULL;
         num_occurrences++, sp += string_len) {
    }

    total_size = (int)(s_len - (string_len - replacement_len) * num_occurrences + 1);

    result = lib_malloc(total_size);

    sp = s;
    dp = result;
    do {
        char *f = strstr(sp, string);

        if (f == NULL) {
            break;
        }

        memcpy(dp, sp, f - sp);
        memcpy(dp + (f - sp), replacement, replacement_len);
        dp += (f - sp) + replacement_len;
        s_len -= (f - sp) + string_len;
        sp = f + string_len;
        num_occurrences--;
    } while (num_occurrences != 0);

    memcpy(dp, sp, s_len + 1);

    return result;
}

/* ------------------------------------------------------------------------- */


/* Split `path' into a file name and a directory component.  Unlike
   the MS-DOS `fnsplit', the directory does not have a trailing '/'.  */
void util_fname_split(const char *path, char **directory_return,
                      char **name_return)
{
    const char *p;
#if 0
    printf("%s:%d:%s(): got '%s'\n", __FILE__, __LINE__, __func__, path);
#endif
    /* if no input, return "."/"" */
    if (path == NULL) {
        if (directory_return != NULL) {
            *directory_return = lib_strdup(".");
        }
        if (name_return != NULL) {
            *name_return = lib_strdup("");
        }
#if 0
        printf("%s:%d:%s(): dir = '%s', name = '%s' (didn't find DIRSEP)\n",
                __FILE__, __LINE__, __func__,
                directory_return != NULL ? *directory_return : "NULL",
                name_return != NULL ? *name_return : "NULL");
#endif
        return;
    }

    /* get ptr to last dir seperator before the filename */
    p = strrchr(path, ARCHDEP_DIR_SEP_CHR);

#if (ARCHDEP_DIR_SEP_CHR == '\\')
# if 0
    printf("WE HAVE \\ AS A DIR SEPARATOR!\n");
# endif
    /* Both `/' and `\' are valid.  */
    {
        const char *p1;

        p1 = strrchr(path, '/');
        if (p == NULL || p < p1) {
            p = p1;
        }
    }
#endif

    /* if no path in the input, return "." as path */
    if (p == NULL) {
        if (directory_return != NULL) {
            *directory_return = lib_strdup(".");
        }
        if (name_return != NULL) {
            *name_return = lib_strdup(path);
        }
#if 0
        printf("%s:%d:%s(): dir = '%s', name = '%s' (didn't find DIRSEP)\n",
                __FILE__, __LINE__, __func__,
                directory_return != NULL ? *directory_return : "NULL",
                name_return != NULL ? *name_return : "NULL");
#endif
        return;
    }

    if (directory_return != NULL) {
        *directory_return = lib_malloc((size_t)(p - path + 1));
        memcpy(*directory_return, path, p - path);
        (*directory_return)[p - path] = '\0';
    }

    if (name_return != NULL) {
        *name_return = lib_strdup(p + 1);
    }
#if 0
    printf("%s:%d:%s(): dir = '%s', name = '%s' (didn't find DIRSEP)\n",
            __FILE__, __LINE__, __func__,
            directory_return != NULL ? *directory_return : "NULL",
            name_return != NULL ? *name_return : "NULL");
#endif
    return;
}

/* ------------------------------------------------------------------------- */

/*! \brief Read bytes from a position in a file

 \param fd
   file descriptor as obtained by fopen().

 \param buf
   pointer where to from the file

 \param num
   number of bytes to read.

 \param offset
   the offset from start of file

 \return
   0 on success, else -1.

*/

int util_fpread(ADFILE *fd, void *buf, size_t num, long offset)
{

    if (archdep_fseek(fd, offset, SEEK_SET) < 0 || archdep_fread(buf, num, 1, fd) < 1) {
        return -1;
    }

    return 0;
}

/*! \brief Write bytes to a position in a file

 \param fd
   file descriptor as obtained by fopen().

 \param buf
   pointer to the data to be written to the file

 \param num
   number of bytes to write.

 \param offset
   the offset from start of file

 \return
   0 on success, else -1.

*/
int util_fpwrite(ADFILE *fd, const void *buf, size_t num, long offset)
{
    if (archdep_fseek(fd, offset, SEEK_SET) < 0 || archdep_fwrite(buf, num, 1, fd) < 1) {
        return -1;
    }
    return 0;
}

void util_dword_to_be_buf(uint8_t *buf, uint32_t data)
{
    buf[3] = (uint8_t)(data & 0xff);
    buf[2] = (uint8_t)((data >> 8) & 0xff);
    buf[1] = (uint8_t)((data >> 16) & 0xff);
    buf[0] = (uint8_t)((data >> 24) & 0xff);
}

void util_dword_to_le_buf(uint8_t *buf, uint32_t data)
{
    buf[0] = (uint8_t)(data & 0xff);
    buf[1] = (uint8_t)((data >> 8) & 0xff);
    buf[2] = (uint8_t)((data >> 16) & 0xff);
    buf[3] = (uint8_t)((data >> 24) & 0xff);
}

uint32_t util_le_buf_to_dword(uint8_t *buf)
{
    uint32_t data;

    data = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

    return data;
}

uint32_t util_be_buf_to_dword(uint8_t *buf)
{
    uint32_t data;

    data = buf[3] | (buf[2] << 8) | (buf[1] << 16) | (buf[0] << 24);

    return data;
}

void util_int_to_be_buf4(uint8_t *buf, int data)
{
    util_dword_to_be_buf(buf, (uint32_t)data);
}

void util_int_to_le_buf4(uint8_t *buf, int data)
{
    util_dword_to_le_buf(buf, (uint32_t)data);
}

int util_le_buf4_to_int(uint8_t *buf)
{
    return (int)util_le_buf_to_dword(buf);
}

int util_be_buf4_to_int(uint8_t *buf)
{
    return (int)util_be_buf_to_dword(buf);
}

void util_word_to_be_buf(uint8_t *buf, uint16_t data)
{
    buf[1] = (uint8_t)(data & 0xff);
    buf[0] = (uint8_t)((data >> 8) & 0xff);
}

void util_word_to_le_buf(uint8_t *buf, uint16_t data)
{
    buf[0] = (uint8_t)(data & 0xff);
    buf[1] = (uint8_t)((data >> 8) & 0xff);
}

uint16_t util_le_buf_to_word(uint8_t *buf)
{
    uint16_t data;

    data = buf[0] | (buf[1] << 8);

    return data;
}

uint16_t util_be_buf_to_word(uint8_t *buf)
{
    uint16_t data;

    data = buf[1] | (buf[0] << 8);

    return data;
}

/* ------------------------------------------------------------------------- */

/* Check for existance of file named `name'.  */
int util_file_exists(const char *name)
{
    ADFILE *f;

    f = archdep_fopen(name, MODE_READ);
    if ( archdep_fisopen(f) ) {
        archdep_fclose(f);
        return 1;
    } else {
        return 0;
    }
}

/* ------------------------------------------------------------------------- */

char *util_find_next_line(const char *pos)
{
    char *p = strchr(pos, '\n');

    return (char *)(p == NULL ? pos : p + 1);
}

char *util_find_prev_line(const char *text, const char *pos)
{
    const char *p;

    if (pos - text <= 2) {
        return (char *) text;
    }

    for (p = pos - 2; p != text; p--) {
        if (*p == '\n') {
            break;
        }
    }

    if (*p == '\n') {
        p++;
    }

    return (char *)p;
}

/* ------------------------------------------------------------------------- */

/** \brief  Add \a extension to a\ name if missing
 *
 * \param[in,out]   name        input and final 'string'
 * \param[in]       extension   extension to add to \a name
 *
 * \warning Since adding \a extension can trigger a realloc(3) of \a *name,
 *          make sure to handle the output pointer properly.
 *
 */
void util_add_extension(char **name, const char *extension)
{
    size_t name_len;
    size_t ext_len;

    if (extension == NULL || *name == NULL) {
        return;
    }

    ext_len = strlen(extension);

    if (ext_len == 0) {
        return;
    }

    name_len = strlen(*name);
    if ((name_len > ext_len + 1)
        && (util_strcasecmp(&((*name)[name_len - ext_len]), extension) == 0)) {
        return;
    }

    *name = lib_realloc(*name, name_len + ext_len + 2);
    (*name)[name_len] = '.';
    memcpy(&((*name)[name_len + 1]), extension, ext_len + 1);
}

/* Like util_add_extension() but a const filename is passed.  */
char *util_add_extension_const(const char *filename, const char *extension)
{
    char *ext_filename;

    ext_filename = lib_strdup(filename);

    util_add_extension(&ext_filename, extension);

    return ext_filename;
}

/* like util_add_extension(), but using a var[MAXPATH] type string
   without using realloc if extension is not present. */
void util_add_extension_maxpath(char *name, const char *extension, unsigned int maxpath)
{
    size_t name_len, ext_len;

    if (extension == NULL || name == NULL) {
        return;
    }

    name_len = strlen(name);
    ext_len = strlen(extension);

    if (ext_len == 0) {
        return;
    }

    if (name_len + ext_len > maxpath) {
        return;
    }

    if ((name_len > ext_len + 1)
        && (util_strcasecmp(&((name)[name_len - ext_len]), extension) == 0)) {
        return;
    }

    name[name_len] = '.';
    memcpy(name + name_len + 1, extension, ext_len + 1);
}

char *util_get_extension(const char *filename)
{
    char *s;

    if (filename == NULL) {
        return NULL;
    }

    s = strrchr(filename, '.');
    if (s) {
        return s + 1;
    } else {
        return NULL;
    }
}

/* char to char tolower function, still uses tolower,
   but it keeps the ugly casting to avoid warnings
   out of the main sources. */
char util_tolower(char c)
{
    return (char)tolower((unsigned char)c);
}

/* char to char toupper function, still uses toupper,
   but it keeps the ugly casting to avoid warnings
   out of the main sources. */
char util_toupper(char c)
{
    return (char)toupper((unsigned char)c);
}

/** \brief  Skip leading whitespace in string
 *
 * \param[in]   s   string
 *
 * \return  pointer to first non-whitespace character or terminating nul
 */
const char *util_skip_whitespace(const char *s)
{
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

/** \brief  Skip trailing whitespace in string
 *
 * \param[in]   s   string
 *
 * \return  pointer to first non-whitespace character or terminating nul,
 *          starting from the end of the string
 */
const char *util_skip_whitespace_trailing(const char *s)
{
    const char *p;

    if (*s == '\0') {
        /* empty string */
        return s;
    }

    /* last character in the string */
    p = s + strlen(s) - 1;

    while (*p != '\0' && isspace((unsigned char)*p)) {
        p--;
    }
    if (p < s) {
        /* entire string contained whitespace */
        return s;
    } else {
        return p;
    }
}

/* generate a list in the form "%X/%X/.../%X" */
char *util_gen_hex_address_list(int start, int stop, int step)
{
    char *temp1, *temp2;
    char *temp3 = NULL;
    int i = start;

    temp1 = lib_strdup("");
    while (i < stop) {
        temp2 = lib_msprintf("0x%X", (unsigned int)i);
        temp3 = util_concat(temp1, temp2, NULL);
        lib_free(temp1);
        lib_free(temp2);
        temp1 = temp3;
        if (i + step < stop) {
            temp3 = util_concat(temp1, "/", NULL);
            lib_free(temp1);
            temp1 = temp3;
        }
        i += step;
    }
    return temp3;
}


/** \brief  Join multiple paths into a single path
 *
 * Joins a list of strings into a path for use with the current arch
 *
 * \param   [in]    path    list of paths to join, NULL-terminated
 *
 * \return  heap-allocated string, free with lib_free()
 */
char *util_join_paths(const char *path, ...)
{
    const char *arg;
    char *result;
    char *endptr;
    size_t result_len;
    size_t len;
    va_list ap;
#if 0
    printf("%s: first argument: '%s'\n", __func__, path);
#endif
    /* silly way to use a varags function, but lets catch it anyway */
    if (path == NULL) {
        return NULL;
    }

    /* determine size of result string */
    va_start(ap, path);
    result_len = strlen(path);
    while ((arg = va_arg(ap, const char *)) != NULL) {
        result_len += (strlen(arg) + 1);
    }
    va_end(ap);
#if 0
    /* cannot use %zu here due to MS' garbage C lib */
    printf("%s: result length: %"PRI_SIZE_T"\n", __func__, result_len);
#endif
    /* initialize result string */
    result = lib_calloc(result_len + 1, 1);
    strcpy(result, path);
    endptr = result + (ptrdiff_t)strlen(path);

    /* now concatenate arguments into a pathname */
    va_start(ap, path);
    while ((arg = va_arg(ap, const char *)) != NULL) {
#if 0
        printf("%s: adding '%s' to the result.", __func__, arg);
#endif
        len = strlen(arg);
        if (*arg != ARCHDEP_DIR_SEP_CHR) {
            *endptr++ = ARCHDEP_DIR_SEP_CHR;
        }
        memcpy(endptr, arg, len + 1);
        endptr += (ptrdiff_t)len;
    }

    va_end(ap);
    return result;
}


/** \brief  Compare strings, ignoring case
 *
 * \param[in]   s1  string to compare
 * \param[in]   s2  string to compare
 *
 * \return  -1 if \a s1 \< \a s2, 0 if \a s1 == \a s2, 1 if \a s1 \> \a s2
 *
 * \note    Does naive case-folding, so don't rely on this for sorting strings
 *          in non-ASCII encoding (for example when e-umlaut exists but E-umlaut
 *          doesn't exist)
 */
int util_strcasecmp(const char *s1, const char *s2)
{
    while (*s1 != '\0' && *s2 != '\0') {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);

        if (c1 < c2) {
            return -1;
        } else if (c1 > c2) {
            return 1;
        }

        s1++;
        s2++;
    }

    if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else {
        return 1;
    }
}


/** \brief  Compare strings, ignoring case, comparing at most \a n chars
 *
 * \param[in]   s1  string to compare
 * \param[in]   s2  string to compare
 *
 * \return  -1 if \a s1 \< \a s2, 0 if \a s1 == \a s2, 1 if \a s1 \> \a s2
 *
 * \note    Does naive case-folding, so don't rely on this for sorting strings
 *          in non-ASCII encoding (for example when e-umlaut exists but E-umlaut
 *          doesn't exist)
 */
int util_strncasecmp(const char *s1, const char *s2, size_t n)
{
    while (*s1 != '\0' && *s2 != '\0' && n > 0) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);

        if (c1 < c2) {
            return -1;
        } else if (c1 > c2) {
            return 1;
        }

        s1++;
        s2++;
        n--;
    }

    if ((n == 0) || (*s1 != '\0' && *s2 != '\0') || (*s1 == '\0' && *s2 == '\0')) {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else {
        return 1;
    }
}


/** \brief  Split string into substrings
 *
 * Split \a string into substrings on \a delimiter, returning at most
 * \a max_tokens substrings, plus the remainder as a final substring if there
 * is anything left in \a string. The \a delimiter is not among the substrings,
 * except in the remainder if there is a remainder.
 *
 * If \a string is `NULL` or "" `NULL is returned.
 * If \a delimiter is `NULL` or "" an array of a single element is returned,
 * with the single element being a copy of the full \a string.
 * If \a max_tokens is reached the remaining characters in \a string are
 * returned as the final array element.
 * Consecutive instances of \a delimiter are treated as a single delimiter,
 * there are no empty tokens returned.
 *
 * \param[in]   string      string to split on \a delimiter
 * \param[in]   delimiter   string delimiter
 * \param[in]   max_tokens  maximum number of substrings to generate
 *
 * \return  array of substrings, `NULL`-terminated
 *
 * \note    free both the array and its elements with lib_free() after use
 */
char **util_strsplit(const char *string, const char *delimiter, int max_tokens)
{
    size_t       delimlen;
    size_t       ressize;
    size_t       resindex;
    char       **result;
    const char  *curpos;

    if (string == NULL || *string == '\0') {
        return NULL;
    }
    if (delimiter == NULL || *delimiter == '\0') {
        result = lib_malloc(sizeof *result * 2);
        result[0] = lib_strdup(string);
        result[1] = NULL;
        return result;
    }
    delimlen = strlen(delimiter);

    if (max_tokens > 1) {
        ressize = (size_t)max_tokens + 2; /* +1 for remainder, +1 for NULL */
    } else {
        ressize = 16;
    }
    result = lib_malloc(sizeof *result * ressize);

    curpos   = string;
    resindex = 0;
    ressize  = 0;
    while (*curpos != '\0' && ((max_tokens < 1) || (size_t)resindex < max_tokens)) {
        const char *delimpos;
        char       *token;
        size_t      tokenlen;

        /* -1 for remainder, -1 for terminating NULL */
        if (resindex == ressize - 2) {
            ressize *= 2;
            result = realloc(result, sizeof *result * ressize);
        }

        delimpos = strstr(curpos, delimiter);
        if (delimpos == NULL) {
            /* no more data, append remainder */
            break;
        }

        if (delimpos == curpos) {
            /* delimiter at start of string or right after previous delimiter,
             * skip */
            curpos += delimlen;
        } else {
            /* append token */
            tokenlen = (size_t)(delimpos - curpos);
            token = lib_malloc(tokenlen + 1);
            memcpy(token, curpos, tokenlen);
            token[tokenlen] = '\0';
            result[resindex++] = token;
            curpos = delimpos + delimlen;
        }
    }

    if (*curpos != '\0') {
        /* add remainder of string as the final element */
        result[resindex++] = lib_strdup(curpos);
    }
    /* terminate list */
    result[resindex] = NULL;
    return result;
}
