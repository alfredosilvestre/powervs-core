#include "videoport.h"

#include <QDebug>

VideoPort::VideoPort(const int num)
{
    this->num = num;
    debugName = QString("{VideoPort ") + QString::number(num) + QString("} ");
    state = NONE;
    player = NULL;
}

VideoPort::~VideoPort()
{
    if(player != NULL)
        deactivatePlayout();
}

// VideoPort functions

const int VideoPort::activatePlayout()
{
    switch(state)
    {
        case PLAYOUT:
            return -1;
        case INGEST:
            return 0; // TODO: deactivate input
        case BRIDGE:
            return 0; // TODO: deactivate input
        case NONE:
            break;
    }

    state = PLAYOUT;
    player = new Player();
    player->setDeckLinkOutput("Output (" + QString::number(num) + ")");

    return 0;
}

const int VideoPort::deactivatePlayout()
{
    if(state != PLAYOUT)
        return -1;

    state = NONE;
    delete player;
    player = NULL;

    return 0;
}

// TODO: set lock state, get state, port get mode (input/output), port set mode

// Playout functions

const int64_t VideoPort::loadItem(const QString& path, const bool canTake) const
{
    if(state != PLAYOUT)
        return -1;

    qDebug() << debugName + "Loaded clip location: " + path;
    return player->loadMedia(path);
}

const int VideoPort::changeFormat(const QString& format) const
{
    if(state != PLAYOUT)
        return -1;

    qDebug() << debugName + "Format changed to: " + format;
    player->changeFormat(format);

    return 0;
}

const int64_t VideoPort::getCurrentPlayTime() const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isPlaying())
        return -1;

    return player->getCurrentPlayTime();
}

const int64_t VideoPort::getCurrentTimeCode() const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isPlaying())
        return -1;

    return player->getCurrentTimeCode();
}

const int VideoPort::recue(const bool loop) const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isLoaded())
        return -1;

    qDebug() << debugName + "Recued media";
    player->recue();

    return 0;
}

const int VideoPort::take() const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isLoaded())
        return -1;

    qDebug() << debugName + "Play media";
    player->take();

    return 0;
}

const int VideoPort::pause() const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isLoaded() && !player->isPlaying())
        return -1;

    qDebug() << debugName + "Pause media";
    player->pause(false);

    return 0;
}

const int VideoPort::dropMedia(const bool immediate) const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isLoaded())
        return -1;

    qDebug() << debugName + "Droped media";
    player->dropMedia(immediate);

    return 0;
}

const int VideoPort::seek(const int64_t pos) const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isLoaded())
        return -1;

    qDebug() << debugName + "Seek value changed: " + QString::number(pos);
    player->seek(pos);

    return 0;
}

const int VideoPort::nextFrame() const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isLoaded() && !player->isPaused())
        return -1;

    qDebug() << debugName + "Play next frame";
    player->nextFrame();

    return 0;
}

const int VideoPort::previousFrame() const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isLoaded() && !player->isPaused())
        return -1;

    qDebug() << debugName + "Play previous frame";
    player->previousFrame();

    return 0;
}

const double VideoPort::forward() const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isLoaded() && !player->isPlaying())
        return -1;

    qDebug() << debugName + "";
    return player->forward();
}

const double VideoPort::reverse() const
{
    if(state != PLAYOUT)
        return -1;

    if(!player->isLoaded() && !player->isPlaying())
        return -1;

    qDebug() << debugName + "";
    return player->reverse();
}
