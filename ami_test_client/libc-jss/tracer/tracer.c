/* tracer.c - simple trace file handling for debugging
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "debug.h"
#include "tracer.h"


#define TRACER_DEFAULT_START_ID 10000


// A tra->id -be rak egy új id-t, amit a tra->seqfile alapján inkrementál és
// ment el. Ha nem létezik a seq fájl vagy faszság van benne, akkor
// tra->start_id értéke kerül a tra->id -be, és ezt is menti el a seq fájlba.
//
// visszatérési érték:
// 0  = sikeres volt új id-t generálni és elmenteni
// -1 = valami grimbusz volt
static int new_id (tracer_t *tra) {
    // ha nem létezik még a tra->seqpath fájl, akkor legyártjuk 644-es joggal,
    // mert különben az fopen "r+" NULL-al tér vissza
    if (access(tra->seqpath, F_OK)) {
        int fd = open(tra->seqpath, O_RDWR | O_CREAT, 0644);
        close(fd);
    }

    FILE *file = fopen(tra->seqpath, "r+");
    if (file == NULL) {
        printf("Can't open %s: %s\n", tra->seqpath, strerror(errno));
        return -1;
    }

    // seq fájl zárolása
    flock(fileno(file), LOCK_EX);

    unsigned long long id = 0;
    fscanf(file, "%llu\n", &id);
    id = (id == 0) ? tra->start_id : id + 1;
    rewind(file);
    fprintf(file, "%llu\n", id);

    // seq fájl elengedése
    flock(fileno(file), LOCK_UN);
    fclose(file);

    tra->id = id;
    return 0;
}

// visszatérési érték:
// 0  = létezik a könyvtár
// -1 = nem létezik a könyvtár
static int test_directory_exists (const char *path) {
    DIR *dir = opendir(path);
    if (dir) {
        closedir(dir);
        return 0;
    }
    return -1;
}

// Létrehoz egy új tracer objektumot. Paraméterül kap egy könyvtár nevet, ahova
// a tracer fájlokat fogja menteni a tracer_open(), tracer_printf() és
// tracer_close(). A könyvtárnév lehet relatív elérési útvonal is. A
// tracer_new() a realpath() rendszerhívás segítségével megállapítja az
// abszolút útvonalat (/var/log/....) és a továbbiakban ezt fogja használni.
// Ezáltal a programban tetszés szerint lehet chdir() hívásokat elhelyezni.
// Azok nem fogják befolyásolni a tracert.
tracer_t *tracer_new (const char *directory) {
    tracer_t *tra = malloc(sizeof(*tra));
    if (tra == NULL) {
        fprintf(stderr, "tracer_new(): out of memory");
        return NULL;
    }
    memset(tra, 0, sizeof(*tra));

    // relatív útvonalat menti el a tra->path -ba
    realpath(directory, tra->path);

    // tra->path könyvtár készítése 755 jogokkal
    mkdir(tra->path, 0755);

    // ha nem létezik a könyvtár, akkor grimbusz van és NULL-ra állítjuk a
    // path-ot, aminek hatására a tracer_open() nem fog dolgozni
    if (test_directory_exists(tra->path)) {
        tra->path[0] = '\0';
    }

    // sorszám-fájl elérési útjának tárolása
    snprintf(tra->seqpath, sizeof(tra->seqpath), "%s/.seq", tra->path);

    // default értékek
    tra->start_id = TRACER_DEFAULT_START_ID; // ezt írja felül a tracer_set_start_id()

    return tra;
}

void tracer_open (tracer_t *tra) {
    if (tra == NULL || tra->path[0] == '\0')
        return;

    // ha már van nyitott fájlunk, akkor azt először bezárjuk
    if (tra->file != NULL)
        tracer_close(tra);

    // új tra->id generálása seq fájlból. Ha nem sikerül, akkor return-al
    // visszatérünk
    if (new_id(tra))
        return;

    // fájl teljes elérési útjának elkészítése
    snprintf(tra->filepath, sizeof(tra->filepath), "%s/%llu", tra->path, tra->id);

    // megnyitás írásra
    tra->file = fopen(tra->filepath, "w");
}

unsigned long long tracer_close (tracer_t *tra) {
    if (tra == NULL || tra->file == NULL)
        return 0;

    fclose(tra->file);
    tra->file = NULL;
    unsigned long long tmp = tra->id;
    tra->id = 0;
    return tmp;
}

void tracer_printf (tracer_t *tra, const char *fmt, ...) {
    if (tra == NULL || tra->file == NULL)
        return;

    va_list ap;
    va_start(ap, fmt);
    vfprintf(tra->file, fmt, ap);
    va_end(ap);
}


void tracer_destroy (tracer_t *tra) {
    if (tra == NULL)
        return;

    tracer_close(tra);
    free(tra);
}

void tracer_set_start_id (tracer_t *tra, int start_id) {
    if (tra == NULL)
        return;

    tra->start_id = start_id;
}

unsigned long long tracer_get_id (tracer_t *tra) {
    if (tra == NULL)
        return 0;

    return tra->id;
}

char *tracer_get_id_as_string (tracer_t *tra) {
    if (tra == NULL)
        return 0;

    snprintf(tra->id_as_string, sizeof(tra->id_as_string), "%llu", tra->id);
    return tra->id_as_string;
}

