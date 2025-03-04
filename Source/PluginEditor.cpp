/*
   ==============================================================================

   This file contains the basic framework code for a JUCE plugin editor.

   ==============================================================================
   */

#include <vector>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "juce_core/juce_core.h"
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_basics/juce_gui_basics.h"

ResponseCurveComponent::ResponseCurveComponent(SimpleEQAudioProcessor& p)
    : audioProcessor(p)
{
	const auto& params = audioProcessor.getParameters();

	for (auto param : params) {
		param->addListener(this);
	}

	startTimerHz(60);
}

ResponseCurveComponent::~ResponseCurveComponent()
{
	const auto& params = audioProcessor.getParameters();

	for (auto param : params) {
		param->removeListener(this);
	}
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
	// (Our component is opaque, so we must completely fill the background with a
	// solid colour)
	g.fillAll(juce::Colours::black);

	auto responseCurveArea = getLocalBounds();
	auto responseCurveWidth = responseCurveArea.getWidth();

	auto& lowCut = monoChain.get<ChainPositions::LowCut>();
	auto& peak = monoChain.get<ChainPositions::Peak>();
	auto& highCut = monoChain.get<ChainPositions::HighCut>();

	auto sampleRate = audioProcessor.getSampleRate();

	std::vector<double> mags;

	mags.resize(responseCurveWidth);

	for (int i = 0; i < responseCurveWidth; ++i) {
		double mag = 1.f;

		auto freq =
		    juce::mapToLog10((double) i / (double) responseCurveWidth, 20.0, 20000.0);

		if (!monoChain.isBypassed<ChainPositions::Peak>())
			mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);

		/* LowCut chainPositions */
		if (!lowCut.isBypassed<0>())
			mag *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq,
										      sampleRate);

		if (!lowCut.isBypassed<1>())
			mag *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq,
										      sampleRate);

		if (!lowCut.isBypassed<2>())
			mag *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq,
										      sampleRate);

		if (!lowCut.isBypassed<3>())
			mag *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq,
										      sampleRate);

		/* HighCut chainPositions */
		if (!highCut.isBypassed<0>())
			mag *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq,
										       sampleRate);

		if (!highCut.isBypassed<1>())
			mag *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq,
										       sampleRate);

		if (!highCut.isBypassed<2>())
			mag *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq,
										       sampleRate);

		if (!highCut.isBypassed<3>())
			mag *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq,
										       sampleRate);

		mags[i] = juce::Decibels::gainToDecibels(mag);
	}

	juce::Path responseCurve;

	const double outputMin = responseCurveArea.getBottom();
	const double outputMax = responseCurveArea.getY();

	auto map = [outputMin, outputMax](double input) {
		return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
	};

	responseCurve.startNewSubPath(responseCurveArea.getX(), map(mags.front()));

	for (size_t i = 1; i < mags.size(); i++) {
		responseCurve.lineTo(responseCurveArea.getX() + i, map(mags[i]));
	}

	g.setColour(juce::Colours::grey);
	g.drawRoundedRectangle(responseCurveArea.toFloat(), 4.f, 4.f);

	g.setColour(juce::Colours::white);
	g.strokePath(responseCurve, juce::PathStrokeType(2.f));
}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue)
{
	parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback()
{
	if (parametersChanged.compareAndSetBool(false, true)) {
		// update the monoChain
		auto chainSettings = getChainSettings(audioProcessor.apvts);

		auto peakCoefficients =
		    makePeakFilter(chainSettings, audioProcessor.getSampleRate());
		updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients,
				   peakCoefficients);

		auto lowCutCoefficients =
		    makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
		auto highCutCoefficients =
		    makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());

		updateCutFilter(monoChain.get<ChainPositions::LowCut>(),
				lowCutCoefficients,
				static_cast<Slope>(chainSettings.lowCutSlope));
		updateCutFilter(monoChain.get<ChainPositions::HighCut>(),
				highCutCoefficients,
				static_cast<Slope>(chainSettings.highCutSlope));

		// signal a repaint
		repaint();
	}
}

SimpleEQAudioProcessorEditor::SimpleEQAudioProcessorEditor(SimpleEQAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , audioProcessor(p)
    , peakFreqSlider(*audioProcessor.apvts.getParameter("Peak Freq"), "Hz")
    , peakGainSlider(*audioProcessor.apvts.getParameter("Peak Gain"), "dB")
    , peakQualitySlider(*audioProcessor.apvts.getParameter("Peak Quality"), "")
    , lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz")
    , lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "Hz")
    , highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "db/Oct")
    , highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "db/Oct")
    ,

    responseCurveComponent(audioProcessor)
    , peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider)
    , peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider)
    , peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider)
    , lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider)
    , lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider)
    , highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider)
    , highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{
	// Make sure that before the constructor has finished, you've set the
	// editor's size to whatever you need it to be.

	for (auto* comp : getComps()) {
		addAndMakeVisible(comp);
	}

	setSize(600, 400);
}

SimpleEQAudioProcessorEditor::~SimpleEQAudioProcessorEditor() {}

void SimpleEQAudioProcessorEditor::paint(juce::Graphics& g)
{
	// (Our component is opaque, so we must completely fill the background with a
	// solid colour)
	g.fillAll(juce::Colours::black);
}

void SimpleEQAudioProcessorEditor::resized()
{
	// This is generally where you'll want to lay out the positions of any
	// subcomponents in your editor..

	auto bounds = getLocalBounds();
	auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);

	responseCurveComponent.setBounds(responseArea);

	auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
	auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

	lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
	lowCutSlopeSlider.setBounds(lowCutArea);

	highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
	highCutSlopeSlider.setBounds(highCutArea);
	peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
	peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
	peakQualitySlider.setBounds(bounds);
}

std::vector<juce::Component*> SimpleEQAudioProcessorEditor::getComps()
{
	return {&peakFreqSlider,
		&peakGainSlider,
		&peakQualitySlider,
		&lowCutFreqSlider,
		&highCutFreqSlider,
		&lowCutSlopeSlider,
		&highCutSlopeSlider,
		&responseCurveComponent};
}
