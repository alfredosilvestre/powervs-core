#include "audiosamplearray.h"

AudioSampleArray::AudioSampleArray(uint8_t** samples, int linesize, int nb_samples)
{
    this->samples = samples;
    this->linesize = linesize;
    this->nb_samples = nb_samples;
}

AudioSampleArray::~AudioSampleArray()
{
}

uint8_t** AudioSampleArray::getSamples()
{
    return samples;
}

int AudioSampleArray::getLinesize()
{
    return linesize;
}

int AudioSampleArray::getNbSamples()
{
    return nb_samples;
}
