/*
 * Asterisk -- An open source telephony toolkit.
 *
 * (C) 2024 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 *
 * Author: Andreas Eversberg
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#include "asterisk.h"
#include "asterisk/utils.h"
#include "asterisk/manager.h"
#include "asterisk/res_pjsip.h"

#include "volte.h"

const pj_str_t STR_SECURITY_VERIFY = { "Security-Verify", 15 };
const pj_str_t STR_REQUIRE = { "Require", 7 };
const pj_str_t STR_PROXY_REQUIRE = { "Proxy-Require", 13 };
const pj_str_t STR_SEC_AGREE = { "sec-agree", 9 };
const pj_str_t STR_P_ACCESS_NETWORK_INFO = { "P-Access-Network-Info", 21 };

/* Create string header and add given value. */
static pj_status_t add_value_string_hdr(pjsip_tx_data *tdata, const pj_str_t *name, const pj_str_t *value)
{
	pjsip_generic_string_hdr *hdr;

	/* Add header. */
	hdr = pjsip_generic_string_hdr_create(tdata->pool, name, value);
	if (!hdr) {
		ast_log(LOG_ERROR, "Failed to create string header.");
		return -ENOMEM;
	}

	/* Append header */
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr *)hdr);

	return PJ_SUCCESS;
}

pj_status_t volte_add_security_verify(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata)
{
	const pj_str_t security = { transport_state->volte.security_server,
				    strlen(transport_state->volte.security_server)};
	pj_status_t status;

	status = add_value_string_hdr(tdata, &STR_SECURITY_VERIFY, &security);
	if (status)
		return status;

	return PJ_SUCCESS;
}

/* Add Sec-Agree to header. */
pj_status_t volte_add_sec_agree(pjsip_tx_data *tdata)
{
	pj_bool_t created = PJ_FALSE;
	pjsip_require_hdr *hdr;
	pj_status_t status;

	/* "Require: sec-agree" */
	status = PJ_SUCCESS;
	hdr = pjsip_msg_find_hdr_by_name(tdata->msg, &STR_REQUIRE, NULL);
	if (!hdr) {
		hdr = pjsip_require_hdr_create(tdata->pool);
		if (hdr)
			created = PJ_TRUE;
		else
			status = PJ_ENOMEM;
	}
	if (hdr) {
		hdr->values[hdr->count++] = STR_SEC_AGREE;
	}
	if (created)
		pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);
	if (status)
		return status;

	/* "Proxy-Require: sec-agree" */
	status = add_value_string_hdr(tdata, &STR_PROXY_REQUIRE, &STR_SEC_AGREE);
	if (status)
		return status;

	return PJ_SUCCESS;
}

/* Add P-Access-Network-Info to header. */
pj_status_t volte_add_p_access_network_info(pjsip_tx_data *tdata, char *info)
{
	pj_status_t status;

	const pj_str_t info_str = { info, strlen(info) };

	/* "P-Access-Network-Info" */
	status = add_value_string_hdr(tdata, &STR_P_ACCESS_NETWORK_INFO, &info_str);
	if (status)
		return status;

	return PJ_SUCCESS;
}

static const char *volte_invite_contact_params[] = {
	"+g.3gpp.icsi-ref",
	"\"urn%3Aurn-7%3A3gpp-service.ims.icsi.mmtel\"",
	"audio",
	"",
	"+g.3gpp.mid-call",
	"",
	"+g.3gpp.srvcc-alerting",
	"",
	"+g.3gpp.ps2cs-srvcc-orig-pre-alerting",
	"",
	NULL
};

static const char *volte_update_contact_params[] = {
	"+g.3gpp.icsi-ref",
	"\"urn%3Aurn-7%3A3gpp-service.ims.icsi.mmtel\"",
	"audio",
	"",
	NULL
};

void volte_add_contact_params(pjsip_tx_data *tdata, pj_bool_t update)
{
	pjsip_contact_hdr *contact;
	const char **params;
	pjsip_param *p;
	pj_str_t name, value;

	contact = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, NULL);
	if (!contact) {
		ast_log(LOG_ERROR, "There is no Contact header, please fix!\n");
		return;
	}

	if (!update)
		params = volte_invite_contact_params;
	else
		params = volte_update_contact_params;

	while (*params) {
		p = PJ_POOL_ALLOC_T(tdata->pool, pjsip_param);
		if (!p) {
			ast_log(LOG_ERROR, "No memory\n");
			return;
		}
		pj_cstr(&name, *params++);
		pj_cstr(&value, *params++);
		p->name = name;
		p->value = value;
		pj_list_insert_before(&contact->other_param, p);
	}
}
