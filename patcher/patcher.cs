// Zelda para o Hookshot: o patch (mapa F7, Randomizer, tracker de itens, correr no B...) que se aplica sozinho a cada versão
// nova do Hookshot Builder. Leva dentro: fonte\ (os nossos arquivos), originais\ (os do builder na versão em que foram feitos)
// e notas\ (os dados do mapa e os ícones do tracker). O mesmo que aplicar.py, sem Python e sem Git.
//   Hookshot-Zelda-Patch.exe                     janela
//   Hookshot-Zelda-Patch.exe --aplicar [--jogo D:\hookshot] [--sem-recompilar]
//   Hookshot-Zelda-Patch.exe --remover [--jogo D:\hookshot] [--sem-recompilar]
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;

static class Patch
{
    public static readonly string Builder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HookshotBuilder");
    public static readonly string Estado = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HookshotZelda");
    static readonly string[] Novos = { "gpu/vk/zelda_ids.h", "gpu/vk/zelda_notas.h" };
    static readonly byte[] Marca = Encoding.ASCII.GetBytes("(Zelda: AchadoIA");
    static readonly Encoding L1 = Encoding.GetEncoding(28591);   // (bytes <-> texto sem perder nada)
    public static Action<string> Log = s => Console.WriteLine(s);

    // ---- o pacote dentro do exe ----
    static ZipArchive Pacote()
    {
        var s = Assembly.GetExecutingAssembly().GetManifestResourceStream("pacote.zip");
        if (s == null) throw new Exception("o exe não tem o pacote (pacote.zip)");
        return new ZipArchive(s, ZipArchiveMode.Read);
    }
    public static string IdDoPacote()
    {
        using (var z = Pacote()) { var e = z.GetEntry("pacote.txt"); using (var r = new StreamReader(e.Open())) return r.ReadToEnd().Trim(); }
    }
    // fonte\ e originais\ ficam em %LOCALAPPDATA%\HookshotZelda: uma junção com uma versão nova do builder atualiza as cópias de lá
    static void PrepararEstado()
    {
        string id = IdDoPacote(), marca = Path.Combine(Estado, "pacote.txt");
        if (File.Exists(marca) && File.ReadAllText(marca).Trim() == id) return;
        if (Directory.Exists(Path.Combine(Estado, "fonte"))) Directory.Delete(Path.Combine(Estado, "fonte"), true);
        if (Directory.Exists(Path.Combine(Estado, "originais"))) Directory.Delete(Path.Combine(Estado, "originais"), true);
        using (var z = Pacote())
            foreach (var e in z.Entries)
                if (e.FullName.StartsWith("fonte/") || e.FullName.StartsWith("originais/"))
                {
                    if (e.FullName.EndsWith("/")) continue;
                    var dst = Path.Combine(Estado, e.FullName.Replace('/', '\\'));
                    Directory.CreateDirectory(Path.GetDirectoryName(dst));
                    e.ExtractToFile(dst, true);
                }
        File.WriteAllText(marca, id + "\n");
    }

    // ---- onde está tudo ----
    public static string VersaoDoBuilder(out string tools)
    {
        string v = "";
        try { v = File.ReadAllText(Path.Combine(Builder, "versions", "current.txt")).Trim(); } catch { }
        if (v == "" && Directory.Exists(Path.Combine(Builder, "versions")))
        {
            var vs = Directory.GetDirectories(Path.Combine(Builder, "versions")).Select(Path.GetFileName).Where(d => d.StartsWith("v")).ToList();
            vs.Sort((x, y) => CompararVersao(y, x));
            if (vs.Count > 0) v = vs[0];
        }
        tools = Path.Combine(Builder, "versions", v, "HookshotBuilder", "tools");
        if (v == "" || !Directory.Exists(tools)) { tools = null; return null; }
        return v;
    }
    static int CompararVersao(string x, string y)
    {
        var a = x.TrimStart('v').Split('.'); var b = y.TrimStart('v').Split('.');
        for (int i = 0; i < Math.Max(a.Length, b.Length); i++)
        {
            int p = 0, q = 0;
            if (i < a.Length) int.TryParse(a[i], out p);
            if (i < b.Length) int.TryParse(b[i], out q);
            if (p != q) return p.CompareTo(q);
        }
        return 0;
    }
    public static string JogoDoBuilder()
    {
        try
        {
            var j = File.ReadAllText(Path.Combine(Builder, "builder-settings.json"));
            var m = Regex.Match(j, "\"Out\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\"");
            if (m.Success)
            {
                var o = m.Groups[1].Value.Replace("\\\\", "\\").Replace("\\/", "/");
                if (File.Exists(Path.Combine(o, "hookshot.exe"))) return o;
            }
        }
        catch { }
        return null;
    }
    public static string JogoDoExe(string exe)
    {
        var d = Path.GetDirectoryName(Path.GetFullPath(exe));
        return Path.GetFileName(d).Equals("bin", StringComparison.OrdinalIgnoreCase) ? Path.GetDirectoryName(d) : d;
    }

    // ---- a junção ----
    static List<string> Arquivos(string raiz)
    {
        return Directory.GetFiles(raiz, "*", SearchOption.AllDirectories)
            .Select(f => f.Substring(raiz.Length + 1).Replace('\\', '/')).Where(r => r != "VERSAO.txt").OrderBy(r => r, StringComparer.Ordinal).ToList();
    }
    static string P(string raiz, string rel) { return Path.Combine(raiz, rel.Replace('/', '\\')); }
    static bool Contem(byte[] h, byte[] n)
    {
        for (int i = 0; i + n.Length <= h.Length; i++)
        {
            int k = 0;
            while (k < n.Length && h[i + k] == n[k]) k++;
            if (k == n.Length) return true;
        }
        return false;
    }
    static string Lf(byte[] b) { return L1.GetString(b).Replace("\r\n", "\n"); }
    static string Selo()
    {
        using (var sha = SHA1.Create())
        {
            var ms = new MemoryStream();
            foreach (var r in Novos) { var b = File.ReadAllBytes(P(Path.Combine(Estado, "fonte"), r)); ms.Write(b, 0, b.Length); }
            var h = sha.ComputeHash(ms.ToArray());
            return BitConverter.ToString(h).Replace("-", "").ToLowerInvariant().Substring(0, 12);
        }
    }
    static string ComSelo(string rel, string dados, string s)
    {
        return rel.StartsWith("host/") && rel.EndsWith(".c") ? "/* (Zelda: AchadoIA) selo dos cabecalhos gpu/vk/zelda_*.h: " + s + " */\n" + dados : dados;
    }
    static string SemSelo(string dados)
    {
        if (dados.StartsWith("/* (Zelda: AchadoIA) selo")) { int i = dados.IndexOf('\n'); return i < 0 ? "" : dados.Substring(i + 1); }
        return dados;
    }

    // devolve false (e explica no log) se houve conflito: nesse caso nada foi tocado
    public static bool Juntar(string tools, string versao)
    {
        PrepararEstado();
        string orig = Path.Combine(Estado, "originais"), fonte = Path.Combine(Estado, "fonte"), conf = Path.Combine(Estado, "conflitos");
        string baseVer = File.Exists(Path.Combine(orig, "VERSAO.txt")) ? File.ReadAllText(Path.Combine(orig, "VERSAO.txt")).Trim() : "?";
        var planos = new List<Tuple<string, string, byte[]>>();
        var conflitos = new List<string>();
        var juntados = new List<string>();
        if (Directory.Exists(conf)) Directory.Delete(conf, true);
        foreach (var rel in Arquivos(orig))
        {
            var b = P(tools, rel);
            if (!File.Exists(b)) { conflitos.Add(rel + " (sumiu da versão nova do builder)"); continue; }
            var atual = File.ReadAllBytes(b);
            string norm = SemSelo(L1.GetString(atual)).Replace("\r\n", "\n"), o = Lf(File.ReadAllBytes(P(orig, rel))), f = Lf(File.ReadAllBytes(P(fonte, rel)));
            if (Contem(atual, Marca) || norm == o || norm == f) { planos.Add(Tuple.Create(rel, f, (byte[])null)); continue; }
            int n;
            var junto = Diff3.Merge(o, norm, f, "builder " + versao, "original " + baseVer, "Zelda", out n);
            if (n > 0)
            {
                var dst = P(conf, rel);
                Directory.CreateDirectory(Path.GetDirectoryName(dst));
                File.WriteAllBytes(dst, L1.GetBytes(junto));
                conflitos.Add(rel + " (" + n + " conflito(s))");
                continue;
            }
            planos.Add(Tuple.Create(rel, junto, atual));
            juntados.Add(rel);
        }
        if (conflitos.Count > 0)
        {
            Log("O builder " + versao + " mudou partes que o patch também muda (o patch foi feito na " + baseVer + "). Nada foi aplicado:");
            foreach (var c in conflitos) Log("  " + c);
            Log("Os arquivos com as marcas <<<<<<< ficaram em " + conf + " — mande para o Claude resolver.");
            return false;
        }
        string s = Selo();
        foreach (var p in planos)
        {
            File.WriteAllBytes(P(tools, p.Item1), L1.GetBytes(ComSelo(p.Item1, p.Item2, s)));
            if (p.Item3 != null)   // (a versão nova vira a base, e o juntado vira o nosso)
            {
                File.WriteAllBytes(P(orig, p.Item1), p.Item3);
                File.WriteAllBytes(P(fonte, p.Item1), L1.GetBytes(p.Item2));
            }
        }
        foreach (var r in Novos) File.Copy(P(fonte, r), P(tools, r), true);
        if (juntados.Count > 0)
        {
            File.WriteAllText(Path.Combine(orig, "VERSAO.txt"), versao + "\n");
            Log("Juntado com o builder " + versao + " (o patch era da " + baseVer + "): " + string.Join(", ", juntados));
        }
        Log("Aplicados " + (planos.Count + Novos.Length) + " arquivos no builder " + versao + ".");
        return true;
    }

    public static void Restaurar(string tools)
    {
        PrepararEstado();
        string orig = Path.Combine(Estado, "originais");
        int n = 0;
        foreach (var rel in Arquivos(orig))
        {
            var b = P(tools, rel);
            if (File.Exists(b) && Contem(File.ReadAllBytes(b), Marca)) { File.Copy(P(orig, rel), b, true); n++; }
        }
        foreach (var r in Novos) if (File.Exists(P(tools, r))) File.Delete(P(tools, r));
        Log("Builder restaurado (" + n + " arquivos voltaram ao original).");
    }

    // ---- os dados na pasta do jogo ----
    // o Python que vem com o Hookshot Builder (já tem zstandard): roda o randomizer e os geradores de mods, sem instalar nada
    static string PythonDoBuilder(string tools) { return tools == null ? null : Path.Combine(tools, "python", "python.exe"); }

    public static void DadosDoJogo(string jogo, string tools)
    {
        var z = Path.Combine(jogo, "zelda");
        Directory.CreateDirectory(z);
        if (!File.Exists(Path.Combine(z, "modo.txt"))) File.WriteAllText(Path.Combine(z, "modo.txt"), "normal\n");
        int n = 0, nc = 0, nm = 0;
        using (var pk = Pacote())
            foreach (var e in pk.Entries)
            {
                if (e.FullName.EndsWith("/")) continue;
                if (e.FullName.StartsWith("notas/"))
                {
                    var dst = Path.Combine(z, e.FullName.Replace('/', '\\'));
                    if (File.Exists(dst)) continue;   // (não mexe no que já está lá)
                    Directory.CreateDirectory(Path.GetDirectoryName(dst));
                    e.ExtractToFile(dst);
                    n++;
                }
                else if (e.FullName.StartsWith("codigo/"))   // (o código sempre na versão deste exe)
                {
                    var dst = Path.Combine(z, e.FullName.Replace('/', '\\'));
                    Directory.CreateDirectory(Path.GetDirectoryName(dst));
                    e.ExtractToFile(dst, true);
                    nc++;
                }
                else if (e.FullName.StartsWith("mods/"))      // (mods prontos: só os que faltam)
                {
                    var dst = Path.Combine(jogo, e.FullName.Replace('/', '\\'));
                    if (File.Exists(dst)) continue;
                    Directory.CreateDirectory(Path.GetDirectoryName(dst));
                    e.ExtractToFile(dst);
                    nm++;
                }
            }
        Log(n > 0 ? "Dados do mapa/tracker: " + n + " arquivos novos em " + z + "\\notas." : "Dados do mapa e do tracker já estão no jogo.");
        Log("Randomizer e geradores de mods: " + nc + " arquivos em " + z + "\\codigo.");
        Log(nm > 0 ? "Mods prontos: " + nm + " arquivos novos em " + Path.Combine(jogo, "mods") + "." : "Os mods prontos já estão no jogo.");
        AjustarConfig(z, tools);
    }

    // config.ini: o que não existe neste PC passa a apontar para o Python do builder e para o randomizer de zelda\codigo
    static void AjustarConfig(string z, string tools)
    {
        var p = Path.Combine(z, "config.ini");
        var linhas = File.Exists(p) ? File.ReadAllLines(p).ToList() : new List<string> { "correr_r=1" };
        string py = PythonDoBuilder(tools), rando = Path.Combine(z, "codigo", "randomizer", "randomizer.py");
        Func<string, string> valor = k => { var l = linhas.FirstOrDefault(x => x.StartsWith(k + "=")); return l == null ? null : l.Substring(k.Length + 1).Trim(); };
        Action<string, string> por = (k, v) => { linhas.RemoveAll(x => x.StartsWith(k + "=")); linhas.Insert(0, k + "=" + v); };
        var atual = valor("randomizer");
        if (atual == null || !File.Exists(atual)) { por("randomizer", rando); Log("config.ini: randomizer = " + rando); }
        atual = valor("python");
        if ((atual == null || !File.Exists(atual)) && py != null && File.Exists(py)) { por("python", py); Log("config.ini: python = " + py); }
        File.WriteAllLines(p, linhas);
    }

    // os mods gerados no PC a partir do jogo instalado (Dark Link: modelo, escudo, espada, aura + tabela de tamanhos);
    // depois da recompilação, que refaz prompts\rsizetable.zs
    public static void ModsGerados(string jogo, string tools)
    {
        var py = PythonDoBuilder(tools);
        var script = Path.Combine(jogo, "zelda", "codigo", "mods", "dark_link.py");
        if (py == null || !File.Exists(py) || !File.Exists(script)) { Log("Mod do Dark Link: sem o Python do builder ou o script, pulei."); return; }
        var psi = new ProcessStartInfo(py, "-I -u -X utf8 \"" + script + "\" --jogo \"" + jogo + "\"")
        {
            UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8,
        };
        Log("Gerando o mod do Dark Link...");
        using (var pr = new Process { StartInfo = psi })
        {
            pr.OutputDataReceived += (s, e) => { if (e.Data != null) Log("  " + e.Data); };
            pr.ErrorDataReceived += (s, e) => { if (e.Data != null) Log("  " + e.Data); };
            pr.Start(); pr.BeginOutputReadLine(); pr.BeginErrorReadLine();
            pr.WaitForExit();
            Log(pr.ExitCode == 0 ? "Mod do Dark Link pronto." : "O mod do Dark Link falhou (código " + pr.ExitCode + "); o resto do patch está ok.");
        }
    }
    static string AcharNoPath(string exe)
    {
        foreach (var d in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(';'))
        {
            try
            {
                var p = Path.Combine(d.Trim(), exe);
                if (d.Trim() != "" && File.Exists(p) && !p.Contains("WindowsApps")) return p;
            }
            catch { }
        }
        return null;
    }

    public static void Backup(string jogo)
    {
        var dst = Path.Combine(jogo, "_backup-antes-zelda");
        if (Directory.Exists(dst)) return;
        Directory.CreateDirectory(dst);
        foreach (var f in new[] { "hookshot.exe", "hookshot_vk.dll" })
        {
            var p = Path.Combine(jogo, "bin", f);
            if (File.Exists(p)) File.Copy(p, Path.Combine(dst, f));
        }
        Log("Backup do exe e da dll de antes em " + dst);
    }

    public static bool JogoAberto() { return Process.GetProcessesByName("hookshot").Length > 0; }

    public static int Recompilar(string tools, string jogo)
    {
        var py = Path.Combine(tools, "python", "python.exe");
        var log = Path.Combine(Estado, "build.log");
        var psi = new ProcessStartInfo(py, "-I -u -X utf8 \"" + Path.Combine(tools, "nx2", "make_game.py") + "\" --update \"" + jogo + "\" --log \"" + log + "\"")
        {
            WorkingDirectory = tools, UseShellExecute = false, CreateNoWindow = true,
            RedirectStandardOutput = true, RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8,
        };
        Log("Recompilando o jogo (uns 11 minutos; o log completo fica em " + log + ")...");
        var t = Stopwatch.StartNew();
        using (var p = new Process { StartInfo = psi })
        {
            p.OutputDataReceived += (s, e) => { if (e.Data != null) Log(e.Data); };
            p.ErrorDataReceived += (s, e) => { if (e.Data != null) Log(e.Data); };
            p.Start(); p.BeginOutputReadLine(); p.BeginErrorReadLine();
            p.WaitForExit();
            Log(string.Format("Fim em {0:0.0} min, código {1}.", t.Elapsed.TotalMinutes, p.ExitCode));
            return p.ExitCode;
        }
    }

    // o fluxo completo; devolve 0 = ok
    public static int Rodar(bool remover, string jogo, bool recompilar)
    {
        string tools, versao = VersaoDoBuilder(out tools);
        if (versao == null) { Log("Não achei o Hookshot Builder em " + Builder + "."); return 2; }
        if (jogo == null || !File.Exists(Path.Combine(jogo, "hookshot.exe"))) { Log("Escolha a pasta do jogo (o hookshot.exe)."); return 2; }
        Log("Builder " + versao + ": " + tools);
        Log("Jogo: " + jogo);
        if (recompilar && JogoAberto()) { Log("Feche o jogo (hookshot.exe) antes: ele não pode estar aberto enquanto recompila."); return 3; }
        if (remover) Restaurar(tools);
        else
        {
            if (!Juntar(tools, versao)) return 4;
            DadosDoJogo(jogo, tools);
        }
        if (!recompilar) { if (!remover) ModsGerados(jogo, tools); Log("Pronto (sem recompilar)."); return 0; }
        Backup(jogo);
        int r = Recompilar(tools, jogo);
        if (r == 0 && !remover) ModsGerados(jogo, tools);
        Log(r == 0 ? (remover ? "Pronto: o jogo voltou a ser o Hookshot original." : "Pronto! Pode abrir o jogo.") : "A recompilação falhou (veja o log acima).");
        return r == 0 ? 0 : 5;
    }
}

class Janela : Form
{
    readonly Label lBuilder = new Label(), lJogo = new Label(), lStatus = new Label();
    readonly NeoButton bAplicar = new NeoButton { Text = "Aplicar patch", Fill = Neo.Yellow };
    readonly NeoButton bRemover = new NeoButton { Text = "Remover", Fill = Neo.Pink };
    readonly NeoButton bTrocar = new NeoButton { Text = "Trocar...", Fill = Neo.Cyan };
    readonly NeoCheck cRecompilar = new NeoCheck { Text = "Recompilar o jogo agora (uns 11 minutos)", Checked = true };
    readonly TextBox log = new TextBox();
    string jogo;

    public Janela()
    {
        Text = "Zelda para o Hookshot — patch";
        try { Icon = new Icon(Assembly.GetExecutingAssembly().GetManifestResourceStream("achadoia.ico")); } catch { }
        BackColor = Neo.Bg; ForeColor = Neo.Ink; Font = Neo.Text(14);
        ClientSize = new Size(760, 640); FormBorderStyle = FormBorderStyle.FixedSingle; MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen; DoubleBuffered = true;

        foreach (var l in new[] { lBuilder, lJogo, lStatus }) { l.AutoSize = false; l.BackColor = Neo.White; l.ForeColor = Neo.Ink; Controls.Add(l); }
        lBuilder.Font = Neo.Bold(14); lJogo.Font = Neo.Bold(14);
        lBuilder.SetBounds(40, 150, 540, 22); lJogo.SetBounds(40, 180, 540, 22);
        bTrocar.SetBounds(600, 160, 120, 44); bTrocar.Font = Neo.Black(13);
        cRecompilar.SetBounds(30, 236, 460, 30); cRecompilar.BackColor = Neo.Bg;
        bAplicar.SetBounds(30, 280, 300, 56); bAplicar.Font = Neo.Black(18);
        bRemover.SetBounds(350, 280, 160, 56); bRemover.Font = Neo.Black(16);
        lStatus.SetBounds(30, 350, 700, 24); lStatus.BackColor = Neo.Bg; lStatus.Font = Neo.Bold(15);
        log.Multiline = true; log.ReadOnly = true; log.ScrollBars = ScrollBars.Vertical; log.BorderStyle = BorderStyle.None;
        log.BackColor = Neo.Ink; log.ForeColor = Color.FromArgb(230, 228, 220); log.Font = Neo.Mono(12); log.WordWrap = true;
        log.SetBounds(36, 388, 688, 220);
        Controls.AddRange(new Control[] { bTrocar, cRecompilar, bAplicar, bRemover, log });

        bTrocar.Click += (s, e) => Escolher();
        bAplicar.Click += (s, e) => Iniciar(false);
        bRemover.Click += (s, e) => Iniciar(true);
        Patch.Log = Escrever;

        string tools, v = Patch.VersaoDoBuilder(out tools);
        lBuilder.Text = v != null ? "Hookshot Builder " + v : "Hookshot Builder não encontrado";
        jogo = Patch.JogoDoBuilder();
        MostrarJogo();
        string pacote = "";
        try { pacote = Patch.IdDoPacote(); } catch (Exception ex) { pacote = ex.Message; }
        Escrever("Patch " + pacote + ". Leva: mapa F7 (MapGenie), Randomizer na tela inicial, tracker de itens, correr segurando B, " +
                 "cutscenes puláveis no Randomizer, Bracelete Goron junto da bolsa de bombas.");
        Escrever("Rode de novo sempre que o Hookshot Builder atualizar: o patch se junta sozinho com a versão nova.");
        if (v == null) { bAplicar.Enabled = bRemover.Enabled = false; Status("Instale/abra o Hookshot Builder primeiro.", Neo.Pink); }
        else Status("Pronto para aplicar.", Neo.Ink);
    }

    void MostrarJogo()
    {
        lJogo.Text = jogo != null ? "Jogo: " + jogo : "Jogo: (escolha o hookshot.exe)";
    }
    void Escolher()
    {
        using (var d = new OpenFileDialog { Title = "Onde está o hookshot.exe?", Filter = "Hookshot (hookshot.exe)|hookshot.exe|Programas (*.exe)|*.exe" })
        {
            if (jogo != null) d.InitialDirectory = jogo;
            if (d.ShowDialog(this) != DialogResult.OK) return;
            var j = Patch.JogoDoExe(d.FileName);
            if (!File.Exists(Path.Combine(j, "hookshot.exe"))) { Status("Essa pasta não tem o hookshot.exe.", Neo.Pink); return; }
            jogo = j; MostrarJogo();
        }
    }

    void Iniciar(bool remover)
    {
        if (jogo == null) { Escolher(); if (jogo == null) return; }
        if (remover && MessageBox.Show(this, "Tirar o patch do Hookshot Builder" + (cRecompilar.Checked ? " e recompilar o jogo sem ele" : "") + "?",
                                       Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        bAplicar.Enabled = bRemover.Enabled = bTrocar.Enabled = cRecompilar.Enabled = false;
        bool rec = cRecompilar.Checked;
        Status(rec ? "Trabalhando... (a recompilação leva uns 11 minutos; não feche esta janela)" : "Trabalhando...", Neo.Ink);
        var th = new Thread(() =>
        {
            int r;
            try { r = Patch.Rodar(remover, jogo, rec); }
            catch (Exception ex) { Escrever("Erro: " + ex.Message); r = 9; }
            BeginInvoke((Action)(() =>
            {
                bAplicar.Enabled = bRemover.Enabled = bTrocar.Enabled = cRecompilar.Enabled = true;
                if (r == 0) Status(remover ? "Patch removido." : (rec ? "Pronto! Pode abrir o jogo." : "Aplicado (falta recompilar)."), Color.FromArgb(30, 120, 40));
                else if (r == 3) Status("Feche o jogo e tente de novo.", Neo.Pink);
                else if (r == 4) Status("Conflito com a versão nova do builder — veja o log.", Neo.Pink);
                else Status("Não deu certo — veja o log.", Neo.Pink);
            }));
        }) { IsBackground = true };
        th.Start();
    }

    void Status(string s, Color c) { lStatus.Text = s; lStatus.ForeColor = c; }
    void Escrever(string s)
    {
        if (InvokeRequired) { BeginInvoke((Action<string>)Escrever, s); return; }
        if (log.TextLength > 300000) log.Text = log.Text.Substring(log.TextLength - 150000);
        log.AppendText(s + "\r\n");
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        Neo.Box(g, new Rectangle(24, 20, 700, 100), Neo.Yellow);
        Neo.Label(g, "ZELDA PARA O HOOKSHOT", Neo.Black(32), Neo.Ink, new Rectangle(44, 30, 660, 50));
        Neo.Label(g, "Mapa F7 · Randomizer · Tracker · Correr no B  —  Mod by @AchadoIA", Neo.Bold(15), Neo.Ink, new Rectangle(46, 78, 660, 28));
        Neo.Box(g, new Rectangle(24, 138, 572, 76), Neo.White);
        Neo.Box(g, new Rectangle(24, 380, 704, 236), Neo.Ink, Neo.Shadow, Neo.Border);
    }
}

static class Programa
{
    [STAThread]
    static int Main(string[] args)
    {
        bool aplicar = args.Contains("--aplicar"), remover = args.Contains("--remover");
        if (aplicar || remover)
        {
            string jogo = null;
            int i = Array.IndexOf(args, "--jogo");
            if (i >= 0 && i + 1 < args.Length) jogo = args[i + 1];
            if (jogo == null) jogo = Patch.JogoDoBuilder();
            var logf = Path.Combine(Patch.Estado, "patch.log");
            Directory.CreateDirectory(Patch.Estado);
            using (var w = new StreamWriter(logf, false, Encoding.UTF8) { AutoFlush = true })
            {
                var lk = new object();
                Patch.Log = s => { lock (lk) { w.WriteLine(s); Console.WriteLine(s); } };
                try { return Patch.Rodar(remover, jogo, !args.Contains("--sem-recompilar")); }
                catch (Exception ex) { Patch.Log("Erro: " + ex); return 9; }
            }
        }
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new Janela());
        return 0;
    }
}
