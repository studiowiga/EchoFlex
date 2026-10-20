#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>

//==============================================================================
// ECHOFLEX (Studio Wiga) — reproduction de la pedale Dunlop EP103 Echoplex Delay
// (emulation numerique de l'echo a bande Maestro Echoplex EP-3).
// Voir README.md pour la documentation.
//==============================================================================

namespace wiga
{
    // Biquad RBJ (passe-bas, passe-haut, cloche, plateau grave)
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
        {
            b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
            a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
        }

        void setLowpass (double sr, double fc, double q = 0.7071)
        {
            fc = juce::jlimit (20.0, sr * 0.45, fc);
            const double w = juce::MathConstants<double>::twoPi * fc / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
        }

        void setHighpass (double sr, double fc, double q = 0.7071)
        {
            fc = juce::jlimit (10.0, sr * 0.45, fc);
            const double w = juce::MathConstants<double>::twoPi * fc / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
        }

        void setPeak (double sr, double fc, double q, double gainDb)
        {
            fc = juce::jlimit (20.0, sr * 0.45, fc);
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = juce::MathConstants<double>::twoPi * fc / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set (1.0 + al * A, -2.0 * c, 1.0 - al * A, 1.0 + al / A, -2.0 * c, 1.0 - al / A);
        }

        void setLowShelf (double sr, double fc, double gainDb)
        {
            fc = juce::jlimit (20.0, sr * 0.45, fc);
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = juce::MathConstants<double>::twoPi * fc / sr, c = std::cos (w), s = std::sin (w);
            const double al = s / 2.0 * std::sqrt (2.0);
            const double sa = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                 (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
        }

        inline float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void reset() noexcept { z1 = z2 = 0.0f; }
    };

    struct PresetData
    {
        const char* name;
        float delay, sustain, volume, age;   // 0..10 comme sur la pedale
        int   ageOn;                         // 0 = son clair d'usine, 1 = reglage AGE actif
    };

    // Une piste de bande (ligne a retard + tete d'enregistrement + boucle de reinjection)
    struct TapeChannel
    {
        std::vector<float> line;
        int mask = 0, write = 0;
        Biquad recLP, loopLP, loopHP, preHP, preLow, prePeak, outLP;
        float fbState = 0.0f;

        void prepare (int size)
        {
            line.assign ((size_t) size, 0.0f);
            mask = size - 1; write = 0; fbState = 0.0f;
            for (auto* f : { &recLP, &loopLP, &loopHP, &preHP, &preLow, &prePeak, &outLP })
                f->reset();
        }

        inline float read (float dSamp) const noexcept
        {
            dSamp = juce::jlimit (2.0f, (float) (mask - 4), dSamp);
            const float rp = (float) write - dSamp;
            const int   i  = (int) std::floor (rp);
            const float fr = rp - (float) i;
            const float xm1 = line[(size_t) ((i - 1) & mask)], x0 = line[(size_t) (i & mask)];
            const float x1  = line[(size_t) ((i + 1) & mask)], x2 = line[(size_t) ((i + 2) & mask)];
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * fr + c2) * fr + c1) * fr + x0;
        }
    };
}

//==============================================================================
class EchoflexAudioProcessor : public juce::AudioProcessor
{
public:
    EchoflexAudioProcessor();
    ~EchoflexAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Presets d'usine
    static int getNumFactoryPresets();
    static const wiga::PresetData& getFactoryPreset (int index);
    void applyFactoryPreset (int index);

    // Presets utilisateur (emplacements 1 a 9, sauvegardes sur le disque)
    bool saveUserPreset (int slot);
    bool loadUserPreset (int slot);
    bool hasUserPreset (int slot) const;
    static juce::File getUserPresetFile();

    // TAP : appel depuis l'interface (clic sur la prise TAP) ou le MIDI
    void tap();
    static float knobToMs (float knob);          // potard DELAY 0..10 -> 65..750 ms
    static double syncBeats (int division);      // division -> nombre de temps
    static juce::StringArray syncNames();

    // Informations pour l'interface
    std::atomic<int>   currentPreset   { 0 };    // >=0 usine, 100+n utilisateur, -1 = modifie
    std::atomic<int>   lastMidiCC      { -1 };
    std::atomic<int>   lastMidiValue   { 0 };
    std::atomic<int>   lastMidiProgram { -1 };
    std::atomic<int>   midiCounter     { 0 };
    std::atomic<int>   tapCounter      { 0 };
    std::atomic<float> delayMsDisplay  { 300.0f }; // temps de retard reel
    std::atomic<float> echoLevel       { 0.0f };   // niveau des repetitions (animation)
    std::atomic<bool>  ageEdit         { false };  // potard VOLUME en mode reglage AGE (LED clignote)

    enum MidiMap
    {
        ccDelay      = 20,
        ccSustain    = 21,
        ccVolume     = 22,
        ccAge        = 23,
        ccAgeOn      = 24, // >= 64 : AGE actif, < 64 : son clair
        ccTap        = 25, // chaque appui (passage >= 64) = un TAP
        ccFootswitch = 26, // >= 64 : effet ON, < 64 : effet OFF
        ccFootToggle = 27  // chaque appui (passage >= 64) inverse l'etat
    };

private:
    void handleMidi (juce::MidiBuffer& midi);
    void setParam (const juce::String& id, float plainValue);
    float computeTargetMs();

    std::atomic<float>* pDelay   = nullptr;
    std::atomic<float>* pSustain = nullptr;
    std::atomic<float>* pVolume  = nullptr;
    std::atomic<float>* pAge     = nullptr;
    std::atomic<float>* pAgeOn   = nullptr;
    std::atomic<float>* pActive  = nullptr;
    std::atomic<float>* pTrails  = nullptr;
    std::atomic<float>* pWet     = nullptr;
    std::atomic<float>* pStereo  = nullptr;
    std::atomic<float>* pTimeSrc = nullptr;
    std::atomic<float>* pSyncDiv = nullptr;
    std::atomic<float>* pTapMs   = nullptr;

    wiga::TapeChannel chan[2];
    double sr = 44100.0;

    // etats lisses
    float smDelaySamp = 0.0f, smFb = 0.0f, smVol = 0.0f, smAge = 0.0f;
    float smActive = 1.0f, smDry = 1.0f, smInGate = 1.0f;
    float tapeCoef = 0.0f, ctrlCoef = 0.0f, activeCoef = 0.0f;

    // mecanique de bande
    double wowPhase = 0.0, flPhase1 = 0.0, flPhase2 = 0.0;
    float  drift = 0.0f, driftTarget = 0.0f, distress = 0.0f, distressTarget = 0.0f;
    int    driftCounter = 0;
    float  hissEnv = 0.0f;
    juce::Random noise;
    float  lastAgeParamCut = -1.0f;

    // TAP
    double lastTapTime = 0.0;
    double tapIntervals[3] { 0.0, 0.0, 0.0 };
    int    numTapIntervals = 0;
    int    prevTapCC = 0, prevToggleValue = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoflexAudioProcessor)
};
