#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mutex>
#include "MidiBridge.h"

class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics &) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshDevices();
    void connectBridge();
    void disconnectBridge();
    void updateStatus();
    void addLogMessage(const juce::String &msg);

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
    juce::ComboBox logLevelCombo_;

    // Connection indicator
    juce::Label connectionIndicator_;

    // Bridge
    std::unique_ptr<BidirectionalMidiBridge> bridge_;

    // Log level filtering
    enum class LogLevel
    {
        ErrorsOnly = 0,
        Important = 1,
        Verbose = 2
    };
    LogLevel currentLogLevel_ = LogLevel::Important;

    // Message log (circular buffer) - reduced size since we filter now
    juce::StringArray logMessages_;
    static constexpr int kMaxLogMessages = 50;

    // Pending log messages (batched updates to avoid UI thrashing)
    juce::StringArray pendingLogMessages_;
    std::mutex logMutex_;

    void flushPendingLogs();
    void queueLogMessage(const juce::String &msg, LogLevel level);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
