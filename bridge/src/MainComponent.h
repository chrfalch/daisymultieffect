#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MidiBridge.h"

class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshDevices();
    void connectBridge();
    void disconnectBridge();
    void updateStatus();
    void addLogMessage(const juce::String& msg);

    // UI Components
    juce::Label titleLabel_;
    
    // Device A (Network MIDI)
    juce::Label deviceALabel_;
    juce::ComboBox deviceACombo_;
    
    // Device B (Hardware MIDI) 
    juce::Label deviceBLabel_;
    juce::ComboBox deviceBCombo_;
    
    // Buttons
    juce::TextButton refreshButton_;
    juce::TextButton connectButton_;
    
    // Status
    juce::Label statusLabel_;
    juce::Label statsLabel_;
    
    // Activity log
    juce::TextEditor logView_;
    juce::Label logLabel_;
    
    // Connection indicator
    juce::Label connectionIndicator_;
    
    // Bridge
    std::unique_ptr<BidirectionalMidiBridge> bridge_;
    
    // Message log (circular buffer)
    juce::StringArray logMessages_;
    static constexpr int kMaxLogMessages = 100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
