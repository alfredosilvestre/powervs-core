#include "videoencoder.h"

#include <QStringList>

extern "C"
{
    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
}

#include <log4cxx/logger.h>

using namespace log4cxx;

const uint16_t default_intra_matrix[] = {
  8, 16, 19, 22, 26, 27, 29, 34,
  16, 16, 22, 24, 27, 29, 34, 37,
  19, 22, 26, 27, 29, 34, 34, 38,
  22, 22, 26, 27, 29, 34, 37, 40,
  22, 26, 27, 29, 32, 35, 40, 48,
  26, 27, 29, 32, 35, 40, 48, 58,
  26, 27, 29, 34, 38, 46, 56, 69,
  27, 29, 35, 38, 46, 56, 69, 83
};

const uint16_t imx30_intra_matrix[] = {
  32, 16, 16, 17, 19, 22, 26, 34,
  16, 16, 18, 19, 21, 26, 33, 37,
  19, 18, 19, 23, 21, 28, 35, 45,
  18, 19, 20, 22, 28, 35, 32, 49,
  18, 20, 23, 29, 32, 33, 44, 45,
  18, 20, 20, 25, 35, 39, 52, 58,
  20, 22, 23, 28, 31, 44, 51, 57,
  19, 21, 26, 30, 38, 48, 45, 75
};

VideoEncoder::VideoEncoder()
{
    outputContext = NULL;
    videoStream = NULL;
    codecContext = NULL;

    videoWidth = 0;
    videoHeight = 0;

    inputPixFmt = AV_PIX_FMT_NONE;
    outputPixFmt = AV_PIX_FMT_NONE;

    frame = NULL;
    tmpFrame = NULL;
    swsContext = NULL;

    frameSize = 0;
}

VideoEncoder::~VideoEncoder()
{
    cleanup();
}

double VideoEncoder::initialize(QString format, AVFormatContext* outputContext)
{
    this->outputContext = outputContext;

    AVCodec* codec = NULL;
    if(format.contains("imx") || format.contains("xdcam"))
        codec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
    else return -1;

    videoStream = avformat_new_stream(outputContext, codec);
    codecContext = avcodec_alloc_context3(codec);
    videoStream->id = outputContext->nb_streams-1;

    frame = av_frame_alloc();
    tmpFrame = av_frame_alloc();
    inputPixFmt = AV_PIX_FMT_UYVY422;

    if(format.contains("imx"))
    {
        if(format.contains("imx30"))
            initIMX30();
        else if(format.contains("imx50"))
            initIMX50();
        else return -1;

        AVRational b;
        if(format.contains("4:3"))
        {
            b.den = 3;
            b.num = 4;
        }
        else
        {
            b.den = 9;
            b.num = 16;
        }

        AVRational c;
        c.den = codecContext->width;
        c.num = codecContext->height;

        videoStream->sample_aspect_ratio = av_mul_q(b, c);
        codecContext->sample_aspect_ratio = videoStream->sample_aspect_ratio;
    }
    else if(format.contains("xdcamHD422"))
    {
        QStringList split = format.split(" ");

        if(split.size() == 2)
        {
            QStringList split2 = split[0].split("_");
            if(split2.size() == 2)
            {
                if(split2[1] == "720p")
                    initXDCAMHD422_720p(split[1].toInt());
                else if(split2[1] == "1080p")
                    initXDCAMHD422_1080p(split[1].toInt());
                else if(split2[1] == "1080i")
                    initXDCAMHD422_1080i(split[1].toInt());
                else return -1;

                AVRational b;
                b.den = 9;
                b.num = 16;

                AVRational c;
                c.den = codecContext->width;
                c.num = codecContext->height;

                videoStream->sample_aspect_ratio = av_mul_q(b, c);
                codecContext->sample_aspect_ratio = videoStream->sample_aspect_ratio;
            }
            else return -1;
        }
        else return -1;
    }

    frame->format = codecContext->pix_fmt;
    frame->width  = codecContext->width;
    frame->height = codecContext->height;
    av_frame_get_buffer(frame, 32);

    tmpFrame->format = inputPixFmt;
    tmpFrame->width  = codecContext->width;
    tmpFrame->height = codecContext->height;
    av_frame_get_buffer(tmpFrame, 32);

    swsContext = sws_getContext(tmpFrame->width, tmpFrame->height, (AVPixelFormat)tmpFrame->format,
                            frame->width, frame->height, (AVPixelFormat)frame->format,
                            SWS_BICUBIC, NULL, NULL, NULL);

    frameSize = av_image_get_buffer_size((AVPixelFormat)tmpFrame->format, tmpFrame->width, tmpFrame->height, 1);

    // Some formats want stream headers to be separate.
    if (outputContext->oformat->flags & AVFMT_GLOBALHEADER)
        codecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

    // Open video codec
    avcodec_parameters_from_context(videoStream->codecpar, codecContext);
    if(avcodec_open2(codecContext, codec, NULL) < 0)
        return false;

    return av_q2d(videoStream->time_base) * 1000;;
}

void VideoEncoder::initIMX()
{
    // PAL
    videoWidth = 720;
    videoHeight = 608;

    codecContext->width = videoWidth;
    codecContext->height = videoHeight;

    // -r 25
    codecContext->time_base.den = videoStream->time_base.den = 25;
    codecContext->time_base.num = videoStream->time_base.num = 1;

    // -pix_fmt yuv422p
    outputPixFmt = codecContext->pix_fmt = AV_PIX_FMT_YUV422P;

    // -g 1
    codecContext->gop_size = 1;

    // -flags +ildct+low_delay
    codecContext->flags |= CODEC_FLAG_INTERLACED_DCT | CODEC_FLAG_LOW_DELAY;

    // -dc 10 (10-8 = 2)
    codecContext->intra_dc_precision = 2;

    // -intra_vlc 1
    av_opt_set(codecContext->priv_data, "intra_vlc", "1", 0);
    // -non_linear_quant 1
    av_opt_set(codecContext->priv_data, "non_linear_quant", "1", 0);

    // -ps 1
    av_opt_set(codecContext->priv_data, "ps", "1", 0);

    // -qmin 1
    codecContext->qmin = 1;

    // -rc_buf_aggressivity 0.25
    av_opt_set(codecContext->priv_data, "rc_buf_aggressivity", "0.25", 0);

    // -lmin "1*QP2LAMBDA"
    int lmin = 1 * FF_QP2LAMBDA;
    av_opt_set(codecContext->priv_data, "lmin", QString::number(lmin).toStdString().c_str(), 0);

    // -rc_max_vbv_use 1 -rc_min_vbv_use 1
    codecContext->rc_max_available_vbv_use = 1;
    codecContext->rc_min_vbv_overflow_use = 1;

    codecContext->color_primaries = AVCOL_PRI_BT470BG;
    codecContext->color_trc = AVCOL_TRC_BT709;
    codecContext->colorspace = AVCOL_SPC_SMPTE170M;

    // -bf 0
    codecContext->max_b_frames = 0;

    // Avid uses special intra quantiser matrix
    //codecContext->intra_matrix = (uint16_t *)av_mallocz(sizeof(uint16_t) * 64);
    //memcpy(codecContext->intra_matrix, imx30_intra_matrix, sizeof(uint16_t) * 64);

    frame->interlaced_frame = 1;
    frame->top_field_first = 1;
	
    codecContext->thread_count = 2;
}

void VideoEncoder::initIMX30()
{
    initIMX();

    // -vtag mx3p
    codecContext->codec_tag = MKTAG('m','x','3','p'); // IMX30

    // -qmax 8
    codecContext->qmax = 8;

    // -b:v 30000k -minrate 30000k -maxrate 30000k
    codecContext->bit_rate = codecContext->rc_min_rate = codecContext->rc_max_rate = 30000000;

    // -bufsize 1200000 -rc_init_occupancy 1200000
    codecContext->rc_buffer_size = codecContext->rc_initial_buffer_occupancy = 1200000;
}

void VideoEncoder::initIMX50()
{
    initIMX();

    // -vtag mx5p
    codecContext->codec_tag = MKTAG('m','x','5','p'); // IMX50

    // -qmax 3
    codecContext->qmax = 3;

    // -b:v 50000k -minrate 50000k -maxrate 50000k
    codecContext->bit_rate = codecContext->rc_min_rate = codecContext->rc_max_rate = 50000000;

    // -bufsize 2000000 -rc_init_occupancy 2000000
    codecContext->rc_buffer_size = codecContext->rc_initial_buffer_occupancy = 2000000;
}

void VideoEncoder::initXDCAMHD422()
{
    // -pix_fmt yuv422p
    outputPixFmt = codecContext->pix_fmt = AV_PIX_FMT_YUV422P;

    // -g 12
    codecContext->gop_size = 12;

    // -dc 10 (10-8 = 2)
    codecContext->intra_dc_precision = 2;

    // -intra_vlc 1
    av_opt_set(codecContext->priv_data, "intra_vlc", "1", 0);
    // -non_linear_quant 1
    av_opt_set(codecContext->priv_data, "non_linear_quant", "1", 0);

    // -qmin 1
    codecContext->qmin = 1;

    // -qmax 12
    codecContext->qmax = 12;

    // -lmin "1*QP2LAMBDA"
    int lmin = 1 * FF_QP2LAMBDA;
    av_opt_set(codecContext->priv_data, "lmin", QString::number(lmin).toStdString().c_str(), 0);

    // -rc_max_vbv_use 1 -rc_min_vbv_use 1
    codecContext->rc_max_available_vbv_use = 1;
    codecContext->rc_min_vbv_overflow_use = 1;

    // -b:v 50000k -minrate 50000k -maxrate 50000k
    codecContext->bit_rate = codecContext->rc_min_rate = codecContext->rc_max_rate = 50000000;

    // -bufsize 17825792 -rc_init_occupancy 17825792
    codecContext->rc_buffer_size = codecContext->rc_initial_buffer_occupancy = 17825792;

    // -bf 2
    codecContext->max_b_frames = 2;

    // -b_strategy 1
    av_opt_set(codecContext->priv_data, "b_strategy", "1", 0);

    // -sc_threshold 1000000000
    av_opt_set(codecContext->priv_data, "sc_threshold", "1000000000", 0);

    codecContext->color_primaries = AVCOL_PRI_BT709;
    codecContext->color_trc = AVCOL_TRC_BT709;
    codecContext->colorspace = AVCOL_SPC_BT709;
	
    codecContext->thread_count = 4;
}

void VideoEncoder::initXDCAMHD422_720p(int rate)
{
    initXDCAMHD422();

    // 720
    videoWidth = 1280;
    videoHeight = 720;

    codecContext->width = videoWidth;
    codecContext->height = videoHeight;

    // -r 50
    codecContext->time_base.den = videoStream->time_base.den = rate;
    codecContext->time_base.num = videoStream->time_base.num = 1;

    // -r 60
    if(rate == 60)
    {
        codecContext->time_base.den = videoStream->time_base.den = 60000;
        codecContext->time_base.num = videoStream->time_base.num = 1001;
    }

    // -vtag xd5a
    if(rate == 50)
        codecContext->codec_tag = MKTAG('x','d','5','a'); // XDCAM HD422 720p50
    // -vtag xd59
    else if(rate == 60)
        codecContext->codec_tag = MKTAG('x','d','5','9'); // XDCAM HD422 720p60

}

void VideoEncoder::initXDCAMHD422_1080p(int rate)
{
    initXDCAMHD422();

    // 1080
    videoWidth = 1920;
    videoHeight = 1080;

    codecContext->width = videoWidth;
    codecContext->height = videoHeight;

    // -r 25
    codecContext->time_base.den = videoStream->time_base.den = rate;
    codecContext->time_base.num = videoStream->time_base.num = 1;

    // -vtag xd5e
    if(rate == 25)
        codecContext->codec_tag = MKTAG('x','d','5','e'); // XDCAM HD422 1080p25
}

void VideoEncoder::initXDCAMHD422_1080i(int rate)
{
    initXDCAMHD422();

    // 1080
    videoWidth = 1920;
    videoHeight = 1080;

    codecContext->width = videoWidth;
    codecContext->height = videoHeight;

    // -r 25
    codecContext->time_base.den = videoStream->time_base.den = rate;
    codecContext->time_base.num = videoStream->time_base.num = 1;

    // -r 30
    if(rate == 30)
    {
        codecContext->time_base.den = videoStream->time_base.den = 30000;
        codecContext->time_base.num = videoStream->time_base.num = 1001;
    }

    // -vtag xd5c
    if(rate == 25)
        codecContext->codec_tag = MKTAG('x','d','5','c'); // XDCAM HD422 1080i50
    // -vtag xd5b
    else if(rate == 30)
        codecContext->codec_tag = MKTAG('x','d','5','b'); // XDCAM HD422 1080i60

    // -flags +ildct+ilme
    codecContext->flags |= CODEC_FLAG_INTERLACED_DCT | CODEC_FLAG_INTERLACED_ME;

    frame->interlaced_frame = 1;
    frame->top_field_first = 1;
}

double VideoEncoder::muxVideoFrame(AVDecodedFrame* videoBuffer)
{
    AVPacket pkt = { 0 };
    int ret;

    if(videoBuffer == NULL)
    {
        ret = avcodec_send_frame(codecContext, NULL);
        if (ret < 0)
            LOG4CXX_ERROR(Logger::getLogger("VideoEncoder"), "Error sending a video frame for encoding");
    }
    else
    {
        av_image_fill_arrays(tmpFrame->data, tmpFrame->linesize, videoBuffer->getBuffer(),
                       (AVPixelFormat)tmpFrame->format, tmpFrame->width, tmpFrame->height, 1);

        sws_scale(swsContext, tmpFrame->data, tmpFrame->linesize,
                  0, tmpFrame->height, frame->data, frame->linesize);

        ret = avcodec_send_frame(codecContext, frame);
        if (ret < 0 && ret != AVERROR_EOF)
            LOG4CXX_ERROR(Logger::getLogger("VideoEncoder"), "Error sending a video frame for encoding");
    }

    if(ret >= 0)
    {
        av_init_packet(&pkt);
        ret = avcodec_receive_packet(codecContext, &pkt);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        {
            LOG4CXX_ERROR(Logger::getLogger("VideoEncoder"), "Error during video frame encoding");
        }
        else if (ret >= 0)
        {
            pkt.stream_index = videoStream->index;

            pkt.pts = pkt.dts = codecContext->frame_number;
            av_packet_rescale_ts(&pkt, codecContext->time_base, videoStream->time_base);

            av_interleaved_write_frame(outputContext, &pkt);
            av_packet_unref(&pkt);
        }
    }

    return av_rescale_q(codecContext->frame_number, codecContext->time_base, videoStream->time_base);
}
void VideoEncoder::cleanup()
{
    if(codecContext != NULL)
        avcodec_free_context(&codecContext);
    codecContext = NULL;
    videoStream = NULL;

    outputContext = NULL;

    if(swsContext != NULL)
        sws_freeContext(swsContext);
    swsContext = NULL;

    if(tmpFrame != NULL)
        av_frame_free(&tmpFrame);
    tmpFrame = NULL;

    if(frame != NULL)
        av_frame_free(&frame);
    frame = NULL;

    videoWidth = 0;
    videoHeight = 0;

    inputPixFmt = AV_PIX_FMT_NONE;
    outputPixFmt = AV_PIX_FMT_NONE;

    frameSize = 0;
}

int VideoEncoder::getVideoCodecAddress()
{
    return (int)codecContext;
}
