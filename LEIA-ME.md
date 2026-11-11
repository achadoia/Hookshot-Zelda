# Hookshot-Zelda

Mapa F7 (as Notas do Zelda) e modo Randomizer **dentro** do Ocarina of Time nativo de PC (Hookshot, `D:\hookshot`).

O Hookshot Builder traz o código do runtime (`host\`, GPL) e do menu Insert (`gpu\vk\`, Dear ImGui). Este repositório
guarda as mudanças nesses arquivos e recompila a pasta do jogo com elas.

## Como aplicar

    python aplicar.py              # copia fonte\ para o builder e recompila D:\hookshot (make_game.py --update)
    python aplicar.py --restaurar  # volta os arquivos originais do builder e recompila

- `originais\` = os arquivos do builder **v2.1.0** como vieram; `fonte\` = os nossos. `git diff --no-index originais fonte`
  mostra tudo o que mudou.
- Se o builder for atualizado, `aplicar.py` para quando um arquivo dele não bate mais com `originais\`: refazer as mudanças
  sobre a versão nova.
- O exe e a dll de antes ficam em `D:\hookshot\_backup-antes-zelda\`.

## O que muda no jogo

| Onde | O quê |
|---|---|
| F7 / botão do controle | Em jogo: o **mapa** (página 6 do menu). Na tela de título: o **modo de jogo** (página 7). |
| Menu Insert > Controls | Linha nova **"Mapa do Zelda: botão"**: Nenhum, L3, R3... (salva como `zelda_mapa_pad`). |
| Tela de título | Selo no canto inferior esquerdo: modo atual (NORMAL / RANDOMIZER + seed) e a tecla. |
| Randomizer | Embaralha os 98 baús de item (E:\OoT-Notas\randomizer). Só **novo jogo**, saves separados. |
| Correr no B | Segurando B com o analógico inclinado (0,15 s), o jogo recebe A segurado: rola e corre como segurando A. O toque de B continua só B (cancelar, descer da escada). Só em jogo; `correr_b=0` em `zelda\config.ini` desliga. |
| Bracelete (randomizer) | Puxar a bomba é "levantar" (como a flor-bomba) e o Remake só deixa com o Bracelete Goron. Com a bolsa de bombas de um baú e sem o bracelete, o bracelete entra junto (`hos_main.c`, `oot_inventory_refresh`). |
| Tracker de itens | Menu Insert > Gameplay: "Tracker de itens" (liga), "Tracker: opacidade", "Tracker: tamanho". Uma coluna fina na esquerda, só em jogo, com os ícones do Remake (`ferramentas\gerar_tracker.py` -> `zelda\notas\tracker`): itens, equipamento, upgrades, pedras, medalhões, músicas; aceso = tem, apagado = falta, munição com a quantidade. |
| Cutscenes (randomizer) | A seed tira a trava `SkipSetting/DisableSkip` da abertura (Dm_Kokiri_0000) e dos créditos: com isso todas as cutscenes aceitam o pular do jogo. (As outras nunca tiveram trava; o fundo do título fica como está.) |

### Mapa (página 6)
L/R (Q/E, roda do mouse): área · Esquerda/Direita: item · Cima/Baixo: filtro por categoria · A/Enter: criança/adulto ·
B/Esc/F7/botão: fecha. Mostra o mapa da área, os ícones do próprio jogo, o que já foi pego (verde) e o Link.
- **Ao vivo**: baús, canções e listas pelas flags Bool do GameDataMgr (lidas da memória do jogo); a cena atual, a idade e a
  posição do Link vêm do gancho do Link (`host/hos_main.c zelda_push_state`, 4x/s).
- **Do save**: skulltulas e corações (chaves de 64 bits) vêm do último `progress.sav` (o jogo faz autosave sempre).

### Randomizer (página 7)
- **Normal**: a campanha de sempre (`user\save`).
- **Randomizer: novo jogo**: roda `python randomizer.py --saida D:\hookshot\zelda\randomizer.novo` (~10 s), põe a seed no lugar,
  guarda o `user\save_randomizer` anterior como `save_randomizer.backup-<data>`, grava `zelda\modo.txt` e **reinicia o jogo**.
- **Randomizer: continuar**: volta para a seed atual (reinicia).
- **O que embaralhar** (caixas, como nas opções do Ship of Harkinian, cada uma com a explicação embaixo): baús das masmorras,
  baús do mundo; lojas, NPCs/quests e músicas aparecem como "em breve" (precisam trocar o item na hora da entrega, no runtime).
  A escolha fica em `zelda\embaralhar.txt` e vai para o gerador (`--embaralhar masmorras,mundo`); a seed guarda a dela em
  `randomizer\embaralhar.txt`.
- No modo randomizer: `host/mods.c` põe `zelda\randomizer\romfs` por cima da RomFS; `host/nn_fs.c` usa `user\save_randomizer`
  (copia `option.sav`/`account.sav` da campanha normal na primeira vez); `host/prompts.c` serve `zelda\randomizer\rsizetable.zs`.

**A tela preta do v1 (NS2-IA)**: as cenas reescritas saem 10–20% maiores e o motor descarta um arquivo maior que a sua entrada
na ResourceSizeTable (a margem original é de ~284 bytes). O gerador agora grava a RSTB com essas entradas aumentadas.

## Arquivos em `D:\hookshot\zelda`
- `modo.txt` — `normal` ou `randomizer` (lido no início).
- `config.ini` — `python=` e `randomizer=` (o gerador); `correr_b=1|0`.
- `notas\` — `dados.txt` + `mapas\*.rgba.zst` + `icones\*.rgba.zst`, gerados por `ferramentas\gerar_notas.py` a partir de
  `D:\NS2-IA\notas\0400486024DEC000` (E:\OoT-Notas\tools\gerar_notas.py / icones_notas.py).
- `randomizer\` — a seed atual (`romfs\`, `rsizetable.zs`, `seed.txt`, `spoiler.txt`); `randomizer.anterior\` a de antes.
- `gerar.log` — a saída do último gerador.

## Mudanças por arquivo (`fonte\`)
- `gpu/vk/zelda_ids.h` (novo) — ids do estado ao vivo (5000–5004), `zelda_fnv`, a pasta `zelda` e o modo.
- `gpu/vk/zelda_notas.h` (novo) — dados, imagens (zstd → ImTextureData), save, flags ao vivo, páginas 6 e 7, selo.
- `gpu/vk/vk_menu.cpp` — F7 (tecla 17), botão do controle, linha nos Controles, páginas, selo, `menu_option_set`.
- `gpu/vk/vk_imgui.cpp` — o espaço de imagens do menu de 16 para 256 (com 16, o mapa F7 esgotava o espaço depois de 2 ou 3 áreas e o Hookshot parava de desenhar menu, mapa e FPS até reiniciar).
- `host/hos_main.c` — `zelda_push_state` no gancho do Link; marca a hora da última atualização do Link (`hos_zelda_link_ns`).
- `host/nn_hid.c` — correr no B (`zelda_b_dash`).
- `host/oot_menu.c` — avisa quando uma tela de menu do jogo está na frente (o selo do título).
- `host/mods.c` — `hos_zelda_rando`, `hos_zelda_dir`, a camada da seed.
- `host/nn_fs.c` — `save_randomizer`.
- `host/prompts.c` — a RSTB da seed.
