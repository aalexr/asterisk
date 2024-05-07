/* test_amysql.c
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ev.h>

#include "amysql.h"
#include "debug.h"

static struct ev_loop *loop;
static ev_timer timeout_watcher;

static void cbtest (int fc, char *fv[], char *e);

static void q () {
    amysql_query(cbtest, "select * from smsout where sent='0' order by RAND()");
}

static void cbtest (int fc, char *fv[], char *e) {
    debi(fc);
    printf("e=%s\n", e);
    int i;
    for (i = 0; i < fc; i++) {
        printf("%d --- %s\n", i, fv[i]);
    }

    // 3 másodperc múlva q() futtatása
    ev_once(loop, -1, -1, 3, q, 0);
}

static void timeout_cb (EV_P_ ev_timer *w, int revents) {
    printf("timeout\n");
}

int main (int argc, char *argv[]) {
    loop = ev_default_loop (0);

    ev_timer_init (&timeout_watcher, timeout_cb, 0, 0.5);
    ev_timer_start (loop, &timeout_watcher);

    amysql_init(loop);

    q(); // berúgjuk a mocit:)

    ev_loop (loop, 0);

    return 0;
}

