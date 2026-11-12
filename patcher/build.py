r"""Monta o Hookshot-Zelda-Patch.exe: pacote.zip (fonte\, originais\, notas\ do jogo, codigo\ = randomizer + geradores de
mods + dados do OoT-Notas, mods\ prontos) + csc do .NET Framework.
    python patcher\build.py [--jogo D:\hookshot]  ->  Hookshot-Zelda-Patch.exe na raiz do repo"""
import argparse, os, subprocess, sys, time, zipfile

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
CSC = r"C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
FW = os.path.dirname(CSC)

ap = argparse.ArgumentParser()
ap.add_argument("--jogo", default=r"D:\hookshot")
ap.add_argument("--oot", default=r"E:\OoT-Notas", help="repo OoT-Notas: o codigo do randomizer e dos mods")
a = ap.parse_args()

# o que vai para <jogo>\zelda\codigo (o patcher roda tudo com o Python do Hookshot Builder, que ja tem zstandard)
CODIGO = ["tools/ootlib.py",
          "randomizer/randomizer.py", "randomizer/logica.py", "randomizer/especiais.py", "randomizer/modfs.py",
          "randomizer/evfl.py", "randomizer/bymlrw.py",
          "mods/dark_link.py", "mods/dark_link_aura.py",
          # (dados tirados do jogo que o randomizer le; data\ do repo e gitignored, este exe e pessoal)
          "data/rando_locais.json", "data/rando_lojas.json", "data/rando_deku.json", "data/banc_flags.json"]
# mods prontos (arquivos) que vao para <jogo>\mods; o "Dark Link (Link adulto)" e gerado no PC pelo dark_link.py
MODS_PRONTOS = ["Olhos Sharingan (Link)", "Tunica preta (Link crianca)"]
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
    for rel in CODIGO:
        p = os.path.join(a.oot, *rel.split("/"))
        if not os.path.isfile(p):
            sys.exit(f"falta {p}")
        z.write(p, "codigo/" + rel, zipfile.ZIP_DEFLATED)
    for m in MODS_PRONTOS:
        raiz = os.path.join(a.jogo, "mods", m)
        if not os.path.isdir(raiz):
            sys.exit(f"falta o mod {raiz}")
        for d, _, fs in os.walk(raiz):
            for f in fs:
                p = os.path.join(d, f)
                z.write(p, "mods/" + m + "/" + os.path.relpath(p, raiz).replace("\\", "/"), zipfile.ZIP_STORED)
    oot = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=a.oot, capture_output=True, text=True).stdout.strip()
    z.writestr("codigo/VERSAO.txt", f"OoT-Notas {oot}, pacote {pid}\n")
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
