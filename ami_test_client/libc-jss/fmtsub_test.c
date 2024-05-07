/* fmtsub_test.c
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

#include "misc.h"

/*
const char *args[][2] = {
    {"%n", "2463434"},
    {"%v", "0.9.6-rc2"},
    {"%n", "Jekusa Poromeck"},
    {0}
}; 
*/
//~ void prob (const char *args[][2]) {
    //~ int i;
    //~ for (i = 0; afmt[i][0] != 0; i++) {
        //~ printf("hello %s --- %s\n", afmt[i][0], afmt[i][1]);
        //~ 
    //~ }
//~ }

int main (int argc, char **argv) {
    char buf2[32];
    char buf[64];
    
    char *pattern = argv[1];

    char proba[64];
    strcpy(proba, "PROBA");

    const char *args[][2] = {
        {"%n", NULL},
        {"", "URES_STRING"},
        {"%v", "0.9.6-rc2"},
        {"%a", "Jekusa Poromeck"},
        {"%p", proba},
        {"Windows", "Linux"},
        {"\\%", "%"},
        {0}
    };

    if (fmtsub(buf, sizeof(buf), pattern, args)) {
        printf("fmtsub() hibaval tert vissza!\n");
    }

    printf("%d s %s s\n", (int)strlen(buf), buf);

    strncpy(buf2, buf, sizeof(buf2)-1);
    printf("%d s %s s\n", (int)strlen(buf2), buf2);
    
    return 0;
}
