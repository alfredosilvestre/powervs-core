#include "audioencoder.h"

extern "C"
{
    #include <libavutil/opt.h>
}

#include <log4cxx/logger.h>

using namespace log4cxx;

AudioEncoder::AudioEncoder()
{
    outputContext = NULL;
    audioStream = NULL;
    audioCodecContext = NULL;
    frame = NULL;
    swrContext = NULL;
    bytesPerSample = 16;
    frameSize = 7680;
}

AudioEncoder::~AudioEncoder()
{
    cleanup();
}

bool AudioEncoder::initialize(QString format, AVFormatContext* outputContext)
{
    this->outputContext = outputContext;

    AVCodec* codec = NULL;

    if(format.contains("imx") || format.contains("xdcam"))
        codec = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE);
    else return false;

    if(format.contains("imx"))
    {
        audioStream = avformat_new_stream(outputContext, codec);
        audioCodecContext = avcodec_alloc_context3(codec);
        audioStream->id = outputContext->nb_streams-1;

        initIMX(audioCodecContext);

        // Some formats want stream headers to be separate.
        if (outputContext->oformat->flags & AVFMT_GLOBALHEADER)
            audioCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

        // Open audio codec
        avcodec_parameters_from_context(audioStream->codecpar, audioCodecContext);
        if(avcodec_open2(audioCodecContext, codec, NULL) < 0)
            return false;
    }
    else if(format.contains("xdcam"))
    {
        // Create 8 audio streams
        for(int i=0; i<8; i++)
        {
            AVStream* newAudioStream = avformat_new_stream(outputContext, codec);
            AVCodecContext* codecContext = avcodec_alloc_context3(codec);
            initXDCam(newAudioStream, codecContext);

            // Some formats want stream headers to be separate.
            if (outputContext->oformat->flags & AVFMT_GLOBALHEADER)
                codecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

            // Open audio codec
            avcodec_parameters_from_context(newAudioStream->codecpar, codecContext);
            if(avcodec_open2(codecContext, codec, NULL) < 0)
                return false;

            streamsList.append(newAudioStream);
            audioCodecContextList.append(codecContext);
        }
    }

    frame = av_frame_alloc();

    swrContext = swr_alloc();
    av_opt_set_int(swrContext, "in_channel_count", inChannelCount, 0);
    av_opt_set_int(swrContext, "out_channel_count", outChannelCount, 0);
    av_opt_set_int(swrContext, "in_channel_layout", inChannelLayout, 0);
    av_opt_set_int(swrContext, "out_channel_layout", outChannelLayout,  0);
    av_opt_set_int(swrContext, "in_sample_rate", inSampleRate, 0);
    av_opt_set_int(swrContext, "out_sample_rate", outSampleRate, 0);
    av_opt_set_sample_fmt(swrContext, "in_sample_fmt", inSampleFormat, 0);
    av_opt_set_sample_fmt(swrContext, "out_sample_fmt", outSampleFormat,  0);
    swr_init(swrContext);

    return true;
}

void AudioEncoder::initIMX(AVCodecContext* codecContext)
{
    codecContext->sample_fmt = AV_SAMPLE_FMT_S16;
    codecContext->bit_rate = 768000;
    codecContext->sample_rate = 48000;
    codecContext->channels = 8;
    codecContext->channel_layout = AV_CH_LAYOUT_7POINT1;

    codecContext->time_base.den = audioStream->time_base.den = 48000;
    codecContext->time_base.num = audioStream->time_base.num = 1;

    codecContext->codec_tag = MKTAG('s', 'o', 'w', 't');

    inChannelCount = codecContext->channels;
    outChannelCount = codecContext->channels;
    inChannelLayout = codecContext->channel_layout;
    outChannelLayout = codecContext->channel_layout;
    inSampleRate = codecContext->sample_rate;
    outSampleRate = codecContext->sample_rate;
    inSampleFormat = codecContext->sample_fmt;
    outSampleFormat = codecContext->sample_fmt;

    bytesPerSample = 2 * inChannelCount;
    frameSize = codecContext->sample_rate * bytesPerSample;
}

void AudioEncoder::initXDCam(AVStream* newAudioStream, AVCodecContext* codecContext)
{
    newAudioStream->id = outputContext->nb_streams-1;

    codecContext->sample_fmt = AV_SAMPLE_FMT_S16;
    codecContext->bit_rate = 768000;
    codecContext->sample_rate = 48000;
    codecContext->channels = 1;
    codecContext->channel_layout = AV_CH_LAYOUT_MONO;

    codecContext->time_base.den = newAudioStream->time_base.den = 48000;
    codecContext->time_base.num = newAudioStream->time_base.num = 1;

    codecContext->codec_tag = MKTAG('s', 'o', 'w', 't');

    inChannelCount = 8;
    outChannelCount = 8;
    inChannelLayout = AV_CH_LAYOUT_7POINT1;
    outChannelLayout = AV_CH_LAYOUT_7POINT1;
    inSampleRate = codecContext->sample_rate;
    outSampleRate = codecContext->sample_rate;
    inSampleFormat = codecContext->sample_fmt;
    outSampleFormat = AV_SAMPLE_FMT_S16P;

    bytesPerSample = 2*inChannelCount;
    frameSize = codecContext->sample_rate * bytesPerSample;
}

double AudioEncoder::muxAudioFrame(AVDecodedFrame* audioBuffer)
{
    if(audioStream != NULL)
    {
        if(audioBuffer == NULL)
            encodeAudioFrame(audioCodecContext, NULL, audioStream);
        else
        {
            int size = audioBuffer->getSize();
            frame->nb_samples = size / bytesPerSample;

            if(frameSize % size != 0)
                convertAudioBuffer(audioBuffer->getBuffer(), frame->nb_samples, audioCodecContext);
            else avcodec_fill_audio_frame(frame, audioCodecContext->channels, audioCodecContext->sample_fmt, audioBuffer->getBuffer(), size, 1);
        }

        encodeAudioFrame(audioCodecContext, frame, audioStream);

        return av_rescale_q(audioCodecContext->frame_number, audioCodecContext->time_base, audioStream->time_base);;
    }

    // XDCam 8 channel mono audio
    if(audioBuffer == NULL)
        encodeAudioFrame(audioCodecContextList[0], NULL, streamsList[0]);
    else
    {
        int nb_samples = audioBuffer->getSize() / bytesPerSample;
        convertAudioBuffer(audioBuffer->getBuffer(), nb_samples, audioCodecContextList[0]);
    }

    return av_rescale_q(audioCodecContextList[0]->frame_number / audioCodecContextList.size(), audioCodecContextList[0]->time_base, streamsList[0]->time_base);
}

void AudioEncoder::convertAudioBuffer(const uint8_t* audioData, int nb_samples, AVCodecContext* codecContext)
{
    uint8_t** dst_samples_data;
    int dst_samples_linesize;
    av_samples_alloc_array_and_samples(&dst_samples_data, &dst_samples_linesize, outChannelCount, nb_samples, outSampleFormat, 1);
    int dst_nb_samples = dst_samples_linesize / av_get_bytes_per_sample(outSampleFormat);

    swr_convert(swrContext, dst_samples_data, dst_nb_samples, &audioData, nb_samples);

    if(audioStream != NULL)
        avcodec_fill_audio_frame(frame, codecContext->channels, codecContext->sample_fmt, dst_samples_data[0], dst_samples_linesize, 1);
    else
    {
        for(int i=0; i<8; i++)
        {
            frame->nb_samples = nb_samples;

            avcodec_fill_audio_frame(frame, codecContext->channels, codecContext->sample_fmt, dst_samples_data[i], dst_samples_linesize, 1);

            encodeAudioFrame(codecContext, frame, streamsList[i]);
        }
    }

    av_freep(&dst_samples_data[0]);
    av_freep(&dst_samples_data);
}

void AudioEncoder::encodeAudioFrame(AVCodecContext* codecContext, AVFrame* audioFrame, const AVStream* stream)
{
    AVPacket pkt = { 0 };
    int ret;
    ret = avcodec_send_frame(codecContext, audioFrame);
    if (ret < 0 && ret != AVERROR_EOF)
        LOG4CXX_ERROR(Logger::getLogger("AudioEncoder"), "Error sending an audio frame for encoding");

    if(ret >= 0)
    {
        av_init_packet(&pkt);
        ret = avcodec_receive_packet(codecContext, &pkt);

        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        {
            LOG4CXX_ERROR(Logger::getLogger("AudioEncoder"), "Error during audio frame encoding");
        }
        else if (ret >= 0)
        {
            pkt.stream_index = stream->index;
            pkt.flags |= AV_PKT_FLAG_KEY;

            pkt.pts = pkt.dts = codecContext->frame_number;
            av_packet_rescale_ts(&pkt, codecContext->time_base, stream->time_base);

            av_interleaved_write_frame(outputContext, &pkt);
            av_packet_unref(&pkt);
        }
    }

    if(audioFrame != NULL)
        av_frame_unref(audioFrame);
}

void AudioEncoder::cleanup()
{
    if(audioCodecContext != NULL)
        avcodec_free_context(&audioCodecContext);
    audioCodecContext = NULL;
    audioStream = NULL;

    for(int i=0; i<audioCodecContextList.count(); i++)
    {
        avcodec_free_context(&audioCodecContextList[i]);
    }
    audioCodecContextList.clear();
    streamsList.clear();

    outputContext = NULL;

    if(frame != NULL)
        av_frame_free(&frame);
    frame = NULL;

    if(swrContext != NULL)
        swr_free(&swrContext);
    swrContext = NULL;

    bytesPerSample = 16;
    frameSize = 7680;
}
