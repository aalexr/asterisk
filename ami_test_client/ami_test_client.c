
#include <stdio.h>
#include <string.h>
#include <ev.h>
#include <stdlib.h>
#include <errno.h>

#include "ami.h"
#include "debug.h"
#include "utlist.h"

#define CON_DEBUG
#include "logger.h"

#include "milenage.h"

ev_timer timer;
ev_timer timer_reconnect;

ami_t *ami;
int dstatus_n = 2;

//~ static void ami_event_callback (ami_event_t *ame) {
    //~ char *userdata = (char*)ame->userdata;
    //~ char *status = ami_getvar(ame, "Status");
    //~
    //~ char status2[64];
    //~ strncpy(status2, ami_getvar(ame, "Status"), 64);
    //~ ami_strncpy(ame, status2, "Status", 64);
//~
    //~ ami_event_unregister(ame);
//~ }

void utproba () {
    typedef struct st {
        struct st *prev;
        struct st *next;
        char str[32];
    } st;

    st *head = NULL;
    st *el;
    st *x10, *x15;

    int i;
    for (i = 0; i < 20; i++) {
        el = (st*)malloc(sizeof(*el));
        bzero(el, sizeof(*el));

        sprintf(el->str, "str %d", i);
        if (i == 10)
            x10 = el;
        if (i == 15)
            x15 = el;
        DL_APPEND(head, el);
    }

    st *eltmp;
    DL_FOREACH_SAFE(head, el, eltmp) {
        if (el == x10) {
            st *csere = (st*)malloc(sizeof(st));
            strcpy(csere->str, "csere1");
            DL_APPEND(head, csere);
            csere = (st*)malloc(sizeof(st));
            strcpy(csere->str, "csere2");
            DL_APPEND(head, csere);

            DL_DELETE(head, el);
            free(el);
        }

        if (el == x15) {
            DL_DELETE(head, el);
            free(el);
        }

    }

    DL_FOREACH_SAFE(head, el, eltmp) {
        printf("s %s s\n", el->str);
        DL_DELETE(head, el);
        free(el);
    }

    return;
}

int do_register = 0;

void event_response (ami_event_t *event) {
	const char *message_str = ami_getvar(event, "Message");

	printf("response: %s\n", message_str);
}

void event_fullybooted (ami_event_t *event) {
    printf("*** Fully booted ***\n");
    if (do_register)
	    return;
    do_register = 1;
   ami_action(ami, event_response, NULL, "Action: PJSIPRegister\nRegistration: volte_ims");
   ami_action(ami, event_response, NULL, "Action: PJSIPAccessNetworkInfo\nRegistration: volte_ims\nInfo: hello");
}

int hex_to_octet_string(const char *name, const char *input, uint8_t *output, size_t output_size)
{
        int i, n;

        if (!input || !input[0]) {
                printf("Missing value for hex string '%s'.\n", name);
                return -1;
        }
        i = n = 0;
        while (*input) {
                if (i == output_size) {
                        printf("Value for hex string '%s' too long, expecting %zu bytes.\n", name,
                                output_size);
                        return -1;
                }
                if (*input >= '0' && *input <= '9')
                        output[i] = (output[i] << 4) | (*input - '0');
                else if (*input >= 'a' && *input <= 'f')
                        output[i] = (output[i] << 4) | (*input - 'a' + 10);
                else if (*input >= 'A' && *input <= 'F')
                        output[i] = (output[i] << 4) | (*input - 'A' + 10);
                else {
                        printf("Value for hex string '%s' has invalid character '%c'.\n", name, *input);
                        return -1;
                }
                if (++n == 2) {
                        n = 0;
                        i++;
                }
                input++;
        }
        if (i < output_size) {
                printf("Value for hex string '%s' too short, expecting %zu bytes.\n", name, output_size);
                return -1;
        }

        return 0;
}

void octest_string_to_hex(uint8_t *input, size_t input_size, char *output)
{
	while(input_size) {
		sprintf(output, "%02x", *input);
		input++;
		input_size--;
		output += 2;
	}
}

static const char *usim_opc = "775A1F887D2AD66F9719C2C79F847B50";
static const char *usim_k = "D534E07854B75E475C667A856AA31F9C";
static const char *usim_sqn = "000000011000";

void event_authrequest (ami_event_t *event) {
	const char *rand_str, *autn_str;
	uint8_t opc[16], k[16], sqn[6], rand[16], autn[16], res[8], ik[16], ck[16], auts[14];
	char res_str[256], ik_str[256], ck_str[256], auts_str[256];
	size_t res_len = 8;
	rand_str = ami_getvar(event, "RAND");
	autn_str = ami_getvar(event, "AUTN");
	int rc;

	if (!rand_str || !autn_str) {
    		printf("Missing parameters in AuthRequest\n");
		return;
	}

	if (hex_to_octet_string("RAND", rand_str, rand, sizeof(rand))) {
    		printf("Invalid RAND parameter in AuthRequest\n");
		return;
	}
	if (hex_to_octet_string("AUTN", autn_str, autn, sizeof(autn))) {
    		printf("Invalid AUTN parameter in AuthRequest\n");
		return;
	}
	if (hex_to_octet_string("OPC", usim_opc, opc, sizeof(opc))) {
    		printf("Invalid OPC parameter in AuthRequest\n");
		return;
	}
	if (hex_to_octet_string("K", usim_k, k, sizeof(k))) {
    		printf("Invalid K parameter in AuthRequest\n");
		return;
	}
	if (hex_to_octet_string("SQN", usim_sqn, sqn, sizeof(sqn))) {
    		printf("Invalid SQN parameter in AuthRequest\n");
		return;
	}
	rc = milenage_check(opc, k, sqn, rand, autn, ik, ck, res, &res_len, auts);
	octest_string_to_hex(res, sizeof(res), res_str);
	octest_string_to_hex(ik, sizeof(ik), ik_str);
	octest_string_to_hex(ck, sizeof(ck), ck_str);
	octest_string_to_hex(auts, sizeof(auts), auts_str);
	if (rc == -2) {
    		printf("*** Sending AuthResponse with authentication token ***\n");
    		ami_action(ami, NULL, NULL, "Action: AuthResponse\nRegistration: volte_ims\nAUTS: %s", auts_str);
	} else if (!rc) {
    		printf("*** Sending AuthResponse with authentication result and keys ***\n");
    		ami_action(ami, NULL, NULL, "Action: AuthResponse\nRegistration: volte_ims\nRES: %s\nCK: %s\nIK: %s", res_str, ck_str, ik_str);
	} else {
    		printf("*** Sending AuthResponse with authentication error\n");
    		ami_action(ami, NULL, NULL, "Action: AuthResponse\nRegistration: volte_ims");
	}
		
}

void reconnect_ami (ami_event_t *event) {
    printf("*** Reconnecting... ***\n");
    ev_timer_stop(EV_DEFAULT, &timer_reconnect);
    ami_connect(ami);
}

void event_connect (ami_event_t *event) {
    printf("*** CONNECTED ***\n");
    printf("  semmi   = %s\n  host    = %s\n  ip      = %s\n  port    = %s\n",
        ami_getvar(event, "semmi"),
        ami_getvar(event, "Host"),
        ami_getvar(event, "IP"),
        ami_getvar(event, "Port"));
}

void event_disconnect (ami_event_t *event) {
    printf("*** DISCONNECTED ***\n");
    printf("  reason  = %s\n  host    = %s\n  ip      = %s\n  port    = %s\n",
        ami_getvar(event, "Reason"),
        ami_getvar(event, "Host"),
        ami_getvar(event, "IP"),
        ami_getvar(event, "Port"));
    ev_timer_again(EV_DEFAULT, &timer_reconnect);
    ev_timer_start(EV_DEFAULT, &timer_reconnect);
}

static int registered=0;

void event_registry (ami_event_t *event) {
	const char *status_str;
	status_str = ami_getvar(event, "Status");

	printf("registry-status=%s\n", status_str);

	if (!strcasecmp(status_str, "Registered")) {
		if (!registered) {
			registered = 1;
			ami_action(ami, event_response, NULL, "Action: PJSIPUnregister\nRegistration: volte_ims");
		}
	}
}

int main (int argc, char *argv[]) {
    ami = ami_new(EV_DEFAULT);
    if (ami == NULL) {
        con_debug("ami_new() returned NULL");
        return 1;
    }

    char host[128];

    if (argc < 2) {
        //~ printf("usage: %s <host>\n", argv[0]);
        printf("Default host used!\n");
        strcpy(host, "127.0.0.1");
    } else {
        strcpy(host, argv[1]);
    }

    printf("Connecting to %s ...\n", host);
    ami_credentials(ami, "jolly", "svenja", host, "5038");
    ami_connect(ami);

    ami_event_register(ami, event_fullybooted, NULL, "Event: FullyBooted");

    ev_timer_init(&timer_reconnect, (void*)reconnect_ami, 3, 3);
    //~ ev_timer_start(EV_DEFAULT, &timer_reconnect);

    ami_event_register(ami, event_connect, NULL, "Connect");
    ami_event_register(ami, event_disconnect, NULL, "Disconnect");

    ami_event_register(ami, event_authrequest, NULL, "Event: AuthRequest");

    ami_event_register(ami, event_registry, NULL, "Event: Registry");

    printf("\n");
    ami_dump_lists(ami);

    ami_action(ami, NULL, NULL, "Action: Login\nUsername: jolly\nSecret: svenja");

    ev_loop(EV_DEFAULT, 0);

    ami_destroy(ami);

    return 0;
}



