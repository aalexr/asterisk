/* htclient.h
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#define HTCLIENT_BUFSIZ 1024

// változás esetén módosítsd a htclient_dump() függvényt a htclient.c fájlban!
typedef struct htclient_t {
    struct {
        void *url_parser_ptr;           // url_parser.h -ban definiált pointer, ezt kell majd felszabadítani
        char full[HTCLIENT_BUFSIZ];     // http://jsi@jss.hu/auth?param=59
        char *scheme;                   // http
        char *host;                     // jss.hu
        char *port;                     // (null)
        char *path;                     // auth
        char *query;                    // param=59
        char *fragment;                 // (null)
        char *username;                 // jsi
        char *password;                 // (null)
        int set;                        // 1 lesz, ha rendben beállítottuk az URL-t
    } url;
    char request_data[HTCLIENT_BUFSIZ];
    char response_data[HTCLIENT_BUFSIZ];
    struct {
        char *name;
        char *value;
    } headers[16];
    int num_headers;
    char *header_buf;
    char error[128];
} htclient_t;

// új htclient objektum
htclient_t *htclient_new ();

// URL beállítása
// Kezeli a scheme, host, port, path URL részeket. Tovább infó a
// htclient_t->url struktúrában. Az URL path részét nem kezeli a
// htclient_perform(), ezért azt külön el kell menteni a request-ben.
//
// például: http://jss.hu:3333/akarmi.jpg
int htclient_url (htclient_t *htc, const char *fmt, ...);

// Beállítja a teljes HTTP request-et. Beleértve a fejléceket meg mindent. 
void htclient_request_set (htclient_t *htc, const char *fmt, ...);

// Végrehajtja a lekérdezést. A függvény addig blokkol, amíg nem érkezik meg a
// válasz vagy valami hiba történik.
int htclient_perform (htclient_t *htc);

// Visszaadja a válaszban szereplő header értékét. Ha a megadott header nem
// létezik, akkor a visszatérési érték NULL.
char *htclient_header_get (htclient_t *htc, const char *name);

// Felszabadítja a htclient objektumot
void htclient_destroy (htclient_t *htc);

// Ha volt hiba, akkor visszaadja a hibastringet, ha nem, akkor NULL
char *htclient_error (htclient_t *htc);

// htclient_t struktúra dumpolása
void htclient_dump (htclient_t *htc);

