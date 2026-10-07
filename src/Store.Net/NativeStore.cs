using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace RvtFileInfo.Store;

public sealed class FileInfoRecord
{
    public string Discipline { get; set; } = "";
    public string Location { get; set; } = "";
    public string Originator { get; set; } = "";
    public string SubDiscipline { get; set; } = "";
    public string DocumentType { get; set; } = "";
    public string Program { get; set; } = "";
    public string SubProgram { get; set; } = "";
    public string ModifiedUtc { get; set; } = "";
    public string Source { get; set; } = "";
}

public sealed class StoreException : Exception
{
    public StoreException(int result, string message) : base(message)
    {
        HResult = result;
    }
}

public static class NativeStore
{
    const uint FieldAll = 0x7F;

    static NativeStore()
    {
        NativeLibrary.SetDllImportResolver(typeof(NativeStore).Assembly, Resolve);
    }

    public static FileInfoRecord? Read(string path)
    {
        var payload = new RfiPayload();
        var hr = Native.rfi_read(path, ref payload);
        if (hr == 1) return null; // S_FALSE: no payload
        Throw(hr);
        return FromPayload(payload);
    }

    public static void Write(string path, FileInfoRecord record, string source, uint mask = FieldAll)
    {
        var payload = ToPayload(record, source, mask);
        Throw(Native.rfi_write(path, ref payload));
    }

    public static string ListStreams(string path)
    {
        uint count = 0;
        var hr = Native.rfi_list_streams(path, null, ref count);
        if (hr != 0 && (uint)hr != 0x8007007A) Throw(hr);
        var buffer = new char[Math.Max(count, 1)];
        Throw(Native.rfi_list_streams(path, buffer, ref count));
        return new string(buffer).TrimEnd('\0');
    }

    static FileInfoRecord FromPayload(RfiPayload payload) => new()
    {
        Discipline = payload.Discipline ?? "",
        Location = payload.Location ?? "",
        Originator = payload.Originator ?? "",
        SubDiscipline = payload.SubDiscipline ?? "",
        DocumentType = payload.DocumentType ?? "",
        Program = payload.Program ?? "",
        SubProgram = payload.SubProgram ?? "",
        ModifiedUtc = payload.ModifiedUtc ?? "",
        Source = payload.Source ?? ""
    };

    static RfiPayload ToPayload(FileInfoRecord record, string source, uint mask) => new()
    {
        Schema = 1,
        SetMask = mask,
        Discipline = record.Discipline ?? "",
        Location = record.Location ?? "",
        Originator = record.Originator ?? "",
        SubDiscipline = record.SubDiscipline ?? "",
        DocumentType = record.DocumentType ?? "",
        Program = record.Program ?? "",
        SubProgram = record.SubProgram ?? "",
        ModifiedUtc = record.ModifiedUtc ?? "",
        Source = source ?? ""
    };

    static void Throw(int hr)
    {
        if (hr >= 0) return;
        var buffer = new StringBuilder(1024);
        Native.rfi_last_error(buffer, (uint)buffer.Capacity);
        var message = buffer.Length == 0 ? $"Store call failed (0x{hr:X8})." : buffer.ToString();
        throw new StoreException(hr, message);
    }

    static IntPtr Resolve(string library, Assembly assembly, DllImportSearchPath? path)
    {
        if (!library.Equals("RvtFileInfo.Store", StringComparison.OrdinalIgnoreCase)) return IntPtr.Zero;
        foreach (var candidate in Candidates())
        {
            if (File.Exists(candidate) && NativeLibrary.TryLoad(candidate, out var handle)) return handle;
        }
        return IntPtr.Zero;
    }

    static IEnumerable<string> Candidates()
    {
        var env = Environment.GetEnvironmentVariable("RFI_STORE_DLL");
        if (!string.IsNullOrWhiteSpace(env)) yield return env;
        yield return Path.Combine(AppContext.BaseDirectory, "RvtFileInfo.Store.dll");
        var programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        yield return Path.Combine(programFiles, "RvtFileInfo", "RvtFileInfo.Store.dll");
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode, Pack = 4)]
    struct RfiPayload
    {
        public uint Schema;
        public uint SetMask;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 257)] public string Discipline;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 257)] public string Location;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 257)] public string Originator;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 257)] public string SubDiscipline;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 257)] public string DocumentType;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 257)] public string Program;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 257)] public string SubProgram;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 40)] public string ModifiedUtc;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)] public string Source;
    }

    static class Native
    {
        [DllImport("RvtFileInfo.Store", CharSet = CharSet.Unicode)]
        public static extern int rfi_read(string path, ref RfiPayload payload);

        [DllImport("RvtFileInfo.Store", CharSet = CharSet.Unicode)]
        public static extern int rfi_write(string path, ref RfiPayload payload);

        [DllImport("RvtFileInfo.Store", CharSet = CharSet.Unicode)]
        public static extern int rfi_list_streams(string path, [Out] char[]? buffer, ref uint count);

        [DllImport("RvtFileInfo.Store", CharSet = CharSet.Unicode)]
        public static extern int rfi_last_error(StringBuilder buffer, uint count);
    }
}

public static class SyncDecision
{
    public static bool ShouldImport(string? fileUtc, string? modelUtc)
    {
        if (!TryParse(fileUtc, out var file)) return false;
        if (!TryParse(modelUtc, out var model)) return true;
        return file > model;
    }

    static bool TryParse(string? text, out DateTime utc)
    {
        return DateTime.TryParse(text, null, System.Globalization.DateTimeStyles.AdjustToUniversal | System.Globalization.DateTimeStyles.AssumeUniversal, out utc);
    }
}
