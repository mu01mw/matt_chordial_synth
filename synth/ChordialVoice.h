/*
  ==============================================================================

    ChordialVoice.h
    Created: 27 Aug 2018 11:37:44am
    Author:  matth

  ==============================================================================
*/

#pragma once
namespace chordial
{
namespace synth
{

struct ChordialSound : public juce::SynthesiserSound
{
	ChordialSound() {}
	bool appliesToNote(int) override { return true; }
	bool appliesToChannel(int) override { return true; }
};

class ChordialVoice : public juce::SynthesiserVoice
{
public:
    ChordialVoice(std::shared_ptr<ChordialModMatrixCore> matrixCore,
				  std::shared_ptr<ChordialOscillatorMaster<float>> masterOscillator,
				  std::shared_ptr<ChordialFilterMaster<float>> masterFilter,
                  ChordialMasterADSR<float, float>& masterADSR1,
				  ChordialMasterADSR<float, float>& masterADSR2);

	// Inherited via SynthesiserVoice
	void prepare(const juce::dsp::ProcessSpec& spec);
	virtual bool canPlaySound(juce::SynthesiserSound *) override;
	virtual void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound * sound, int currentPitchWheelPosition) override;
	virtual void stopNote(float velocity, bool allowTailOff) override;
	virtual void pitchWheelMoved(int newPitchWheelValue) override;
	virtual void controllerMoved(int controllerNumber, int newControllerValue) override;
	virtual void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
	
private:
	enum {
		osc1 = 0,
		osc2,
		osc3,
		filter,
		gain
	};

	juce::HeapBlock<char> heapBlock;
	juce::dsp::AudioBlock<float> tempBlock;

	juce::dsp::ProcessorChain<ChordialOscillatorVoice<float>, ChordialOscillatorVoice<float>, ChordialOscillatorVoice<float>, 
		ChordialFilterVoice<float>,
		ChordialDCAVoice<float>> processorChain;

	static constexpr size_t controlRate = 100;
	size_t controlUpdateCounter = controlRate;

	ChordialVoiceADSR<float, float> adsr1;
	ChordialVoiceADSR<float, float> adsr2;
	ChordialModMatrix<float> modMatrix;
};

}
}