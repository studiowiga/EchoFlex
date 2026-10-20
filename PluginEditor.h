#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Potard : filmstrip haute definition (potard noir a aileron facon Echoplex),
// lumiere fixe en haut a gauche ; aileron, trait blanc et moletage tournent
//==============================================================================
class PointerKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
    juce::Image strip;   // filmstrip verticale : 128 images carrees
};

//==============================================================================
// Potard VOLUME « poussoir » : clic court = AGE on/off, appui long (2 s) = reglage AGE
//==============================================================================
class PushKnob : public juce::Slider
{
public:
    std::function<void()> onShortPress;
    std::function<void()> onLongPress;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    // appele par le timer de l'editeur
    void checkLongPress (double nowMs);
    bool isHolding() const { return holding && ! moved && ! longFired; }
    double holdStart() const { return downTime; }

private:
    bool holding = false, moved = false, longFired = false;
    double downTime = 0.0;
};

//==============================================================================
// Zone cliquable du footswitch (dessine sur l'image de fond)
//==============================================================================
class FootSwitchArea : public juce::Component
{
public:
    explicit FootSwitchArea (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    bool isOn() const { return value; }

private:
    bool value = true;
    bool pressed = false;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// Zone cliquable de la prise TAP (remplace la pedale MXR M199)
//==============================================================================
class TapArea : public juce::Component
{
public:
    std::function<void()> onTap;
    TapArea() { setMouseCursor (juce::MouseCursor::PointingHandCursor); setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics&) override {}
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu() && onTap)
            onTap();
    }
};

//==============================================================================
// Transmet le clic droit des commandes vers le menu de l'editeur
//==============================================================================
struct RightClickForwarder : public juce::MouseListener
{
    std::function<void()> callback;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && callback)
            callback();
    }
};

//==============================================================================
class EchoflexAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit EchoflexAudioProcessorEditor (EchoflexAudioProcessor&);
    ~EchoflexAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Repere de reference (100 %) ; taille par defaut = taille minimale (identique a Wack'd)
    static constexpr int baseW = 760;
    static constexpr int baseH = 958;
    static constexpr int defaultH = 493;

private:
    void timerCallback() override;
    void showContextMenu();
    void setScaledHeight (int h);
    void attachVolumeKnob (bool toAge);
    void setAgeEdit (bool on);
    void toggleAge();
    void setParamValue (const char* id, float plain);
    void drawLeds (juce::Graphics&);
    void drawStatus (juce::Graphics&);
    void drawTapFlash (juce::Graphics&);
    void showStatus();
    juce::String buildStatusLine() const;
    juce::String buildPresetLine() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;

    EchoflexAudioProcessor& proc;
    PointerKnobLookAndFeel lnf;
    RightClickForwarder rightClickFwd;
    juce::Image background;

    juce::Slider sustain, delay;
    PushKnob volume;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aSustain, aDelay, aVolume;
    bool volumeOnAge = false;

    FootSwitchArea footswitch;
    TapArea tapArea;

    // Animation LED / ligne d'etat
    juce::Random rng;
    float ledFlicker = 1.0f;
    float ledLevel = 1.0f;
    float echoGlow = 0.0f;
    float ageLedLevel = 0.0f;
    float tapFlash = 0.0f;
    int   lastTapCount = 0;
    double blinkPhase = 0.0;
    double lastTime = 0.0;

    juce::String lastStatus, lastPreset;
    float lastKnobs[4] { -1.0f, -1.0f, -1.0f, -1.0f };
    float lastAgeOn = -1.0f;
    double statusUntil = 0.0;
    float statusAlpha = 0.0f;
    int lastMidiCount = 0;
    int midiFlash = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoflexAudioProcessorEditor)
};
