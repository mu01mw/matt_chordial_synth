


juce::SynthesiserVoice* chordial::synth::ChordialSynthesiser::findVoiceToSteal(juce::SynthesiserSound* soundToPlay, int midiChannel, int midiNoteNumber) const
{
	jassert(!voices.isEmpty());

	// this is a list of voices we can steal, sorted by how long they've been running
	juce::Array<juce::SynthesiserVoice*> usableVoices;
	usableVoices.ensureStorageAllocated(voices.size());

	for (auto* voice : voices)
	{
		if (voice->canPlaySound(soundToPlay))
		{
			jassert(voice->isVoiceActive()); // We wouldn't be here otherwise

			usableVoices.add(voice);

			// NB: Using a functor rather than a lambda here due to scare-stories about
			// compilers generating code containing heap allocations..
			struct Sorter
			{
				bool operator() (const juce::SynthesiserVoice* a, const juce::SynthesiserVoice* b) const noexcept { return a->wasStartedBefore(*b); }
			};

			std::sort(usableVoices.begin(), usableVoices.end(), Sorter());
		}
	}

	// The oldest note that's playing with the target pitch is ideal..
	for (auto* voice : usableVoices)
		if (voice->getCurrentlyPlayingNote() == midiNoteNumber)
			return voice;

	// Oldest voice that has been released (no finger on it and not held by sustain pedal)
	for (auto* voice : usableVoices)
		if (voice->isPlayingButReleased())
			return voice;

	// Oldest voice that doesn't have a finger on it:
	for (auto* voice : usableVoices)
		if (!voice->isKeyDown())
			return voice;

	// Oldest voice that isn't protected
	for (auto* voice : usableVoices)
			return voice;

	jassert(false);
	return nullptr;
}
