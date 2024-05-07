/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include <ctype.h>                      /* for tolower */

#include "asterisk/module.h"
#include "asterisk/format.h"
#include "asterisk/logger.h"            /* for ast_log, LOG_WARNING */
#include "asterisk/strings.h"           /* for ast_str_append */
#include "asterisk/utils.h"             /* for MAX, MIN */

struct evs_attr {
	int dummy;
};

static void evs_destroy(struct ast_format *format)
{
	struct evs_attr *attr = ast_format_get_attribute_data(format);

	ast_free(attr);
}

static void attr_init(struct evs_attr *attr)
{
	memset(attr, 0, sizeof(*attr));
}

static int evs_clone(const struct ast_format *src, struct ast_format *dst)
{
	struct evs_attr *original = ast_format_get_attribute_data(src);
	struct evs_attr *attr = ast_malloc(sizeof(*attr));

#warning hacking
abort();
	if (!attr) {
		return -1;
	}

	if (original) {
		*attr = *original;
	} else {
		attr_init(attr);
	}

	ast_format_set_attribute_data(dst, attr);

	return 0;
}

static struct ast_format *evs_parse_sdp_fmtp(const struct ast_format *format, const char *attributes)
{
	char *attribs = ast_strdupa(attributes), *attrib;
	struct ast_format *cloned;
	struct evs_attr *attr;
//	unsigned int val;

	cloned = ast_format_clone(format);
	if (!cloned) {
		return NULL;
	}
	attr = ast_format_get_attribute_data(cloned);

	/* lower-case everything, so we are case-insensitive */
	for (attrib = attribs; *attrib; ++attrib) {
		*attrib = tolower(*attrib);
	} /* based on channels/chan_sip.c:process_a_sdp_image() */

	ast_log(LOG_WARNING, "Unhandled received attribute '%s', please fix!\n", attrib);

	return cloned;
}

static void evs_generate_sdp_fmtp(const struct ast_format *format, unsigned int payload, struct ast_str **str)
{
	struct evs_attr *attr = ast_format_get_attribute_data(format);

#warning hacking
//	if (!attr) {
//		return;
//	}

	ast_str_append(str, 0, "a=fmtp:%u br=5.9; bw=nb-wb\r\n", payload);
}

static enum ast_format_cmp_res evs_cmp(const struct ast_format *format1, const struct ast_format *format2)
{
	if (ast_format_get_sample_rate(format1) == ast_format_get_sample_rate(format2)) {
		return AST_FORMAT_CMP_EQUAL;
	}

	return AST_FORMAT_CMP_NOT_EQUAL;
}

static struct ast_format *evs_getjoint(const struct ast_format *format1, const struct ast_format *format2)
{
	struct evs_attr *attr1 = ast_format_get_attribute_data(format1);
	struct evs_attr *attr2 = ast_format_get_attribute_data(format2);
	struct ast_format *jointformat;
	struct evs_attr *attr_res;

	if (ast_format_get_sample_rate(format1) != ast_format_get_sample_rate(format2)) {
		return NULL;
	}

	jointformat = ast_format_clone(format1);
	if (!jointformat) {
		return NULL;
	}
	attr_res = ast_format_get_attribute_data(jointformat);

	if (!attr1 || !attr2) {
		attr_init(attr_res);
	} else {
		/* Take the lowest max bitrate */
//		attr_res->maxbitrate = MIN(attr1->maxbitrate, attr2->maxbitrate);
	}

	return jointformat;
}

static struct ast_format_interface evs_interface = {
	.format_destroy = evs_destroy,
	.format_clone = evs_clone,
	.format_cmp = evs_cmp,
	.format_get_joint = evs_getjoint,
	.format_parse_sdp_fmtp = evs_parse_sdp_fmtp,
	.format_generate_sdp_fmtp = evs_generate_sdp_fmtp,
};

static int load_module(void)
{
	if (ast_format_interface_register("vevs", &evs_interface)) {
		return AST_MODULE_LOAD_DECLINE;
	}

	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "Vocal EVS Format Attribute Module",
	.support_level = AST_MODULE_SUPPORT_CORE,
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_CHANNEL_DEPEND,
);
