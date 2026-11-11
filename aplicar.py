"""Aplica o Zelda (mapa F7, Randomizer, tracker, correr no B...) no Hookshot e recompila a pasta do jogo.

    python aplicar.py              acha o jogo e o builder, junta as mudancas, recompila
    python aplicar.py --escolher   pede o hookshot.exe numa janela (em vez de achar sozinho)
    python aplicar.py --so-copiar  so junta e copia (sem recompilar)
    python aplicar.py --restaurar  poe de volta os arquivos originais do builder e recompila

Pensado para sobreviver a versoes novas do Hookshot (regra do usuario: um patch que se aplica sozinho):
- o builder vem de %LOCALAPPDATA%\\HookshotBuilder\\versions\\<current.txt>\\HookshotBuilder\\tools (o runtime host\\ e o menu
  gpu\\vk\\ vem com o codigo); a pasta do jogo, de builder-settings.json ("Out"), ou do hookshot.exe escolhido.
- originais\\ = os arquivos do builder na versao em que as mudancas foram feitas (VERSAO.txt); fonte\\ = os nossos.
- Builder igual a originais\\: copia fonte\\. Builder diferente (versao nova): junta em 3 vias (git merge-file: o arquivo novo,
  a base antiga, o nosso). Juntou limpo: aplica e ATUALIZA originais\\ e fonte\\ para a versao nova. Conflito: para e deixa o
  resultado com as marcas em conflitos\\, sem tocar no builder.
- O exe e a dll de antes ficam em <jogo>\\_backup-antes-zelda\\ (uma vez); o log da compilacao em build.log.
"""
import argparse
import filecmp
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
BUILDER = os.path.join(os.environ["LOCALAPPDATA"], "HookshotBuilder")
NOVOS = ["gpu/vk/zelda_ids.h", "gpu/vk/zelda_notas.h"]   # (arquivos que o builder nao tem)
MARCA = b"(Zelda: AchadoIA"   # (sem o ")": um comentario nosso pode continuar depois do nome)


def versao_do_builder():
    try:
        v = open(os.path.join(BUILDER, "versions", "current.txt"), encoding="utf-8").read().strip()
    except OSError:
        v = ""
    if not v:   # (sem current.txt: a pasta de versao mais nova)
        vs = [d for d in os.listdir(os.path.join(BUILDER, "versions")) if d.startswith("v")]
        v = max(vs, key=lambda s: [int(x) if x.isdigit() else 0 for x in s[1:].split(".")]) if vs else ""
    tools = os.path.join(BUILDER, "versions", v, "HookshotBuilder", "tools")
    if not v or not os.path.isdir(tools):
        raise SystemExit(f"nao achei o Hookshot Builder em {BUILDER}")
    return v, tools


def pasta_do_jogo(escolher):
    if not escolher:
        try:
            out = json.load(open(os.path.join(BUILDER, "builder-settings.json"), encoding="utf-8")).get("Out")
            if out and os.path.isfile(os.path.join(out, "hookshot.exe")):
                return out
        except (OSError, ValueError):
            pass
    import tkinter
    from tkinter import filedialog
    tkinter.Tk().withdraw()
    exe = filedialog.askopenfilename(title="Onde está o hookshot.exe?", filetypes=[("Hookshot", "hookshot.exe"), ("exe", "*.exe")])
    if not exe:
        raise SystemExit("nenhum hookshot.exe escolhido")
    d = os.path.dirname(os.path.abspath(exe))
    return os.path.dirname(d) if os.path.basename(d).lower() == "bin" else d   # (bin\hookshot.exe e o jogo; a pasta e a de cima)


def arquivos(raiz):
    out = []
    for d, _, fs in os.walk(raiz):
        for f in fs:
            rel = os.path.relpath(os.path.join(d, f), raiz).replace("\\", "/")
            if rel != "VERSAO.txt":
                out.append(rel)
    return sorted(out)


def selo():
    """o hash dos cabecalhos novos: vai no topo dos .c do host, que o builder nao recompila por um cabecalho de gpu/vk"""
    h = hashlib.sha1()
    for rel in NOVOS:
        h.update(open(os.path.join(AQUI, "fonte", *rel.split("/")), "rb").read())
    return h.hexdigest()[:12]


def com_selo(rel, dados, s):
    if rel.startswith("host/") and rel.endswith(".c"):
        return (f"/* (Zelda: AchadoIA) selo dos cabecalhos gpu/vk/zelda_*.h: {s} */\n").encode() + dados
    return dados


def sem_selo(dados):
    if dados.startswith(b"/* (Zelda: AchadoIA) selo"):
        return dados.split(b"\n", 1)[1]
    return dados


def juntar(tools, versao):
    """Leva fonte\\ para o builder. Devolve a lista do que foi juntado em 3 vias (versao nova)."""
    s = selo()
    base_ver = open(os.path.join(AQUI, "originais", "VERSAO.txt"), encoding="utf-8").read().strip() \
        if os.path.isfile(os.path.join(AQUI, "originais", "VERSAO.txt")) else "?"
    planos, conflitos, juntados = [], [], []
    for rel in arquivos(os.path.join(AQUI, "originais")):
        b = os.path.join(tools, *rel.split("/"))
        o = os.path.join(AQUI, "originais", *rel.split("/"))
        f = os.path.join(AQUI, "fonte", *rel.split("/"))
        if not os.path.isfile(b):
            conflitos.append(rel + " (sumiu da versao nova do builder)")
            continue
        atual = open(b, "rb").read()
        lf = lambda p: open(p, "rb").read().replace(b"\r\n", b"\n")   # (CRLF de um editor nao conta como mudanca)
        norm = sem_selo(atual).replace(b"\r\n", b"\n")
        if MARCA in atual or norm == lf(o) or norm == lf(f):   # (ja aplicado nesta versao, ou o original de sempre)
            planos.append((rel, lf(f), None))
            continue
        # versao nova: 3 vias (o novo, a base antiga, o nosso), as tres em LF
        tmp = os.path.join(AQUI, "conflitos", ".tmp")
        os.makedirs(tmp, exist_ok=True)
        trio = []
        for nome, p in (("novo", b), ("base", o), ("nosso", f)):
            q = os.path.join(tmp, nome)
            open(q, "wb").write(lf(p))
            trio.append(q)
        r = subprocess.run(["git", "merge-file", "-p", "--diff3", "-L", f"builder {versao}", "-L", f"original {base_ver}",
                            "-L", "Zelda"] + trio, capture_output=True)
        shutil.rmtree(tmp, ignore_errors=True)
        if r.returncode < 0 or r.returncode > 127:
            raise SystemExit(f"git merge-file falhou em {rel}: {r.stderr.decode(errors='replace')}")
        if r.returncode:
            dst = os.path.join(AQUI, "conflitos", *rel.split("/"))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, "wb").write(r.stdout)
            conflitos.append(f"{rel} ({r.returncode} conflito(s): conflitos\\{rel})")
            continue
        planos.append((rel, r.stdout, atual))
        juntados.append(rel)
    if conflitos:
        raise SystemExit(f"o builder {versao} mudou partes que nos tambem mudamos (base {base_ver}); nada foi aplicado:\n  "
                         + "\n  ".join(conflitos) + "\nresolva os arquivos em conflitos\\ e copie para fonte\\.")
    for rel, dados, novo_original in planos:
        dst = os.path.join(tools, *rel.split("/"))
        with open(dst, "wb") as fh:
            fh.write(com_selo(rel, dados, s))
        if novo_original is not None:   # (a versao nova vira a base, e o juntado vira o nosso)
            open(os.path.join(AQUI, "originais", *rel.split("/")), "wb").write(novo_original)
            open(os.path.join(AQUI, "fonte", *rel.split("/")), "wb").write(dados)
    for rel in NOVOS:
        shutil.copyfile(os.path.join(AQUI, "fonte", *rel.split("/")), os.path.join(tools, *rel.split("/")))
    if juntados:
        open(os.path.join(AQUI, "originais", "VERSAO.txt"), "w", encoding="utf-8").write(versao + "\n")
        print(f"juntado com o builder {versao} (antes {base_ver}): " + ", ".join(juntados))
    print(f"aplicados {len(planos) + len(NOVOS)} arquivos no builder {versao} (selo {s})")
    return juntados


def restaurar(tools):
    for rel in arquivos(os.path.join(AQUI, "originais")):
        b = os.path.join(tools, *rel.split("/"))
        if os.path.isfile(b) and MARCA in open(b, "rb").read():
            shutil.copyfile(os.path.join(AQUI, "originais", *rel.split("/")), b)
    for rel in NOVOS:
        p = os.path.join(tools, *rel.split("/"))
        if os.path.isfile(p):
            os.remove(p)
    print("builder restaurado")


def dados_do_jogo(jogo):
    z = os.path.join(jogo, "zelda")
    os.makedirs(z, exist_ok=True)
    if not os.path.isfile(os.path.join(z, "modo.txt")):
        open(os.path.join(z, "modo.txt"), "w").write("normal\n")
    if not os.path.isfile(os.path.join(z, "config.ini")):
        open(os.path.join(z, "config.ini"), "w").write(
            "python=" + (shutil.which("python") or "python") + "\n"
            "randomizer=E:\\OoT-Notas\\randomizer\\randomizer.py\ncorrer_b=1\n")
    if not os.path.isdir(os.path.join(z, "notas", "tracker")):   # (os icones do tracker, da RomFS deste jogo)
        subprocess.run([sys.executable, os.path.join(AQUI, "ferramentas", "gerar_tracker.py"), "--jogo", jogo])


def backup(jogo):
    dst = os.path.join(jogo, "_backup-antes-zelda")
    if os.path.isdir(dst):
        return
    os.makedirs(dst)
    for f in ("hookshot.exe", "hookshot_vk.dll"):
        p = os.path.join(jogo, "bin", f)
        if os.path.isfile(p):
            shutil.copy2(p, os.path.join(dst, f))
    print("backup do exe e da dll em", dst)


def recompilar(tools, jogo):
    py = os.path.join(tools, "python", "python.exe")
    log = os.path.join(AQUI, "build.log")
    cmd = [py, "-I", "-u", "-X", "utf8", os.path.join(tools, "nx2", "make_game.py"), "--update", jogo, "--log", log]
    print("recompilando:", " ".join(cmd), flush=True)
    t = time.time()
    r = subprocess.run(cmd, cwd=tools)
    print(f"fim em {(time.time() - t) / 60:.1f} min, codigo {r.returncode} (log: {log})")
    return r.returncode


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--escolher", action="store_true", help="pede o hookshot.exe numa janela")
    ap.add_argument("--jogo", help="a pasta do jogo (em vez de achar)")
    ap.add_argument("--so-copiar", action="store_true")
    ap.add_argument("--restaurar", action="store_true")
    a = ap.parse_args()
    versao, tools = versao_do_builder()
    jogo = a.jogo or pasta_do_jogo(a.escolher)
    print(f"builder {versao}: {tools}\njogo: {jogo}")
    if a.restaurar:
        restaurar(tools)
    else:
        juntar(tools, versao)
        dados_do_jogo(jogo)
    if a.so_copiar:
        return
    backup(jogo)
    sys.exit(recompilar(tools, jogo))


if __name__ == "__main__":
    main()
