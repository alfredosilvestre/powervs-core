#include "avdecodedframe.h"
#include "player.h"
#include "ffdecoder.h"

#include <QStringList>

extern "C"
{
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    #include <libavfilter/buffersrc.h>
    #include <libavfilter/buffersink.h>
}

#include <log4cxx/logger.h>

using namespace log4cxx;

FFDecoder::FFDecoder(Player* player, bool loop) : QThread(player)
{
    this->player = player;

    avFormatContext = NULL;
    videoCodecContext = NULL;
    audioCodecContext = NULL;

    videoStream = NULL;
    audioStream = NULL;

    tmpFrame = NULL;
    frameUYVY = NULL;
    swsContext = NULL;

#ifdef GUI
    framePreview = NULL;
    swsContextPreview = NULL;
#endif

    frameAudio = NULL;
    swrContext = NULL;

#ifdef PORTAUDIO
    swrContextPreview = NULL;
#endif

    filterFrame = NULL;
    filterGraph = NULL;
    buffersinkContext = NULL;
    buffersrcContext = NULL;

    decoding = false;
    this->loop = loop;
    fps = 0.0;
    rate = 1.0;
}

FFDecoder::~FFDecoder()
{
    cleanup();
}

void FFDecoder::setRate(const double rate)
{
    this->rate = rate;
}

void FFDecoder::toggleLoop(const bool loop, const bool active)
{
    this->loop = loop;
    if(loop && !decoding && active)
    {
        seek(0, AVSEEK_FLAG_BACKWARD);
        decoding = true;
        start();
    }
}

void FFDecoder::changeFormat(const QString& format)
{
	// Video output variables
	if(format == "PAL" || format == "PAL 16:9")
	{
        outputWidth = 720;
        outputHeight = videoCodecContext->height == 608 ? 608 : 576;
	}
    else if(format == "720p50" || format == "720p5994")
	{
		outputWidth = 1280;
		outputHeight = 720;
	}
    else if(format == "1080p25" || format == "1080i50" || format == "1080i5994")
	{
		outputWidth = 1920;
		outputHeight = 1080;
	}

	outputFrameSize = (outputWidth * outputHeight) * 2;
	outputFormat = AV_PIX_FMT_UYVY422;
	
#ifdef GUI
    // Preview variables
    if(format == "PAL")
        previewWidth = 360;
    else previewWidth = 512;
    previewHeight = videoCodecContext->height == 608 ? 304 : 288;
    previewOffset = 0;
    if(previewHeight == 304)
        previewOffset = 16 * previewWidth;
    previewFrameSize = (previewWidth * previewHeight - previewOffset) * 4; 
    previewFormat = AV_PIX_FMT_RGBA;
#endif

    // Video convert contexts
	if(swsContext != NULL)
        sws_freeContext(swsContext);
    swsContext = NULL;
    swsContext = sws_getContext(videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
							outputWidth, outputHeight, outputFormat,
							SWS_BILINEAR, NULL, NULL, NULL);

#ifdef GUI
	if(swsContextPreview != NULL)
        sws_freeContext(swsContextPreview);
    swsContextPreview = NULL;
    swsContextPreview = sws_getContext(videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
							previewWidth, previewHeight, previewFormat,
							SWS_FAST_BILINEAR, NULL, NULL, NULL);
#endif

    createFrames(format);
}

void FFDecoder::createFrames(const QString& format)
{
    // Allocate video frame for getting source video
    if(tmpFrame != NULL)
        av_frame_free(&tmpFrame);
    tmpFrame = av_frame_alloc();

#ifdef GUI
    // Allocate video frame for video preview
    if(framePreview != NULL)
        av_frame_free(&framePreview);
    framePreview = av_frame_alloc();
    framePreview->width  = previewWidth;
    framePreview->height = previewHeight;
    framePreview->format = previewFormat;
    av_frame_get_buffer(framePreview, 32);
#endif

    // Allocate video frame for output
    if(frameUYVY != NULL)
        av_frame_free(&frameUYVY);
    frameUYVY = av_frame_alloc();
    frameUYVY->width  = outputWidth;
    frameUYVY->height = outputHeight;
    frameUYVY->format = outputFormat;
    if(format != "720p50" && format != "720p5994")
    {
        frameUYVY->interlaced_frame = 1;
        frameUYVY->top_field_first = 1;
    }
    av_frame_get_buffer(frameUYVY, 32);

    // Allocate audio frame
    if(frameAudio != NULL)
        av_frame_free(&frameAudio);
    frameAudio = av_frame_alloc();
}

#ifdef DECKLINK
const int64_t FFDecoder::init(const QString& path, const QString& format, DeckLinkOutput* deckLinkOutput)
#else
const int64_t FFDecoder::init(const QString& path, const QString& format)
#endif
{
    // Open video file
    //if(avformat_open_input(&avFormatContext, "http://docs.gstreamer.com/media/sintel_trailer-480p.webm", NULL, NULL) != 0)
    AVDictionary* options = NULL;
    av_dict_set(&options, "enable_drefs", "1", 0);
    if(avformat_open_input(&avFormatContext, path.toStdString().c_str(), NULL, &options) != 0)
    {
        cleanup();
        LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Problem avformat_open_input()");
        return -1;
    }

    if(avformat_find_stream_info(avFormatContext, NULL) < 0)
    {
        cleanup();
        LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Problem avformat_find_stream_info()");
        return -1;
    }

    // Dump information about input file
    //av_dump_format(avFormatContext, 0, path.toStdString().c_str(), 0);

    int64_t duration_ms = 0;
    AVRational tb;
    tb.den = 1000;
    tb.num = 1;

    for(unsigned int i=0; i < avFormatContext->nb_streams; i++)
    {
        AVStream* stream = avFormatContext->streams[i];

        duration_ms = av_rescale_q(stream->duration, stream->time_base, tb);
        if(duration_ms <= 0)
            duration_ms = avFormatContext->duration / 1000;
        if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            break;
    }

    for(unsigned int i=0; i < avFormatContext->nb_streams; i++)
    {
        AVCodecParameters* codecParameters = avFormatContext->streams[i]->codecpar;
        AVMediaType codecType = codecParameters->codec_type;

        if(codecType == AVMEDIA_TYPE_VIDEO || codecType == AVMEDIA_TYPE_AUDIO)
        {
            AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
            if(codec == NULL)
            {
                cleanup();
                LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Problem codec == NULL");
                return -1;
            }

            AVCodecContext* codecContext = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(codecContext, codecParameters);

            // Open codec
            if(avcodec_open2(codecContext, codec, NULL) < 0)
            {
                cleanup();
                LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Problem avcodec_open2()");
                return -1;
            }

            codecContextList.append(codecContext);

            if(codecType == AVMEDIA_TYPE_VIDEO && videoCodecContext == NULL && videoStream == NULL)
            {
                videoCodecContext = codecContext;

                AVDictionaryEntry* timecode = av_dict_get(avFormatContext->metadata, "timecode", NULL, 0);
                if(timecode != NULL)
                {
                    QStringList split = QString(timecode->value).split(QRegExp(":|;"));

                    int64_t start_millisecond = split.at(0).toInt() * 1000 * 3600;
                    start_millisecond += split.at(1).toInt() * 1000 * 60;
                    start_millisecond += split.at(2).toInt() * 1000;
                    start_millisecond += split.at(3).toInt() * 1000 / 25;

                    player->setStartMillisecond(start_millisecond);
                }

                videoStream = avFormatContext->streams[i];
                fps = av_q2d(av_guess_frame_rate(avFormatContext, videoStream, NULL));

#ifdef DECKLINK
                if(deckLinkOutput != NULL)
                {
                    // Reset adjustment
                    deckLinkOutput->adjustFPS(1.0);

                    // Adjust output according to source file FPS
                    if(format == "PAL" || format == "PAL 16:9" || format == "1080p25" || format == "1080i50")
                    {
                        if(fps != 25.0)
                        {
                            deckLinkOutput->adjustFPS(fps/25.0);
                            LOG4CXX_WARN(Logger::getLogger("FFDecoder"), "FPS mismatch on file: " + path.toStdString() + " -> " + QString::number(fps).toStdString() + " != 25.0 (Output format: " + format.toStdString() + ")");
                        }
                    }
                    else if(format == "720p50")
                    {
                        if(fps != 50.0)
                        {
                            deckLinkOutput->adjustFPS(fps/50.0);
                            LOG4CXX_WARN(Logger::getLogger("FFDecoder"), "FPS mismatch on file: " + path.toStdString() + " -> " + QString::number(fps).toStdString() + " != 50.0 (Output format: " + format.toStdString() + ")");
                        }
                    }
                    else if(format == "720p5994")
                    {
                        if(fps != 59.94)
                        {
                            deckLinkOutput->adjustFPS(fps/59.94);
                            LOG4CXX_WARN(Logger::getLogger("FFDecoder"), "FPS mismatch on file: " + path.toStdString() + " -> " + QString::number(fps).toStdString() + " != 59.94 (Output format: " + format.toStdString() + ")");
                        }
                    }
                    else if(format == "1080i5994")
                    {
                        if(fps != 29.97)
                        {
                            deckLinkOutput->adjustFPS(fps/29.97);
                            LOG4CXX_WARN(Logger::getLogger("FFDecoder"), "FPS mismatch on file: " + path.toStdString() + " -> " + QString::number(fps).toStdString() + " != 29.97 (Output format: " + format.toStdString() + ")");
                        }
                    }
                }
#endif

                if(videoCodecContext->pix_fmt == AV_PIX_FMT_NONE)
                {
                    cleanup();
                    LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Problem pix_fmt == AV_PIX_FMT_NONE");
                    return -1;
                }

				changeFormat(format);
            }
            else if(codecType == AVMEDIA_TYPE_AUDIO && audioCodecContext == NULL && audioStream == NULL)
            {
                audioCodecContext = codecContext;
                audioStream = avFormatContext->streams[i];

                if(audioCodecContext->sample_rate != 48000)
                {
                    cleanup();
                    LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Problem sample_rate != 48000");
                    return -1;
                }

                // Audio output variables
                outputChannelCount = 8;
                outputChannelLayout = AV_CH_LAYOUT_7POINT1;
                outputSampleRate = 48000;
                outputSampleFormat = AV_SAMPLE_FMT_S16;

                int inputChannelCount = audioCodecContext->channels > 1 ? audioCodecContext->channels : 8;
                uint64_t inputChannelLayout = audioCodecContext->channel_layout;
                if(audioCodecContext->channels == 1 || audioCodecContext->channels == 8)
                    inputChannelLayout = AV_CH_LAYOUT_7POINT1;
                else if(audioCodecContext->channels == 2)
                    inputChannelLayout = AV_CH_LAYOUT_STEREO;
                else if(audioCodecContext->channels == 4)
                    inputChannelLayout = AV_CH_LAYOUT_3POINT1;
                AVSampleFormat inputSampleFormat = audioCodecContext->channels > 1 ? audioCodecContext->sample_fmt : av_get_planar_sample_fmt(audioCodecContext->sample_fmt);

                // Audio resample context
                swrContext = swr_alloc();
                av_opt_set_int(swrContext, "in_channel_count",  inputChannelCount, 0);
                av_opt_set_int(swrContext, "out_channel_count",  outputChannelCount, 0);
                av_opt_set_int(swrContext, "in_channel_layout",  inputChannelLayout, 0);
                av_opt_set_int(swrContext, "out_channel_layout", outputChannelLayout,  0);
                av_opt_set_int(swrContext, "in_sample_rate",     audioCodecContext->sample_rate, 0);
                av_opt_set_int(swrContext, "out_sample_rate",    outputSampleRate, 0);
                av_opt_set_sample_fmt(swrContext, "in_sample_fmt",  inputSampleFormat, 0);
                av_opt_set_sample_fmt(swrContext, "out_sample_fmt", outputSampleFormat,  0);

                if(swr_init(swrContext) < 0)
                {
                    cleanup();
                    LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Error initializing resampling context");
                    return -1;
                }

#ifdef PORTAUDIO
                swrContextPreview = swr_alloc();
                av_opt_set_int(swrContextPreview, "in_channel_count",  inputChannelCount, 0);
                av_opt_set_int(swrContextPreview, "out_channel_count",  2, 0);
                av_opt_set_int(swrContextPreview, "in_channel_layout",  inputChannelLayout, 0);
                av_opt_set_int(swrContextPreview, "out_channel_layout", AV_CH_LAYOUT_STEREO,  0);
                av_opt_set_int(swrContextPreview, "in_sample_rate",     audioCodecContext->sample_rate, 0);
                av_opt_set_int(swrContextPreview, "out_sample_rate",    outputSampleRate, 0);
                av_opt_set_sample_fmt(swrContextPreview, "in_sample_fmt",  inputSampleFormat, 0);
                av_opt_set_sample_fmt(swrContextPreview, "out_sample_fmt", outputSampleFormat,  0);

                if(swr_init(swrContextPreview) < 0)
                {
                    cleanup();
                    LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Error initializing preview resampling context");
                    return -1;
                }
#endif
            }
        }
    }

    // Create audio and video frames
    this->createFrames(format);

    return duration_ms;
}

const bool FFDecoder::initFilters(const QString& cg, const QString& format)
{
    QString overlay = videoCodecContext->height == 608 ? "0:32" : "0:0";

    QString filter = "movie=cg/" + cg + " [watermark]; [in][watermark] overlay=" + overlay + ",format=pix_fmts=" + QString::number(videoCodecContext->pix_fmt) + " [out]";

    char args[512];
    AVFilter* buffersrc  = avfilter_get_by_name("buffer");
    AVFilter* buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs  = avfilter_inout_alloc();
    filterGraph = avfilter_graph_alloc();

    // Create the source buffer to filter input frames
    _snprintf_s(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             videoStream->codecpar->width, videoStream->codecpar->height, videoStream->codecpar->format,
             videoStream->time_base.num, videoStream->time_base.den,
             videoStream->codecpar->sample_aspect_ratio.num, videoStream->codecpar->sample_aspect_ratio.den);

    if(avfilter_graph_create_filter(&buffersrcContext, buffersrc, "in", args, NULL, filterGraph) < 0)
    {
        LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Cannot create buffer source");
        cleanupFilters();
        return false;
    }

    // Create the sink buffer to output the filtered frames
    enum AVPixelFormat pix_fmts[] = { videoCodecContext->pix_fmt, AV_PIX_FMT_NONE };
    AVBufferSinkParams* buffersink_params;
    buffersink_params = av_buffersink_params_alloc();
    buffersink_params->pixel_fmts = pix_fmts;

    if (avfilter_graph_create_filter(&buffersinkContext, buffersink, "out", NULL, buffersink_params, filterGraph) < 0)
    {
        LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Cannot create buffer sink");
        av_free(buffersink_params);
        cleanupFilters();
        return false;
    }
    av_free(buffersink_params);

    // Endpoints for the filter graph
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrcContext;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersinkContext;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if(avfilter_graph_parse_ptr(filterGraph, filter.toStdString().c_str(), &inputs, &outputs, NULL) < 0)
    {
        LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Cannot parse filter graph");
        cleanupFilters();
        return false;
    }

    if(avfilter_graph_config(filterGraph, NULL) < 0)
    {
        LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Cannot config filter graph");
        cleanupFilters();
        return false;
    }

    // Allocate video frame for parsing the filtergraph
    filterFrame = av_frame_alloc();
    if(format != "720p50" && format != "720p5994")
    {
        filterFrame->interlaced_frame = 1;
        filterFrame->top_field_first = 1;
    }

    return true;
}

void FFDecoder::startDecoding()
{
    decoding = true;
}

int FFDecoder::convertOutput(SwrContext* swrContext, const uint8_t** inputSamples, const int& input_nb_samples, int nb_channels, uint8_t*** outputBuffer)
{
    int64_t dst_nb_samples = av_rescale_rnd(swr_get_delay(swrContext, audioCodecContext->sample_rate) + frameAudio->nb_samples, outputSampleRate, audioCodecContext->sample_rate, AV_ROUND_UP);

    int line_size;
    av_samples_alloc_array_and_samples(outputBuffer, &line_size, nb_channels, dst_nb_samples, outputSampleFormat, 1);

    int ret = swr_convert(swrContext, *outputBuffer, dst_nb_samples, (const uint8_t**)inputSamples, input_nb_samples);

    return av_samples_get_buffer_size(NULL, nb_channels, ret, outputSampleFormat, 1);
}

#ifdef PORTAUDIO
void FFDecoder::pushAudioFrame(const double& sr, const int& data_size, uint8_t** outputBuffer, const int& data_size_preview, uint8_t** previewBuffer)
#else
void FFDecoder::pushAudioFrame(const double& sr, const int& data_size, uint8_t** outputBuffer)
#endif
{
    double pts = (double)data_size / sr;

    int64_t diff = pts * 1000.0 + 0.5; // 0.5 is to round up

    AVDecodedFrame* a = new AVDecodedFrame(AVMEDIA_TYPE_AUDIO, outputBuffer[0], data_size, diff, diff);
    AVDecodedFrame* aVU = new AVDecodedFrame(AVMEDIA_TYPE_AUDIO, outputBuffer[0], data_size, diff, diff);

#ifdef PORTAUDIO
    pts = (double)data_size_preview / sr;
    diff = pts * 1000.0 + 0.5;
    AVDecodedFrame* ap = new AVDecodedFrame(AVMEDIA_TYPE_AUDIO, previewBuffer[0], data_size_preview, diff, diff);
#endif

    LOG4CXX_TRACE(Logger::getLogger("FFDecoder"), "AUDIO -> Start push");

    bool pushed = false;
    while(!(pushed = player->pushAudioFrame(a)) && decoding)
        this->usleep(10);

    if(!pushed && !decoding)
    {
        if(a != NULL)
            delete a;
    }

    LOG4CXX_TRACE(Logger::getLogger("FFDecoder"), "AUDIO -> pushAudioFrame");

    while(!(pushed = player->pushAudioFrameVU(aVU)) && decoding)
        this->usleep(10);

    if(!pushed && !decoding)
    {
        if(aVU != NULL)
            delete aVU;
    }

    LOG4CXX_TRACE(Logger::getLogger("FFDecoder"), "AUDIO -> pushAudioFrameVU");

#ifdef PORTAUDIO
    while(!(pushed = player->pushAudioFramePreview(ap)) && decoding)
        this->usleep(10);

    if(!pushed && !decoding)
    {
        if(ap != NULL)
            delete ap;
    }
#endif

    LOG4CXX_TRACE(Logger::getLogger("FFDecoder"), "AUDIO -> pushAudioFramePreview");

    LOG4CXX_TRACE(Logger::getLogger("FFDecoder"), "AUDIO -> End push");
}

void FFDecoder::run()
{
    if(avFormatContext != NULL)
    {
        videoTB = videoStream == NULL ? 0 : av_q2d(videoStream->time_base);
        fpsx2 = fps * 2.0;
        lastPTS = 0.0;
        firstDecodedFrame = true;
        totalFrames = 0;

        sr = (double)(outputChannelCount * 2 * outputSampleRate);
        bytes_per_sample = av_get_bytes_per_sample(outputSampleFormat) * outputChannelCount;

        inputSamples = NULL;
        input_linesize = 0;
        input_nb_samples = 0;

        swrContextMono = NULL;

        if(audioStream != NULL)
        {
            if(audioCodecContext->channels == 1)
            {
                swrContextMono = swr_alloc();

                av_opt_set_int(swrContextMono, "in_channel_count",  1, 0);
                av_opt_set_int(swrContextMono, "out_channel_count",  1, 0);
                av_opt_set_int(swrContextMono, "in_channel_layout",  AV_CH_LAYOUT_MONO, 0);
                av_opt_set_int(swrContextMono, "out_channel_layout", AV_CH_LAYOUT_MONO,  0);
                av_opt_set_int(swrContextMono, "in_sample_rate",     audioCodecContext->sample_rate, 0);
                av_opt_set_int(swrContextMono, "out_sample_rate",    audioCodecContext->sample_rate, 0);
                av_opt_set_sample_fmt(swrContextMono, "in_sample_fmt",  audioCodecContext->sample_fmt, 0);
                av_opt_set_sample_fmt(swrContextMono, "out_sample_fmt", audioCodecContext->sample_fmt,  0);

                swr_init(swrContextMono);
            }
        }

        last_audio_index = 0;
        index = 0;

        while(decoding)
        {
            AVPacket packet = { 0 };

            int ret = av_read_frame(avFormatContext, &packet);
            if(ret >= 0)
            {
                if(videoStream != NULL && packet.stream_index == videoStream->index)
                {
                    decodeVideo(&packet);
                }
                else if(audioStream != NULL)
                {
                    if(avFormatContext->streams[packet.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                        decodeAudio(&packet);
                }
            }
            else
            {
                if(ret == AVERROR_EOF || ret == AVERROR(EIO) || (videoStream != NULL && totalFrames >= videoStream->duration))
                {
                    if(videoStream != NULL && packet.stream_index == videoStream->index)
                    {
                        decodeVideo(&packet);
                    }
                    else if(audioStream != NULL)
                    {
                        if(avFormatContext->streams[packet.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                            decodeAudio(&packet);
                    }
                }

                while(!player->pushVideoFrame(NULL) && decoding)
                    this->usleep(10);
                while(!player->pushVideoFramePreview(NULL) && decoding)
                    this->usleep(10);
                while(!player->pushAudioFrame(NULL) && decoding)
                    this->usleep(10);
                while(!player->pushAudioFrameVU(NULL) && decoding)
                    this->usleep(10);
#ifdef PORTAUDIO
                while(!player->pushAudioFramePreview(NULL) && decoding)
                    this->usleep(10);
#endif

                if(!loop)
                    decoding = false;
                else
                {
                    lastPTS = 0.0;
                    totalFrames = 0;
                    seek(0, AVSEEK_FLAG_BACKWARD);
                }
            }

            av_packet_unref(&packet);
        }

        if(audioSamplesList.empty() && inputSamples != NULL)
        {
            av_freep(&inputSamples[0]);
            av_freep(&inputSamples);
        }
        inputSamples = NULL;

        while(!audioSamplesList.empty())
        {
            AudioSampleArray* sampleArray = audioSamplesList.takeFirst();
            inputSamples = sampleArray->getSamples();

            if(inputSamples != NULL)
            {
                av_freep(&inputSamples[0]);
                av_freep(&inputSamples);
            }

            delete sampleArray;
            inputSamples = NULL;
        }

        if(swrContextMono != NULL)
            swr_free(&swrContextMono);
        swrContextMono = NULL;
    }
}

void FFDecoder::decodeVideo(AVPacket* packet)
{
    int ret;
    ret = avcodec_send_packet(videoCodecContext, packet);
    if (ret < 0)
    {
        LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Error sending video packet for decoding");
        return;
    }

    while(ret >= 0)
    {
        ret = avcodec_receive_frame(videoCodecContext, tmpFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0)
        {
            LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Error during decoding video packet");
            return;
        }

        totalFrames++;
        double pts = 0.0;

        if(packet->dts != AV_NOPTS_VALUE)
            pts = av_frame_get_best_effort_timestamp(tmpFrame);
        else pts = 0;

        pts -= avFormatContext->start_time;
        pts *= videoTB;
        pts += tmpFrame->repeat_pict * fpsx2;

        int64_t diff = (pts - lastPTS) * 1000.0 + 0.5; // 0.5 is to round up
        if(lastPTS == 0.0)
            diff = 1000.0 / fps;

        diff /= rate;
        lastPTS = pts;

        // Add the frame to the filtergraph
        if(filterFrame != NULL)
            av_buffersrc_add_frame_flags(buffersrcContext, tmpFrame, AV_BUFFERSRC_FLAG_KEEP_REF);

        while(true)
        {
            AVFrame* scaleFrame = tmpFrame;
            if(filterFrame != NULL)
            {
                scaleFrame = filterFrame;

                if(av_buffersink_get_frame(buffersinkContext, filterFrame) < 0)
                    break;
            }

#if defined(DECKLINK) || defined(GUI)
            double pts_ms = pts * 1000;
#endif
            AVDecodedFrame* v = NULL;

#ifdef DECKLINK
            sws_scale(swsContext, scaleFrame->data, scaleFrame->linesize, 0, videoCodecContext->height, frameUYVY->data, frameUYVY->linesize);
            v = new AVDecodedFrame(AVMEDIA_TYPE_VIDEO, frameUYVY->data[0], outputFrameSize, diff, pts_ms);
#endif

            AVDecodedFrame* vp = NULL;
#ifdef GUI
            sws_scale(swsContextPreview, scaleFrame->data, scaleFrame->linesize, 0, videoCodecContext->height, framePreview->data, framePreview->linesize);
            vp = new AVDecodedFrame(AVMEDIA_TYPE_VIDEO, framePreview->data[0] + previewOffset, previewFrameSize, diff, pts_ms);
#endif

            if(firstDecodedFrame)
            {
                player->setRecueBuffers(v, vp);
                firstDecodedFrame = false;
            }

            LOG4CXX_TRACE(Logger::getLogger("FFDecoder"), "VIDEO -> Start push");

            bool pushed = false;

            while(!(pushed = player->pushVideoFrame(v)) && decoding)
                this->usleep(10);

            if(!pushed && !decoding)
            {
                if(v != NULL)
                    delete v;
            }

            LOG4CXX_TRACE(Logger::getLogger("FFDecoder"), "VIDEO -> pushVideoFrame");

            while(!(pushed = player->pushVideoFramePreview(vp)) && decoding)
                this->usleep(10);

            if(!pushed && !decoding)
            {
                if(vp != NULL)
                    delete vp;
            }

            LOG4CXX_TRACE(Logger::getLogger("FFDecoder"), "VIDEO -> pushVideoFramePreview");

            LOG4CXX_TRACE(Logger::getLogger("FFDecoder"), "VIDEO -> End push");

            if(filterFrame != NULL)
                av_frame_unref(filterFrame);
            else break;
        }

        av_frame_unref(tmpFrame);
    }
}

void FFDecoder::decodeAudio(AVPacket* packet)
{
    int ret;
    ret = avcodec_send_packet(audioCodecContext, packet);
    if (ret < 0)
    {
        LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Error sending audio packet for decoding");
        return;
    }

    while(ret >= 0)
    {
        ret = avcodec_receive_frame(audioCodecContext, frameAudio);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0)
        {
            LOG4CXX_ERROR(Logger::getLogger("FFDecoder"), "Error during decoding audio packet");
            return;
        }

        if(rate == 1.0)
        {
            if(packet->stream_index == audioStream->index)
            {
                uint8_t** outputBuffer = NULL;
                int data_size = 0;

                if(audioCodecContext->channels > 1)
                {
                    if(frameAudio->format != outputSampleFormat || audioCodecContext->channels != outputChannelCount)
                        data_size = convertOutput(swrContext, (const uint8_t**)frameAudio->extended_data, frameAudio->nb_samples, outputChannelCount, &outputBuffer);
                    else
                    {
                        data_size = frameAudio->nb_samples * bytes_per_sample;
                        outputBuffer = frameAudio->extended_data;
                    }
#ifdef PORTAUDIO
                    uint8_t** previewBuffer = NULL;
                    int data_size_preview = convertOutput(swrContextPreview, (const uint8_t**)frameAudio->extended_data, frameAudio->nb_samples, 2, &previewBuffer);
                    pushAudioFrame(sr, data_size, outputBuffer, data_size_preview, previewBuffer);

                    av_freep(&previewBuffer[0]);
                    av_freep(&previewBuffer);
#else
                    pushAudioFrame(sr, data_size, outputBuffer);
#endif

                    if(frameAudio->format != outputSampleFormat || audioCodecContext->channels != outputChannelCount)
                    {
                        av_freep(&outputBuffer[0]);
                        av_freep(&outputBuffer);
                    }
                }
                else if(swrContextMono != NULL)
                {
                    if(last_audio_index != packet->stream_index)
                        index = 0;

                    if(last_audio_index == packet->stream_index)
                    {
                        audioSamplesList.append(new AudioSampleArray(inputSamples, input_linesize, input_nb_samples));
                        inputSamples = NULL;
                    }

                    if(inputSamples == NULL)
                    {
                        av_samples_alloc_array_and_samples(&inputSamples, &input_linesize, outputChannelCount, frameAudio->nb_samples, av_get_planar_sample_fmt(audioCodecContext->sample_fmt), 1);
                        input_nb_samples = input_linesize / av_get_bytes_per_sample(audioCodecContext->sample_fmt);
                        swr_convert(swrContextMono, &inputSamples[packet->stream_index - audioStream->index], input_linesize, (const uint8_t**)frameAudio->extended_data, frameAudio->nb_samples);
                    }
                    else
                    {
                        while(!audioSamplesList.empty())
                        {
                            AudioSampleArray* sampleArray = audioSamplesList.takeFirst();
                            inputSamples = sampleArray->getSamples();
                            input_linesize = sampleArray->getLinesize();
                            input_nb_samples = sampleArray->getNbSamples();

                            data_size = convertOutput(swrContext, (const uint8_t**)inputSamples, input_nb_samples, outputChannelCount, &outputBuffer);

#ifdef PORTAUDIO
                            uint8_t** previewBuffer = NULL;
                            int data_size_preview = convertOutput(swrContextPreview, (const uint8_t**)inputSamples, input_nb_samples, 2, &previewBuffer);
                            pushAudioFrame(sr, data_size, outputBuffer, data_size_preview, previewBuffer);

                            av_freep(&previewBuffer[0]);
                            av_freep(&previewBuffer);
#else
                            pushAudioFrame(sr, data_size, outputBuffer);
#endif

                            av_freep(&outputBuffer[0]);
                            av_freep(&outputBuffer);

                            av_freep(&inputSamples[0]);
                            av_freep(&inputSamples);

                            delete sampleArray;
                            inputSamples = NULL;
                        }

                        av_samples_alloc_array_and_samples(&inputSamples, &input_linesize, outputChannelCount, frameAudio->nb_samples, av_get_planar_sample_fmt(audioCodecContext->sample_fmt), 1);
                        input_nb_samples = input_linesize / av_get_bytes_per_sample(audioCodecContext->sample_fmt);
                        swr_convert(swrContextMono, &inputSamples[packet->stream_index - audioStream->index], input_linesize, (const uint8_t**)frameAudio->extended_data, frameAudio->nb_samples);
                    }
                }
            }
            else
            {
                if(audioCodecContext->channels == 1 && swrContextMono != NULL)
                {
                    if(last_audio_index != packet->stream_index)
                        index = 0;

                    if(last_audio_index == audioStream->index)
                        audioSamplesList.append(new AudioSampleArray(inputSamples, input_linesize, input_nb_samples));

                    if(last_audio_index == packet->stream_index || index == 0)
                    {
                        inputSamples = NULL;

                        if(!audioSamplesList.empty() && index < audioSamplesList.size())
                        {
                            AudioSampleArray* sampleArray = audioSamplesList.at(index++);
                            inputSamples = sampleArray->getSamples();
                            input_linesize = sampleArray->getLinesize();
                            input_nb_samples = sampleArray->getNbSamples();
                        }

                        if(inputSamples == NULL)
                        {
                            av_samples_alloc_array_and_samples(&inputSamples, &input_linesize, outputChannelCount, frameAudio->nb_samples, av_get_planar_sample_fmt(audioCodecContext->sample_fmt), 1);
                            input_nb_samples = input_linesize / av_get_bytes_per_sample(audioCodecContext->sample_fmt);
                        }
                    }

                    if(inputSamples != NULL)
                        swr_convert(swrContextMono, &inputSamples[packet->stream_index - audioStream->index], input_linesize, (const uint8_t**)frameAudio->extended_data, frameAudio->nb_samples);
                }
            }

            last_audio_index = packet->stream_index;
        }
    }
}

void FFDecoder::stopDecoding()
{
    decoding = false;
    this->wait();
}

void FFDecoder::seek(const int64_t pos, const int seek_flag)
{
    if(videoStream != NULL)
    {
        AVRational tb;
        tb.den = 1000;
        tb.num = 1;

        av_seek_frame(avFormatContext, videoStream->index, av_rescale_q(pos, tb, videoStream->time_base), seek_flag);
    }

    for(int i=0; i<codecContextList.count(); i++)
    {
        avcodec_flush_buffers(codecContextList[i]);
    }

    avio_flush(avFormatContext->pb);
    avformat_flush(avFormatContext);
}

void FFDecoder::cleanup()
{
    cleanupFilters();

#ifdef PORTAUDIO
    if(swrContextPreview != NULL)
        swr_free(&swrContextPreview);
    swrContextPreview = NULL;
#endif

    if(swrContext != NULL)
        swr_free(&swrContext);
    swrContext = NULL;

#ifdef GUI
    if(swsContextPreview != NULL)
        sws_freeContext(swsContextPreview);
    swsContextPreview = NULL;

    if(framePreview != NULL)
        av_frame_free(&framePreview);
    framePreview = NULL;
#endif

    if(swsContext != NULL)
        sws_freeContext(swsContext);
    swsContext = NULL;

    if(frameUYVY != NULL)
        av_frame_free(&frameUYVY);
    frameUYVY = NULL;

    if(tmpFrame != NULL)
        av_frame_free(&tmpFrame);
    tmpFrame = NULL;

    if(frameAudio != NULL)
        av_frame_free(&frameAudio);
    frameAudio = NULL;

    if(avFormatContext != NULL)
    {
        for(int i=0; i<codecContextList.count(); i++)
        {
            avcodec_free_context(&codecContextList[i]);
        }
        codecContextList.clear();

        avformat_close_input(&avFormatContext);
        avformat_free_context(avFormatContext);
    }
    avFormatContext = NULL;
    audioStream = NULL;
    videoStream = NULL;
}

void FFDecoder::cleanupFilters()
{
    if(filterGraph != NULL)
        avfilter_graph_free(&filterGraph);
    filterGraph = NULL;

    if(filterFrame != NULL)
        av_frame_free(&filterFrame);
    filterFrame = NULL;
}
