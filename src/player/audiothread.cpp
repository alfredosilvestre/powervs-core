#include "audiothread.h"

extern "C"
{
    #include <libavutil/time.h>
}

#include <log4cxx/logger.h>

using namespace log4cxx;

AudioThread::AudioThread(bool preview, QObject* parent) : QThread(parent)
{
    playing = false;
    loop = false;
    this->preview = preview;
}

AudioThread::~AudioThread()
{
    cleanup();
}

void AudioThread::startPlaying()
{
    playing = true;
}

void AudioThread::toggleLoop(const bool loop, const bool active)
{
    audioMutex.lock();

    this->loop = loop;
    if(loop && !playing && active)
    {
        playing = true;
        start();
    }

    audioMutex.unlock();
}

#ifdef PORTAUDIO
int AudioThread::paCallback(const void*, void* outputBuffer, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData)
{
    AudioThread* self = (AudioThread*)userData;

    self->audioMutex.lock();
    if(!self->audioSamplesList.empty())
    {
        AVDecodedFrame* a = self->audioSamplesList.takeFirst();
        if(a != NULL)
        {
            memcpy(outputBuffer, a->getBuffer(), a->getSize());
            delete a;
        }
        else if(!self->loop)
            self->playing = false;
    }
    self->audioMutex.unlock();

    return 0;
}
#endif

void AudioThread::run()
{
#ifdef PORTAUDIO
    PaStream* stream = NULL;
    if(preview)
    {
        int nb_samples = 1024;
        while(audioSamplesList.empty() && playing)
            this->usleep(1000);

        audioMutex.lock();
        if(!audioSamplesList.empty() && audioSamplesList.first() != NULL)
            nb_samples = audioSamplesList.first()->getSize()/4; // Divide audio buffer size by 2*channels
        audioMutex.unlock();

        PaError err = Pa_OpenDefaultStream(&stream, 0, 2, paInt16, 48000, nb_samples, paCallback, this);
        if(err < 0)
        {
            std::string error = "Audio Driver Error: ";
            error += Pa_GetErrorText(err);
            LOG4CXX_ERROR(Logger::getLogger("AudioThread"), error);
        }
        else Pa_StartStream(stream);
    }
#endif

    int64_t last_clock = av_gettime();

    while(playing)
    {
        if(!preview)
        {
            int64_t diff = 0;

            audioMutex.lock();

            if(!audioSamplesList.empty())
            {
                AVDecodedFrame* a = audioSamplesList.takeFirst();
                if(a != NULL)
                {
#ifdef GUI
                    emit previewAudio(QByteArray((char*)a->getBuffer(), a->getSize()));
#endif
                    diff = a->getPTS();
                    delete a;
                }
                else if(!loop)
                    playing = false;
            }
            audioMutex.unlock();

            if(diff > 0)
            {
                last_clock += diff;
                int64_t msecs = last_clock - av_gettime();

                if(msecs > 0.0)
                    this->usleep(msecs);
            }
        }
        else this->msleep(10);
    }

#ifdef PORTAUDIO
    this->msleep(100);
    if(preview && stream != NULL)
        Pa_CloseStream(stream);
#endif

#ifdef GUI
    if(!preview)
        emit previewAudio(QByteArray());
#endif
}

void AudioThread::stopPlaying()
{
    playing = false;
    this->wait();
}

const bool AudioThread::pushAudioFrame(AVDecodedFrame* a)
{
    audioMutex.lock();

    if(audioSamplesList.size() > 100)
    {
        audioMutex.unlock();
        return false;
    }

    audioSamplesList.append(a);
    audioMutex.unlock();

    return true;
}

void AudioThread::cleanup()
{
    audioMutex.lock();
    qDeleteAll(audioSamplesList);
    audioSamplesList.clear();
    audioMutex.unlock();
}
