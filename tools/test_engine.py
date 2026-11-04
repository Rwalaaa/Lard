"""Tests hors-ligne du moteur Lard : génère des signaux, appelle build/render, mesure."""
import subprocess, sys, os, itertools
import numpy as np

SR = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RENDER = os.environ.get("LARD_RENDER") or next(
    (p for p in [os.path.join(ROOT, "build", "lard_render"),
                 os.path.join(ROOT, "build", "Release", "lard_render.exe"),
                 os.path.join(ROOT, "build", "lard_render.exe")] if os.path.exists(p)),
    os.path.join(ROOT, "build", "lard_render"))
OUT = os.path.join(ROOT, "build", "test_audio")
os.makedirs(OUT, exist_ok=True)
LAT = 32
fails = []


def write_wav(path, x):
    x = np.asarray(x, dtype=np.float32)
    if x.ndim == 1:
        x = np.stack([x, x], axis=1)
    ch = x.shape[1]
    data = x.astype("<f4").tobytes()
    import struct
    hdr = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 3, ch, SR, SR * ch * 4, ch * 4, 32)
    hdr += b"data" + struct.pack("<I", len(data))
    with open(path, "wb") as f:
        f.write(hdr + data)


def read_wav(path):
    import struct
    b = open(path, "rb").read()
    ch = struct.unpack("<H", b[22:24])[0]
    i = 12
    while i < len(b):
        cid, size = b[i:i + 4], struct.unpack("<I", b[i + 4:i + 8])[0]
        if cid == b"data":
            return np.frombuffer(b[i + 8:i + 8 + size], dtype="<f4").reshape(-1, ch)
        i += 8 + size + (size & 1)
    raise ValueError(path)


def render(name, x, fat, color=0.5, gain=0.0, mix=1.0, block=512):
    src = os.path.join(OUT, f"{name}_in.wav")
    dst = os.path.join(OUT, f"{name}_f{fat}_c{color}_m{mix}_b{block}.wav")
    if not os.path.exists(src):
        write_wav(src, x)
    r = subprocess.run([RENDER, src, dst, str(fat), str(color), str(gain), str(mix), str(block)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr)
    cpu = float(r.stdout.split("cpu_pct=")[1])
    return read_wav(dst), cpu


def db(x):
    return 20 * np.log10(np.maximum(x, 1e-12))


def rms(x):
    return np.sqrt(np.mean(np.square(x)))


def check(cond, msg):
    print(("  OK   " if cond else "  ÉCHEC ") + msg)
    if not cond:
        fails.append(msg)


# ---------------- signaux ----------------
t = np.arange(int(SR * 4)) / SR
sine1k = 0.25 * np.sin(2 * np.pi * 1000 * t)          # -12 dBFS
sine9k = 0.5 * np.sin(2 * np.pi * 9000 * t)           # -6 dBFS
# balayage log 20 Hz -> 22 kHz, -18 dBFS
T = 8.0
ts = np.arange(int(SR * T)) / SR
f0, f1 = 20.0, 22000.0
K = T / np.log(f1 / f0)
sweep = 0.125 * np.sin(2 * np.pi * f0 * K * (np.exp(ts / K) - 1))

bpm = 128
beat = int(SR * 60 / bpm)
bars = 8
n = beat * 4 * bars
rng = np.random.default_rng(1)
kick = np.zeros(n); bass = np.zeros(n); hats = np.zeros(n)
kt = np.arange(int(0.35 * SR)) / SR
kph = 2 * np.pi * np.cumsum(50 + 110 * np.exp(-kt * 30)) / SR
kone = np.sin(kph) * np.exp(-kt * 8)
for b in range(4 * bars):
    s = b * beat
    kick[s:s + len(kone)] += kone[:max(0, min(len(kone), n - s))]
bt = np.arange(int(beat / 2)) / SR
saw = 2 * ((55 * bt) % 1) - 1
saw = np.convolve(saw, np.ones(8) / 8, mode="same") * np.exp(-bt * 6)
for b in range(4 * bars):
    s = b * beat + beat // 2
    bass[s:s + len(saw)] += saw[:max(0, min(len(saw), n - s))]
ht = int(0.04 * SR)
hone = rng.standard_normal(ht) * np.exp(-np.arange(ht) / SR * 120)
hone = np.diff(hone, prepend=0)
for b in range(8 * bars):
    s = b * beat // 2 + beat // 4
    hats[s:s + ht] += hone[:max(0, min(ht, n - s))]
loop = 0.55 * kick + 0.30 * bass + 0.06 * hats
loop = loop / np.max(np.abs(loop)) * 0.89            # crête -1 dBFS
loopSt = np.stack([loop, np.roll(loop, 3) * 0.95], axis=1)
silence = np.zeros(SR * 4)

# ---------------- tests ----------------
print("A. Transparence à Fatness = 0")
y, _ = render("sine1k", sine1k, 0.0)
# référence = entrée passée par le même filtre anti-continu (passe-haut 5 Hz)
R = 1 - 2 * np.pi * 5 / SR
ref = np.zeros_like(sine1k); x1 = y1 = 0.0
for i, v in enumerate(sine1k):
    y1 = v - x1 + R * y1; x1 = v; ref[i] = y1
err = y[LAT:, 0] - ref[:len(y) - LAT]
check(db(rms(err[SR:])) - db(rms(sine1k)) < -60,
      f"sinus 1 kHz : résidu {db(rms(err[SR:])) - db(rms(sine1k)):.1f} dB (< -60)")
y, _ = render("sweep", sweep, 0.0)
Y = np.abs(np.fft.rfft(y[LAT:, 0][:len(sweep) - LAT]))
X = np.abs(np.fft.rfft(sweep[:len(sweep) - LAT]))
freqs = np.fft.rfftfreq(len(sweep) - LAT, 1 / SR)
for f, tol in [(50, 0.1), (1000, 0.1), (10000, 0.1), (16000, 0.2), (18000, 0.5), (20000, 1.5)]:
    band = (freqs > f * 0.97) & (freqs < f * 1.03)
    d = db(np.sum(Y[band])) - db(np.sum(X[band]))
    check(abs(d) < tol, f"réponse {f} Hz : {d:+.2f} dB (±{tol})")

print("B. Mix = 0 : signal sec exact (aligné de 32 échantillons)")
y, _ = render("loop", loopSt, 1.0, mix=0.0)
err = np.max(np.abs(y[LAT:] - loopSt[:len(y) - LAT]))
check(err < 1e-6, f"écart max {err:.2e}")

print("C/D. Stabilité et plafond sur tous les coins de réglage (boucle à -1 dBFS)")
base_rms = rms(loopSt)
results = {}
for fat, color in itertools.product([0.0, 0.25, 0.5, 0.75, 1.0], [0.0, 0.5, 1.0]):
    y, cpu = render("loop", loopSt, fat, color)
    finite = np.all(np.isfinite(y))
    pk = db(np.max(np.abs(y)))
    results[(fat, color)] = (y, cpu)
    check(finite and pk < 0.0, f"fat={fat:<4} color={color:<3} : crête {pk:+.2f} dBFS, NaN/inf={'non' if finite else 'OUI'}")

print("E. Repliement (sinus 9 kHz à -6 dBFS, Fatness = 1)")
y, _ = render("sine9k", sine9k, 1.0)
seg = y[SR:3 * SR, 0] * np.blackman(2 * SR)
S = np.abs(np.fft.rfft(seg))
fr = np.fft.rfftfreq(len(seg), 1 / SR)
fund = S[np.argmin(np.abs(fr - 9000))]
harm = np.zeros_like(S, dtype=bool)
for h in range(1, 3):
    harm |= np.abs(fr - 9000 * h) < 30
harm |= fr < 30
worst = db(np.max(S[~harm])) - db(fund)
worst_f = fr[~harm][np.argmax(S[~harm])]
check(worst < -60, f"pire composante non harmonique {worst:.1f} dB à {worst_f:.0f} Hz (< -60)")

print("F. Niveau et densité selon Fatness (couleur neutre)")
prev = -99
for fat in [0.0, 0.25, 0.5, 0.75, 1.0]:
    y = results[(fat, 0.5)][0]
    r = db(rms(y)) - db(base_rms)
    crest = db(np.max(np.abs(y))) - db(rms(y))
    print(f"       fat={fat:<4}  RMS {r:+5.1f} dB  facteur de crête {crest:4.1f} dB")
    check(r >= prev - 0.05, f"niveau croissant à fat={fat}")
    prev = r

print("G. Couleur : brillance croissante (fat = 0.5)")
cents = []
for c in [0.0, 0.5, 1.0]:
    y = results[(0.5, c)][0][:, 0]
    Sy = np.abs(np.fft.rfft(y)); fy = np.fft.rfftfreq(len(y), 1 / SR)
    cents.append(np.sum(fy * Sy) / np.sum(Sy))
print(f"       centroïde spectral : {cents[0]:.0f} / {cents[1]:.0f} / {cents[2]:.0f} Hz")
check(cents[0] < cents[1] < cents[2], "centroïde croissant avec Color")

print("H. Indépendance à la taille de bloc (DAW : 64, 37, 512, 2048)")
ref = results[(0.75, 0.5)][0]
for blk in [64, 37, 2048]:
    y, _ = render("loop", loopSt, 0.75, block=blk)
    d = np.max(np.abs(y - ref))
    check(d < 1e-6, f"bloc {blk} : écart {d:.2e}")

print("I. Silence (dénormaux) et charge CPU")
y, cpu_sil = render("silence", silence, 1.0)
check(np.all(y == 0) or np.max(np.abs(y)) < 1e-20, f"silence en sortie (max {np.max(np.abs(y)):.1e})")
cpu = results[(1.0, 0.5)][1]
print(f"       CPU (stéréo, 48 kHz, 1 cœur) : {cpu:.2f} % musique, {cpu_sil:.2f} % silence")
check(cpu < 5 and cpu_sil < 5, "charge CPU < 5 % d'un cœur")

print()
print("RÉSULTAT :", "TOUT OK" if not fails else f"{len(fails)} échec(s)")
sys.exit(1 if fails else 0)
