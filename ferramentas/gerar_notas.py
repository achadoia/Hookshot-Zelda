"""Converte as Notas do Zelda do NS2-IA (notas.json + mapas .jpg + icones .png, gerados por E:\\OoT-Notas\\tools) para o
formato que o mapa F7 do Hookshot le (gpu/vk/zelda_notas.h): um texto separado por TAB e imagens RGBA cruas em zstd.

    python gerar_notas.py [--de D:\\NS2-IA\\notas\\0400486024DEC000] [--para D:\\hookshot\\zelda\\notas]

dados.txt (UTF-8, uma linha por registro, campos separados por TAB):
    M  largura altura                                   (o espaco das coordenadas dos mapas)
    A  id nome masmorra mapa_crianca mapa_adulto cx cz altura
    C  id nome grupo cor(RRGGBB) letra visivel rastreavel
    K  categoria nome area era(0 ambos, 1 crianca, 2 adulto) x y flag(u32) chave(u64 hex, 0 sem)
    L  lista nome flag
    B  banc area idade(0 crianca, 1 adulto)
*.rgba.zst: zstd de "RGBA" + u32 largura + u32 altura + pixels RGBA8.
"""
import argparse
import json
import os
import struct

import zstandard
from PIL import Image


def imagem(src, dst, tamanho=None):
    im = Image.open(src).convert("RGBA")
    if tamanho:
        im = im.resize(tamanho, Image.LANCZOS)
    w, h = im.size
    raw = b"RGBA" + struct.pack("<II", w, h) + im.tobytes()
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "wb") as f:
        f.write(zstandard.ZstdCompressor(level=12).compress(raw))


def limpo(s):
    return str(s).replace("\t", " ").replace("\n", " ").replace("\r", " ")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--de", default=r"D:\NS2-IA\notas\0400486024DEC000")
    ap.add_argument("--para", default=r"D:\hookshot\zelda\notas")
    a = ap.parse_args()
    d = json.load(open(os.path.join(a.de, "notas.json"), encoding="utf-8"))
    linhas = [f"M\t{d.get('mapa_w', 1000)}\t{d.get('mapa_h', 720)}"]
    mapas = set()
    for ar in d["areas"]:
        m = ar.get("mapas") or {}
        mc, ma = m.get("crianca") or "", m.get("adulto") or ""
        nomes = []
        for p in (mc, ma):
            n = os.path.splitext(os.path.basename(p))[0] if p else ""
            if n:
                mapas.add((p, n))
            nomes.append(n)
        linhas.append("\t".join(["A", ar["id"], limpo(ar["nome"]), "1" if ar.get("masmorra") else "0", nomes[0], nomes[1],
                                 str(ar.get("cx", 0)), str(ar.get("cz", 0)), str(ar.get("altura", 0))]))
    for c in d["categorias"]:
        linhas.append("\t".join(["C", c["id"], limpo(c["nome"]), limpo(c.get("grupo", "")), c.get("cor", "#ffffff").lstrip("#"),
                                 limpo(c.get("letra", "?")), "1" if c.get("visivel", True) else "0",
                                 "1" if c.get("rastreavel", True) else "0"]))
    era = {"ambos": "0", "crianca": "1", "adulto": "2"}
    for k in d["checks"]:
        if k.get("x") is None or k.get("y") is None:   # (sem lugar no mapa: so nas contas)
            k["x"], k["y"] = -1, -1
        linhas.append("\t".join(["K", k["cat"], limpo(k["nome"]), k["area"], era.get(k.get("era", "ambos"), "0"),
                                 "%.1f" % k["x"], "%.1f" % k["y"], str(int(k.get("flag", 0) or 0)),
                                 k.get("chave", "0") or "0"]))
    for li in d.get("listas", []):
        linhas.append("\t".join(["L", limpo(li["lista"]), limpo(li["nome"]), str(int(li.get("flag", 0)))]))
    for banc, (area, idade) in d.get("bancs", {}).items():
        linhas.append("\t".join(["B", banc, area, "1" if idade == "adulto" else "0"]))
    os.makedirs(a.para, exist_ok=True)
    with open(os.path.join(a.para, "dados.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(linhas) + "\n")
    for p, n in sorted(mapas):
        imagem(os.path.join(a.de, p), os.path.join(a.para, "mapas", n + ".rgba.zst"))
    pasta_ic = os.path.join(a.de, "icones")
    n_ic = 0
    if os.path.isdir(pasta_ic):
        for f in sorted(os.listdir(pasta_ic)):
            if f.lower().endswith(".png"):
                imagem(os.path.join(pasta_ic, f), os.path.join(a.para, "icones", f[:-4] + ".rgba.zst"), (64, 64))
                n_ic += 1
    print(f"notas: {len(d['areas'])} areas, {len(d['checks'])} checks, {len(mapas)} mapas, {n_ic} icones -> {a.para}")


if __name__ == "__main__":
    main()
