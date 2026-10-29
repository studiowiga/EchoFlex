#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    Font font (float h, bool bold = true)
    {
        return Font (FontOptions (h, bold ? Font::bold : Font::plain));
    }

    // Positions dans le repere de reference 760 x 958 (meme cadrage que Wack'd),
    // relevees sur le visuel Echoflex mis a l'echelle du projet (1520 x 1916 = 2 x 760 x 958)
    const Point<float> knobVolume  { 380.5f, 147.5f };
    const Point<float> knobSustain { 200.9f, 280.0f };
    const Point<float> knobDelay   { 557.9f, 280.5f };
    const float knobR = 59.9f;                 // rayon exterieur des dents (couvre le potard de la photo)
    const float knobStripHalf = 1.20f;         // marge de l'image (ombre)
    const int   knobFrames = 128;

    const Point<float>     ledPos    { 381.0f, 547.8f };
    const float            ledR      = 12.2f;   // lentille rouge (dans la bague chromee de la photo)
    const Point<float>     agePos    { 379.4f, 290.7f };
    const float            ageR      = 5.8f;
    const Point<float>     syncPos   { 615.0f, 201.8f };   // LED SYNC a droite de l'inscription DELAY
    const float            syncR     = 5.8f;
    const Rectangle<float> statusRect{ 150.0f, 566.0f, 460.0f, 52.0f };
    const Point<float>     footPos   { 379.1f, 691.6f };
    const float            footR     = 55.0f;
    const Rectangle<float> tapRect   { 66.0f, 760.0f, 92.0f, 76.0f };   // prise TAP + inscription
    const Rectangle<float> syncRect  { 518.0f, 176.0f, 116.0f, 36.0f };  // inscription DELAY + LED SYNC

    const Colour redCore (0xffffd6c8);
    const Colour redMid  (0xffff2616);
    const Colour redDeep (0xff8c0504);
    const Colour ambCore (0xfffff1c8);
    const Colour ambMid  (0xffffa02a);
    const Colour ambDeep (0xffc85a00);
    const Colour grnCore (0xffe6ffe9);
    const Colour grnMid  (0xff36e05a);
    const Colour grnDeep (0xff0b7a24);

    constexpr double longPressMs = 2000.0;   // comme la pedale : maintenir VOLUME 2 s
}

//==============================================================================
// POTARD
//==============================================================================
void PointerKnobLookAndFeel::prepareScaled (int px)
{
    if (! strip.isValid() || px <= 0)
        return;
    if (px >= strip.getWidth())                // plus grand que l'original : on garde l'original
    {
        scaledStrip = {};
        scaledSize = 0;
        return;
    }
    if (px == scaledSize && scaledStrip.isValid())
        return;
    scaledStrip = strip.rescaled (px, px * knobFrames, Graphics::highResamplingQuality);
    scaledSize = px;
}

void PointerKnobLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                               float pos, float, float, Slider&)
{
    const auto& img = scaledStrip.isValid() ? scaledStrip : strip;
    if (! img.isValid())
        return;

    const int fs = img.getWidth();
    const int frame = jlimit (0, knobFrames - 1, roundToInt (pos * (float) (knobFrames - 1)));

    g.setImageResamplingQuality (scaledStrip.isValid() ? Graphics::mediumResamplingQuality
                                                       : Graphics::highResamplingQuality);
    g.drawImage (img, x, y, w, h, 0, frame * fs, fs, fs);
}

//==============================================================================
// POTARD POUSSOIR (VOLUME / AGE)
//==============================================================================
void PushKnob::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    holding = true; moved = false; longFired = false;
    downTime = Time::getMillisecondCounterHiRes();
    Slider::mouseDown (e);
}

void PushKnob::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isPopupMenu() || longFired)
        return;
    if (e.getDistanceFromDragStart() > 3)
        moved = true;
    Slider::mouseDrag (e);
}

void PushKnob::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    const bool wasClick = holding && ! moved && ! longFired
                          && Time::getMillisecondCounterHiRes() - downTime < 600.0;
    holding = false;
    Slider::mouseUp (e);

    if (wasClick && onShortPress)
        onShortPress();
}

void PushKnob::checkLongPress (double nowMs)
{
    if (isHolding() && nowMs - downTime >= longPressMs)
    {
        longFired = true;
        if (onLongPress)
            onLongPress();
    }
}

//==============================================================================
// FOOTSWITCH
//==============================================================================
FootSwitchArea::FootSwitchArea (RangedAudioParameter& p)
    : attachment (p, [this] (float v) { value = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool FootSwitchArea::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * 0.5f;
}

void FootSwitchArea::paint (Graphics& g)
{
    if (pressed)
    {
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (getLocalBounds().toFloat().reduced ((float) getWidth() * 0.23f));
    }
}

void FootSwitchArea::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    pressed = true;
    repaint();
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);
}

void FootSwitchArea::mouseUp (const MouseEvent&)
{
    pressed = false;
    repaint();
}

//==============================================================================
// EDITEUR
//==============================================================================
EchoflexAudioProcessorEditor::EchoflexAudioProcessorEditor (EchoflexAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      footswitch (*p.apvts.getParameter ("active"))
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    lnf.strip  = ImageCache::getFromMemory (BinaryData::knob_strip_png, BinaryData::knob_strip_pngSize);

    const char* ids[4] = { "delay", "sustain", "volume", "age" };
    for (int i = 0; i < 4; ++i)
        lastKnobs[i] = proc.apvts.getRawParameterValue (ids[i])->load();
    lastAgeOn   = proc.apvts.getRawParameterValue ("ageOn")->load() > 0.5f ? 1.0f : 0.0f;
    lastTimeSrc = proc.apvts.getRawParameterValue ("timeSrc")->load();
    lastSyncDiv = proc.apvts.getRawParameterValue ("syncDiv")->load();
    // clic droit sur une commande : MIDI Learn de cette commande en tete du menu
    rightClickFwd.callback = [this] (Component* c)
    {
        using P = EchoflexAudioProcessor;
        Array<int> items;
        if      (c == &delay)      items = { P::lDelay };
        else if (c == &sustain)    items = { P::lSustain };
        else if (c == &volume)     items = { P::lVolume, P::lAge, P::lAgeOn };   // VOLUME + poussoir AGE
        else if (c == &footswitch) items = { P::lFoot, P::lFootToggle };
        showContextMenu (items);
    };

    auto setupKnob = [this] (Slider& s)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (220);
        s.setLookAndFeel (&lnf);
        s.setMouseCursor (MouseCursor::PointingHandCursor);
        s.setBufferedToImage (false);
        s.addMouseListener (&rightClickFwd, false);
        addAndMakeVisible (s);
    };

    setupKnob (sustain);
    setupKnob (delay);
    setupKnob (volume);
    aSustain = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "sustain", sustain);
    aDelay   = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "delay",   delay);
    attachVolumeKnob (proc.ageEdit.load());

    // Tourner DELAY reprend la main sur le TAP (comme la pedale) ;
    // en SYNC, le potard DELAY choisit la division de tempo (1/16 ... 1/2)
    delay.onDragStart = [this]
    {
        if (roundToInt (proc.apvts.getRawParameterValue ("timeSrc")->load()) == 1)
            setParamValue ("timeSrc", 0.0f);
        proc.currentPreset = -1;
    };
    delay.onValueChange = [this]
    {
        if (roundToInt (proc.apvts.getRawParameterValue ("timeSrc")->load()) != 2 || ! delay.isMouseButtonDown())
            return;
        const int d = EchoflexAudioProcessor::syncDivFromKnob ((float) delay.getValue());
        if (d != roundToInt (proc.apvts.getRawParameterValue ("syncDiv")->load()))
            setParamValue ("syncDiv", (float) d);
    };
    sustain.onDragStart = [this] { proc.currentPreset = -1; };
    volume.onDragStart  = [this] { proc.currentPreset = -1; };

    volume.onShortPress = [this]
    {
        if (proc.ageEdit.load()) setAgeEdit (false);      // verrouille le reglage AGE
        else                     toggleAge();             // reglage AGE <-> son clair d'usine
    };
    volume.onLongPress = [this] { setAgeEdit (! proc.ageEdit.load()); };

    footswitch.addMouseListener (&rightClickFwd, false);
    addAndMakeVisible (footswitch);

    tapArea.onClick      = [this] { proc.tap(); };
    tapArea.onRightClick = [this] { showContextMenu ({ EchoflexAudioProcessor::lTap }); };
    addAndMakeVisible (tapArea);

    // SYNC : clic = suit le tempo du DAW (on/off), clic droit = division, molette = division
    syncArea.onClick      = [this] { toggleSync(); };
    syncArea.onRightClick = [this] { showSyncMenu(); };
    syncArea.onWheel      = [this] (float dy) { stepSyncDivision (dy > 0 ? -1 : 1); };
    addAndMakeVisible (syncArea);

    // Taille par defaut = taille minimale (impossible de reduire en dessous)
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* cons = getConstrainer())
        cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setOpaque (true);                      // anti-gel : l'hote ne redessine jamais ce qui est derriere la fenetre
    setSize (defaultW, defaultH);
    rebuildCaches();

    lastTapCount = proc.tapCounter.load();
    lastTime = Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

EchoflexAudioProcessorEditor::~EchoflexAudioProcessorEditor()
{
    stopTimer();
    proc.ageEdit = false;
    for (auto* s : { (Slider*) &sustain, (Slider*) &delay, (Slider*) &volume })
    {
        s->removeMouseListener (&rightClickFwd);
        s->setLookAndFeel (nullptr);
    }
    footswitch.removeMouseListener (&rightClickFwd);
}

void EchoflexAudioProcessorEditor::setParamValue (const char* id, float plain)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void EchoflexAudioProcessorEditor::attachVolumeKnob (bool toAge)
{
    aVolume.reset();
    aVolume = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, toAge ? "age" : "volume", volume);
    volumeOnAge = toAge;
}

void EchoflexAudioProcessorEditor::setAgeEdit (bool on)
{
    proc.ageEdit = on;
    if (on)
        setParamValue ("ageOn", 1.0f);          // on entend le reglage pendant qu'on le fait
    // l'attachement du potard ne change qu'une fois le bouton de la souris relache
    // (sinon le geste d'automation du DAW reste ouvert : affichage fige dans Reaper)
    if (! volume.isMouseButtonDown())
        attachVolumeKnob (on);
    showStatus();
}

void EchoflexAudioProcessorEditor::toggleAge()
{
    const bool on = proc.apvts.getRawParameterValue ("ageOn")->load() > 0.5f;
    setParamValue ("ageOn", on ? 0.0f : 1.0f);
    showStatus();
}

void EchoflexAudioProcessorEditor::toggleSync()
{
    const bool on = roundToInt (proc.apvts.getRawParameterValue ("timeSrc")->load()) == 2;
    if (! on)   // la division suit la position actuelle du potard DELAY
        setParamValue ("syncDiv", (float) EchoflexAudioProcessor::syncDivFromKnob (proc.apvts.getRawParameterValue ("delay")->load()));
    setParamValue ("timeSrc", on ? 0.0f : 2.0f);
    proc.currentPreset = -1;
    showStatus();
}

void EchoflexAudioProcessorEditor::stepSyncDivision (int delta)
{
    const int n = EchoflexAudioProcessor::syncNames().size();
    const int d = jlimit (0, n - 1, roundToInt (proc.apvts.getRawParameterValue ("syncDiv")->load()) + delta);
    setParamValue ("syncDiv", (float) d);
    setParamValue ("delay", EchoflexAudioProcessor::knobForSyncDiv (d));
    if (roundToInt (proc.apvts.getRawParameterValue ("timeSrc")->load()) != 2)
        setParamValue ("timeSrc", 2.0f);
    showStatus();
}

void EchoflexAudioProcessorEditor::showStatus()
{
    statusUntil = Time::getMillisecondCounterHiRes() + 2500.0;
}

void EchoflexAudioProcessorEditor::setScaledHeight (int h)
{
    h = jmax ((int) defaultH, h);
    setSize (roundToInt ((float) h * (float) baseW / (float) baseH), h);
}

Rectangle<int> EchoflexAudioProcessorEditor::scaled (Rectangle<float> r) const
{
    const float s = (float) getHeight() / (float) baseH;
    return r.transformedBy (AffineTransform::scale (s)).getSmallestIntegerContainer();
}

float EchoflexAudioProcessorEditor::physicalScale() const
{
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

//==============================================================================
// Caches d'affichage : fond et potards deja a la bonne taille -> plus de
// re-echantillonnage d'une image de 1520 x 1916 a chaque rafraichissement.
//==============================================================================
void EchoflexAudioProcessorEditor::rebuildCaches (float sc)
{
    if (sc <= 0.0f)
        sc = cacheScale > 0.0f ? cacheScale : physicalScale();
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0)
        return;
    if (w == cacheW && h == cacheH && std::abs (sc - cacheScale) < 0.01f && bgScaled.isValid())
        return;

    cacheW = w; cacheH = h; cacheScale = sc;
    const int pw = roundToInt ((float) w * sc), ph = roundToInt ((float) h * sc);
    bgScaled = (pw >= background.getWidth()) ? background
                                             : background.rescaled (pw, ph, Graphics::highResamplingQuality);

    const float s = (float) h / (float) baseH;
    lnf.prepareScaled (roundToInt (knobR * knobStripHalf * 2.0f * s * sc));
    repaint();
}

void EchoflexAudioProcessorEditor::resized()
{
    const float half = knobR * knobStripHalf;
    auto place = [&] (Slider& s, Point<float> c)
    {
        s.setBounds (scaled ({ c.x - half, c.y - half, half * 2.0f, half * 2.0f }));
    };
    place (volume,  knobVolume);
    place (sustain, knobSustain);
    place (delay,   knobDelay);

    footswitch.setBounds (scaled ({ footPos.x - footR, footPos.y - footR, footR * 2.0f, footR * 2.0f }));
    tapArea.setBounds (scaled (tapRect));
    syncArea.setBounds (scaled (syncRect));

    // anti-gel : jamais de setSize ici (les limites du constrainer suffisent) ;
    // pendant un redimensionnement a la souris, on attend que la taille se stabilise
    resizeSettle = 4;
}

//==============================================================================
// Ligne d'etat
//==============================================================================
String EchoflexAudioProcessorEditor::buildStatusLine() const
{
    auto v = [this] (const char* id) { return String (proc.apvts.getRawParameterValue (id)->load(), 1); };
    const int ms = roundToInt (proc.delayMsDisplay.load());

    // MIDI Learn : attente d'un CC, puis « <Commande> = CC n » pendant 2 s
    const int lt = proc.learnTarget.load();
    if (lt >= 0)
        return "MIDI LEARN: move a control for " + String (EchoflexAudioProcessor::learnInfo (lt).name);
    if (Time::getMillisecondCounterHiRes() < learnMsgUntil)
        return learnMsg;

    if (proc.ageEdit.load())
        return "AGE " + v ("age") + "  (push VOLUME to lock)";

    return "DELAY " + String (ms) + " ms   SUSTAIN " + v ("sustain") + "   VOLUME " + v ("volume");
}

String EchoflexAudioProcessorEditor::buildPresetLine() const
{
    const bool ageOn = proc.apvts.getRawParameterValue ("ageOn")->load() > 0.5f;
    const bool on    = proc.apvts.getRawParameterValue ("active")->load() > 0.5f;
    const int  src   = roundToInt (proc.apvts.getRawParameterValue ("timeSrc")->load());

    String s = ageOn ? "AGE " + String (proc.apvts.getRawParameterValue ("age")->load(), 1) : String ("CLEAR TAPE");
    if (src == 1) s << " / TAP";
    else if (src == 2)
    {
        s << " / SYNC " << EchoflexAudioProcessor::syncNames()[roundToInt (proc.apvts.getRawParameterValue ("syncDiv")->load())];
        const float bpm = proc.hostBpm.load();
        if (bpm > 1.0f) s << " @ " << String (bpm, 1) << " BPM";
        else            s << " (no host tempo)";
    }
    if (! on) s << " / BYPASS";

    const int cp = proc.currentPreset.load();
    if (cp >= 0 && cp < EchoflexAudioProcessor::getNumFactoryPresets())
        s << "  |  P" << (cp + 1) << " " << EchoflexAudioProcessor::getFactoryPreset (cp).name;
    else if (cp > 100)
        s << "  |  USER " << (cp - 100);

    const int cc = proc.lastMidiCC.load();
    if (cc >= 0)       s << "  |  CC" << cc << "=" << proc.lastMidiValue.load();
    else if (cc == -2) s << "  |  PC " << proc.lastMidiProgram.load();

    return s;
}

//==============================================================================
// Anti-gel de la fenetre (Windows) : JUCE 8 dessine par defaut avec Direct2D (carte graphique).
// Avec certains pilotes (Intel notamment) la fenetre du plugin cesse de se rafraichir alors que
// le DAW continue de tourner. Le moteur logiciel est stable partout et largement assez rapide ici.
//
// Anti-gel v2 (Studio Wiga) :
//  1. moteur de rendu logiciel force sur Windows, applique a chaque nouvelle fenetre hote (peer)
//     et reverifie toutes les secondes, jamais en boucle ;
//  2. aucun calcul lourd dans paint() : les caches d'images sont reconstruits par le timer seulement,
//     apres stabilisation de la taille ou quand l'echelle d'affichage change (au plus toutes les 250 ms) ;
//  3. chien de garde : si un redessin demande n'est pas arrive au bout de 1,5 s alors que la fenetre est
//     visible, le moteur de rendu est reapplique et toute la fenetre est redessinee ;
//  4. cote audio : aucun appel a l'hote depuis le thread audio (voir PluginProcessor).
void EchoflexAudioProcessorEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer != lastPeer) { lastPeer = peer; repaint(); }
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw)
            peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void EchoflexAudioProcessorEditor::parentHierarchyChanged()
{
    useSoftwareRenderer();
}

void EchoflexAudioProcessorEditor::visibilityChanged()
{
    useSoftwareRenderer();
    if (isShowing())
        repaint();
}

void EchoflexAudioProcessorEditor::askRepaint (Rectangle<int> r)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
    repaint (r);
}

//==============================================================================
void EchoflexAudioProcessorEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const double dt = jlimit (0.0, 0.1, (now - lastTime) * 0.001);
    lastTime = now;

    // au cas ou la fenetre a ete recreee par l'hote (changement d'ecran, d'echelle...) : au plus 1 fois par seconde
    if (getPeer() != lastPeer || now - lastRendererCheckMs > 1000.0) { lastRendererCheckMs = now; useSoftwareRenderer(); }

    // caches : reconstruits ici seulement (jamais dans paint)
    if (resizeSettle > 0 && --resizeSettle == 0)
        rebuildCaches();
    else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
    {
        lastScaleCheckMs = now;
        const float sc = physicalScale();
        if (std::abs (sc - cacheScale) > 0.01f)
            rebuildCaches (sc);
    }

    // chien de garde : un redessin demande n'arrive pas -> on relance le moteur de rendu et on redessine tout
    if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
    {
        repaintAskedMs = 0.0;
        lastPeer = nullptr;
        useSoftwareRenderer();
        repaint();
    }

    volume.checkLongPress (now);

    if (volumeOnAge != proc.ageEdit.load() && ! volume.isMouseButtonDown())
        attachVolumeKnob (proc.ageEdit.load());

    const int mc = proc.midiCounter.load();
    if (mc != lastMidiCount) { lastMidiCount = mc; midiFlash = 8; }
    if (midiFlash > 0) --midiFlash;

    // MIDI Learn : la ligne d'etat reste affichee pendant l'attente ; message 2 s apres l'affectation
    const int learned = proc.lastLearned.exchange (-1);
    if (learned >= 0)
    {
        learnMsg = String (EchoflexAudioProcessor::learnInfo (learned).name) + " = CC " + String (proc.ccNum[learned].load());
        learnMsgUntil = now + 2000.0;
        statusUntil = jmax (statusUntil, learnMsgUntil);
    }
    if (proc.learnTarget.load() >= 0)
        showStatus();

    const int tc = proc.tapCounter.load();
    if (tc != lastTapCount) { lastTapCount = tc; tapFlash = 1.0f; showStatus(); }
    tapFlash = jmax (0.0f, tapFlash - (float) dt * 5.0f);

    // La ligne d'etat n'apparait que lorsqu'un reglage bouge, reste 2,5 s puis s'efface.
    {
        const char* ids[4] = { "delay", "sustain", "volume", "age" };
        for (int i = 0; i < 4; ++i)
        {
            const float v = proc.apvts.getRawParameterValue (ids[i])->load();
            if (std::abs (v - lastKnobs[i]) > 1.0e-4f) { lastKnobs[i] = v; showStatus(); }
        }
        const float ao = proc.apvts.getRawParameterValue ("ageOn")->load() > 0.5f ? 1.0f : 0.0f;
        if (std::abs (ao - lastAgeOn) > 0.5f) { lastAgeOn = ao; showStatus(); }
        const float ts = proc.apvts.getRawParameterValue ("timeSrc")->load();
        const float sd = proc.apvts.getRawParameterValue ("syncDiv")->load();
        if (std::abs (ts - lastTimeSrc) > 0.5f || std::abs (sd - lastSyncDiv) > 0.5f)
        { lastTimeSrc = ts; lastSyncDiv = sd; showStatus(); }

        for (auto* s : { (Slider*) &sustain, (Slider*) &delay, (Slider*) &volume })
            if (s->isMouseButtonDown())
                showStatus();
        if (proc.ageEdit.load())
            showStatus();

        const float target = now < statusUntil ? 1.0f : 0.0f;
        statusAlpha += (target - statusAlpha) * (target > statusAlpha ? 0.45f : 0.12f);
        if (statusAlpha < 0.005f) statusAlpha = 0.0f;
        if (statusAlpha > 0.995f) statusAlpha = 1.0f;
    }

    // LED AGE : allumee si AGE actif, clignote pendant le reglage (et se remplit pendant l'appui long)
    blinkPhase += dt;
    const bool ageOn = proc.apvts.getRawParameterValue ("ageOn")->load() > 0.5f;
    float ageTarget = ageOn ? 1.0f : 0.0f;
    if (proc.ageEdit.load())
        ageTarget = std::fmod (blinkPhase, 0.4) < 0.2 ? 1.0f : 0.08f;
    else if (volume.isHolding() && now - volume.holdStart() > 350.0)
        ageTarget = (float) jlimit (0.0, 1.0, (now - volume.holdStart()) / longPressMs) * (std::fmod (blinkPhase, 0.2) < 0.1 ? 1.0f : 0.5f);
    ageLedLevel += (ageTarget - ageLedLevel) * 0.6f;
    if (std::abs (ageTarget - ageLedLevel) < 0.002f) ageLedLevel = ageTarget;

    // LED SYNC (verte) : eteinte hors synchro ; en synchro, flash a chaque division du tempo
    {
        const bool syncOn = roundToInt (proc.apvts.getRawParameterValue ("timeSrc")->load()) == 2;
        float target = 0.0f;
        if (syncOn)
        {
            const double bpm = proc.hostBpm.load();
            const double beats = EchoflexAudioProcessor::syncBeats (roundToInt (proc.apvts.getRawParameterValue ("syncDiv")->load()));
            double ph;
            if (bpm > 1.0 && proc.hostPlaying.load())
            {
                const double ppq = proc.hostPpq.load() + (now - proc.hostPpqTime.load()) / 60000.0 * bpm;
                ph = std::fmod (ppq / beats, 1.0);
                if (ph < 0.0) ph += 1.0;
            }
            else
            {
                const double periodMs = jmax (65.0, (double) proc.delayMsDisplay.load());
                freePhase = std::fmod (freePhase + dt * 1000.0 / periodMs, 1.0);
                ph = freePhase;
            }
            target = bpm > 1.0 ? 0.30f + 0.70f * (float) std::exp (-ph * 9.0) : 0.22f;
        }
        syncLedLevel += (target - syncLedLevel) * (target > syncLedLevel ? 0.85f : 0.5f);
        if (std::abs (target - syncLedLevel) < 0.002f) syncLedLevel = target;
    }

    // LED rouge : allumage / extinction en fondu, lueur qui respire avec les repetitions
    ledLevel += ((footswitch.isOn() ? 1.0f : 0.0f) - ledLevel) * (footswitch.isOn() ? 0.45f : 0.28f);
    if (ledLevel < 0.003f) ledLevel = 0.0f;
    if (ledLevel > 0.997f) ledLevel = 1.0f;
    const float echoNow = jlimit (0.0f, 1.0f, proc.echoLevel.load() * 2.5f);
    echoGlow += (echoNow - echoGlow) * (echoNow > echoGlow ? 0.5f : 0.12f);
    ledFlicker = ledLevel > 0.0f ? 0.965f + 0.035f * rng.nextFloat() : 1.0f;

    // --- On ne redessine que ce qui a change (zones separees, pas une grande union) ---
    const float ledNow = ledLevel * ledFlicker * (0.86f + 0.22f * echoGlow);
    if (std::abs (ledNow - drawnLed) > 0.002f)
    {
        drawnLed = ledNow;
        askRepaint (scaled ({ ledPos.x - 52.0f, ledPos.y - 52.0f, 104.0f, 104.0f }));
    }
    if (std::abs (ageLedLevel - drawnAge) > 0.002f)
    {
        drawnAge = ageLedLevel;
        askRepaint (scaled ({ agePos.x - 26.0f, agePos.y - 26.0f, 52.0f, 52.0f }));
    }
    if (std::abs (syncLedLevel - drawnSync) > 0.002f)
    {
        drawnSync = syncLedLevel;
        askRepaint (scaled ({ syncPos.x - 26.0f, syncPos.y - 26.0f, 52.0f, 52.0f }));
    }
    const float tapNow = jmax (tapFlash, tapArea.isMouseOver() ? 0.25f : 0.0f);
    if (std::abs (tapNow - drawnTap) > 0.002f)
    {
        drawnTap = tapNow;
        askRepaint (scaled (tapRect.expanded (12.0f)));
    }

    const auto newStatus = buildStatusLine();
    const auto newPreset = buildPresetLine();
    const bool textChanged = newStatus != lastStatus || newPreset != lastPreset;
    lastStatus = newStatus;
    lastPreset = newPreset;
    if (std::abs (statusAlpha - drawnStatus) > 0.002f || (statusAlpha > 0.0f && (textChanged || midiFlash > 0)))
    {
        drawnStatus = statusAlpha;
        askRepaint (scaled (statusRect.expanded (4.0f)));
    }
}

//==============================================================================
void EchoflexAudioProcessorEditor::drawLeds (Graphics& g)
{
    // Lentille allumee dessinee dans la bague chromee de la photo ; eteinte = la photo telle quelle.
    auto lens = [&g] (Point<float> c, float r, float level, float glow, Colour core, Colour mid, Colour deep, float bloomR)
    {
        if (level <= 0.003f)
            return;

        ColourGradient bloom (mid.withAlpha (jmin (1.0f, 0.62f * level * glow)), c.x, c.y,
                              mid.withAlpha (0.0f), c.x + bloomR, c.y, true);
        bloom.addColour (0.35, mid.withAlpha (jmin (1.0f, 0.16f * level * glow)));
        g.setGradientFill (bloom);
        g.fillEllipse (c.x - bloomR, c.y - bloomR, bloomR * 2.0f, bloomR * 2.0f);

        ColourGradient body (core.withAlpha (level), c.x - r * 0.10f, c.y - r * 0.12f,
                             deep.withAlpha (level), c.x + r * 1.05f, c.y + r * 0.2f, true);
        body.addColour (0.30, core.interpolatedWith (mid, 0.55f).withAlpha (level));
        body.addColour (0.62, mid.withAlpha (level));
        g.setGradientFill (body);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

        g.setColour (deep.darker (0.6f).withAlpha (0.55f * level));
        g.drawEllipse (c.x - r + 0.6f, c.y - r + 0.6f, r * 2.0f - 1.2f, r * 2.0f - 1.2f, r * 0.12f);

        ColourGradient sp (Colours::white.withAlpha (0.75f * level), c.x - r * 0.30f, c.y - r * 0.42f,
                           Colours::white.withAlpha (0.0f), c.x - r * 0.30f + r * 0.32f, c.y - r * 0.42f, true);
        g.setGradientFill (sp);
        g.fillEllipse (c.x - r * 0.62f, c.y - r * 0.70f, r * 0.64f, r * 0.52f);
    };

    const float glow = ledFlicker * (0.86f + 0.22f * echoGlow);
    lens (ledPos,  ledR,  ledLevel,     glow, redCore, redMid, redDeep, 50.0f);
    lens (agePos,  ageR,  ageLedLevel,  1.0f, ambCore, ambMid, ambDeep, 24.0f);
    lens (syncPos, syncR, syncLedLevel, 1.0f, grnCore, grnMid, grnDeep, 24.0f);
}

void EchoflexAudioProcessorEditor::drawTapFlash (Graphics& g)
{
    const float a = jmax (tapFlash, tapArea.isMouseOver() ? 0.25f : 0.0f);
    if (a <= 0.0f)
        return;

    const Point<float> jackC { 86.0f, 793.0f };
    ColourGradient gl (Colours::white.withAlpha (0.55f * a), jackC.x, jackC.y,
                       Colours::white.withAlpha (0.0f), jackC.x + 26.0f, jackC.y, true);
    g.setGradientFill (gl);
    g.fillEllipse (jackC.x - 26.0f, jackC.y - 26.0f, 52.0f, 52.0f);

    ColourGradient lb (Colour (0xffff3a24).withAlpha (0.30f * tapFlash), 137.0f, 798.0f,
                       Colour (0xffff3a24).withAlpha (0.0f), 137.0f + 24.0f, 798.0f, true);
    g.setGradientFill (lb);
    g.fillEllipse (137.0f - 24.0f, 798.0f - 30.0f, 48.0f, 60.0f);
}

void EchoflexAudioProcessorEditor::drawStatus (Graphics& g)
{
    const float a = statusAlpha;
    if (a <= 0.0f)
        return;

    const Colour accent = proc.ageEdit.load() ? ambMid : Colour (0xffd8dde2);

    g.setColour (Colours::black.withAlpha (0.78f * a));
    g.fillRoundedRectangle (statusRect, 8.0f);
    g.setColour (accent.withAlpha (0.45f * a));
    g.drawRoundedRectangle (statusRect.reduced (0.75f), 8.0f, 1.5f);

    g.setColour (Colours::white.withAlpha (a));
    g.setFont (font (18.5f, true));
    g.drawFittedText (lastStatus, statusRect.withHeight (28.0f).translated (0.0f, 3.0f).reduced (6.0f, 0.0f).toNearestInt(),
                      Justification::centred, 1, 0.75f);

    g.setColour ((midiFlash > 0 || tapFlash > 0.0f ? Colour (0xffff6a50) : accent).withMultipliedAlpha (a));
    g.setFont (font (14.5f, true));
    g.drawFittedText (lastPreset, statusRect.withTrimmedTop (28.0f).withTrimmedBottom (4.0f).reduced (6.0f, 0.0f).toNearestInt(),
                      Justification::centred, 1, 0.8f);
}

void EchoflexAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colours::black);
    // anti-gel : aucun rescale ici, on dessine le cache existant (ou l'original en attendant le timer)
    const Image& bg = bgScaled.isValid() ? bgScaled : background;
    g.setImageResamplingQuality (bg.getWidth() == background.getWidth() ? Graphics::highResamplingQuality
                                                                         : Graphics::lowResamplingQuality);
    if (bg.isValid())
        g.drawImage (bg, getLocalBounds().toFloat());

    const float s = (float) getHeight() / (float) baseH;
    g.addTransform (AffineTransform::scale (s));

    drawLeds (g);
    drawTapFlash (g);
    drawStatus (g);
}

//==============================================================================
void EchoflexAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showContextMenu();
}

void EchoflexAudioProcessorEditor::showSyncMenu()
{
    auto raw = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const int src = roundToInt (raw ("timeSrc"));
    PopupMenu m;
    addLearnItems (m, { EchoflexAudioProcessor::lSync });
    m.addSectionHeader ("Sync delay to DAW tempo");
    m.addItem (1, "Sync on", true, src == 2);
    m.addSeparator();
    const auto names = EchoflexAudioProcessor::syncNames();
    for (int i = 0; i < names.size(); ++i)
        m.addItem (10 + i, names[i], true, src == 2 && roundToInt (raw ("syncDiv")) == i);

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&syncArea),
        [safe = Component::SafePointer<EchoflexAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            if (safe->handleLearnResult (r)) return;
            if (r == 1) safe->toggleSync();
            else
            {
                safe->setParamValue ("syncDiv", (float) (r - 10));
                safe->setParamValue ("delay", EchoflexAudioProcessor::knobForSyncDiv (r - 10));
                safe->setParamValue ("timeSrc", 2.0f);
                safe->showStatus();
            }
        });
}

//==============================================================================
// MIDI Learn (menus) : 1000 + n = learn, 1100 = annuler, 1101 = numeros par defaut, 1200 + n = effacer
//==============================================================================
static String ccLabel (int cc)
{
    return cc >= 0 ? "CC " + String (cc) : String (CharPointer_UTF8 ("\xe2\x80\x94"));
}

void EchoflexAudioProcessorEditor::addLearnItems (PopupMenu& m, const Array<int>& learnItems)
{
    if (learnItems.isEmpty())
        return;
    for (int i : learnItems)
    {
        const String name = EchoflexAudioProcessor::learnInfo (i).name;
        const int cc = proc.ccNum[i].load();
        m.addItem (1000 + i, "MIDI Learn: " + name + " (" + ccLabel (cc) + ")", true, proc.learnTarget.load() == i);
        m.addItem (1200 + i, "Clear MIDI: " + name, cc >= 0);
    }
    if (proc.learnTarget.load() >= 0)
        m.addItem (1100, "Cancel MIDI learn");
    m.addSeparator();
}

bool EchoflexAudioProcessorEditor::handleLearnResult (int r)
{
    using P = EchoflexAudioProcessor;
    if (r >= 1000 && r < 1000 + P::numLearn)      proc.learnTarget.store (r - 1000);
    else if (r == 1100)                           proc.learnTarget.store (-1);
    else if (r == 1101)
    {
        proc.learnTarget.store (-1);
        proc.resetMidiDefaults();
        learnMsg = "MIDI: default CC 20 - 28";
        learnMsgUntil = Time::getMillisecondCounterHiRes() + 2000.0;
    }
    else if (r >= 1200 && r < 1200 + P::numLearn)
    {
        proc.ccNum[r - 1200].store (-1);
        learnMsg = String (P::learnInfo (r - 1200).name) + " = " + ccLabel (-1);
        learnMsgUntil = Time::getMillisecondCounterHiRes() + 2000.0;
    }
    else
        return false;

    showStatus();
    return true;
}

void EchoflexAudioProcessorEditor::showContextMenu (Array<int> learnItems)
{
    auto raw = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    PopupMenu menu;
    addLearnItems (menu, learnItems);

    PopupMenu sizes;
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    sizes.addItem (1, "Default (" + String (defaultW) + " x " + String (defaultH) + ")", true, getHeight() == defaultH);
    sizes.addItem (2, "75 %",  true, getHeight() == roundToInt (baseH * 0.75f));
    sizes.addItem (3, "100 %", true, getHeight() == baseH);
    menu.addSubMenu ("Window size", sizes);
    menu.addSeparator();

    PopupMenu factory;
    for (int i = 0; i < EchoflexAudioProcessor::getNumFactoryPresets(); ++i)
        factory.addItem (100 + i, String (i + 1) + ". " + EchoflexAudioProcessor::getFactoryPreset (i).name,
                         true, proc.currentPreset.load() == i);
    menu.addSubMenu ("Factory presets", factory);

    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists,
                      proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    menu.addSubMenu ("Load user preset", load);
    menu.addSubMenu ("Save user preset", save);
    menu.addSeparator();

    PopupMenu age;
    age.addItem (50, "Age on (click VOLUME)", true, raw ("ageOn") > 0.5f);
    age.addItem (51, proc.ageEdit.load() ? "Lock Age setting" : "Set Age (hold VOLUME 2 s)");
    menu.addSubMenu ("Age", age);

    const int src = roundToInt (raw ("timeSrc"));
    PopupMenu timeMenu;
    timeMenu.addItem (60, "DELAY knob (65 - 750 ms)", true, src == 0);
    timeMenu.addItem (61, "Tap tempo (" + String (roundToInt (raw ("tapMs"))) + " ms) - click TAP", true, src == 1);
    PopupMenu sync;
    const auto names = EchoflexAudioProcessor::syncNames();
    for (int i = 0; i < names.size(); ++i)
        sync.addItem (70 + i, names[i], true, src == 2 && roundToInt (raw ("syncDiv")) == i);
    timeMenu.addSubMenu ("Sync to DAW tempo (click DELAY / SYNC)", sync, true, nullptr, src == 2);
    menu.addSubMenu ("Delay time", timeMenu);

    PopupMenu settings;
    settings.addItem (40, "True relay bypass",       true, raw ("bypassMode") < 0.5f);
    settings.addItem (41, "Trails bypass",           true, raw ("bypassMode") > 0.5f);
    settings.addSeparator();
    settings.addItem (42, "Dry + echo",              true, raw ("mix") < 0.5f);
    settings.addItem (43, "100% wet (kill dry)",     true, raw ("mix") > 0.5f);
    settings.addSeparator();
    settings.addItem (44, "Mono (EP103 default)",    true, raw ("channels") < 0.5f);
    settings.addItem (45, "Stereo",                  true, raw ("channels") > 0.5f);
    menu.addSubMenu ("Startup settings", settings);
    menu.addSeparator();

    PopupMenu midi;
    for (int i = 0; i < EchoflexAudioProcessor::numLearn; ++i)
        midi.addItem (1000 + i, "Learn: " + String (EchoflexAudioProcessor::learnInfo (i).name)
                                  + " (" + ccLabel (proc.ccNum[i].load()) + ")", true, proc.learnTarget.load() == i);
    if (proc.learnTarget.load() >= 0)
        midi.addItem (1100, "Cancel MIDI learn");
    midi.addItem (1101, "Reset MIDI to defaults (CC 20 - 28)");
    midi.addSeparator();
    midi.addItem (908, "Program Change 1-10  Factory presets",      false);
    menu.addSubMenu ("MIDI", midi);

    menu.addSeparator();
    menu.addItem (999, "Echoflex delay v1.4.0 - STUDIO WIGA", false);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this),
        [safe = Component::SafePointer<EchoflexAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0)
                return;

            auto& ed = *safe;
            if (ed.handleLearnResult (r)) return;
            if      (r == 1) ed.setScaledHeight (defaultH);
            else if (r == 2) ed.setScaledHeight (roundToInt (baseH * 0.75f));
            else if (r == 3) ed.setScaledHeight (baseH);
            else if (r == 40 || r == 41) ed.setParamValue ("bypassMode", r == 41 ? 1.0f : 0.0f);
            else if (r == 42 || r == 43) ed.setParamValue ("mix",        r == 43 ? 1.0f : 0.0f);
            else if (r == 44 || r == 45) ed.setParamValue ("channels",   r == 45 ? 1.0f : 0.0f);
            else if (r == 50) ed.toggleAge();
            else if (r == 51) ed.setAgeEdit (! ed.proc.ageEdit.load());
            else if (r == 60) ed.setParamValue ("timeSrc", 0.0f);
            else if (r == 61) ed.setParamValue ("timeSrc", 1.0f);
            else if (r >= 70 && r < 80)
            {
                ed.setParamValue ("syncDiv", (float) (r - 70));
                ed.setParamValue ("delay", EchoflexAudioProcessor::knobForSyncDiv (r - 70));
                ed.setParamValue ("timeSrc", 2.0f);
            }
            else if (r >= 100 && r < 200) ed.proc.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  ed.proc.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  ed.proc.saveUserPreset (r - 300);

            ed.showStatus();
        });
}
