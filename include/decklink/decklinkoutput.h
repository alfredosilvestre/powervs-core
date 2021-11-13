#ifndef DECKLINKOUTPUT_H
#define DECKLINKOUTPUT_H

#include "DeckLinkAPI_h.h"

#include <stdint.h>
#include <QObject>
#include <QString>

class Player;

class DeckLinkOutput : public QObject, public IDeckLinkVideoOutputCallback, public IDeckLinkAudioOutputCallback
{
    Q_OBJECT

    public:
        DeckLinkOutput(IDeckLink* deckLinkDevice);
        ~DeckLinkOutput();

    public:
        bool isInitialized();

        bool enableCard();
        void loadBars();
        void disableCard();
        const QString getDeviceName() const;
        const bool changeFormat(const QString& format);
        void setRate(const double rate);
        void setStartFrame(const int64_t start_frame);
        void adjustFPS(const double adjust);

    public:
        void startPlayback(Player* player);
        void stopPlayback(const bool immediate);
        void outputVideo(const uint8_t* videoBuffer, const int size, const bool preview, const int64_t timecode);
        void outputAudio(const uint8_t* audioBuffer, const int size);

    private:
        void print_output_modes(IDeckLink* deckLink);

    private:
        Player* player;

        QString deviceName;
        IDeckLinkOutput* outputCard;
        BMDDisplayMode setDisplayMode;
        QString filename;

        int width;
        int height;
        int frameSize;
        int rowBytes;
        int vancRows;
        int videoOffset;
        bool canPreview;
        int64_t startFrame;
        double ratio;
        int64_t totalFrames;
        int64_t totalAudioSamples;
        int prerollCount;
        int64_t endFrame;
        bool playing;

        BMDAudioSampleRate sampleRate;
        BMDAudioSampleType sampleType;
        int channelCount;
        BMDAudioOutputStreamType streamType;

        BMDPixelFormat pixelFormat;
        BMDTimeValue originalFrameDuration;
        BMDTimeValue frameDisplayTime;
        BMDTimeValue frameDuration;
        BMDTimeScale frameTimescale;

    public:
        virtual HRESULT STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame* completed_frame, BMDOutputFrameCompletionResult result);
        virtual HRESULT STDMETHODCALLTYPE RenderAudioSamples(BOOL preroll);
        virtual HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped();

        // IUnknown needs only a dummy implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID, LPVOID*) { return E_NOINTERFACE; }
        virtual ULONG STDMETHODCALLTYPE AddRef () { return 1; }
        virtual ULONG STDMETHODCALLTYPE Release () { return 1; }

    signals:
        void genlockError();
};

#endif // DECKLINKOUTPUT_H
