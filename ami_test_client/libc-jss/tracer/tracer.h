/* tracer.h - simple trace file handling for debugging
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*
    SYNOPSIS
    --------

    tracer_t *tra = tracer_new("MyTraceLogFolder");
    if (tra == NULL) {
        printf("out of memory\n");
        return -1;
    }

    // first file
    tracer_open(tra);
    tracer_printf(tra, "blahhh\n");
    tracer_close(tra);

    // secondary file
    tracer_open(tra);
    tracer_printf(tra, "hello %d\n", 59);
    tracer_printf(tra, "hallo %d\n", 61);

    // you can save this ID to your logs for later examination
    unsigned long long id;
    id = tracer_get_id(tra);

    // close and retrieve ID in one step
    printf("closed, tracer id = %llu\n", tracer_close(tra));

    // close and free all resources
    tracer_destroy(tra);


    DESCRIPTION
    -----------

    A tracer elve az, hogy a program (démon) futása során tetszőleges,
    nagyobb mennyiségű információt gyűjthessünk össze debuggolás céljából
    úgy, hogy az ne szemetelje tele a konzolt vagy a log fájlokat. Ezek a
    debug infók sorszámozott szöveges fájlokba kerülnek, majd a fájlok
    sorszámára lehet hivatkozni a logban, így utólag könnyen kikereshető egy
    esemény részletes körülményképe. Ráadásul egy `find -mtime` paranccsal
    gyorsan meg lehet szabadnulni a régi, fölösleges tracelogoktól.

*/


#include <limits.h>


typedef struct tracer_t {
    unsigned long long id;
    char id_as_string[32];
    unsigned long long start_id;
    FILE *file;
    char path[PATH_MAX]; // realpath() függvény ide rakja az abszolút elérési utat
    char filepath[PATH_MAX]; // path + fájlnév kerül ide (tracer adat fájl)
    char seqpath[PATH_MAX]; // path + fájlnév kerül ide (sorszám fájl)
} tracer_t;


// új tracer objektum, melynek munkakönyvtára a directory-ban megadott abszolút
// vagy relatív elérési útvonal. A tracer_new() a realpath() rendszerhívás
// segítségével megállapítja az abszolút útvonalat (/var/log/....) és a
// továbbiakban ezt fogja használni. Ezáltal a programban tetszés szerint lehet
// chdir() hívásokat elhelyezni, mert azok nem fogják befolyásolni a tracert.
tracer_t *tracer_new (const char *directory);

// Megnyitja írásra a sorban következő új fájlt. A fájl sorszámát a
// munkakönyvtárban található .seq fájl alapján inkrementálja. Ha a .seq nem
// létezik, akkor a tracer_set_start_id() függvénnyel beállított érték lesz a
// kezdő sorszám. Ennek alapértelmezett értéke TRACER_DEFAULT_START_ID. Ha a
// tracer_new() nem tudja beállítani vagy létrehozni a munkakönyvtárat,
// akkor a tracer_open() nem csinál semmit. A .seq fájl kezelése szál-biztos és
// processz-biztos, mert flock() által van zárolva.
void tracer_open (tracer_t *tra);

// Formázott szöveget ír a már megnyitott fájlba. Ha nincs megnyitva a fájl,
// akkor nem csinál semmit.
void tracer_printf (tracer_t *tra, const char *fmt, ...);

// Lezárja a tracer fájlt és visszaadja a sorszámát, ugyan azt, amit a
// tracer_get_id() ad vissza.
unsigned long long tracer_close (tracer_t *tra);

// Ha van, akkor lezárja az éppen megnyitott fájlt és felszabadítja a tracer
// objektum által lefoglalt memóriát. Ha a paraméterül NULL-t kap, nem csinál
// semmit.
void tracer_destroy (tracer_t *tra);

// Beállítja a kezdő sorszámot, mely akkor lesz felhasználva, ha nincs .seq
// fájl. Az alapértelmezett érték TRACER_DEFAULT_START_ID.
void tracer_set_start_id (tracer_t *tra, int start_id);

// Visszaadja az éppen használt fájl sorszámát. Ha a hívás pillanatában nincs
// nyitva fájl, tehát nem egy tracer_open() és egy tracer_close() között
// vagyunk, akkor a visszatérési érték "0".
unsigned long long tracer_get_id (tracer_t *tra);

// String-ként adja vissza az éppen használt fájl sorszámát. Ha a hívás
// pillanatában nincs nyitva fájl, tehát nem egy tracer_open() és egy
// tracer_close() között vagyunk, akkor a visszatérési érték "0".
char *tracer_get_id_as_string (tracer_t *tra);

