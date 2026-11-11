/* (Zelda: AchadoIA) O que o runtime (host/) e o menu (gpu/vk/vk_menu.cpp) dividem para o mapa F7 e o Randomizer.
   C e C++. Tudo mora em <pasta do jogo>\zelda:
     modo.txt                 "normal" ou "randomizer": lido UMA vez, no inicio; trocar de modo reinicia o jogo
     randomizer\romfs\...     as cenas da seed (por cima da RomFS, so no modo randomizer: host/mods.c)
     randomizer\rsizetable.zs a tabela de tamanhos com as entradas dessas cenas aumentadas (host/prompts.c)
     randomizer\seed.txt, spoiler.txt
     notas\dados.txt, notas\mapas\*.rgba.zst, notas\icones\*.rgba.zst   (o mapa F7: gpu/vk/zelda_notas.h)
     config.ini               python=, randomizer= (o gerador: E:\OoT-Notas\randomizer\randomizer.py)
   Saves: <user>\save no modo normal, <user>\save_randomizer no randomizer (host/nn_fs.c): a campanha normal nunca e
   tocada por uma seed. */
#pragma once
#include <stdio.h>
#include <string.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* rx_option_set(id, valor) do lado do jogo (host/hos_main.c, no gancho do Link, 2x por segundo) para o menu */
#define ZELDA_ID_BANC 5000   /* FNV-1a 32 do nome da cena atual (Sequence_CurrentBanc); 0 desconhecida */
#define ZELDA_ID_POS_X 5001  /* posicao do Link x 16 (inteiro) */
#define ZELDA_ID_POS_Z 5002
#define ZELDA_ID_LINK 5003   /* 1: o Link apareceu nesta execucao (saiu da tela de titulo) */
#define ZELDA_ID_MENU 5004   /* 1: uma tela de menu do jogo esta na frente (host/oot_menu.c) */
#define ZELDA_ID_RUPIAS 5005 /* PlayerStatus.CurrentRupee (o HUD do Tempo); -1 desconhecido */
#define ZELDA_ID_HORA 5006   /* a hora do dia do jogo em segundos desde a meia-noite (0..86399); -1 desconhecida */

static inline unsigned zelda_fnv(const char *s) {
    unsigned h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    return h ? h : 1;
}

/* <pasta do jogo>\zelda (sem barra no fim); exe_path: o caminho do jogo como hos_game_path_a o da (a pasta, nao bin\) */
static inline void zelda_dir_from(const char *exe_path, char *out, size_t cap) {
    snprintf(out, cap, "%s", exe_path);
    char *a = strrchr(out, '\\'), *b = strrchr(out, '/');
    char *s = a > b ? a : b;
    if (s) *s = 0;
    const size_t n = strlen(out);
    snprintf(out + n, cap - n, "\\zelda");
}

/* 1: modo.txt diz randomizer E a seed existe (randomizer\romfs) */
static inline int zelda_rando_in(const char *zdir) {
    char p[MAX_PATH], line[64] = {0};
    snprintf(p, sizeof p, "%s\\modo.txt", zdir);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    if (!fgets(line, sizeof line, f)) line[0] = 0;
    fclose(f);
    if (strncmp(line, "randomizer", 10)) return 0;
    snprintf(p, sizeof p, "%s\\randomizer\\romfs", zdir);
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
}
