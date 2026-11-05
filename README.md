# Lard & Koquille

Deux plugins audio VST3 pour Windows 64 bits :

- **Lard** : densification agressive (compression + saturation) pour kicks, basses, synthés.
- **Koquille** : saturateur à lampes pour colorer finement un bus ou un mix.

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

## Installer (Windows)

1. Décompresser le zip.
2. Copier les dossiers `Lard.vst3` et `Koquille.vst3` dans `C:\Program Files\Common Files\VST3\`.
3. Dans Ableton : Préférences → Plug-ins → activer « Utiliser les dossiers système VST3 », puis Réanalyser.

Les deux plugins apparaissent dans Plug-ins → Rwalaaa.

## Compiler

Prérequis : CMake 3.22+ et un compilateur C++17 (Visual Studio sous Windows). JUCE est téléchargé automatiquement.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Les plugins sortent dans `build/Lard_artefacts/Release/VST3/` et `build/Koquille_artefacts/Release/VST3/`.

## Tests des moteurs audio

```
python3 tools/test_engine.py     # Lard
python3 tools/test_koquille.py   # Koquille
```

Les outils `lard_render` et `render_koquille` appliquent chaque moteur à un fichier WAV float32, hors DAW. Les scripts vérifient la transparence, les niveaux, les harmoniques, le repliement, la stabilité et la charge CPU.
