# Lard, Koquille & MAKAR

Trois plugins audio VST3 pour Windows 64 bits :

- **Lard** : densification agressive (compression + saturation) pour kicks, basses, synthés.
- **Koquille** : saturateur à lampes pour colorer finement un bus ou un mix.
- **MAKAR** : saturateur de piste à 5 circuits, de la chaleur discrète à la destruction.

## Lard

Compression, saturation et plafond dans deux gros boutons.

| Réglage | Rôle |
|---|---|
| **Fatness** | Compression puis saturation suréchantillonnée x4. Plus on tourne, plus c'est dense et fort (jusqu'à environ +7 dB). |
| **Color** | Sombre ↔ brillant (bascule d'égalisation autour de 700 Hz avant la saturation). |
| **Gain** | Volume de sortie, de -24 à +12 dB. |
| **Mix** | Dosage sec / traité (compression parallèle). |

Latence : 32 échantillons, compensée automatiquement par le DAW.

### Interface

- Le démon se réveille avec **Fatness** : les yeux s'embrasent, la gueule s'ouvre, les braises montent.
- Sa lueur vacille et flambe en direct avec la compression ; le vu-mètre en haut à droite affiche la réduction de gain.
- **Double-clic** sur un réglage : retour à sa valeur par défaut.
- Fenêtre redimensionnable de 75 % à 200 % par le coin bas-droit ; la taille est gardée avec le projet.

## Koquille

Deux étages de lampes en série, transformateurs, saturation parallèle par bande. Le gain automatique garde le même volume, pour juger la couleur à l'oreille et non au niveau.

| Réglage | Rôle |
|---|---|
| **Input** | Attaque des lampes (±12 dB). Le niveau est compensé : on change le grain, pas le volume. |
| **Pentode** | Lampe symétrique : harmoniques impaires, son plus mordant. |
| **Triode** | Lampe asymétrique : harmoniques paires, son plus chaud. |
| **Density** | Pousse les deux lampes ensemble. |
| **Saturation parallèle** | Sature une seule bande (Grave, Bas-médium, Médium, Aigu) et la rajoute ; caractère Douce ou Agressive. |
| **Calibration** | Sombre / Normal / Brillant (aigu au-dessus de 6 kHz). |
| **Air** | Brillance au-dessus de 12 kHz, jusqu'à environ +5 dB. |
| **Mix** | Dosage sec / traité. |
| **Output** | Volume de sortie (±12 dB). |
| **Gain auto** | Garde le volume identique au signal d'entrée, quels que soient les réglages. |

Latence : 33 échantillons, compensée automatiquement par le DAW.

- L'aiguille **COULEUR** mesure en direct ce que les lampes changent au son.
- Les filaments chauffent avec les réglages et la couleur ; le nautile s'illumine avec Density.
- **Double-clic** sur un potard : valeur par défaut. Fenêtre redimensionnable de 75 % à 200 %.

## MAKAR

Un saturateur à poser sur une piste : on choisit un circuit, on règle l'attaque, on filtre avant et après. Avec **Auto**, le volume reste le même : le Drive change le grain, pas le niveau.

| Circuit | Caractère |
|---|---|
| **01 Bande** | Magnéto : l'aigu sature en premier puis est rabaissé, attaques arrondies, son doux. |
| **02 Transfo** | Transformateur : le grave sature en premier, gras et appuyé. |
| **03 Germanium** | Transistor asymétrique : harmoniques paires et impaires, grain rugueux. |
| **04 Triode** | Lampe polarisée : harmoniques paires, chaud. |
| **05 Pentode** | Lampe symétrique : harmoniques impaires, plus mordant. |

| Réglage | Rôle |
|---|---|
| **Drive** | Attaque du circuit, de -12 à +30 dB. |
| **Punish** | +20 dB d'attaque en plus, pour écraser. |
| **Low Cut** | Coupe-bas avant la saturation (12 dB/oct), de 20 Hz (coupé) à 1 kHz. |
| **Thump** | Bosse de +4 dB dans le grave, à la fréquence du coupe-bas (50 Hz au minimum). |
| **Tone** | Bascule sombre ↔ brillant (±5 dB autour de 800 Hz), avant la saturation. |
| **High Cut** | Coupe-haut après la saturation (6 dB/oct), de 1 kHz à 20 kHz (coupé). |
| **Steep** | Coupe-haut à 30 dB/oct au lieu de 6. |
| **Mix** | Dosage sec / traité. |
| **Output** | Volume de sortie, de -24 à +12 dB. |
| **Auto** | Compense le volume selon le Drive et le circuit. |

Latence : 33 échantillons, compensée automatiquement par le DAW.

- Le vu-mètre **ATTITUDE** montre en direct la force avec laquelle le signal attaque le circuit.
- La courbe **FILTRES** affiche la réponse réelle de Low Cut, Thump, Tone, High Cut et Steep.
- **Double-clic** sur un réglage : valeur par défaut. Fenêtre redimensionnable de 75 % à 200 %.

## Installer (Windows)

1. Décompresser le zip.
2. Copier les dossiers `Lard.vst3`, `Koquille.vst3` et `MAKAR.vst3` dans `C:\Program Files\Common Files\VST3\`.
3. Dans Ableton : Préférences → Plug-ins → activer « Utiliser les dossiers système VST3 », puis Réanalyser.

Les trois plugins apparaissent dans Plug-ins → Rwalaaa.

## Compiler

Prérequis : CMake 3.22+ et un compilateur C++17 (Visual Studio sous Windows). JUCE est téléchargé automatiquement.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Les plugins sortent dans `build/Lard_artefacts/Release/VST3/`, `build/Koquille_artefacts/Release/VST3/` et `build/MAKAR_artefacts/Release/VST3/`.

## Tests des moteurs audio

```
python3 tools/test_engine.py     # Lard
python3 tools/test_koquille.py   # Koquille
python3 tools/test_makar.py      # MAKAR
```

Les outils `lard_render`, `render_koquille` et `render_makar` appliquent chaque moteur à un fichier WAV float32, hors DAW. Les scripts vérifient la transparence, les niveaux, les harmoniques, le repliement, la stabilité et la charge CPU.
