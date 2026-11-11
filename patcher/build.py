"""Monta o Hookshot-Zelda-Patch.exe: pacote.zip (fonte\, originais\, notas\ do jogo) + csc do .NET Framework.
    python patcher\build.py [--jogo D:\hookshot]  ->  Hookshot-Zelda-Patch.exe na raiz do repo"""
import argparse, os, subprocess, sys, time, zipfile

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
CSC = r"C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
FW = os.path.dirname(CSC)

ap = argparse.ArgumentParser()
ap.add_argument("--jogo", default=r"D:\hookshot")
a = ap.parse_args()
hash_ = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=RAIZ, capture_output=True, text=True).stdout.strip()
sujo = subprocess.run(["git", "status", "--porcelain", "fonte", "originais"], cwd=RAIZ, capture_output=True, text=True).stdout.strip()
pid = f"{hash_}{'+' if sujo else ''} {time.strftime('%Y-%m-%d %H:%M')}"
pz = os.path.join(AQUI, "pacote.zip")
with zipfile.ZipFile(pz, "w") as z:
    z.writestr("pacote.txt", pid + "\n", zipfile.ZIP_DEFLATED)
    for pasta in ("fonte", "originais"):
        for d, _, fs in os.walk(os.path.join(RAIZ, pasta)):
            for f in fs:
                p = os.path.join(d, f)
                z.write(p, os.path.relpath(p, RAIZ).replace("\\", "/"), zipfile.ZIP_DEFLATED)
    notas = os.path.join(a.jogo, "zelda", "notas")
    for d, _, fs in os.walk(notas):
        for f in fs:
            p = os.path.join(d, f)
            z.write(p, "notas/" + os.path.relpath(p, notas).replace("\\", "/"), zipfile.ZIP_STORED)
exe = os.path.join(RAIZ, "Hookshot-Zelda-Patch.exe")
cmd = [CSC, "/nologo", "/target:winexe", "/optimize+", f"/out:{exe}", f"/win32icon:{os.path.join(AQUI, 'achadoia.ico')}",
       f"/resource:{pz},pacote.zip", f"/resource:{os.path.join(AQUI, 'achadoia.ico')},achadoia.ico",
       "/r:System.Windows.Forms.dll", "/r:System.Drawing.dll", "/r:System.Core.dll",
       "/r:" + os.path.join(FW, "System.IO.Compression.dll"), "/r:" + os.path.join(FW, "System.IO.Compression.FileSystem.dll"),
       os.path.join(AQUI, "patcher.cs"), os.path.join(AQUI, "diff3.cs"), os.path.join(AQUI, "neo.cs")]
r = subprocess.run(cmd)
os.remove(pz)
if r.returncode:
    sys.exit(r.returncode)
print(f"{exe} ({os.path.getsize(exe) / 1e6:.1f} MB), pacote {pid}")
