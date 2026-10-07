using System.IO;

namespace RvtFileInfo.RevitAddin;

public static class AddinLog
{
    public static void Write(string message)
    {
        try
        {
            var dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "RvtFileInfo", "logs");
            Directory.CreateDirectory(dir);
            File.AppendAllText(Path.Combine(dir, "addin.log"), $"{DateTime.UtcNow:o} {message}{Environment.NewLine}");
        }
        catch
        {
        }
    }
}
