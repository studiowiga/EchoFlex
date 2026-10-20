#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Presets d'usine — reglages notes comme sur la pedale (0 a 10)
// DELAY : 0 = 65 ms, 5 = 221 ms, 10 = 750 ms (courbe logarithmique)
//==============================================================================
static const wiga::PresetData factoryPresets[] =
{
    //  nom                     delay sustain volume age  ageOn
    { "Noon (Manual Start)",     5.0f, 5.0f,  5.0f,  5.0f, 0 },   // reglage de depart du manuel : tout a midi
    { "Fifties Slapback",        2.1f, 1.2f,  6.5f,  4.0f, 1 },
    { "Rockabilly Double",       2.8f, 3.5f,  5.5f,  3.0f, 1 },
    { "Surf Echo",               3.6f, 4.5f,  6.5f,  2.0f, 0 },
    { "Hot Lead 400",            7.3f, 4.8f,  5.0f,  2.5f, 1 },
    { "Fresh Tape Wash",         8.6f, 6.5f,  6.0f,  0.0f, 0 },
    { "Worn Cartridge",          6.2f, 5.5f,  6.5f,  10.f, 1 },
    { "Dark Ambience",           9.3f, 7.6f,  6.0f,  8.0f, 1 },
    { "Dub Spiral",              7.6f, 8.7f,  7.0f,  6.0f, 1 },
    { "Runaway Oscillation",     4.2f, 10.f,  7.0f,  5.0f, 1 },
};

int EchoflexAudioProcessor::getNumFactoryPresets() { return (int) std::size (factoryPresets); }

const wiga::PresetData& EchoflexAudioProcessor::getFactoryPreset (int index)
{
    return factoryPresets[(size_t) juce::jlimit (0, getNumFactoryPresets() - 1, index)];
}

//==============================================================================
float EchoflexAudioProcessor::knobToMs (float knob)
{
    return 65.0f * std::pow (750.0f / 65.0f, juce::jlimit (0.0f, 10.0f, knob) / 10.0f);
}

juce::StringArray EchoflexAudioProcessor::syncNames()
{
    return { "1/2", "1/4 dotted", "1/4", "1/4 triplet", "1/8 dotted", "1/8", "1/8 triplet", "1/16" };
}

double EchoflexAudioProcessor::syncBeats (int d)
{
    static const double beats[] = { 2.0, 1.5, 1.0, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0, 0.25 };
    return beats[juce::jlimit (0, 7, d)];
}

//==============================================================================
EchoflexAudioProcessor::EchoflexAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ECHOFLEX", createLayout())
{
    pDelay   = apvts.getRawParameterValue ("delay");
    pSustain = apvts.getRawParameterValue ("sustain");
    pVolume  = apvts.getRawParameterValue ("volume");
    pAge     = apvts.getRawParameterValue ("age");
    pAgeOn   = apvts.getRawParameterValue ("ageOn");
    pActive  = apvts.getRawParameterValue ("active");
    pTrails  = apvts.getRawParameterValue ("bypassMode");
    pWet     = apvts.getRawParameterValue ("mix");
    pStereo  = apvts.getRawParameterValue ("channels");
    pTimeSrc = apvts.getRawParameterValue ("timeSrc");
    pSyncDiv = apvts.getRawParameterValue ("syncDiv");
    pTapMs   = apvts.getRawParameterValue ("tapMs");
}

juce::AudioProcessorValueTreeState::ParameterLayout EchoflexAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto knob = [] (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return String (v, 1); }));
    };

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "delay", 1 }, "Delay", NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (roundToInt (knobToMs (v))) + " ms"; })));
    p.push_back (knob ("sustain", "Sustain", 5.0f));
    p.push_back (knob ("volume",  "Volume",  5.0f));
    p.push_back (knob ("age",     "Age",     5.0f));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "ageOn", 1 },  "Age On", false));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "active", 1 }, "Footswitch", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "bypassMode", 1 }, "Bypass",
                                                         StringArray { "True bypass", "Trails" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mix", 1 }, "Mix",
                                                         StringArray { "Dry + Echo", "100% Wet" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "channels", 1 }, "Channels",
                                                         StringArray { "Mono", "Stereo" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "timeSrc", 1 }, "Time Source",
                                                         StringArray { "Knob", "Tap", "Host sync" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "syncDiv", 1 }, "Sync Division",
                                                         syncNames(), 2));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "tapMs", 1 }, "Tap Time", NormalisableRange<float> (65.0f, 4000.0f, 0.1f, 0.4f), 500.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (roundToInt (v)) + " ms"; })));

    return { p.begin(), p.end() };
}

bool EchoflexAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void EchoflexAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    // 4 s (TAP) + modulation + marge
    int size = 1;
    while (size < (int) (sr * 4.3) + 16)
        size <<= 1;

    for (auto& c : chan)
        c.prepare (size);

    tapeCoef   = (float) (1.0 - std::exp (-1.0 / (0.110 * sr)));   // inertie du moteur de bande
    ctrlCoef   = (float) (1.0 - std::exp (-1.0 / (0.030 * sr)));
    activeCoef = (float) (1.0 - std::exp (-1.0 / (0.008 * sr)));

    smDelaySamp = computeTargetMs() * (float) sr / 1000.0f;
    smActive = smInGate = pActive->load() > 0.5f ? 1.0f : 0.0f;
    smDry    = pWet->load() > 0.5f ? 0.0f : 1.0f;
    smVol    = 0.0f;
    smFb     = 0.0f;
    smAge    = pAgeOn->load() > 0.5f ? pAge->load() / 10.0f : 0.0f;
    lastAgeParamCut = -1.0f;
    wowPhase = flPhase1 = flPhase2 = 0.0;
    drift = driftTarget = distress = distressTarget = 0.0f;
    hissEnv = 0.0f;
}

//==============================================================================
void EchoflexAudioProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void EchoflexAudioProcessor::tap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt  = now - lastTapTime;
    lastTapTime = now;

    if (dt >= 60.0 && dt <= 4200.0)
    {
        // moyenne des 3 derniers intervalles (comme une M199 + EP103)
        tapIntervals[2] = tapIntervals[1];
        tapIntervals[1] = tapIntervals[0];
        tapIntervals[0] = juce::jmin (dt, 4000.0);
        numTapIntervals = juce::jmin (3, numTapIntervals + 1);

        // un intervalle tres different relance la moyenne
        if (numTapIntervals > 1 && std::abs (tapIntervals[0] - tapIntervals[1]) > 0.35 * tapIntervals[1])
            numTapIntervals = 1;

        double sum = 0.0;
        for (int i = 0; i < numTapIntervals; ++i)
            sum += tapIntervals[i];

        setParam ("tapMs", (float) juce::jlimit (65.0, 4000.0, sum / numTapIntervals));
        setParam ("timeSrc", 1.0f);
        currentPreset = -1;
    }
    else
    {
        numTapIntervals = 0;
    }

    ++tapCounter;
}

float EchoflexAudioProcessor::computeTargetMs()
{
    const int src = juce::roundToInt (pTimeSrc->load());

    if (src == 1)
        return juce::jlimit (65.0f, 4000.0f, pTapMs->load());

    if (src == 2)
    {
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto bpm = pos->getBpm())
                    if (*bpm > 1.0)
                        return (float) juce::jlimit (65.0, 4000.0,
                                                     60000.0 / *bpm * syncBeats (juce::roundToInt (pSyncDiv->load())));
    }

    return knobToMs (pDelay->load());
}

void EchoflexAudioProcessor::handleMidi (juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();

        if (m.isController())
        {
            const int cc = m.getControllerNumber();
            const int v  = m.getControllerValue();
            const float knobValue = (float) v / 127.0f * 10.0f;

            switch (cc)
            {
                case ccDelay:      setParam ("delay", knobValue); setParam ("timeSrc", 0.0f); currentPreset = -1; break;
                case ccSustain:    setParam ("sustain", knobValue); currentPreset = -1; break;
                case ccVolume:     setParam ("volume",  knobValue); currentPreset = -1; break;
                case ccAge:        setParam ("age",     knobValue); currentPreset = -1; break;
                case ccAgeOn:      setParam ("ageOn",  v >= 64 ? 1.0f : 0.0f); break;
                case ccFootswitch: setParam ("active", v >= 64 ? 1.0f : 0.0f); break;
                case ccTap:
                    if (v >= 64 && prevTapCC < 64)
                        tap();
                    prevTapCC = v;
                    break;
                case ccFootToggle:
                    if (v >= 64 && prevToggleValue < 64)
                        setParam ("active", pActive->load() > 0.5f ? 0.0f : 1.0f);
                    prevToggleValue = v;
                    break;
                default: break;
            }

            lastMidiCC    = cc;
            lastMidiValue = v;
            ++midiCounter;
        }
        else if (m.isProgramChange())
        {
            const int prog = m.getProgramChangeNumber();
            if (prog < getNumFactoryPresets())
                applyFactoryPreset (prog);

            lastMidiProgram = prog;
            lastMidiCC      = -2; // indique un Program Change
            ++midiCounter;
        }
    }

    midi.clear();
}

//==============================================================================
// Traitement audio — modele « EP-3 en boitier » (EP103)
//
//  Entree --+--------------------------------------------- direct (analogique, intact)
//           |                                                       |
//           +--> tete d'enregistrement (passe-bas) --> BANDE --> tete de lecture
//                         ^                                   |        |
//                         +-- SUSTAIN <-- usure de bande <-----+        v
//                                     (passe-haut, passe-bas,     preampli EP101
//                                      saturation)                 (grave + presence)
//                                                                       |
//                                                     VOLUME (niveau d'echo) --> sortie
//
//  - Pleurage (wow ~0,55 Hz), scintillement (flutter 7,3 / 11,7 Hz) et derive lente
//    aleatoire modulent la vitesse de bande : leger par defaut, tres marque en AGE.
//  - AGE : assombrit, sature, module davantage et ajoute des « faiblesses » de bande.
//  - Le moteur de bande a de l'inertie : tourner DELAY fait glisser la hauteur.
//  - SUSTAIN : peu de repetitions sur la 1re moitie, croissance rapide ensuite,
//    auto-oscillation au maximum (bornee par la saturation de la bande).
//==============================================================================
void EchoflexAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    handleMidi (midi);

    const int numSamples = buffer.getNumSamples();
    const int inChans    = getTotalNumInputChannels();
    const int outChans   = getTotalNumOutputChannels();

    if (numSamples == 0 || chan[0].line.empty())
        return;

    const float targetMs = computeTargetMs();
    delayMsDisplay = targetMs;
    const float targetSamp = targetMs * (float) sr / 1000.0f;

    const float s = pSustain->load() / 10.0f;
    const float v = pVolume->load()  / 10.0f;
    const bool  active  = pActive->load() > 0.5f;
    const bool  trails  = pTrails->load() > 0.5f;
    const bool  wetOnly = pWet->load()    > 0.5f;
    const bool  stereo  = pStereo->load() > 0.5f && inChans > 1 && outChans > 1;
    const float ageT    = pAgeOn->load() > 0.5f ? pAge->load() / 10.0f : 0.0f;

    // SUSTAIN : 1re moitie douce (1 a 2 repetitions), 2e moitie quasi exponentielle
    float g = s < 0.5f ? 0.62f * s
                       : 0.31f + 0.77f * std::pow ((s - 0.5f) / 0.5f, 1.6f);
    g *= 0.92f + 0.16f * v;                              // interaction VOLUME / SUSTAIN (comme la pedale)
    const float volT = 1.45f * std::pow (v, 1.5f);       // midi ~ -7 dB, maxi ~ +3 dB

    // filtres : mis a jour par bloc avec AGE lisse
    smAge += (ageT - smAge) * 0.25f;
    const float a = smAge;
    if (std::abs (a - lastAgeParamCut) > 1.0e-4f)
    {
        lastAgeParamCut = a;
        for (auto& c : chan)
        {
            c.recLP .setLowpass  (sr, 9500.0 * (1.0 - a) + 3600.0 * a, 0.707);
            c.loopLP.setLowpass  (sr, 7200.0 * (1.0 - a) + 2500.0 * a, 0.60);
            c.loopHP.setHighpass (sr, 95.0 + 120.0 * a, 0.60);
            c.preHP .setHighpass (sr, 55.0, 0.70);
            c.preLow.setLowShelf (sr, 180.0, 1.5);
            c.prePeak.setPeak    (sr, 3000.0 - 900.0 * a, 0.8, 2.5);
            c.outLP .setLowpass  (sr, 13000.0 * (1.0 - a) + 5200.0 * a, 0.707);
        }
    }

    const float wowDepthMs   = 0.14f  + 2.1f  * a;
    const float flDepthMs    = 0.011f + 0.10f * a;
    const float driftDepthMs = 0.05f  + 0.85f * a;
    const double wowRate     = 0.55 + 0.25 * a;
    const float drive        = 1.05f + 1.7f * a;
    const float asym         = 0.10f * a;
    const float hissLevel    = 0.00008f + 0.0009f * a;
    const float msToSamp     = (float) sr / 1000.0f;
    const int   driftPeriod  = juce::jmax (1, (int) (sr / 25.0));
    const float driftCoef    = (float) (1.0 - std::exp (-1.0 / (0.12 * sr)));
    const float distCoef     = (float) (1.0 - std::exp (-1.0 / (0.05 * sr)));
    const float envCoef      = (float) (1.0 - std::exp (-1.0 / (0.25 * sr)));

    const float* inL = buffer.getReadPointer (0);
    const float* inR = inChans > 1 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = outChans > 1 ? buffer.getWritePointer (1) : nullptr;

    auto tapeSat = [drive, asym] (float x)
    {
        x += asym * x * x;
        return std::tanh (drive * x) / drive;
    };

    auto safety = [] (float x)
    {
        const float ax = std::abs (x);
        if (ax < 0.97f) return x;
        return std::copysign (0.97f + 0.03f * std::tanh ((ax - 0.97f) / 0.03f), x);
    };

    const int nCh = stereo ? 2 : 1;
    float levelAcc = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        smDelaySamp += (targetSamp - smDelaySamp) * tapeCoef;
        smFb        += (g    - smFb)  * ctrlCoef;
        smVol       += (volT - smVol) * ctrlCoef;
        smActive    += ((active ? 1.0f : 0.0f) - smActive) * activeCoef;
        smInGate    += ((active ? 1.0f : 0.0f) - smInGate) * activeCoef;
        smDry       += ((wetOnly ? 0.0f : 1.0f) - smDry)  * activeCoef;

        // --- mecanique de bande ---------------------------------------------------
        wowPhase += wowRate / sr;           if (wowPhase >= 1.0) wowPhase -= 1.0;
        flPhase1 += 7.3 / sr;               if (flPhase1 >= 1.0) flPhase1 -= 1.0;
        flPhase2 += 11.7 / sr;              if (flPhase2 >= 1.0) flPhase2 -= 1.0;

        if (++driftCounter >= driftPeriod)
        {
            driftCounter = 0;
            driftTarget = noise.nextFloat() * 2.0f - 1.0f;
            const float r = noise.nextFloat();
            distressTarget = r * r * r * r;
        }
        drift    += (driftTarget    - drift)    * driftCoef;
        distress += (distressTarget - distress) * distCoef;

        const float twoPi = juce::MathConstants<float>::twoPi;
        const float modMs = wowDepthMs * std::sin (twoPi * (float) wowPhase)
                          + flDepthMs  * (std::sin (twoPi * (float) flPhase1) + 0.4f * std::sin (twoPi * (float) flPhase2))
                          + driftDepthMs * drift;
        const float dSamp = juce::jmax (4.0f, smDelaySamp + modMs * msToSamp);
        const float tapeGain = 1.0f - 0.38f * a * distress;     // faiblesses / pertes de contact de la bande
        const float hiss = (noise.nextFloat() * 2.0f - 1.0f) * hissLevel * (0.15f + hissEnv);

        const float dryInL = inL[i];
        const float dryInR = inR[i];
        float wet[2] { 0.0f, 0.0f };

        for (int c = 0; c < nCh; ++c)
        {
            auto& ch = chan[c];
            const float x = nCh == 2 ? (c == 0 ? dryInL : dryInR) : 0.5f * (dryInL + dryInR);

            const float y = ch.read (dSamp) * tapeGain;

            // boucle de reinjection : bande usee (bande passante reduite + saturation)
            float fb = ch.loopLP.process (ch.loopHP.process (y));
            fb = tapeSat (fb);
            ch.fbState = fb;

            const float rec = ch.recLP.process (x * smInGate + smFb * fb + hiss);
            ch.line[(size_t) ch.write] = tapeSat (rec * 0.9f) / 0.9f;
            ch.write = (ch.write + 1) & ch.mask;

            // preampli EP101 sur le signal d'echo uniquement
            float w = ch.prePeak.process (ch.preLow.process (ch.preHP.process (y)));
            w = std::tanh (1.2f * w) / 1.2f;
            wet[c] = ch.outLP.process (w) * smVol;
        }

        if (nCh == 1)
            wet[1] = wet[0];

        hissEnv += (juce::jmin (1.0f, std::abs (wet[0]) * 6.0f) - hissEnv) * envCoef;
        levelAcc = juce::jmax (levelAcc, std::abs (wet[0]));

        const float act = smActive;
        const float wetGate = trails ? 1.0f : act;
        const float oL = dryInL * (1.0f - act) + dryInL * smDry * act + wet[0] * wetGate;
        const float oR = dryInR * (1.0f - act) + dryInR * smDry * act + wet[1] * wetGate;

        outL[i] = safety (oL);
        if (outR != nullptr)
            outR[i] = safety (oR);
    }

    for (int chn = 2; chn < outChans; ++chn)
        buffer.clear (chn, 0, numSamples);

    echoLevel = levelAcc;
}

//==============================================================================
// Programmes (menu de presets du DAW)
//==============================================================================
int EchoflexAudioProcessor::getNumPrograms() { return getNumFactoryPresets(); }

int EchoflexAudioProcessor::getCurrentProgram()
{
    const int c = currentPreset.load();
    return (c >= 0 && c < getNumFactoryPresets()) ? c : 0;
}

void EchoflexAudioProcessor::setCurrentProgram (int index) { applyFactoryPreset (index); }

const juce::String EchoflexAudioProcessor::getProgramName (int index)
{
    return getFactoryPreset (index).name;
}

void EchoflexAudioProcessor::applyFactoryPreset (int index)
{
    const auto& p = getFactoryPreset (index);
    setParam ("delay",   p.delay);
    setParam ("sustain", p.sustain);
    setParam ("volume",  p.volume);
    setParam ("age",     p.age);
    setParam ("ageOn",   (float) p.ageOn);
    setParam ("timeSrc", 0.0f);
    currentPreset = juce::jlimit (0, getNumFactoryPresets() - 1, index);
}

//==============================================================================
// Presets utilisateur
//==============================================================================
juce::File EchoflexAudioProcessor::getUserPresetFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Studio Wiga")
             .getChildFile ("Echoflex")
             .getChildFile ("UserPresets.xml");
}

bool EchoflexAudioProcessor::saveUserPreset (int slot)
{
    if (slot < 1 || slot > 9)
        return false;

    auto file = getUserPresetFile();
    file.getParentDirectory().createDirectory();

    std::unique_ptr<juce::XmlElement> root;
    if (file.existsAsFile())
        root = juce::XmlDocument::parse (file);
    if (root == nullptr || ! root->hasTagName ("UserPresets"))
        root = std::make_unique<juce::XmlElement> ("UserPresets");

    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
    {
        if (e->getIntAttribute ("slot") == slot)
        {
            root->removeChildElement (e, true);
            break;
        }
    }

    auto* e = root->createNewChildElement ("Preset");
    e->setAttribute ("slot",    slot);
    e->setAttribute ("delay",   (double) pDelay->load());
    e->setAttribute ("sustain", (double) pSustain->load());
    e->setAttribute ("volume",  (double) pVolume->load());
    e->setAttribute ("age",     (double) pAge->load());
    e->setAttribute ("ageOn",   pAgeOn->load() > 0.5f ? 1 : 0);
    e->setAttribute ("timeSrc", juce::roundToInt (pTimeSrc->load()));
    e->setAttribute ("tapMs",   (double) pTapMs->load());
    e->setAttribute ("syncDiv", juce::roundToInt (pSyncDiv->load()));

    const bool ok = root->writeTo (file);
    if (ok)
        currentPreset = 100 + slot;
    return ok;
}

bool EchoflexAudioProcessor::loadUserPreset (int slot)
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile())
        return false;

    auto root = juce::XmlDocument::parse (file);
    if (root == nullptr)
        return false;

    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
    {
        if (e->getIntAttribute ("slot") == slot)
        {
            setParam ("delay",   (float) e->getDoubleAttribute ("delay",   5.0));
            setParam ("sustain", (float) e->getDoubleAttribute ("sustain", 5.0));
            setParam ("volume",  (float) e->getDoubleAttribute ("volume",  5.0));
            setParam ("age",     (float) e->getDoubleAttribute ("age",     5.0));
            setParam ("ageOn",   (float) e->getIntAttribute    ("ageOn",   0));
            setParam ("tapMs",   (float) e->getDoubleAttribute ("tapMs",   500.0));
            setParam ("syncDiv", (float) e->getIntAttribute    ("syncDiv", 2));
            setParam ("timeSrc", (float) e->getIntAttribute    ("timeSrc", 0));
            currentPreset = 100 + slot;
            return true;
        }
    }
    return false;
}

bool EchoflexAudioProcessor::hasUserPreset (int slot) const
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile())
        return false;

    if (auto root = juce::XmlDocument::parse (file))
        for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
            if (e->getIntAttribute ("slot") == slot)
                return true;

    return false;
}

//==============================================================================
void EchoflexAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EchoflexAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = (int) state.getProperty ("currentPreset", 0);
            apvts.replaceState (state);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* EchoflexAudioProcessor::createEditor()
{
    return new EchoflexAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EchoflexAudioProcessor();
}
