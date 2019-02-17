/*
  ==============================================================================

    ChordialOscillator.h
    Created: 11 Sep 2018 12:59:56pm
    Author:  matth

  ==============================================================================
*/

#pragma once
namespace chordial
{
namespace synth
{

template <typename FloatType>
class ChordialOscillatorVoice;

template <typename FloatType>
class ChordialOscillatorMaster
{
public:
    enum class Waveform { /*sine,*/ triangle, saw, square };

    void setWaveform(Waveform type)
    {
        waveform.store(type);
    }

    void setAntialiasing(bool shouldBeAntialiased)
    {
        antialiased.store(shouldBeAntialiased);
    }

    void setDetuneAmount(FloatType amount)
    {
        detuneAmount.store(amount);
    }

    FloatType getDetuneAmount()
    {
        return detuneAmount.load();
    }
    
    void setPanoramicSpread(FloatType amount)
    {
        panSpreadAmount.store(amount);
    }

    FloatType getPanoramicSpread()
    {
        return panSpreadAmount.load();
    }

    void setFrequencyModulationDepth(FloatType depth)
    {
        frequencyModulationDepth.store(depth);
    }

    FloatType getFrequencyModulationDepth()
    {
        return frequencyModulationDepth.load();
    }

    FloatType* getFMInputPtr()
    {
        return &frequencyModulation;
    }

private:
    friend class ChordialOscillatorVoice<FloatType>;
    
    std::atomic<Waveform> waveform{ Waveform::triangle };
    std::atomic<bool> antialiased{ true };
    std::atomic<FloatType> detuneAmount{ static_cast<FloatType>(0.0) };
    std::atomic<FloatType> panSpreadAmount{ static_cast<FloatType>(0.5) };
    FloatType frequencyModulation{ static_cast<FloatType>(0.0) };
    std::atomic<FloatType> frequencyModulationDepth{ static_cast<FloatType>(0.0) };
};


template <typename FloatType>
    class ChordialOscillatorVoice : public ChordialModuleVoice<FloatType>
{
public:
    std::shared_ptr<ChordialOscillatorMaster<FloatType>> getMasterOscillator()
    {
        return masterOscillator;
    }
    
    void setMasterOscillator(std::shared_ptr<ChordialOscillatorMaster<FloatType>> master)
    {
        masterOscillator = master;
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        this->sampleRate = spec.sampleRate;
        updatePhaseIncrement();
        this->updateDownSampleRate();

        auto maxChannels = juce::jmin<int>((int)spec.numChannels, 2);
        
        tempBlock = juce::dsp::AudioBlock<float>(heapBlock, maxChannels, spec.maximumBlockSize);
    }

    template <typename ProcessContext>
    void process(const ProcessContext &context)
    {
        updateOscillatorFrequency();
        auto& output = context.getOutputBlock();
        tempBlock.clear();
        
        for(int i=0; i<output.getNumSamples(); ++i)
            tempBlock.setSample(0, i, processSample());
        
        for(int i=1; i<tempBlock.getNumChannels(); ++i)
            tempBlock.getSubsetChannelBlock(i, 1).add(tempBlock.getSubsetChannelBlock(0,1));
        
        const auto localPanSpread = masterOscillator->panSpreadAmount.load();
        const auto localPanMultiplier = panMultiplier.load();
        const auto panValue = localPanSpread * localPanMultiplier;
        
        if (tempBlock.getNumChannels() == 2)
        {
            if (panValue > 0)
                tempBlock.getSingleChannelBlock(0).multiply(1.0-panValue);
            else if (panValue < 0)
                tempBlock.getSingleChannelBlock(1).multiply(1.0+panValue);
        }
        
        output.add(tempBlock);
    }
    
    FloatType processSample()
    {
        updatePhaseIncrement();
        const auto localWaveform = masterOscillator->waveform.load();
        const auto localAA = masterOscillator->antialiased.load();
        const auto pi = juce::MathConstants<FloatType>::pi;
        FloatType t = phase / juce::MathConstants<FloatType>::twoPi;
        FloatType x = 0.0;
        
        switch (localWaveform)
        {
        /*case ChordialOscillatorMaster<FloatType>::Waveform::sine:
            //lastOutput = std::sin(phase);
            x = phase * (2 * pi) - pi;
            lastOutput = (4.0 / pi) * x + (-4.0 / (pi*pi)) * x * std::abs(x);
            DBG(lastOutput);
            break;*/
                
        case ChordialOscillatorMaster<FloatType>::Waveform::saw:
            lastOutput = 2.0 * (phase / juce::MathConstants<FloatType>::twoPi) - 1.0;
            if(localAA)
                lastOutput -= blep(t);
            break;
                
        case ChordialOscillatorMaster<FloatType>::Waveform::square:
            if(phase < juce::MathConstants<FloatType>::pi)
                lastOutput = 1.0;
            else
                lastOutput = -1.0;
                
            if(localAA)
            {
                lastOutput += blep(t);
                lastOutput -= blep(std::fmod(t + 0.5, 1.0));
            }
            break;

        case ChordialOscillatorMaster<FloatType>::Waveform::triangle:
            lastOutput = 2.0 * std::abs(2.0 * (phase / juce::MathConstants<FloatType>::twoPi) - 1.0) - 1.0;
            break;
                
        default:
            lastOutput = 0.0;
        }
        
        updatePhase();
        return lastOutput;
    }

    void reset()
    {
        phase = 0.0;
    }

    void setBaseFrequency(FloatType frequencyInHz)
    {
		setBaseFrequencyWithoutUpdating(frequencyInHz);
        updateOscillatorFrequency(true);
    }

	void setBaseFrequencyWithoutUpdating(FloatType frequencyInHz)
	{
		baseFrequency.store(frequencyInHz);
	}

    FloatType getBaseFrequency()
    {
        return baseFrequency.load();
    }
    
    void setDetuneMultiplier(FloatType multiplier)
    {
        detuneMultiplier.store(multiplier);
    }
    
    void setPanoramicSpreadMultiplier(FloatType multiplier)
    {
        panMultiplier.store(multiplier);
    }

    FloatType* getOutputPtr()
    {
        return &lastOutput;
    }



    // call this every control processing block
    void updateOscillatorFrequency(bool force = false)
    {
        const auto localDetune = masterOscillator->detuneAmount.load();
        const auto localDetuneMultiplier = detuneMultiplier.load();
        const auto localFMDepth = masterOscillator->frequencyModulationDepth.load();

        auto detunedFrequency = baseFrequency.load() * std::pow(2.0, localDetune* localDetuneMultiplier);

        detunedFrequency *= std::pow(2.0, localFMDepth * masterOscillator->frequencyModulation);

        smoothedFrequency.setValue(detunedFrequency, force);
    }
private:
    // Every sample
    void updatePhaseIncrement()
    {
        const auto nextFrequencyValue = smoothedFrequency.getNextValue();
        phaseIncrement = nextFrequencyValue * 2 * juce::MathConstants<FloatType>::pi / this->sampleRate;
    }

    void updatePhase()
    {
        phase += phaseIncrement;
        while (phase >= juce::MathConstants<FloatType>::twoPi) {
            phase -= juce::MathConstants<FloatType>::twoPi;
        }
    }
    
    FloatType blep(FloatType t)
    {
        FloatType dt = phaseIncrement / juce::MathConstants<FloatType>::twoPi;
        if (t < dt) {
            t /= dt;
            return t + t - t*t - 1.0;
        }
        else if (t > 1.0 - dt) {
            t = (t - 1.0) / dt;
            return t*t + t + t + 1.0;
        }
        // 0 otherwise
        return 0.0;
    }

    void updateSmoothing(FloatType time) override
    {
        smoothedFrequency.reset(this->sampleRate, time);
    }

    std::shared_ptr<ChordialOscillatorMaster<FloatType>> masterOscillator;
    FloatType lastOutput{ static_cast<FloatType>(0.0) };

    juce::LinearSmoothedValue<FloatType> smoothedFrequency{ static_cast<FloatType>(440.0) };
    
    std::atomic<FloatType> detuneMultiplier{ static_cast<FloatType>(1.0) };
    std::atomic<FloatType> panMultiplier{ static_cast<FloatType>(1.0) };

    // For oscillator implementation
    std::atomic<FloatType> baseFrequency{ static_cast<FloatType>(440.0) };
    FloatType phase{ static_cast<FloatType>(0.0) };
    FloatType phaseIncrement;
    juce::HeapBlock<char> heapBlock;
    juce::dsp::AudioBlock<FloatType> tempBlock;
};

}
}
