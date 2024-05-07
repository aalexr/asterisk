/* pipe.c
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ev.h>
#include <string.h>

#include "netsocket.h"

static char *lhost; // = "0.0.0.0";
static int lport; // = 1111;

static char *host; // = "irc.jss.hu";
static int port; // = 6667;

//~ static int i = 0;
int n = 0;

//~ static netsocket_t *cnetsocket; // client
static netsocket_t *snetsocket; // server

static ev_timer halfsec_timer;
//~ static ev_timer reconnect_timer;


static void halfsec_timeout (EV_P_ ev_timer *w, int revents) {
    //~ if (i == 2) netsocket_disconnect(netsocket, "ötödik másodperc miatt");
    //~ if (i == 7) netsocket_connect(netsocket, "pingoly", 5555);
    //~ printf("timeout %d\n", n++);
}

//~ static void reconnect_timeout (EV_P_ ev_timer *w, int revents) {
    //~ ev_timer_stop(EV_DEFAULT, &reconnect_timer);
    //~ printf("Connecting [#%d] to %s:%d...\n", i++, host, port);
    //~ netsocket_connect(cnetsocket, host, port);
//~ }

static void ccallback (netsocket_t *netsocket, int event) {
    //~ printf("Callback event = %d - %s\n", event, netsocket->disconnect_reason);
    
    switch (event) {
        case NETSOCKET_EVENT_CONNECT:
            printf("Connected to %s (%s) port %d\n",
                netsocket->host,
                netsocket->ip,
                netsocket->port
            );
            //~ char *msg = "Szevasz szerver!\n";
            //~ netsocket_write(netsocket, msg, strlen(msg));
            break;
            
        case NETSOCKET_EVENT_DISCONNECT:
            //~ ev_timer_again(EV_DEFAULT, &reconnect_timer);
            //~ ev_timer_start(EV_DEFAULT, &reconnect_timer);
            if (netsocket->connected) {
                printf("Disconnected from %s: %s\n",
                    netsocket->host,
                    netsocket->disconnect_reason
                );
            } else {
                printf("Can't connect to %s: %s\n",
                    netsocket->host,
                    netsocket->disconnect_reason
                );
            }
            netsocket_destroy(netsocket->userdata);
            netsocket_destroy(netsocket);
            break;
            
        case NETSOCKET_EVENT_READ:
            printf("Got %d bytes from %s:%d\n",
                netsocket->inbuf_len,
                netsocket->host,
                netsocket->port
            );
            //~ printf("inbuf = %s\n", netsocket->inbuf);
            netsocket_write(netsocket->userdata, netsocket->inbuf, netsocket->inbuf_len);
            break;
            
        default:
            printf("Unhandled event: %d\n", event);
            break;
    }
    
    //~ return;
    //~ printf("Callback jott: %s\nHost: %s\nIP: %s\nErr: %s\n",
        //~ (char*)netsocket->userdata, netsocket->host, netsocket->ip, strerror(netsocket->err));
}

static void scallback (netsocket_t *netsocket, int event) {
    //~ printf("Callback event = %d - %s\n", event, netsocket->disconnect_reason);
    
    switch (event) {
        case NETSOCKET_EVENT_CONNECT:
            // a netsocket az új kliens socket
            // a szülőt, tehát azt a netsocket objektumot, ami a szerver
            // és figyel a porton, azt a netsocket->parent lehet elérni
            // a netsocket->userdata megegyezik a szülő userdata-val
            printf("Connect from %s:%d (%s)\n",
                netsocket->ip,
                netsocket->port,
                netsocket->host
            );
            //~ char *msg = "Szevasz kliens!\n";
            //~ netsocket_write(netsocket, msg, strlen(msg));
            
            netsocket_t *newnetsocket;
            newnetsocket = netsocket_new(ccallback, netsocket, NULL);
            netsocket_host(newnetsocket, host);
            netsocket_port(newnetsocket, port);
            netsocket->userdata = newnetsocket;
            netsocket_connect(netsocket->userdata);
            //~ netsocket_disconnect(netsocket, "megszakadsz");
            //~ netsocket_destroy(netsocket);
            break;
            
        case NETSOCKET_EVENT_DISCONNECT:
            //~ ev_timer_again(EV_DEFAULT, &reconnect_timer);
            //~ ev_timer_start(EV_DEFAULT, &reconnect_timer);
            if (netsocket->connected) {
                printf("Disconnected from %s: %s\n",
                    netsocket->host,
                    netsocket->disconnect_reason
                );
                netsocket_destroy(netsocket->userdata);
                netsocket_destroy(netsocket);
            } else {
                printf("Can't listen on %s:%d: %s\n",
                    netsocket->host,
                    netsocket->port,
                    netsocket->disconnect_reason
                );
                exit(255);
            }
            break;

        case NETSOCKET_EVENT_READ:
            printf("Got %d bytes from %s:%d\n",
                netsocket->inbuf_len,
                netsocket->host,
                netsocket->port
            );
            //~ printf("inbuf = %s\n", netsocket->inbuf);
            netsocket_write(netsocket->userdata, netsocket->inbuf, netsocket->inbuf_len);
            break;
            
        default:
            printf("Unhandled event: %d\n", event);
            break;
    }
    
    //~ return;
    //~ printf("Callback jott: %s\nHost: %s\nIP: %s\nErr: %s\n",
        //~ (char*)netsocket->userdata, netsocket->host, netsocket->ip, strerror(netsocket->err));
}

int main (int argc, char **argv) {


    if (argc < 5) {
        printf("usage: %s <local host> <local port> <remote host> <remote port>\n", argv[0]);
        exit(255);
    }
    
    lhost = argv[1];
    lport = atoi(argv[2]);  
    host = argv[3];
    port = atoi(argv[4]);   
    
    //~ cnetsocket = netsocket_new(ccallback, "juzerdata");
    //~ netsocket_connect(netsocket, "pingoly", 5555);
    //~ netsocket_destroy(netsocket);

    snetsocket = netsocket_new(scallback, "ez itt a szerver", EV_DEFAULT);
    netsocket_lhost(snetsocket, lhost);
    netsocket_lport(snetsocket, lport);
    if (netsocket_listen(snetsocket)) {
        perror("netsocket_listen");
        exit(255);
    }

    ev_timer_init(&halfsec_timer, halfsec_timeout, 0.5, 0.5);
    ev_timer_start(EV_DEFAULT, &halfsec_timer);

    // azonnali reconnect, majd disconnect esetén 1.5 másodperc múlva
    //~ ev_timer_init(&reconnect_timer, reconnect_timeout, 0., 1.5);
    //~ ev_timer_start(EV_DEFAULT, &reconnect_timer);

    ev_loop(EV_DEFAULT, 0);
    
    return 0;
}

