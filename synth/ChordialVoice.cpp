/*
  ==============================================================================

    ChordialVoice.cpp
    Created: 27 Aug 2018 11:37:44am
    Author:  matth

  ==============================================================================
*/

namespace chordial
{
namespace synth
{

ChordialVoice::ChordialVoice(std::shared_ptr<ChordialModMatrixCore> matrixCore,
                             std::shared_ptr<ChordialOscillatorMaster<float>> masterOscillator,
                             std::shared_ptr<ChordialFilterMaster<float>> masterFilter,
                             ChordialMasterADSR<float, float>& masterADSR1,
                             ChordialMasterADSR<float, float>& masterADSR2) : adsr1(masterADSR1), adsr2(masterADSR2)
{
    auto& o1 = processorChain.template get<osc1>();
    o1.setMasterOscillator(masterOscillator);
    o1.setDetuneMultiplier(1.0f);
    o1.setPanoramicSpreadMultiplier(1.0f);
    
    auto& o2 = processorChain.template get<osc2>();
    o2.setMasterOscillator(masterOscillator);
    o2.setDetuneMultiplier(-1.0f);
    o2.setPanoramicSpreadMultiplier(-1.0f);

    auto& o3 = processorChain.template get<osc3>();
    o3.setMasterOscillator(masterOscillator);
    o3.setDetuneMultiplier(0.0f);
    o3.setPanoramicSpreadMultiplier(0.0f);

    auto& f = processorChain.template get<filter>();
    f.setMasterFilter(masterFilter);

    modMatrix.setCore(matrixCore);

    modMatrix.addModSource({ VOICE_ADSR1_OUT, adsr1.getOutputPtr() });
    modMatrix.addModSource({ VOICE_ADSR2_OUT, adsr2.getOutputPtr() });
    modMatrix.addModDestination({ VOICE_FILTER_MASTER_CUTOFF_IN, f.getCutoffModVoicePtr() });
    modMatrix.addModDestination({ VOICE_DCA_GAIN_IN, processorChain.template get<gain>().getGainModInputPtr() });
    
}

void ChordialVoice::prepare(const juce::dsp::ProcessSpec & spec)
{
    tempBlock = juce::dsp::AudioBlock<float>(heapBlock, spec.numChannels, spec.maximumBlockSize);
    processorChain.prepare(spec);

    auto& o1 = processorChain.template get<osc1>();
    auto& o2 = processorChain.template get<osc2>();
    auto& dca = processorChain.template get<gain>();
    dca.setSamplesPerControlSignal(controlRate);
    o1.setSamplesPerControlSignal(controlRate);
    o2.setSamplesPerControlSignal(controlRate);
}

bool ChordialVoice::canPlaySound(juce::SynthesiserSound *)
{
    return true;
}

void ChordialVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound * sound, int currentPitchWheelPosition)
{
    auto hz = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

    auto& o1 = processorChain.template get<osc1>();
    o1.setBaseFrequency(hz);

    auto& o2 = processorChain.template get<osc2>();
    o2.setBaseFrequency(hz);

    auto& o3 = processorChain.template get<osc3>();
    o3.setBaseFrequency(hz);

    auto& f = processorChain.template get<filter>();
    f.setNoteNumber(midiNoteNumber);
    f.reset();

    adsr1.gate(true);
    adsr2.gate(true);

}

void ChordialVoice::stopNote(float velocity, bool allowTailOff)
{
    adsr1.gate(false);
    adsr2.gate(false);
    if (!allowTailOff)
    {
        adsr1.reset();
        adsr2.reset();
        clearCurrentNote();
    }
}

void ChordialVoice::pitchWheelMoved(int newPitchWheelValue)
{
}

void ChordialVoice::controllerMoved(int controllerNumber, int newControllerValue)
{
}

void ChordialVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (adsr1.isActive() || adsr2.isActive())
    {
        auto subBlock = tempBlock.getSubBlock((size_t)startSample, (size_t)numSamples);
        subBlock.clear();


        for (size_t pos = 0; pos < numSamples;)
        {
            auto max = juce::jmin(static_cast<size_t> (numSamples - pos), controlUpdateCounter);
            auto block = subBlock.getSubBlock(pos, max);
            juce::dsp::ProcessContextReplacing<float> context(block);
            
            pos += max;
            controlUpdateCounter -= max;
            if (controlUpdateCounter == 0)
            {
                controlUpdateCounter = controlRate;
                adsr1.getNextValue();
                adsr2.getNextValue();
                modMatrix.process();
                if (!adsr1.isActive() && !adsr2.isActive())
                    clearCurrentNote();
            }
            processorChain.process(context);
        }

        juce::dsp::AudioBlock<float>(outputBuffer)
            .getSubBlock((size_t)startSample, (size_t)numSamples)
            .add(subBlock);
    }
}
}
}
