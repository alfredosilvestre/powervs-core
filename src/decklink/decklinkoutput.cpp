#include "decklinkoutput.h"
#include "player.h"

#include <QDataStream>
#include <QFile>

#include <comutil.h>

#include <log4cxx/logger.h>

using namespace log4cxx;

// List of known pixel formats and their matching display names
const BMDPixelFormat knownPixelFormats[] = { bmdFormat8BitYUV, bmdFormat10BitYUV, bmdFormat8BitARGB, bmdFormat8BitBGRA, bmdFormat10BitRGB, (BMDPixelFormat)0 };
const char* knownPixelFormatNames[]	= { "8-bit YUV", "10-bit YUV", "8-bit ARGB", "8-bit BGRA", "10-bit RGB", NULL };

DeckLinkOutput::DeckLinkOutput(IDeckLink* deckLinkDevice)
{
    BSTR deviceNameBSTR = NULL;
    deckLinkDevice->GetModelName(&deviceNameBSTR);
    _bstr_t deviceName(deviceNameBSTR, false);
    this->deviceName.append(deviceName);
    this->setDisplayMode = bmdModePAL;
    filename = "";

    // ** List the video output display modes supported by the card
    //print_output_modes(deckLinkDevice);

    width = 720;
    height = 576;
    rowBytes = width * 2;
    frameSize = width * rowBytes;
    vancRows = 32;
    videoOffset = vancRows * rowBytes;
    canPreview = true;
    startFrame = 0;
    ratio = 25.0 / 1000;
    totalFrames = 0;
    prerollCount = 0;
    endFrame = 0;
    totalAudioSamples = 0;
    playing = false;

    sampleRate = bmdAudioSampleRate48kHz;
    sampleType = bmdAudioSampleType16bitInteger;
    channelCount = 8;
    streamType = bmdAudioOutputStreamTimestamped;
    pixelFormat = bmdFormat8BitYUV;

    deckLinkDevice->QueryInterface(IID_IDeckLinkOutput, (void**)&outputCard);
}

DeckLinkOutput::~DeckLinkOutput()
{
    if(outputCard != NULL)
    {
        this->disableCard();
        outputCard->Release();
    }
}

bool DeckLinkOutput::isInitialized()
{
    return outputCard != NULL;
}

void DeckLinkOutput::print_output_modes(IDeckLink* deckLink)
{
    IDeckLinkOutput*					deckLinkOutput = NULL;
    IDeckLinkDisplayModeIterator*		displayModeIterator = NULL;
    IDeckLinkDisplayMode*				displayMode = NULL;
    HRESULT								result;

    // Query the DeckLink for its configuration interface
    result = deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&deckLinkOutput);
    if (result != S_OK)
    {
        fprintf(stderr, "Could not obtain the IDeckLinkOutput interface - result = %08x\n", (unsigned int)result);
        goto bail;
    }

    // Obtain an IDeckLinkDisplayModeIterator to enumerate the display modes supported on output
    result = deckLinkOutput->GetDisplayModeIterator(&displayModeIterator);
    if (result != S_OK)
    {
        fprintf(stderr, "Could not obtain the video output display mode iterator - result = %08x\n", (unsigned int)result);
        goto bail;
    }

    // List all supported output display modes
    printf("Supported video output display modes:\n");
    while (displayModeIterator->Next(&displayMode) == S_OK)
    {
        BSTR			displayModeBSTR = NULL;

        result = displayMode->GetName(&displayModeBSTR);
        if (result == S_OK)
        {
            _bstr_t					modeName(displayModeBSTR, false);
            int						modeWidth;
            int						modeHeight;
            BMDTimeValue			frameRateDuration;
            BMDTimeScale			frameRateScale;
            int						pixelFormatIndex = 0; // index into the gKnownPixelFormats / gKnownFormatNames arrays
            BMDDisplayModeSupport	displayModeSupport;

            // Obtain the display mode's properties
            modeWidth = displayMode->GetWidth();
            modeHeight = displayMode->GetHeight();
            displayMode->GetFrameRate(&frameRateDuration, &frameRateScale);
            printf(" %-20s \t %d x %d \t %7g FPS\t", (char*)modeName, modeWidth, modeHeight, (double)frameRateScale / (double)frameRateDuration);

            // Print the supported pixel formats for this display mode
            while ((knownPixelFormats[pixelFormatIndex] != 0) && (knownPixelFormatNames[pixelFormatIndex] != NULL))
            {
                if ((deckLinkOutput->DoesSupportVideoMode(displayMode->GetDisplayMode(), knownPixelFormats[pixelFormatIndex], bmdVideoOutputFlagDefault, &displayModeSupport, NULL) == S_OK)
                        && (displayModeSupport != bmdDisplayModeNotSupported))
                {
                    printf("%s\t", knownPixelFormatNames[pixelFormatIndex]);
                }
                pixelFormatIndex++;
            }

            printf("\n");
        }

        // Release the IDeckLinkDisplayMode object to prevent a leak
        displayMode->Release();
    }

    printf("\n");

bail:
    // Ensure that the interfaces we obtained are released to prevent a memory leak
    if (displayModeIterator != NULL)
        displayModeIterator->Release();

    if (deckLinkOutput != NULL)
        deckLinkOutput->Release();
}

bool DeckLinkOutput::enableCard()
{
    BMDDisplayModeSupport displayModeSupport;
    IDeckLinkDisplayMode* displayMode;

    // FIXME: Issue with Decklink Duo 2
    // Add flag to setOutputFlag for testing | bmdVideoOutputVANC);
    BMDVideoOutputFlags setOutputFlag = (BMDVideoOutputFlags)(bmdVideoOutputFlagDefault | bmdVideoOutputVITC);

    outputCard->DoesSupportVideoMode(setDisplayMode, pixelFormat, setOutputFlag, &displayModeSupport, &displayMode);

    if (displayModeSupport != bmdDisplayModeNotSupported)
    {
        outputCard->EnableAudioOutput(sampleRate, sampleType, channelCount, streamType);
        outputCard->EnableVideoOutput(setDisplayMode, setOutputFlag);

        width = displayMode->GetWidth();
        height = displayMode->GetHeight();
        rowBytes = width * 2;
        frameSize = height * rowBytes;
        displayMode->GetFrameRate(&frameDuration, &frameTimescale);
        originalFrameDuration = frameDisplayTime = frameDuration;
        canPreview = true;
        startFrame = 0;
        ratio = 25.0 / 1000;
        totalFrames = 0;
        prerollCount = 0;
        endFrame = 0;
        playing = false;
        totalAudioSamples = 0;

        this->loadBars();

        return true;
    }

    return false;
}

void DeckLinkOutput::loadBars()
{
    if(filename != "")
    {
        char* arr = new char[frameSize];

        QFile file(filename);
        if(file.open(QIODevice::ReadWrite))
        {
            QDataStream stream(&file);
            stream.readRawData(arr, frameSize);
        }

        this->outputVideo((uint8_t*)arr, frameSize, true, 0);
        delete[] arr;
    }
}

void DeckLinkOutput::disableCard()
{
    BOOL active;
    outputCard->IsScheduledPlaybackRunning(&active);
    if(active)
        outputCard->StopScheduledPlayback(0, NULL, 0);

    outputCard->DisableVideoOutput();
    outputCard->DisableAudioOutput();
}

const QString DeckLinkOutput::getDeviceName() const
{
    return deviceName;
}

const bool DeckLinkOutput::changeFormat(const QString& format)
{
    if(!filename.contains(format))
    {
        disableCard();
        filename = "bars/" + format + ".yuv";

        if(format == "PAL" || format == "PAL 16:9")
        {
            filename = "bars/PAL.yuv";
            setDisplayMode = bmdModePAL;
        }
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

    return true;
}

void DeckLinkOutput::setRate(const double rate)
{
    frameDisplayTime = frameDuration / rate;
    ratio = 25.0 * rate / 1000;
}

void DeckLinkOutput::setStartFrame(const int64_t start_frame)
{
    startFrame = start_frame;
}

void DeckLinkOutput::adjustFPS(const double adjust)
{
    frameDuration = originalFrameDuration / adjust;
    frameDisplayTime = frameDuration;
}

void DeckLinkOutput::startPlayback(Player* player)
{
    while(playing)
    {
        Sleep(1);
    }

    totalFrames = 0;
    prerollCount = 0;
    endFrame = 0;
    playing = false;
    totalAudioSamples = 0;
    canPreview = false;
    this->player = player;

    if(player != NULL)
    {
        outputCard->SetScheduledFrameCompletionCallback(this);
        outputCard->SetAudioCallback(this);

        int count = 0;
        for(int i=0; i<10; i++)
        {
            AVDecodedFrame* v = NULL;
            do
            {
                v = player->getNextVideoFrame();

                // Ensures there is no deadlock if the file is too small
                if(count++ > 100)
                    break;
            }
            while(v == NULL);

            if(v != NULL)
            {
                this->outputVideo(v->getBuffer(), v->getSize(), false, startFrame + v->getClockPTS() * ratio);
                delete v;
            }
        }

        outputCard->BeginAudioPreroll();
    }
    else
    {
        outputCard->SetScheduledFrameCompletionCallback(NULL);
        outputCard->SetAudioCallback(NULL);

        outputCard->StartScheduledPlayback(0, frameTimescale, 1.0);
        playing = true;
    }
}

void DeckLinkOutput::stopPlayback(const bool immediate)
{
    if(immediate)
    {
        LOG4CXX_DEBUG(Logger::getLogger("DeckLinkOutput"), "Immediate stop playback");

        outputCard->StopScheduledPlayback(0, NULL, 0);
        totalFrames = 0;
        prerollCount = 0;
        endFrame = 0;
        totalAudioSamples = 0;
        canPreview = true;
        playing = false;
    }
    else
    {
        if(endFrame > 0 && totalFrames > endFrame)
        {
            LOG4CXX_DEBUG(Logger::getLogger("DeckLinkOutput"), "Stopping playback at frame: " + QString::number(endFrame-1).toStdString());
            outputCard->StopScheduledPlayback((endFrame-1) * frameDisplayTime, NULL, frameTimescale);
        }
        else
        {
            LOG4CXX_DEBUG(Logger::getLogger("DeckLinkOutput"), "Stopping playback at frame: " + QString::number(totalFrames-1).toStdString());
            outputCard->StopScheduledPlayback((totalFrames-1) * frameDisplayTime, NULL, frameTimescale);
        }
    }
}

void DeckLinkOutput::outputVideo(const uint8_t* videoBuffer, const int size, const bool preview, const int64_t timecode)
{
    IDeckLinkMutableVideoFrame*	pDLVideoFrame;
    void* pFrame;

    HRESULT res = outputCard->CreateVideoFrame(width, height, rowBytes, pixelFormat, bmdFrameFlagDefault, &pDLVideoFrame);
    if(res == S_OK)
    {
        pDLVideoFrame->GetBytes((void**)&pFrame);

        if(size > frameSize)
        {
            memcpy(pFrame, videoBuffer + videoOffset, frameSize);
            // TODO: check this with decklink duo 2
            /*IDeckLinkVideoFrameAncillary* vanc;
            if(outputCard->CreateAncillaryData(pixelFormat, &vanc) == S_OK && vanc != NULL)
            {
                BMDDisplayMode vancDisplayMode = vanc->GetDisplayMode();
                LOG4CXX_ERROR(Logger::getLogger("DeckLinkOutput"), "create vanc:" + vancDisplayMode);
                for(int i=0; i<vancRows; i++)
                {
                    void* buffer;
                    if(vanc->GetBufferForVerticalBlankingLine(i+1, &buffer) == S_OK && buffer)
                    {
                        memcpy(buffer, videoBuffer + i * rowBytes, rowBytes);
                        LOG4CXX_ERROR(Logger::getLogger("DeckLinkOutput"), "vanc buffer:" + i * rowBytes);
                    }
                }

                pDLVideoFrame->SetAncillaryData(vanc);
                vanc->Release();
                LOG4CXX_ERROR(Logger::getLogger("DeckLinkOutput"), "done vanc");
            }*/
        }
        else memcpy(pFrame, videoBuffer, frameSize);

        int64_t frames = timecode;
        unsigned char fr = frames % 25;
        frames /= 25;
        unsigned char sec = frames % 60;
        frames /= 60;
        unsigned char min = frames % 60;
        frames /= 60;
        unsigned char hr = frames;

        pDLVideoFrame->SetTimecodeFromComponents(bmdTimecodeVITC, hr, min, sec, fr, bmdTimecodeFlagDefault);

        if(preview)
        {
            if(canPreview)
                outputCard->DisplayVideoFrameSync(pDLVideoFrame);
        }
        else
        {
            res = outputCard->ScheduleVideoFrame(pDLVideoFrame, totalFrames * frameDisplayTime, frameDisplayTime, frameTimescale);
            if(res == S_OK)
                totalFrames++;
            else LOG4CXX_ERROR(Logger::getLogger("DeckLinkOutput"), "Error scheduling video frame: " + QString::number(res).toStdString());
        }

        pDLVideoFrame->Release();

        BMDReferenceStatus referenceStatus;
        res = outputCard->GetReferenceStatus(&referenceStatus);
        if(referenceStatus != bmdReferenceLocked && referenceStatus != bmdReferenceNotSupportedByHardware)
            emit genlockError();
    }
}

void DeckLinkOutput::outputAudio(const uint8_t* audioBuffer, const int size)
{
    // Divide audio buffer size by 2*channels
    int sample_size = size / 16;

    // Check for playback active if in bridge mode
    BOOL active;
    outputCard->IsScheduledPlaybackRunning(&active);
    if(!active && prerollCount == 0)
        outputCard->StartScheduledPlayback(0, frameTimescale, 1.0);

    HRESULT res = outputCard->ScheduleAudioSamples((void*)audioBuffer, sample_size, totalAudioSamples, sampleRate, NULL);

    if(res == S_OK)
        totalAudioSamples += sample_size;
    else LOG4CXX_ERROR(Logger::getLogger("DeckLinkOutput"), "Error scheduling audio samples: " + QString::number(res).toStdString());
}

HRESULT STDMETHODCALLTYPE DeckLinkOutput::ScheduledFrameCompleted(IDeckLinkVideoFrame*, BMDOutputFrameCompletionResult result)
{
    if(playing)
    {
        if(result == bmdOutputFrameDisplayedLate)
        {
            LOG4CXX_WARN(Logger::getLogger("DeckLinkOutput"), "Frame displayed late");
            totalFrames += 1;
        }
        else if(result == bmdOutputFrameDropped)
        {
            LOG4CXX_WARN(Logger::getLogger("DeckLinkOutput"), "Dropped frame");
        }
        else if(result == bmdOutputFrameFlushed)
             LOG4CXX_WARN(Logger::getLogger("DeckLinkOutput"), "Flushed frame");

        int count = 0;
        AVDecodedFrame* v = NULL;
        do
        {
            v = player->getNextVideoFrame();

            if(v == NULL) // Check for end of file
            {
                LOG4CXX_DEBUG(Logger::getLogger("DeckLinkOutput"), "Reached EOF");
                endFrame = totalFrames;
            }
            // Ensures there is no deadlock
            if(count++ > 10)
                break;
        }
        while(v == NULL);

        if(v != NULL)
        {
            this->outputVideo(v->getBuffer(), v->getSize(), false, startFrame + v->getClockPTS() * ratio);
            delete v;
        }
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeckLinkOutput::RenderAudioSamples(BOOL preroll)
{
    if(preroll && prerollCount++ > 20)
    {
        outputCard->EndAudioPreroll();
        outputCard->StartScheduledPlayback(0, frameTimescale, 1.0);
        playing = true;
        LOG4CXX_DEBUG(Logger::getLogger("DeckLinkOutput"), "Playback started");
    }
    else
    {
        if(playing || preroll)
        {
            // Check the count of buffered audio samples and only schedule if below waterlevel
            uint32_t bufferedSampleFrameCount;
            outputCard->GetBufferedAudioSampleFrameCount(&bufferedSampleFrameCount);

            if(bufferedSampleFrameCount < (uint32_t)sampleRate)
            {
                int count = 0;
                AVDecodedFrame* a = NULL;
                do
                {
                    a = player->getNextAudioSample();

                    // Ensures there is no deadlock
                    if(count++ > 10)
                        break;
                }
                while(a == NULL);

                if(a != NULL)
                {
                    this->outputAudio(a->getBuffer(), a->getSize());
                    delete a;
                }
            }
        }
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeckLinkOutput::ScheduledPlaybackHasStopped()
{
    LOG4CXX_DEBUG(Logger::getLogger("DeckLinkOutput"), "Playback stopped at frame: " + QString::number(totalFrames).toStdString());

    totalFrames = 0;
    prerollCount = 0;
    endFrame = 0;
    playing = false;
    totalAudioSamples = 0;
    canPreview = true;

    return S_OK;
}
