#include "iobridge.h"

extern "C"
{
    #include <libavfilter/buffersrc.h>
    #include <libavfilter/buffersink.h>
    #include <libavutil/imgutils.h>
}

#include <log4cxx/logger.h>

using namespace log4cxx;

IOBridge::IOBridge(QObject *parent) :
    QObject(parent)
{
#ifdef GUI
    preview = NULL;
#endif
#ifdef DECKLINK
    deckLinkOutput = NULL;
    deckLinkInput = NULL;
#endif
    recorder = new Recorder(this);
    ioBridge = NULL;

    recording = false;

    width = 720;
    height = 576;
    videoOffset = 32 * width * 2;
    frameSize = height * width * 2;
    frameUYVY = NULL;
#ifdef GUI
    framePreview = NULL;
    previewWidth = 360;
    previewSize = 0;
    swsContextPreview = NULL;
#endif

    filterFrame = NULL;
    filterGraph = NULL;
    buffersinkContext = NULL;
    buffersrcContext = NULL;
    filterFPS = 50;

    currentMediaFormat = "PAL";
    currentCG = "";

    initFFMpeg();
}

IOBridge::~IOBridge()
{
    this->cleanup();
}

#ifdef GUI
void IOBridge::togglePreviewVideo(PreviewerRGB* previewer)
{
    if(this->preview != NULL)
    {
        this->preview->stopAudio();
        this->disconnect(this, SIGNAL(previewVideo(QByteArray)), preview, SLOT(previewVideo(QByteArray)));
        this->disconnect(this, SIGNAL(previewAudio(QByteArray)), preview, SLOT(previewAudio(QByteArray)));
    }

    this->preview = previewer;
    if(this->preview != NULL)
    {
        this->connect(this, SIGNAL(previewVideo(QByteArray)), preview, SLOT(previewVideo(QByteArray)));
        this->connect(this, SIGNAL(previewAudio(QByteArray)), preview, SLOT(previewAudio(QByteArray)));

        preview->changeFormat(currentMediaFormat);
    }
}
#endif

#ifdef DECKLINK
const QString IOBridge::setDeckLinkInput(QString inter)
{
    videoMutex.lock();
    audioMutex.lock();

    if(deckLinkInput != NULL)
    {
        deckLinkInput->setIOBridge(NULL);
        deckLinkInput->disableCard();
#ifdef GUI
        if(preview != NULL)
            preview->stopAudio();
#endif
    }
    deckLinkInput = NULL;

    if(inter.contains("Input"))
    {
        inter.remove(QRegExp(".*\\("));
        inter.remove(")");

        deckLinkInput = DeckLinkControl::getInput(inter.toInt());
        deckLinkInput->enableCard();
        deckLinkInput->setIOBridge(this);
    }

    audioMutex.unlock();
    videoMutex.unlock();

    return inter;
}

const DeckLinkInput* IOBridge::getDecklinkInput() const
{
    return deckLinkInput;
}

const QString IOBridge::setDeckLinkOutput(QString inter)
{
    videoMutex.lock();
    audioMutex.lock();

    if(deckLinkOutput != NULL)
        deckLinkOutput->disableCard();
    deckLinkOutput = NULL;

#ifdef GUI
    if(preview != NULL)
        preview->stopAudio();
#endif

    if(inter.contains("Output"))
    {
        inter.remove(QRegExp(".*\\("));
        inter.remove(")");

        deckLinkOutput = DeckLinkControl::getOutput(inter.toInt());
        deckLinkOutput->enableCard();
        deckLinkOutput->startPlayback(NULL);
    }

    audioMutex.unlock();
    videoMutex.unlock();

    return inter;
}

const DeckLinkOutput* IOBridge::getDecklinkOutput() const
{
    return deckLinkOutput;
}
#endif

const void IOBridge::setIOBridge(IOBridge* ioBridge)
{
    if(ioBridge != NULL)
        this->connect(this->parent(), SIGNAL(timecodeReceived(QString)), ioBridge->parent(), SLOT(updateTimecode(QString)));
    else if(this->ioBridge != NULL)
        this->disconnect(this->parent(), SIGNAL(timecodeReceived(QString)), this->ioBridge->parent(), SLOT(updateTimecode(QString)));

    this->ioBridge = ioBridge;
}

const void IOBridge::rebootInterface()
{
    if(ioBridge != NULL)
    {
#ifdef DECKLINK
        if(deckLinkInput != NULL)
            deckLinkInput->setIOBridge(NULL);
#endif
        ioBridge->rebootInterface();
#ifdef DECKLINK
        if(deckLinkInput != NULL)
            deckLinkInput->setIOBridge(this);
#endif
    }
    else
    {
#ifdef DECKLINK
    if(deckLinkOutput != NULL)
        deckLinkOutput->stopPlayback(true);
#endif
    }
}

const Recorder* IOBridge::getRecorder() const
{
    return recorder;
}

void IOBridge::changeFormat(const QString& format)
{
    videoMutex.lock();
    audioMutex.lock();

#ifdef DECKLINK
    if(deckLinkInput != NULL)
        deckLinkInput->setIOBridge(NULL);
#endif

    if(ioBridge != NULL)
        ioBridge->changeFormat(format);

    cleanupFFMpeg();

    currentMediaFormat = format;

#ifdef DECKLINK
    if(deckLinkInput != NULL)
        deckLinkInput->changeFormat(format);
    if(deckLinkOutput != NULL)
        deckLinkOutput->changeFormat(format);
#endif

#ifdef GUI
    if(preview != NULL)
        preview->changeFormat(format);
#endif

    if(recorder != NULL && !recorder->isRunning())
        recorder->changeFormat(format);

    if(format == "PAL" || format == "PAL 16:9")
    {
        width = 720;
        height = 576;
        filterFPS = 50;
    }
    else if(format == "720p50" || format == "720p5994")
    {
        width = 1280;
        height = 720;
        filterFPS = 25; // FIXME: needs testing
    }
    else if(format == "1080p25")
    {
        width = 1920;
        height = 1080;
        filterFPS = 25; // FIXME: needs testing
    }
    else if(format == "1080i50" || format == "1080i5994")
    {
        width = 1920;
        height = 1080;
        filterFPS = 50;
    }

    frameSize = height * width * 2;
#ifdef GUI
    if(format == "PAL")
        previewWidth = 360;
    else previewWidth = 512;
#endif

    if(format == "PAL" || format == "PAL 16:9")
        videoOffset = 32 * width * 2;
    else
        videoOffset = 0;

    initFFMpeg();

#ifdef DECKLINK
    if(deckLinkInput != NULL)
        deckLinkInput->setIOBridge(this);
#endif

    audioMutex.unlock();
    videoMutex.unlock();
}

bool IOBridge::isRecording() const
{
    return recorder->isRunning();
}

void IOBridge::startRecording()
{
#ifdef DECKLINK
    if(deckLinkInput != NULL)
        deckLinkInput->flushCard();
#endif
    recording = true;
}

void IOBridge::stopRecording()
{
    recording = false;
}

void IOBridge::clearBuffers()
{
    while(true)
    {
        AVDecodedFrame* buffer = getNextVideoFrame();
        if(buffer == NULL)
            break;
        delete buffer;
    }

    while(true)
    {
        AVDecodedFrame* buffer = getNextAudioSample();
        if(buffer == NULL)
            break;
        delete buffer;
    }
}

const void IOBridge::pushVideoFrame(uint8_t* v, const int frameSize)
{
    videoMutex.lock();

    if(frameSize == this->frameSize)
    {
        if(frameUYVY != NULL)
        {
            av_image_fill_arrays(frameUYVY->data, frameUYVY->linesize, (uint8_t*)v + videoOffset,
                           (AVPixelFormat)frameUYVY->format, frameUYVY->width, frameUYVY->height, 1);

            if(filterFrame != NULL)
            {
                frameUYVY->pts = 0;
                av_buffersrc_add_frame_flags(buffersrcContext, frameUYVY, AV_BUFFERSRC_FLAG_KEEP_REF);
            }

            AVFrame* scaleFrame = frameUYVY;
            if(filterFrame != NULL)
            {
                scaleFrame = filterFrame;

                if(av_buffersink_get_frame(buffersinkContext, filterFrame) < 0)
                    scaleFrame = frameUYVY;
                else memcpy(v + videoOffset, filterFrame->data[0], frameSize);

                av_frame_unref(filterFrame);
            }
#ifdef GUI
            sws_scale(swsContextPreview, scaleFrame->data, scaleFrame->linesize, 0, height, framePreview->data, framePreview->linesize);

            emit previewVideo(QByteArray((char*)framePreview->data[0], previewSize));
#endif

    #ifdef DECKLINK
            if(deckLinkOutput != NULL)
                deckLinkOutput->outputVideo(v, frameSize + videoOffset, false, 0);
    #endif
            if(recording)
                videoFramesList.append(new AVDecodedFrame(AVMEDIA_TYPE_VIDEO, v, frameSize + videoOffset, 0, 0));

            if(ioBridge != NULL)
                ioBridge->pushVideoFrame(v, frameSize);
        }
    }

    videoMutex.unlock();
}

const void IOBridge::pushAudioFrame(uint8_t* a, const int audioSize)
{
    audioMutex.lock();

#ifdef DECKLINK
    if(deckLinkOutput != NULL)
        deckLinkOutput->outputAudio(a, audioSize);
#endif

    if(ioBridge != NULL)
        ioBridge->pushAudioFrame(a, audioSize);

    if(recording)
        audioSamplesList.append(new AVDecodedFrame(AVMEDIA_TYPE_AUDIO, a, audioSize, 0, 0));

#ifdef GUI
    emit previewAudio(QByteArray((char*)a, audioSize));
#endif

    audioMutex.unlock();
}

AVDecodedFrame* IOBridge::getNextVideoFrame()
{
    videoMutex.lock();

    if(!videoFramesList.isEmpty())
    {
        AVDecodedFrame* buffer = videoFramesList.takeFirst();
        videoMutex.unlock();
        return buffer;
    }

    videoMutex.unlock();
    return NULL;
}

AVDecodedFrame* IOBridge::getNextAudioSample()
{
    audioMutex.lock();

    if(!audioSamplesList.isEmpty())
    {
        AVDecodedFrame* buffer = audioSamplesList.takeFirst();
        audioMutex.unlock();
        return buffer;
    }

    audioMutex.unlock();
    return NULL;
}

void IOBridge::initFFMpeg()
{
    // Allocate video frame for input
    frameUYVY = av_frame_alloc();
    frameUYVY->width  = width;
    frameUYVY->height = height;
    frameUYVY->format = AV_PIX_FMT_UYVY422;
    frameUYVY->interlaced_frame = 1;
    frameUYVY->top_field_first = 1;
    av_frame_get_buffer(frameUYVY, 32);

#ifdef GUI
    // Allocate video frame for video preview
    framePreview = av_frame_alloc();
    framePreview->width  = previewWidth;
    framePreview->height = 288;
    framePreview->format = AV_PIX_FMT_RGBA;
    av_frame_get_buffer(framePreview, 32);
    previewSize = framePreview->width * framePreview->height * 4;

    swsContextPreview = sws_getContext(frameUYVY->width, frameUYVY->height, (AVPixelFormat)frameUYVY->format,
                            framePreview->width, framePreview->height, (AVPixelFormat)framePreview->format,
                            SWS_FAST_BILINEAR, NULL, NULL, NULL);
#endif

    if(currentCG != "")
        this->initFilters(currentCG, currentMediaFormat);
}

const bool IOBridge::initFilters(const QString& cg, const QString& format)
{
    QString filter = "movie=cg/" + cg + " [watermark]; [in][watermark] overlay=0:0,format=pix_fmts=" + QString::number(frameUYVY->format) + " [out]";

    char args[512];
    AVFilter* buffersrc  = avfilter_get_by_name("buffer");
    AVFilter* buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs  = avfilter_inout_alloc();
    filterGraph = avfilter_graph_alloc();

    // Create the source buffer to filter input frames
    _snprintf_s(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             frameUYVY->width, frameUYVY->height, frameUYVY->format, filterFPS, 1, 1, 1);

    if(avfilter_graph_create_filter(&buffersrcContext, buffersrc, "in", args, NULL, filterGraph) < 0)
    {
        LOG4CXX_ERROR(Logger::getLogger("IOBridge"), "Cannot create buffer source");
        cleanupFilters();
        return false;
    }

    // Create the sink buffer to output the filtered frames
    enum AVPixelFormat pix_fmts[] = { (AVPixelFormat)frameUYVY->format, AV_PIX_FMT_NONE };
    AVBufferSinkParams* buffersink_params;
    buffersink_params = av_buffersink_params_alloc();
    buffersink_params->pixel_fmts = pix_fmts;

    if (avfilter_graph_create_filter(&buffersinkContext, buffersink, "out", NULL, buffersink_params, filterGraph) < 0)
    {
        LOG4CXX_ERROR(Logger::getLogger("IOBridge"), "Cannot create buffer sink");
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
        LOG4CXX_ERROR(Logger::getLogger("IOBridge"), "Cannot parse filter graph");
        cleanupFilters();
        return false;
    }

    if(avfilter_graph_config(filterGraph, NULL) < 0)
    {
        LOG4CXX_ERROR(Logger::getLogger("IOBridge"), "Cannot config filter graph");
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

// CG functions
const void IOBridge::activateFilter(const QString& cg)
{
    videoMutex.lock();
    audioMutex.lock();

    currentCG = cg;
    this->cleanupFilters();
    if(cg != "")
        this->initFilters(cg, currentMediaFormat);

    audioMutex.unlock();
    videoMutex.unlock();
}

void IOBridge::cleanupFFMpeg()
{
    cleanupFilters();

#ifdef GUI
    if(swsContextPreview != NULL)
        sws_freeContext(swsContextPreview);
    swsContextPreview = NULL;

    if(framePreview != NULL)
        av_frame_free(&framePreview);
    framePreview = NULL;

    previewWidth = 360;
#endif

    if(frameUYVY != NULL)
        av_frame_free(&frameUYVY);
    frameUYVY = NULL;
}

void IOBridge::cleanupFilters()
{
    if(filterGraph != NULL)
        avfilter_graph_free(&filterGraph);
    filterGraph = NULL;
    buffersinkContext = NULL;
    buffersrcContext = NULL;

    if(filterFrame != NULL)
        av_frame_free(&filterFrame);
    filterFrame = NULL;
}

void IOBridge::cleanup()
{
    videoMutex.lock();
    videoFramesList.clear();
    videoMutex.unlock();
    audioMutex.lock();
    audioSamplesList.clear();
    audioMutex.unlock();

    cleanupFFMpeg();

#ifdef DECKLINK
    if(deckLinkInput != NULL)
        deckLinkInput->disableCard();
    deckLinkInput = NULL;
    if(deckLinkOutput != NULL)
        deckLinkOutput->disableCard();
    deckLinkOutput = NULL;
#endif

#ifdef GUI
    if(preview != NULL)
        preview->stopAudio();
    preview = NULL;
#endif
}

bool IOBridge::startRecording(QString path, QString filename, QString extension, const char* timecode)
{
    startRecording();
    if(recorder->startRecording(path, filename, extension, timecode))
    {
        recorder->start();
        return true;
    }

    return false;
}

int64_t IOBridge::stopRecording(bool recordRestart)
{
    stopRecording();
    return recorder->stopRecording(recordRestart);
}

int64_t IOBridge::getCurrentRecordTime()
{
    return recorder->getCurrentRecordTime();
}

QString IOBridge::checkFFError(int addr, const char* timecode)
{
    return recorder->checkFFError(addr, timecode);
}
