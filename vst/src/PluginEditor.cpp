#include "PluginEditor.h"
#include "core/effects/effect_metadata.h"
#include <cmath>

//==============================================================================
// SlotComponent
//==============================================================================

SlotComponent::SlotComponent(DaisyMultiFXProcessor &processor, int slotIndex)
    : processor_(processor), slotIndex_(slotIndex)
{
    juce::String prefix = "slot" + juce::String(slotIndex);
    auto &vts = processor_.getValueTreeState();

    // Title
    titleLabel_.setText("Slot " + juce::String(slotIndex + 1), juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    titleLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel_);

    // Enable button
    enableButton_.setButtonText("On");
    addAndMakeVisible(enableButton_);
    enableAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        vts, prefix + "_enabled", enableButton_);

    // Type combo - add items from shared metadata (DON'T add listener yet)
    for (size_t i = 0; i < Effects::kNumEffects; ++i)
    {
        typeCombo_.addItem(Effects::kAllEffects[i].meta->name, static_cast<int>(i + 1));
    }
    addAndMakeVisible(typeCombo_);

    // Mix slider
    mixSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible(mixSlider_);
    mixLabel_.setText("Mix", juce::dontSendNotification);
    mixLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mixLabel_);
    mixAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        vts, prefix + "_mix", mixSlider_);

    // Parameter sliders - create all first before any attachments trigger
    for (int p = 0; p < 5; ++p)
    {
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        addAndMakeVisible(*slider);

        auto label = std::make_unique<juce::Label>();
        label->setText("---", juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*label);

        paramSliders_.push_back(std::move(slider));
        paramLabels_.push_back(std::move(label));
    }

    // Now create param attachments after labels exist
    for (int p = 0; p < 5; ++p)
    {
        auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, prefix + "_p" + juce::String(p), *paramSliders_[p]);
        paramAttachments_.push_back(std::move(attachment));
    }

    // NOW add listener and create type attachment - this may trigger comboBoxChanged
    typeCombo_.addListener(this);
    typeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        vts, prefix + "_type", typeCombo_);

    // Manually update labels based on current selection
    int currentType = typeCombo_.getSelectedId();
    if (currentType > 0)
        updateParameterLabels(currentType - 1);
}

SlotComponent::~SlotComponent()
{
    typeCombo_.removeListener(this);
}

void SlotComponent::comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &typeCombo_)
    {
        int typeIndex = typeCombo_.getSelectedId();
        if (typeIndex > 0)
            updateParameterLabels(typeIndex - 1);
    }
}

void SlotComponent::updateParameterLabels(int effectTypeIndex)
{
    // Get effect metadata from shared source
    const ::EffectMeta *meta = nullptr;
    if (effectTypeIndex >= 0 && static_cast<size_t>(effectTypeIndex) < Effects::kNumEffects)
    {
        meta = Effects::kAllEffects[effectTypeIndex].meta;
    }

    // Update labels and visibility based on effect's parameter count
    for (int i = 0; i < 5; ++i)
    {
        bool hasParam = meta && i < meta->numParams;
        const char *paramName = hasParam ? meta->params[i].name : "---";

        paramLabels_[i]->setText(paramName, juce::dontSendNotification);
        paramSliders_[i]->setVisible(hasParam);
        paramLabels_[i]->setVisible(hasParam);

        // Dim unused params
        float alpha = hasParam ? 1.0f : 0.3f;
        paramSliders_[i]->setAlpha(alpha);
        paramLabels_[i]->setAlpha(alpha);
    }
}

void SlotComponent::paint(juce::Graphics &g)
{
    g.setColour(juce::Colours::darkgrey);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

    g.setColour(juce::Colours::grey);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 8.0f, 2.0f);
}

void SlotComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Title row
    auto titleRow = bounds.removeFromTop(25);
    titleLabel_.setBounds(titleRow.removeFromLeft(80));
    enableButton_.setBounds(titleRow.removeFromRight(50));

    bounds.removeFromTop(5);

    // Type combo
    typeCombo_.setBounds(bounds.removeFromTop(25));

    bounds.removeFromTop(10);

    // Knobs layout - 2 rows of 3
    int knobSize = 70;
    int labelHeight = 20;

    // First row: Mix + Param1 + Param2
    auto row1 = bounds.removeFromTop(knobSize + labelHeight);
    int knobSpacing = (row1.getWidth() - knobSize * 3) / 4;

    auto mixArea = row1.removeFromLeft(knobSpacing + knobSize);
    mixArea.removeFromLeft(knobSpacing);
    mixLabel_.setBounds(mixArea.removeFromBottom(labelHeight));
    mixSlider_.setBounds(mixArea);

    for (int i = 0; i < 2 && i < (int)paramSliders_.size(); ++i)
    {
        auto area = row1.removeFromLeft(knobSpacing + knobSize);
        area.removeFromLeft(knobSpacing);
        paramLabels_[i]->setBounds(area.removeFromBottom(labelHeight));
        paramSliders_[i]->setBounds(area);
    }

    bounds.removeFromTop(5);

    // Second row: Param3 + Param4 + Param5
    auto row2 = bounds.removeFromTop(knobSize + labelHeight);
    row2.removeFromLeft(knobSpacing);

    for (int i = 2; i < 5 && i < (int)paramSliders_.size(); ++i)
    {
        auto area = row2.removeFromLeft(knobSize + knobSpacing);
        paramLabels_[i]->setBounds(area.removeFromBottom(labelHeight));
        paramSliders_[i]->setBounds(area.removeFromLeft(knobSize));
    }
}

//==============================================================================
// DaisyMultiFXEditor
//==============================================================================

DaisyMultiFXEditor::DaisyMultiFXEditor(DaisyMultiFXProcessor &p)
    : AudioProcessorEditor(&p), processor_(p)
{
    // Title
    titleLabel_.setText("DaisyMultiFX", juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(24.0f).withStyle("Bold"));
    titleLabel_.setJustificationType(juce::Justification::centred);
    titleLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel_);

    // Tempo
    tempoSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 25);
    addAndMakeVisible(tempoSlider_);

    tempoLabel_.setText("Tempo:", juce::dontSendNotification);
    tempoLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(tempoLabel_);

    tempoAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getValueTreeState(), "tempo", tempoSlider_);

    // Level meter labels
    inputLevelLabel_.setText("IN: ---", juce::dontSendNotification);
    inputLevelLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    inputLevelLabel_.setFont(juce::FontOptions(14.0f));
    addAndMakeVisible(inputLevelLabel_);

    outputLevelLabel_.setText("OUT: ---", juce::dontSendNotification);
    outputLevelLabel_.setColour(juce::Label::textColourId, juce::Colours::orange);
    outputLevelLabel_.setFont(juce::FontOptions(14.0f));
    addAndMakeVisible(outputLevelLabel_);

    // Create 4 slot components
    for (int i = 0; i < 4; ++i)
    {
        auto slot = std::make_unique<SlotComponent>(processor_, i);
        addAndMakeVisible(*slot);
        slots_.push_back(std::move(slot));
    }

    setSize(900, 500);
    setResizable(true, true);
    setResizeLimits(600, 400, 1200, 800);

    // Start timer for level meter updates
    startTimerHz(30);
}

DaisyMultiFXEditor::~DaisyMultiFXEditor()
{
    stopTimer();
}

void DaisyMultiFXEditor::paint(juce::Graphics &g)
{
    // Background gradient
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Header background
    g.setColour(juce::Colour(0xff16213e));
    g.fillRect(0, 0, getWidth(), 60);

    // Draw level meter bars
    auto header = getLocalBounds().removeFromTop(60);
    header.reduce(20, 10);

    // Input meter bar (after title)
    int meterX = 220;
    int meterY = 35;
    int meterWidth = 80;
    int meterHeight = 12;

    // Input meter background
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(meterX, meterY, meterWidth, meterHeight);

    // Input meter fill
    float inLevel = std::min(1.0f, inputLevel_);
    if (inLevel > 0.001f)
    {
        g.setColour(inLevel > 0.9f ? juce::Colours::red : juce::Colours::lightgreen);
        g.fillRect(meterX, meterY, (int)(meterWidth * inLevel), meterHeight);
    }

    // Output meter background
    int outMeterX = meterX + meterWidth + 80;
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(outMeterX, meterY, meterWidth, meterHeight);

    // Output meter fill
    float outLevel = std::min(1.0f, outputLevel_);
    if (outLevel > 0.001f)
    {
        g.setColour(outLevel > 0.9f ? juce::Colours::red : juce::Colours::orange);
        g.fillRect(outMeterX, meterY, (int)(meterWidth * outLevel), meterHeight);
    }
}

void DaisyMultiFXEditor::timerCallback()
{
    // Get levels from processor
    inputLevel_ = processor_.getInputLevel();
    outputLevel_ = processor_.getOutputLevel();

    // Convert to dB for display
    auto toDb = [](float level) -> juce::String
    {
        if (level < 0.0001f)
            return "---";
        float db = 20.0f * std::log10(level);
        return juce::String(db, 1) + " dB";
    };

    inputLevelLabel_.setText("IN: " + toDb(inputLevel_), juce::dontSendNotification);
    outputLevelLabel_.setText("OUT: " + toDb(outputLevel_), juce::dontSendNotification);

    // Also log to console if there's signal
    static int frameCount = 0;
    if (++frameCount % 30 == 0 && inputLevel_ > 0.001f) // Log every second if signal present
    {
        DBG("Audio levels - IN: " << inputLevel_ << " OUT: " << outputLevel_);
    }

    // Trigger repaint for meter bars
    repaint(0, 0, getWidth(), 60);
}

void DaisyMultiFXEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header
    auto header = bounds.removeFromTop(60);
    header.reduce(20, 10);

    titleLabel_.setBounds(header.removeFromLeft(150));

    // Level meter labels (positioned above the meter bars)
    inputLevelLabel_.setBounds(220, 8, 80, 20);
    outputLevelLabel_.setBounds(380, 8, 80, 20);

    auto tempoArea = header.removeFromRight(300);
    tempoLabel_.setBounds(tempoArea.removeFromLeft(60));
    tempoSlider_.setBounds(tempoArea);

    // Slots area
    bounds.reduce(10, 10);

    int slotWidth = (bounds.getWidth() - 30) / 4; // 4 slots with 10px gaps
    int slotHeight = bounds.getHeight();

    for (int i = 0; i < (int)slots_.size(); ++i)
    {
        slots_[i]->setBounds(bounds.removeFromLeft(slotWidth));
        bounds.removeFromLeft(10);
    }
}
