// teste: junta base/novo/nosso como o git merge-file -p --diff3 (novo = atual) e grava a saida
using System; using System.IO; using System.Text;
static class T { static int Main(string[] a) {
    var e = Encoding.GetEncoding(28591); int n;
    var r = Diff3.Merge(e.GetString(File.ReadAllBytes(a[1])), e.GetString(File.ReadAllBytes(a[0])), e.GetString(File.ReadAllBytes(a[2])), "novo", "base", "nosso", out n);
    File.WriteAllBytes(a[3], e.GetBytes(r)); return n; } }
