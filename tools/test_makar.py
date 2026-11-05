"""Tests hors-ligne du moteur MAKAR."""
import os, sys, itertools
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from audiotest import (SR, db, rms, Renderer, Checker, harmonics, worst_alias,
                       log_sweep, response_at, pink_noise)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.environ.get("MAKAR_RENDER") or next(
    (p for p in [os.path.join(ROOT, "build", "render_makar"),
                 os.path.join(ROOT, "build", "Release", "render_makar.exe"),
                 os.path.join(ROOT, "build", "render_makar.exe")] if os.path.exists(p)),
    os.path.join(ROOT, "build", "render_makar"))
render = Renderer(EXE, os.path.join(ROOT, "build", "test_audio_makar"))
check = Checker()
LAT = 33
STYLES = ["Bande", "Transfo", "Germanium", "Triode", "Pentode"]

t = np.arange(SR * 4) / SR
sine1k = 0.25 * np.sin(2 * np.pi * 1000 * t)       # -12 dBFS
sine100 = 0.25 * np.sin(2 * np.pi * 100 * t)
sine9k = 0.5 * np.sin(2 * np.pi * 9000 * t)
pinkSt = np.stack([pink_noise(), np.roll(pink_noise(), 7)], axis=1)
loud = pinkSt * 10 ** (14 / 20)                      # -4 dBFS RMS : on pousse fort
silence = np.zeros(SR * 3)

print("A. Drive à 0 : discret (sinus 1 kHz, -12 dBFS)")
for s, name in enumerate(STYLES):
    _, thd = harmonics(render("s1k", sine1k, style=s, drive=0)[0][:, 0], 1000)
    check(thd < -35, f"{name:<10} THD {thd:.1f} dB (< -35)")

print("B. Caractères (Drive 0,5)")
H = {}
for s, name in enumerate(STYLES):
    H[name] = harmonics(render("s1k", sine1k, style=s, drive=0.5)[0][:, 0], 1000)
    h, thd = H[name]
    print(f"       {name:<10} H2 {h[0]:6.1f}  H3 {h[1]:6.1f}  THD {thd:6.1f} dB")
check(H["Triode"][0][0] > H["Triode"][0][1] + 6, "Triode : paires dominantes (H2 > H3 + 6)")
check(H["Pentode"][0][1] > H["Pentode"][0][0] + 15, "Pentode : impaires seulement (H3 > H2 + 15)")
hg = H["Germanium"][0]
check(abs(hg[0] - hg[1]) < 6 and min(hg[0], hg[1]) > -35,
      f"Germanium : paires et impaires mêlées (H2 {hg[0]:.1f}, H3 {hg[1]:.1f}, écart < 6 dB)")
check(H["Bande"][1] < min(H[n][1] for n in STYLES if n != "Bande"), "Bande : le plus doux des cinq")
_, t100 = harmonics(render("s100", sine100, style=1, drive=0.5)[0][:, 0], 100)
_, t1k = H["Transfo"]
print(f"       Transfo : 100 Hz -> THD {t100:.1f} dB, 1 kHz -> THD {t1k:.1f} dB")
check(t100 > t1k + 6, "Transfo : le grave sature davantage que le médium")

print("C. Drive et Punish font monter la saturation")
for s, name in enumerate(STYLES):
    vals = [harmonics(render("s1k", sine1k, style=s, drive=d)[0][:, 0], 1000)[1] for d in (0.0, 0.5, 1.0)]
    vp = harmonics(render("s1k", sine1k, style=s, drive=1, punish=1)[0][:, 0], 1000)[1]
    ok = vals[0] < vals[1] < vals[2] < vp + 0.5
    check(ok, f"{name:<10} THD {vals[0]:.0f} -> {vals[1]:.0f} -> {vals[2]:.0f} -> Punish {vp:.0f} dB")

print("D. Niveau (sinus -12 dBFS) : stable à Drive 0, monte modérément avec le Drive")
for s, name in enumerate(STYLES):
    l0 = db(rms(render("s1k", sine1k, style=s, drive=0)[0][SR:, 0])) - db(rms(sine1k))
    l1 = db(rms(render("s1k", sine1k, style=s, drive=1)[0][SR:, 0])) - db(rms(sine1k))
    lp = db(rms(render("s1k", sine1k, style=s, drive=1, punish=1)[0][SR:, 0])) - db(rms(sine1k))
    check(abs(l0) < 1.5 and 1.0 < l1 < 10 and lp < 14, f"{name:<10} {l0:+.1f} / {l1:+.1f} / Punish {lp:+.1f} dB")

print("E. Réponse en fréquence à bas niveau (-30 dBFS, Drive 0)")
sw = log_sweep(amp=0.03)
for s, name in [(3, "Triode"), (1, "Transfo")]:
    at = response_at(render("sweep", sw, style=s, drive=0)[0], sw, LAT)
    vals = {f: at(f) for f in (50, 100, 1000, 5000, 10000, 16000)}
    lim = 0.5 if name == "Triode" else 1.5   # Transfo : +1,2 dB d'air voulu vers 12 kHz
    check(all(abs(v) < (0.5 if f <= 5000 else lim) for f, v in vals.items()),
          f"{name:<10} " + "  ".join(f"{f}:{v:+.2f}" for f, v in vals.items()))

print("F. Repliement (sinus 9 kHz)")
for s, name in enumerate(STYLES):
    w1, _ = worst_alias(render("s9k", sine9k * 0.5, style=s, drive=0.5)[0][:, 0], 9000)
    w2, f2 = worst_alias(render("s9k", sine9k, style=s, drive=1, punish=1)[0][:, 0], 9000)
    check(w1 < -60 and w2 < -40, f"{name:<10} -12 dBFS Drive 0,5 : {w1:.0f} dB | extrême : {w2:.0f} dB à {f2:.0f} Hz")

print("G. Stabilité (bruit rose fort, tous les styles, extrêmes)")
bad = []
for s, d, pu in itertools.product(range(5), (0, 1), (0, 1)):
    y, _ = render("loud", loud, style=s, drive=d, punish=pu)
    if not (np.all(np.isfinite(y)) and np.max(np.abs(y)) < 4):
        bad.append((s, d, pu))
check(not bad, f"20 combinaisons : aucune valeur infinie ni emballement {bad if bad else ''}")

print("H. Mix = 0 exact, taille de bloc indifférente, changement de style sans clic")
y, _ = render("pink", pinkSt, style=2, drive=1, punish=1, mix=0)
check(np.max(np.abs(y[LAT:] - pinkSt[:len(y) - LAT])) < 1e-6, "Mix 0 : signal sec exact (aligné de 33)")
ref, cpu = render("pink", pinkSt, style=3, drive=0.7)
for blk in (37, 2048):
    y, _ = render("pink", pinkSt, style=3, drive=0.7, block=blk)
    check(np.max(np.abs(y - ref)) < 1e-6, f"bloc {blk} : identique")
y, _ = render("s1k", sine1k, style=0, style2=2, drive=0.6)
dy = np.abs(np.diff(y[:, 0]))
mid = len(y) // 2
jump = np.max(dy[mid - 600:mid + 600]); normal = np.max(dy[SR // 2:mid - 2000])
check(jump < 2.5 * normal, f"Bande -> Germanium en cours : saut max {jump:.3f} (normal {normal:.3f})")

print("I. Silence et charge CPU")
y, _ = render("silence", silence, style=2, drive=1, punish=1)
check(np.max(np.abs(y)) < 1e-20, f"silence en sortie (max {np.max(np.abs(y)):.1e})")
CPU_LIMIT = float(os.environ.get("MAKAR_CPU_LIMIT", "6"))
print(f"       CPU (stéréo, 48 kHz, 1 cœur) : {cpu:.2f} %")
check(cpu < CPU_LIMIT, f"charge CPU < {CPU_LIMIT:g} % d'un cœur")

sys.exit(check.summary())
