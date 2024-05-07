/* logger.c
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

#include "logger.h"
#include "debug.h"
#include "misc.h"

#include "utlist.h"
#include "misc.h"

//~ #define concat(dst, src) strncat((dst), (src), ((sizeof(dst)) - strlen(dst) - 1))
//~ #define concatf(dst, ...) snprintf((dst) + strlen((dst)), sizeof((dst)) - strlen((dst)), __VA_ARGS__)

static char *logfile = NULL;
static char logfile_buf[128];

/* TODO: rendbe tenni ezt a marhaságot valami kultúrált megoldással úgy, 
hogy a hívó függvény által átadott stringet lemásoljuk és helyben a 
másolatot kezeljük. Azért, hogy a hívó utána a saját mutatójával azt 
csinálhasson, amit csak akar. Kezelni a NULL-t, a default értékeket és 
valami kultúrált köntöst adni neki, ne ezt a három változós izét. */
static char *timestamp_format_default = "%a %H:%M:%S";
static char *timestamp_format = "%a %H:%M:%S";
static char timestamp_format_buf[128];

static int initialised = 0;
struct timeval first_time = {0, 0};
char ela[16];

typedef struct el {
    struct timeval tm;
    char *line;
    struct el *prev; /* needed for a doubly-linked list only */
    struct el *next; /* needed for singly- or doubly-linked lists */
} el;

/* Ha a tv (első paraméter) meg van adva, akkor nincs gettimeofday hívás, hanem 
 * a tv-ben beállított idő lesz a mérvadó. Ha a tv értéke NULL, akkor a 
 * jelenlegi idővel (gettimeofday) számolunk. Ha meg van adva a ptm, akkor az is
 * be lesz állítva. Ha az elapsed nem NULL, akkor kiszámolja az indítástól 
 * eltelt időt, és visszaad egy mutatót, ami a kiszámolt érték ssss.mmmu 
 * formátumú stringre mutat.
 */
static struct timeval get_time (struct timeval *tv, struct tm **ptm, char *elapsed) {
    struct timeval mytv;
    
    if (tv == NULL) {
        gettimeofday(&mytv, NULL);
    } else {
        mytv.tv_sec = tv->tv_sec;
        mytv.tv_usec = tv->tv_usec;
    }

    if (tv)
        *tv = mytv;

    if (ptm)
        *ptm = localtime(&mytv.tv_sec); // thread safe?

    if (elapsed) {
        long int delta = ((mytv.tv_sec * 1000000) + mytv.tv_usec) - ((first_time.tv_sec * 1000000) + first_time.tv_usec);
        snprintf(elapsed, 8, "%3.3f", (float)delta / 1000000);
    }

    return mytv;
}

void con_init () {
    if (initialised)
        return;
    first_time = get_time(NULL, NULL, NULL);
    initialised = 1;
    return;
}

void con_logfile (const char *file) {
    if (file == NULL) {
        logfile = NULL;
        return;
    }
    strncpy(logfile_buf, file, sizeof(logfile_buf));
    logfile = logfile_buf;  
}

void con_timestamp_format (const char *format) {
    if (format == NULL) {
        timestamp_format = timestamp_format_default;
        return;
    }
    strncpy(timestamp_format_buf, format, sizeof(timestamp_format_buf));
    timestamp_format = timestamp_format_buf;
}

// gány, memóriazabáló függvény :)
void _con_writef (enum con_callmode cm, char *file, int line, const char *function, const char *fmt, ...) {
    if (!initialised)
        con_init();

    // botrányos deklarációk
    char tmp[1024] = {0}; // ide kerül a végeredmény
    char tmp2[1024]; // va_arg szöveges kimenete
    char timestamp[64]; // timestamp
    char elapsed[64]; // elapsed

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp2, sizeof(tmp2), fmt, ap);
    va_end(ap);

    // TODO: ezt az idő lekérdezős részt külön függvénybe kell rakni!
    struct timeval tv;
    struct tm *ptm;
    tv = get_time(NULL, &ptm, NULL); // idő, ami alapján a továbbiakban számolunk
    strftime(timestamp, sizeof(timestamp), timestamp_format, ptm); // szöveges timestamp
    get_time(&tv, &ptm, elapsed); // elapsed lekérdezése TODO: ez borzalmas, átírni!!!

    switch (cm) {
        case CON_CALLMODE_CONFT:
        case CON_CALLMODE_CONFTN:
            concatf(tmp, "%s %s", timestamp, tmp2);
            break;

        case CON_CALLMODE_CONF:
        case CON_CALLMODE_CONFN:
            strncpy(tmp, tmp2, sizeof(tmp) - 1);
            break;

        case CON_CALLMODE_DEBUG:
            concatf(tmp, "%s /%s/ [%s:%d %s]: %s", timestamp, elapsed, file, line, function, tmp2);
            break;
    }

    chomp(tmp);

    // Kiiratás
    printf("%s", tmp);
    if (cm != CON_CALLMODE_CONFTN && cm != CON_CALLMODE_CONFN)
        printf("\n");
    fflush(stdout);

    if (logfile) {
        FILE *f = fopen(logfile, "a");
        if (f != NULL) {
            fprintf(f, "%s", tmp);
            if (cm != CON_CALLMODE_CONFTN && cm != CON_CALLMODE_CONFN)
                fprintf(f, "\n");
            fclose(f);
        }
    }

}

