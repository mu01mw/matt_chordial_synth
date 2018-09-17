/*
  ==============================================================================

    ChordialFIlter.h
    Created: 12 Sep 2018 8:56:11pm
    Author:  matth

  ==============================================================================
*/

#pragma once
namespace chordial
{
namespace synth
{

template <typename SampleType>
class ChordialFilterVoice;

template <typename SampleType>
class ChordialFilterMaster
{
public:
	ChordialFilterMaster() {}

	void setCutoff(SampleType hz)
	{
		cutoff.store(hz);
	}

	void setResonance(SampleType value)
	{
		resonance.store(value);
	}

	void setCutoffModDepth(SampleType value)
	{
		cutoffModDepth.store(value);
	}

	SampleType* getCutoffModPtr()
	{
		return &cutoffMod;
	}
private:
	friend class ChordialFilterVoice<SampleType>;
	std::atomic<SampleType> cutoff{ static_cast<SampleType>(329.63) };
	std::atomic<SampleType> resonance{ static_cast<SampleType>(0.0) };
	SampleType cutoffMod{ static_cast<SampleType>(0.0) };
	std::atomic<SampleType> cutoffModDepth{ static_cast<SampleType>(4.0) };
};

template <typename SampleType>
class ChordialFilterVoice
{
public:
	ChordialFilterVoice()
	{
		filter.setMode(juce::dsp::LadderFilter<SampleType>::Mode::LPF24);
		filter.setEnabled(true);
		filter.setDrive(1);

	}

	void setMasterFilter(std::shared_ptr<ChordialFilterMaster<float>> masterFilter)
	{
		master = masterFilter;
	}

	void prepare(const juce::dsp::ProcessSpec& spec)
	{
		oversampling = std::make_unique<juce::dsp::Oversampling<SampleType>> (spec.numChannels, 2, juce::dsp::Oversampling<SampleType>::FilterType::filterHalfBandPolyphaseIIR);
		oversampling->initProcessing(spec.maximumBlockSize);

		juce::dsp::ProcessSpec specOS;
		specOS.sampleRate = spec.sampleRate * oversampling->getOversamplingFactor();
		specOS.maximumBlockSize = spec.maximumBlockSize * oversampling->getOversamplingFactor();
		specOS.numChannels = spec.numChannels;
		filter.prepare(specOS);
	}

	template <typename ProcessContext>
	void process(const ProcessContext &context)
	{
		updateCutoff();
		filter.setResonance(master->resonance.load());

		ProcessContext contextOS(oversampling->processSamplesUp(context.getInputBlock()));
		filter.process(contextOS);
		oversampling->processSamplesDown(context.getOutputBlock());
	}

	void reset()
	{
		filter.reset();
	}

	void setNoteNumber(int noteNumber)
	{
		keyboardTrackValue = std::pow(2.0, (noteNumber - 64) / 12.0);
	}

	SampleType* getCutoffModVoicePtr() { return &cutoffModVoice; }

private:
	void updateCutoff()
	{
		if (master != nullptr)
		{
			auto freq = keyboardTrackValue * master->cutoff.load() * std::pow(2.0,cutoffModVoice*master->cutoffModDepth.load());
			filter.setCutoffFrequencyHz(freq);
		}
	}

	std::shared_ptr<ChordialFilterMaster<float>> master;
	juce::dsp::LadderFilter<SampleType> filter;
	SampleType cutoffModVoice{ static_cast<SampleType>(0.0) };
	SampleType keyboardTrackValue{ static_cast<SampleType>(1.0) };
	std::unique_ptr<juce::dsp::Oversampling<SampleType>> oversampling;
};
}
}