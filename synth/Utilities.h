/*
  ==============================================================================

    Utilities.h
    Created: 14 Sep 2018 3:44:16pm
    Author:  matth

  ==============================================================================
*/

#pragma once
namespace chordial
{
namespace synth
{
// MOD NAMES
// SOURCES
    const std::string GLOBAL_LFO1_OUT("lfo1_out");
    const std::string VOICE_ADSR1_OUT("adsr1_out");
    const std::string VOICE_ADSR2_OUT("adsr2_out");

// DESTINATIONS
    const std::string GLOBAL_OSC_MASTER_FM_IN("Master_Osc_FM_input");
    const std::string GLOBAL_FILTER_MASTER_CUTOFF_IN("Master_Filter_Cutoff_In");
    const std::string VOICE_FILTER_MASTER_CUTOFF_IN("Voice_Filter_Cutoff_In");
    const std::string VOICE_DCA_GAIN_IN("dca_in");
}
}