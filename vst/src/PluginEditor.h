#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SlotComponent : public juce::Component,
                      private juce::ComboBox::Listener
{
public:
    SlotComponent(DaisyMultiFXProcessor &processor, int slotIndex);
    ~SlotComponent() override;

    void paint(juce::Graphics &g) override;
    void resized() override;

    void comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged) override;

private:
    void updateParameterLabels(int effectTypeIndex);
    void loadModelButtonClicked();
    void loadIRButtonClicked();
    void updateModelLabel();

    DaisyMultiFXProcessor &processor_;
    int slotIndex_;

    juce::Label titleLabel_;
    juce::ToggleButton enableButton_;
    juce::ComboBox typeCombo_;
    juce::Slider mixSlider_;
    juce::Label mixLabel_;

    // Neural Amp / Cabinet IR specific controls
    juce::TextButton loadModelButton_;
    juce::Label modelNameLabel_;
    bool isNeuralAmpSlot_ = false;
    bool isCabinetIRSlot_ = false;

    // Parameters - can be sliders or combo boxes depending on type
    std::vector<std::unique_ptr<juce::Slider>> paramSliders_;
    std::vector<std::unique_ptr<juce::ComboBox>> paramCombos_;
    std::vector<std::unique_ptr<juce::Label>> paramLabels_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> paramAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> paramComboAttachments_;

    // File chooser for model loading
    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotComponent)
};

class DaisyMultiFXEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit DaisyMultiFXEditor(DaisyMultiFXProcessor &);
    ~DaisyMultiFXEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    void timerCallback() override;

private:
    DaisyMultiFXProcessor &processor_;

    // Header
    juce::Label titleLabel_;
    juce::Slider tempoSlider_;
    juce::Label tempoLabel_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment_;

    // Level meters
    float inputLevel_ = 0.0f;
    float outputLevel_ = 0.0f;
    juce::Label inputLevelLabel_;
    juce::Label outputLevelLabel_;

    // Effect slots
    std::vector<std::unique_ptr<SlotComponent>> slots_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DaisyMultiFXEditor)
};
