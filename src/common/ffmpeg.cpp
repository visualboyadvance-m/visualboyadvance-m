// this code has been partially lifted from the output-example.c program in
// libavformat.  Not much of that original code remains.

// unlike the rest of the wx code, this has no wx dependency at all, and
// could be used by other front ends as well.

#define __STDC_LIMIT_MACROS // required for ffmpeg
#define __STDC_CONSTANT_MACROS // required for ffmpeg

#include "../gba/Sound.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mathematics.h>
#ifndef AV_PKT_FLAG_KEY
#define AV_PKT_FLAG_KEY PKT_FLAG_KEY
#endif
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(52,96,0)
// note that there is no sane way to easily free a context w/o free_context()
// so this will probably leak
static void avformat_free_context(AVFormatContext *ctx)
{
    if(ctx->pb)
	url_fclose(ctx->pb);
    for(int i = 0; i < ctx->nb_streams; i++) {
	if(ctx->streams[i]->codec)
	    avcodec_close(ctx->streams[i]->codec);
	av_freep(&ctx->streams[i]->codec);
	av_freep(&ctx->streams[i]);
    }
    av_free(ctx);
}
#endif
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(52,45,0)
#define av_guess_format guess_format
#endif
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(52,105,0)
#define avio_open url_fopen
#define avio_close url_fclose
#endif
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52,64,0)
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(50,1,0)
// this will almost definitely fail on big-endian systems
#define PIX_FMT_RGB565LE PIX_FMT_RGB565
#endif
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(50,38,0)
#define AV_SAMPLE_FMT_S16 SAMPLE_FMT_S16
#endif
}

// For compatibility with 3.0+ ffmpeg
#include <libavutil/version.h>
#ifndef PixelFormat
#define PixelFormat AVPixelFormat
#endif
#if LIBAVCODEC_VERSION_MAJOR >= 56
#define CODEC_ID_NONE AV_CODEC_ID_NONE
#define CODEC_ID_PCM_S16LE AV_CODEC_ID_PCM_S16LE
#define CODEC_ID_PCM_S16BE AV_CODEC_ID_PCM_S16BE
#define CODEC_ID_PCM_U16LE AV_CODEC_ID_PCM_U16LE
#define CODEC_ID_PCM_U16BE AV_CODEC_ID_PCM_U16BE
#endif
#if LIBAVCODEC_VERSION_MAJOR > 56
#define CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
#endif
#if LIBAVUTIL_VERSION_MAJOR > 54
#define avcodec_alloc_frame av_frame_alloc
#define PIX_FMT_RGB565LE AV_PIX_FMT_RGB565LE
#define PIX_FMT_RGB24 AV_PIX_FMT_RGB24
#define PIX_FMT_RGBA AV_PIX_FMT_RGBA
#endif

#define priv_AVFormatContext AVFormatContext
#define priv_AVStream AVStream
#define priv_AVOutputFormat AVOutputFormat
#define priv_AVFrame AVFrame
#define priv_SwsContext SwsContext
#define priv_PixelFormat PixelFormat
#include "ffmpeg.h"

// I have no idea what size to make these buffers
// I don't see any ffmpeg functions to guess the size, either

#ifdef AV_INPUT_BUFFER_MIN_SIZE

    // use frame size, or AV_INPUT_BUFFER_MIN_SIZE (that seems to be what it wants)
#define AUDIO_BUF_LEN (frame_len > AV_INPUT_BUFFER_MIN_SIZE ? frame_len : AV_INPUT_BUFFER_MIN_SIZE)
    // use maximum frame size * 32 bpp * 2 for good measure
#define VIDEO_BUF_LEN (AV_INPUT_BUFFER_MIN_SIZE + 256 * 244 * 4 * 2)

#else

    // use frame size, or FF_MIN_BUFFER_SIZE (that seems to be what it wants)
#define AUDIO_BUF_LEN (frame_len > FF_MIN_BUFFER_SIZE ? frame_len : FF_MIN_BUFFER_SIZE)
    // use maximum frame size * 32 bpp * 2 for good measure
#define VIDEO_BUF_LEN (FF_MIN_BUFFER_SIZE + 256 * 244 * 4 * 2)

#endif

bool MediaRecorder::did_init = false;

MediaRecorder::MediaRecorder() : oc(0), vid_st(0), aud_st(0), video_buf(0),
    audio_buf(0), audio_buf2(0), converter(0), convpic(0)
{
    if(!did_init) {
	did_init = true;
	av_register_all();
    }
    pic = avcodec_alloc_frame();
}

MediaRet MediaRecorder::setup_sound_stream(const char *fname, AVOutputFormat *fmt)
{
    oc = avformat_alloc_context();
    if(!oc)
	return MRET_ERR_NOMEM;
    oc->oformat = fmt;
    strncpy(oc->filename, fname, sizeof(oc->filename) - 1);
    oc->filename[sizeof(oc->filename) - 1] = 0;
    if(fmt->audio_codec == CODEC_ID_NONE)
	return MRET_OK;

    AVCodecContext *ctx;
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53,10,0)
    aud_st = av_new_stream(oc, 1);
#else
    aud_st = avformat_new_stream(oc, NULL);
#endif
    if(!aud_st) {
	avformat_free_context(oc);
	oc = NULL;
	return MRET_ERR_NOMEM;
    }

    AVCodec *codec = avcodec_find_encoder(fmt->audio_codec);

    if (!codec) {
	avformat_free_context(oc);
	oc = NULL;
	return MRET_ERR_NOCODEC;
    }

    ctx = aud_st->codec;
    ctx->codec_id = fmt->audio_codec;
    ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    // Some encoders don't like int16_t (SAMPLE_FMT_S16)
    ctx->sample_fmt = codec->sample_fmts[0];
    // This was changed in the initial ffmpeg 3.0 update,
    // but shouldn't (as far as I'm aware) cause problems with older versions
    ctx->bit_rate = 128000; // arbitrary; in case we're generating mp3
    ctx->sample_rate = soundGetSampleRate();
    ctx->channels = 2;
    ctx->time_base.den = 60;
    ctx->time_base.num = 1;
    if(fmt->flags & AVFMT_GLOBALHEADER)
	ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53,6,0)
    if(avcodec_open(ctx, codec)) {
#else
    if(avcodec_open2(ctx, codec, NULL)) {
#endif
	avformat_free_context(oc);
	oc = NULL;
	return MRET_ERR_NOCODEC;
    }

    return MRET_OK;
}

MediaRet MediaRecorder::setup_video_stream(const char *fname, int w, int h, int d)
{
    AVCodecContext *ctx;
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53,10,0)
    vid_st = av_new_stream(oc, 0);
#else
    vid_st = avformat_new_stream(oc, NULL);
#endif
    if(!vid_st) {
	avformat_free_context(oc);
	oc = NULL;
	return MRET_ERR_NOMEM;
    }
    ctx = vid_st->codec;
    ctx->codec_id = oc->oformat->video_codec;
    ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    ctx->width = w;
    ctx->height = h;
    ctx->time_base.den = 60;
    ctx->time_base.num = 1;
    // dunno if any of these help; some output just looks plain crappy
    // will have to investigate further
    ctx->bit_rate = 400000;
    ctx->gop_size = 12;
    ctx->max_b_frames = 2;
    switch(d) {
    case 16:
	// FIXME: test & make endian-neutral
	pixfmt = PIX_FMT_RGB565LE;
	break;
    case 24:
	pixfmt = PIX_FMT_RGB24;
	break;
    case 32:
    default: // should never be anything else
	pixfmt = PIX_FMT_RGBA;
	break;
    }
    ctx->pix_fmt = pixfmt;
    pixsize = d >> 3;
    linesize = pixsize * w;
    ctx->max_b_frames = 2;
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
	ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

    AVCodec *codec = avcodec_find_encoder(oc->oformat->video_codec);
    // make sure RGB is supported (mostly not)
    if(codec->pix_fmts) {
	const enum PixelFormat *p;
#if LIBAVCODEC_VERSION_MAJOR < 55
	int64_t mask = 0;
#endif
	for(p = codec->pix_fmts; *p != -1; p++) {
	    // may get complaints about 1LL; thus the cast
#if LIBAVCODEC_VERSION_MAJOR < 55
	    mask |= ((int64_t)1) << *p;
#endif
	    if(*p == pixfmt)
		break;
	}
	if(*p == -1) {
	    // if not supported, use a converter to the next best format
	    // this is swscale, the converter used by the output demo
#if LIBAVCODEC_VERSION_MAJOR < 55
	    enum PixelFormat dp = (PixelFormat)avcodec_find_best_pix_fmt(mask, pixfmt, 0, NULL);
#else
#if LIBAVCODEC_VERSION_MICRO >= 100
// FFmpeg
		enum AVPixelFormat dp = avcodec_find_best_pix_fmt_of_list(codec->pix_fmts, pixfmt, 0, NULL);
#else
// Libav
		enum AVPixelFormat dp = avcodec_find_best_pix_fmt2(codec->pix_fmts, pixfmt, 0, NULL);
#endif
#endif
	    if(dp == -1)
		dp = codec->pix_fmts[0];
	    if(!(convpic = avcodec_alloc_frame()) ||
	       avpicture_alloc((AVPicture *)convpic, dp, w, h) < 0) {
		avformat_free_context(oc);
		oc = NULL;
		return MRET_ERR_NOMEM;
	    }
#if LIBSWSCALE_VERSION_INT < AV_VERSION_INT(0, 12, 0)
	    converter = sws_getContext(w, h, pixfmt, w, h, dp, SWS_BICUBIC,
				       NULL, NULL, NULL);
#else
	    converter = sws_alloc_context();
	    // what a convoluted, inefficient way to set options
	    av_opt_set_int(converter, "sws_flags", SWS_BICUBIC, 0);
	    av_opt_set_int(converter, "srcw", w, 0);
	    av_opt_set_int(converter, "srch", h, 0);
	    av_opt_set_int(converter, "dstw", w, 0);
	    av_opt_set_int(converter, "dsth", h, 0);
	    av_opt_set_int(converter, "src_format", pixfmt, 0);
	    av_opt_set_int(converter, "dst_format", dp, 0);
	    sws_init_context(converter, NULL, NULL);
#endif
	    ctx->pix_fmt = dp;
	}
    }
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53,6,0)
    if(!codec || avcodec_open(ctx, codec)) {
#else
    if(!codec || avcodec_open2(ctx, codec, NULL)) {
#endif
	avformat_free_context(oc);
	oc = NULL;
	return MRET_ERR_NOCODEC;
    }

    return MRET_OK;
}

MediaRet MediaRecorder::finish_setup(const char *fname)
{
    if(audio_buf)
	free(audio_buf);
    if(audio_buf2)
	free(audio_buf2);
    audio_buf2 = NULL;
    in_audio_buf2 = 0;
    if(aud_st) {
	frame_len = aud_st->codec->frame_size * 4;
	sample_len = soundGetSampleRate() * 4 / 60;
	switch(aud_st->codec->codec_id) {
	case CODEC_ID_PCM_S16LE:
	case CODEC_ID_PCM_S16BE:
	case CODEC_ID_PCM_U16LE:
	case CODEC_ID_PCM_U16BE:
	    frame_len = sample_len;
	}
	audio_buf = (uint8_t *)malloc(AUDIO_BUF_LEN);
	if(!audio_buf) {
	    avformat_free_context(oc);
	    oc = NULL;
	    return MRET_ERR_NOMEM;
	}
	if(frame_len != sample_len && (frame_len > sample_len || sample_len % frame_len)) {
	    audio_buf2 = (uint16_t *)malloc(frame_len);
	    if(!audio_buf2) {
		avformat_free_context(oc);
		oc = NULL;
		return MRET_ERR_NOMEM;
	    }
	}
    } else
	audio_buf = NULL;
    if(video_buf)
	free(video_buf);
    if(vid_st) {
	video_buf = (uint8_t *)malloc(VIDEO_BUF_LEN);
	if(!video_buf) {
	    avformat_free_context(oc);
	    oc = NULL;
	    return MRET_ERR_NOMEM;
	}
    } else {
	video_buf = NULL;
    }
    if(!(oc->oformat->flags & AVFMT_NOFILE)) {
	if(avio_open(&oc->pb, fname, AVIO_FLAG_WRITE) < 0) {
	    avformat_free_context(oc);
	    oc = NULL;
	    return MRET_ERR_FERR;
	}
    }
    avformat_write_header(oc, NULL);    
    return MRET_OK;
}

MediaRet MediaRecorder::Record(const char *fname, int width, int height, int depth)
{
    if(oc)
	return MRET_ERR_RECORDING;
    aud_st = vid_st = NULL;
    AVOutputFormat *fmt = av_guess_format(NULL, fname, NULL);
    if(!fmt)
	fmt = av_guess_format("avi", NULL, NULL);
    if(!fmt || fmt->video_codec == CODEC_ID_NONE)
	return MRET_ERR_FMTGUESS;
    MediaRet ret;
    if((ret = setup_sound_stream(fname, fmt)) == MRET_OK &&
       (ret = setup_video_stream(fname, width, height, depth)) == MRET_OK)
	ret = finish_setup(fname);
    return ret;
}

MediaRet MediaRecorder::Record(const char *fname)
{
    if(oc)
	return MRET_ERR_RECORDING;
    aud_st = vid_st = NULL;
    AVOutputFormat *fmt = av_guess_format(NULL, fname, NULL);
    if(!fmt)
	fmt = av_guess_format("wav", NULL, NULL);
    if(!fmt || fmt->audio_codec == CODEC_ID_NONE)
	return MRET_ERR_FMTGUESS;
    MediaRet ret;
    if((ret = setup_sound_stream(fname, fmt)) == MRET_OK)
	ret = finish_setup(fname);
    return ret;
}

void MediaRecorder::Stop()
{
    if(oc) {
	if(in_audio_buf2)
	    AddFrame((uint16_t *)0);
	av_write_trailer(oc);
	avformat_free_context(oc);
	oc = NULL;
    }
    if(audio_buf) {
	free(audio_buf);
	audio_buf = NULL;
    }
    if(video_buf) {
	free(video_buf);
	video_buf = NULL;
    }
    if(audio_buf2) {
	free(audio_buf2);
	audio_buf2 = NULL;
    }
    if(convpic) {
	avpicture_free((AVPicture *)convpic);
	av_free(convpic);
	convpic = NULL;
    }
    if(converter) {
	sws_freeContext(converter);
	converter = NULL;
    }
}

MediaRecorder::~MediaRecorder()
{
    Stop();
}

// Still needs updating for avcodec_encode_video2
MediaRet MediaRecorder::AddFrame(const uint8_t *vid)
{
    if(!oc || !vid_st)
	return MRET_OK;

    AVCodecContext *ctx = vid_st->codec;
    AVPacket pkt;
#if LIBAVCODEC_VERSION_MAJOR >= 56
    int ret, got_packet = 0;
#endif

    // strip borders.  inconsistent between depths for some reason
    // but fortunately consistent between gb/gba.
    int tbord, rbord;
    switch(pixsize) {
    case 2:
	//    16-bit: 2 @ right, 1 @ top
	tbord = 1; rbord = 2; break;
    case 3:
	//    24-bit: no border
	tbord = rbord = 0; break;
    case 4:
	//    32-bit: 1 @ right, 1 @ top
	tbord = 1; rbord = 1; break;
    }
    avpicture_fill((AVPicture *)pic, (uint8_t *)vid + tbord * (linesize + pixsize * rbord),
		   (PixelFormat)pixfmt, ctx->width + rbord, ctx->height);
    // satisfy stupid sws_scale()'s integrity check
    pic->data[1] = pic->data[2] = pic->data[3] = pic->data[0];
    pic->linesize[1] = pic->linesize[2] = pic->linesize[3] = pic->linesize[0];

    AVFrame *f = pic;

    if(converter) {
	sws_scale(converter, pic->data, pic->linesize, 0, ctx->height,
		  convpic->data, convpic->linesize);
	f = convpic;
    }
    av_init_packet(&pkt);
    pkt.stream_index = vid_st->index;
#ifdef AVFMT_RAWPICTURE
    if(oc->oformat->flags & AVFMT_RAWPICTURE) {
	// this won't work due to border
	// not sure what formats set this, anyway
	pkt.flags |= AV_PKT_FLAG_KEY;
	pkt.data = f->data[0];
	pkt.size = linesize * ctx->height;
    } else {
#endif
#if LIBAVCODEC_VERSION_MAJOR >= 56
        pkt.data = video_buf;
        pkt.size = VIDEO_BUF_LEN;
        f->format = ctx->pix_fmt;
        f->width = ctx->width;
        f->height = ctx->height;
        ret = avcodec_encode_video2(ctx, &pkt, f, &got_packet);
        if(!ret && got_packet && ctx->coded_frame) {
            ctx->coded_frame->pts = pkt.pts;
            ctx->coded_frame->key_frame = !!(pkt.flags & AV_PKT_FLAG_KEY);
        }
#else
	pkt.size = avcodec_encode_video(ctx, video_buf, VIDEO_BUF_LEN, f);
#endif
	if(!pkt.size)
	    return MRET_OK;
	if(ctx->coded_frame && ctx->coded_frame->pts != AV_NOPTS_VALUE)
	    pkt.pts = av_rescale_q(ctx->coded_frame->pts, ctx->time_base, vid_st->time_base);
	if(pkt.size > VIDEO_BUF_LEN) {
	    avformat_free_context(oc);
	    oc = NULL;
	    return MRET_ERR_BUFSIZE;
	}
	if(ctx->coded_frame->key_frame)
	    pkt.flags |= AV_PKT_FLAG_KEY;
	pkt.data = video_buf;
#ifdef AVFMT_RAWPICTURE
    }
#endif
    if(av_interleaved_write_frame(oc, &pkt) < 0) {
	avformat_free_context(oc);
	oc = NULL;
	// yeah, err might not be a file error, but if it isn't, it's a
	// coding error rather than a user-controllable error
	// and better resolved using debugging
	return MRET_ERR_FERR;
    }
    return MRET_OK;
}

#if LIBAVCODEC_VERSION_MAJOR >= 56
/* FFmpeg depricated avcodec_encode_audio.
 * It was removed completely in 3.0.
 * This will at least get audio recording *working*
 */
static inline int MediaRecorderEncodeAudio(AVCodecContext *ctx,
                                           AVPacket *pkt,
                                           uint8_t *buf, int buf_size,
                                           const short *samples)
{
    AVFrame *frame;
    av_init_packet(pkt);
    int ret, samples_size, got_packet = 0;

    pkt->data = buf;
    pkt->size = buf_size;
    if (samples) {
        frame = frame = av_frame_alloc();
        if (ctx->frame_size) {
            frame->nb_samples = ctx->frame_size;
        } else {
            frame->nb_samples = (int64_t)buf_size * 8 /
                            (av_get_bits_per_sample(ctx->codec_id) *
                            ctx->channels);
        }
        frame->format = ctx->sample_fmt;
        frame->channel_layout = ctx->channel_layout;
        samples_size = av_samples_get_buffer_size(NULL, ctx->channels,
                        frame->nb_samples, ctx->sample_fmt, 1);
        avcodec_fill_audio_frame(frame, ctx->channels, ctx->sample_fmt,
                        (const uint8_t *)samples, samples_size, 1);
        //frame->pts = AV_NOPTS_VALUE;
    } else {
        frame = NULL;
    }
        ret = avcodec_encode_audio2(ctx, pkt, frame, &got_packet);
    if (!ret && got_packet && ctx->coded_frame) {
        ctx->coded_frame->pts = pkt->pts;
        ctx->coded_frame->key_frame = !!(pkt->flags & AV_PKT_FLAG_KEY);
    }
        if (frame && frame->extended_data != frame->data)
        av_freep(&frame->extended_data);
        return ret;

}
#endif

MediaRet MediaRecorder::AddFrame(const uint16_t *aud)
{
    if(!oc || !aud_st)
	return MRET_OK;
    // aud == NULL means just flush out last frame
    if(!aud && !in_audio_buf2)
	return MRET_OK;
    AVCodecContext *ctx = aud_st->codec;
    AVPacket pkt;

    int len = sample_len;
    if(in_audio_buf2) {
	int ncpy = frame_len - in_audio_buf2;
	if(ncpy > len)
	    ncpy = len;
	if(aud) {
	    memcpy(audio_buf2 + in_audio_buf2/2, aud, ncpy);
	    len -= ncpy;
	    aud += ncpy / 2;
	} else {
	    memset(audio_buf2 + in_audio_buf2/2, 0, ncpy);
	    len = 0;
	}
	in_audio_buf2 += ncpy;
    }
    while(len + in_audio_buf2 >= frame_len) {
	av_init_packet(&pkt);
	#if LIBAVCODEC_VERSION_MAJOR >= 56
	MediaRecorderEncodeAudio(ctx, &pkt, audio_buf, frame_len,
	#else
	pkt.size = avcodec_encode_audio(ctx, audio_buf, frame_len,
	#endif
					(const short *)(in_audio_buf2 ? audio_buf2 : aud));
	if(ctx->coded_frame && ctx->coded_frame->pts != AV_NOPTS_VALUE)
	    pkt.pts = av_rescale_q(ctx->coded_frame->pts, ctx->time_base, aud_st->time_base);
	pkt.flags |= AV_PKT_FLAG_KEY;
	pkt.stream_index = aud_st->index;
	#if LIBAVCODEC_VERSION_MAJOR < 57
	pkt.data = audio_buf;
	#endif
	if(av_interleaved_write_frame(oc, &pkt) < 0) {
	    avformat_free_context(oc);
	    oc = NULL;
	    // yeah, err might not be a file error, but if it isn't, it's a
	    // coding error rather than a user-controllable error
	    // and better resolved using debugging
	    return MRET_ERR_FERR;
	}
	if(in_audio_buf2)
	    in_audio_buf2 = 0;
	else {
	    aud += frame_len / 2;
	    len -= frame_len;
	}
    }
    if(len > 0) {
	memcpy(audio_buf2, aud, len);
	in_audio_buf2 = len;
    }
    return MRET_OK;
}
