#include "MainComponent.h"

MainComponent::MainComponent()
{
    // Title
    titleLabel_.setText("Daisy MIDI Bridge", juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(28.0f).withStyle("Bold"));
    titleLabel_.setJustificationType(juce::Justification::centred);
    titleLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel_);

    // Device A
    deviceALabel_.setText("Network MIDI (iPad):", juce::dontSendNotification);
    deviceALabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(deviceALabel_);
    addAndMakeVisible(deviceACombo_);

    // Device B
    deviceBLabel_.setText("Hardware MIDI (Daisy Seed):", juce::dontSendNotification);
    deviceBLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(deviceBLabel_);
    addAndMakeVisible(deviceBCombo_);

    // Buttons
    refreshButton_.setButtonText("Refresh Devices");
    refreshButton_.onClick = [this]()
    { refreshDevices(); };
    addAndMakeVisible(refreshButton_);

    connectButton_.setButtonText("Connect");
    connectButton_.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    connectButton_.onClick = [this]()
    {
        if (bridge_ && (bridge_->isConnected() || bridge_->isPartiallyConnected()))
            disconnectBridge();
        else
            connectBridge();
    };
    addAndMakeVisible(connectButton_);

    // Status
    statusLabel_.setText("Not connected", juce::dontSendNotification);
    statusLabel_.setFont(juce::FontOptions(16.0f));
    statusLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
    statusLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel_);

    statsLabel_.setText("Messages: A→B: 0  |  B→A: 0", juce::dontSendNotification);
    statsLabel_.setFont(juce::FontOptions(14.0f));
    statsLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statsLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statsLabel_);

    // Connection indicator (big colored circle)
    connectionIndicator_.setText("●", juce::dontSendNotification);
    connectionIndicator_.setFont(juce::FontOptions(48.0f));
    connectionIndicator_.setColour(juce::Label::textColourId, juce::Colours::darkgrey);
    connectionIndicator_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(connectionIndicator_);

    // Log
    logLabel_.setText("Activity Log:", juce::dontSendNotification);
    logLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(logLabel_);

    logView_.setMultiLine(true);
    logView_.setReadOnly(true);
    logView_.setScrollbarsShown(true);
    logView_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
    logView_.setColour(juce::TextEditor::textColourId, juce::Colours::lightgreen);
    logView_.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(logView_);

    // Initialize device lists
    refreshDevices();

    // Start timer for stats updates
    startTimerHz(4);

    setSize(500, 600);

    addLogMessage("Daisy MIDI Bridge started");
    addLogMessage("Select devices and click Connect");
}

MainComponent::~MainComponent()
{
    stopTimer();
}

void MainComponent::paint(juce::Graphics &g)
{
    // Dark gradient background
    g.fillAll(juce::Colour(0xff2d2d3a));

    // Header area
    g.setColour(juce::Colour(0xff1e1e28));
    g.fillRect(0, 0, getWidth(), 80);

    // Separator line
    g.setColour(juce::Colours::darkgrey);
    g.drawLine(20, 80, getWidth() - 20, 80, 1.0f);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Header
    auto header = bounds.removeFromTop(80);
    titleLabel_.setBounds(header.reduced(10));

    bounds.reduce(20, 10);

    // Connection indicator
    auto indicatorArea = bounds.removeFromTop(60);
    connectionIndicator_.setBounds(indicatorArea);

    // Status
    statusLabel_.setBounds(bounds.removeFromTop(30));
    statsLabel_.setBounds(bounds.removeFromTop(25));

    bounds.removeFromTop(15);

    // Device A selection
    deviceALabel_.setBounds(bounds.removeFromTop(25));
    deviceACombo_.setBounds(bounds.removeFromTop(30));

    bounds.removeFromTop(15);

    // Device B selection
    deviceBLabel_.setBounds(bounds.removeFromTop(25));
    deviceBCombo_.setBounds(bounds.removeFromTop(30));

    bounds.removeFromTop(20);

    // Buttons
    auto buttonRow = bounds.removeFromTop(40);
    refreshButton_.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2 - 5));
    buttonRow.removeFromLeft(10);
    connectButton_.setBounds(buttonRow);

    bounds.removeFromTop(20);

    // Log area
    logLabel_.setBounds(bounds.removeFromTop(25));
    logView_.setBounds(bounds.reduced(0, 5));
}

void MainComponent::timerCallback()
{
    updateStatus();
}

void MainComponent::refreshDevices()
{
    deviceACombo_.clear();
    deviceBCombo_.clear();

    auto inputs = MidiBridge::getAvailableInputs();
    auto outputs = MidiBridge::getAvailableOutputs();

    addLogMessage("Scanning MIDI devices...");

    // Find devices that have both input and output (bidirectional)
    int itemId = 1;
    for (const auto &name : inputs)
    {
        if (outputs.contains(name))
        {
            deviceACombo_.addItem(name, itemId);
            deviceBCombo_.addItem(name, itemId);
            addLogMessage("  Found: " + name);
            itemId++;
        }
    }

    if (itemId == 1)
    {
        addLogMessage("  No bidirectional MIDI devices found!");
    }

    // Auto-select Network/Session for device A
    for (int i = 0; i < deviceACombo_.getNumItems(); ++i)
    {
        auto name = deviceACombo_.getItemText(i);
        if (name.containsIgnoreCase("Session") || name.containsIgnoreCase("Network"))
        {
            deviceACombo_.setSelectedItemIndex(i);
            break;
        }
    }

    // Auto-select Daisy for device B
    for (int i = 0; i < deviceBCombo_.getNumItems(); ++i)
    {
        auto name = deviceBCombo_.getItemText(i);
        if (name.containsIgnoreCase("Daisy"))
        {
            deviceBCombo_.setSelectedItemIndex(i);
            break;
        }
    }
}

void MainComponent::connectBridge()
{
    auto deviceA = deviceACombo_.getText();
    auto deviceB = deviceBCombo_.getText();

    if (deviceA.isEmpty() || deviceB.isEmpty())
    {
        addLogMessage("ERROR: Select both devices first");
        return;
    }

    if (deviceA == deviceB)
    {
        addLogMessage("ERROR: Cannot bridge device to itself");
        return;
    }

    addLogMessage("Connecting: " + deviceA + " <=> " + deviceB);

    bridge_ = std::make_unique<BidirectionalMidiBridge>();

    // Set up monitoring callback
    bridge_->onMidiRouted = [this](const juce::MidiMessage &msg,
                                   const juce::String &from,
                                   const juce::String &to)
    {
        juce::String msgType;
        if (msg.isSysEx())
        {
            auto *data = msg.getSysExData();
            int size = msg.getSysExDataSize();
            // Check if it's our manufacturer ID (0x7D)
            if (size >= 3 && data[0] == 0x7D)
            {
                // Protocol: F0 7D <sender> <cmd> ... F7
                // SysEx data excludes F0/F7, so: 7D <sender> <cmd> ...
                uint8_t sender = data[1];
                uint8_t cmd = data[2];

                juce::String senderName;
                switch (sender)
                {
                case 0x01:
                    senderName = "FW";
                    break; // Firmware
                case 0x02:
                    senderName = "VST";
                    break; // VST
                case 0x03:
                    senderName = "App";
                    break; // iOS App
                default:
                    senderName = "0x" + juce::String::toHexString(sender);
                    break;
                }

                juce::String cmdName;
                switch (cmd)
                {
                case 0x10:
                    cmdName = "SET_PATCH";
                    break;
                case 0x12:
                    cmdName = "GET_PATCH";
                    break;
                case 0x13:
                    cmdName = "PATCH_DUMP";
                    break;
                case 0x20:
                    cmdName = "SET_PARAM";
                    break;
                case 0x21:
                    cmdName = "SET_ENABLED";
                    break;
                case 0x22:
                    cmdName = "SET_TYPE";
                    break;
                case 0x23:
                    cmdName = "SET_ROUTING";
                    break;
                case 0x30:
                    cmdName = "GET_EFFECT_META";
                    break;
                case 0x31:
                    cmdName = "EFFECT_META_LIST";
                    break;
                case 0x32:
                    cmdName = "GET_ALL_META";
                    break;
                case 0x33:
                    cmdName = "EFFECT_META";
                    break;
                case 0x34:
                    cmdName = "EFFECT_DISCOVERED";
                    break;
                case 0x35:
                    cmdName = "META_V2";
                    break;
                case 0x36:
                    cmdName = "META_V3";
                    break;
                case 0x37:
                    cmdName = "META_V4";
                    break;
                case 0x38:
                    cmdName = "META_V5";
                    break;
                case 0x40:
                    cmdName = "BUTTON_STATE";
                    break;
                case 0x41:
                    cmdName = "TEMPO_UPDATE";
                    break;
                default:
                    cmdName = "0x" + juce::String::toHexString(cmd);
                    break;
                }
                msgType = senderName + ":" + cmdName + " (" + juce::String(size) + "b)";
            }
            else if (size >= 2 && data[0] == 0x7D)
            {
                // Legacy format without sender
                msgType = "SysEx legacy 0x" + juce::String::toHexString(data[1]) + " (" + juce::String(size) + "b)";
            }
            else
            {
                msgType = "SysEx (" + juce::String(size) + " bytes)";
            }
        }
        else if (msg.isNoteOn())
            msgType = "Note On " + juce::MidiMessage::getMidiNoteName(msg.getNoteNumber(), true, true, 4);
        else if (msg.isNoteOff())
            msgType = "Note Off " + juce::MidiMessage::getMidiNoteName(msg.getNoteNumber(), true, true, 4);
        else if (msg.isController())
            msgType = "CC#" + juce::String(msg.getControllerNumber()) + "=" + juce::String(msg.getControllerValue());
        else if (msg.isProgramChange())
            msgType = "PC " + juce::String(msg.getProgramChangeNumber());
        else if (msg.isPitchWheel())
            msgType = "Pitch";
        else if (msg.isAftertouch())
            msgType = "AT";
        else if (msg.isChannelPressure())
            msgType = "ChPres";
        else
            msgType = "MIDI 0x" + juce::String::toHexString(msg.getRawData()[0]);

        // Use MessageManager to safely update UI from MIDI thread
        juce::MessageManager::callAsync([this, msgType, from, to]()
                                        {
            // Shorten device names for log
            auto shortFrom = from.contains("Session") ? "Network" :
                            (from.contains("Daisy") ? "Daisy" : from.substring(0, 12));
            auto shortTo = to.contains("Session") ? "Network" :
                          (to.contains("Daisy") ? "Daisy" : to.substring(0, 12));
            addLogMessage(msgType + ": " + shortFrom + " -> " + shortTo); });
    };

    if (bridge_->connectDevices(deviceA, deviceB))
    {
        if (bridge_->isConnected())
        {
            addLogMessage("SUCCESS: Full bidirectional connection");
            connectButton_.setButtonText("Disconnect");
            connectButton_.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
        }
        else
        {
            addLogMessage("WARNING: Partial connection only");
            connectButton_.setButtonText("Disconnect");
            connectButton_.setColour(juce::TextButton::buttonColourId, juce::Colours::darkorange);
        }
    }
    else
    {
        addLogMessage("FAILED: Could not connect devices");
        bridge_.reset();
    }

    updateStatus();
}

void MainComponent::disconnectBridge()
{
    if (bridge_)
    {
        addLogMessage("Disconnecting bridge...");
        bridge_->disconnect();
        bridge_.reset();
    }

    connectButton_.setButtonText("Connect");
    connectButton_.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);

    updateStatus();
}

void MainComponent::updateStatus()
{
    if (bridge_ && bridge_->isConnected())
    {
        statusLabel_.setText("Connected: " + bridge_->getDeviceAName() + " <=> " + bridge_->getDeviceBName(),
                             juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        connectionIndicator_.setColour(juce::Label::textColourId, juce::Colours::green);

        statsLabel_.setText("Messages: A->B: " + juce::String(bridge_->getAtoBCount()) +
                                "  |  B->A: " + juce::String(bridge_->getBtoACount()),
                            juce::dontSendNotification);
    }
    else if (bridge_ && bridge_->isPartiallyConnected())
    {
        statusLabel_.setText("Partial connection (check devices)", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colours::orange);
        connectionIndicator_.setColour(juce::Label::textColourId, juce::Colours::orange);

        statsLabel_.setText("Messages: A->B: " + juce::String(bridge_->getAtoBCount()) +
                                "  |  B->A: " + juce::String(bridge_->getBtoACount()),
                            juce::dontSendNotification);
    }
    else
    {
        statusLabel_.setText("Not connected", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
        connectionIndicator_.setColour(juce::Label::textColourId, juce::Colours::darkgrey);
        statsLabel_.setText("Messages: A->B: 0  |  B->A: 0", juce::dontSendNotification);
    }
}

void MainComponent::addLogMessage(const juce::String &msg)
{
    auto timestamp = juce::Time::getCurrentTime().toString(false, true, true, true);
    auto fullMsg = "[" + timestamp + "] " + msg;

    logMessages_.add(fullMsg);

    // Keep log size bounded
    while (logMessages_.size() > kMaxLogMessages)
    {
        logMessages_.remove(0);
    }

    // Update text view
    logView_.setText(logMessages_.joinIntoString("\n"));
    logView_.moveCaretToEnd();
}
