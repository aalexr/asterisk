/* misc.c
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/* altalanos fuggvenyek */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>  // inet_pton() függvény
#include <stdarg.h>

#include "debug.h"

char *chomp (char *s) {
    if (s == NULL)
        return NULL;
    size_t len = strlen(s);

    int n;
    for (n = len - 1; n >= 0; n--) {
        if (s[n] == '\n' || s[n] == '\r')
            s[n] = '\0';
        else
            return s;
    }
    return s;
}

/*
fmtsub - mintából, változó-érték párokból és készít stringet behelyettesítéssel

paraméterek:
    dest        ebbe a stringbe menti el az eredményt
    size        dest mérete
    pattern     minta
    args        változó-érték táblázat, nullával lezárva

visszatérési érték:
    0           sikerült a behelyettesítés
    -1          valamelyik mutató == NULL vagy a size == 0

A táblázat megadása:

const char *args[][2] = {
    {"%n", "2463434"},
    {"%v", "0.9.6-rc2"},
    {"version", "Linux 2.6.32"},
    {"boo", "baa"},
    {0}}; */
int fmtsub (char *dest, size_t size, const char *pattern, const char *args[][2]) {
    char buf[4096];
//~ debs(dest); debi(size); debs(pattern); debi(args);
    if (dest == NULL || pattern == NULL || args == NULL || size == 0)
        return -1;

    // ha a pattern üres string, akkor üres stringet adunk vissza
    if (strlen(pattern) == 0) {
        strcpy(dest, "");
        return 0;
    }

    int pospat, posbuf, a;
    int lenpat = strlen(pattern);
    buf[0] = '\0';
    // byteonként végigmegyünk a pattern stringen
    for (pospat = 0, posbuf = 0; pospat < lenpat && posbuf < sizeof(buf) - 1;) {
        const char *subpat = &pattern[pospat];
        int match = 0;

        // végigmegyünk az args táblázaton
        for (a = 0; args[a][0] != 0; a++) {
            const char *var = args[a][0]; // "%n"
            const char *exp = args[a][1]; // "2463434"

            // ha a változónév megtalálható a pattern jelenlegi pozíciójánál
            if (strlen(var) != 0 && !strncmp(subpat, var, strlen(var))) {
                if (exp  != NULL) {
                    // bemásoljuk a változó értékét a bufferbe
                    strncat(buf, exp, sizeof(buf) - posbuf - 1);
                    posbuf += strlen(exp);
                }

                /* ezen a ponton a posbuf nagyobb lehet, mint a buf, ami 
                igen kellemetlenül érintené a lenti buf[posbuf] = '\0' 
                értékadást, ezért a posbuf pozícionálót beállítjuk a buf 
                utolsó byte-jára. ide fog kerülni a NULL. */
                if (posbuf >= sizeof(buf) - 1)
                    posbuf = sizeof(buf) - 1;

                // pattern pozícióját a változónév utáni karakterre állítjuk
                pospat += strlen(var);

                // kihagyjuk a lenti if-et
                match = 1;
                break;
            }
        }

        // 1 bájt másolása pattern-ből buf-ba
        if (!match) {
            buf[posbuf] = pattern[pospat];
            buf[posbuf+1] = '\0';
            pospat++;
            posbuf++;
        }
    }

    buf[posbuf] = '\0'; // lezáró NULL
    strncpy(dest, buf, size - 1);
    return 0;
}

// http://en.wikipedia.org/wiki/ROT13
// paraméter az a string, amit módosítani kell
// visszatérés ugyan az, mint a paraméter kompatibilitási izé miatt
char *encode_rot13 (char *s) {
    if (s == NULL)
        return NULL;

    int i;
    for (i = 0; s[i]; i++) {
        if (s[i] >= 'a' && s[i] <= 'm') { s[i] += 13; continue; }
        if (s[i] >= 'A' && s[i] <= 'M') { s[i] += 13; continue; }
        if (s[i] >= 'n' && s[i] <= 'z') { s[i] -= 13; continue; }
        if (s[i] >= 'N' && s[i] <= 'Z') { s[i] -= 13; continue; }
    }
    return s;
}

// szóközöket és tabokat vág le a megadott string bal és/vagy jobb oldaláról
// trim  - mindkét oldalról vág
// ltrim - bal oldalról vág
// rtrim - jobb oldalról vág
char *_trim (char *s, int trim_from_left, int trim_from_right) {
    if (s == NULL)
        return NULL;

    int n, i;
    size_t len = strlen(s);

    if (trim_from_right) {
        for (n = len - 1; n >= 0; n--) {
            if (s[n] == 32 || s[n] == '\t') {
                s[n] = '\0';
                len--;
            } else {
                break;
            }
        }
    }

    if (trim_from_left) {
        for (n = 0; n < len; n++)
            if (s[n] != 32 && s[n] != '\t')
                break;

        if (n > 0) {
            for (i = n; i < len; i++)
                s[i-n] = s[i];

            for (i = len - n; i < len; i++)
                s[i] = '\0';
        }
    }
    return s;
}

/* megszámlálja a <delimeter> elemeket */
static int split_get_size (char *buffer, int delimeter) {
    int c = 0, i = 0;
    if (buffer[ 0 ] == 0)
        return 0;
    while (buffer[i] != 0) {
        if (buffer[i] == delimeter)
            c++;
        i++;
    }
    /* egyet hozzáadok, mert a "hello" is értéknek számít, hiába nincs benne ';' */
    return c + 1;
}

/* visszatér egy pointer tömbbel, ami az elemekre mutat, a tömböt 0 pointerrel zárja */
char **split (char *buffer, int delimeter) {
    int size = split_get_size(buffer, delimeter);
    /* helyfoglalás a pointer tömbnek */
    char **res = (char **)malloc(sizeof(char *) * (size + 1));
    int i=0;
    int p=0;
    while (buffer[i] != 0) {
        /* a sor elejét eltárolom */
        res[p++] = buffer + i;
        /* a sor végét megkeresem */
        do {
            i++;
        } while (buffer[i] != 0 && buffer[i] != delimeter);
        /* felülírjuk a ';'-t 0-val */
        if (buffer[i] != 0) {
            buffer[i] = 0;
            i++;
        }
    }

    /* null pointerrel zárjuk */
    res[p] = 0;
    return res;
}

// 1-et ad vissza, ha érvényes a kapott IP cím
// 0-át ha nem
int is_valid_ip (const char *ip) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ip, &(sa.sin_addr));
    return result != 0;
}

void die (const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(-1);
}

void read_lines_from_stdin (char *dst, int size) {
    char *line = NULL;  // ideiglenes buffer a getline-nak
    size_t len = 0;
    ssize_t read;
    int remaining_size; // dst stringbe még ennyi byte fér el

    remaining_size = size - 1;

    while ((read = getline(&line, &len, stdin)) != -1) {
        strncat(dst, line, remaining_size);
        remaining_size -= read;
        if (remaining_size <= 0)
            break;
    }
    chomp(dst); // utolsó \n karakter chompolása
    free(line);
}

char *strcutpbrk (char *str, const char *accept) {
    if (str == NULL)
        return NULL;

    if (accept == NULL)
        return str;

    char *at = strpbrk(str, accept);
    if (at != NULL)
        *at = '\0';

    return str;
}

char *strdelchars (char *str, const char *dels) {
    if (!str)
        return NULL;

    if (!dels)
        return str;

    int len_str = strlen(str);
    int len_dels = strlen(dels);

    if (!len_str || !len_dels)
        return str;

    int i, j, k = 0;
    for (i = 0; i < len_str; i++) {
        for (j = 0; j < len_dels; j++)
            if (str[i] == dels[j])
                break;
        if (j == len_dels) // ha nem volt break
            str[k++] = str[i];
    }
    str[k] = '\0';
    return str;
}

