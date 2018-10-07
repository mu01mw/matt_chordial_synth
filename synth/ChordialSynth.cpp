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
    masterOscillator->setWaveform(ChordialOscillatorMaster<float>::Waveform::saw);
    masterOscillator->setAntialiasing(true);
    masterOscillator->setPanoramicSpread(0.0f);
    masterOscillator->setDetuneAmount(0.000001);

    masterFilter = std::make_shared<ChordialFilterMaster<float>>();

    // INIT MODULATION
    lfo1.setMasterOscillator(std::make_shared<ChordialOscillatorMaster<float>>());
    lfo1.getMasterOscillator()->setAntialiasing(false);
    lfo1.getMasterOscillator()->setWaveform(ChordialOscillatorMaster<float>::Waveform::triangle);
    lfo1.setBaseFrequency(3.0f);

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
    synth.addSound(sound.release());
    setNumberOfVoices(1);

    // INIT EFFECTS

    /*auto dir = File::getCurrentWorkingDirectory();
    int numTries = 0;
    while (!dir.getChildFile("Resources").exists() && numTries++ < 15)
    dir = dir.getParentDirectory();
    auto& convolution = processorChain.template get<convolutionIndex>(); // [5]
    convolution.loadImpulseResponse(dir.getChildFile("Resources").getChildFile("phone90s.wav"), true, false, 0); // [6]*/

    // INIT PARAMETERS
    auto initParam = [&](const juce::String& paramId, const juce::String& label, float rangeStart, float rangeEnd, float defaultValue, float interval = 0.0f, float skew = 1.0f)
    {
        apvtState.createAndAddParameter(paramId, label, juce::String(), juce::NormalisableRange<float>(rangeStart, rangeEnd, interval, skew), defaultValue, nullptr, nullptr);
        apvtState.addParameterListener(paramId, this);
    };

    initParam(ADSR1_ATTACK_PARAM, "ADSR1 Attack", 1.0f, 500.0f, 10.0f);
    initParam(ADSR1_DECAY_PARAM, "ADSR1 Decay", 1.0f, 10000.0f, 100.0f);
    initParam(ADSR1_SUSTAIN_PARAM, "ADSR1 Sustain", 0.0f, 1.0f, 0.5f);
    initParam(ADSR1_RELEASE_PARAM, "ADSR1 Release", 3.0f, 10000.0f, 2000.0f);

    initParam(ADSR2_ATTACK_PARAM, "ADSR2 Attack", 1.0f, 500.0f, 10.0f);
    initParam(ADSR2_DECAY_PARAM, "ADSR2 Decay", 1.0f, 10000.0f, 100.0f);
    initParam(ADSR2_SUSTAIN_PARAM, "ADSR2 Sustain", 0.0f, 1.0f, 0.5f);
    initParam(ADSR2_RELEASE_PARAM, "ADSR3 Release", 3.0f, 10000.0f, 2000.0f);

    initParam(LFO_AMT_PARAM, "LFO Amount", 0.0f, 0.5f, 0.0f, 0.0f, 0.5f);
    initParam(LFO_FREQ_PARAM, "LFO Frequency", 0.1f, 30.0f, 3.0f);
    initParam(DETUNE_AMT_PARAM, "Detune Oscillators", 0.000001f, 0.05f, 0.000001f);
    initParam(SPREAD_PARAM, "Panoramic Spread", 0.0f, 1.0f, 0.0f);
    initParam(FILTER_CUTOFF_PARAM, "Filter Cutoff", 20.0f, 20000.0f, 329.63f, 0.0f, 0.199f);
    initParam(FILTER_RESONANCE_PARAM, "Filter Resonance", 0.0f, 1.0f, 0.0f);
    initParam(FILTER_CUTOFF_MOD_DEPTH_PARAM, "Filter Cutoff Modulation Depth", 0.0f, 8.0f, 4.0f);
}

void ChordialSynth::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 2;

    for (int i = 0; i < synth.getNumVoices(); ++i) {
        auto voice = (ChordialVoice*)synth.getVoice(i);
        voice->prepare(spec);
    }

    fxChain.prepare(spec);

    auto downSampleRate = spec.sampleRate / controlRate;

    lfo1.prepare({ downSampleRate, spec.maximumBlockSize, 1 });
    
    masterADSR1.setSampleRate(downSampleRate);
    masterADSR2.setSampleRate(downSampleRate);
}

void ChordialSynth::setNumberOfVoices(int num)
{
    auto currentNumVoices = synth.getNumVoices();
    while (num != currentNumVoices)
    {
        if (num < currentNumVoices)
            synth.removeVoice(currentNumVoices - 1);
        else
        {
            auto voice = std::make_unique<ChordialVoice>(matrixCoreVoice, masterOscillator, masterFilter, masterADSR1, masterADSR2);
            synth.addVoice(voice.release());
        }

        currentNumVoices = synth.getNumVoices();
    }
}

int ChordialSynth::getNumberOfVoices()
{
    return synth.getNumVoices();
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
            lfo1.processSample();
            modMatrixGlobal.process();
        }

        synth.renderNextBlock(buffer, midiMessages, startSample, max);
    }
    buffer.applyGain(0.1f);

    //auto block = juce::dsp::AudioBlock<float>(buffer);
    //auto contextToUse = juce::dsp::ProcessContextReplacing<float>(block);
    //processorChain.process(contextToUse);
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
        lfo1.setBaseFrequency(newValue);
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
