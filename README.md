# Lard

Plugin audio (VST3) de densification : compression, saturation et plafond dans deux gros boutons.

| Réglage | Rôle |
|---|---|
| **Fatness** | Compression puis saturation suréchantillonnée x4. Plus on tourne, plus c'est dense et fort (jusqu'à environ +7 dB). |
| **Color** | Sombre ↔ brillant (bascule d'égalisation autour de 700 Hz avant la saturation). |
| **Gain** | Volume de sortie, de -24 à +12 dB. |
| **Mix** | Dosage sec / traité (compression parallèle). |

Latence : 32 échantillons, compensée automatiquement par le DAW.

## Installer (Windows)

Copier `Lard.vst3` dans `C:\Program Files\Common Files\VST3\`, puis relancer l'analyse des plugins dans le DAW (Ableton : Préférences → Plug-ins → Réanalyser).

## Compiler

Prérequis : CMake 3.22+ et un compilateur C++17 (Visual Studio 2022 sous Windows). JUCE est téléchargé automatiquement.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Le plugin sort dans `build/Lard_artefacts/Release/VST3/`.

## Tests du moteur audio

```
python3 tools/test_engine.py
```

L'outil `lard_render` applique le moteur à un fichier WAV float32, hors DAW. Le script vérifie la transparence, les crêtes, le repliement, la stabilité et la charge CPU.
