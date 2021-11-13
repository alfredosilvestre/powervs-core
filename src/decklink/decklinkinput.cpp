#include "decklinkinput.h"
#include "iobridge.h"

#include <comdef.h>
#include <comutil.h>
#include <stdint.h>

DeckLinkInput::DeckLinkInput(IDeckLink* deckLinkDevice)
{
    BSTR deviceNameBSTR = NULL;
    deckLinkDevice->GetModelName(&deviceNameBSTR);
    _bstr_t deviceName(deviceNameBSTR, false);
    this->deviceName.append(deviceName);
    setDisplayMode = bmdModePAL;

    width = 720;
    height = 576;
    rowBytes = width * 2;
    frameSize = width * rowBytes;
    bytesPerSample = 16;
    vancRows = 32;
    videoOffset = vancRows * rowBytes;

    deckLinkDevice->QueryInterface(IID_IDeckLinkInput, (void**)&inputCard);
    if(inputCard != NULL)
        inputCard->SetCallback(this);
}

DeckLinkInput::~DeckLinkInput()
{
    if(inputCard != NULL)
    {
        this->disableCard();
        inputCard->Release();
    }
}

bool DeckLinkInput::isInitialized()
{
    return inputCard != NULL;
}

bool DeckLinkInput::enableCard()
{
    BMDDisplayModeSupport displayModeSupport;
    IDeckLinkDisplayMode* displayMode;

    BMDPixelFormat setPixelFormat = bmdFormat8BitYUV;
    BMDVideoInputFlags setInputFlag = bmdVideoInputFlagDefault;
    BMDAudioSampleRate sampleRate = bmdAudioSampleRate48kHz;
    BMDAudioSampleType sampleType = bmdAudioSampleType16bitInteger;
    int channelCount = 8;

    inputCard->DoesSupportVideoMode(setDisplayMode, setPixelFormat, setInputFlag, &displayModeSupport, &displayMode);

    if (displayModeSupport != bmdDisplayModeNotSupported)
    {
        inputCard->EnableAudioInput(sampleRate, sampleType, channelCount);
        inputCard->EnableVideoInput(setDisplayMode, setPixelFormat, setInputFlag);

        width = displayMode->GetWidth();
        height = displayMode->GetHeight();
        rowBytes = width * 2;
        frameSize = height * rowBytes;
        bytesPerSample = 2*channelCount;

        emit timecodeReceived("00:00:00:00");
        inputCard->FlushStreams();
        inputCard->StartStreams();

        return true;
    }

    return false;
}

void DeckLinkInput::disableCard()
{
    inputCard->StopStreams();
    inputCard->DisableVideoInput();
    inputCard->DisableAudioInput();
}

void DeckLinkInput::flushCard()
{
    inputCard->PauseStreams();
    inputCard->FlushStreams();
    inputCard->StartStreams();
}

QString DeckLinkInput::getDeviceName()
{
    return deviceName;
}

bool DeckLinkInput::changeFormat(QString format)
{
    inputCard->PauseStreams();

    if(format == "PAL" || format == "PAL 16:9")
        setDisplayMode = bmdModePAL;
    else if(format == "720p50")
        setDisplayMode = bmdModeHD720p50;
    else if(format == "720p5994")
        setDisplayMode = bmdModeHD720p5994;
    else if(format == "1080p25")
        setDisplayMode = bmdModeHD1080p25;
    else if(format == "1080i50")
        setDisplayMode = bmdModeHD1080i50;
    else if(format == "1080i5994")
        setDisplayMode = bmdModeHD1080i5994;

    bool success = enableCard();

    if(format == "PAL" || format == "PAL 16:9")
    {
        vancRows = 32;
        videoOffset = vancRows * rowBytes;
    }
    else // TODO: VANC for HD resolutions
    {
        vancRows = 0;
        videoOffset = 0;
    }

    return success;
}

void DeckLinkInput::setIOBridge(IOBridge* ioBridge)
{
    this->ioBridge = ioBridge;
}

HRESULT STDMETHODCALLTYPE DeckLinkInput::VideoInputFrameArrived(IDeckLinkVideoInputFrame* video, IDeckLinkAudioInputPacket* audio)
{
    // Process video frame
    if(video != NULL)
    {
        if(!(video->GetFlags() & bmdFrameHasNoInputSource))
        {
            // Get pointer to video data
            char* videoPointer;
            video->GetBytes((void**)&videoPointer);

            IDeckLinkTimecode* timecode;
            video->GetTimecode(bmdTimecodeVITC, &timecode);

            if(timecode != NULL)
            {
                BSTR* tc_string = new BSTR;
                timecode->GetString(tc_string);
                QString str(_com_util::ConvertBSTRToString(*tc_string));
                delete tc_string;
                emit timecodeReceived(str);

                timecode->Release();
            }

            uint8_t* videoBuffer = new uint8_t[frameSize + videoOffset];

            for(int i=0; i<videoOffset; i++) // Insert videoOffset lines of black
            {
                if(i%2 == 0)
                    videoBuffer[i] = 128;
                else videoBuffer[i] = 16;
            }
            memcpy(videoBuffer + videoOffset, videoPointer, frameSize);

            // FIXME: Issue with Decklink Duo 2
            /*if(videoOffset > 0) // TODO: insert hourly timecode to VANC if activated
            {
                IDeckLinkVideoFrameAncillary* vanc;
                if(video->GetAncillaryData(&vanc) == S_OK && vanc != NULL)
                {
                    for(int i=0; i<vancRows; i++)
                    {
                        void* buffer;
                        if(vanc->GetBufferForVerticalBlankingLine(i+1, &buffer) == S_OK && buffer != NULL)
                            memcpy(videoBuffer + i * rowBytes, buffer, rowBytes);
                    }
                    vanc->Release();
                }
            }*/

            if(ioBridge != NULL)
                ioBridge->pushVideoFrame(videoBuffer, frameSize);

            delete[] videoBuffer;
        }
        else emit signalError();
    }

    // Process audio for the video frame
    if (audio != NULL)
    {
        // Get pointer to audio data
        char* audioPointer;
        audio->GetBytes((void**)&audioPointer);

        // Get audio size and send it to whoever is connected
        // Multiply audio sample frame count by 2*channels
        int audioSize = audio->GetSampleFrameCount() * bytesPerSample;

        uint8_t* audioBuffer = new uint8_t[audioSize];
        memcpy(audioBuffer, audioPointer, audioSize);

        if(ioBridge != NULL)
            ioBridge->pushAudioFrame(audioBuffer, audioSize);

        delete[] audioBuffer;
    }

    return S_OK;
}
