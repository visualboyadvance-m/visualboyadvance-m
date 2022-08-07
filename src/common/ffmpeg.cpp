#include "ffmpeg.h"

#define STREAM_FRAME_RATE 60
#define STREAM_PIXEL_FORMAT AV_PIX_FMT_YUV420P
#define IN_SOUND_FORMAT AV_SAMPLE_FMT_S16

struct supportedCodecs {
    AVCodecID codecId;
    char const *longName;
    char const *exts;
};

const supportedCodecs audioSupported[] = {
    { AV_CODEC_ID_MP3, "MP3 (MPEG audio layer 3)", "mp3" },
    { AV_CODEC_ID_AAC, "ADTS AAC (Advanced Audio Coding)", "aac,adts" },
    { AV_CODEC_ID_PCM_S16LE, "WAV / WAVE (Waveform Audio)", "wav" }
};

const supportedCodecs videoSupported[] = {
    { AV_CODEC_ID_MPEG4, "AVI (Audio Video Interleaved)", "avi" },
    { AV_CODEC_ID_MPEG4, "raw MPEG-4 video", "m4v" },
    { AV_CODEC_ID_FLV1, "FLV (Flash Video)", "flv" }
};

std::vector<char *> recording::getSupVidNames()
{
    std::vector<char *> result;
    for (auto&& codec : videoSupported)
        result.push_back((char *)codec.longName);
    return result;
}

std::vector<char *> recording::getSupVidExts()
{
    std::vector<char *> result;
    for (auto&& codec : videoSupported)
        result.push_back((char *)codec.exts);
    return result;
}

std::vector<char *> recording::getSupAudNames()
{
    std::vector<char *> result;
    for (auto&& codec : audioSupported)
        result.push_back((char *)codec.longName);
    return result;
}

std::vector<char *> recording::getSupAudExts()
{
    std::vector<char *> result;
    for (auto&& codec : audioSupported)
        result.push_back((char *)codec.exts);
    return result;
}


// avoid 'error: taking address of temporary array'
// for debug function when compiling
#ifdef av_err2str
#undef av_err2str
#define av_err2str(errnum) av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)
#endif

#ifdef av_ts2str
#undef av_ts2str
#define av_ts2str(ts) av_ts_make_string((char*)__builtin_alloca(AV_TS_MAX_STRING_SIZE), ts)
#endif

#ifdef av_ts2timestr
#undef av_ts2timestr
#define av_ts2timestr(ts,tb) av_ts_make_time_string((char*)__builtin_alloca(AV_TS_MAX_STRING_SIZE), ts, tb)
#endif

// debug function
//static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
//{
//    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
//    fprintf(stderr, "pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
//            av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
//            av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
//            av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
//            pkt->stream_index);
//}

recording::MediaRecorder::~MediaRecorder()
{
    Stop();
}

recording::MediaRet recording::MediaRecorder::setup_audio_stream()
{
    // audio stream
    ast = avformat_new_stream(oc, NULL);
    if (!ast) return MRET_ERR_BUFSIZE;
    ast->id = oc->nb_streams - 1;
    // audio codec
    acodec = avcodec_find_encoder(fmt->audio_codec);
    if (!acodec) return MRET_ERR_NOCODEC;
    // audio codec context
    aenc = avcodec_alloc_context3(acodec);
    if (!aenc) return MRET_ERR_BUFSIZE;
    aenc->sample_fmt = acodec->sample_fmts ? acodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    aenc->bit_rate = 128000; // mp3
    aenc->sample_rate = sampleRate;
    // this might be useful to check if the codec suports the
    // sample rate, but it is not strictly needed for now
    bool isSupported = false;
    if (acodec->supported_samplerates)
    {
        for (int i = 0; acodec->supported_samplerates[i]; ++i)
        {
            if (acodec->supported_samplerates[i] == sampleRate)
            {
                isSupported = true;
                break;
            }
        }
    }
    if (!isSupported && acodec->supported_samplerates) return MRET_ERR_NOCODEC;
    aenc->channels = av_get_channel_layout_nb_channels(aenc->channel_layout);
    aenc->channel_layout = AV_CH_LAYOUT_STEREO;
    if (acodec->channel_layouts)
    {
        aenc->channel_layout = acodec->channel_layouts[0];
        for (int i = 0; acodec->channel_layouts[i]; ++i)
        {
            if (acodec->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                aenc->channel_layout = AV_CH_LAYOUT_STEREO;
        }
    }
    aenc->channels = av_get_channel_layout_nb_channels(aenc->channel_layout);
    aenc->time_base = { 1, aenc->sample_rate };
    ast->time_base  = { 1, STREAM_FRAME_RATE };
    // open and use codec on stream
    int nb_samples;
    if (avcodec_open2(aenc, acodec, NULL) < 0)
        return MRET_ERR_NOCODEC;
    if (avcodec_parameters_from_context(ast->codecpar, aenc) < 0)
        return MRET_ERR_BUFSIZE;
    // number of samples per frame
    if (aenc->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
    {
        //nb_samples = 10000; // can be any value, but we use our aud buffer size
        nb_samples = 1470;
    }
    else
        nb_samples = aenc->frame_size;
    // audio frame for input
    audioframeTmp = av_frame_alloc();
    if (!audioframeTmp) return MRET_ERR_BUFSIZE;
    audioframeTmp->format = IN_SOUND_FORMAT;
    audioframeTmp->channel_layout = aenc->channel_layout;
    audioframeTmp->sample_rate = aenc->sample_rate;
    audioframeTmp->nb_samples = nb_samples;
    if (nb_samples)
    {
        if (av_frame_get_buffer(audioframeTmp, 0) < 0)
            return MRET_ERR_BUFSIZE;
    }
    // audio frame for output
    audioframe = av_frame_alloc();
    if (!audioframe) return MRET_ERR_BUFSIZE;
    audioframe->format = aenc->sample_fmt;
    audioframe->channel_layout = aenc->channel_layout;
    audioframe->sample_rate = aenc->sample_rate;
    audioframe->nb_samples = nb_samples;
    if (nb_samples)
    {
        if (av_frame_get_buffer(audioframe, 0) < 0)
            return MRET_ERR_BUFSIZE;
    }
    // initialize the converter
    swr = swr_alloc();
    if (!swr)
    {
        return MRET_ERR_BUFSIZE;
    }
    av_opt_set_int       (swr, "in_channel_count",  aenc->channels,    0);
    av_opt_set_int       (swr, "in_sample_rate",    aenc->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt",     IN_SOUND_FORMAT,   0);
    av_opt_set_int       (swr, "out_channel_count", aenc->channels,    0);
    av_opt_set_int       (swr, "out_sample_rate",   aenc->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt",    aenc->sample_fmt,  0);
    if (swr_init(swr) < 0)
    {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        return MRET_ERR_BUFSIZE;
    }
    // auxiliary buffer for setting up frames for encode
    audioBufferSize = nb_samples * aenc->channels * sizeof(uint16_t);
    audioBuffer = (uint16_t *) calloc(nb_samples * aenc->channels, sizeof(uint16_t));
    if (!audioBuffer) return MRET_ERR_BUFSIZE;
    samplesInAudioBuffer = 0;
    posInAudioBuffer = 0;
    return MRET_OK;
}

recording::MediaRet recording::MediaRecorder::setup_video_stream_info(int width, int height, int depth)
{
    switch (depth)
    {
        case 16:
            // FIXME: test & make endian-neutral
            pixfmt = AV_PIX_FMT_RGB565LE;
            break;
        case 24:
            pixfmt = AV_PIX_FMT_RGB24;
            break;
        case 32:
            pixfmt = AV_PIX_FMT_RGBA;
            break;
        default: // should never be anything else
            pixfmt = AV_PIX_FMT_RGBA;
            break;
    }
    // initialize the converter
    sws = sws_getContext(width, height, pixfmt, // from
                         width, height, STREAM_PIXEL_FORMAT, // to
                         SWS_BICUBIC, NULL, NULL, NULL); // params
    if (!sws) return MRET_ERR_BUFSIZE;
    // getting info about frame
    pixsize = depth >> 3;
    linesize = pixsize * width;
    switch (pixsize)
    {
        case 2:
            // 16-bit: 2 @ right, 1 @ top
            tbord = 1; rbord = 2;
            break;
        case 3:
            // 24-bit: no border
            tbord = rbord = 0;
            break;
        case 4:
            // 32-bit: 1 @ right, 1 @ top
            tbord = rbord = 1;
            break;
        default:
            break;
    }
    return MRET_OK;
}

recording::MediaRet recording::MediaRecorder::setup_video_stream(int width, int height)
{
    // video stream
    st = avformat_new_stream(oc, NULL);
    if (!st) return MRET_ERR_NOMEM;
    st->id = oc->nb_streams - 1;
    st->time_base = { 1, STREAM_FRAME_RATE };
    // video codec
    vcodec = avcodec_find_encoder(fmt->video_codec);
    if (!vcodec) return MRET_ERR_FMTGUESS;
    // codec context
    enc = avcodec_alloc_context3(vcodec);
    enc->codec_id = fmt->video_codec;
    enc->bit_rate = 400000; // arbitrary
    enc->width = width;
    enc->height = height;
    enc->time_base = st->time_base;
    enc->gop_size = 12;
    enc->pix_fmt = STREAM_PIXEL_FORMAT;
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (enc->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        enc->max_b_frames = 2;
    if (enc->codec_id == AV_CODEC_ID_MPEG1VIDEO)
        enc->mb_decision = 2;
    // open and use codec on stream
    if (avcodec_open2(enc, vcodec, NULL) < 0) return MRET_ERR_NOCODEC;
    if (avcodec_parameters_from_context(st->codecpar, enc) < 0) return MRET_ERR_BUFSIZE;
    // frame for input
    frameIn = av_frame_alloc();
    if (!frameIn) return MRET_ERR_NOMEM;
    frameIn->format = pixfmt;
    frameIn->width  = width;
    frameIn->height = height;
    if (av_frame_get_buffer(frameIn, 32) < 0) return MRET_ERR_NOMEM;
    // frame for output
    frameOut = av_frame_alloc();
    if (!frameOut) return MRET_ERR_NOMEM;
    frameOut->format = STREAM_PIXEL_FORMAT;
    frameOut->width  = width;
    frameOut->height = height;
    if (av_frame_get_buffer(frameOut, 32) < 0) return MRET_ERR_NOMEM;
    return MRET_OK;
}

recording::MediaRet recording::MediaRecorder::finish_setup(const char *fname)
{
    av_dump_format(oc, 0, fname, 1);
    // open the output file
    if (!(fmt->flags & AVFMT_NOFILE))
    {
        if (avio_open(&oc->pb, fname, AVIO_FLAG_WRITE) < 0)
            return MRET_ERR_FERR;
    }
    // write the stream header
    if (avformat_write_header(oc, NULL) < 0) return MRET_ERR_FERR;
    return MRET_OK;
}

recording::MediaRecorder::MediaRecorder() : isRecording(false),
    sampleRate(44100), oc(NULL), fmt(NULL), audioOnlyRecording(false)
{
    // pic info
    pixfmt = AV_PIX_FMT_NONE;
    pixsize = linesize = -1;
    tbord = rbord = 0;
    sws = NULL;
    // stream info
    st = NULL;
    vcodec = NULL;
    enc = NULL;
    npts = 0;
    frameIn = frameOut = NULL;
    // audio setup
    swr = NULL;
    acodec = NULL;
    ast = NULL;
    aenc = NULL;
    samplesCount = 0;
    audioframe = NULL;
    audioframeTmp = NULL;
    audioBuffer = NULL;
    posInAudioBuffer = 0;
    samplesInAudioBuffer = 0;
    audioBufferSize = 0;
}

// video : return error code to user
recording::MediaRet recording::MediaRecorder::Record(const char *fname, int width, int height, int depth)
{
    MediaRet ret;
    if (isRecording) return MRET_ERR_RECORDING;
    isRecording = true;
    // initial setup
    ret = setup_common(fname);
    if (ret != MRET_OK)
    {
        Stop();
        return ret;
    }
    // video stream
    ret = setup_video_stream_info(width, height, depth);
    if (ret != MRET_OK)
    {
        Stop();
        return ret;
    }
    ret = setup_video_stream(width, height);
    if (ret != MRET_OK)
    {
        Stop();
        return ret;
    }
    // audio stream
    ret = setup_audio_stream();
    if (ret != MRET_OK)
    {
        Stop();
        return ret;
    }
    // last details
    ret = finish_setup(fname);
    if (ret != MRET_OK)
    {
        Stop();
        return ret;
    }
    return MRET_OK;
}

recording::MediaRet recording::MediaRecorder::AddFrame(const uint8_t *vid)
{
    if (!isRecording) return MRET_OK;
    // fill and encode frame variables
    int got_packet = 0, ret = 0;
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    // fill frame with current pic
    ret = av_image_fill_arrays(frameIn->data, frameIn->linesize,
                               (uint8_t *)vid + tbord * (linesize + pixsize * rbord),
                               pixfmt, enc->width + rbord, enc->height, 1);
    if (ret < 0) return MRET_ERR_RECORDING;
    // convert from input format to output
    sws_scale(sws, (const uint8_t * const *) frameIn->data,
              frameIn->linesize, 0, enc->height, frameOut->data,
              frameOut->linesize);
    // set valid pts for frame
    frameOut->pts = npts++;
    // finally, encode frame
    got_packet = avcodec_receive_packet(enc, &pkt);
    ret = avcodec_send_frame(enc, frameOut);
    if (ret < 0) return MRET_ERR_RECORDING;
    if (!got_packet)
    {
        // rescale output packet timestamp values from codec
        // to stream timebase
        av_packet_rescale_ts(&pkt, enc->time_base, st->time_base);
        pkt.stream_index = st->index;
        //log_packet(oc, &pkt);
        ret = av_interleaved_write_frame(oc, &pkt);
        if (ret < 0) return MRET_ERR_RECORDING;
    }
    return MRET_OK;
}

void recording::MediaRecorder::Stop()
{
    if (oc)
    {
        // write the trailer; must be called before av_codec_close()
        if (!audioOnlyRecording)
            av_write_trailer(oc);
    }
    isRecording = false;
    if (sws)
    {
        sws_freeContext(sws);
        sws = NULL;
    }
    if (st)
    {
        st = NULL;
    }
    if (enc)
    {
        avcodec_free_context(&enc);
        avcodec_close(enc);
        enc = NULL;
    }
    if (vcodec)
    {
        vcodec = NULL;
    }
    if (frameIn)
    {
        av_frame_free(&frameIn);
        frameIn = NULL;
    }
    if (frameOut)
    {
        av_frame_free(&frameOut);
        frameOut = NULL;
    }
    npts = 0;
    if (oc)
    {
        flush_frames();
        // close the output file
        if (!(fmt->flags & AVFMT_NOFILE))
        {
            avio_closep(&oc->pb);
        }
        fmt = NULL;
        avformat_free_context(oc);
        oc = NULL;
    }

    // audio
    audioOnlyRecording = false;
    if (swr)
    {
        swr_free(&swr);
        swr = NULL;
    }
    if (acodec)
    {
        acodec = NULL;
    }
    if (ast)
    {
        ast = NULL;
    }
    if (aenc)
    {
        avcodec_free_context(&aenc);
        avcodec_close(aenc);
        aenc = NULL;
    }
    samplesCount = 0;
    if (audioframe)
    {
        av_frame_free(&audioframe);
        audioframe = NULL;
    }
    if (audioframeTmp)
    {
        av_frame_free(&audioframeTmp);
        audioframeTmp = NULL;
    }
    if (audioBuffer)
    {
        free(audioBuffer);
        audioBuffer = NULL;
    }
    samplesInAudioBuffer = 0;
    posInAudioBuffer = 0;
}

recording::MediaRet recording::MediaRecorder::setup_common(const char *fname)
{
    avformat_alloc_output_context2(&oc, NULL, NULL, fname);
    if (!oc) return MRET_ERR_BUFSIZE;
    fmt = oc->oformat;
    return MRET_OK;
}

// audio : return error code to user
recording::MediaRet recording::MediaRecorder::Record(const char *fname)
{
    MediaRet ret;
    if (isRecording) return MRET_ERR_RECORDING;
    isRecording = true;
    audioOnlyRecording = true;
    // initial setup
    ret = setup_common(fname);
    if (ret != MRET_OK)
    {
        Stop();
        return ret;
    }
    // audio stream
    ret = setup_audio_stream();
    if (ret != MRET_OK)
    {
        Stop();
        return ret;
    }
    // last details
    ret = finish_setup(fname);
    if (ret != MRET_OK)
    {
        Stop();
        return ret;
    }
    return MRET_OK;
}

#define MIN(a,b) (((a)<(b))?(a):(b))

recording::MediaRet recording::MediaRecorder::AddFrame(const uint16_t *aud, int length)
{
    if (!isRecording) return MRET_OK;
    AVCodecContext *c = aenc;
    int samples_size = av_samples_get_buffer_size(NULL, c->channels, audioframeTmp->nb_samples, IN_SOUND_FORMAT, 1);

    int realLength = length / sizeof *aud;
    bool isMissing = false;
    int cp = -1;

    if (c->frame_size == 0) // no compression/limit for audio frames
    {
        int maxCopy = realLength;
        memcpy(audioBuffer + posInAudioBuffer, aud, maxCopy * 2);
        posInAudioBuffer += maxCopy;
        samplesInAudioBuffer += (maxCopy / 2);
        aud += maxCopy;
    }

    if (samplesInAudioBuffer < c->frame_size)
    {
        int missingSamples = (c->frame_size - samplesInAudioBuffer);
        // 2 * missingSamples =: for 2 channels
        // realLength =: entire samples (1470 ~ 735 samples per channel)
        int maxCopy = MIN(2 * missingSamples, realLength);
        memcpy(audioBuffer + posInAudioBuffer, aud, maxCopy * 2);
        posInAudioBuffer += maxCopy;
        samplesInAudioBuffer += (maxCopy / 2);
        aud += maxCopy;
        if (maxCopy < realLength)
        {
            isMissing = true;
            cp = realLength - maxCopy;
        }
    }
    if (samplesInAudioBuffer != c->frame_size && (c->frame_size > 0 || samplesInAudioBuffer != realLength)) // not enough samples
    {
        return MRET_OK;
    }

    int got_packet;
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    if (avcodec_fill_audio_frame(audioframeTmp, c->channels, IN_SOUND_FORMAT, (const uint8_t *)audioBuffer, samples_size, 1) < 0)
    {
        return MRET_ERR_RECORDING;
    }

    int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr, c->sample_rate) + audioframeTmp->nb_samples, c->sample_rate, c->sample_rate, AV_ROUND_UP);
    av_assert0(dst_nb_samples == audioframeTmp->nb_samples);

    if (swr_convert(swr, audioframe->data, audioframe->nb_samples, (const uint8_t **)audioframeTmp->data, audioframeTmp->nb_samples) < 0)
    {
        return MRET_ERR_RECORDING;
    }
    audioframe->pts = av_rescale_q(samplesCount, {1, c->sample_rate}, c->time_base);
    samplesCount += dst_nb_samples;

    got_packet = avcodec_receive_packet(c, &pkt);
    if (avcodec_send_frame(c, audioframe) < 0)
    {
        return MRET_ERR_RECORDING;
    }
    if (!got_packet)
    {
        av_packet_rescale_ts(&pkt, { 1, c->sample_rate }, ast->time_base);
        pkt.stream_index = ast->index;
        //log_packet(oc, &pkt);
        if (av_interleaved_write_frame(oc, &pkt) < 0)
        {
            return MRET_ERR_RECORDING;
        }
    }
    // if we are missing part of the sample, adjust here
    // for next iteration
    posInAudioBuffer = 0;
    samplesInAudioBuffer = 0;
    memset(audioBuffer, 0, audioBufferSize);
    if (isMissing)
    {
        memcpy(audioBuffer, aud, cp * 2);
        posInAudioBuffer = cp;
        samplesInAudioBuffer = (cp / 2);
    }
    return MRET_OK;
}

// flush last frames to avoid
// "X frames left in the queue on closing"
void recording::MediaRecorder::flush_frames()
{
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    // flush last audio frames
    while (avcodec_receive_packet(aenc, &pkt) >= 0)
        avcodec_send_frame(aenc, NULL);
}
