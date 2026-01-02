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

    // Neural Amp model loading button
    loadModelButton_.setButtonText("Load Model...");
    loadModelButton_.onClick = [this]
    { loadModelButtonClicked(); };
    addAndMakeVisible(loadModelButton_);
    loadModelButton_.setVisible(false); // Hidden by default

    // Neural Amp model name label
    modelNameLabel_.setText("No Model Loaded", juce::dontSendNotification);
    modelNameLabel_.setFont(juce::FontOptions(12.0f));
    modelNameLabel_.setJustificationType(juce::Justification::centred);
    modelNameLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(modelNameLabel_);
    modelNameLabel_.setVisible(false);

    // Mix slider
    mixSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible(mixSlider_);
    mixLabel_.setText("Mix", juce::dontSendNotification);
    mixLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mixLabel_);
    mixAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        vts, prefix + "_mix", mixSlider_);

    // Parameter controls - create sliders and combo boxes for all params
    // We'll show/hide based on parameter type when effect changes
    for (int p = 0; p < 7; ++p)
    {
        // Create slider (used for Number params)
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        addAndMakeVisible(*slider);

        // Create combo box (used for Enum params)
        auto combo = std::make_unique<juce::ComboBox>();
        addAndMakeVisible(*combo);
        combo->setVisible(false); // Hidden by default

        auto label = std::make_unique<juce::Label>();
        label->setText("---", juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*label);

        paramSliders_.push_back(std::move(slider));
        paramCombos_.push_back(std::move(combo));
        paramLabels_.push_back(std::move(label));
    }

    // Create param attachments (sliders only initially - combos attached dynamically)
    for (int p = 0; p < 7; ++p)
    {
        auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, prefix + "_p" + juce::String(p), *paramSliders_[p]);
        paramAttachments_.push_back(std::move(attachment));
        paramComboAttachments_.push_back(nullptr); // Placeholder
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
    uint8_t effectTypeId = 0;
    if (effectTypeIndex >= 0 && static_cast<size_t>(effectTypeIndex) < Effects::kNumEffects)
    {
        meta = Effects::kAllEffects[effectTypeIndex].meta;
        effectTypeId = Effects::kAllEffects[effectTypeIndex].typeId;
    }

    // Check if this is a Neural Amp or Cabinet IR effect (both need file loading)
    isNeuralAmpSlot_ = (effectTypeId == Effects::NeuralAmp::TypeId);
    isCabinetIRSlot_ = (effectTypeId == Effects::CabinetIR::TypeId);

    bool needsFileLoad = isNeuralAmpSlot_ || isCabinetIRSlot_;
    loadModelButton_.setVisible(needsFileLoad);
    modelNameLabel_.setVisible(needsFileLoad);

    if (isNeuralAmpSlot_)
    {
        loadModelButton_.setButtonText("Load Model...");
        updateModelLabel();
    }
    else if (isCabinetIRSlot_)
    {
        loadModelButton_.setButtonText("Load IR...");
        updateModelLabel();
    }

    juce::String prefix = "slot" + juce::String(slotIndex_);
    auto &vts = processor_.getValueTreeState();

    // Update labels and visibility based on effect's parameter count and type
    for (int i = 0; i < 7; ++i)
    {
        bool hasParam = meta && i < meta->numParams;
        const char *paramName = hasParam ? meta->params[i].name : "---";
        bool isEnum = hasParam && meta->params[i].kind == ParamValueKind::Enum;

        paramLabels_[i]->setText(paramName, juce::dontSendNotification);
        paramLabels_[i]->setVisible(hasParam);

        if (isEnum && meta->params[i].enumeration)
        {
            // Use combo box for enum params
            paramSliders_[i]->setVisible(false);
            paramCombos_[i]->setVisible(true);

            // Clear and populate combo box
            paramCombos_[i]->clear();
            const auto *enumInfo = meta->params[i].enumeration;
            for (int opt = 0; opt < enumInfo->numOptions; ++opt)
            {
                paramCombos_[i]->addItem(enumInfo->options[opt].name, opt + 1);
            }

            // Remove old attachment and create new combo attachment
            paramAttachments_[i].reset();
            paramComboAttachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                vts, prefix + "_p" + juce::String(i), *paramCombos_[i]);
        }
        else
        {
            // Use slider for number params
            paramSliders_[i]->setVisible(hasParam);
            paramCombos_[i]->setVisible(false);

            // Remove combo attachment and restore slider attachment if needed
            if (paramComboAttachments_[i])
            {
                paramComboAttachments_[i].reset();
                paramAttachments_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    vts, prefix + "_p" + juce::String(i), *paramSliders_[i]);
            }
        }

        // Dim unused params
        float alpha = hasParam ? 1.0f : 0.3f;
        paramSliders_[i]->setAlpha(alpha);
        paramCombos_[i]->setAlpha(alpha);
        paramLabels_[i]->setAlpha(alpha);
    }

    // Trigger layout update for combo boxes
    resized();
}

void SlotComponent::loadModelButtonClicked()
{
    if (isNeuralAmpSlot_)
    {
        fileChooser_ = std::make_unique<juce::FileChooser>(
            "Select Neural Amp Model",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.json");

        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        fileChooser_->launchAsync(flags, [this](const juce::FileChooser &fc)
                                  {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                bool success = processor_.loadNeuralAmpModel(slotIndex_, file);
                if (success)
                {
                    updateModelLabel();
                }
                else
                {
                    modelNameLabel_.setText("Load Failed!", juce::dontSendNotification);
                    modelNameLabel_.setColour(juce::Label::textColourId, juce::Colours::red);
                }
            } });
    }
    else if (isCabinetIRSlot_)
    {
        fileChooser_ = std::make_unique<juce::FileChooser>(
            "Select Cabinet Impulse Response",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav;*.aif;*.aiff;*.flac");

        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        fileChooser_->launchAsync(flags, [this](const juce::FileChooser &fc)
                                  {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                bool success = processor_.loadCabinetIR(slotIndex_, file);
                if (success)
                {
                    updateModelLabel();
                }
                else
                {
                    modelNameLabel_.setText("Load Failed!", juce::dontSendNotification);
                    modelNameLabel_.setColour(juce::Label::textColourId, juce::Colours::red);
                }
            } });
    }
}

void SlotComponent::updateModelLabel()
{
    juce::String displayName;

    if (isNeuralAmpSlot_)
    {
        displayName = processor_.getNeuralAmpModelName(slotIndex_);
    }
    else if (isCabinetIRSlot_)
    {
        displayName = processor_.getCabinetIRName(slotIndex_);
    }
    else
    {
        return;
    }

    modelNameLabel_.setText(displayName, juce::dontSendNotification);

    // Green if loaded, grey if not
    if (displayName == "No Model" || displayName == "No IR" ||
        displayName.startsWith("Load Failed") ||
        displayName == "Not Neural Amp" || displayName == "Not Cabinet IR" ||
        displayName == "RTNeural Disabled" || displayName == "Invalid Slot" ||
        displayName == "No Effect")
    {
        modelNameLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    }
    else
    {
        modelNameLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
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

    bounds.removeFromTop(5);

    // File load controls (shown for Neural Amp and Cabinet IR effects)
    if (isNeuralAmpSlot_ || isCabinetIRSlot_)
    {
        loadModelButton_.setBounds(bounds.removeFromTop(25));
        bounds.removeFromTop(3);
        modelNameLabel_.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(5);
    }

    // Knobs layout
    int knobSize = 70;
    int labelHeight = 20;
    int knobSpacing = (bounds.getWidth() - knobSize * 3) / 4;

    // Collect visible knob params and enum params separately
    std::vector<int> knobParamIndices;
    std::vector<int> enumParamIndices;

    for (int i = 0; i < (int)paramSliders_.size(); ++i)
    {
        if (paramCombos_[i]->isVisible())
        {
            enumParamIndices.push_back(i);
        }
        else if (paramSliders_[i]->isVisible())
        {
            knobParamIndices.push_back(i);
        }
    }

    // Layout knobs in grid: Mix first, then params flow consecutively
    // Row 1: Mix + up to 2 params
    // Row 2: up to 3 params
    // Row 3: up to 2 params (if needed)

    auto row1 = bounds.removeFromTop(knobSize + labelHeight);

    // Mix knob
    auto mixArea = row1.removeFromLeft(knobSpacing + knobSize);
    mixArea.removeFromLeft(knobSpacing);
    mixLabel_.setBounds(mixArea.removeFromBottom(labelHeight));
    mixSlider_.setBounds(mixArea);

    // First 2 knob params in row 1
    for (int k = 0; k < 2 && k < (int)knobParamIndices.size(); ++k)
    {
        int idx = knobParamIndices[k];
        auto area = row1.removeFromLeft(knobSpacing + knobSize);
        area.removeFromLeft(knobSpacing);
        paramLabels_[idx]->setBounds(area.removeFromBottom(labelHeight));
        paramSliders_[idx]->setBounds(area);
    }

    bounds.removeFromTop(5);

    // Row 2: next 3 knob params
    auto row2 = bounds.removeFromTop(knobSize + labelHeight);
    row2.removeFromLeft(knobSpacing);

    for (int k = 2; k < 5 && k < (int)knobParamIndices.size(); ++k)
    {
        int idx = knobParamIndices[k];
        auto area = row2.removeFromLeft(knobSize + knobSpacing);
        paramLabels_[idx]->setBounds(area.removeFromBottom(labelHeight));
        paramSliders_[idx]->setBounds(area.removeFromLeft(knobSize));
    }

    // Row 3: remaining knob params (if any)
    if (knobParamIndices.size() > 5)
    {
        bounds.removeFromTop(5);
        auto row3 = bounds.removeFromTop(knobSize + labelHeight);
        row3.removeFromLeft(knobSpacing);

        for (int k = 5; k < (int)knobParamIndices.size(); ++k)
        {
            int idx = knobParamIndices[k];
            auto area = row3.removeFromLeft(knobSize + knobSpacing);
            paramLabels_[idx]->setBounds(area.removeFromBottom(labelHeight));
            paramSliders_[idx]->setBounds(area.removeFromLeft(knobSize));
        }
    }

    // Hide bounds for unused params
    for (int i = 0; i < (int)paramSliders_.size(); ++i)
    {
        if (!paramSliders_[i]->isVisible() && !paramCombos_[i]->isVisible())
        {
            paramSliders_[i]->setBounds(0, 0, 0, 0);
            paramLabels_[i]->setBounds(0, 0, 0, 0);
        }
    }

    // Bottom section: Enum parameter combos (full width rows)
    for (int idx : enumParamIndices)
    {
        bounds.removeFromTop(5);
        auto comboRow = bounds.removeFromTop(25 + labelHeight);
        paramLabels_[idx]->setBounds(comboRow.removeFromBottom(labelHeight));
        paramCombos_[idx]->setBounds(comboRow);
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

    // Create slot components based on kNumSlots constant
    for (int i = 0; i < kNumSlots; ++i)
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

    int numGaps = kNumSlots - 1;
    int slotWidth = (bounds.getWidth() - numGaps * 10) / kNumSlots;
    int slotHeight = bounds.getHeight();

    for (int i = 0; i < (int)slots_.size(); ++i)
    {
        slots_[i]->setBounds(bounds.removeFromLeft(slotWidth));
        bounds.removeFromLeft(10);
    }
}
