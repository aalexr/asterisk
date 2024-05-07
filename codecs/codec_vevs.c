/*
 * Asterisk -- An open source telephony toolkit.
 */

/*! \file
 *
 * \brief Translate between signed linear and Vocal EVS codec
 *
 * \ingroup codecs
 */

/*** MODULEINFO
	<depend>vevs</depend>
 ***/

#include "asterisk.h"

#include "asterisk/translate.h"
#include "asterisk/config.h"
#include "asterisk/module.h"
#include "asterisk/utils.h"
#include "asterisk/linkedlists.h"

#include <stdbool.h>
#define uint8 uint8_t
#define uint16 uint16_t
#define uint32 uint32_t
#define vbool bool
#define sint15 int16_t
#include "vocal-evs/evs.h"

#define BUFFER_SAMPLES	8000		/* 0.5 seconds @ 16 kHz */
#define BUFFER_FRAMES	8192		/* Enough for about 0.5 seconds @ 128 Kbps */

/* Sample frame data */

#include "asterisk/slin.h"
#include "ex_vevs.h"

/* Encoder and decoder instances */

struct evs_enc_pvt {
	evs_enc_ctx_t enc;		/* Encoder states */
	int chunk;			/* Size of chunk to encode (in samples) */
	int16_t buf[BUFFER_SAMPLES];	/* Buffer to store received uncompressed audio */
};

struct evs_dec_pvt {
	evs_dec_ctx_t dec;		/* Decoder states */
	int chunk;			/* Size of chunk to decode (in samples) */
};

#warning hacking
int vocal_evs_init_coder (evs_enc_ctx_t *ctx, uint16 bandwidth, uint16 sampling_frequency, uint32 bitrate, vbool dtx_enable) { return 320; }
int vocal_evs_process_coder (evs_enc_ctx_t *ctx, uint8 *enc_data, uint16 enc_data_size, const sint15 *enc_samples, uint16 sample_count) { return 320; }
int vocal_evs_close_coder (evs_enc_ctx_t *ctx) { return 0; }
int vocal_evs_init_decoder (evs_dec_ctx_t *ctx, uint16 sampling_frequency) { return 320; }
int vocal_evs_process_decoder (evs_dec_ctx_t *ctx, sint15 *dec_samples, uint16 max_sample_count, uint8 *dec_data, uint16 dec_data_size) { return 320; }
int vocal_evs_close_decoder (evs_dec_ctx_t *ctx) { return 0; }


/* Create and destroy codec intances */

static int evs_enc_new(struct ast_trans_pvt *pvt)
{
	struct evs_enc_pvt *enc = pvt->pvt;
	const unsigned int sample_rate = pvt->t->src_codec.sample_rate;
	int rc;

	memset(enc, 0, sizeof(*enc));
	rc = vocal_evs_init_coder(&enc->enc, EVS_BW_NB, EVS_SF_16K, EVS_BR_5K90, false);
	if (rc <= 0) {
		ast_log(LOG_ERROR, "Error creating the Vocal EVS encoder\n");
		return -1;
	}
	enc->chunk = rc;

	ast_debug(3, "Created encoder (Vocal EVS) with sample rate %d\n", sample_rate);

	return 0;
}

static void evs_enc_destroy(struct ast_trans_pvt *pvt)
{
	struct evs_enc_pvt *enc = pvt->pvt;

	vocal_evs_close_coder(&enc->enc);

	ast_debug(3, "Destroyed encoder (Vocal EVS)\n");
}

static int evs_dec_new(struct ast_trans_pvt *pvt)
{
	struct evs_dec_pvt *dec = pvt->pvt;
	const unsigned int sample_rate = pvt->t->src_codec.sample_rate;
	int rc;

	memset(dec, 0, sizeof(*dec));
	rc = vocal_evs_init_decoder(&dec->dec, EVS_SF_16K);
	if (rc <= 0) {
		ast_log(LOG_ERROR, "Error creating the Vocal EVS decoder\n");
		return -1;
	}
	dec->chunk = rc;

	ast_debug(3, "Created decoder (Vocal EVS) with sample rate %d\n", sample_rate);

	return 0;
}

static void evs_dec_destroy(struct ast_trans_pvt *pvt)
{
	struct evs_dec_pvt *dec = pvt->pvt;

	vocal_evs_close_decoder(&dec->dec);

	ast_debug(3, "Destroyed encoder (Vocal EVS)\n");
}

/* Encoder */

static int lintoevs_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct evs_enc_pvt *enc = pvt->pvt;

	/* XXX We should look at how old the rest of our stream is, and if it
	   is too old, then we should overwrite it entirely, otherwise we can
	   get artifacts of earlier talk that do not belong */
	if (pvt->samples + f->samples > BUFFER_SAMPLES) {
		ast_log(LOG_WARNING, "Out of buffer space\n");
		return -1;
	}
	memcpy(enc->buf + pvt->samples, f->data.ptr, f->datalen);
	pvt->samples += f->samples;
	return 0;
}

static struct ast_frame *lintoevs_frameout(struct ast_trans_pvt *pvt)
{
	struct evs_enc_pvt *enc = pvt->pvt;
	struct ast_frame *result = NULL;
	struct ast_frame *last = NULL;
	int samples = 0; /* output samples */

	while (pvt->samples >= enc->chunk) {
		struct ast_frame *current;
		int rc;

		rc = vocal_evs_process_coder(&enc->enc, pvt->outbuf.uc, BUFFER_FRAMES, enc->buf, enc->chunk);
		if (rc <= 0) {
			ast_log(LOG_ERROR, "Error encoding with Vocal EVS encoder\n");
			pvt->samples = 0;
			return NULL;
		}
		samples += enc->chunk;
		pvt->samples -= enc->chunk;
		current = ast_trans_frameout(pvt, rc, enc->chunk);
		if (!current) {
			continue;
		} else if (last) {
			/* Append frame */
			AST_LIST_NEXT(last, frame_list) = current;
		} else {
			/* Return first frame */
			result = current;
		}
		last = current;
	}

	/* Move the data at the end of the buffer to the front */
	if (samples) {
		memmove(enc->buf, enc->buf + samples, pvt->samples * sizeof(int16_t));
	}

	return result;
}

/* Decoder */

static int evstolin_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct evs_dec_pvt *dec = pvt->pvt;
	int16_t *dst = pvt->outbuf.i16;
	int rc;

	if (BUFFER_SAMPLES - pvt->samples < dec->chunk) {
		ast_log(LOG_WARNING, "Out of buffer space\n");
		return -1;
	}

	if (f->datalen == 0) {
		ast_log(LOG_DEBUG, "Conceal missing EVS frame.\n");
		/* The documentation does not give a hint on how bad frames indicated and if they are concealed at all. This may not work!*/
		rc = vocal_evs_process_decoder(&dec->dec, dst + pvt->samples, BUFFER_SAMPLES - pvt->samples, NULL, 0);
		if (rc <= 0) {
			ast_log(LOG_ERROR, "Error concealing missing EVS frame with Vocal EVS decoder\n");
			return -1;
		}
	} else {
		rc = vocal_evs_process_decoder(&dec->dec, dst + pvt->samples, BUFFER_SAMPLES - pvt->samples, f->data.ptr, f->datalen);
		if (rc <= 0) {
			ast_log(LOG_ERROR, "Error decoding with Vocal EVS decoder\n");
			return -1;
		}
	}
	pvt->samples += rc;
	pvt->datalen += rc * sizeof(int16_t);

	return 0;
}

static struct ast_translator lintoevs = {
	.name = "lintovevs",
	.src_codec = {
		.name = "slin",
		.type = AST_MEDIA_TYPE_AUDIO,
		.sample_rate = 16000,
	},
	.dst_codec = {
		.name = "vevs",
		.type = AST_MEDIA_TYPE_AUDIO,
		.sample_rate = 16000,
	},
	.format = "vevs",
	.newpvt = evs_enc_new,
	.framein = lintoevs_framein,
	.frameout = lintoevs_frameout,
	.destroy = evs_enc_destroy,
	.sample = slin16_sample,
	.buf_size = BUFFER_FRAMES,
	.desc_size = sizeof (struct evs_enc_pvt ),
};

static struct ast_translator evstolin = {
	.name = "vevstolin",
	.src_codec = {
		.name = "vevs",
		.type = AST_MEDIA_TYPE_AUDIO,
		.sample_rate = 16000,
	},
	.dst_codec = {
		.name = "slin",
		.type = AST_MEDIA_TYPE_AUDIO,
		.sample_rate = 16000,
	},
	.format = "slin",
	.newpvt = evs_dec_new,
	.framein = evstolin_framein,
	.destroy = evs_dec_destroy,
	.sample = vevs_sample,
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
	.desc_size = sizeof (struct evs_dec_pvt ),
};

static int unload_module(void)
{
	int res;

	res = ast_unregister_translator(&lintoevs);
	res |= ast_unregister_translator(&evstolin);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_register_translator(&evstolin);
	res |= ast_register_translator(&lintoevs);

	if (res) {
		unload_module();
		return AST_MODULE_LOAD_DECLINE;
	}

	return AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Vocal EVS Coder/Decoder",
	.support_level = AST_MODULE_SUPPORT_CORE,
	.load = load_module,
	.unload = unload_module,
);
