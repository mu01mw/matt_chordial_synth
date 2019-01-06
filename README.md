# matt_chordial_synth

A module for JUCE containing various synthesiser components:

* Oscillator (Saw, triangle, square)
* Envelope generator & DCA
* Filter
* Modulation matrix

Components are designed to be used independently or in a juce::dsp::ProcessorChain. Audio signals are processed at the sampling rate; control signals are processed at a user-definable control rate, with smoothing.

The ChordialVoice and ChordialSynthesiser classes provide an example of how to connect the components together (3 oscillator, LFO, 2 envelope generators, DCA and filter).

A basic demo of the module can be found [here](https://github.com/mu01mw/ChordialSynthDemo)
