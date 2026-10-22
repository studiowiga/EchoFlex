#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Potard : filmstrip haute definition (potard Echoflex d'apres la photo du visuel),
// lumiere fixe en haut a gauche ; aileron, trait blanc et dents tournent.
// Une copie deja mise a la taille d'affichage est gardee en cache (affichage fluide).
//==============================================================================
class PointerKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
    void prepareScaled (int pixelSize);    // pixelSize = cote d'une image en pixels physiques
    juce::Image strip;                     // filmstrip d'origine : 128 images carrees
    juce::Image scaledStrip;               // copie a la taille d'affichage
    int scaledSize = 0;
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
// Zone cliquable (prise TAP, commande SYNC) : dessin fait par l'editeur
//==============================================================================
class ClickArea : public juce::Component
{
public:
    std::function<void()> onClick, onRightClick;
    std::function<void (float)> onWheel;
    ClickArea() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override {}
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); }
        else if (onClick) onClick();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (onWheel) onWheel (w.deltaY);
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
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Repere de reference (100 %) ; taille par defaut = taille minimale (identique a Wack'd)
    static constexpr int baseW = 760;
    static constexpr int baseH = 958;
    static constexpr int defaultH = 493;

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void showContextMenu();
    void showSyncMenu();
    void setScaledHeight (int h);
    void attachVolumeKnob (bool toAge);
    void setAgeEdit (bool on);
    void toggleAge();
    void toggleSync();
    void stepSyncDivision (int delta);
    void setParamValue (const char* id, float plain);
    void drawLeds (juce::Graphics&);
    void drawStatus (juce::Graphics&);
    void drawTapFlash (juce::Graphics&);
    void showStatus();
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    juce::String buildStatusLine() const;
    juce::String buildPresetLine() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;

    EchoflexAudioProcessor& proc;
    PointerKnobLookAndFeel lnf;
    RightClickForwarder rightClickFwd;
    juce::Image background, bgScaled;
    int cacheW = 0, cacheH = 0;
    float cacheScale = 0.0f;
    int resizeSettle = 0;

    juce::Slider sustain, delay;
    PushKnob volume;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aSustain, aDelay, aVolume;
    bool volumeOnAge = false;

    FootSwitchArea footswitch;
    ClickArea tapArea, syncArea;

    // Animation LED / ligne d'etat
    juce::Random rng;
    float ledFlicker = 1.0f, ledLevel = 1.0f, echoGlow = 0.0f;
    float ageLedLevel = 0.0f;
    float syncLedLevel = 0.0f;
    float tapFlash = 0.0f;
    int   lastTapCount = 0;
    double blinkPhase = 0.0;
    double freePhase = 0.0;
    double lastTime = 0.0;
    float drawnLed = -1.0f, drawnAge = -1.0f, drawnSync = -1.0f, drawnTap = -1.0f, drawnStatus = -1.0f;

    juce::String lastStatus, lastPreset;
    float lastKnobs[4] { -1.0f, -1.0f, -1.0f, -1.0f };
    float lastAgeOn = -1.0f, lastTimeSrc = -1.0f, lastSyncDiv = -1.0f;
    double statusUntil = 0.0;
    float statusAlpha = 0.0f;
    int lastMidiCount = 0;
    int midiFlash = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoflexAudioProcessorEditor)
};
