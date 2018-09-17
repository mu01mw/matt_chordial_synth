/*
  ==============================================================================

    ChordialSynth.h
    Created: 2 Sep 2018 4:42:09pm
    Author:  matth

  ==============================================================================
*/

#pragma once
namespace chordial
{
namespace synth
{

// PARAMETER NAMES
#define ADSR1_ATTACK_PARAM "adsr1_attack"
#define ADSR1_DECAY_PARAM "adsr1_decay"
#define ADSR1_SUSTAIN_PARAM "adsr1_sustain"
#define ADSR1_RELEASE_PARAM "adsr1_release"

#define ADSR2_ATTACK_PARAM "adsr2_attack"
#define ADSR2_DECAY_PARAM "adsr2_decay"
#define ADSR2_SUSTAIN_PARAM "adsr2_sustain"
#define ADSR2_RELEASE_PARAM "adsr2_release"

#define LFO_AMT_PARAM "lfo_amt"
#define LFO_FREQ_PARAM "lfo_freq"
#define DETUNE_AMT_PARAM "detune_amt"
#define SPREAD_PARAM "pan_spread"
#define FILTER_CUTOFF_PARAM "filter_cutoff"
#define FILTER_RESONANCE_PARAM "filter_resonance"
#define FILTER_CUTOFF_MOD_DEPTH_PARAM "filter_cutoff_mod_depth"

class ChordialSynth : private juce::AudioProcessorValueTreeState::Listener
{
public:
	ChordialSynth(juce::AudioProcessorValueTreeState& apvtState);
	void prepareToPlay(double sampleRate, int samplesPerBlock);
	void setNumberOfVoices(int num);
	int getNumberOfVoices();
	void setWidth(float width);
	float getWidth();
	void setDetune(float detune);
	float getDetune();
	void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

private:
	void parameterChanged(const juce::String &parameterID, float newValue);

	enum class fxIndices
	{
		convolutionIndex
	};
	juce::dsp::ProcessorChain<juce::dsp::Convolution> fxChain;

	juce::Synthesiser synth;
    std::shared_ptr<ChordialOscillatorMaster<float>> masterOscillator;
	std::shared_ptr<ChordialFilterMaster<float>> masterFilter;

	static constexpr size_t controlRate = 100;
	size_t controlUpdateCounter = controlRate;

	ChordialMasterADSR<float, float> masterADSR1;
	ChordialMasterADSR<float, float> masterADSR2;
	ChordialOscillatorVoice<float> lfo1;
	
	juce::AudioProcessorValueTreeState& apvtState;

	std::shared_ptr<ChordialModMatrixCore> matrixCoreVoice;

	std::shared_ptr<ChordialModMatrixCore> matrixCoreGlobal;
	ChordialModMatrix<float> modMatrixGlobal;
};

}
}