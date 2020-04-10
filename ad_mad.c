/*
 * Copyright (C) 2018-2020 Alibaba Group Holding Limited
 */

#if defined(CONFIG_DECODER_MAD) && CONFIG_DECODER_MAD

#include "avutil/common.h"
#include "avcodec/ad_cls.h"
#include "mad.h"

#define TAG                    "ad_mad"

#define INBUF_SIZE_MAX         (1024*8)

struct ad_mad_priv {
    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;
    unsigned char input_buffer[INBUF_SIZE_MAX + MAD_BUFFER_GUARD];
};

static void _init_mad(struct ad_mad_priv *priv)
{
    mad_stream_init(&priv->stream);
    priv->stream.options |= MAD_OPTION_IGNORECRC;
    mad_frame_init(&priv->frame);
    mad_synth_init(&priv->synth);
}

static void _free_mad(struct ad_mad_priv *priv)
{
    mad_stream_finish(&priv->stream);
    mad_frame_finish(&priv->frame);
    mad_synth_finish(&priv->synth);
}

static inline int _scale(mad_fixed_t sample)
{
    sample += 1L << (MAD_F_FRACBITS - 16);
    if (sample >= MAD_F_ONE) {
        sample = MAD_F_ONE - 1;
    } else if (sample < -MAD_F_ONE) {
        sample = -MAD_F_ONE;
    }
    return sample >> (MAD_F_FRACBITS - 15);
}

static int _ad_mad_open(ad_cls_t *o)
{
    struct ad_mad_priv *priv = NULL;

    priv = aos_zalloc(sizeof(struct ad_mad_priv));
    CHECK_RET_TAG_WITH_RET(NULL != priv, -1);

    _init_mad(priv);

    o->priv   = priv;

    return 0;
}

/**
 * @brief  fill the internal decode buffer
 * @param  [in] priv
 * @param  [in] pkt
 * @return -1 when err happens, otherwise the number of bytes consumed from the
 *         input avpacket is returned
 */
static int _fill_buffer(struct ad_mad_priv *priv, const avpacket_t *pkt)
{
    ssize_t read_size, remaining, len;
    unsigned char *read_start;

    if (priv->stream.next_frame != NULL) {
        remaining = priv->stream.bufend - priv->stream.next_frame;
        memmove(priv->input_buffer, priv->stream.next_frame, remaining);
        read_start = priv->input_buffer + remaining;
        read_size = INBUF_SIZE_MAX - remaining;
    } else {
        read_size = INBUF_SIZE_MAX;
        read_start = priv->input_buffer;
        remaining = 0;
    }

    if (read_size > 0) {
        read_size = read_size > pkt->len ? pkt->len : read_size;
        memcpy(read_start, pkt->data, read_size);
        len = read_size + remaining;
        mad_stream_buffer(&priv->stream, priv->input_buffer, len);
        priv->stream.error = 0;
    }

    return read_size > 0 ? read_size : -1;
}

static int _ad_mad_decode(ad_cls_t *o, avframe_t *frame, int *got_frame, const avpacket_t *pkt)
{
    int ret;
    sf_t sf;
    uint8_t *buf;
    int i, j, rc, channel, bits = 16;
    struct ad_mad_priv *priv = o->priv;

    *got_frame = 0;
    ret = _fill_buffer(priv, pkt);
    CHECK_RET_TAG_WITH_GOTO(ret > 0, quit);

    if (mad_frame_decode(&priv->frame, &priv->stream)) {
        if (priv->stream.error == MAD_ERROR_BUFLEN) {
            LOGD(TAG, "need more data\n");
            goto quit;
        }
        if (!MAD_RECOVERABLE(priv->stream.error)) {
            LOGD(TAG, "unrecoverable frame level error.\n");
            ret = -1;
            goto quit;
        } else {
            LOGE(TAG, "mad err type = %d, may be recoverable.", (priv->stream.error));
            goto quit;
        }
    }
    mad_synth_frame(&priv->synth, &priv->frame);

    channel           = MAD_NCHANNELS(&priv->frame.header);
    sf                = sf_make_channel(channel) | sf_make_rate(priv->frame.header.samplerate) | sf_make_bit(bits) | sf_make_signed(bits > 8);
    frame->sf         = sf;
    o->ash.sf         = sf;
    frame->nb_samples = priv->synth.pcm.length;

    rc = avframe_get_buffer(frame);
    if (rc < 0) {
        LOGD(TAG, "avframe_get_buffer failed, may be oom. sf = %u, ch = %d, rate = %d\n", sf, channel,
             priv->frame.header.samplerate);
        ret = -1;
        goto quit;
    }
    buf = frame->data[0];

    j = 0;
    for (i = 0; i < priv->synth.pcm.length; i++) {
        short sample;

        sample = _scale(priv->synth.pcm.samples[0][i]);
        buf[j++] = (sample >> 0) & 0xff;
        buf[j++] = (sample >> 8) & 0xff;

        if (channel == 2) {
            sample = _scale(priv->synth.pcm.samples[1][i]);
            buf[j++] = (sample >> 0) & 0xff;
            buf[j++] = (sample >> 8) & 0xff;
        }
    }
    *got_frame = 1;

quit:
    return ret;
}

static int _ad_mad_control(ad_cls_t *o, int cmd, void *arg, size_t *arg_size)
{
    //TODO
    return 0;
}

static int _ad_mad_reset(ad_cls_t *o)
{
    struct ad_mad_priv *priv = o->priv;

    _free_mad(priv);
    _init_mad(priv);

    return 0;
}

static int _ad_mad_close(ad_cls_t *o)
{
    struct ad_mad_priv *priv = o->priv;

    _free_mad(priv);
    aos_free(priv);
    o->priv = NULL;

    return 0;
}

const struct ad_ops ad_ops_mad = {
    .name           = "mad",
    .id             = AVCODEC_ID_MP3,

    .open           = _ad_mad_open,
    .decode         = _ad_mad_decode,
    .control        = _ad_mad_control,
    .reset          = _ad_mad_reset,
    .close          = _ad_mad_close,
};

#endif
