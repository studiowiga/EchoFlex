#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    Font font (float h, bool bold = true)
    {
        return Font (FontOptions (h, bold ? Font::bold : Font::plain));
    }

    // Positions dans le repere de reference 760 x 958 (meme cadrage que Wack'd)
    // (releves sur le visuel ECHOFLEX, mis a l'echelle du projet : 1520 x 1916 = 2 x 760 x 958)
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
    const Rectangle<float> statusRect{ 150.0f, 566.0f, 460.0f, 52.0f };
    const Point<float>     footPos   { 379.1f, 691.6f };
    const float            footR     = 55.0f;
    const Rectangle<float> tapRect   { 66.0f, 760.0f, 92.0f, 76.0f };   // prise TAP + inscription

    const Colour redCore (0xffffd6c8);
    const Colour redMid  (0xffff2616);
    const Colour redDeep (0xff8c0504);
    const Colour ambCore (0xfffff1c8);
    const Colour ambMid  (0xffffa02a);
    const Colour ambDeep (0xffc85a00);

    constexpr double longPressMs = 2000.0;   // comme la pedale : maintenir VOLUME 2 s
}

//==============================================================================
// POTARD
//==============================================================================
void PointerKnobLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                               float pos, float, float, Slider&)
{
    if (! strip.isValid())
        return;

    const int fs = strip.getWidth();
    const int frame = jlimit (0, knobFrames - 1, roundToInt (pos * (float) (knobFrames - 1)));

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (strip, x, y, w, h, 0, frame * fs, fs, fs);
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
    : attachment (p, [this] (float v) { value = v > 0.5f; if (auto* par = getParentComponent()) par->repaint(); })
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
        // bouton enfonce : le capuchon s'assombrit
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
    lastAgeOn = proc.apvts.getRawParameterValue ("ageOn")->load();
    rightClickFwd.callback = [this] { showContextMenu(); };

    auto setupKnob = [this] (Slider& s)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (220);
        s.setLookAndFeel (&lnf);
        s.setMouseCursor (MouseCursor::PointingHandCursor);
        s.addMouseListener (&rightClickFwd, false);
        addAndMakeVisible (s);
    };

    setupKnob (sustain);
    setupKnob (delay);
    setupKnob (volume);
    aSustain = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "sustain", sustain);
    aDelay   = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "delay",   delay);
    attachVolumeKnob (proc.ageEdit.load());

    // Tourner DELAY reprend la main sur le TAP / la synchro (comme la pedale)
    delay.onDragStart = [this] { setParamValue ("timeSrc", 0.0f); proc.currentPreset = -1; };
    delay.onValueChange = [this]
    {
        if (delay.isMouseButtonDown() && proc.apvts.getRawParameterValue ("timeSrc")->load() > 0.5f)
            setParamValue ("timeSrc", 0.0f);
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

    tapArea.onTap = [this] { proc.tap(); };
    tapArea.addMouseListener (&rightClickFwd, false);
    addAndMakeVisible (tapArea);

    // Taille par defaut = taille minimale (impossible de reduire en dessous)
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* cons = getConstrainer())
        cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (defaultW, defaultH);

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
    tapArea.removeMouseListener (&rightClickFwd);
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
    attachVolumeKnob (on);
    showStatus();
    repaint();
}

void EchoflexAudioProcessorEditor::toggleAge()
{
    const bool on = proc.apvts.getRawParameterValue ("ageOn")->load() > 0.5f;
    setParamValue ("ageOn", on ? 0.0f : 1.0f);
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
}

//==============================================================================
// Ligne d'etat
//==============================================================================
String EchoflexAudioProcessorEditor::buildStatusLine() const
{
    auto v = [this] (const char* id) { return String (proc.apvts.getRawParameterValue (id)->load(), 1); };
    const int ms = roundToInt (proc.delayMsDisplay.load());

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
    if      (src == 1) s << " / TAP";
    else if (src == 2) s << " / SYNC " << EchoflexAudioProcessor::syncNames()[roundToInt (proc.apvts.getRawParameterValue ("syncDiv")->load())];
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
void EchoflexAudioProcessorEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const double dt = jlimit (0.0, 0.1, (now - lastTime) * 0.001);
    lastTime = now;

    volume.checkLongPress (now);

    // l'etat AGE a pu changer ailleurs (automation, MIDI, preset)
    if (volumeOnAge != proc.ageEdit.load())
        attachVolumeKnob (proc.ageEdit.load());

    const int mc = proc.midiCounter.load();
    if (mc != lastMidiCount) { lastMidiCount = mc; midiFlash = 8; }
    if (midiFlash > 0) --midiFlash;

    const int tc = proc.tapCounter.load();
    if (tc != lastTapCount) { lastTapCount = tc; tapFlash = 1.0f; showStatus(); }
    tapFlash = jmax (0.0f, tapFlash - (float) dt * 5.0f);

    // La ligne d'etat n'apparait que lorsqu'un reglage bouge, reste 2,5 s puis s'efface.
    {
        const char* ids[4] = { "delay", "sustain", "volume", "age" };
        for (int i = 0; i < 4; ++i)
        {
            const float v = proc.apvts.getRawParameterValue (ids[i])->load();
            if (std::abs (v - lastKnobs[i]) > 1.0e-4f)
            {
                lastKnobs[i] = v;
                showStatus();
            }
        }
        const float ao = proc.apvts.getRawParameterValue ("ageOn")->load() > 0.5f ? 1.0f : 0.0f;
        if (std::abs (ao - lastAgeOn) > 0.5f) { lastAgeOn = ao; showStatus(); }

        for (auto* s : { (Slider*) &sustain, (Slider*) &delay, (Slider*) &volume })
            if (s->isMouseButtonDown())
                showStatus();
        if (proc.ageEdit.load())
            showStatus();

        const float target = now < statusUntil ? 1.0f : 0.0f;
        statusAlpha += (target - statusAlpha) * (target > statusAlpha ? 0.45f : 0.12f);
        if (statusAlpha < 0.005f) statusAlpha = 0.0f;
    }

    lastStatus = buildStatusLine();
    lastPreset = buildPresetLine();

    // LED AGE : allumee si AGE actif, clignote pendant le reglage (et pendant l'appui long)
    blinkPhase += dt;
    const bool ageOn = proc.apvts.getRawParameterValue ("ageOn")->load() > 0.5f;
    float ageTarget = ageOn ? 1.0f : 0.0f;
    if (proc.ageEdit.load())
        ageTarget = std::fmod (blinkPhase, 0.4) < 0.2 ? 1.0f : 0.08f;
    else if (volume.isHolding() && now - volume.holdStart() > 350.0)
        ageTarget = (float) jlimit (0.0, 1.0, (now - volume.holdStart()) / longPressMs) * (std::fmod (blinkPhase, 0.2) < 0.1 ? 1.0f : 0.5f);
    ageLedLevel += (ageTarget - ageLedLevel) * 0.6f;

    ledFlicker = 0.965f + 0.035f * rng.nextFloat();

    // LED rouge : allumage / extinction en fondu, lueur qui respire avec les repetitions
    ledLevel += ((footswitch.isOn() ? 1.0f : 0.0f) - ledLevel) * (footswitch.isOn() ? 0.45f : 0.28f);
    if (ledLevel < 0.003f) ledLevel = 0.0f;
    const float echoNow = jlimit (0.0f, 1.0f, proc.echoLevel.load() * 2.5f);
    echoGlow += (echoNow - echoGlow) * (echoNow > echoGlow ? 0.5f : 0.12f);

    repaint (scaled ({ ledPos.x - 50.0f, ledPos.y - 50.0f, 100.0f, 100.0f })
                 .getUnion (scaled ({ agePos.x - 30.0f, agePos.y - 30.0f, 60.0f, 60.0f }))
                 .getUnion (scaled (statusRect.expanded (4.0f)))
                 .getUnion (scaled (tapRect.expanded (12.0f))));
}

//==============================================================================
void EchoflexAudioProcessorEditor::drawLeds (Graphics& g)
{
    // Lentille allumee dessinee dans la bague chromee de la photo ; eteinte = la photo telle quelle.
    auto lens = [&g] (Point<float> c, float r, float level, float glow, Colour core, Colour mid, Colour deep, float bloomR)
    {
        if (level <= 0.003f)
            return;

        // halo sur le panneau et la bague
        ColourGradient bloom (mid.withAlpha (0.62f * level * glow), c.x, c.y,
                              mid.withAlpha (0.0f), c.x + bloomR, c.y, true);
        bloom.addColour (0.35, mid.withAlpha (0.16f * level * glow));
        g.setGradientFill (bloom);
        g.fillEllipse (c.x - bloomR, c.y - bloomR, bloomR * 2.0f, bloomR * 2.0f);

        // dome de la lentille : coeur chaud, rouge, bord plus sombre (refraction)
        ColourGradient body (core.withAlpha (level), c.x - r * 0.10f, c.y - r * 0.12f,
                             deep.withAlpha (level), c.x + r * 1.05f, c.y + r * 0.2f, true);
        body.addColour (0.30, core.interpolatedWith (mid, 0.55f).withAlpha (level));
        body.addColour (0.62, mid.withAlpha (level));
        g.setGradientFill (body);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

        // anneau interne plus sombre au bord du dome
        g.setColour (deep.darker (0.6f).withAlpha (0.55f * level));
        g.drawEllipse (c.x - r + 0.6f, c.y - r + 0.6f, r * 2.0f - 1.2f, r * 2.0f - 1.2f, r * 0.12f);

        // reflet du verre (meme position que sur la photo : haut-gauche)
        ColourGradient sp (Colours::white.withAlpha (0.75f * level), c.x - r * 0.30f, c.y - r * 0.42f,
                           Colours::white.withAlpha (0.0f), c.x - r * 0.30f + r * 0.32f, c.y - r * 0.42f, true);
        g.setGradientFill (sp);
        g.fillEllipse (c.x - r * 0.62f, c.y - r * 0.70f, r * 0.64f, r * 0.52f);
    };

    // LED principale (rouge) : effet actif ; respire legerement avec le niveau des echos
    const float glow = ledFlicker * (0.86f + 0.22f * echoGlow);
    lens (ledPos, ledR, ledLevel, glow, redCore, redMid, redDeep, 50.0f);

    // LED AGE (ambre)
    lens (agePos, ageR, ageLedLevel, 1.0f, ambCore, ambMid, ambDeep, 24.0f);
}

void EchoflexAudioProcessorEditor::drawTapFlash (Graphics& g)
{
    if (tapFlash <= 0.0f && ! tapArea.isMouseOver())
        return;

    // reflet sur l'ecrou de la prise TAP + inscription qui s'eclaire a chaque TAP
    const float a = jmax (tapFlash, tapArea.isMouseOver() ? 0.25f : 0.0f);
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
    g.drawText (lastStatus.isEmpty() ? buildStatusLine() : lastStatus,
                statusRect.withHeight (28.0f).translated (0.0f, 3.0f), Justification::centred);

    g.setColour ((midiFlash > 0 || tapFlash > 0.0f ? Colour (0xffff6a50) : accent).withMultipliedAlpha (a));
    g.setFont (font (14.5f, true));
    g.drawText (lastPreset.isEmpty() ? buildPresetLine() : lastPreset,
                statusRect.withTrimmedTop (28.0f).withTrimmedBottom (4.0f), Justification::centred);
}

void EchoflexAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (Colours::black);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());

    const float s = (float) getHeight() / (float) baseH;
    g.addTransform (AffineTransform::scale (s));

    // Prises jack, vis, titre ECHOFLEX et logo Studio Wiga font partie de l'image de fond.
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

void EchoflexAudioProcessorEditor::showContextMenu()
{
    auto raw = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    PopupMenu menu;

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
    timeMenu.addSubMenu ("Host tempo sync", sync, true, nullptr, src == 2);
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
    midi.addItem (900, "CC 20  Delay",                              false);
    midi.addItem (901, "CC 21  Sustain",                            false);
    midi.addItem (902, "CC 22  Volume",                             false);
    midi.addItem (903, "CC 23  Age amount",                         false);
    midi.addItem (904, "CC 24  Age (0-63 clear / 64+ age)",         false);
    midi.addItem (905, "CC 25  Tap (each press)",                   false);
    midi.addItem (906, "CC 26  Footswitch (64+ ON / 0-63 OFF)",     false);
    midi.addItem (907, "CC 27  Footswitch toggle (each press)",     false);
    midi.addItem (908, "Program Change 1-10  Factory presets",      false);
    menu.addSubMenu ("MIDI map", midi);

    menu.addSeparator();
    menu.addItem (999, "ECHOFLEX delay v1.1.0 - STUDIO WIGA", false);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this),
        [safe = Component::SafePointer<EchoflexAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0)
                return;

            auto& ed = *safe;
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
                ed.setParamValue ("timeSrc", 2.0f);
            }
            else if (r >= 100 && r < 200) ed.proc.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  ed.proc.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  ed.proc.saveUserPreset (r - 300);

            ed.showStatus();
            ed.repaint();
        });
}
