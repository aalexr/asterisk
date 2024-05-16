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

/* Add a value to an array header. If the header does not exist, it is created. */
static pj_status_t add_value_array_hdr(pjsip_tx_data *tdata, const pj_str_t *name, const pj_str_t *value)
{
	pjsip_generic_array_hdr *hdr;
	pj_bool_t created = PJ_FALSE;

	/* Create header, if not yet created. */
	hdr = pjsip_msg_find_hdr_by_name(tdata->msg, name, NULL);
	if (!hdr) {
		hdr = pjsip_generic_array_hdr_create(tdata->pool, name);
		created = PJ_TRUE;
	}
	if (!hdr) {
		ast_log(LOG_ERROR, "Failed to create array header.");
		return -ENOMEM;
	}
	if (hdr->count == PJSIP_GENERIC_ARRAY_MAX_COUNT) {
		ast_log(LOG_ERROR, "Too many evalue in array, skipping %s.", value->ptr);
		return -E2BIG;
	}
	pj_strdup(tdata->pool, &hdr->values[hdr->count++], value);

	/* Append header, if created. */
	if (created)
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
	pj_status_t status;

	/* "Require: sec-agree" */
	status = add_value_array_hdr(tdata, &STR_REQUIRE, &STR_SEC_AGREE);
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
