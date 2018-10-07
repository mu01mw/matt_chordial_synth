/*
  ==============================================================================

    ChordialModule.h
    Created: 14 Sep 2018 3:51:39pm
    Author:  matth

  ==============================================================================
*/

#pragma once

namespace chordial
{
namespace synth
{

template <typename SampleType>
class ChordialModuleVoice
{
public:
    virtual ~ChordialModuleVoice(){};
    void setSamplesPerControlSignal(int samples)
    {
        samplesPerControlSignal = samples;
        updateDownSampleRate();
    }

    

protected:
    virtual void updateSmoothing(SampleType time) {};

    double sampleRate{ 44100.0 };

    void updateDownSampleRate()
    {
        jassert(sampleRate > 0.0);
        jassert(samplesPerControlSignal > 0);

        auto downSampleRate = sampleRate / samplesPerControlSignal;
        updateSmoothing(1 / downSampleRate);
    }

    int samplesPerControlSignal{ 100 };
};

}
}
