/* amysql.c
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/* TODO: megvizsgálni a libzdb-t, mert ismeri a MySQL-t, PostgreSQL-t és az
Oracle-t is. A libzdb tudtommal nem aszinkron, viszont az amysql.c mintájára
aszinkronná lehetne tenni:) */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mysql/mysql.h>
#include <ev.h>

#include "debug.h"
#include "logger.h"

#define FIELDBUFSIZE 8192
#define FIELDVSIZE 32
#define QUERYBUFSIZE 4096

MYSQL *db;
MYSQL_RES *result;
MYSQL_ROW row;
int busy = 0;
void (*callback)(int, char**, char*);

int fieldc;                  // result mezők száma
char *fieldv[FIELDVSIZE];    // result mező elemek
char fieldbuf[FIELDBUFSIZE];     // result buffer (ide mutatnak a fieldv elemek)
char querybuf[QUERYBUFSIZE];     // itt tároljuk el a query-t
char error_str[256];
int error_num;

struct option_t {
    char host[64];
    char user[64];
    char password[64];
    char database[64];
    char charset[16];
    int connect_timeout;
    int data_timeout;
} option = {
    .host = "localhost",
    .user = "",
    .password = "",
    .database = "",
    .charset = "",          // alapértelmezetten nem piszkáljuk a charset-et
    .connect_timeout = 10,  // TODO: prod környezetben ezeket magasra venni!
    .data_timeout = 20,     /* Each attempt uses this timeout value and there
                            are retries if necessary, so the total effective
                            timeout value is three times the option value. */
};

static ev_async evasync_main;
static ev_async evasync_thread;

static struct ev_loop *loop_main;    // főprogram event loopja (kapja az amysql_init())
static struct ev_loop *loop_thread;  // thread event loopja

pthread_t thread_id;

void amysql_option_host (const char *host) {
    strncpy(option.host, host, sizeof(option.host) - 1);
}

void amysql_option_user (const char *user) {
    strncpy(option.user, user, sizeof(option.user) - 1);
}

void amysql_option_password (const char *password) {
    strncpy(option.password, password, sizeof(option.password) - 1);
}

void amysql_option_database (const char *database) {
    strncpy(option.database, database, sizeof(option.database) - 1);
}

void amysql_option_charset (const char *charset) {
    strncpy(option.charset, charset, sizeof(option.charset) - 1);
}

void amysql_option_connect_timeout (int connect_timeout) {
    option.connect_timeout = connect_timeout;
}

void amysql_option_data_timeout (int data_timeout) {
    option.data_timeout = data_timeout;
}

// thread: AMYSQL, ha amysql_query() hívás volt
// thread: nincs definiálva, ha amysql_sync_query() hívás volt
// Függvény a következőket csinálja:
//  reset, init, config, connect, query, fetch, save, disconnect, free
static void query () {
    con_debug("send query: %s", querybuf);
    //** Buffer reset **//
    memset(fieldbuf, 0, sizeof(fieldbuf));
    memset(fieldv, 0, sizeof(fieldv));
    memset(error_str, 0, sizeof(error_str));
    fieldc = 0;
    db = NULL;
    result = NULL;

    //** Init **//
    db = mysql_init(NULL);
    if (db == NULL) {
        con_debug("mysql_init() returned NULL");
        char *errmsg = "mysql_init(): Can't allocate memory";
        strncpy(error_str, errmsg, sizeof(error_str));
        error_num = -1;
        goto done;
    }

    //** Config **//
    mysql_options(db, MYSQL_OPT_CONNECT_TIMEOUT, &option.connect_timeout);
    mysql_options(db, MYSQL_OPT_READ_TIMEOUT, &option.data_timeout);
    mysql_options(db, MYSQL_OPT_WRITE_TIMEOUT, &option.data_timeout);

    //** Connect **//
    if (!mysql_real_connect(db, option.host, option.user, option.password, option.database, 0, NULL, 0)) {
        con_debug("Can't connect to MySQL server: %s", mysql_error(db));
        strncpy(error_str, mysql_error(db), sizeof(error_str));
        error_num = -1;
        goto done;
    }

    if (option.charset && strlen(option.charset)) {
        if (mysql_set_character_set(db, option.charset)) {
            strncpy(error_str, mysql_error(db), sizeof(error_str));
            error_num = -1;
            goto done;
        }
    }

    //** Query **//
    if (mysql_query(db, querybuf)) {;
        con_debug("query failed: %s", mysql_error(db));
        strncpy(error_str, mysql_error(db), sizeof(error_str));
        error_num = -1;
        goto done;
    }

    //** Result **//
    result = mysql_store_result(db);
    if (result == NULL) { // INSERT INTO sikeres
        con_debug("INSERT or UPDATE OK: result = NULL");
        error_num = 0;
        goto done;
    }

    //** Fetch only ONE row **//
    int num_fields = mysql_num_fields(result);
    row = mysql_fetch_row(result);
    if (row == NULL) { // SELECT-ben a WHERE nem illeszkedett semmire
        con_debug("SELECT empty: row = NULL");
        error_num = 0;
        goto done;
    }

    /* Az alábbi for ciklus a következőket csinálja: Egyetlen row mezőinek
    másolása fieldbuf bufferbe és feldarabolása fieldv-be. A fieldbuf-ba
    folyamatosan bemásoljuk a row-ból kiolvasott byteokat. Ezzel a
    folytonossággal helyet spórolunk meg, mert nem kell minden mezőnek fix
    méretet lefoglalni. A fieldv mutató tömbbe pedig az egyes mezők
    kezdőcimeit mentjük el, ami a fieldbuf azon indexére mutat, ahol az
    adott mező kezdődik. Az argc és *argv[] pároshoz hasonlóan a fieldc
    változóba kerül a fieldv tömb mérete. Ha elegendő volt a FIELDBUFSIZE és
    a FIELDVSIZE, akkor ez megegyezik a MySQL által visszaadott
    mysql_num_fields() értékével. Ellenkező esetben annyi lesz, amennyit
    sikerült kigyűjteni, tehát amennyit biztonsággal ki lehet olvasni. */

    //** Save **//
    int rpos = 0;
    for (fieldc = 0; fieldc < num_fields && fieldc < FIELDVSIZE; fieldc++) {
        if (row[fieldc] == NULL) {
            fieldv[fieldc] = NULL;
        } else {
            fieldv[fieldc] = &fieldbuf[rpos];
            int i;
            for (i = 0; row[fieldc][i] != 0; i++) {
                fieldbuf[rpos] = row[fieldc][i];
                rpos++;
                if (rpos > FIELDBUFSIZE - 2) { // ne szaladjunk túl a fieldbuf méretén
                    fieldc++;
                    error_num = 0;
                    goto done;
                }
            }
            fieldbuf[rpos] = 0;
            rpos++;
            if (rpos > FIELDBUFSIZE - 2) { // ne szaladjunk túl a fieldbuf méretén
                fieldc++;
                error_num = 0;
                goto done;
            }
        }
    }
    //~ int k; for (k = 0; k < fieldc; k++) printf("%d s %s s\n", k, fieldv[k]); printf("\n");

done:
    //** Free & Disconnect **//
    if (result != NULL)
        mysql_free_result(result);
    if (db != NULL)
        mysql_close(db);

    // TODO: strerror üzenetét hozzácsapni az errstr-hez, az err != 0
    // ha 4-es hiba (interrupted system call) jön, akkor azt a mysql_connect()
    // timeout okozza. Vajon ez hogy van megoldva? SIGALRM??? Az bebaszna!
    //perror("strerror()");

}

void query_and_evasync () {
    query();

    con_debug("ev_async: AMYSQL -> MAIN");
    ev_async_send(loop_main, &evasync_main);
}

// thread: MAIN
int amysql_is_busy () {
    return busy;
}

// thread: MAIN
int amysql_query (void *cb, const char *fmt, ...) {
    if (busy) {
        con_debug("amysql is BUSY");
        return -1;
    }
    busy = 1;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(querybuf, sizeof(querybuf) - 1, fmt, ap);
    va_end(ap);

    callback = cb;

    con_debug("ev_async: MAIN -> AMYSQL");
    ev_async_send(loop_thread, &evasync_thread);
    return 0;
}

// thread: MAIN
static void invoke_callback (EV_P_ ev_async *w, int revents) {
    /* callback hívás közben már lehet új amysql_query() függvényt hívni,
    mert féligmeddig biztonságos. Lehetőség szerint a callback függvény
    legvégén legyen új amysql_query() hívás, mert az amysql thread felül
    fogja írni a fieldc/fieldv és error_num/error_str változókat. */
    busy = 0;

    callback(fieldc, fieldv, (error_num) ? error_str : NULL);
}

// thread: AMYSQL
static void *infinite_loop () {
    /* Itt lehet inicializálni az AMYSQL thread saját event watchereit */

    ev_loop(loop_thread, 0);
    con_debug("FATAL: amysql event loop exited! ");
    return NULL;
}

// thread: MAIN
int amysql_init (struct ev_loop *loop) {
    busy = 0;

    loop_main = loop;
    loop_thread = ev_loop_new(EVFLAG_AUTO);

    ev_async_init(&evasync_main, (void*)invoke_callback);
    ev_async_init(&evasync_thread, (void*)query_and_evasync);

    /* A mostani kezdetleges thread tapasztalataim alapján, szerintem az
    ev_async watchereket itt, még a pthread_create() előtt kell indítani.
    Azaz még a hívó thread kontextusában. Mert ha az új thread-be
    indítanánk, akkor elképzelhető, hogy visszatérés után a hívó függvény
    egyből egy ev_async_send() hívással folytatja a futást és lehet, hogy az
    új thread még nem járna az ev_async_init()-nél és ev_async_start()-nál.
    Ez pedig ugye nem jó, mert azelőtt hívnánk meg valamit, mint hogy
    inicializáltuk volna. A többi, (a thread-re nézve saját) event watcher,
    io, időzítő, stb.. inicializálását már végezheti a thread saját maga,
    tehát mehet a pthread_create() által meghívott függvénybe. */
    ev_async_start(loop_main, &evasync_main);
    ev_async_start(loop_thread, &evasync_thread);

    int err;
    err = pthread_create(&thread_id, NULL, infinite_loop, NULL);
    if (err)
        con_debug("thread creating error: (%d) %s", err, strerror(err));
    return err;
}

int amysql_sync_query (int *parc, char **parv[], const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(querybuf, sizeof(querybuf) - 1, fmt, ap);
    va_end(ap);

    query();

    if (parc)
        *parc = fieldc;
    if (parv)
        *parv = fieldv;

    return error_num;
}

const char *amysql_strerror () {
    return (error_num) ? error_str : NULL;
}
