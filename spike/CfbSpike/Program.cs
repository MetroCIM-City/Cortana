using System.IO.Compression;
using System.Text;
using OpenMcdf;

var outDir = args.Length > 0 ? args[0] : Path.Combine(Path.GetTempPath(), "rvtfileinfo-spike");
if (args.Length >= 2 && args[0] == "streams")
{
    foreach (var name in ListNames(args[1])) Console.WriteLine(name);
    return 0;
}
Directory.CreateDirectory(outDir);
var report = new StringBuilder();

var synthetic = Path.Combine(outDir, "synthetic.cfb");
var payload = """{"schema":1,"discipline":"Ää Öö العربية 中文 😀","location":"L01","originator":"ABC","subDiscipline":"Facade","documentType":"Model","program":"Housing","subProgram":"Phase1","modifiedUtc":"2026-01-01T00:00:00Z","source":"spike"}""";
WritePayload(synthetic, payload, extraStream: true);
var readBack = ReadPayload(synthetic);
var g1 = readBack == payload;
report.AppendLine($"G1 synthetic unicode round-trip: {(g1 ? "PASS" : "FAIL")}");

var copied = Path.Combine(outDir, "copied.cfb");
File.Copy(synthetic, copied, true);
var renamed = Path.Combine(outDir, "renamed.cfb");
File.Move(copied, renamed, true);
var zipPath = Path.Combine(outDir, "packed.zip");
if (File.Exists(zipPath)) File.Delete(zipPath);
using (var zip = ZipFile.Open(zipPath, ZipArchiveMode.Create))
{
    zip.CreateEntryFromFile(renamed, "renamed.cfb");
}
var unzipped = Path.Combine(outDir, "unzipped");
if (Directory.Exists(unzipped)) Directory.Delete(unzipped, true);
ZipFile.ExtractToDirectory(zipPath, unzipped);
var g6 = ReadPayload(Path.Combine(unzipped, "renamed.cfb")) == payload;
report.AppendLine($"G6 copy/rename/zip: {(g6 ? "PASS" : "FAIL")}");

var sample = args.Length > 1 ? args[1] : "";
if (!string.IsNullOrWhiteSpace(sample) && File.Exists(sample))
{
    var clone = Path.Combine(outDir, "sample-copy.rvt");
    File.Copy(sample, clone, true);
    var before = ListNames(clone);
    WritePayload(clone, payload, extraStream: false);
    var afterWrite = ReadPayload(clone) == payload && before.All(name => ListNames(clone).Contains(name));
    var sampleZip = Path.Combine(outDir, "sample.zip");
    if (File.Exists(sampleZip)) File.Delete(sampleZip);
    using (var zip = ZipFile.Open(sampleZip, ZipArchiveMode.Create)) zip.CreateEntryFromFile(clone, "sample.rvt");
    var sampleOut = Path.Combine(outDir, "sample-unzipped");
    if (Directory.Exists(sampleOut)) Directory.Delete(sampleOut, true);
    ZipFile.ExtractToDirectory(sampleZip, sampleOut);
    var afterZip = ReadPayload(Path.Combine(sampleOut, "sample.rvt")) == payload;
    report.AppendLine($"G1 real-file copy: {(afterWrite ? "PASS" : "FAIL")} ({before.Count} original streams kept)");
    report.AppendLine($"G6 real-file zip: {(afterZip ? "PASS" : "FAIL")}");
    report.AppendLine($"SAMPLE {clone}");
}
else
{
    report.AppendLine("G1 real-file copy: SKIPPED (no sample path)");
}

Console.Write(report.ToString());
return g1 && g6 ? 0 : 1;

static void WritePayload(string path, string json, bool extraStream)
{
    var create = !File.Exists(path);
    using var root = create ? RootStorage.Create(path) : RootStorage.Open(path, FileMode.Open, FileAccess.ReadWrite);
    if (extraStream && !create)
    {
        // existing file keeps its streams
    }
    if (create)
    {
        using var basic = root.CreateStream("BasicFileInfo");
        basic.Write(Encoding.Unicode.GetBytes("Revit\0"));
    }
    if (root.TryOpenStream("RvtFileInfo", out var existing))
    {
        using (existing)
        {
            existing.SetLength(0);
            var data = Encoding.UTF8.GetBytes(json);
            existing.Write(data);
        }
        root.Flush();
        return;
    }
    using var created = root.CreateStream("RvtFileInfo");
    created.Write(Encoding.UTF8.GetBytes(json));
    root.Flush();
}

static string ReadPayload(string path)
{
    using var root = RootStorage.OpenRead(path);
    using var stream = root.OpenStream("RvtFileInfo");
    using var reader = new StreamReader(stream, Encoding.UTF8);
    return reader.ReadToEnd();
}

static List<string> ListNames(string path)
{
    using var root = RootStorage.OpenRead(path);
    return root.EnumerateEntries().Select(entry => entry.Name).ToList();
}
