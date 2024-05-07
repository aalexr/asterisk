/* test_htclient.c
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "htclient.h"
#include "debug.h"
#include "url_parser.h"

void url_test () {
    htclient_t *h = htclient_new();

    htclient_url(h, "http://10.42.20.14:3000/akarmi.jpg");
    htclient_dump(h);

    htclient_destroy(h);
}

int main (int argc, const char *argv[]) {
    // url_test(); return(0);

    htclient_t *h;
    h = htclient_new();
    if (h == NULL) {
        printf("out of memory\n");
        return -1;
    }

    htclient_url(h, "http://10.42.20.14:3000/dovecheck");
    htclient_request_set(h,
        "GET /auth\r\n"
        "Auth-server: akarmi\r\n"
        "Auth-port: %d\r\n"
        "\r\n"
        , 5555
    );
    htclient_perform(h);

htclient_dump(h);
    if (htclient_error(h)) {
        printf("error: %s\n", htclient_error(h));
        goto err;
    }
    
    char *hdr = htclient_header_get(h, "Auth-server");
    printf("Header vissza: %s\n", hdr);

    printf("len = %s\n", htclient_header_get(h, "Content-Length"));

    htclient_destroy(h);
    return 0;

err:
    htclient_destroy(h);
    return -1;
}

