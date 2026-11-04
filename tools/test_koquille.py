"""Tests hors-ligne du moteur Koquille : signaux de test, appel de build/render_koquille, mesures."""
import os, sys, struct, subprocess, itertools
import numpy as np

SR = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RENDER = os.environ.get("KOQ_RENDER") or next(
    (p for p in [os.path.join(ROOT, "build", "render_koquille"),
                 os.path.join(ROOT, "build", "Release", "render_koquille.exe"),
                 os.path.join(ROOT, "build", "render_koquille.exe")] if os.path.exists(p)),
    os.path.join(ROOT, "build", "render_koquille"))
OUT = os.path.join(ROOT, "build", "test_audio_koq")
os.makedirs(OUT, exist_ok=True)
LAT = 33
fails = []


def write_wav(path, x):
    x = np.asarray(x, dtype=np.float32)
    if x.ndim == 1:
        x = np.stack([x, x], axis=1)
    ch = x.shape[1]
    data = x.astype("<f4").tobytes()
    hdr = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 3, ch, SR, SR * ch * 4, ch * 4, 32)
    hdr += b"data" + struct.pack("<I", len(data))
    with open(path, "wb") as f:
        f.write(hdr + data)


def read_wav(path):
    b = open(path, "rb").read()
    ch = struct.unpack("<H", b[22:24])[0]
    i = 12
    while i < len(b):
        cid, size = b[i:i + 4], struct.unpack("<I", b[i + 4:i + 8])[0]
        if cid == b"data":
            return np.frombuffer(b[i + 8:i + 8 + size], dtype="<f4").reshape(-1, ch)
        i += 8 + size + (size & 1)
    raise ValueError(path)


_count = [0]


def render(name, x, **params):
    src = os.path.join(OUT, f"{name}_in.wav")
    if not os.path.exists(src):
        write_wav(src, x)
    _count[0] += 1
    dst = os.path.join(OUT, f"{name}_out{_count[0]}.wav")
    args = [RENDER, src, dst] + [f"{k}={v}" for k, v in params.items()]
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr)
    cpu = float(r.stdout.split("cpu_pct=")[1])
    y = read_wav(dst)
    os.remove(dst)
    return y, cpu


def db(x):
    return 20 * np.log10(np.maximum(x, 1e-12))


def rms(x):
    return np.sqrt(np.mean(np.square(x)))


def check(cond, msg):
    print(("  OK   " if cond else "  ÉCHEC ") + msg)
    if not cond:
        fails.append(msg)


def harmonics(y, f0, n=9):
    """Amplitudes (dB relatifs au fondamental) des harmoniques 2..n, et THD (dB)."""
    seg = y[SR:3 * SR] * np.blackman(2 * SR)
    S = np.abs(np.fft.rfft(seg))
    fr = np.fft.rfftfreq(len(seg), 1 / SR)
    amp = lambda f: np.max(S[np.abs(fr - f) < 6]) if f < SR / 2 - 50 else 0.0
    h1 = amp(f0)
    hs = [amp(f0 * k) for k in range(2, n + 1)]
    thd = db(np.sqrt(np.sum(np.square(hs)))) - db(h1)
    return [db(h) - db(h1) for h in hs], thd


# ---------------- signaux ----------------
t = np.arange(int(SR * 4)) / SR
sine1k = 0.25 * np.sin(2 * np.pi * 1000 * t)       # -12 dBFS
sine60 = 0.25 * np.sin(2 * np.pi * 60 * t)
sine9k = 0.5 * np.sin(2 * np.pi * 9000 * t)
rng = np.random.default_rng(3)
# bruit rose approximatif (-18 dBFS RMS) pour la stabilité et le CPU
white = rng.standard_normal(SR * 6)
pink = np.cumsum(white)
pink = pink - np.convolve(pink, np.ones(2048) / 2048, mode="same")
pink = pink / rms(pink) * 10 ** (-18 / 20)
pinkSt = np.stack([pink, np.roll(pink, 7)], axis=1)
silence = np.zeros(SR * 3)

# ---------------- tests ----------------
print("A. Transparence avec tout à zéro")
y, _ = render("sine1k", sine1k)
# référence : l'entrée passée par les deux passe-haut 5 Hz du moteur (liaison + anti-continu)
R = 1 - 2 * np.pi * 5 / SR
ref = sine1k.copy()
for _ in range(2):
    out = np.zeros_like(ref); x1 = y1 = 0.0
    for i, v in enumerate(ref):
        y1 = v - x1 + R * y1; x1 = v; out[i] = y1
    ref = out
err = y[LAT:, 0] - ref[:len(y) - LAT]
res = db(rms(err[SR:])) - db(rms(sine1k))
check(res < -55, f"sinus 1 kHz : résidu {res:.1f} dB (< -55)")
_, thd60 = harmonics(render("sine60", sine60)[0][:, 0], 60)
print(f"       transformateurs seuls, 60 Hz à -12 dBFS : THD {thd60:.1f} dB")
check(-75 < thd60 < -35, "les transfos colorent le grave, légèrement (-75 < THD < -35 dB)")

print("B. Caractère des lampes (sinus 1 kHz à -12 dBFS)")
hT, thdT = harmonics(render("sine1k", sine1k, triode=0.6)[0][:, 0], 1000)
hP, thdP = harmonics(render("sine1k", sine1k, pentode=0.6)[0][:, 0], 1000)
print(f"       triode 0.6  : H2 {hT[0]:.1f}  H3 {hT[1]:.1f}  THD {thdT:.1f} dB")
print(f"       pentode 0.6 : H2 {hP[0]:.1f}  H3 {hP[1]:.1f}  THD {thdP:.1f} dB")
check(hT[0] > hT[1] + 6, "triode : harmoniques paires dominantes (H2 > H3 + 6 dB)")
check(hP[1] > hP[0] + 15, "pentode : harmoniques impaires dominantes (H3 > H2 + 15 dB)")
check(-45 < thdT < -10 and -45 < thdP < -10, "THD dans une zone musicale (-45 à -10 dB) à mi-course")

print("C. Density pousse les deux lampes")
prev = -999
for d in [0.0, 0.5, 1.0]:
    _, th = harmonics(render("sine1k", sine1k, triode=0.3, pentode=0.3, density=d)[0][:, 0], 1000)
    print(f"       density={d:<3} : THD {th:.1f} dB")
    check(th > prev + 3, f"THD croissante (density={d})")
    prev = th

print("D. Niveau stable quand on pousse Input (compensation interne)")
for g in [-12, 0, 12]:
    y, _ = render("pink", pinkSt, input=g, triode=0.4, pentode=0.3)
    d = db(rms(y[SR:])) - db(rms(pinkSt[SR:]))
    print(f"       input={g:+d} dB : niveau {d:+.1f} dB")
    check(abs(d) < 3, f"écart de niveau < 3 dB (input={g:+d})")

print("E. Repliement (sinus 9 kHz, lampes à fond)")


def alias_db(x, **params):
    y, _ = render(f"alias{len(params)}_{np.max(x):.2f}", x, **params)
    seg = y[SR:3 * SR, 0] * np.blackman(2 * SR)
    S = np.abs(np.fft.rfft(seg)); fr = np.fft.rfftfreq(len(seg), 1 / SR)
    fund = S[np.argmin(np.abs(fr - 9000))]
    harm = (np.abs(fr - 9000) < 30) | (np.abs(fr - 18000) < 30) | (fr < 30)
    return db(np.max(S[~harm])) - db(fund), fr[~harm][np.argmax(S[~harm])]


w, f = alias_db(0.5 * sine9k, triode=1, pentode=1, density=1)
check(w < -60, f"-12 dBFS, tout à fond : {w:.1f} dB à {f:.0f} Hz (< -60)")
w, f = alias_db(sine9k, triode=1, pentode=1, density=1, input=6)
check(w < -45, f"cas extrême (-6 dBFS, tout à fond, input +6) : {w:.1f} dB à {f:.0f} Hz (< -45, masqué par la saturation)")

print("F. Stabilité sur tous les coins de réglage (bruit rose -18 dBFS RMS)")
for tr, pe, de, gi in itertools.product([0, 1], [0, 1], [0, 1], [-12, 12]):
    y, _ = render("pink", pinkSt, triode=tr, pentode=pe, density=de, input=gi)
    ok = np.all(np.isfinite(y)) and np.max(np.abs(y)) < 4
    if not ok:
        check(False, f"triode={tr} pentode={pe} density={de} input={gi}")
check(True, "32 combinaisons : aucune valeur infinie ni emballement")

print("G. Mix = 0 : signal sec exact (aligné de 33 échantillons), taille de bloc indifférente")
y, _ = render("pink", pinkSt, triode=1, pentode=1, mix=0)
err = np.max(np.abs(y[LAT:] - pinkSt[:len(y) - LAT]))
check(err < 1e-6, f"écart max {err:.2e}")
ref, cpu = render("pink", pinkSt, triode=0.7, pentode=0.4, density=0.5)
for blk in [37, 2048]:
    y, _ = render("pink", pinkSt, triode=0.7, pentode=0.4, density=0.5, block=blk)
    check(np.max(np.abs(y - ref)) < 1e-6, f"bloc {blk} : identique")

print("H. Silence et charge CPU")
y, _ = render("silence", silence, triode=1, pentode=1, density=1)
check(np.max(np.abs(y)) < 1e-20, f"silence en sortie (max {np.max(np.abs(y)):.1e})")
CPU_LIMIT = float(os.environ.get("KOQ_CPU_LIMIT", "6"))
print(f"       CPU (stéréo, 48 kHz, 1 cœur) : {cpu:.2f} %")
check(cpu < CPU_LIMIT, f"charge CPU < {CPU_LIMIT:g} % d'un cœur")

print()
print("RÉSULTAT :", "TOUT OK" if not fails else f"{len(fails)} échec(s)")
sys.exit(1 if fails else 0)
