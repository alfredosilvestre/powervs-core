#include "muxer.h"

Muxer::Muxer()
{
    outputContext = NULL;
    videoTimeBase = 0.0;
    videoStream = NULL;
}

Muxer::~Muxer()
{

}

bool Muxer::initOutputFile(const char* filename, QString format, const char* timecode)
{
    outputContext = NULL;
    QString format_name;
    if(format.contains("imx"))
        format_name = "mxf_d10";
    else if(format.contains("xdcamHD422"))
        format_name = "mxf";

    if(avformat_alloc_output_context2(&outputContext, NULL, format_name.toStdString().c_str(), filename) < 0)
    {
        cleanup();
        return false;
    }

    av_dict_set(&outputContext->metadata, "timecode", timecode, 0);

    videoTimeBase = videoEncoder.initialize(format, outputContext);
    if(videoTimeBase == -1)
    {
        cleanup();
        return false;
    }
    videoStream = outputContext->streams[0];
    if(!audioEncoder.initialize(format, outputContext))
    {
        cleanup();
        return false;
    }

	// Dump information about output file
    //av_dump_format(outputContext, 0, filename, 1);

    // Open the output file, if needed
    if(!(outputContext->oformat->flags & AVFMT_NOFILE) && (avio_open(&outputContext->pb, filename, AVIO_FLAG_WRITE) < 0))
    {
        cleanup();
        return false;
    }

    // Write the muxer header
    if(avformat_write_header(outputContext, NULL) < 0)
    {
        cleanup();
        return false;
    }

    return true;
}

double Muxer::muxAudioFrame(AVDecodedFrame* audioBuffer)
{
    return audioEncoder.muxAudioFrame(audioBuffer);
}

double Muxer::muxVideoFrame(AVDecodedFrame* videoBuffer)
{
    return videoEncoder.muxVideoFrame(videoBuffer);
}

int64_t Muxer::getCurrentRecordTime()
{
    if(videoStream != NULL)
        return videoStream->nb_frames * videoTimeBase;
    return 0;
}

int64_t Muxer::closeOutputFile()
{
    int64_t duration = 0;

    if(outputContext != NULL)
    {
        // Flush the encoders
        muxAudioFrame(NULL);
        muxVideoFrame(NULL);

        // Write the muxer trailer
        av_write_trailer(outputContext);

        // Get the final duration
        duration = getCurrentRecordTime();
    }

    cleanup();

    return duration;
}

int Muxer::getVideoCodecAddress()
{
    return videoEncoder.getVideoCodecAddress();
}

void Muxer::cleanup()
{
    if(outputContext != NULL)
    {
        // Close the encoders
        videoEncoder.cleanup();
        audioEncoder.cleanup();

        // Close the output file
        if(outputContext->pb != NULL)
            avio_close(outputContext->pb);

        // Free the context and its streams
        avformat_free_context(outputContext);
    }
    outputContext = NULL;

    videoTimeBase = 0.0;
    videoStream = NULL;
}
