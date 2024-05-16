#pragma once

pj_status_t volte_add_sec_agree(pjsip_tx_data *tdata);
pj_status_t volte_add_security_verify(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata);
pj_status_t volte_add_p_access_network_info(pjsip_tx_data *tdata, char *info);
