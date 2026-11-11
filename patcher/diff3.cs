// Junção em 3 vias (como o git merge-file --diff3), sem precisar do git: diff de Myers por linhas + agrupamento das mudanças
// dos dois lados. Mudanças que se sobrepõem (ou encostam) nos dois lados viram conflito, a não ser que sejam iguais.
using System;
using System.Collections.Generic;
using System.Text;

static class Diff3
{
    public struct Hunk { public int bs, be, os, oe, side; }   // base[bs,be) vira outro[os,oe)

    // os pedaços onde b difere de a
    public static List<Hunk> Diff(int[] a, int[] b, int side)
    {
        int n = a.Length, m = b.Length, pre = 0, suf = 0;
        while (pre < n && pre < m && a[pre] == b[pre]) pre++;
        while (suf < n - pre && suf < m - pre && a[n - 1 - suf] == b[m - 1 - suf]) suf++;
        int N = n - pre - suf, M = m - pre - suf, max = N + M, off = max + 1;
        var V = new int[2 * max + 3];
        var trace = new List<int[]>();
        int D = -1;
        for (int d = 0; d <= max && D < 0; d++)
        {
            for (int k = -d; k <= d; k += 2)
            {
                int x = (k == -d || (k != d && V[off + k - 1] < V[off + k + 1])) ? V[off + k + 1] : V[off + k - 1] + 1;
                int y = x - k;
                while (x < N && y < M && a[pre + x] == b[pre + y]) { x++; y++; }
                V[off + k] = x;
                if (x >= N && y >= M) { D = d; break; }
            }
            var s = new int[2 * d + 1];
            Array.Copy(V, off - d, s, 0, 2 * d + 1);
            trace.Add(s);
        }
        // volta pelo caminho juntando os pares iguais
        var pares = new List<KeyValuePair<int, int>>();
        int cx = N, cy = M;
        for (int d = D; d > 0; d--)
        {
            int[] s = trace[d - 1];
            int k = cx - cy;
            int pk = (k == -d || (k != d && s[k - 1 + d - 1] < s[k + 1 + d - 1])) ? k + 1 : k - 1;
            int px = s[pk + d - 1], py = px - pk;
            int sx = pk == k + 1 ? px : px + 1, sy = sx - k;   // (onde a cobra do passo d começa)
            while (cx > sx && cy > sy) { cx--; cy--; pares.Add(new KeyValuePair<int, int>(cx, cy)); }
            cx = px; cy = py;
        }
        while (cx > 0 && cy > 0) { cx--; cy--; pares.Add(new KeyValuePair<int, int>(cx, cy)); }
        pares.Reverse();
        var hs = new List<Hunk>();
        int ai = 0, bi = 0;
        foreach (var p in pares)
        {
            if (p.Key > ai || p.Value > bi) hs.Add(new Hunk { bs = pre + ai, be = pre + p.Key, os = pre + bi, oe = pre + p.Value, side = side });
            ai = p.Key + 1; bi = p.Value + 1;
        }
        if (ai < N || bi < M) hs.Add(new Hunk { bs = pre + ai, be = pre + N, os = pre + bi, oe = pre + M, side = side });
        return hs;
    }

    // a = o novo (builder), b = o nosso; devolve o texto juntado e quantos conflitos (marcados como no diff3 do git)
    public static string Merge(string baseText, string aText, string bText, string la, string lbase, string lb, out int conflitos)
    {
        var ids = new Dictionary<string, int>();
        Func<string, string[]> split = t => t.Split('\n');
        string[] O = split(baseText), A = split(aText), B = split(bText);
        Func<string[], int[]> num = ls =>
        {
            var r = new int[ls.Length];
            for (int j = 0; j < ls.Length; j++) { int v; if (!ids.TryGetValue(ls[j], out v)) { v = ids.Count; ids[ls[j]] = v; } r[j] = v; }
            return r;
        };
        int[] o = num(O), a = num(A), b = num(B);
        var all = Diff(o, a, 0);
        all.AddRange(Diff(o, b, 1));
        all.Sort((x, y) => x.bs != y.bs ? x.bs.CompareTo(y.bs) : (x.be != y.be ? x.be.CompareTo(y.be) : x.side.CompareTo(y.side)));
        var outp = new List<string>();
        conflitos = 0;
        int pos = 0, i = 0;
        while (i < all.Count)
        {
            var g = new List<Hunk> { all[i] };
            int gs = all[i].bs, ge = all[i].be;
            i++;
            while (i < all.Count && all[i].bs <= ge) { g.Add(all[i]); ge = Math.Max(ge, all[i].be); i++; }
            for (int p = pos; p < gs; p++) outp.Add(O[p]);
            bool temA = false, temB = false;
            foreach (var h in g) { if (h.side == 0) temA = true; else temB = true; }
            var va = Versao(O, A, g, 0, gs, ge);
            if (!temB) outp.AddRange(va);
            else
            {
                var vb = Versao(O, B, g, 1, gs, ge);
                if (!temA) outp.AddRange(vb);
                else if (Iguais(va, vb)) outp.AddRange(va);
                else
                {
                    conflitos++;
                    outp.Add("<<<<<<< " + la); outp.AddRange(va);
                    outp.Add("||||||| " + lbase); for (int p = gs; p < ge; p++) outp.Add(O[p]);
                    outp.Add("======="); outp.AddRange(vb);
                    outp.Add(">>>>>>> " + lb);
                }
            }
            pos = ge;
        }
        for (int p = pos; p < O.Length; p++) outp.Add(O[p]);
        return string.Join("\n", outp);
    }

    static List<string> Versao(string[] O, string[] X, List<Hunk> g, int side, int gs, int ge)
    {
        var r = new List<string>();
        int p = gs;
        foreach (var h in g)
        {
            if (h.side != side) continue;
            for (; p < h.bs; p++) r.Add(O[p]);
            for (int q = h.os; q < h.oe; q++) r.Add(X[q]);
            p = h.be;
        }
        for (; p < ge; p++) r.Add(O[p]);
        return r;
    }

    static bool Iguais(List<string> x, List<string> y)
    {
        if (x.Count != y.Count) return false;
        for (int i = 0; i < x.Count; i++) if (x[i] != y[i]) return false;
        return true;
    }
}
