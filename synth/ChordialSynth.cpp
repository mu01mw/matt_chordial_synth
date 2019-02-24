/*
  ==============================================================================

    ChordialSynth.cpp
    Created: 2 Sep 2018 4:42:09pm
    Author:  matth

  ==============================================================================
*/

namespace chordial
{
namespace synth
{

ChordialSynth::ChordialSynth(juce::AudioProcessorValueTreeState& state) : apvtState(state)
{
    masterOscillator = std::make_shared<ChordialOscillatorMaster<float>>();
    masterOscillator->setWaveform(ChordialOscillatorMaster<float>::Waveform::triangle);
    masterOscillator->setAntialiasing(true);
    masterOscillator->setPanoramicSpread(1.0f);
    masterOscillator->setDetuneAmount(0.01);
    masterOscillator->setFrequencyModulationDepth(0.05f);

    masterADSR2.setAttackTimeMs(100.0f);
    masterADSR2.setDecayTimeMs(2000.0f);
    masterADSR2.setSustainValue(0.25f);

    masterFilter = std::make_shared<ChordialFilterMaster<float>>();
    masterFilter->setResonance(0.75f);

    // INIT MODULATION
    lfo1.setMasterOscillator(std::make_shared<ChordialOscillatorMaster<float>>());
    lfo1.getMasterOscillator()->setAntialiasing(false);
    lfo1.getMasterOscillator()->setWaveform(ChordialOscillatorMaster<float>::Waveform::triangle);
    lfo1.setBaseFrequency(5.0f);

    // INIT MODMATRIX
    matrixCoreVoice = std::make_shared<ChordialModMatrixCore>();
    matrixCoreGlobal = std::make_shared<ChordialModMatrixCore>();
    modMatrixGlobal.setCore(matrixCoreGlobal);

    modMatrixGlobal.addModSource({ GLOBAL_LFO1_OUT, lfo1.getOutputPtr() });
    modMatrixGlobal.addModDestination({ GLOBAL_OSC_MASTER_FM_IN, masterOscillator->getFMInputPtr() });
    modMatrixGlobal.addModDestination({ GLOBAL_FILTER_MASTER_CUTOFF_IN, masterFilter->getCutoffModPtr() });
    
    matrixCoreGlobal->addRow(GLOBAL_LFO1_OUT, GLOBAL_OSC_MASTER_FM_IN, true);
    matrixCoreGlobal->addRow(GLOBAL_LFO1_OUT, GLOBAL_FILTER_MASTER_CUTOFF_IN, false);
    
    matrixCoreVoice->addRow(VOICE_ADSR1_OUT, VOICE_DCA_GAIN_IN, true);
    matrixCoreVoice->addRow(VOICE_ADSR2_OUT, VOICE_FILTER_MASTER_CUTOFF_IN, true);

    // INIT VOICES
    auto sound = std::make_unique<ChordialSound>();
    synthesiser.addSound(sound.release());
    setNumberOfVoices(1);

    // INIT EFFECTS

    /*auto dir = juce::File::getCurrentWorkingDirectory();
    int numTries = 0;
    while (!dir.getChildFile("Resources").exists() && numTries++ < 15)
    dir = dir.getParentDirectory();
    auto& convolution = fxChain.template get<(int)fxIndices::convolutionIndex>(); // [5]
    convolution.loadImpulseResponse(dir.getChildFile("Resources").getChildFile("phone90s.wav"), true, false, 0); // [6]
	*/

    // INIT PARAMETERS
    auto initParam = [&](const juce::String& paramId, const juce::String& label, float rangeStart, float rangeEnd, float defaultValue, float interval = 0.0f, float skew = 1.0f)
    {
        apvtState.createAndAddParameter(paramId, label, juce::String(), juce::NormalisableRange<float>(rangeStart, rangeEnd, interval, skew), defaultValue, nullptr, nullptr);
        apvtState.addParameterListener(paramId, this);
    };

    initParam(ADSR1_ATTACK_PARAM, "Attack", 1.0f, 500.0f, masterADSR1.getAttackTimeMs());
    initParam(ADSR1_DECAY_PARAM, "Decay", 1.0f, 10000.0f, masterADSR1.getDecayTimeMs());
    initParam(ADSR1_SUSTAIN_PARAM, "Sustain", 0.0f, 1.0f, masterADSR1.getSustainValue());
    initParam(ADSR1_RELEASE_PARAM, "Release", 3.0f, 10000.0f, masterADSR1.getReleaseTimeMs());

    initParam(ADSR2_ATTACK_PARAM, "Attack", 1.0f, 500.0f, masterADSR2.getAttackTimeMs());
    initParam(ADSR2_DECAY_PARAM, "Decay", 1.0f, 10000.0f, masterADSR2.getDecayTimeMs());
    initParam(ADSR2_SUSTAIN_PARAM, "Sustain", 0.0f, 1.0f, masterADSR2.getSustainValue());
    initParam(ADSR2_RELEASE_PARAM, "Release", 3.0f, 10000.0f, masterADSR2.getReleaseTimeMs());

    initParam(LFO_AMT_PARAM, "LFO Amount", 0.0f, 0.5f, masterOscillator->getFrequencyModulationDepth(), 0.0f, 0.5f);
    initParam(LFO_FREQ_PARAM, "LFO Frequency", 0.1f, 30.0f, lfo1.getBaseFrequency());
    initParam(DETUNE_AMT_PARAM, "Detune", 0.000001f, 0.05f, masterOscillator->getDetuneAmount());
    initParam(SPREAD_PARAM, "Panoramic Spread", 0.0f, 1.0f, masterOscillator->getPanoramicSpread());
    initParam(FILTER_CUTOFF_PARAM, "Cutoff", 20.0f, 20000.0f, masterFilter->getCutoff(), 0.0f, 0.199f);
    initParam(FILTER_RESONANCE_PARAM, "Resonance", 0.0f, 1.0f, masterFilter->getResonance());
    initParam(FILTER_CUTOFF_MOD_DEPTH_PARAM, "Cutoff Mod (Env2)", 0.0f, 8.0f, masterFilter->getCutoffModDepth());
}

void ChordialSynth::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	synthesiser.setCurrentPlaybackSampleRate(sampleRate);
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 2;

    for (int i = 0; i < synthesiser.getNumVoices(); ++i) {
        auto voice = (ChordialVoice*)synthesiser.getVoice(i);
        voice->prepare(spec);
    }

    //fxChain.prepare(spec);

    auto downSampleRate = spec.sampleRate / controlRate;

    lfo1.prepare({ downSampleRate, spec.maximumBlockSize, 1 });
    
    masterADSR1.setSampleRate(downSampleRate);
    masterADSR2.setSampleRate(downSampleRate);
}

void ChordialSynth::setNumberOfVoices(int num)
{
    auto currentNumVoices = synthesiser.getNumVoices();
    while (num != currentNumVoices)
    {
        if (num < currentNumVoices)
			synthesiser.removeVoice(currentNumVoices - 1);
        else
        {
            auto voice = std::make_unique<ChordialVoice>(matrixCoreVoice, masterOscillator, masterFilter, masterADSR1, masterADSR2);
			synthesiser.addVoice(voice.release());
        }

        currentNumVoices = synthesiser.getNumVoices();
    }
}

int ChordialSynth::getNumberOfVoices()
{
    return synthesiser.getNumVoices();
}

void ChordialSynth::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer & midiMessages)
{
    auto numSamples = buffer.getNumSamples();
    for (size_t pos = 0; pos < numSamples;)
    {
        auto max = juce::jmin(static_cast<size_t> (numSamples - pos), controlUpdateCounter);
        auto startSample = pos;

        pos += max;
        controlUpdateCounter -= max;
        if (controlUpdateCounter == 0)
        {
            controlUpdateCounter = controlRate;
			lfo1.updateOscillatorFrequency(true);
            lfo1.processSample();
            modMatrixGlobal.process();
        }

		synthesiser.renderNextBlock(buffer, midiMessages, startSample, max);
    }
    buffer.applyGain(0.1f);

    /*auto block = juce::dsp::AudioBlock<float>(buffer);
    auto contextToUse = juce::dsp::ProcessContextReplacing<float>(block);
    fxChain.process(contextToUse);*/
}

void ChordialSynth::parameterChanged(const juce::String & parameterID, float newValue)
{
    if (parameterID == ADSR1_ATTACK_PARAM)
        masterADSR1.setAttackTimeMs(newValue);
    else if (parameterID == ADSR1_DECAY_PARAM)
        masterADSR1.setDecayTimeMs(newValue);
    else if (parameterID == ADSR1_SUSTAIN_PARAM)
        masterADSR1.setSustainValue(newValue);
    else if (parameterID == ADSR1_RELEASE_PARAM)
        masterADSR1.setReleaseTimeMs(newValue);

    else if (parameterID == ADSR2_ATTACK_PARAM)
        masterADSR2.setAttackTimeMs(newValue);
    else if (parameterID == ADSR2_DECAY_PARAM)
        masterADSR2.setDecayTimeMs(newValue);
    else if (parameterID == ADSR2_SUSTAIN_PARAM)
        masterADSR2.setSustainValue(newValue);
    else if (parameterID == ADSR2_RELEASE_PARAM)
        masterADSR2.setReleaseTimeMs(newValue);

    else if (parameterID == LFO_FREQ_PARAM)
        lfo1.setBaseFrequencyWithoutUpdating(newValue);
    else if (parameterID == LFO_AMT_PARAM)
        masterOscillator->setFrequencyModulationDepth(newValue);
    else if (parameterID == SPREAD_PARAM)
        masterOscillator->setPanoramicSpread(newValue);
    else if (parameterID == DETUNE_AMT_PARAM)
        masterOscillator->setDetuneAmount(newValue);
    else if (parameterID == FILTER_CUTOFF_PARAM)
        masterFilter->setCutoff(newValue);
    else if (parameterID == FILTER_RESONANCE_PARAM)
        masterFilter->setResonance(newValue);
    else if (parameterID == FILTER_CUTOFF_MOD_DEPTH_PARAM)
        masterFilter->setCutoffModDepth(newValue);
}

}
}
