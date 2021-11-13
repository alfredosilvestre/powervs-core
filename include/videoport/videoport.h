#ifndef VIDEOPORT_H
#define VIDEOPORT_H

#include "player.h"

class VideoPort
{
    public:
        VideoPort(const int num);
        ~VideoPort();

    public:
        enum PortState { NONE, PLAYOUT, INGEST, BRIDGE };

    private:
        int num;
        QString debugName;
        PortState state;
        DeckLinkInput* input;

        Player* player;

    // VideoPort functions
    public:
        const int activatePlayout();
        const int deactivatePlayout();

    // Playout functions
    public:
        const int64_t loadItem(const QString& path, const bool canTake) const;
        const int changeFormat(const QString& format) const;
        const int64_t getCurrentPlayTime() const;
        const int64_t getCurrentTimeCode() const;

        const int recue(const bool loop) const;
        const int take() const;
        const int pause() const;
        const int dropMedia(const bool immediate) const;

        const int seek(const int64_t pos) const;
        const int nextFrame() const;
        const int previousFrame() const;
        const double forward() const;
        const double reverse() const;
};

#endif // VIDEOPORT_H
