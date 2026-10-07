using RvtFileInfo.Store;

if (args.Length < 2)
{
    Usage();
    return 1;
}

var command = args[0].ToLowerInvariant();
var path = args[1];
try
{
    switch (command)
    {
        case "get":
            Print(NativeStore.Read(path));
            return 0;
        case "set":
            var (record, mask) = ParseSet(args.Skip(2));
            NativeStore.Write(path, record, "cli", mask);
            return 0;
        case "dump":
            Console.WriteLine(NativeStore.ListStreams(path));
            if (args.Any(arg => arg == "--streams")) return 0;
            Print(NativeStore.Read(path));
            return 0;
        default:
            Usage();
            return 1;
    }
}
catch (Exception ex)
{
    Console.Error.WriteLine(ex.Message);
    return 1;
}

static void Print(FileInfoRecord? record)
{
    record ??= new FileInfoRecord();
    Console.WriteLine($"Discipline: {record.Discipline}");
    Console.WriteLine($"Location: {record.Location}");
    Console.WriteLine($"Originator: {record.Originator}");
    Console.WriteLine($"Sub Discipline: {record.SubDiscipline}");
    Console.WriteLine($"Document Type: {record.DocumentType}");
    Console.WriteLine($"Program: {record.Program}");
    Console.WriteLine($"Sub Program: {record.SubProgram}");
    if (!string.IsNullOrEmpty(record.ModifiedUtc)) Console.WriteLine($"ModifiedUtc: {record.ModifiedUtc}");
    if (!string.IsNullOrEmpty(record.Source)) Console.WriteLine($"Source: {record.Source}");
}

static (FileInfoRecord Record, uint Mask) ParseSet(IEnumerable<string> tokens)
{
    var record = new FileInfoRecord();
    uint mask = 0;
    foreach (var token in tokens)
    {
        var split = token.IndexOf('=');
        if (split <= 0) throw new ArgumentException($"Expected Name=Value, got '{token}'.");
        var name = token[..split].Trim();
        var value = token[(split + 1)..];
        switch (Normalize(name))
        {
            case "discipline": record.Discipline = value; mask |= 1u << 0; break;
            case "location": record.Location = value; mask |= 1u << 1; break;
            case "originator": record.Originator = value; mask |= 1u << 2; break;
            case "subdiscipline": record.SubDiscipline = value; mask |= 1u << 3; break;
            case "documenttype": record.DocumentType = value; mask |= 1u << 4; break;
            case "program": record.Program = value; mask |= 1u << 5; break;
            case "subprogram": record.SubProgram = value; mask |= 1u << 6; break;
            default: throw new ArgumentException($"Unknown field '{name}'.");
        }
    }
    if (mask == 0) throw new ArgumentException("Provide at least one Field=Value assignment.");
    return (record, mask);
}

static string Normalize(string name) => name.Replace(" ", "", StringComparison.Ordinal).ToLowerInvariant();

static void Usage()
{
    Console.WriteLine("""
        rvtinfo get  <file.rvt>
        rvtinfo set  <file.rvt> Discipline=ARC Location=L01 Originator=ABC "Sub Discipline"=Facade "Document Type"=Model Program=Housing "Sub Program"=Phase1
        rvtinfo dump <file.rvt> --streams
        """);
}
