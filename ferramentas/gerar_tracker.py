"""Os icones do tracker de itens (o painel ao vivo do Hookshot, gpu/vk/zelda_notas.h), tirados da RomFS do jogo:
    python gerar_tracker.py [--jogo D:\\hookshot]   ->  <jogo>\\zelda\\notas\\tracker\\<nome>.rgba.zst

- itens, equipamentos e upgrades: UI/Tex/Icon/<nome>.bntx.zs (os icones do inventario do Remake)
- musicas: os simbolos do jogo (UI/Layout2Archive/Common...bntx: OcarinaMelodyIcon_NN), coloridos como as notas de cada uma
- medalhoes: o disco da frente na textura do proprio medalhao (Tex/Mt_Item_Medal_<x>_Alb), recortado em circulo
- pedras espirituais: o jogo nao tem textura colorida (e material): uma gema desenhada num anel dourado
Formato: zstd de "RGBA" + u32 largura + u32 altura + pixels RGBA8 (o mesmo do mapa F7)."""
import argparse
import math
import os
import struct
import sys

import zstandard
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, r"E:\OoT-Notas\tools")
import ootlib as L  # noqa: E402
import bntx  # noqa: E402

LADO = 96

ICONES = [   # (o nome no inventario = o nome do arquivo de icone, salvo onde indicado)
    "Weapon_Range_SlingShot", "Weapon_Range_Bow", "Item_Bomb", "Item_Bombchu", "Item_Boomerang", "Item_Hookshot",
    "Item_Longshot", "Item_Hammer", "Item_GlassTruth:Armor_GlassTruth", "Item_Ocarina_Fairy", "Item_Ocarina_Time",
    "Item_DinFire", "Item_FaroreWind", "Item_NayruLove", "Item_Arrow_Fire", "Item_Arrow_Ice", "Item_Arrow_Light",
    "Item_DekuStick", "Item_DekuNut", "Item_DekuSeed", "Item_MagicBean", "Item_Bottle",
    "Weapon_Sword_Kokiri", "Weapon_Sword_Master", "Weapon_Sword_Giant", "Weapon_Sword_Biggoron", "Weapon_Shield_Deku",
    "Weapon_Shield_Hylian", "Weapon_Shield_Mirror", "Armor_Adult_Goron", "Armor_Adult_Zora", "Armor_Adult_HeavyBoot",
    "Armor_Adult_HoverBoot",
    "Item_Gauntlet_Goron:Armor_Child_Gauntlet_Goron", "Armor_Adult_Gauntlet_Silver", "Armor_Adult_Gauntlet_Gold",
    "Item_Scale_Silver", "Item_Scale_Gold", "Item_Wallet_Medium", "Item_Wallet_Large", "Item_FairyBless_MP",
    "Item_FairyBless_MPDouble", "Item_FairyBless_DefenseDouble", "Item_BombBag_Small", "Item_Quiver_Small",
    "Item_BulletBag_Small", "Item_AgonyStone", "Item_Token_Gerudo", "Item_Token_Gold",
]
MUSICAS = [   # (o indice do simbolo, a cor)
    ("Item_Melody_ZeldaLullaby", 0, (235, 225, 255)), ("Item_Melody_EponaSong", 1, (255, 205, 150)),
    ("Item_Melody_SariaSong", 2, (150, 240, 150)), ("Item_Melody_SunSong", 3, (255, 240, 140)),
    ("Item_Melody_TimeSong", 4, (150, 220, 255)), ("Item_Melody_StormSong", 5, (200, 200, 220)),
    ("Item_Melody_ForestMinuet", 7, (90, 220, 90)), ("Item_Melody_FireBolero", 8, (255, 90, 70)),
    ("Item_Melody_WaterSerenade", 9, (90, 150, 255)), ("Item_Melody_ShadowNocturne", 10, (190, 110, 255)),
    ("Item_Melody_SpiritRequiem", 11, (255, 160, 60)), ("Item_Melody_LightPrelude", 12, (255, 235, 90)),
]
MEDALHOES = ["Forest", "Fire", "Water", "Shadow", "Spirit", "Light"]
PEDRAS = [("Item_Stone_Forest", (60, 200, 90)), ("Item_Stone_Fire", (230, 50, 60)), ("Item_Stone_Water", (60, 120, 240))]


def gravar(im, dst):
    im = im.convert("RGBA")
    raw = b"RGBA" + struct.pack("<II", im.width, im.height) + im.tobytes()
    with open(dst, "wb") as f:
        f.write(zstandard.ZstdCompressor(level=12).compress(raw))


def quadrado(im):   # (centraliza num quadrado transparente e reduz)
    bb = im.getbbox() or (0, 0, im.width, im.height)
    im = im.crop(bb)
    s = max(im.size)
    q = Image.new("RGBA", (s, s))
    q.paste(im, ((s - im.width) // 2, (s - im.height) // 2))
    return q.resize((LADO, LADO), Image.LANCZOS)


def tex(path, index=0):
    return bntx.decode(L.unzs(open(path, "rb").read()), index)[0]


def medalhao(nome, romfs):
    im = tex(os.path.join(romfs, "Tex", f"Mt_Item_Medal_{nome}_Alb.bntx.zs"))
    w = im.width   # (o disco da frente, com o simbolo: embaixo a direita na textura, 576 px)
    cx, cy, r = 0.66 * w, 0.66 * w, 0.235 * w
    disco = im.crop((int(cx - r), int(cy - r), int(cx + r), int(cy + r))).resize((LADO, LADO), Image.LANCZOS)
    mask = Image.new("L", (LADO * 4, LADO * 4))
    ImageDraw.Draw(mask).ellipse((2, 2, LADO * 4 - 3, LADO * 4 - 3), fill=255)
    disco.putalpha(mask.resize((LADO, LADO), Image.LANCZOS))
    return disco


def pedra(cor):
    S = LADO * 4
    im = Image.new("RGBA", (S, S))
    d = ImageDraw.Draw(im)
    c = S / 2
    d.ellipse((S * 0.08, S * 0.08, S * 0.92, S * 0.92), outline=(220, 175, 70, 255), width=int(S * 0.07))   # (o anel dourado)
    gema = [(c, S * 0.2), (S * 0.72, S * 0.42), (S * 0.62, S * 0.78), (S * 0.38, S * 0.78), (S * 0.28, S * 0.42)]
    d.polygon(gema, fill=cor + (255,))
    claro = tuple(min(255, int(v * 1.5 + 40)) for v in cor) + (255,)
    d.polygon([(c, S * 0.2), (S * 0.72, S * 0.42), (c, S * 0.47)], fill=claro)   # (a faceta de luz)
    escuro = tuple(int(v * 0.55) for v in cor) + (255,)
    d.polygon([(S * 0.62, S * 0.78), (S * 0.38, S * 0.78), (c, S * 0.47)], fill=escuro)
    d.polygon(gema, outline=(255, 245, 210, 255), width=int(S * 0.02))
    return im.resize((LADO, LADO), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jogo", default=r"D:\hookshot")
    a = ap.parse_args()
    romfs = os.path.join(a.jogo, "romfs")
    L.set_game(a.jogo)
    out = os.path.join(a.jogo, "zelda", "notas", "tracker")
    os.makedirs(out, exist_ok=True)
    n = 0
    for e in ICONES:
        nome, _, arq = e.partition(":")
        p = os.path.join(romfs, "UI", "Tex", "Icon", (arq or nome) + ".bntx.zs")
        if not os.path.isfile(p):
            print("sem icone:", nome)
            continue
        gravar(quadrado(tex(p)), os.path.join(out, nome + ".rgba.zst"))
        n += 1
    common = L.unzs(open(os.path.join(romfs, "UI", "Layout2Archive", "Common.Product.100.Nin_Ounce_NVN2.bntx.zs"), "rb").read())
    idx = {t["name"]: i for i, t in enumerate(bntx.textures(common))}
    for nome, k, cor in MUSICAS:
        im = bntx.decode(common, idx["OcarinaMelodyIcon_%02d" % k])[0].convert("RGBA")
        r, g, b, al = im.split()
        lum = Image.merge("RGB", (r, g, b)).convert("L")
        tint = Image.merge("RGB", [lum.point(lambda v, c=c: v * c // 255) for c in cor])
        tint.putalpha(al)
        gravar(quadrado(tint), os.path.join(out, nome + ".rgba.zst"))
        n += 1
    for m in MEDALHOES:
        gravar(medalhao(m, romfs), os.path.join(out, f"Item_Medal_{m}.rgba.zst"))
        n += 1
    for nome, cor in PEDRAS:
        gravar(pedra(cor), os.path.join(out, nome + ".rgba.zst"))
        n += 1
    print(f"{n} icones em {out}")


if __name__ == "__main__":
    main()
