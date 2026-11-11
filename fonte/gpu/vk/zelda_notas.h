// (Zelda: AchadoIA) O mapa F7 (as Notas do Zelda do NS2-IA, aqui dentro do jogo) e o seletor de modo de jogo
// (Normal / Randomizer) da tela de titulo. Incluido por vk_menu.cpp DENTRO do seu namespace anonimo, depois de draw_ask: usa o
// que a pagina do menu usa (title_banner, column, help_box, hits, prompt_bar, open_now, page, save, app_window).
//   page 6: o mapa. L/R (Q/E) a area, Esquerda/Direita o item, Cima/Baixo o filtro, A (Enter) crianca/adulto, B (Esc) fecha.
//   page 7: o modo de jogo. Normal, Randomizer novo (nova seed: roda o gerador em Python), Randomizer continuar.
// F7, ou o botao do controle escolhido (Controles > "Mapa do Zelda: botao"), abre: o mapa em jogo, o modo de jogo na tela de
// titulo (antes do Link aparecer). Trocar de modo reinicia o jogo (o modo e lido no inicio: host/mods.c, host/nn_fs.c).
// Os dados: <pasta>\zelda\notas (E:\Hookshot-Zelda\ferramentas\gerar_notas.py). O que ja foi pego: as flags ao vivo do
// GameDataMgr (os Bool por hash) e o ultimo save/autosave (as chaves de 64 bits: skulltulas, coracoes).
namespace zelda {

struct Area { std::string id, nome; bool masmorra = false; std::string mapa[2]; float cx = 0, cz = 0, altura = 0; };
struct Cat { std::string id, nome, grupo, letra; ImU32 cor = 0; bool visivel = true, rastreavel = true; };
struct Check { int cat = 0, area = 0, era = 0; std::string nome; float x = -1, y = -1; uint32_t flag = 0; uint64_t chave = 0; };
struct Lista { std::string lista, nome; uint32_t flag = 0; };
struct Tex { ImTextureData* t = nullptr; bool tried = false; float w = 0, h = 0; };

bool loaded = false;
std::string dir, gdir;   // (<pasta>\zelda e a pasta do jogo)
float map_w = 1000, map_h = 720;
std::vector<Area> areas;
std::vector<Cat> cats;
std::vector<Check> checks;
std::vector<Lista> listas;
std::unordered_map<uint32_t, std::pair<int, int>> bancs;   // (FNV do nome da cena -> area, idade 0 crianca 1 adulto)
std::map<std::string, Tex> texs;
bool rando = false;      // (o modo desta execucao)
std::string seed;        // (a seed em zelda\randomizer, se houver)
bool rando_save = false; // (save_randomizer tem um jogo)

// (do lado do jogo: menu_option_set)
std::atomic<int> live_banc{0}, live_x{0}, live_z{0}, live_link{0}, live_menu{0};
int pad_button = 0;   // (0 nenhum, senao HOS_PAD_* + 1: a linha "Mapa do Zelda: botao" dos Controles)

std::string read_line(const std::string& p) {
    std::string s;
    if (FILE* f = fopen(p.c_str(), "rb")) {
        char b[256];
        if (fgets(b, sizeof b, f)) s = b;
        fclose(f);
    }
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}
bool exists(const std::string& p) { return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

/// a pasta dos saves do modo: um progress.sav em <user>\<save>\<uid>\slot_*
bool any_save(const std::string& root) {
    bool found = false;
    WIN32_FIND_DATAA a;
    HANDLE ha = FindFirstFileA((root + "\\*").c_str(), &a);
    if (ha == INVALID_HANDLE_VALUE) return false;
    do {
        if (!(a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || a.cFileName[0] == '.') continue;
        WIN32_FIND_DATAA b;
        HANDLE hb = FindFirstFileA((root + "\\" + a.cFileName + "\\slot_*").c_str(), &b);
        if (hb == INVALID_HANDLE_VALUE) continue;
        do found |= exists(root + "\\" + a.cFileName + "\\" + b.cFileName + "\\progress.sav");
        while (!found && FindNextFileA(hb, &b));
        FindClose(hb);
    } while (!found && FindNextFileA(ha, &a));
    FindClose(ha);
    return found;
}

ImU32 hex_colour(const std::string& h) {
    unsigned v = (unsigned)strtoul(h.c_str(), nullptr, 16);
    return IM_COL32(v >> 16 & 255, v >> 8 & 255, v & 255, 255);
}

void load() {
    if (loaded) return;
    loaded = true;
    char exe[MAX_PATH], zd[MAX_PATH];
    const DWORD n = game_module_path(exe, MAX_PATH);
    exe[n < MAX_PATH ? n : MAX_PATH - 1] = 0;
    zelda_dir_from(exe, zd, sizeof zd);
    dir = zd;
    gdir = dir.substr(0, dir.find_last_of('\\'));
    rando = zelda_rando_in(zd) != 0;
    seed = read_line(dir + "\\randomizer\\seed.txt");
    rando_save = any_save(gdir + "\\user\\save_randomizer");
    FILE* f = fopen((dir + "\\notas\\dados.txt").c_str(), "rb");
    if (!f) {
        logf("zelda: sem %s\\notas\\dados.txt: o mapa F7 fica vazio (rode ferramentas\\gerar_notas.py)", dir.c_str());
        return;
    }
    std::unordered_map<std::string, int> area_at;
    std::unordered_map<std::string, int> cat_at;
    std::vector<std::vector<std::string>> later;   // (K e B: depois das areas e categorias)
    char line[2048];
    while (fgets(line, sizeof line, f)) {
        std::vector<std::string> v;
        std::string cur;
        for (const char* p = line; *p && *p != '\n' && *p != '\r'; p++) {
            if (*p == '\t') v.push_back(cur), cur.clear();
            else cur += *p;
        }
        v.push_back(cur);
        if (v.empty() || v[0].empty()) continue;
        const std::string& t = v[0];
        if (t == "M" && v.size() >= 3) map_w = (float)atof(v[1].c_str()), map_h = (float)atof(v[2].c_str());
        else if (t == "A" && v.size() >= 9) {
            Area a;
            a.id = v[1], a.nome = v[2], a.masmorra = v[3] == "1", a.mapa[0] = v[4], a.mapa[1] = v[5];
            a.cx = (float)atof(v[6].c_str()), a.cz = (float)atof(v[7].c_str()), a.altura = (float)atof(v[8].c_str());
            area_at[a.id] = (int)areas.size();
            areas.push_back(a);
        } else if (t == "C" && v.size() >= 8) {
            Cat c;
            c.id = v[1], c.nome = v[2], c.grupo = v[3], c.cor = hex_colour(v[4]), c.letra = v[5];
            c.visivel = v[6] == "1", c.rastreavel = v[7] == "1";
            cat_at[c.id] = (int)cats.size();
            cats.push_back(c);
        } else if (t == "L" && v.size() >= 4) {
            listas.push_back({v[1], v[2], (uint32_t)strtoul(v[3].c_str(), nullptr, 10)});
        } else if ((t == "K" && v.size() >= 9) || (t == "B" && v.size() >= 4)) {
            later.push_back(v);
        }
    }
    fclose(f);
    for (const auto& v : later) {
        if (v[0] == "B") {
            auto a = area_at.find(v[2]);
            if (a != area_at.end()) bancs[zelda_fnv(v[1].c_str())] = {a->second, v[3] == "1" ? 1 : 0};
            continue;
        }
        auto c = cat_at.find(v[1]);
        auto a = area_at.find(v[3]);
        if (c == cat_at.end() || a == area_at.end()) continue;
        Check k;
        k.cat = c->second, k.nome = v[2], k.area = a->second, k.era = atoi(v[4].c_str());
        k.x = (float)atof(v[5].c_str()), k.y = (float)atof(v[6].c_str());
        k.flag = (uint32_t)strtoul(v[7].c_str(), nullptr, 10);
        k.chave = strtoull(v[8].c_str(), nullptr, 16);
        checks.push_back(k);
    }
    logf("zelda: mapa F7: %zu areas, %zu categorias, %zu itens, %zu cenas; modo %s%s%s", areas.size(), cats.size(), checks.size(),
         bancs.size(), rando ? "RANDOMIZER" : "normal", seed.empty() ? "" : ", seed ", seed.c_str());
}

/// uma imagem de notas\ (rel sem extensao: "mapas\\X", "icones\\Y"), carregada na primeira vez (RGBA cru em zstd)
Tex* tex(const std::string& rel) {
    if (rel.empty() || rel.back() == '\\') return nullptr;
    Tex& t = texs[rel];
    if (t.tried) return t.t ? &t : nullptr;
    t.tried = true;
    std::vector<unsigned char> src;
    if (FILE* f = fopen((dir + "\\notas\\" + rel + ".rgba.zst").c_str(), "rb")) {
        fseek(f, 0, SEEK_END);
        const long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0 && n < (64 << 20)) {
            src.resize((size_t)n);
            if (fread(src.data(), 1, (size_t)n, f) != (size_t)n) src.clear();
        }
        fclose(f);
    }
    if (src.empty()) return nullptr;
    const unsigned long long n = ZSTD_getFrameContentSize(src.data(), src.size());
    if (n == ZSTD_CONTENTSIZE_ERROR || n == ZSTD_CONTENTSIZE_UNKNOWN || n < 12 || n > (64ull << 20)) return nullptr;
    std::vector<unsigned char> raw((size_t)n);
    const size_t got = ZSTD_decompress(raw.data(), raw.size(), src.data(), src.size());
    if (ZSTD_isError(got) || got != raw.size() || memcmp(raw.data(), "RGBA", 4)) return nullptr;
    uint32_t w = 0, h = 0;
    memcpy(&w, raw.data() + 4, 4);
    memcpy(&h, raw.data() + 8, 4);
    if (!w || !h || (uint64_t)w * h * 4 + 12 > raw.size()) return nullptr;
    t.t = IM_NEW(ImTextureData)();
    t.t->Create(ImTextureFormat_RGBA32, (int)w, (int)h);
    memcpy(t.t->GetPixels(), raw.data() + 12, (size_t)w * h * 4);
    t.t->UseColors = true;
    ImGui::RegisterUserTexture(t.t);
    t.w = (float)w, t.h = (float)h;
    return &t;
}

// ---- o save (progress.sav: tabela (hash, u32) com marcadores de secao hash 0; secao 0 Bool, 10 Vector3f (offset), 16
// String64 (offset), 32 a lista de chaves de 64 bits ligadas, ate um 0) ----
struct Save {
    bool ok = false;
    std::unordered_map<uint32_t, uint32_t> bools;
    std::unordered_map<uint32_t, std::string> textos;
    std::unordered_map<uint32_t, std::array<float, 3>> vets;
    std::unordered_set<uint64_t> chaves;
    std::string path;
    uint64_t when = 0;
};
Save save_now;
int64_t save_looked = 0;

void newest(const std::string& root, std::string* best, uint64_t* best_t) {
    WIN32_FIND_DATAA a;
    HANDLE ha = FindFirstFileA((root + "\\*").c_str(), &a);
    if (ha == INVALID_HANDLE_VALUE) return;
    do {
        if (!(a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || a.cFileName[0] == '.') continue;
        WIN32_FIND_DATAA b;
        const std::string uid = root + "\\" + a.cFileName;
        HANDLE hb = FindFirstFileA((uid + "\\slot_*").c_str(), &b);
        if (hb == INVALID_HANDLE_VALUE) continue;
        do {
            const std::string p = uid + "\\" + b.cFileName + "\\progress.sav";
            WIN32_FILE_ATTRIBUTE_DATA d;
            if (!GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &d)) continue;
            const uint64_t t = (uint64_t)d.ftLastWriteTime.dwHighDateTime << 32 | d.ftLastWriteTime.dwLowDateTime;
            if (t > *best_t) *best_t = t, *best = p;
        } while (FindNextFileA(hb, &b));
        FindClose(hb);
    } while (FindNextFileA(ha, &a));
    FindClose(ha);
}

void refresh_save(int64_t now) {
    if (save_looked && now - save_looked < 1000000000LL) return;
    save_looked = now;
    std::string p;
    uint64_t t = 0;
    newest(gdir + "\\user\\" + (rando ? "save_randomizer" : "save"), &p, &t);
    if (p.empty() || (p == save_now.path && t == save_now.when)) return;
    std::vector<unsigned char> d;
    if (FILE* f = fopen(p.c_str(), "rb")) {
        fseek(f, 0, SEEK_END);
        const long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0 && n < (32 << 20)) {
            d.resize((size_t)n);
            if (fread(d.data(), 1, (size_t)n, f) != (size_t)n) d.clear();
        }
        fclose(f);
    }
    auto r32 = [&](size_t o) { uint32_t v; memcpy(&v, d.data() + o, 4); return v; };
    if (d.size() < 0x30 || r32(0) != 0x01020304u) return;   // (meio escrito: a proxima olhada)
    Save s;
    s.path = p, s.when = t;
    const size_t end = std::min<size_t>(r32(8), d.size());
    int section = 0;
    for (size_t o = 0x28; o + 8 <= end; o += 8) {
        const uint32_t h = r32(o), v = r32(o + 4);
        if (!h) {
            section = (int)v;
            continue;
        }
        if (section == 0) s.bools[h] = v;
        else if (section == 16 && (size_t)v + 64 <= d.size()) s.textos[h] = std::string((const char*)d.data() + v, strnlen((const char*)d.data() + v, 64));
        else if (section == 10 && (size_t)v + 12 <= d.size()) {
            std::array<float, 3> f3;
            memcpy(f3.data(), d.data() + v, 12);
            s.vets[h] = f3;
        } else if (section == 32) {
            for (size_t q = v; q + 8 <= d.size(); q += 8) {
                uint64_t k;
                memcpy(&k, d.data() + q, 8);
                if (!k) break;
                s.chaves.insert(k);
            }
        }
    }
    s.ok = true;
    save_now = std::move(s);
}

// ---- ao vivo: o GameDataMgr do jogo (host/hos_main.c oot_gamedata_set_bool: +32 a tabela (hash, indice), +44 o seu
// tamanho, +24 os Bool de 24 bytes com o valor em +22, +16 quantos) ----
bool g_read(uint64_t a, void* out, size_t n) {
    if (!host || !host->guest_base || !host->guest_size) return false;
    const uint64_t size = host->guest_size();
    if (a < 0x100000 || a >= size || n > size - a) return false;
    memcpy(out, host->guest_base() + a, n);
    return true;
}
struct Live { uint64_t table = 0, bools = 0; uint32_t size = 0, count = 0; bool ok = false; };
Live live_now() {
    Live L;
    if (!live_link.load(std::memory_order_relaxed)) return L;
    uint64_t holder = 0, mgr = 0;
    if (!g_read(0x84c9e180ULL, &holder, 8) || !holder || !g_read(holder, &mgr, 8) || !mgr) return L;
    if (!g_read(mgr + 32, &L.table, 8) || !g_read(mgr + 44, &L.size, 4) || !g_read(mgr + 24, &L.bools, 8) ||
        !g_read(mgr + 16, &L.count, 4))
        return L;
    L.ok = L.table && L.bools && L.size && L.size < (1u << 24) && L.count && L.count < (1u << 24);
    return L;
}
int live_bool(const Live& L, uint32_t hash) {   // (-1: nao achou)
    if (!L.ok) return -1;
    for (uint32_t probe = 0, slot = hash % L.size; probe < L.size && probe < 8192; probe++, slot = (slot + 1) % L.size) {
        uint32_t kv[2];
        if (!g_read(L.table + slot * 8ULL, kv, 8) || !kv[0]) return -1;
        if (kv[0] != hash) continue;
        if (kv[1] >= L.count) return -1;
        uint8_t v = 0;
        return g_read(L.bools + kv[1] * 24ULL + 22, &v, 1) ? (v ? 1 : 0) : -1;
    }
    return -1;
}
bool got_flag(const Live& L, uint32_t flag) {
    const int v = live_bool(L, flag);
    if (v >= 0) return v != 0;
    auto it = save_now.bools.find(flag);
    return it != save_now.bools.end() && it->second;
}
bool taken(const Live& L, const Check& k) {
    if (k.flag) return got_flag(L, k.flag);
    return k.chave && save_now.chaves.count(k.chave);
}

// ---- onde o Link esta: a cena ao vivo (ou a do save), a idade ao vivo (ou a do save), a posicao ao vivo (ou a do save) ----
struct Where { int area = -1, age = 0; bool pos = false; float x = 0, z = 0; bool live = false; };
Where where(int link_age_now) {
    Where w;
    const int lb = live_banc.load(std::memory_order_relaxed);
    auto b = lb ? bancs.find((uint32_t)lb) : bancs.end();
    if (b != bancs.end()) {
        w.area = b->second.first, w.age = b->second.second, w.live = true;
        w.pos = true, w.x = live_x.load(std::memory_order_relaxed) / 16.0f, w.z = live_z.load(std::memory_order_relaxed) / 16.0f;
    } else if (save_now.ok) {
        auto t = save_now.textos.find(492931546u);   // (Sequence_CurrentBanc)
        if (t != save_now.textos.end()) {
            auto sb = bancs.find(zelda_fnv(t->second.c_str()));
            if (sb != bancs.end()) w.area = sb->second.first, w.age = sb->second.second;
        }
        auto v = save_now.vets.find(2517292394u);   // (Player_Pos_For_Map)
        if (v != save_now.vets.end()) w.pos = true, w.x = v->second[0], w.z = v->second[2];
    }
    if (link_age_now == 0 || link_age_now == 1) w.age = link_age_now;   // (ao vivo: Sequence_IsChildhood, do lado do jogo)
    else if (save_now.ok) {
        auto c = save_now.bools.find(3466731731u);   // (Sequence_IsChildhood)
        if (c != save_now.bools.end()) w.age = c->second ? 0 : 1;
    }
    return w;
}

// ---- o estado da pagina do mapa ----
int view_area = 0, view_age = 0, sel = -1, filter = 0;   // (filter: 0 todos, senao a categoria filter - 1)
std::vector<int> shown;   // (os itens da area e idade vistas, pelo filtro, da esquerda para a direita)

bool era_ok(const Check& k, int age) { return k.era == 0 || k.era == age + 1; }
const char* age_name(int a) { return a ? "Adulto" : "Criança"; }

void rebuild_shown() {
    shown.clear();
    for (int i = 0; i < (int)checks.size(); i++) {
        const Check& k = checks[i];
        if (k.area != view_area || !era_ok(k, view_age) || k.x < 0) continue;
        if (filter ? k.cat != filter - 1 : !cats[k.cat].visivel) continue;
        shown.push_back(i);
    }
    std::sort(shown.begin(), shown.end(), [](int a, int b) {
        return checks[a].x != checks[b].x ? checks[a].x < checks[b].x : checks[a].y < checks[b].y;
    });
    if (sel >= (int)shown.size()) sel = (int)shown.size() - 1;
}

void open_map(int link_age_now) {
    load();
    refresh_save(prof_now());
    const Where w = where(link_age_now);
    if (w.area >= 0) view_area = w.area;
    view_age = w.age;
    sel = -1;
    rebuild_shown();
}

/// o mapa a mostrar: o da idade, o outro se a area so tem um
std::string map_of(const Area& a, int age) {
    const std::string& m = !a.mapa[age].empty() ? a.mapa[age] : a.mapa[age ^ 1];
    return m.empty() ? "" : "mapas\\" + m;
}

void draw_map(ui::Ctx& c, int link_age_now) {
    const float s = c.s;
    load();
    refresh_save(prof_now());
    if (areas.empty()) {
        const float hh = title_banner(c, "Mapa do Zelda", nullptr);
        help_box(c, c.safe + 40 * s, c.w - c.safe - 40 * s, hh + 40 * s, "Sem dados do mapa",
                 "Rode E:\\Hookshot-Zelda\\ferramentas\\gerar_notas.py para criar a pasta zelda\\notas ao lado do jogo.", nullptr,
                 nullptr);
        const ui::Prompt p[] = {{ui::kBack, "Fechar"}};
        ui::prompt_bar(c, p, 1);
        return;
    }
    view_area = std::clamp(view_area, 0, (int)areas.size() - 1);
    const Area& A = areas[view_area];
    const Live L = live_now();
    const Where w = where(link_age_now);
    // (as contas da area: todas as idades, todos os itens rastreaveis)
    int got = 0, total = 0;
    for (const Check& k : checks)
        if (k.area == view_area && cats[k.cat].rastreavel) total++, got += taken(L, k);
    char right[64];
    snprintf(right, sizeof right, "%d de %d pegos", got, total);
    char title[160];
    snprintf(title, sizeof title, "%s · %s", A.nome.c_str(), A.masmorra ? "Masmorra" : age_name(view_age));
    const float hh = title_banner(c, title, right);
    const float bar = ui::prompt_bar_top(c);
    // (o mapa a esquerda, o painel a direita)
    const float px0 = c.w - c.safe - std::max(300 * s, c.w * 0.27f), lx0 = c.safe, lx1 = px0 - 20 * s;
    const float top = hh + 14 * s, bottom = bar - 12 * s;
    const float k = std::min((lx1 - lx0) / map_w, (bottom - top) / map_h);
    const ImVec2 m0(lx0 + (lx1 - lx0 - map_w * k) * 0.5f, top + (bottom - top - map_h * k) * 0.5f), m1 = m0 + ImVec2(map_w * k, map_h * k);
    ui::shadow(c, m0, m1, 16 * s, 0.7f, 3 * s);
    c.d->AddRectFilled(m0, m1, ui::kCellDark);
    if (Tex* t = tex(map_of(A, view_age))) c.d->AddImage(t->t->GetTexRef(), m0, m1);
    else ui::text_mid_centre(c, c.f.body, 18 * s, (m0.x + m1.x) * 0.5f, (m0.y + m1.y) * 0.5f, ui::kSoft, "Sem imagem deste mapa");
    c.d->AddRect(m0, m1, ui::kBoxEdge, 0, 2 * s);
    c.d->PushClipRect(m0, m1, true);
    auto at = [&](float x, float y) { return ImVec2(m0.x + x * k, m0.y + y * k); };
    const float r = std::clamp(11 * s * std::sqrt(k / s), 8 * s, 15 * s);   // (o raio dos icones: cresce pouco com o mapa)
    for (int j = 0; j < (int)shown.size(); j++) {
        const Check& ck = checks[shown[j]];
        const Cat& cat = cats[ck.cat];
        const bool have = cat.rastreavel && taken(L, ck), is_sel = j == sel;
        const ImVec2 p = at(ck.x, ck.y);
        const float rr = is_sel ? r * 1.35f : r;
        Tex* icon = have ? tex("icones\\" + cat.id + "_pego") : nullptr;
        const bool own_taken_icon = icon != nullptr;
        if (!icon) icon = tex("icones\\" + cat.id);
        if (is_sel) c.d->AddCircleFilled(p, rr * 1.25f, ui::rgba(255, 244, 220, 90));
        if (icon) {
            c.d->AddImage(icon->t->GetTexRef(), p - ImVec2(rr, rr), p + ImVec2(rr, rr), ImVec2(0, 0), ImVec2(1, 1),
                          have && !own_taken_icon ? ui::rgba(255, 255, 255, 110) : ui::rgba(255, 255, 255, 255));
        } else {
            c.d->AddCircleFilled(p, rr * 0.8f, have ? ui::rgba(70, 70, 66, 200) : cat.cor);
            c.d->AddCircle(p, rr * 0.8f, ui::rgba(20, 14, 8, 230), 0, 2 * s);
            ui::text_mid_centre(c, c.f.strong, rr, p.x, p.y, have ? cat.cor : ui::rgba(20, 14, 8), cat.letra.c_str());
        }
        if (have) {   // (o visto verde no canto)
            const ImVec2 q = p + ImVec2(rr * 0.6f, -rr * 0.6f);
            c.d->AddCircleFilled(q, rr * 0.38f, ui::rgba(61, 214, 92));
            c.d->AddLine(q + ImVec2(-rr * 0.18f, 0), q + ImVec2(-rr * 0.04f, rr * 0.15f), ui::rgba(16, 24, 16), 2 * s);
            c.d->AddLine(q + ImVec2(-rr * 0.04f, rr * 0.15f), q + ImVec2(rr * 0.2f, -rr * 0.17f), ui::rgba(16, 24, 16), 2 * s);
        }
        if (is_sel) c.d->AddCircle(p, rr * 1.1f, ui::kInkBright, 0, 2.5f * s);
        hits.add(p.x - rr, p.y - rr, p.x + rr, p.y + rr, menu_hit::kListLine, j);
    }
    // (o Link: na area dele e na idade vista)
    if (w.area == view_area && w.age == view_age && w.pos && A.altura > 0) {
        const float kk = map_h / A.altura;
        const ImVec2 p = at(map_w * 0.5f + (w.x - A.cx) * kk, map_h * 0.5f + (w.z - A.cz) * kk);
        const float lr = r * 1.6f;
        c.d->AddCircleFilled(p, lr * 1.15f, ui::rgba(255, 255, 255, 70));
        if (Tex* t = tex(w.age ? "icones\\link_adulto" : "icones\\link_crianca"))
            c.d->AddImage(t->t->GetTexRef(), p - ImVec2(lr, lr), p + ImVec2(lr, lr));
        else c.d->AddCircleFilled(p, lr * 0.7f, ui::rgba(60, 170, 70));
    }
    c.d->PopClipRect();
    // (o painel: a area, as contas por categoria, o item escolhido, o jogo todo)
    const ImVec2 b0(px0, top), b1(c.w - c.safe, bottom);
    ui::box(c, b0, b1);
    c.d->PushClipRect(b0, b1, true);
    float y = b0.y + 18 * s;
    const float x = b0.x + 20 * s, xr = b1.x - 20 * s;
    ui::text(c, c.f.heading, 20 * s, ImVec2(x, y), ui::kInkBright, A.nome.c_str());
    y += 28 * s;
    char line[200];
    snprintf(line, sizeof line, "%s%s", filter ? "Filtro: " : "Mostrando: ", filter ? cats[filter - 1].nome.c_str() : "todos os itens");
    ui::text(c, c.f.body, 15 * s, ImVec2(x, y), ui::kAccent, line);
    y += 26 * s;
    for (int ci = 0; ci < (int)cats.size(); ci++) {
        int g = 0, t = 0;
        for (const Check& ck : checks)
            if (ck.area == view_area && ck.cat == ci) t++, g += cats[ci].rastreavel && taken(L, ck);
        if (!t) continue;
        c.d->AddCircleFilled(ImVec2(x + 6 * s, y + 9 * s), 5 * s, cats[ci].cor);
        ui::text(c, c.f.body, 15.5f * s, ImVec2(x + 18 * s, y), filter == ci + 1 ? ui::kGoldHi : ui::kInk, cats[ci].nome.c_str());
        if (cats[ci].rastreavel) snprintf(line, sizeof line, "%d/%d", g, t);
        else snprintf(line, sizeof line, "%d", t);
        ui::text(c, c.f.strong, 15.5f * s, ImVec2(xr - ui::size_of(c.f.strong, 15.5f * s, line).x, y), ui::kInk, line);
        y += 22 * s;
    }
    y += 8 * s;
    c.d->AddLine(ImVec2(x, y), ImVec2(xr, y), ui::alpha(ui::kEdge, 0.8f), std::max(1.0f, s));
    y += 12 * s;
    if (sel >= 0 && sel < (int)shown.size()) {
        const Check& ck = checks[shown[sel]];
        const Cat& cat = cats[ck.cat];
        y += ui::para(c, c.f.strong, 17 * s, ImVec2(x, y), ui::kInkBright, ck.nome.c_str(), xr - x) + 6 * s;
        const char* st = !cat.rastreavel ? "O jogo não guarda no save" : taken(L, ck) ? "Já pego" : "Falta pegar";
        ui::text(c, c.f.body, 15.5f * s, ImVec2(x, y), !cat.rastreavel ? ui::kSoft : taken(L, ck) ? ui::rgba(61, 214, 92) : ui::kGoldHi, st);
        y += 22 * s;
        ui::text(c, c.f.body, 14.5f * s, ImVec2(x, y), ui::kDim, cat.nome.c_str());
        y += 28 * s;
    } else {
        y += ui::para(c, c.f.body, 15 * s, ImVec2(x, y), ui::kSoft, "Esquerda/Direita escolhe um item do mapa.", xr - x) + 16 * s;
    }
    // (o jogo todo: por categoria rastreavel)
    ui::text(c, c.f.heading, 17 * s, ImVec2(x, y), ui::kInk, "No jogo todo");
    y += 26 * s;
    for (int ci = 0; ci < (int)cats.size(); ci++) {
        if (!cats[ci].rastreavel || !cats[ci].visivel) continue;
        int g = 0, t = 0;
        for (const Check& ck : checks)
            if (ck.cat == ci) t++, g += taken(L, ck);
        snprintf(line, sizeof line, "%d/%d", g, t);
        ui::text(c, c.f.body, 14.5f * s, ImVec2(x + 4 * s, y), ui::kSoft, cats[ci].nome.c_str());
        ui::text(c, c.f.strong, 14.5f * s, ImVec2(xr - ui::size_of(c.f.strong, 14.5f * s, line).x, y), ui::kInk, line);
        y += 20 * s;
    }
    if (!listas.empty()) {   // (as listas: canções, medalhões...: quantos de cada)
        y += 6 * s;
        std::vector<std::pair<std::string, std::pair<int, int>>> per;
        for (const Lista& li : listas) {
            auto it = std::find_if(per.begin(), per.end(), [&](const auto& p) { return p.first == li.lista; });
            if (it == per.end()) per.push_back({li.lista, {0, 0}}), it = per.end() - 1;
            it->second.second++;
            it->second.first += got_flag(L, li.flag);
        }
        for (const auto& p : per) {
            snprintf(line, sizeof line, "%d/%d", p.second.first, p.second.second);
            ui::text(c, c.f.body, 14.5f * s, ImVec2(x + 4 * s, y), ui::kSoft, p.first.c_str());
            ui::text(c, c.f.strong, 14.5f * s, ImVec2(xr - ui::size_of(c.f.strong, 14.5f * s, line).x, y), ui::kInk, line);
            y += 20 * s;
        }
    }
    // (de onde vem: ao vivo ou o ultimo save)
    const char* fonte = L.ok ? "Ao vivo, do jogo (skulltulas e corações: do último autosave)"
                      : save_now.ok ? "Do último save/autosave" : "Sem save ainda";
    ui::para(c, c.f.body, 13.5f * s, ImVec2(x, b1.y - 44 * s), ui::kDim, fonte, xr - x);
    c.d->PopClipRect();
    const ui::Prompt p[] = {{ui::kTabs, "Área"}, {ui::kLeftRight, "Item"}, {ui::kUpDown, "Filtro"},
                            {ui::kConfirm, A.masmorra ? "Idade" : "Criança/Adulto"}, {ui::kBack, "Fechar"}};
    ui::prompt_bar(c, p, 5);
}

/// a pagina do mapa: as teclas e o mouse (menu_tick)
void tick_map(const bool* press, bool tab_left, bool tab_right, bool mouse, const MkwVkPointer& ptr, int mx, int my, bool& changed) {
    if (areas.empty()) return;
    if (mouse && (ptr.pressed & HOS_MOUSE_LEFT)) {
        const menu_hit::Hit hit = hits.at((float)mx, (float)my);
        if (hit.part == menu_hit::kListLine && hit.index >= 0 && hit.index < (int)shown.size()) sel = hit.index, changed = true;
    }
    if (mouse && ptr.wheel) {   // (a roda: a area)
        view_area = (view_area + (int)areas.size() + (ptr.wheel > 0 ? -1 : 1)) % (int)areas.size();
        sel = -1, rebuild_shown(), changed = true;
    }
    if (tab_left || tab_right) {
        view_area = (view_area + (int)areas.size() + (tab_left ? -1 : 1)) % (int)areas.size();
        sel = -1, rebuild_shown(), changed = true;
    }
    if (press[3] || press[4]) {
        const int n = (int)shown.size();
        if (n) sel = sel < 0 ? (press[4] ? 0 : n - 1) : (sel + n + (press[4] ? 1 : -1)) % n;
        changed = true;
    }
    if (press[1] || press[2]) {   // (o filtro: todos, depois cada categoria com itens nesta area)
        const int n = (int)cats.size() + 1;
        for (int step = 0; step < n; step++) {
            filter = (filter + n + (press[2] ? 1 : -1)) % n;
            if (!filter) break;
            bool any = false;
            for (const Check& ck : checks) any |= ck.area == view_area && ck.cat == filter - 1 && era_ok(ck, view_age);
            if (any) break;
        }
        sel = -1, rebuild_shown(), changed = true;
    }
    if (press[7]) view_age ^= 1, sel = -1, rebuild_shown(), changed = true;
}

// ---- o modo de jogo (page 7): Normal, Randomizer novo, Randomizer continuar ----
int mode_sel = 0;
std::atomic<int> gen_state{0};   // (0 parado, 1 gerando, 2 pronto (reinicia), 3 falhou)
std::string gen_error;
std::mutex gen_mu;

bool busy() { return gen_state.load() == 1 || gen_state.load() == 2; }
std::string lista_marcadas();   // (abaixo: o que embaralhar)
void ler_marcadas();
void open_mode() {
    load();
    seed = read_line(dir + "\\randomizer\\seed.txt");
    rando_save = any_save(gdir + "\\user\\save_randomizer");
    mode_sel = rando ? (seed.empty() ? 1 : 2) : 0;
    if (gen_state.load() == 3) gen_state = 0;
    ler_marcadas();
}
bool write_mode(const char* m) {
    CreateDirectoryA(dir.c_str(), nullptr);
    FILE* f = fopen((dir + "\\modo.txt").c_str(), "wb");
    if (!f) return false;
    fprintf(f, "%s\n", m);
    fclose(f);
    return true;
}
std::string stamp() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char b[32];
    snprintf(b, sizeof b, "%04d%02d%02d-%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return b;
}
void restart_game() {
    logf("zelda: reiniciando no modo %s", read_line(dir + "\\modo.txt").c_str());
    save();
    pipeline_cache_save(true);
    open_now = false, page = 0;
    if (const HWND wnd = app_window()) PostMessageA(wnd, WM_APP + 9, 0, 0);
}
std::string config(const char* key, const char* def) {
    std::string v = def;
    if (FILE* f = fopen((dir + "\\config.ini").c_str(), "rb")) {
        char l[1024];
        const size_t kn = strlen(key);
        while (fgets(l, sizeof l, f))
            if (!strncmp(l, key, kn) && l[kn] == '=') {
                v = l + kn + 1;
                while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == ' ')) v.pop_back();
            }
        fclose(f);
    }
    return v;
}
/// gera uma seed nova (o gerador em Python, ~10 s) e, se deu certo, poe no lugar, guarda o save randomizer antigo e reinicia
void generate() {
    int expected = 0;
    if (!gen_state.compare_exchange_strong(expected, 1)) return;
    std::thread([] {
        const std::string py = config("python", "python"), script = config("randomizer", "E:\\OoT-Notas\\randomizer\\randomizer.py");
        const std::string novo = dir + "\\randomizer.novo", log = dir + "\\gerar.log";
        std::error_code ec;
        std::filesystem::remove_all(novo, ec);
        std::string cmd = "\"" + py + "\" -X utf8 \"" + script + "\" --saida \"" + novo + "\" --embaralhar " + lista_marcadas();
        SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
        HANDLE out = CreateFileA(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        STARTUPINFOA si{};
        si.cb = sizeof si;
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = out;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION pi{};
        std::vector<char> line(cmd.begin(), cmd.end());
        line.push_back(0);
        SetEnvironmentVariableA("PYTHONIOENCODING", "utf-8");
        std::string err;
        const std::string script_dir = script.substr(0, script.find_last_of("\\/"));
        logf("zelda: gerando uma seed: %s", cmd.c_str());
        if (!CreateProcessA(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                            script_dir.empty() ? nullptr : script_dir.c_str(), &si, &pi)) {
            err = "Não deu para rodar o Python (" + py + "). Ajuste zelda\\config.ini.";
        } else {
            const DWORD w = WaitForSingleObject(pi.hProcess, 180000);
            DWORD code = 1;
            if (w == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1), err = "O gerador demorou demais (3 min).";
            else GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (err.empty() && code) err = "O gerador falhou (código " + std::to_string(code) + "). Veja zelda\\gerar.log.";
        }
        if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
        if (err.empty() && (!exists(novo + "\\romfs") || !exists(novo + "\\rsizetable.zs")))
            err = "O gerador não criou as cenas e a tabela de tamanhos. Veja zelda\\gerar.log.";
        if (err.empty()) {   // (a seed no lugar; a anterior guardada uma vez; o save randomizer antigo guardado com a data)
            const std::string cur = dir + "\\randomizer", old = dir + "\\randomizer.anterior";
            std::filesystem::remove_all(old, ec);
            if (exists(cur)) std::filesystem::rename(cur, old, ec);
            std::filesystem::rename(novo, cur, ec);
            if (ec) err = "Não deu para pôr a seed no lugar: " + ec.message();
        }
        if (err.empty()) {
            const std::string sr = gdir + "\\user\\save_randomizer";
            if (exists(sr)) std::filesystem::rename(sr, sr + ".backup-" + stamp(), ec);
            if (ec) err = "Não deu para guardar o save randomizer antigo: " + ec.message();
        }
        if (err.empty() && !write_mode("randomizer")) err = "Não deu para gravar zelda\\modo.txt.";
        if (!err.empty()) {
            logf("zelda: seed nao gerada: %s", err.c_str());
            std::lock_guard lk(gen_mu);
            gen_error = err;
            gen_state = 3;
            return;
        }
        logf("zelda: seed %s pronta", read_line(dir + "\\randomizer\\seed.txt").c_str());
        gen_state = 2;   // (menu_tick reinicia, na sua thread)
    }).detach();
}

// ---- o que embaralhar (como as opcoes do Ship of Harkinian): caixas de selecao, cada uma com a explicacao; vao para o
// gerador (--embaralhar). Baus: a cena (Banc); NPCs e musicas: o item que o fluxo de evento entrega; lojas: as vitrines ----
struct Opcao {
    const char* id;      // (o nome no gerador; vazio = ainda nao existe)
    const char* nome;
    const char* explica;
};
const Opcao opcoes[] = {
    {"masmorras", "Baús das masmorras",
     "Os 56 baús de dentro das masmorras, da Árvore Deku ao Castelo de Ganon. Chaves, mapas e bússolas ficam no lugar, para "
     "nenhuma porta travar o jogo."},
    {"mundo", "Baús do mundo",
     "Os 42 baús fora das masmorras: Floresta Kokiri, Campo de Hyrule, grutas, vilas, Fortaleza Gerudo, Colosso..."},
    {"lojas", "Lojas e vendedores",
     "O que cada loja vende muda de lugar (Kokiri, Mercado, Kakariko, Goron, Zora), com o preço junto. O Escudo Deku "
     "sempre fica numa loja de criança. As máscaras (empréstimo) ficam como estão."},
    {"npcs", "Itens de NPCs e quests",
     "As 6 Grandes Fadas (magias, magia, defesa), Darunia (Bracelete Goron), o Cartão Gerudo e as peças de coração do "
     "homem do telhado, do Skull Kid, dos sapos e do Dampé. Os itens deles podem cair em baús, e vice-versa."},
    {"musicas", "Músicas da ocarina",
     "As 12 músicas trocam de professor: a Impa pode ensinar o Bolero, o Sheik da cratera a Canção de Saria... A lição "
     "toca a música de antes, mas você ganha a nova. As músicas ficam só nos lugares de música."},
};
constexpr int kModes = 3, kOpcoes = (int)(sizeof opcoes / sizeof opcoes[0]), kRows = kModes + kOpcoes;
bool marcada[kOpcoes] = {true, true, true, true, true};
std::string embaralhar_seed;   // (o que a seed atual embaralhou: randomizer\embaralhar.txt)

bool na_lista(const std::string& lista, const char* id) {
    return *id && ("," + lista + ",").find(std::string(",") + id + ",") != std::string::npos;
}
void ler_marcadas() {   // (a ultima escolha: zelda\embaralhar.txt; a da seed atual: randomizer\embaralhar.txt)
    embaralhar_seed = read_line(dir + "\\randomizer\\embaralhar.txt");
    if (embaralhar_seed.empty() && !seed.empty()) embaralhar_seed = "masmorras,mundo";   // (as seeds de antes da escolha)
    const std::string v = read_line(dir + "\\embaralhar.txt");
    if (v.empty()) return;
    for (int i = 0; i < kOpcoes; i++) marcada[i] = na_lista(v, opcoes[i].id);
}
std::string lista_marcadas() {
    std::string v;
    for (int i = 0; i < kOpcoes; i++)
        if (marcada[i] && *opcoes[i].id) v += (v.empty() ? "" : ",") + std::string(opcoes[i].id);
    return v;
}
void gravar_marcadas() {
    if (FILE* f = fopen((dir + "\\embaralhar.txt").c_str(), "wb")) fprintf(f, "%s\n", lista_marcadas().c_str()), fclose(f);
}
std::string nomes_de(const std::string& lista) {   // ("masmorras,mundo" -> "baús das masmorras + baús do mundo")
    std::string out;
    for (const Opcao& o : opcoes)
        if (na_lista(lista, o.id)) out += (out.empty() ? "" : " + ") + std::string(o.nome);
    return out;
}

const char* mode_labels[kModes] = {"Normal", "Randomizer: novo jogo", "Randomizer: continuar"};
bool mode_enabled(int i) { return i != 2 || (!seed.empty() && exists(dir + "\\randomizer\\romfs")); }
std::string aviso;   // (uma mensagem curta: "marque pelo menos um")

void draw_mode(ui::Ctx& c) {
    const float s = c.s, W = 760 * s, H = 720 * s;
    const ImVec2 p((c.w - W) * 0.5f, std::max(c.safe, (c.h - H) * 0.5f)), q = p + ImVec2(W, H);
    ui::shadow(c, p, q, 30 * s, 1.0f, 8 * s);
    c.d->AddRectFilled(p, q, ui::rgba(36, 28, 19, 250));
    c.d->AddRect(p, q, ui::rgba(122, 98, 64, 230), 0, 2 * s);
    const float hb = 50 * s;
    c.d->AddRectFilledMultiColor(p, ImVec2(q.x, p.y + hb), ui::kBannerTop, ui::kBannerTop, ui::kBanner, ui::kBanner);
    c.d->AddRectFilled(ImVec2(p.x, p.y + hb - 3 * s), ImVec2(q.x, p.y + hb), ui::kBannerEdge);
    ui::text_mid_centre(c, c.f.heading, 23 * s, (p.x + q.x) * 0.5f, p.y + hb * 0.5f, ui::kBannerInk, "Modo de jogo");
    char now[200];
    snprintf(now, sizeof now, "Agora: %s%s%s", rando ? "RANDOMIZER" : "NORMAL", rando && !seed.empty() ? " · seed " : "",
             rando ? seed.c_str() : "");
    ui::text_mid(c, c.f.strong, 17 * s, p.x + 28 * s, p.y + hb + 22 * s, ui::kGoldHi, now);
    if (rando && !embaralhar_seed.empty())
        ui::text_mid(c, c.f.body, 14.5f * s, p.x + 28 * s, p.y + hb + 44 * s, ui::kSoft,
                     ("Embaralhado nesta seed: " + nomes_de(embaralhar_seed)).c_str());
    const float tw = W - 56 * s, bw = W - 120 * s, bh = 40 * s, x0 = p.x + 60 * s;
    float by = p.y + hb + 66 * s;
    for (int i = 0; i < kModes; i++) {
        const ImVec2 a(x0, by), b = a + ImVec2(bw, bh);
        char label[160];
        if (i == 2 && !seed.empty()) snprintf(label, sizeof label, "%s (seed %s)", mode_labels[i], seed.c_str());
        else snprintf(label, sizeof label, "%s", mode_labels[i]);
        if (mode_enabled(i)) ui::button(c, a, b, label, i == mode_sel && !busy());
        else {
            c.d->AddRectFilled(a, b, ui::rgba(40, 32, 22, 200));
            ui::text_mid_centre(c, c.f.body, 17 * s, (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, ui::kDim, "Randomizer: continuar (sem seed ainda)");
        }
        hits.add(a.x, a.y, b.x, b.y, menu_hit::kButton, i);
        by += bh + 10 * s;
    }
    // (as caixas: o que o "novo jogo" embaralha)
    by += 8 * s;
    ui::text_mid(c, c.f.strong, 16.5f * s, x0, by + 10 * s, ui::kAccent, "O que embaralhar no novo jogo");
    by += 26 * s;
    const float rh = 34 * s;
    for (int k = 0; k < kOpcoes; k++) {
        const int row = kModes + k;
        const bool pronto = *opcoes[k].id != 0, sel = row == mode_sel && !busy();
        const ImVec2 a(x0, by), b = a + ImVec2(bw, rh);
        if (sel) c.d->AddRectFilled(a, b, ui::rgba(92, 70, 38, 220), 3 * s), c.d->AddRect(a, b, ui::kGoldHi, 3 * s, 0, 1.5f * s);
        const float bx = a.x + 12 * s, bs = 20 * s, cy = (a.y + b.y) * 0.5f;
        const ImVec2 c0(bx, cy - bs * 0.5f), c1 = c0 + ImVec2(bs, bs);
        c.d->AddRectFilled(c0, c1, ui::rgba(22, 17, 11, 240), 2 * s);
        c.d->AddRect(c0, c1, pronto ? ui::kAccent : ui::kDim, 2 * s, 0, 1.5f * s);
        if (marcada[k] && pronto) {   // (o visto)
            const ImVec2 v[3] = {c0 + ImVec2(bs * 0.2f, bs * 0.52f), c0 + ImVec2(bs * 0.42f, bs * 0.75f), c0 + ImVec2(bs * 0.82f, bs * 0.25f)};
            c.d->AddPolyline(v, 3, ui::kGoldHi, 0, 3 * s);
        }
        ui::text_mid(c, c.f.body, 17 * s, bx + bs + 14 * s, cy, pronto ? (sel ? ui::kInkBright : ui::kInk) : ui::kDim, opcoes[k].nome);
        hits.add(a.x, a.y, b.x, b.y, menu_hit::kButton, row);
        by += rh + 4 * s;
    }
    // (a explicacao da linha escolhida, ou o estado da geracao)
    const int st = gen_state.load();
    std::string msg;
    ImU32 col = ui::kSoft;
    if (st == 1) msg = "Gerando a seed (" + nomes_de(lista_marcadas()) + ")... uns 20 a 40 segundos.";
    else if (st == 2) msg = "Seed pronta. Reiniciando o jogo...";
    else if (st == 3) {
        std::lock_guard lk(gen_mu);
        msg = gen_error, col = ui::rgba(255, 120, 100);
    } else if (!aviso.empty()) msg = aviso, col = ui::rgba(255, 190, 110);
    else if (mode_sel == 0) msg = "A campanha de sempre (saves em user\\save). Não muda nada nos arquivos do jogo.";
    else if (mode_sel == 1) {
        const std::string l = lista_marcadas();
        msg = l.empty() ? "Marque abaixo pelo menos uma opção." :
              "Gera uma seed nova com: " + nomes_de(l) + ". Só vale para um NOVO JOGO, com saves separados: a sua campanha "
              "normal fica intocada." + std::string(rando_save ? " O save randomizer atual é guardado numa pasta com a data." : "");
    } else if (mode_sel == 2) msg = "Volta para a seed atual e o save randomizer dela.";
    else msg = opcoes[mode_sel - kModes].explica;
    ui::para(c, c.f.body, 15.5f * s, ImVec2(p.x + 28 * s, by + 10 * s), col, msg.c_str(), tw);
    const ui::Prompt pr[] = {{ui::kUpDown, "Escolher"}, {ui::kConfirm, "Confirmar / marcar"}, {ui::kBack, "Voltar"}};
    ui::prompt_bar(c, pr, 3);
}

/// a pagina do modo: as teclas e o mouse (menu_tick)
void tick_mode(const bool* press, bool mouse, const MkwVkPointer& ptr, int mx, int my, bool& changed) {
    if (gen_state.load() == 2) {
        gen_state = 0;
        restart_game();
        changed = true;
        return;
    }
    if (busy()) return;   // (gerando: nada muda)
    auto usable = [](int r) { return r >= kModes || mode_enabled(r); };
    bool go = press[7];
    if (mouse) {
        const menu_hit::Hit hit = hits.at((float)mx, (float)my);
        if (hit.part == menu_hit::kButton && hit.index >= 0 && hit.index < kRows && usable(hit.index)) {
            if (hit.index != mode_sel && (ptr.pressed || ptr.held)) mode_sel = hit.index, aviso.clear(), changed = true;
            if (ptr.pressed & HOS_MOUSE_LEFT) mode_sel = hit.index, go = true;
        }
        if (ptr.pressed & HOS_MOUSE_RIGHT) open_now = false, page = 0, changed = true;
    }
    if (press[1] || press[2]) {
        const int dir_ = press[2] ? 1 : -1;
        for (int i = 0; i < kRows; i++) {
            mode_sel = (mode_sel + kRows + dir_) % kRows;
            if (usable(mode_sel)) break;
        }
        if (gen_state.load() == 3) gen_state = 0;
        aviso.clear(), changed = true;
    }
    if (press[5]) {
        open_now = false, page = 0, changed = true;
        return;
    }
    if (!go) return;
    changed = true;
    aviso.clear();
    if (mode_sel >= kModes) {   // (uma caixa)
        const int k = mode_sel - kModes;
        if (!*opcoes[k].id) aviso = "Esta opção ainda não está pronta.";
        else marcada[k] = !marcada[k], gravar_marcadas();
        return;
    }
    if (mode_sel == 0) {
        if (!rando) open_now = false, page = 0;   // (ja e o normal)
        else if (write_mode("normal")) restart_game();
    } else if (mode_sel == 2) {
        if (rando) open_now = false, page = 0;   // (ja e esta seed)
        else if (write_mode("randomizer")) restart_game();
    } else if (lista_marcadas().empty()) {
        aviso = "Marque pelo menos uma opção.";
    } else {
        generate();
    }
}

/// o selo do modo de jogo na tela de titulo (um menu do jogo na frente e o Link ainda nao apareceu)
bool badge_wanted() { return !live_link.load(std::memory_order_relaxed) && live_menu.load(std::memory_order_relaxed); }
void draw_badge(ui::Ctx& c, const char* pad_label) {
    load();
    const float s = c.s;
    char l1[160], l2[160];
    snprintf(l1, sizeof l1, "Modo: %s%s%s", rando ? "RANDOMIZER" : "NORMAL", rando && !seed.empty() ? " · seed " : "", rando ? seed.c_str() : "");
    if (pad_label && *pad_label) snprintf(l2, sizeof l2, "F7 ou %s: escolher o modo de jogo", pad_label);
    else snprintf(l2, sizeof l2, "F7: escolher o modo de jogo (Normal / Randomizer)");
    const float w = std::max(ui::size_of(c.f.strong, 17 * s, l1).x, ui::size_of(c.f.body, 14.5f * s, l2).x) + 36 * s;
    const ImVec2 a(c.safe + 8 * s, c.h - c.safe - 70 * s), b = a + ImVec2(w, 58 * s);
    ui::shadow(c, a, b, 12 * s, 0.6f, 3 * s);
    c.d->AddRectFilled(a, b, ui::rgba(30, 23, 15, 225), 4 * s);
    c.d->AddRect(a, b, rando ? ui::kGoldHi : ui::kBoxEdge, 4 * s, 1.5f * s);
    c.d->AddRectFilled(a, ImVec2(a.x + 5 * s, b.y), rando ? ui::kGoldHi : ui::kAccent, 4 * s);
    ui::text(c, c.f.strong, 17 * s, ImVec2(a.x + 18 * s, a.y + 9 * s), rando ? ui::kGoldHi : ui::kInkBright, l1);
    ui::text(c, c.f.body, 14.5f * s, ImVec2(a.x + 18 * s, a.y + 33 * s), ui::kSoft, l2);
}

// ---- o tracker de itens: uma coluna fina na esquerda da tela, ao vivo, com os icones do proprio Remake (zelda\notas\tracker,
// ferramentas\gerar_tracker.py). O que se tem aceso, o que falta apagado; municoes com a quantidade. As quantidades sao as da
// pagina de inventario do menu (item_counts: host/hos_main.c oot_inventory_refresh, 2x por segundo) ----
#define ZT_POUCH(label, name, kind) name,
#define ZT_NO_SECTION(title)
constexpr const char* trk_pouch[] = {OOT_CHEAT_ITEMS(ZT_NO_SECTION, ZT_POUCH)};
#undef ZT_POUCH
#undef ZT_NO_SECTION
std::atomic<int64_t> live_link_ns{0};   // (a ultima vez que o lado do jogo disse "o Link esta em jogo": ZELDA_ID_LINK)

int trk_count(const char* pouch) {   // (-1: o jogo ainda nao mandou)
    for (int i = 0; i < (int)(sizeof trk_pouch / sizeof trk_pouch[0]); i++)
        if (!strcmp(trk_pouch[i], pouch)) return item_counts[i].load(std::memory_order_relaxed);
    return -1;
}
struct TrkSlot {
    const char* niveis[3];   // (do primeiro ao ultimo: aparece o maior que se tem, senao o primeiro apagado)
    const char* icone;       // (um icone fixo no lugar do nivel: a bomba para as bolsas), ou nullptr
    const char* conta;       // (a municao mostrada no canto), ou nullptr
};
const TrkSlot trk_slots[] = {
    // itens
    {{"Weapon_Range_SlingShot"}, nullptr, "Item_DekuSeed"}, {{"Weapon_Range_Bow"}, nullptr, "Item_Arrow_Normal"},
    {{"Item_BombBag_Small", "Item_BombBag_Medium", "Item_BombBag_Large"}, "Item_Bomb", "Item_Bomb"},
    {{"Item_Bombchu"}, nullptr, "Item_Bombchu"}, {{"Item_Boomerang"}},
    {{"Item_Hookshot", "Item_Longshot"}}, {{"Item_Hammer"}}, {{"Item_GlassTruth"}},
    {{"Item_Ocarina_Fairy", "Item_Ocarina_Time"}}, {{"Item_Bottle"}, nullptr, "Item_Bottle"},
    {{"Item_DekuStick"}, nullptr, "Item_DekuStick"}, {{"Item_DekuNut"}, nullptr, "Item_DekuNut"},
    {{"Item_MagicBean"}, nullptr, "Item_MagicBean"}, {{"Item_DinFire"}}, {{"Item_FaroreWind"}},
    {{"Item_NayruLove"}}, {{"Item_Arrow_Fire"}}, {{"Item_Arrow_Ice"}}, {{"Item_Arrow_Light"}}, {{nullptr}},
    // equipamento
    {{"Weapon_Sword_Kokiri"}}, {{"Weapon_Sword_Master"}}, {{"Weapon_Sword_Giant", "Weapon_Sword_Biggoron"}},
    {{"Weapon_Shield_Deku"}}, {{"Weapon_Shield_Hylian"}}, {{"Weapon_Shield_Mirror"}}, {{"Armor_Adult_Goron"}},
    {{"Armor_Adult_Zora"}}, {{"Armor_Adult_HeavyBoot"}}, {{"Armor_Adult_HoverBoot"}},
    // upgrades
    {{"Item_Gauntlet_Goron", "Armor_Adult_Gauntlet_Silver", "Armor_Adult_Gauntlet_Gold"}},
    {{"Item_Scale_Silver", "Item_Scale_Gold"}}, {{"Item_Wallet_Medium", "Item_Wallet_Large"}},
    {{"Item_FairyBless_MP", "Item_FairyBless_MPDouble"}}, {{"Item_FairyBless_DefenseDouble"}},
    // pedras, medalhoes, o resto da colecao
    {{"Item_Stone_Forest"}}, {{"Item_Stone_Fire"}}, {{"Item_Stone_Water"}}, {{"Item_AgonyStone"}}, {{"Item_Token_Gerudo"}},
    {{"Item_Medal_Forest"}}, {{"Item_Medal_Fire"}}, {{"Item_Medal_Water"}}, {{"Item_Medal_Shadow"}}, {{"Item_Medal_Spirit"}},
    {{"Item_Medal_Light"}}, {{"Item_Token_Gold"}, nullptr, "Item_Token_Gold"}, {{nullptr}}, {{nullptr}}, {{nullptr}},
    // musicas
    {{"Item_Melody_ZeldaLullaby"}}, {{"Item_Melody_EponaSong"}}, {{"Item_Melody_SariaSong"}}, {{"Item_Melody_SunSong"}},
    {{"Item_Melody_TimeSong"}}, {{"Item_Melody_StormSong"}}, {{"Item_Melody_ForestMinuet"}}, {{"Item_Melody_FireBolero"}},
    {{"Item_Melody_WaterSerenade"}}, {{"Item_Melody_ShadowNocturne"}}, {{"Item_Melody_SpiritRequiem"}},
    {{"Item_Melody_LightPrelude"}},
};
// (onde cada grupo comeca: uma linha fina entre eles)
const int trk_grupos[] = {0, 20, 30, 35, 50, 62};

bool tracker_wanted(int64_t now) {
    return now - live_link_ns.load(std::memory_order_relaxed) < 1500000000LL && trk_count("Item_DekuSeed") >= 0;
}
void draw_tracker(ui::Ctx& c, int opacity_pct, int size_pct) {
    load();
    const float s = c.s * size_pct / 100.0f, op = std::clamp(opacity_pct, 10, 100) / 100.0f;
    const int cols = 5;
    const float cell = 46 * s, gap = 4 * s, pad = 8 * s, sep = 7 * s;
    const int n = (int)(sizeof trk_slots / sizeof trk_slots[0]);
    int rows = 0;
    for (int g = 0; g + 1 < (int)(sizeof trk_grupos / sizeof trk_grupos[0]); g++) rows += (trk_grupos[g + 1] - trk_grupos[g] + cols - 1) / cols;
    const float W = pad * 2 + cols * cell + (cols - 1) * gap;
    const float H = pad * 2 + rows * cell + (rows - 1) * gap + 4 * sep;
    const ImVec2 a(c.safe + 6 * c.s, std::max(c.safe + 60 * c.s, c.h * 0.2f)), b = a + ImVec2(W, H);
    auto mul = [&](ImU32 col, float k) { return (col & 0x00ffffffu) | ((ImU32)(((col >> 24) & 0xff) * k) << 24); };
    c.d->AddRectFilled(a, b, mul(ui::rgba(20, 15, 9, 200), op), 6 * s);
    c.d->AddRect(a, b, mul(ui::rgba(122, 98, 64, 200), op), 6 * s, 0, 1.2f * s);
    float y = a.y + pad;
    for (int g = 0; g + 1 < (int)(sizeof trk_grupos / sizeof trk_grupos[0]); g++) {
        if (g) {
            c.d->AddLine(ImVec2(a.x + pad, y + sep * 0.5f - gap * 0.5f), ImVec2(b.x - pad, y + sep * 0.5f - gap * 0.5f),
                         mul(ui::rgba(122, 98, 64, 160), op), 1 * s);
            y += sep;
        }
        for (int i = trk_grupos[g], k = 0; i < trk_grupos[g + 1] && i < n; i++, k++) {
            const TrkSlot& sl = trk_slots[i];
            if (k && k % cols == 0) y += cell + gap;
            if (!sl.niveis[0]) continue;
            int nivel = -1;
            for (int v = 0; v < 3 && sl.niveis[v]; v++)
                if (trk_count(sl.niveis[v]) > 0) nivel = v;
            const char* nome = sl.icone ? sl.icone : sl.niveis[std::max(nivel, 0)];
            const ImVec2 p(a.x + pad + (k % cols) * (cell + gap), y), q = p + ImVec2(cell, cell);
            if (Tex* t = tex(std::string("tracker\\") + nome)) {
                const ImU32 tint = nivel >= 0 ? IM_COL32(255, 255, 255, (int)(255 * op)) : IM_COL32(90, 90, 90, (int)(110 * op));
                c.d->AddImage(t->t->GetTexRef(), p, q, ImVec2(0, 0), ImVec2(1, 1), tint);
            }
            if (nivel >= 1 && sl.niveis[1] && !sl.icone) {   // (o nivel de um progressivo: 2, 3 no canto de cima)
                char nv[4];
                snprintf(nv, sizeof nv, "%d", nivel + 1);
                ui::text(c, c.f.strong, 13 * s, ImVec2(q.x - 10 * s, p.y - 1 * s), mul(ui::kGoldHi, op), nv);
            }
            if (sl.conta && nivel >= 0) {
                const int v = trk_count(sl.conta);
                if (v > 0) {
                    char num[8];
                    snprintf(num, sizeof num, "%d", v);
                    const ImVec2 ts = ui::size_of(c.f.strong, 15 * s, num);
                    const ImVec2 tp(q.x - ts.x - 1 * s, q.y - ts.y + 2 * s);
                    for (int dx = -1; dx <= 1; dx++)
                        for (int dy = -1; dy <= 1; dy++)
                            if (dx || dy) ui::text(c, c.f.strong, 15 * s, tp + ImVec2(dx * 1.2f * s, dy * 1.2f * s), mul(ui::rgba(0, 0, 0, 230), op), num);
                    ui::text(c, c.f.strong, 15 * s, tp, mul(ui::rgba(120, 255, 120), op), num);
                }
            }
        }
        y += cell + gap;
    }
}

}   // namespace zelda
