"""Aplica o Zelda (mapa F7 + Randomizer) no Hookshot e recompila a pasta do jogo.

    python aplicar.py              copia fonte\\ para o Hookshot Builder e recompila D:\\hookshot (make_game.py --update)
    python aplicar.py --so-copiar  so copia (sem recompilar)
    python aplicar.py --restaurar  poe de volta os arquivos originais do builder (originais\\) e recompila

O builder (%LOCALAPPDATA%\\HookshotBuilder\\versions\\<versao>\\HookshotBuilder\\tools) e de onde o jogo e compilado: o
runtime (host\\) e o menu (gpu\\vk\\) vem com o codigo. Se o builder for atualizado e um arquivo dele nao for mais igual ao
de originais\\, o script para: e preciso refazer as mudancas sobre a versao nova (diff originais\\ fonte\\).
O exe e a dll que funcionavam ficam em D:\\hookshot\\_backup-antes-zelda\\ (uma vez) e o log em build.log.
"""
import argparse
import filecmp
import hashlib
import os
import shutil
import subprocess
import sys
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
JOGO = r"D:\hookshot"
VERSAO = "v2.1.0"
TOOLS = os.path.join(os.environ["LOCALAPPDATA"], "HookshotBuilder", "versions", VERSAO, "HookshotBuilder", "tools")
NOVOS = ["gpu/vk/zelda_ids.h", "gpu/vk/zelda_notas.h"]   # (arquivos que o builder nao tem)


def arquivos(raiz):
    out = []
    for d, _, fs in os.walk(raiz):
        for f in fs:
            out.append(os.path.relpath(os.path.join(d, f), raiz).replace("\\", "/"))
    return sorted(out)


def selo():
    """o hash dos cabecalhos novos: vai no topo dos .c do host, que o builder nao recompila por um cabecalho de gpu/vk"""
    h = hashlib.sha1()
    for rel in NOVOS:
        h.update(open(os.path.join(AQUI, "fonte", *rel.split("/")), "rb").read())
    return h.hexdigest()[:12]


def conferir_builder():
    if not os.path.isdir(TOOLS):
        raise SystemExit(f"nao achei o builder {VERSAO} em {TOOLS}")
    ruins = []
    for rel in arquivos(os.path.join(AQUI, "originais")):
        b = os.path.join(TOOLS, *rel.split("/"))
        if not os.path.isfile(b):
            ruins.append(rel + " (sumiu)")
            continue
        orig = os.path.join(AQUI, "originais", *rel.split("/"))
        if filecmp.cmp(b, orig, shallow=False):
            continue
        # (ja aplicado antes: o do builder e o nosso com o selo)
        txt = open(b, "rb").read()
        if b"(Zelda: AchadoIA)" in txt:
            continue
        ruins.append(rel)
    if ruins:
        raise SystemExit("o builder mudou (atualizacao?): refazer as mudancas sobre a versao nova:\n  " + "\n  ".join(ruins))


def copiar(de_onde):
    s = selo()
    n = 0
    for rel in arquivos(de_onde):
        src = os.path.join(de_onde, *rel.split("/"))
        dst = os.path.join(TOOLS, *rel.split("/"))
        dados = open(src, "rb").read()
        if de_onde.endswith("fonte") and rel.startswith("host/") and rel.endswith(".c"):
            dados = (f"/* (Zelda: AchadoIA) selo dos cabecalhos gpu/vk/zelda_*.h: {s} */\n").encode() + dados
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as f:
            f.write(dados)
        n += 1
    if not de_onde.endswith("fonte"):   # (restaurar: os novos saem)
        for rel in NOVOS:
            p = os.path.join(TOOLS, *rel.split("/"))
            if os.path.isfile(p):
                os.remove(p)
    print(f"copiados {n} arquivos de {de_onde} para o builder (selo {s})")


def backup():
    dst = os.path.join(JOGO, "_backup-antes-zelda")
    if os.path.isdir(dst):
        return
    os.makedirs(dst)
    for f in ("hookshot.exe", "hookshot_vk.dll"):
        p = os.path.join(JOGO, "bin", f)
        if os.path.isfile(p):
            shutil.copy2(p, os.path.join(dst, f))
    print("backup do exe e da dll em", dst)


def recompilar():
    py = os.path.join(TOOLS, "python", "python.exe")
    log = os.path.join(AQUI, "build.log")
    cmd = [py, "-I", "-u", "-X", "utf8", os.path.join(TOOLS, "nx2", "make_game.py"), "--update", JOGO, "--log", log]
    print("recompilando:", " ".join(cmd), flush=True)
    t = time.time()
    r = subprocess.run(cmd, cwd=TOOLS)
    print(f"fim em {(time.time() - t) / 60:.1f} min, codigo {r.returncode} (log: {log})")
    return r.returncode


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--so-copiar", action="store_true")
    ap.add_argument("--restaurar", action="store_true")
    a = ap.parse_args()
    conferir_builder()
    if a.restaurar:
        copiar(os.path.join(AQUI, "originais"))
    else:
        copiar(os.path.join(AQUI, "fonte"))
        z = os.path.join(JOGO, "zelda")
        os.makedirs(z, exist_ok=True)
        if not os.path.isfile(os.path.join(z, "modo.txt")):
            open(os.path.join(z, "modo.txt"), "w").write("normal\n")
        if not os.path.isfile(os.path.join(z, "config.ini")):
            open(os.path.join(z, "config.ini"), "w").write(
                "python=" + (shutil.which("python") or "python") + "\n"
                "randomizer=E:\\OoT-Notas\\randomizer\\randomizer.py\n")
    if a.so_copiar:
        return
    backup()
    sys.exit(recompilar())


if __name__ == "__main__":
    main()
