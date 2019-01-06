#pragma once
namespace chordial
{
namespace synth
{
	class ChordialSynthesiser : public juce::Synthesiser
	{
	private:
		juce::SynthesiserVoice* findVoiceToSteal(juce::SynthesiserSound *soundToPlay, int midiChannel, int midiNoteNumber) const override;
	};
}
}