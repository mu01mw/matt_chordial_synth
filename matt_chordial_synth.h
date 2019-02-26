 /*
 BEGIN_JUCE_MODULE_DECLARATION

  ID:               matt_chordial_synth
  vendor:           matt
  version:          0.0.1
  name:             Chordial2 Synth classes
  description:      Chordial Synth classes

  dependencies:     juce_audio_basics, juce_audio_processors, juce_core, juce_dsp

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/


#pragma once
#define MATT_CHORDIAL_H_INCLUDED

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <unordered_map>
#include <atomic>

#include "synth/Utilities.h"
#include "synth/ChordialModule.h"
#include "synth/ChordialOscillator.h"
#include "synth/ChordialFilter.h"
#include "synth/ChordialDCA.h"
#include "synth/ChordialEnvelope.h"
#include "synth/ChordialModMatrix.h"
#include "synth/ChordialVoice.h"
#include "synth/ChordialSynthesiser.h"
