/* netsocket.c
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

// TODO: datapipe-nál jól látható, hogy a connect előtt bejött adatok
// nem kerülnek kiküldésre. Valami buffer megoldást kéne alkalmazni. vagy mégsem?
// ... végülis a Centauri nézőpontja szerint connect előtt miért küldenénk adatokat?

// TODO: gethostbyaddr és gethostbyname függvényeket lecserélni getaddrinfo és
// getnameinfo függvényekre
//
// Figyelem! Amelyik függvényben invoke_callback van, annak a legaljára kell
// destroy_netsocket. Na meg ezt a kommentet átfogalmazni, mert valószínűleg
// baromira nem fogom érteni két hét múlva, hogy mi a szar is akar ez lenni

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdarg.h>

#include "netsocket.h"
#include "debug.h"

static void dummy_callback (netsocket_t *obj, int event) {
    printf("dummy_callback called at %s:%d\n", __FILE__, __LINE__);
    printf(" event = %d\n mode = %s\n host = %s\n port = %d\n lhost = %s\n lport = %d\n",
        event,
        (obj->mode == NETSOCKET_SERVER) ? "server" : "client",
        obj->host,
        obj->port,
        obj->lhost,
        obj->lport
    );
}

static void sock_close (netsocket_t *obj) {
    if (!obj->sock)
        return;
    ev_io_stop(obj->loop, &obj->w_in);
    ev_io_stop(obj->loop, &obj->w_out);
    ev_timer_stop(obj->loop, &obj->w_connect_timeout);
    close(obj->sock);
    obj->sock = 0;
}

static void invoke_callback (netsocket_t *obj, int event) {
    obj->in_callback++;
    obj->callback(obj, event);
    obj->in_callback--;

    // késleltetett destroy
    if (obj->destroy_request) {
        if (!obj->in_callback) {
            netsocket_destroy(obj);
        }
    }
}


static void disconnect (netsocket_t *obj, char *reason, int ignore_callback) {
    if (obj == NULL)
        return;

    sock_close(obj);
    if (reason != NULL)
        strncpy(obj->disconnect_reason, reason, sizeof(obj->disconnect_reason) - 1);

    if (!ignore_callback)
        invoke_callback(obj, NETSOCKET_EVENT_DISCONNECT);

    //~ if (!obj->connected)
        //~ return;
    obj->connected = 0;

    // automatikusan felszabaditjuk a gyermek objektumot abban az esetben, ha
    // az obj kliens modban van es bejovo kapcsolatrol van szo, maskeppen mondva
    // akkor, ha egy szerver objektum gyermek objektuma szakad meg
    // ilyen objektumot a sock_accept allokal malloccal

    //~ if (obj->mode == NETSOCKET_CLIENT && obj->direction == NETSOCKET_IN)
        //~ netsocket_destroy(obj);
}

/**
 * @brief Disconnect from peer, close socket
 * @param obj the netsocket object
 * @param reason description of disconnect reason
 *
 * If connection in progress, abort the connection and close the socket.
 * If connection was established, disconnect from peer, close socket and
 * detach from event loop. Call the callback with NETSOCKET_EVENT_DISCONNECT
 * event. Save the "reason" to obj->reason.
 */
void netsocket_disconnect (netsocket_t *obj, char *reason) {
    disconnect(obj, reason, 1); // 1 jelzi, hogy nem kérünk callback hívást
}

void netsocket_disconnect_withevent (netsocket_t *obj, char *reason) {
    disconnect(obj, reason, 0); // 0 jelzi, hogy kérünk callback hívást
}

// ha ezt meghívjuk, akkor a bejövő kapcsolatoknál nem kérdezzük meg
// a kliens IP címének nevét a DNS szervertől
void netsocket_disable_lookup_on_accept (netsocket_t *obj) {
    obj->disable_lookup_on_accept = 1;
}

static int sock_accept (netsocket_t *parent) {
    netsocket_t *obj = netsocket_new(dummy_callback, NULL, parent->loop);

    obj->mode = NETSOCKET_CLIENT;
    obj->direction = NETSOCKET_IN;

    socklen_t addrlen = sizeof(obj->addr);
    obj->sock = accept(parent->sock, (struct sockaddr *) &obj->addr, &addrlen);
    if (obj->sock < 0) { // TODO rendes hibakezelés, memória felszabadítás
        perror("accept"); // például: Too many open files
        netsocket_destroy(obj);
        return -1;
    }

    // kliens IP cím lekérdezése, ha nincs beállítva a "disable_lookup_on_accept"
    if (!parent->disable_lookup_on_accept)
        obj->hostent = gethostbyaddr(&obj->addr.sin_addr, sizeof(&obj->addr.sin_addr), AF_INET);

    // ha nincs hostja az IP-nek, akkor a hostent NULL lesz
    if (obj->hostent != NULL)
        strncpy(obj->host, obj->hostent->h_name, sizeof(obj->host) - 1);

    // IP cím tárolása szöveges formátumban
    strncpy(obj->ip, inet_ntoa(obj->addr.sin_addr), sizeof(obj->ip) - 1);

    // ha nincs hostja az IP-nek, akkor az IP lesz a host
    if (!strlen(obj->host))
        strncpy(obj->host, obj->ip, sizeof(obj->host) - 1);

    // Kliens portja
    obj->port = ntohs(obj->addr.sin_port);

    ev_io_set(&obj->w_in, obj->sock, EV_READ);
    ev_io_set(&obj->w_out, obj->sock, EV_WRITE);

    ev_io_start(obj->loop, &obj->w_in);
    ev_io_start(obj->loop, &obj->w_out);

    obj->parent = parent;
    obj->callback = parent->callback;
    obj->userdata = parent->userdata;
    strncpy(obj->lhost, parent->lhost, sizeof(obj->lhost));
    obj->lport = parent->lport;

    return 0;
}

static void w_connect_timeout_cb (EV_P_ ev_io *w, int revents) {
    netsocket_t *obj = w->data;
    disconnect(obj, "Connection timed out", 0);
}

static void w_in_cb (EV_P_ ev_io *w, int revents) {
    netsocket_t *obj = w->data;
    int i;

    // ha a szerver portra csatlakozott új kliens
    if (obj->mode == NETSOCKET_SERVER) {
        sock_accept(obj);
        return;
    }

    /* Ide akkor kerülünk, amikor egy kliensről adat érkezik.
     * A NETSOCKET_EVENT_CONNECT eseményt és az obj->connected 1-re állítását
     * a w_out_cb() függvény okozza, de az valójában a sock_accept() függvényben
     * lenne esedékes. Szervernél, amikor várjuk a klienseket, normális esetben
     * nincs ezzel semmmi gond, de ha valgrind-el fut, akkor valamiért
     * összekeveredik az event loop-ban a sorrend és előbb hívódik meg az
     * új kliens objektummal a NETSOCKET_EVENT_READ, mint a NETSOCKET_EVENT_CONNECT.
     * Ez a hívó kódjában okozhat kollóziót (és okozott is), ezért ha nincs
     * beállítva az obj->connected, de mégis adatot akarnánk beolvasni, akkor
     * egyszerűen csak "elnapoljuk" a feladatot. Ekkor az event loop megteszi
     * prevenciós körét a w_out_cb() függvényben is, ahol megtörténik a
     * NETSOCKET_EVENT_CONNECT és utána ismét visszatér ide. Ezzel kényszerítjük
     * ki a helyes sorrendet.
     */
    if (!obj->connected)
        return;

    // adat beolvasása a kliensről
    i = read(obj->sock, obj->inbuf, sizeof(obj->inbuf));
    if (i < 1) {
        disconnect(obj, (i == 0) ? "Connection reset by peer" : strerror(errno), 0);
    } else {
        obj->inbuf_len = i;
        invoke_callback(obj, NETSOCKET_EVENT_READ);
    }
}

static int sockerr (int sock) {
    int optval = 0;
    int err;
    socklen_t optlen = sizeof(optval);
    err = getsockopt(sock, SOL_SOCKET, SO_ERROR, &optval, &optlen);
    if (err) return err;
    return optval;
}

static void w_out_cb (EV_P_ ev_io *w, int revents) {
    netsocket_t *obj = w->data;
//~ printf("Can't connect to %s: %s\n", obj->host, strerror(sockerr(obj->sock)));
    obj->err = sockerr(obj->sock);
    ev_io_stop(EV_A_ w);
    if (obj->err) {
        disconnect(obj, strerror(obj->err), 0);
    } else {
        obj->connected = 1;
        ev_timer_stop(obj->loop, &obj->w_connect_timeout);
        invoke_callback(obj, NETSOCKET_EVENT_CONNECT);
    }
}

/**
 * @brief Make outgoing connection to remote host
 * @param obj the netsocket object
 * @returns 0 if everything ok, otherwise -1
 *
 * Use netsocket as client. The function check the syntax of the "host" and "port" variable and
 * lookup the host. If the host resolved successfully, then create an
 * non-blocking outgoing socket, call connect(3) and add the socket to
 * the event loop. In case of problem, close socket and call callback
 * with event NETSOCKET_EVENT_DISCONNECT.
 */
int netsocket_connect (netsocket_t *obj) {
    obj->mode = NETSOCKET_CLIENT;
    obj->direction = NETSOCKET_OUT;

    if (obj->host == NULL) {
        disconnect(obj, "Invalid host", 0);
        return -1;
    }

    if (obj->port < 1 || obj->port > 65535) {
        disconnect(obj, "Invalid port", 0);
        return -1;
    }

    // TODO aszinkronná tenni a host lookup-ot
    if ((obj->hostent = gethostbyname(obj->host)) == NULL) {
        disconnect(obj, "Host not found", 0);
        return -1;
    }

    struct in_addr **pptr;
    pptr = (struct in_addr **)obj->hostent->h_addr_list;
    strncpy(obj->ip, inet_ntoa(**(pptr++)), sizeof(obj->ip));
    obj->sock = socket(PF_INET, SOCK_STREAM, 0);

    ev_io_set(&obj->w_in, obj->sock, EV_READ);
    ev_io_set(&obj->w_out, obj->sock, EV_WRITE);
    ev_timer_set(&obj->w_connect_timeout, (float)obj->connect_timeout / 1000, 0);

    ev_io_start(obj->loop, &obj->w_in);
    ev_io_start(obj->loop, &obj->w_out);
    ev_timer_start(obj->loop, &obj->w_connect_timeout);

    // socket non-block
    fcntl(obj->sock, F_SETFL, fcntl(obj->sock, F_GETFL, 0) | O_NONBLOCK);

    bzero(&obj->addr, sizeof(obj->addr));
    obj->addr.sin_family = AF_INET;
    obj->addr.sin_port = htons(obj->port);
    obj->addr.sin_addr.s_addr = *(long*)(obj->hostent->h_addr);

    connect(obj->sock, (struct sockaddr*)&obj->addr, sizeof(obj->addr));
    if (errno != EINPROGRESS) {
        netsocket_disconnect(obj, strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * @brief Listening for incoming connections
 * @param obj the netsocket object
 * @returns 0 if everything ok, otherwise -1
 *
 * Use netsocket as server. The function check the syntax of
 * the "lhost" and "lport" variable and
 * lookup the lhost. If the host resolved successfully, then create an
 * non-blocking incoming socket, bind to lhost, and add the socket to
 * the event loop. If "lhost" is NULL, then "0.0.0.0" is assumed.
 *
 * When a peer connects to the listening port, create a new netsocket object and inherit
 * lhost, lport, userdata and callback variables from the server object.
 * Accept the incoming connection,
 * add them to the event loop and call the callback with NETSOCKET_EVENT_CONNECT event.
 * The callback's object parameter is the newly created client object. The server object's
 * address is in obj->parent pointer. For each incoming connection make individually a
 * new netsocket object.
 */
int netsocket_listen (netsocket_t *obj) {
    obj->mode = NETSOCKET_SERVER;

    if (!strlen(obj->lhost))
        strcpy(obj->lhost, "0.0.0.0");

    if (obj->lport < 1 || obj->lport > 65535) {
        disconnect(obj, "Invalid local port", 0);
        return -1;
    }

    // TODO aszinkronná tenni a host lookup-ot
    if ((obj->hostent = gethostbyname(obj->lhost)) == NULL) {
        disconnect(obj, "Local host not found", 0);
        return -1;
    }

    struct in_addr **pptr;
    pptr = (struct in_addr **)obj->hostent->h_addr_list;
    strncpy(obj->ip, inet_ntoa(**(pptr++)), sizeof(obj->ip));

    // TODO: socket függvény hibájának csekkolása
    obj->sock = socket(AF_INET, SOCK_STREAM, 0);

    fcntl(obj->sock, F_SETFL, fcntl(obj->sock, F_GETFL, 0) | O_NONBLOCK);

    int optval = 1;
    if (setsockopt(obj->sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt"); // TODO normális hibakezelés
        return -1;
    }

    bzero(&obj->addr, sizeof(obj->addr));
    obj->addr.sin_family = AF_INET;
    obj->addr.sin_port = htons(obj->lport);
    obj->addr.sin_addr.s_addr = *(long*)(obj->hostent->h_addr);

    if (bind(obj->sock, (struct sockaddr *) &obj->addr, sizeof(obj->addr)) < 0) {
        disconnect(obj, "Error on bind()", 0); // TODO rendes hibaüzenet
        return -1;
    }

    if (listen(obj->sock, 5)) {
        disconnect(obj, "Error on listen()", 0);
        return -1;
    }

    ev_io_set(&obj->w_in, obj->sock, EV_READ);
    ev_io_set(&obj->w_out, obj->sock, EV_WRITE);
    //~ ev_timer_set(&obj->w_connect_timeout, (float)obj->connect_timeout / 1000, 0);

    ev_io_start(obj->loop, &obj->w_in);
    ev_io_start(obj->loop, &obj->w_out);
    //~ ev_timer_start(EV_DEFAULT, &obj->w_connect_timeout);

    return 0;
}

/**
 * @brief write data to netsocket socket
 * @param obj the netsocket object
 * @param data data string
 * @param length length of data string
 * @returns length of written data
 *
 * Write data to socket. O yeahhh:)
 */
int netsocket_write (netsocket_t *obj, const char *data, int length) {
    int i = 0;
    if (!obj->sock) return 0; // véletlenül sem írunk az stdout-ra :)

    /* Úgy néz ki, hogy Linux alatt a send() az igazi. Az MSG_NOSIGNAL nélkül a
    rendszer PIPE szignállal kinyírja a szervert abban az esetben, ha "Broken
    pipe" állapot lép fel, tehát a socket bezárult (kliens megszakadt) de a
    szerver még írni akar rá. Az MSG_NOSIGNAL állítólag csak Linux alatt
    létezik. BSD alatt pl. az a megoldás, hogy a setsockopt függvénnyel be kell
    állítani az adott socketen a SO_NOSIGPIPE flag-et, majd write() vagy más
    függvénnyel lehet a socketre irkálni. További probléma a netsocket
    szerkezetéből adódik, ahová mindig eljutok, amikor aszinkron rendszerben
    meghívódik egy callback és a callback-en belül van törölve az az objektum,
    ami a callback-et meghívta. Ez ugye paradoxon, avagy magad alatt vágod a
    fát, C-ben pedig csúnya segfault vagy még rosszabb. Jelen esetben itt a
    send() függvény visszatérési értékénél kellene ellenőrizni a "Broken pipe"
    esetet és ha bekövetkezik, akkor a netsocket_disconnect() függvénnyel
    megszakítani a kapcsolatot és hátrahagyni a "Broken pipe" üzenetet. Ebben az
    esetben a netsocket_disconnect meghívja a sock_close függvényt, ami törli a
    netsocket objektumot, majd visszatér ide. Ezen a ponton már a netsocket
    objektum, amivel eddig dolgoztunk, nem létezik és elképzelhető, hogy érkezik
    még egy netsocket_write függvényhívás vagy eleve mivel callback-ben vagyunk,
    más is történik a netsocket-tel, ami már végzetes egy nem létező objektumon.
    Erre kell kitalálni valami megoldást! Két ötlet van: vagy hardcore módon
    minden netsocket függvény a meghívásnál leellenőrzi, hogy a paraméterül
    átadott netsocket objektum létezik, írható, stb., és ha nem, akkor visszatér
    hibával. A másik módszer pedig, hogy aszinkron módon történik a netsocket
    objektumok törlése. A netsocket_disconnect megrendeli a törlést, és majd a
    callback lefutása után ténylegesen le is lesz törölve. Ezt az aszinkron
    megoldást már csináltam Perl-ben és jól működött, de valahogyan most ezt a
    kommentet fogalmazva, a hardcore verzió tűnik a legjobbnak, tehát: minden
    azonnal történjen meg, semmi ne blokkolódjon, ha már nem létezik az adott
    objektum, akkor hibával térjünk vissza és ne folytassuk a műveletet. Ezt
    minden olyan helyen ellenőrizni kell, ahol az objektummal munka van. Ez a
    koncepció talán a szálaknál is alkalmazható egy-egy mutex lock körzet alatt.
    Végiggondolni a szitut úgy, hogy egy callback-ből hívott netsocket_write
    hívja meg a netsocket_disconnect-et:) Praktikusan az, ami miatt ezt a
    litániát megírtam. */

    /* Ötlet. Az aszinkron megoldást végiggondolni jobban! A netsocket_destroy()
    meghívására ne törlődjön azonnal a netsocket. Ha callback-ben van éppen,
    akkor a callback után történjen a felszabadítás. Vagy dupla mutatót használni
    és ellenőrizni a NULL értéket? Faszság:) */

    // i = write(obj->sock, data, length); // nem szignál-biztos
    i = send(obj->sock, data, length, MSG_NOSIGNAL);
    return i;
}

/**
 * @brief Create a new netsocket object
 * @param callback callback function pointer
 * @param userdata user defined pointer
 * @param loop event loop
 * @returns a new netsocket object
 *
 * Create and return a malloc'ed netsocket object. Save callback and userdata
 * in the netsocket structure. Callback will be called when an event occur. Userdata
 * is an user defined pointer. If the optional loop parameter is given, then tell
 * the netsocket to use this event loop for async operations. This is useful in
 * multithreading environment. If the loop is NULL, then the default event loop
 * will be used.
 */
netsocket_t *netsocket_new (void *callback, void *userdata, struct ev_loop *loop) {
    netsocket_t *obj = malloc(sizeof(*obj));
    if (obj == NULL)
        return NULL;

    //~ printf("netsocket object size = %d\n", sizeof(*obj));
    bzero(obj, sizeof(*obj)); // mindent nullázunk

    // ha meg van adva a loop paraméter, akkor azt használjuk eseménykezelőnek
    // ellenkező esetben az alapértelmezett eseménykezelőt
    obj->loop = (loop != NULL) ? loop : ev_default_loop(0);

    // default értékek
    obj->connect_timeout = 5000; // 5000 millisec

    obj->callback = callback;
    obj->userdata = userdata;

    obj->w_in.data = obj;
    obj->w_out.data = obj;
    obj->w_connect_timeout.data = obj;
    ev_init(&obj->w_in, (void*)w_in_cb);
    ev_init(&obj->w_out, (void*)w_out_cb);
    ev_init(&obj->w_connect_timeout, (void*)w_connect_timeout_cb);

    return obj;
}

/**
 * @brief destroy the netsocket object
 * @param obj the netsocket object
 *
 * Disconnect from peer(s) without call the callback, close sockets,
 * free all resources and destroy the netsocket object.
 */
// TODO ha a szerver netsocket-et szabadítjuk fel, akkor az összes hozzá tartozó
// kliens is menjen a levesbe
void netsocket_destroy (netsocket_t *obj) {
    // ha nincs objektum, akkor lófaszjóska
    if (obj == NULL)
        return;

    if (obj->in_callback > 0) {
        obj->destroy_request = 1;
        return;
    }

    disconnect(obj, NULL, 1);
    free(obj);
}

// formázott konzol üzenet kiírása időbélyeggel
int netsocket_printf (netsocket_t *obj, const char *fmt, ...) {
    char tmp[8192];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp) - 1, fmt, ap);
    va_end(ap);

    return netsocket_write(obj, tmp, strlen(tmp));
}

void netsocket_host (netsocket_t *obj, const char *host) {
    if (host != NULL)
        strncpy(obj->host, host, sizeof(obj->host) - 1);
}

void netsocket_lhost (netsocket_t *obj, const char *lhost) {
    if (lhost != NULL)
        strncpy(obj->lhost, lhost, sizeof(obj->lhost) - 1);
}

void netsocket_port (netsocket_t *obj, int port) {
    obj->port = port;
}

void netsocket_lport (netsocket_t *obj, int lport) {
    obj->lport = lport;
}

int netsocket_is_connected (netsocket_t *obj) {
    return obj->connected;
}

