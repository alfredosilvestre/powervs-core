#include "videothread.h"

extern "C"
{
    #include <libavutil/time.h>
}

VideoThread::VideoThread(QObject* parent) : QThread(parent)
{
    playing = false;
    loop = false;
    video_clock = 0;
}

VideoThread::~VideoThread()
{
    cleanup(0);
}

void VideoThread::toggleLoop(const bool loop, const bool active)
{
    videoMutex.lock();

    this->loop = loop;
    if(loop && !playing && active)
    {
        playing = true;
        start();
    }

    videoMutex.unlock();
}

void VideoThread::startPlaying()
{
    playing = true;
}

void VideoThread::run()
{
    int64_t last_clock = av_gettime();

    while(playing)
    {
        int64_t diff = 0;
        int64_t clockPTS = 0;

        videoMutex.lock();

        if(!previewFramesList.empty())
        {
            AVDecodedFrame* vp = previewFramesList.takeFirst();
            if(vp != NULL)
            {
                diff = vp->getPTS();
                clockPTS = vp->getClockPTS();
                emit previewVideo(QByteArray((char*)vp->getBuffer(), vp->getSize()));
                delete vp;
            }
            else if(!loop)
                playing = false;
        }

        videoMutex.unlock();

        if(diff > 0)
        {
            video_clock = clockPTS;
            last_clock += diff;
            int64_t msecs = last_clock - av_gettime();

            if(msecs > 0.0)
                this->usleep(msecs);
        }
    }
}

void VideoThread::stopPlaying()
{
    playing = false;
    this->wait();
}

const bool VideoThread::pushVideoFrame(AVDecodedFrame* vp)
{
    videoMutex.lock();

    if(previewFramesList.size() > 50)
    {
        videoMutex.unlock();
        return false;
    }

    previewFramesList.append(vp);
    videoMutex.unlock();

    return true;
}

const int64_t VideoThread::getCurrentPlayTime() const
{
    return video_clock;
}

void VideoThread::cleanup(const int64_t pos)
{
    video_clock = pos;

    videoMutex.lock();

    qDeleteAll(previewFramesList);
    previewFramesList.clear();

    videoMutex.unlock();
}
