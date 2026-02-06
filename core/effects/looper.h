
#pragma once
#include "effects/base_effect.h"
#include "effects/effect_metadata.h"
#include "effects/fast_math.h"
#include <cstring>

// Looper pedal effect with recording, playback, and overdubbing
// Features:
// - Record up to 30 seconds stereo
// - Seamless loop playback with crossfade
// - Overdubbing with adjustable feedback
// - Clear and undo functionality
struct LooperEffect : BaseEffect
{
    static constexpr uint8_t TypeId = Effects::Looper::TypeId;
    
    // Buffer size: 30 seconds at 48kHz stereo = 1,440,000 samples per channel
    static constexpr int MAX_SAMPLES = 48000 * 30;
    
    enum class State : uint8_t
    {
        Idle = 0,
        Recording = 1,
        Playing = 2,
        Overdubbing = 3
    };
    
    float *bufL_ = nullptr;
    float *bufR_ = nullptr;
    
    State state_ = State::Idle;
    int writePos_ = 0;      // Write position (recording/overdubbing)
    int readPos_ = 0;       // Read position (playback)
    int loopLength_ = 0;    // Length of recorded loop in samples
    
    // Parameters (0..1 normalized)
    float level_ = 1.0f;        // id0: playback level
    float feedback_ = 0.8f;     // id1: overdub feedback
    float action_ = 0.0f;       // id2: action trigger (0=idle, changes trigger actions)
    
    // Previous action value for edge detection
    float prevAction_ = 0.0f;
    
    float sampleRate_ = 48000.0f;
    
    // Crossfade parameters for smooth loop boundaries
    static constexpr int FADE_SAMPLES = 480; // 10ms at 48kHz
    
    uint8_t GetTypeId() const override { return TypeId; }
    ChannelMode GetSupportedModes() const override { return ChannelMode::Stereo; }
    const EffectMeta &GetMetadata() const override { return Effects::Looper::kMeta; }
    
    // Bind external SDRAM buffers before Init
    void BindBuffers(float *bufL, float *bufR)
    {
        bufL_ = bufL;
        bufR_ = bufR;
    }
    
    void Init(float sr) override
    {
        sampleRate_ = sr;
        state_ = State::Idle;
        writePos_ = 0;
        readPos_ = 0;
        loopLength_ = 0;
        
        // Clear buffers
        if (bufL_ && bufR_)
        {
            for (int i = 0; i < MAX_SAMPLES; i++)
            {
                bufL_[i] = 0.0f;
                bufR_[i] = 0.0f;
            }
        }
    }
    
    void SetParam(uint8_t id, float v) override
    {
        switch (id)
        {
        case 0: // Level
            level_ = v;
            break;
        case 1: // Feedback
            feedback_ = v;
            break;
        case 2: // Action trigger
            action_ = v;
            ProcessAction();
            break;
        }
    }
    
    uint8_t GetParamsSnapshot(ParamDesc *out, uint8_t max) const override
    {
        if (max < 3)
            return 0;
        out[0] = {0, (uint8_t)(level_ * 127.0f + 0.5f)};
        out[1] = {1, (uint8_t)(feedback_ * 127.0f + 0.5f)};
        out[2] = {2, (uint8_t)(action_ * 127.0f + 0.5f)};
        return 3;
    }
    
    // Output parameters for UI feedback
    uint8_t GetOutputParams(OutputParamDesc *out, uint8_t max) const override
    {
        if (max < 3)
            return 0;
        out[0] = {3, (float)state_};                           // Current state
        out[1] = {4, loopLength_ / sampleRate_};               // Loop length in seconds
        out[2] = {5, loopLength_ > 0 ? (float)readPos_ / (float)loopLength_ : 0.0f}; // Position 0-1
        return 3;
    }
    
    void ProcessAction()
    {
        // Detect rising edge (parameter changed)
        if (FastMath::fabs(action_ - prevAction_) < 0.01f)
            return;
        
        prevAction_ = action_;
        
        // Action values map to different operations:
        // 0.0-0.2: Stop/Clear
        // 0.2-0.4: Record
        // 0.4-0.6: Play
        // 0.6-0.8: Overdub
        // 0.8-1.0: Clear
        
        if (action_ < 0.2f)
        {
            // Stop - return to idle
            state_ = State::Idle;
            readPos_ = 0;
        }
        else if (action_ < 0.4f)
        {
            // Record/Stop record
            if (state_ == State::Idle || state_ == State::Playing || state_ == State::Overdubbing)
            {
                // Start recording
                state_ = State::Recording;
                writePos_ = 0;
                loopLength_ = 0;
            }
            else if (state_ == State::Recording)
            {
                // Stop recording, start playing
                if (writePos_ > 0)
                {
                    loopLength_ = writePos_;
                    readPos_ = 0;
                    state_ = State::Playing;
                }
                else
                {
                    state_ = State::Idle;
                }
            }
        }
        else if (action_ < 0.6f)
        {
            // Play (if we have a loop)
            if (loopLength_ > 0)
            {
                state_ = State::Playing;
                readPos_ = 0;
            }
        }
        else if (action_ < 0.8f)
        {
            // Overdub (if we have a loop)
            if (loopLength_ > 0 && state_ != State::Recording)
            {
                state_ = State::Overdubbing;
                writePos_ = readPos_;
            }
        }
        else
        {
            // Clear
            state_ = State::Idle;
            loopLength_ = 0;
            writePos_ = 0;
            readPos_ = 0;
            
            // Clear buffers
            if (bufL_ && bufR_)
            {
                for (int i = 0; i < MAX_SAMPLES; i++)
                {
                    bufL_[i] = 0.0f;
                    bufR_[i] = 0.0f;
                }
            }
        }
    }
    
    void ProcessStereo(float &l, float &r) override
#if !defined(DAISY_SEED_BUILD)
    {
        if (!bufL_ || !bufR_)
            return;
        
        float outL = 0.0f;
        float outR = 0.0f;
        
        switch (state_)
        {
        case State::Idle:
            // Pass through
            outL = l;
            outR = r;
            break;
            
        case State::Recording:
            // Record input to buffer
            if (writePos_ < MAX_SAMPLES)
            {
                bufL_[writePos_] = l;
                bufR_[writePos_] = r;
                writePos_++;
            }
            else
            {
                // Buffer full, stop recording and start playing
                loopLength_ = MAX_SAMPLES;
                readPos_ = 0;
                state_ = State::Playing;
            }
            // Pass through during recording
            outL = l;
            outR = r;
            break;
            
        case State::Playing:
            // Play back loop
            if (loopLength_ > 0)
            {
                // Apply crossfade at loop boundaries for seamless looping
                float playL = bufL_[readPos_];
                float playR = bufR_[readPos_];
                
                // Crossfade at loop start/end
                if (readPos_ < FADE_SAMPLES)
                {
                    // Fade in from end
                    float fadeIn = (float)readPos_ / (float)FADE_SAMPLES;
                    int endPos = loopLength_ - FADE_SAMPLES + readPos_;
                    if (endPos >= 0 && endPos < loopLength_)
                    {
                        float fadeOut = 1.0f - fadeIn;
                        playL = playL * fadeIn + bufL_[endPos] * fadeOut;
                        playR = playR * fadeIn + bufR_[endPos] * fadeOut;
                    }
                }
                
                outL = l + playL * level_;
                outR = r + playR * level_;
                
                readPos_++;
                if (readPos_ >= loopLength_)
                    readPos_ = 0;
            }
            else
            {
                // No loop, pass through
                outL = l;
                outR = r;
            }
            break;
            
        case State::Overdubbing:
            // Play back and record simultaneously with feedback
            if (loopLength_ > 0)
            {
                // Read existing content
                float playL = bufL_[readPos_];
                float playR = bufR_[readPos_];
                
                // Apply crossfade at loop boundaries
                if (readPos_ < FADE_SAMPLES)
                {
                    float fadeIn = (float)readPos_ / (float)FADE_SAMPLES;
                    int endPos = loopLength_ - FADE_SAMPLES + readPos_;
                    if (endPos >= 0 && endPos < loopLength_)
                    {
                        float fadeOut = 1.0f - fadeIn;
                        playL = playL * fadeIn + bufL_[endPos] * fadeOut;
                        playR = playR * fadeIn + bufR_[endPos] * fadeOut;
                    }
                }
                
                // Mix new input with existing (overdub with feedback)
                bufL_[readPos_] = playL * feedback_ + l;
                bufR_[readPos_] = playR * feedback_ + r;
                
                // Output is mix of input and playback
                outL = l + playL * level_;
                outR = r + playR * level_;
                
                readPos_++;
                if (readPos_ >= loopLength_)
                    readPos_ = 0;
            }
            else
            {
                // No loop, pass through
                outL = l;
                outR = r;
            }
            break;
        }
        
        l = outL;
        r = outR;
    }
#else
    ; // Firmware: defined in effects_itcmram.cpp (ITCMRAM-placed)
#endif
};
