"""Utilitaires communs aux tests hors-ligne des moteurs (WAV, rendu, mesures)."""
import os, struct, subprocess
import numpy as np

SR = 48000


def write_wav(path, x, sr=SR):
    x = np.asarray(x, dtype=np.float32)
    if x.ndim == 1:
        x = np.stack([x, x], axis=1)
    ch = x.shape[1]
    data = x.astype("<f4").tobytes()
    hdr = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 3, ch, sr, sr * ch * 4, ch * 4, 32)
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


def db(x):
    return 20 * np.log10(np.maximum(x, 1e-12))


def rms(x):
    return np.sqrt(np.mean(np.square(x)))


class Renderer:
    """Appelle un outil de rendu (in.wav out.wav nom=valeur ...) et relit le résultat."""

    def __init__(self, exe, outdir):
        self.exe, self.outdir, self.n = exe, outdir, 0
        self.last = {}
        os.makedirs(outdir, exist_ok=True)

    def __call__(self, name, x, **params):
        # le fichier d'entrée est identifié par son contenu : même nom, signal différent -> autre fichier
        import hashlib
        h = hashlib.sha1(np.ascontiguousarray(x, dtype=np.float32).tobytes()).hexdigest()[:10]
        src = os.path.join(self.outdir, f"{name}_{h}_in.wav")
        if not os.path.exists(src):
            write_wav(src, x)
        self.n += 1
        dst = os.path.join(self.outdir, f"{name}_out{self.n}.wav")
        r = subprocess.run([self.exe, src, dst] + [f"{k}={v}" for k, v in params.items()],
                           capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(r.stderr)
        self.last = {kv.split("=")[0]: float(kv.split("=")[1]) for kv in r.stdout.split() if "=" in kv}
        y = read_wav(dst)
        os.remove(dst)
        return y, self.last.get("cpu_pct", 0.0)


class Checker:
    def __init__(self):
        self.fails = []

    def __call__(self, cond, msg):
        print(("  OK   " if cond else "  ÉCHEC ") + msg)
        if not cond:
            self.fails.append(msg)

    def summary(self):
        print()
        print("RÉSULTAT :", "TOUT OK" if not self.fails else f"{len(self.fails)} échec(s)")
        return 1 if self.fails else 0


def harmonics(y, f0, n=9, sr=SR):
    """Amplitudes (dB relatifs au fondamental) des harmoniques 2..n, et THD (dB)."""
    seg = y[sr:3 * sr] * np.blackman(2 * sr)
    S = np.abs(np.fft.rfft(seg))
    fr = np.fft.rfftfreq(len(seg), 1 / sr)
    amp = lambda f: np.max(S[np.abs(fr - f) < 6]) if f < sr / 2 - 50 else 0.0
    h1 = amp(f0)
    hs = [amp(f0 * k) for k in range(2, n + 1)]
    thd = db(np.sqrt(np.sum(np.square(hs)))) - db(h1)
    return [db(h) - db(h1) for h in hs], thd


def worst_alias(y, f0, sr=SR):
    """Pire composante non harmonique (dB relatifs au fondamental) et sa fréquence."""
    seg = y[sr:3 * sr] * np.blackman(2 * sr)
    S = np.abs(np.fft.rfft(seg)); fr = np.fft.rfftfreq(len(seg), 1 / sr)
    fund = S[np.argmin(np.abs(fr - f0))]
    harm = fr < 30
    for k in range(1, int((sr / 2) // f0) + 1):
        harm |= np.abs(fr - f0 * k) < 30
    return db(np.max(S[~harm])) - db(fund), fr[~harm][np.argmax(S[~harm])]


def log_sweep(seconds=8.0, amp=0.125, sr=SR):
    ts = np.arange(int(sr * seconds)) / sr
    K = seconds / np.log(22000 / 20)
    return amp * np.sin(2 * np.pi * 20 * K * (np.exp(ts / K) - 1))


def response_at(y, x, lat, sr=SR):
    """Fonction f -> réponse (dB) du rendu y par rapport au balayage x, décalé de lat."""
    Y = np.abs(np.fft.rfft(y[lat:, 0][:len(x) - lat]))
    X = np.abs(np.fft.rfft(x[:len(x) - lat]))
    fr = np.fft.rfftfreq(len(x) - lat, 1 / sr)
    def at(f):
        b = (fr > f * 0.97) & (fr < f * 1.03)
        return db(np.sum(Y[b])) - db(np.sum(X[b]))
    return at


def pink_noise(seconds=6, level_db=-18, seed=3, sr=SR):
    rng = np.random.default_rng(seed)
    p = np.cumsum(rng.standard_normal(sr * seconds + 8192))
    p = p - np.convolve(p, np.ones(2048) / 2048, mode="same")
    p = p[4096:-4096]
    return p / rms(p) * 10 ** (level_db / 20)
