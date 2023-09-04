#pragma once
#include <JuceHeader.h>
#include <array>
#include "DynamicsProcessor.h"

class BasicDynamicsProcessorAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
    using String = juce::String;

    enum PID 
    {
        Threshold,
        Ratio,
        Knee,
        Attack,
        Release,
        Makeup,
        NumParams
    };

    String toString(PID p)
    {
        switch (p)
        {
		case Threshold: return "Threshold";
		case Ratio: return "Ratio";
		case Knee: return "Knee";
		case Attack: return "Attack";
		case Release: return "Release";
		case Makeup: return "Makeup";
		default: return "Invalid Name";
		}
	}

    String toID(const String& name)
    {
        return name.removeCharacters(" ").toLowerCase();
	}

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
public:
    BasicDynamicsProcessorAudioProcessor();
    ~BasicDynamicsProcessorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::array<juce::RangedAudioParameter*, NumParams> params;

    dsp::DynamicsProcessor dynamicsProcessor;
};
