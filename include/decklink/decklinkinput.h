#ifndef DECKLINKINPUT_H
#define DECKLINKINPUT_H

#include "DeckLinkAPI_h.h"

#include <QObject>
#include <QMutex>

class IOBridge;

class DeckLinkInput : public QObject, public IDeckLinkInputCallback
{
    Q_OBJECT

    public:
        DeckLinkInput(IDeckLink* deckLinkDevice);
        ~DeckLinkInput();

    public:
        bool isInitialized();

        bool enableCard();
        void disableCard();
        void flushCard();
        QString getDeviceName();
        bool changeFormat(QString format);
        void setIOBridge(IOBridge* ioBridge);

        void startStreams();
        void pauseStreams();
        void flushStreams();
        void stopStreams();

    private:
        IOBridge* ioBridge;

        QString deviceName;
        IDeckLinkInput* inputCard;
        BMDDisplayMode setDisplayMode;

        int width;
        int height;
        int frameSize;
        int rowBytes;
        int bytesPerSample;
        int vancRows;
        int videoOffset;

    public:
        virtual HRESULT STDMETHODCALLTYPE DeckLinkInput::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode*, BMDDetectedVideoInputFormatFlags) { return S_OK; }
        virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame* pArrivedFrame, IDeckLinkAudioInputPacket*);

        // IUnknown needs only a dummy implementation
        virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID, LPVOID*) { return E_NOINTERFACE; }
        virtual ULONG STDMETHODCALLTYPE AddRef () { return 1; }
        virtual ULONG STDMETHODCALLTYPE Release () { return 1; }

    signals:
        void signalError();
        void timecodeReceived(QString timecode);
};

#endif // DECKLINKINPUT_H
