/* test_tracer.c
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

#include "tracer.h"
#include "debug.h"

int main (int argc, const char *argv[]) {
    tracer_t *tra = tracer_new("proba");
    if (tra == NULL) {
        printf("out of memory\n");
        return -1;
    }

    tracer_open(tra);
    tracer_printf(tra, "hello %d\n", 59);
    tracer_printf(tra, "hallo %d\n", 61);
    printf("lezárva: %llu\n", tracer_close(tra));

    tracer_destroy(tra);
    return -1;
}

