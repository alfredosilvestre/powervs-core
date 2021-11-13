#include "player.h"

#include <log4cxx/logger.h>

using namespace log4cxx;

Player::Player(QObject* parent) : QObject(parent)
{
    decoder = NULL;
    audioThread = new AudioThread(false, this);
#ifdef PORTAUDIO
    audioThreadPreview = new AudioThread(true, this);
#endif
    videoThread = new VideoThread(this);

    startFrame = 0;
    startMillisecond = 0;
    videoRate = 1.0;
    currentMediaFormat = "PAL";
    currentCG = "";

    loop = false;
    loaded = false;
    playing = IDLE;
#ifdef GUI
    preview = NULL;
#endif
#ifdef DECKLINK
    deckLinkOutput = NULL;
#endif
}

Player::~Player()
{
    this->dropMedia(true);
#ifdef DECKLINK
    if(deckLinkOutput != NULL)
        deckLinkOutput->disableCard();
#endif
}

const int64_t Player::initFFMpeg(const QString& path)
{
    decoder = new FFDecoder(this, loop);
    int64_t duration_ms = -1;
#ifdef DECKLINK
    duration_ms = decoder->init(path, currentMediaFormat, deckLinkOutput);
#else
    duration_ms = decoder->init(path, currentMediaFormat);
#endif
    if(duration_ms == -1)
        return duration_ms;

    activateFilter(currentCG);

    decoder->startDecoding();
    decoder->start();

    return duration_ms;
}

void Player::cleanupFFMpeg()
{
    if(decoder != NULL)
    {
        decoder->stopDecoding();
        decoder->setRate(videoRate);
        delete decoder;
    }
    decoder = NULL;

    if(audioThread != NULL)
        audioThread->cleanup();

#ifdef PORTAUDIO
    if(audioThreadPreview != NULL)
        audioThreadPreview->cleanup();
#endif

    if(videoThread != NULL)
        videoThread->cleanup(0);

#ifdef DECKLINK
    if(deckLinkOutput != NULL)
    {
        deckLinkOutput->setRate(videoRate);
        deckLinkOutput->setStartFrame(startFrame);
    }
#endif
}

const bool Player::pushVideoFrame(AVDecodedFrame* v)
{
    if(!loaded)
        return false;

    if(playing == STOPPING)
    {
        if(v != NULL)
            delete v;
        return true;
    }

    videoMutex.lock();
#ifdef DECKLINK
    if(deckLinkOutput != NULL)
    {
        if(videoFramesList.size() > 50)
        {
            videoMutex.unlock();
            return false;
        }

        videoFramesList.append(v);
    }
    else if(v != NULL)
        delete v;
#else
    if(v != NULL)
        delete v;
#endif

    videoMutex.unlock();
    return true;
}

const bool Player::pushVideoFramePreview(AVDecodedFrame* vp)
{
    if(!loaded)
        return false;

    if(playing == STOPPING)
    {
        if(vp != NULL)
            delete vp;
        return true;
    }

    bool pushed = false;
    if(videoThread != NULL)
        pushed = videoThread->pushVideoFrame(vp);
    else if(vp != NULL)
    {
        delete vp;
        pushed = true;
    }

    return pushed;
}

const bool Player::pushAudioFrame(AVDecodedFrame* a)
{
    if(!loaded)
        return false;

    if(playing == STOPPING)
    {
        if(a != NULL)
            delete a;
        return true;
    }

    audioMutex.lock();
#ifdef DECKLINK
    if(deckLinkOutput != NULL)
    {
        if(audioSamplesList.size() > 100)
        {
            audioMutex.unlock();
            return false;
        }

        audioSamplesList.append(a);
    }
    else if(a != NULL)
        delete a;
#else
    if(a != NULL)
        delete a;
#endif

    audioMutex.unlock();
    return true;
}

const bool Player::pushAudioFrameVU(AVDecodedFrame* aVU)
{
    if(!loaded)
        return false;

    if(playing == STOPPING)
    {
        if(aVU != NULL)
            delete aVU;
        return true;
    }

    bool pushed = false;
    if(audioThread != NULL)
        pushed = audioThread->pushAudioFrame(aVU);
    else
    {
        if(aVU != NULL)
            delete aVU;
        pushed = true;
    }

    return pushed;
}

#ifdef PORTAUDIO
const bool Player::pushAudioFramePreview(AVDecodedFrame* ap)
{
    if(!loaded)
        return false;

    if(playing == STOPPING)
    {
        if(ap != NULL)
            delete ap;
        return true;
    }

    bool pushed = false;
    if(audioThreadPreview != NULL)
        pushed = audioThreadPreview->pushAudioFrame(ap);
    else
    {
        if(ap != NULL)
            delete ap;
        pushed = true;
    }

    return pushed;
}
#endif

AVDecodedFrame* Player::getNextVideoFrame()
{
    if(!loaded)
        return NULL;

    if(playing == STOPPING)
        return NULL;

    videoMutex.lock();

    if(!videoFramesList.isEmpty())
    {
        AVDecodedFrame* buffer = videoFramesList.takeFirst();
        videoMutex.unlock();
        return buffer;
    }
    else LOG4CXX_DEBUG(Logger::getLogger("Player"), "Video frame list empty");

    videoMutex.unlock();
    return NULL;
}

AVDecodedFrame* Player::getNextAudioSample()
{
    if(!loaded)
        return NULL;

    if(playing == STOPPING)
        return NULL;

    audioMutex.lock();

    if(!audioSamplesList.isEmpty())
    {
        AVDecodedFrame* buffer = audioSamplesList.takeFirst();
        audioMutex.unlock();
        return buffer;
    }
    else LOG4CXX_DEBUG(Logger::getLogger("Player"), "Audio sample list empty");

    audioMutex.unlock();
    return NULL;
}

// Player functions
#ifdef GUI
void Player::togglePreviewVideo(PreviewerRGB* previewer)
{
    if(this->preview != NULL)
    {
        this->preview->stopAudio();
        if(videoThread != NULL)
            this->disconnect(videoThread, SIGNAL(previewVideo(QByteArray)), preview, SLOT(previewVideo(QByteArray)));
        if(audioThread != NULL)
            this->disconnect(audioThread, SIGNAL(previewAudio(QByteArray)), preview, SLOT(previewAudio(QByteArray)));
    }

    this->preview = previewer;
    if(this->preview != NULL)
    {
        if(videoThread != NULL)
            this->connect(videoThread, SIGNAL(previewVideo(QByteArray)), preview, SLOT(previewVideo(QByteArray)));
        if(audioThread != NULL)
            this->connect(audioThread, SIGNAL(previewAudio(QByteArray)), preview, SLOT(previewAudio(QByteArray)));

        preview->changeFormat(currentMediaFormat);
    }
}

void Player::togglePreviewAudio(bool enabled)
{
    if(enabled)
    {
#ifdef PORTAUDIO
        if(audioThreadPreview == NULL)
            audioThreadPreview = new AudioThread(true, this);
#endif
    }
    else
    {
#ifdef PORTAUDIO
        if(audioThreadPreview != NULL)
        {
            audioThreadPreview->cleanup();
            delete audioThreadPreview;
        }
        audioThreadPreview = NULL;
#endif
    }
}
#endif

#ifdef DECKLINK
const QString Player::setDeckLinkOutput(QString inter)
{
    if(deckLinkOutput != NULL)
        deckLinkOutput->disableCard();
    deckLinkOutput = NULL;

    if(inter.contains("Output"))
    {
        inter.remove(QRegExp(".*\\("));
        inter.remove(")");

        deckLinkOutput = DeckLinkControl::getOutput(inter.toInt());
        deckLinkOutput->enableCard();
        changeFormat(currentMediaFormat);
    }

    return inter;
}

const DeckLinkOutput* Player::getDecklinkOutput() const
{
    return deckLinkOutput;
}
#endif

const int64_t Player::loadMedia(const QString& media_path)
{
    LOG4CXX_INFO(Logger::getLogger("Player"), "Loading clip: " + media_path.toStdString());

    int64_t duration_ms = this->initFFMpeg(media_path);
    if(duration_ms == -1)
        return duration_ms;

    loaded = true;
    playing = IDLE;

    // Guarantees that there are at least 25 frames decoded
    waitForDecoding();

    LOG4CXX_INFO(Logger::getLogger("Player"), "Finished loading clip: " + media_path.toStdString());

    return duration_ms;
}

void Player::waitForDecoding()
{
#ifdef DECKLINK
    if(deckLinkOutput != NULL)
    {
        int count = 0;
        while(true)
        {
            videoMutex.lock();

            if(videoFramesList.size() < 5)
            {
                count++;
                Sleep(1);
            }
            else
            {
                videoMutex.unlock();
                break;
            }

            videoMutex.unlock();

            // Ensures there is no deadlock if the file is too small
            if(count > 100)
                break;
        }
    }
#endif
}

void Player::changeFormat(const QString& format)
{
    currentMediaFormat = format;

#ifdef DECKLINK
    if(deckLinkOutput != NULL)
        deckLinkOutput->changeFormat(format);
#endif
#ifdef GUI
    if(preview != NULL)
        preview->changeFormat(format);
#endif

    if(decoder != NULL)
        decoder->changeFormat(format);
}

void Player::toggleLoop(const bool loop)
{
    if(decoder != NULL)
        decoder->toggleLoop(loop, playing != IDLE);
    if(videoThread != NULL)
        videoThread->toggleLoop(loop, (playing != IDLE && playing != PAUSED));
    if(audioThread != NULL)
        audioThread->toggleLoop(loop, (playing != IDLE && playing != PAUSED));
#ifdef PORTAUDIO
    if(audioThreadPreview != NULL)
        audioThreadPreview->toggleLoop(loop, false);
#endif
    this->loop = loop;
}

void Player::setRecueBuffers(AVDecodedFrame* v, AVDecodedFrame* vp)
{
    if(playing != PLAYING)
    {
#ifdef DECKLINK
        if(deckLinkOutput != NULL && v != NULL)
        {
            int64_t timecode = startFrame + v->getClockPTS() * 25 * videoRate / 1000;
            deckLinkOutput->outputVideo(v->getBuffer(), v->getSize(), true, timecode);
        }
#else
        Q_UNUSED(v);
#endif
#ifdef GUI
        if(preview != NULL && vp != NULL)
            preview->previewVideo(QByteArray((char*)vp->getBuffer(), vp->getSize()));
#else
     Q_UNUSED(vp);
#endif
    }
}

void Player::stop(const bool immediate)
{
    if(immediate)
    {
        if(videoThread != NULL)
            videoThread->stopPlaying();
        if(audioThread != NULL)
            audioThread->stopPlaying();
    #ifdef PORTAUDIO
        if(audioThreadPreview != NULL)
            audioThreadPreview->stopPlaying();
    #endif
    }

    videoRate = 1.0;
    playing = STOPPING;

#ifdef DECKLINK
    if(deckLinkOutput != NULL)
    {
        deckLinkOutput->stopPlayback(immediate);
        deckLinkOutput->setRate(videoRate);

        if(immediate)
        {
            videoMutex.lock();
            qDeleteAll(videoFramesList);
            videoFramesList.clear();
            videoMutex.unlock();

            audioMutex.lock();
            qDeleteAll(audioSamplesList);
            audioSamplesList.clear();
            audioMutex.unlock();
        }
    }
#endif

    playing = IDLE;
}

void Player::recue()
{
    loaded = true;

    if(decoder != NULL)
    {
        decoder->stopDecoding();
        decoder->setRate(videoRate);
    }

    stop(true);

    seek(0);
}

void Player::take()
{
    if(playing != PLAYING)
    {
        playing = PLAYING;
        if(videoThread != NULL)
        {
            videoThread->startPlaying();
            videoThread->start();
        }
        if(audioThread != NULL)
        {
            audioThread->startPlaying();
            audioThread->start();
        }
#ifdef PORTAUDIO
        if(audioThreadPreview != NULL && videoRate == 1.0)
        {
            audioThreadPreview->startPlaying();
            audioThreadPreview->start();
        }
#endif
        if(deckLinkOutput != NULL)
            deckLinkOutput->startPlayback(this);
    }
}

void Player::pause(const bool resetRate)
{
    if(playing != PAUSED)
    {
        playing = PAUSED;
        if(videoThread != NULL)
            videoThread->stopPlaying();
        if(audioThread != NULL)
            audioThread->stopPlaying();

#ifdef PORTAUDIO
        if(audioThreadPreview != NULL)
            audioThreadPreview->stopPlaying();
#endif
        if(resetRate)
        {
            videoRate = 1.0;

            if(decoder != NULL)
                decoder->setRate(videoRate);
#ifdef DECKLINK
			if(deckLinkOutput != NULL)
            {
				deckLinkOutput->setRate(videoRate);
                deckLinkOutput->stopPlayback(true);
            }
#endif
        }

        if(videoThread != NULL && loaded)
            seek(videoThread->getCurrentPlayTime());
    }
}

void Player::dropMedia(const bool immediate)
{
    this->cleanupFFMpeg();

    stop(immediate);

    startFrame = 0;
    startMillisecond = 0;
    loaded = false;
}

void Player::seek(const int64_t pos)
{
    if(decoder != NULL)
    {
        decoder->stopDecoding();

        if(audioThread != NULL)
            audioThread->cleanup();

#ifdef PORTAUDIO
        if(audioThreadPreview != NULL)
            audioThreadPreview->cleanup();
#endif

        int seek_flag = 0;

        if(videoThread != NULL)
        {
            if(videoThread->getCurrentPlayTime() > pos)
                seek_flag = AVSEEK_FLAG_BACKWARD;
            videoThread->cleanup(pos);
        }

#ifdef DECKLINK
    if(deckLinkOutput != NULL)
    {
        videoMutex.lock();
        qDeleteAll(videoFramesList);
        videoFramesList.clear();
        videoMutex.unlock();

        audioMutex.lock();
        qDeleteAll(audioSamplesList);
        audioSamplesList.clear();
        audioMutex.unlock();
    }
#endif

        decoder->seek(pos, seek_flag);
        decoder->startDecoding();
        decoder->start();

        // Guarantees that there are at least 25 frames decoded
        waitForDecoding();
    }
}

void Player::nextFrame()
{
    if(videoThread != NULL)
        seek(videoThread->getCurrentPlayTime() + 40);
}

void Player::previousFrame()
{
    if(videoThread != NULL)
        seek(videoThread->getCurrentPlayTime() - 40);
}

double Player::forward()
{
    if(videoRate < 2.0)
    {
        double rate = videoRate + 0.1;
        if(rate > 2.1)
            return videoRate;
        videoRate = rate;

        if(decoder != NULL)
            decoder->setRate(videoRate);

#ifdef DECKLINK
        if(deckLinkOutput != NULL)
            deckLinkOutput->setRate(videoRate);
#endif
        if(videoThread != NULL)
        {
            pause(false);
            take();
        }

        if(videoRate != 1.0)
        {
#ifdef GUI
            if(preview != NULL)
#endif
            {
#ifdef GUI
                preview->stopAudio();
#endif
#ifdef PORTAUDIO
                audioThreadPreview->cleanup();
#endif
            }
        }
    }

    return videoRate;
}

double Player::reverse()
{
    if(videoRate > 0.1)
    {
        double rate = videoRate - 0.1;
        if(rate < 0.1)
            return videoRate;
        videoRate = rate;

        if(decoder != NULL)
            decoder->setRate(videoRate);

#ifdef DECKLINK
        if(deckLinkOutput != NULL)
            deckLinkOutput->setRate(videoRate);
#endif
        if(videoThread != NULL)
        {
            pause(false);
            take();
        }

        if(videoRate != 1.0)
        {
#ifdef GUI
            if(preview != NULL)
#endif
            {
#ifdef GUI
                preview->stopAudio();
#endif
#ifdef PORTAUDIO
                audioThreadPreview->cleanup();
#endif
            }
        }
    }

    return videoRate;
}

void Player::setStartMillisecond(const int64_t start_millisecond)
{
    startFrame = start_millisecond * 25 / 1000;
    startMillisecond = start_millisecond;

#ifdef DECKLINK
    if(deckLinkOutput != NULL)
        deckLinkOutput->setStartFrame(startFrame);
#endif
}

const int64_t Player::getStartMillisecond() const
{
    return startMillisecond;
}

const int64_t Player::getCurrentPlayTime() const
{
    int64_t time = 0;

    if(videoThread != NULL)
        time = videoThread->getCurrentPlayTime();

    return time;
}

const int64_t Player::getCurrentTimeCode() const
{
    int64_t time = 0;

    if(videoThread != NULL)
        time = videoThread->getCurrentPlayTime() + startMillisecond;

    return time;
}


const bool Player::isLoaded() const
{
    return loaded;
}

const bool Player::isPlaying() const
{
    return playing == PLAYING;
}

const bool Player::isPaused() const
{
    return playing == PAUSED;
}

const bool Player::isEOF() const
{
    if(videoThread != NULL && !videoThread->isFinished())
        return false;
    if(audioThread != NULL && !audioThread->isFinished())
        return false;

    if(playing == IDLE)
        return false;

    return !isPaused();
}

// CG functions
void Player::activateFilter(QString cg)
{
    currentCG = cg;
    if(decoder != NULL && playing != PLAYING)
    {
        decoder->cleanupFilters();
        if(cg != "")
        {
            decoder->initFilters(cg, currentMediaFormat);
            seek(getCurrentPlayTime());
        }
    }
}
