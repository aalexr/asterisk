/* logger_test.c
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#define CON_DEBUG

#include "logger.h"
#include <stdio.h>
#include <errno.h>

int main () {
    
    con_logfile("logger_test.log");
    con_timestamp_format("%a %H:%M:%S");
    
    conft("zsiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiir");
    conft("yeahhhh");
    conft("ok");
    
    FILE *f;
    if (!(f = fopen("/proc/ioports", "r"))) {
        perror("fopen");
        return 1;
    }
    
    char tmp[128];
    int i = 0;
    while (fgets(tmp, sizeof(tmp), f)) {
        con_debug("i=%d %s", i, tmp);
        i++;        
    }
    


    fclose(f);
    
    return 0;
}

