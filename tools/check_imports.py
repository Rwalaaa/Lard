"""Vérifie qu'un plugin Windows ne dépend d'aucune DLL du runtime Visual C++ (MSVCP*, VCRUNTIME*,
api-ms-win-crt-*) : il doit se charger sur un PC où le redistribuable n'est pas installé.
Usage : python check_imports.py chemin/vers/Plugin.vst3/Contents/x86_64-win/Plugin.vst3 [...]"""
import sys
import pefile

FORBIDDEN = ("msvcp", "vcruntime", "api-ms-win-crt-", "ucrtbase", "concrt")
bad = 0
for path in sys.argv[1:]:
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    dlls = [e.dll.decode().lower() for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])]
    runtime = [d for d in dlls if d.startswith(FORBIDDEN)]
    print(f"{path} : {len(dlls)} DLL importées" + (f", RUNTIME VC++ : {', '.join(runtime)}" if runtime else ", aucune du runtime VC++"))
    bad += bool(runtime)
sys.exit(1 if bad else 0)
