/*
  ==============================================================================

    Envelope.h
    Created: 1 Sep 2018 12:49:35pm
    Author:  matth

  ==============================================================================
*/

#pragma once
namespace chordial
{
namespace synth
{

template <typename SampleType, typename NumberType>
class ChordialVoiceADSR;

template <typename SampleType, typename NumberType>
class ChordialMasterADSR
{
public:
	enum class EnvelopeType { oneHit, sustained };

	ChordialMasterADSR()
	{
		setEnvelopeType(EnvelopeType::sustained);
		sampleRate = 44100;
		setAttackTimeMs(10.0);
		setDecayTimeMs(1.0);
		setReleaseTimeMs(2000.0);
		setSustainValue(1.0);
		setTargetRatioA(0.3);
		setTargetRatioDR(0.001);
	}

	void setSampleRate(double rate, int underSamplingRatio = 1)
	{
		sampleRate = rate / underSamplingRatio;
		updateAttackSamples();
		updateDecaySamples();
		updateReleaseSamples();
	}

	void setAttackTimeMs(NumberType ms)
	{
		attackMs = ms;
		updateAttackSamples();
	}

	NumberType getAttackTimeMs()
	{
		return attackMs;
	}

	void setDecayTimeMs(NumberType ms)
	{
		decayMs = ms;
		updateDecaySamples();
	}

	NumberType getDecayTimeMs()
	{
		return decayMs;
	}

	void setSustainValue(NumberType value)
	{
		sustainValue = value;
        updateDecaySamples();
	}

	NumberType getSustainValue()
	{
		return sustainValue;
	}

	void setReleaseTimeMs(NumberType ms)
	{
		releaseMs = ms;
		updateReleaseSamples();
	}

	NumberType getReleaseTimeMs()
	{
		return releaseMs();
	}

	void setTargetRatioA(float targetRatio)
	{
		if (targetRatio < 0.000000001)
			targetRatio = 0.000000001;  // -180 dB
		targetRatioA = targetRatio;
		attackCoef = calcCoef(attackRate, targetRatioA);
		attackBase = (1.0 + targetRatioA) * (1.0 - attackCoef);
	}

	void setTargetRatioDR(float targetRatio)
	{
		if (targetRatio < 0.000000001)
			targetRatio = 0.000000001;  // -180 dB
		targetRatioDR = targetRatio;
		decayCoef = calcCoef(decayRate, targetRatioDR);
		releaseCoef = calcCoef(releaseRate, targetRatioDR);
		decayBase = (sustainValue - targetRatioDR) * (1.0 - decayCoef);
		releaseBase = -targetRatioDR * (1.0 - releaseCoef);
	}

	void setEnvelopeType(EnvelopeType type)
	{
		envelopeType = type;
	}

	EnvelopeType getEnvelopeType()
	{
		return envelopeType;
	}

private:
	void updateAttackSamples()
	{
		attackRate = (attackMs / 1000) * sampleRate;
		attackCoef = calcCoef(attackRate, targetRatioA);
		attackBase = (1.0 + targetRatioA) * (1.0 - attackCoef);
	}

	void updateDecaySamples()
	{
		decayRate = (decayMs / 1000) * sampleRate;
		decayCoef = calcCoef(decayRate, targetRatioDR);
		decayBase = (sustainValue - targetRatioDR) * (1.0 - decayCoef);
	}

	void updateReleaseSamples()
	{
		releaseRate = (releaseMs / 1000) * sampleRate;
		releaseCoef = calcCoef(releaseRate, targetRatioDR);
		releaseBase = -targetRatioDR * (1.0 - releaseCoef);
	}

	float calcCoef(float rate, float targetRatio)
	{
		return (rate <= 0) ? 0.0 : exp(-log((1.0 + targetRatio) / targetRatio) / rate);
	}

	std::atomic<EnvelopeType> envelopeType; // Atomic


	double sampleRate;

	NumberType attackMs, decayMs, releaseMs;
	NumberType attackRate, decayRate, releaseRate;
	std::atomic<NumberType> sustainValue; // Atomic

	std::atomic<NumberType> attackCoef, decayCoef, releaseCoef; // Atomic
	NumberType targetRatioA, targetRatioDR;
	std::atomic<NumberType> attackBase, decayBase, releaseBase; // Atomic

	friend class ChordialVoiceADSR<SampleType, NumberType>;
};

template <typename SampleType, typename NumberType>
class ChordialVoiceADSR
{
public:

	ChordialVoiceADSR(ChordialMasterADSR<SampleType, NumberType>& masterADSR) : master(masterADSR)
	{
		reset();
	}

	SampleType getNextValue()
	{
		// Get atomic variables
		const auto envLocal = master.envelopeType.load();
		const auto susLocal = master.sustainValue.load();
		const auto attackCoefLocal = master.attackCoef.load();
		const auto decayCoefLocal = master.decayCoef.load();
		const auto releaseCoefLocal = master.releaseCoef.load();
		const auto attackBaseLocal = master.attackBase.load();
		const auto decayBaseLocal = master.decayBase.load();
		const auto releaseBaseLocal = master.releaseBase.load();

		switch (state) {
		case State::idle:
			break;
		case State::attack:
			output = attackBaseLocal + output * attackCoefLocal;
			if (output >= 1.0) {
				output = 1.0;
				state = State::decay;
			}
			break;

		case State::decay:
			output = decayBaseLocal + output * decayCoefLocal;
			if (output <= susLocal) {
				output = susLocal;
				state = State::sustain;
			}
			break;

		case State::sustain:
                output = susLocal;
			if (envLocal == ChordialMasterADSR<SampleType, NumberType>::EnvelopeType::oneHit)
				state = State::release;
			break;

		case State::release:
			output = releaseBaseLocal + output * releaseCoefLocal;
			if (output <= 0.0) {
				output = 0.0;
				state = State::idle;
			}
		}

		return output;
	}

	SampleType getOutput()
	{
		return output;
	}

	SampleType* getOutputPtr()
	{
		return &output;
	}

	void gate(bool on)
	{
		if (on)
			state = State::attack;
		else if (state != State::idle)
			state = State::release;
	}

	void reset()
	{
		state = State::idle;
		output = 0.0;
	}

	bool isActive()
	{
		return state != State::idle;
	}

private:
	enum class State
	{
		idle,
		attack,
		decay,
		sustain,
		release
	};

	ChordialMasterADSR<SampleType, NumberType>& master;
	State state;
	SampleType output;
};

}
}
