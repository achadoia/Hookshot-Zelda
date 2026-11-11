"""Mod de olho SO no Link (os outros personagens ficam com o olho normal).

uso: python olho_so_link.py <Eye_Inner_Iris_CmnTex_Chara_Eye_Alb.bntx.zs do mod> <pasta do mod, ex. D:\\hookshot\\mods\\Olhos (Link)>

Os mods de olho (LinkEyeTextureSwaps) trocam Tex/Eye_Inner_Iris_CmnTex_Chara_Eye_Alb, que e a iris de TODOS os personagens.
Aqui o modelo do Link (Player_Child e Player_Adult) passa a pedir uma textura so dele, Lnk_Inner_Iris_CmnTex_Chara_Eye_Alb
(mesmo tamanho de nome: so troca o texto no lugar, nada no arquivo se move), e a textura do mod vai com esse nome.
So o comeco do nome muda: os dicionarios do BFRES/BNTX (arvores radix) comparam os nomes a partir do FIM, entao os bits
que ja estao gravados continuam valendo. Os BFRES saem com o mesmo tamanho (a ResourceSizeTable continua servindo).
"""
import os
import shutil
import sys

sys.path.insert(0, r'E:\OoT-Notas\tools')
sys.path.insert(0, r'E:\OoT-Notas\randomizer')
import ootlib as L  # noqa: E402
import modfs as M  # noqa: E402
import zstandard  # noqa: E402

VELHO = b'Eye_Inner_Iris_CmnTex_Chara_Eye_Alb'
NOVO = b'Lnk_Inner_Iris_CmnTex_Chara_Eye_Alb'
assert len(VELHO) == len(NOVO) and VELHO[-12:] == NOVO[-12:]
MODELOS = ['Model/Player_Child.Player_Child.bfres.zs', 'Model/Player_Adult.Player_Adult.bfres.zs',
           'Model/Player_Child_Op.Player_Child_Op.bfres.zs']   # (Op: o Link da abertura)


def comprimir(raw_original, dados):
    dic = M.dict_do_arquivo(raw_original)
    c = zstandard.ZstdCompressor(level=16, dict_data=dic) if dic else zstandard.ZstdCompressor(level=16)
    return c.compress(dados)


def trocar_uma_vez(d, rel):
    marca = len(VELHO).to_bytes(2, 'little') + VELHO + b'\0'   # (no pool de strings: u16 tamanho + texto + 0)
    n = d.count(marca)
    if n != 1:
        raise SystemExit(f'{rel}: o nome aparece {n} vezes (esperado 1)')
    return d.replace(marca, len(NOVO).to_bytes(2, 'little') + NOVO + b'\0')


def main():
    iris, mod = sys.argv[1], sys.argv[2]
    romfs = os.path.join(mod, 'romfs')
    for rel in MODELOS:
        p = L.find(rel)
        raw = open(p, 'rb').read()
        d = L.unzs(raw)
        novo = trocar_uma_vez(d, rel)
        assert len(novo) == len(d)
        dest = os.path.join(romfs, *rel.split('/'))
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        open(dest, 'wb').write(comprimir(raw, novo))
        assert L.unzs(open(dest, 'rb').read()) == novo
        print('modelo:', rel)
    raw_tex = open(iris, 'rb').read()
    t = trocar_uma_vez(L.unzs(raw_tex), 'textura')
    orig = L.find('Tex/' + VELHO.decode() + '.bntx.zs')
    dest = os.path.join(romfs, 'Tex', NOVO.decode() + '.bntx.zs')
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    open(dest, 'wb').write(comprimir(open(orig, 'rb').read(), t))
    velho = os.path.join(romfs, 'Tex', VELHO.decode() + '.bntx.zs')
    if os.path.exists(velho):   # (a troca para todos: sai)
        os.remove(velho)
        print('removida a troca da iris de todos os personagens')
    print('textura:', dest)


main()
