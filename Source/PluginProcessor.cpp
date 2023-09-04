#include "PluginProcessor.h"
#include "Range.h"
#include <functional>

// create parameter layout
juce::AudioProcessorValueTreeState::ParameterLayout BasicDynamicsProcessorAudioProcessor::createParameterLayout()
{
    using Range = makeRange::Range;

    const auto make = [this](const String& name, const Range& range, float defaultVal, const std::function<String(float, int)>& valToStr)
    {
        return std::make_unique<juce::AudioParameterFloat>
            (
                toID(name), name,
                range, defaultVal,
                "",
                juce::AudioProcessorParameter::genericParameter,
                valToStr
            );
    };

    auto valToStrDb = [](float value, int) { return juce::Decibels::toString(value); };
    auto valToStrMs = [](float value, int) { return juce::String(value, 1) + " ms"; };

	juce::AudioProcessorValueTreeState::ParameterLayout layout;
	layout.add(make(toString(PID::Threshold), makeRange::withCentre(-60.f, 0.f, -20.f), -20.f, valToStrDb));
    layout.add(make(toString(PID::Ratio), makeRange::withCentre(1.f, 40.f, 4.f), 4.f, valToStrDb));
    layout.add(make(toString(PID::Knee), makeRange::withCentre(0.f, 20.f, 2.f), 2.f, valToStrDb));
    layout.add(make(toString(PID::Attack), makeRange::withCentre(1.f, 1000.f, 20.f), 20.f, valToStrMs));
    layout.add(make(toString(PID::Release), makeRange::withCentre(1.f, 1000.f, 120.f), 120.f, valToStrMs));
    layout.add(make(toString(PID::Makeup), makeRange::lin(-30.f, 30.f), 0.f, valToStrDb));
	return layout;
}

BasicDynamicsProcessorAudioProcessor::BasicDynamicsProcessorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
    apvts(*this, nullptr, "Parameters", createParameterLayout()),
    params(),
    dynamicsProcessor()
#endif
{
    for (auto p = 0; p < NumParams; ++p)
		params[p] = apvts.getParameter(toID(toString(static_cast<PID>(p))));
}

BasicDynamicsProcessorAudioProcessor::~BasicDynamicsProcessorAudioProcessor()
{
}

//==============================================================================
const juce::String BasicDynamicsProcessorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BasicDynamicsProcessorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool BasicDynamicsProcessorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool BasicDynamicsProcessorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double BasicDynamicsProcessorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int BasicDynamicsProcessorAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int BasicDynamicsProcessorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void BasicDynamicsProcessorAudioProcessor::setCurrentProgram (int)
{
}

const juce::String BasicDynamicsProcessorAudioProcessor::getProgramName (int)
{
    return {};
}

void BasicDynamicsProcessorAudioProcessor::changeProgramName (int, const juce::String&)
{
}

//==============================================================================
void BasicDynamicsProcessorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dynamicsProcessor.prepare((float)sampleRate, samplesPerBlock);
}

void BasicDynamicsProcessorAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BasicDynamicsProcessorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void BasicDynamicsProcessorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    {
        const auto totalNumInputChannels = getTotalNumInputChannels();
        const auto totalNumOutputChannels = getTotalNumOutputChannels();

        for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear(i, 0, numSamples);
        if (numSamples == 0)
            return;
    }
    const auto numChannels = buffer.getNumChannels();
    auto samples = buffer.getArrayOfWritePointers();

    const auto& thresholdParam = *params[static_cast<int>(PID::Threshold)];
    const auto& ratioParam = *params[static_cast<int>(PID::Ratio)];
    const auto& kneeParam = *params[static_cast<int>(PID::Knee)];
    const auto& attackParam = *params[static_cast<int>(PID::Attack)];
    const auto& releaseParam = *params[static_cast<int>(PID::Release)];
    const auto& makeupParam = *params[static_cast<int>(PID::Makeup)];

    const auto thresholdDb = thresholdParam.getNormalisableRange().convertFrom0to1(thresholdParam.getValue());
    const auto ratioDb = ratioParam.getNormalisableRange().convertFrom0to1(ratioParam.getValue());
    const auto kneeDb = kneeParam.getNormalisableRange().convertFrom0to1(kneeParam.getValue());
    const auto attackMs = attackParam.getNormalisableRange().convertFrom0to1(attackParam.getValue());
    const auto releaseMs = releaseParam.getNormalisableRange().convertFrom0to1(releaseParam.getValue());
    const auto makeupDb = makeupParam.getNormalisableRange().convertFrom0to1(makeupParam.getValue());

    dynamicsProcessor
    (
        samples,
        thresholdDb, ratioDb, kneeDb,
        attackMs, releaseMs,
        makeupDb,
        numChannels, numSamples
    );
}

//==============================================================================
bool BasicDynamicsProcessorAudioProcessor::hasEditor() const
{
    return false;
}

juce::AudioProcessorEditor* BasicDynamicsProcessorAudioProcessor::createEditor()
{
    return nullptr;
}

//==============================================================================
void BasicDynamicsProcessorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.state;
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BasicDynamicsProcessorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
	if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType()))
		apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BasicDynamicsProcessorAudioProcessor();
}
