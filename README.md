# ECHOFLEX (Studio Wiga) — VST3

Reproduction en plugin VST3 (Windows 11 et macOS) de la pédale **Dunlop EP103 Echoplex Delay**, la version « boîtier » de l'écho à bande Maestro **Echoplex EP-3** (1970-1991).
Usage personnel. Visuel photo ECHOFLEX (v1.1) mis à l'échelle du projet (1520 × 1916, même cadrage que Wack'd), logo Studio Wiga en noir et blanc.

## Installation via GitHub

1. Déposer à la racine du dépôt : `background.jpg`, `knob_strip.png`, `CMakeLists.txt`, `PluginProcessor.h/.cpp`, `PluginEditor.h/.cpp`, `README.md`, `Montserrat-OFL.txt` et le dossier `.github/workflows/build.yml`.
2. Le push lance la compilation (onglet **Actions**) : Windows (`windows-2022`) et macOS (binaire universel).
3. Télécharger l'artefact :
   - **Windows** : copier `ECHOFLEX.vst3` dans `C:\Program Files\Common Files\VST3\`
   - **macOS** : dézipper puis copier `ECHOFLEX.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`
4. Relancer l'analyse des plugins dans le DAW. Le plugin apparaît comme **ECHOFLEX (Studio Wiga)**.

## Ce que fait la vraie EP103 (documentation)

| Élément | Pédale d'origine | Dans le plugin |
|---|---|---|
| DELAY | Temps de retard 65 → 750 ms | Identique (courbe logarithmique : midi ≈ 221 ms) |
| SUSTAIN | Nombre de répétitions ; 1re moitié douce (1 à 2 répétitions), 2e moitié qui monte « presque exponentiellement », auto-oscillation au maximum | Identique ; l'auto-oscillation est bornée par la saturation de la bande |
| VOLUME | Niveau de l'écho (le son direct reste à l'unité). Peut glisser l'écho derrière le jeu ou rendre les premières répétitions plus fortes | Midi ≈ −7 dB, maximum ≈ +3 dB. Légère interaction avec SUSTAIN, comme sur la pédale |
| AGE (VOLUME poussé) | Clic sur VOLUME : bascule entre le réglage AGE et le son clair d'usine (LED AGE éteinte = son clair). Maintenir VOLUME 2 s : la LED AGE clignote, VOLUME règle alors l'usure ; nouvel appui = verrouillage | Identique : **clic** sur le potard VOLUME = AGE on/off ; **appui maintenu 2 s** = réglage AGE (LED qui clignote, le potard affiche la valeur AGE) ; **clic** = verrouillage |
| Son clair | Bande neuve fraîchement réglée : écho clair, pleurage/scintillement subtils | Wow 0,55 Hz et flutter 7,3/11,7 Hz très légers |
| AGE au maximum | Plus sombre, plus de saturation de bande, plus de modulation (qui paraît aléatoire), « fatigue » de la bande | Filtres fermés jusqu'à ~2,5 kHz, saturation, wow ×15, dérive aléatoire, pertes de niveau, léger souffle qui suit l'écho |
| Préampli | L'écho reçoit la coloration du préampli Echoplex EP101 ; le son direct est intact | Grave +1,5 dB / présence +2,5 dB / saturation douce sur l'écho uniquement |
| Moteur de bande | Tourner DELAY fait glisser la hauteur des répétitions | Inertie du moteur reproduite (~110 ms) |
| TAP | Prise pour MXR M199, jusqu'à 4 s | Clic sur la prise **TAP** (à gauche, en bas) ou CC 25 ; moyenne des 3 derniers intervalles ; tourner DELAY reprend la main |
| Footswitch / LED | True bypass relais, LED rouge | Clic sur le footswitch, LED rouge animée |
| Mode de démarrage | Trails bypass, 100 % wet, mono/stéréo (switch interne) | Clic droit > **Startup settings** |

Sources : manuel Dunlop EP103 (jimdunlop.com), fiches zZounds et Chicago Music Exchange, test Premier Guitar, article Starlite Campbell sur l'EP-3.

## Interface

- Fenêtre ouverte par défaut à **493 px de haut** (391 × 493), impossible de la réduire en dessous, proportions bloquées, jusqu'à 150 % : **mêmes réglages de taille que Wack'd**.
- **Clic droit** : taille (Default / 75 % / 100 %), presets d'usine, presets utilisateur 1 à 9 (charger / sauvegarder), AGE, source du temps de retard (potard / tap / synchro tempo du DAW), réglages de démarrage, table MIDI.
- **Potards** (v1.1) : reconstruits d'après la photo du visuel, 128 positions pré-calculées. L'éclairage du chapeau et de la jupe est relevé sur la photo et reste fixe (lumière en haut à gauche) ; le motif des 47 dents tourne sous cette lumière ; l'aileron (facette éclairée côté lumière, ombre portée sur le chapeau) et le trait blanc sont redessinés à chaque angle.
- **LED rouge** (effet actif) dans la bague chromée de la photo : allumage/extinction en fondu, léger scintillement, la lueur respire avec le niveau des répétitions. **LED AGE** ambre : fondu, clignote pendant le réglage AGE et se remplit pendant l'appui long.
- **Ligne d'état** sous la LED : elle n'apparaît que lorsqu'un réglage bouge (temps réel en ms, Sustain, Volume, état AGE, tap / synchro, preset, MIDI), reste 2,5 s puis s'efface en fondu.
- Logo **ECHOFLEX** en lettres argent biseautées avec ondes concentriques, logo **Studio Wiga** (deux anneaux entrelacés + STUDIO WIGA) en noir et blanc sous « DELAY ».
- Fond noir avec les ondes du logo en transparence autour de la pédale.

## MIDI

| Message | Fonction |
|---|---|
| CC 20 / 21 / 22 | Delay / Sustain / Volume |
| CC 23 | Quantité d'AGE |
| CC 24 | AGE (0-63 son clair, 64-127 AGE) |
| CC 25 | TAP (chaque appui) |
| CC 26 | Footswitch (≥ 64 ON, < 64 OFF) |
| CC 27 | Footswitch bascule (chaque appui) |
| Program Change 1-10 | Presets d'usine |

Tous les paramètres sont aussi automatisables et assignables en MIDI-learn dans le DAW.

## Presets d'usine

1. Noon (Manual Start) — tout à midi, le réglage de départ du manuel · 2. Fifties Slapback · 3. Rockabilly Double · 4. Surf Echo · 5. Hot Lead 400 · 6. Fresh Tape Wash · 7. Worn Cartridge · 8. Dark Ambience · 9. Dub Spiral · 10. Runaway Oscillation

## Placement dans Reaper

Comme les autres plugins Studio Wiga : **avant** Genome. Le mode Mono (par défaut, comme la pédale) envoie le même écho à gauche et à droite, sans opposition de phase : aucune annulation quand Genome additionne la piste en mono.
