using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using Microsoft.Win32;

const string Clsid = "{C4A91E72-5D38-4F0B-9E16-2B7A6C8D4E50}";
const string AddInId = "{8E2B7C41-6A95-4D13-B8F0-3C5D9A1E7B24}";
string[] DetailNames =
[
    "RvtFileInfo.FileInfo",
    "RvtFileInfo.Discipline",
    "RvtFileInfo.Location",
    "RvtFileInfo.Originator",
    "RvtFileInfo.SubDiscipline",
    "RvtFileInfo.DocumentType",
    "RvtFileInfo.Program",
    "RvtFileInfo.SubProgram"
];
string[] DetailValues = ["FullDetails", "PreviewDetails", "InfoTip"];
string[] Extensions = [".rvt", ".rfa", ".dwg", ".nwd", ".nwf", ".nwc", ".pdf"];

var installDir = AppContext.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar);
var logDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "RvtFileInfo", "logs");
Directory.CreateDirectory(logDir);

try
{
    if (args.Length == 0) return Fail("Usage: RvtFileInfo.Setup install|uninstall");
    return args[0].Equals("uninstall", StringComparison.OrdinalIgnoreCase) ? Uninstall() : Install(args);
}
catch (Exception ex)
{
    return Fail(ex.ToString());
}

int Install(string[] installArgs)
{
    if (Environment.OSVersion.Version.Build < 22000 || !Environment.Is64BitOperatingSystem)
        return Fail("Windows 11 x64 (build 22000 or later) is required.");

    var addin = Flag(installArgs, "--addin", "1") != "0";
    var allYears = Flag(installArgs, "--allyears", "0") == "1";
    var restart = Flag(installArgs, "--restart", "0") == "1";

    using var machine = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
    using var classes = machine.CreateSubKey(@"SOFTWARE\Classes") ?? throw new InvalidOperationException("Could not open HKLM\\SOFTWARE\\Classes.");
    Backup(machine, classes);
    RegisterCom(classes);
    RegisterHandler(machine);
    RegisterSchema();
    AppendDetails(classes);
    if (addin) WriteManifests(allYears);
    Native.SHChangeNotify(0x08000000, 0x0000, 0, 0);
    if (restart) RestartExplorer();
    Log("Install completed.");
    return 0;
}

int Uninstall()
{
    using var machine = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
    using var classes = machine.CreateSubKey(@"SOFTWARE\Classes") ?? throw new InvalidOperationException("Could not open HKLM\\SOFTWARE\\Classes.");
    RemoveManifests(machine);
    RestoreDetails(classes, machine);
    RestoreHandler(machine);
    UnregisterSchema();
    classes.DeleteSubKeyTree($@"CLSID\{Clsid}", false);
    using (var approved = machine.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved", true))
    {
        approved?.DeleteValue(Clsid, false);
    }
    RemoveUnchangedPicklist();
    machine.DeleteSubKeyTree(@"SOFTWARE\RvtFileInfo", false);
    Native.SHChangeNotify(0x08000000, 0x0000, 0, 0);
    Log("Uninstall completed.");
    return 0;
}

void Backup(RegistryKey machine, RegistryKey classes)
{
    using var key = machine.CreateSubKey(@"SOFTWARE\RvtFileInfo");
    using var originals = machine.CreateSubKey(@"SOFTWARE\RvtFileInfo\OriginalHandlers");
    foreach (var ext in Extensions)
    {
        if (originals.GetValue(ext) is string already && already.Length > 0) continue;
        if (originals.GetValue($"{ext}Present") != null) continue;
        using var handler = machine.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\" + ext);
        var current = handler?.GetValue(null) as string;
        if (string.Equals(current, Clsid, StringComparison.OrdinalIgnoreCase)) current = "";
        originals.SetValue($"{ext}Present", string.IsNullOrEmpty(current) ? 0 : 1, RegistryValueKind.DWord);
        originals.SetValue(ext, current ?? "");
        if (ext == ".rvt")
        {
            key.SetValue("OriginalPropertyHandlerPresent", string.IsNullOrEmpty(current) ? 0 : 1, RegistryValueKind.DWord);
            key.SetValue("OriginalPropertyHandler", current ?? "", RegistryValueKind.String);
        }
    }

    var index = (int)(key.GetValue("DetailCount") ?? 0);
    foreach (var rel in DetailLocations(classes))
    {
        foreach (var name in DetailValues)
        {
            if (DetailBackedUp(key, index, rel, name)) continue;
            using var node = classes.OpenSubKey(rel);
            var exists = node?.GetValue(name) is string;
            key.SetValue($"Detail{index}Key", rel);
            key.SetValue($"Detail{index}Name", name);
            key.SetValue($"Detail{index}Exists", exists ? 1 : 0, RegistryValueKind.DWord);
            key.SetValue($"Detail{index}Value", node?.GetValue(name) as string ?? "");
            index++;
        }
    }
    key.SetValue("DetailCount", index, RegistryValueKind.DWord);
    key.SetValue("BackupWritten", 1, RegistryValueKind.DWord);
}

static bool DetailBackedUp(RegistryKey key, int count, string rel, string name)
{
    for (var i = 0; i < count; i++)
    {
        if (string.Equals(key.GetValue($"Detail{i}Key") as string, rel, StringComparison.OrdinalIgnoreCase) &&
            string.Equals(key.GetValue($"Detail{i}Name") as string, name, StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }
    }
    return false;
}

IEnumerable<string> DetailLocations(RegistryKey classes)
{
    var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    foreach (var ext in Extensions)
    {
        EnsureExtensionKey(classes, ext);
        foreach (var rel in new[] { ext, @"SystemFileAssociations\" + ext })
        {
            if (seen.Add(rel)) yield return rel;
        }
        foreach (var progId in ProgIdsFor(classes, ext))
        {
            if (seen.Add(progId)) yield return progId;
        }
    }
}

static void EnsureExtensionKey(RegistryKey classes, string ext)
{
    using var node = classes.CreateSubKey(ext);
    if (node.GetValue(null) is string existing && !string.IsNullOrWhiteSpace(existing)) return;
    if (ext is ".rvt" or ".rfa" or ".dwg" or ".pdf") return;
    var progId = "RvtFileInfo" + ext;
    node.SetValue(null, progId);
    using var prog = classes.CreateSubKey(progId);
    prog.SetValue(null, ext.Trim('.') + " file");
}

static IEnumerable<string> ProgIdsFor(RegistryKey classes, string ext)
{
    using var node = classes.OpenSubKey(ext);
    if (node == null) yield break;
    if (node.GetValue(null) is string primary && !string.IsNullOrWhiteSpace(primary))
    {
        foreach (var name in WithCurVer(classes, primary.Trim())) yield return name;
    }
    using var openWith = node.OpenSubKey("OpenWithProgids");
    if (openWith == null) yield break;
    foreach (var name in openWith.GetValueNames())
    {
        if (string.IsNullOrWhiteSpace(name)) continue;
        foreach (var mapped in WithCurVer(classes, name.Trim())) yield return mapped;
    }
}

static IEnumerable<string> WithCurVer(RegistryKey classes, string progId)
{
    yield return progId;
    using var cur = classes.OpenSubKey(progId + @"\CurVer");
    if (cur?.GetValue(null) is string version && !string.IsNullOrWhiteSpace(version) &&
        !version.Equals(progId, StringComparison.OrdinalIgnoreCase))
    {
        yield return version.Trim();
    }
}

void RegisterCom(RegistryKey classes)
{
    using var inproc = classes.CreateSubKey($@"CLSID\{Clsid}\InprocServer32");
    inproc.SetValue(null, Path.Combine(installDir, "RvtFileInfo.ShellHandler.dll"));
    inproc.SetValue("ThreadingModel", "Both");
    using var root = classes.CreateSubKey($@"CLSID\{Clsid}");
    root.SetValue(null, "RvtFileInfo Property Handler");
}

void RegisterHandler(RegistryKey machine)
{
    foreach (var ext in Extensions)
    {
        using var handler = machine.CreateSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\" + ext);
        handler.SetValue(null, Clsid);
        Log($"Property handler {ext} -> {Clsid}");
    }
    using var approved = machine.CreateSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved");
    approved.SetValue(Clsid, "RvtFileInfo Property Handler");
}

void AppendDetails(RegistryKey classes)
{
    foreach (var rel in DetailLocations(classes))
    {
        using var node = classes.CreateSubKey(rel);
        foreach (var name in DetailValues)
        {
            var current = node.GetValue(name) as string;
            node.SetValue(name, Append(current, name == "InfoTip"));
        }
        Log($"Details layout {rel}");
    }
}

static string Append(string? current, bool infoTip)
{
    var names = new[]
    {
        "prop:RvtFileInfo.FileInfo", "RvtFileInfo.Discipline", "RvtFileInfo.Location", "RvtFileInfo.Originator",
        "RvtFileInfo.SubDiscipline", "RvtFileInfo.DocumentType", "RvtFileInfo.Program", "RvtFileInfo.SubProgram"
    };
    if (string.IsNullOrWhiteSpace(current))
    {
        var body = string.Join(";", names);
        return infoTip
            ? "prop:System.ItemNameDisplay;" + body
            : "prop:System.PropGroup.FileSystem;System.ItemNameDisplay;System.ItemType;System.Size;System.DateModified;" + body;
    }
    var result = current;
    foreach (var name in names)
    {
        if (!result.Contains(name, StringComparison.OrdinalIgnoreCase)) result += ";" + name;
    }
    return result;
}

void RestoreDetails(RegistryKey classes, RegistryKey machine)
{
    using var key = machine.OpenSubKey(@"SOFTWARE\RvtFileInfo");
    if (key == null) return;
    var count = (int)(key.GetValue("DetailCount") ?? 0);
    for (var i = 0; i < count; i++)
    {
        var rel = key.GetValue($"Detail{i}Key") as string;
        var name = key.GetValue($"Detail{i}Name") as string;
        if (string.IsNullOrEmpty(rel) || string.IsNullOrEmpty(name)) continue;
        using var node = classes.CreateSubKey(rel);
        var existed = (int)(key.GetValue($"Detail{i}Exists") ?? 0) == 1;
        if (!existed) node.DeleteValue(name, false);
        else node.SetValue(name, key.GetValue($"Detail{i}Value") as string ?? "");
    }
}

void RestoreHandler(RegistryKey machine)
{
    using var originals = machine.OpenSubKey(@"SOFTWARE\RvtFileInfo\OriginalHandlers");
    using var legacy = machine.OpenSubKey(@"SOFTWARE\RvtFileInfo");
    foreach (var ext in Extensions)
    {
        var present = (int)(originals?.GetValue($"{ext}Present") ?? 0) == 1;
        var original = originals?.GetValue(ext) as string;
        if (ext == ".rvt" && string.IsNullOrEmpty(original))
        {
            present = (int)(legacy?.GetValue("OriginalPropertyHandlerPresent") ?? 0) == 1;
            original = legacy?.GetValue("OriginalPropertyHandler") as string;
        }
        var handlerPath = @"SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\" + ext;
        if (!present || string.IsNullOrEmpty(original) || original.Equals(Clsid, StringComparison.OrdinalIgnoreCase))
        {
            machine.DeleteSubKeyTree(handlerPath, false);
            continue;
        }
        using var handler = machine.CreateSubKey(handlerPath);
        handler.SetValue(null, original);
    }
}

void RegisterSchema()
{
    var schema = Path.Combine(installDir, "RvtFileInfo.propdesc");
    var hr = Native.PSRegisterPropertySchema(schema);
    if (hr < 0) throw new InvalidOperationException($"PSRegisterPropertySchema failed: 0x{hr:X8}");
    Native.PSRefreshPropertySchema();
}

void UnregisterSchema()
{
    var schema = Path.Combine(installDir, "RvtFileInfo.propdesc");
    if (File.Exists(schema)) Native.PSUnregisterPropertySchema(schema);
    Native.PSRefreshPropertySchema();
}

void WriteManifests(bool allYears)
{
    using var machine = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
    using var key = machine.CreateSubKey(@"SOFTWARE\RvtFileInfo");
    var written = new List<string>();
    foreach (var year in new[] { 2025, 2026, 2027 })
    {
        if (!allYears && !RevitInstalled(year)) continue;
        var folder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "Autodesk", "Revit", "Addins", year.ToString());
        Directory.CreateDirectory(folder);
        var manifest = Path.Combine(folder, "RvtFileInfo.addin");
        var assembly = Path.Combine(installDir, "RevitAddin", year.ToString(), "RvtFileInfo.RevitAddin.dll");
        File.WriteAllText(manifest, Manifest(assembly));
        written.Add(manifest);
    }
    key.SetValue("Manifests", written.ToArray(), RegistryValueKind.MultiString);
    var pick = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "RvtFileInfo", "picklists.json");
    var shipped = Path.Combine(installDir, "picklists.json");
    if (!File.Exists(pick) && File.Exists(shipped)) File.Copy(shipped, pick);
    if (File.Exists(pick)) key.SetValue("PicklistHash", Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(pick))));
}

void RemoveManifests(RegistryKey machine)
{
    using var key = machine.OpenSubKey(@"SOFTWARE\RvtFileInfo");
    if (key?.GetValue("Manifests") is string[] paths)
    {
        foreach (var path in paths)
        {
            if (File.Exists(path)) File.Delete(path);
        }
    }
}

void RemoveUnchangedPicklist()
{
    using var machine = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
    using var key = machine.OpenSubKey(@"SOFTWARE\RvtFileInfo");
    var expected = key?.GetValue("PicklistHash") as string;
    var pick = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "RvtFileInfo", "picklists.json");
    if (!File.Exists(pick) || string.IsNullOrEmpty(expected)) return;
    var actual = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(pick)));
    if (string.Equals(actual, expected, StringComparison.OrdinalIgnoreCase)) File.Delete(pick);
}

static bool RevitInstalled(int year)
{
    var candidates = new[]
    {
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Autodesk", $"Revit {year}", "Revit.exe"),
        Path.Combine(@"D:\Program Files\Autodesk", $"Revit {year}", "Revit.exe")
    };
    return candidates.Any(File.Exists);
}

static string Manifest(string assembly) => $"""
    <?xml version="1.0" encoding="utf-8"?>
    <RevitAddIns>
      <AddIn Type="Application">
        <Name>RvtFileInfo</Name>
        <Assembly>{assembly}</Assembly>
        <AddInId>{AddInId}</AddInId>
        <FullClassName>RvtFileInfo.RevitAddin.App</FullClassName>
        <VendorId>RFI</VendorId>
        <VendorDescription>RvtFileInfo</VendorDescription>
      </AddIn>
    </RevitAddIns>
    """;

static string Flag(string[] source, string name, string fallback)
{
    for (var i = 0; i < source.Length - 1; i++)
    {
        if (source[i].Equals(name, StringComparison.OrdinalIgnoreCase)) return source[i + 1];
    }
    return fallback;
}

static void RestartExplorer()
{
    foreach (var process in Process.GetProcessesByName("explorer"))
    {
        try { process.Kill(); } catch { }
    }
    Process.Start("explorer.exe");
}

int Fail(string message)
{
    Log(message);
    Console.Error.WriteLine(message);
    return 1603;
}

void Log(string message)
{
    File.AppendAllText(Path.Combine(logDir, "setup.log"), $"{DateTime.UtcNow:o} {message}{Environment.NewLine}");
}

static class Native
{
    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSRegisterPropertySchema(string path);

    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSUnregisterPropertySchema(string path);

    [DllImport("propsys.dll")]
    public static extern int PSRefreshPropertySchema();

    [DllImport("shell32.dll")]
    public static extern void SHChangeNotify(int eventId, uint flags, nint item1, nint item2);
}
