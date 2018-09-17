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
	setNumberOfVoices(32);

	// INIT EFFECTS

	/*auto dir = File::getCurrentWorkingDirectory();
	int numTries = 0;
	while (!dir.getChildFile("Resources").exists() && numTries++ < 15)
	dir = dir.getParentDirectory();
	auto& convolution = processorChain.template get<convolutionIndex>(); // [5]
	convolution.loadImpulseResponse(dir.getChildFile("Resources").getChildFile("phone90s.wav"), true, false, 0); // [6]*/

	// INIT PARAMETERS
	apvtState.createAndAddParameter(ADSR1_ATTACK_PARAM, "ADSR1 Attack", juce::String(), juce::NormalisableRange<float>(1.0f, 500.0f), 10.0f, nullptr, nullptr);
	apvtState.createAndAddParameter(ADSR1_RELEASE_PARAM, "ADSR1 Release", juce::String(), juce::NormalisableRange<float>(3.0f, 10000.0f), 2000.0f, nullptr, nullptr);

	apvtState.createAndAddParameter(ADSR2_ATTACK_PARAM, "ADSR2 Attack", juce::String(), juce::NormalisableRange<float>(1.0f, 500.0f), 10.0f, nullptr, nullptr);
	apvtState.createAndAddParameter(ADSR2_RELEASE_PARAM, "ADSR2 Release", juce::String(), juce::NormalisableRange<float>(3.0f, 10000.0f), 2000.0f, nullptr, nullptr);

	apvtState.createAndAddParameter(LFO_AMT_PARAM, "LFO Amount", juce::String(), juce::NormalisableRange<float>(0.0f, 0.5f, 0.0f, 0.5f), 0.0f, nullptr, nullptr);
	apvtState.createAndAddParameter(LFO_FREQ_PARAM, "LFO Frequency", juce::String(), juce::NormalisableRange<float>(0.1f, 30.0f), 3.0f, nullptr, nullptr);
    apvtState.createAndAddParameter(DETUNE_AMT_PARAM, "Detune Oscillators", juce::String(), juce::NormalisableRange<float>(0.000001f, 0.05f), 0.000001f, nullptr, nullptr);
	apvtState.createAndAddParameter(SPREAD_PARAM, "Panoramic Spread", juce::String(), juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, nullptr, nullptr);
	apvtState.createAndAddParameter(FILTER_CUTOFF_PARAM, "Filter Cutoff", juce::String(), juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.199f), 329.63f, nullptr, nullptr);
	apvtState.createAndAddParameter(FILTER_RESONANCE_PARAM, "Filter Resonance", juce::String(), juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, nullptr, nullptr);
	apvtState.createAndAddParameter(FILTER_CUTOFF_MOD_DEPTH_PARAM, "Filter Cutoff Modulation Depth", juce::String(), juce::NormalisableRange<float>(0.0f, 8.0f), 4.0f, nullptr, nullptr);
    
	apvtState.addParameterListener(ADSR1_ATTACK_PARAM, this);
	apvtState.addParameterListener(ADSR1_RELEASE_PARAM, this);
	apvtState.addParameterListener(ADSR2_ATTACK_PARAM, this);
	apvtState.addParameterListener(ADSR2_RELEASE_PARAM, this);
    apvtState.addParameterListener(LFO_AMT_PARAM, this);
    apvtState.addParameterListener(LFO_FREQ_PARAM, this);
    apvtState.addParameterListener(DETUNE_AMT_PARAM, this);
	apvtState.addParameterListener(SPREAD_PARAM, this);
	apvtState.addParameterListener(FILTER_CUTOFF_PARAM, this);
	apvtState.addParameterListener(FILTER_RESONANCE_PARAM, this);
	apvtState.addParameterListener(FILTER_CUTOFF_MOD_DEPTH_PARAM, this);
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

void ChordialSynth::setWidth(float width)
{
	// Set variable that all voices can share
}

float ChordialSynth::getWidth()
{
	return 0.0f;
}

void ChordialSynth::setDetune(float detune)
{
	// Set variable that all voices can share
}

float ChordialSynth::getDetune()
{
	return 0.0f;
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