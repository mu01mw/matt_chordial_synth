/*
  ==============================================================================

    ChordialDCA.h
    Created: 14 Sep 2018 1:28:49pm
    Author:  matth

  ==============================================================================
*/

#pragma once
namespace chordial
{
namespace synth
{

template <typename SampleType>
class ChordialDCAVoice : public ChordialModuleVoice<SampleType>
{
public:

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        gain.prepare(spec);
        this->sampleRate = spec.sampleRate;
        this->updateDownSampleRate();
    }

    template <typename ProcessContext>
    void process(const ProcessContext &context)
    {
        gain.setGainLinear(gainModulationInput);
        gain.process(context);
    }

    void reset()
    {
        gain.reset();
    }

    SampleType* getGainModInputPtr() { return &gainModulationInput; }

private:
    void updateSmoothing(SampleType time) override
    {
        gain.setRampDurationSeconds(time);
    }

    juce::dsp::Gain<SampleType> gain;
    SampleType gainModulationInput{ static_cast<SampleType>(0.0) };
};

}
}
