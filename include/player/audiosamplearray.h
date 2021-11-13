#ifndef AUDIOSAMPLEARRAY_H
#define AUDIOSAMPLEARRAY_H

#include <stdint.h>

class AudioSampleArray
{
    public:
        AudioSampleArray(uint8_t** samples, int linesize, int nb_samples);
        ~AudioSampleArray();

    public:
        uint8_t** getSamples();
        int getLinesize();
        int getNbSamples();

    private:
        uint8_t** samples;
        int linesize;
        int nb_samples;
};

#endif // AUDIOSAMPLEARRAY_H
