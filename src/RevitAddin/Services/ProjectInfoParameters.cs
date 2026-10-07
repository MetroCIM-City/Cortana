using System.IO;
using System.Text;
using Autodesk.Revit.DB;
using RvtFileInfo.Store;

namespace RvtFileInfo.RevitAddin;

public static class ProjectInfoParameters
{
    public const string ModifiedName = "RvtFileInfo_ModifiedUtc";

    public static readonly (string Name, Guid Id, bool Visible, string Label)[] Fields =
    [
        ("RvtFileInfo_Discipline", Guid.Parse("A11D0001-7C4E-4B2A-9F10-6E8D3C5B1A01"), true, "Discipline"),
        ("RvtFileInfo_Location", Guid.Parse("A11D0002-7C4E-4B2A-9F10-6E8D3C5B1A02"), true, "Location"),
        ("RvtFileInfo_Originator", Guid.Parse("A11D0003-7C4E-4B2A-9F10-6E8D3C5B1A03"), true, "Originator"),
        ("RvtFileInfo_SubDiscipline", Guid.Parse("A11D0004-7C4E-4B2A-9F10-6E8D3C5B1A04"), true, "Sub Discipline"),
        ("RvtFileInfo_DocumentType", Guid.Parse("A11D0005-7C4E-4B2A-9F10-6E8D3C5B1A05"), true, "Document Type"),
        ("RvtFileInfo_Program", Guid.Parse("A11D0006-7C4E-4B2A-9F10-6E8D3C5B1A06"), true, "Program"),
        ("RvtFileInfo_SubProgram", Guid.Parse("A11D0007-7C4E-4B2A-9F10-6E8D3C5B1A07"), true, "Sub Program"),
        (ModifiedName, Guid.Parse("A11D0008-7C4E-4B2A-9F10-6E8D3C5B1A08"), false, "ModifiedUtc")
    ];

    public static string? OtherOwner(Document document)
    {
        if (!document.IsWorkshared) return null;
        var info = document.ProjectInformation;
        if (WorksharingUtils.GetCheckoutStatus(document, info.Id) != CheckoutStatus.OwnedByOtherUser) return null;
        return WorksharingUtils.GetWorksharingTooltipInfo(document, info.Id).Owner;
    }

    public static bool Ensure(Document document, out string? error)
    {
        error = null;
        if (Fields.All(field => document.ProjectInformation.LookupParameter(field.Name) != null)) return true;
        var app = document.Application;
        var previous = app.SharedParametersFilename;
        var temp = Path.Combine(Path.GetTempPath(), $"RvtFileInfo-{Guid.NewGuid():N}.txt");
        File.WriteAllText(temp, Header(), new UTF8Encoding(false));
        using var transaction = new Transaction(document, "RvtFileInfo: update");
        try
        {
            app.SharedParametersFilename = temp;
            var file = app.OpenSharedParameterFile() ?? throw new InvalidOperationException("Could not open the temporary shared parameter file.");
            var group = file.Groups.Cast<DefinitionGroup>().FirstOrDefault(item => item.Name == "RvtFileInfo") ?? file.Groups.Create("RvtFileInfo");
            transaction.Start();
            var categories = app.Create.NewCategorySet();
            categories.Insert(document.Settings.Categories.get_Item(BuiltInCategory.OST_ProjectInformation));
            var binding = app.Create.NewInstanceBinding(categories);
            foreach (var field in Fields)
            {
                if (document.ProjectInformation.LookupParameter(field.Name) != null) continue;
                var definition = group.Definitions.Cast<Definition>().FirstOrDefault(item => item.Name == field.Name) as ExternalDefinition;
                if (definition == null)
                {
                    var options = new ExternalDefinitionCreationOptions(field.Name, SpecTypeId.String.Text)
                    {
                        GUID = field.Id,
                        Visible = field.Visible,
                        UserModifiable = true
                    };
                    definition = (ExternalDefinition)group.Definitions.Create(options);
                }
                if (!document.ParameterBindings.Insert(definition, binding, GroupTypeId.IdentityData))
                    document.ParameterBindings.ReInsert(definition, binding, GroupTypeId.IdentityData);
            }
            transaction.Commit();
            return true;
        }
        catch (Exception ex)
        {
            if (transaction.HasStarted() && transaction.GetStatus() == TransactionStatus.Started) transaction.RollBack();
            error = ex.Message;
            AddinLog.Write(ex.ToString());
            return false;
        }
        finally
        {
            app.SharedParametersFilename = previous;
            try { File.Delete(temp); } catch { }
        }
    }

    public static FileInfoRecord Read(Document document)
    {
        var info = document.ProjectInformation;
        return new FileInfoRecord
        {
            Discipline = Value(info, "RvtFileInfo_Discipline"),
            Location = Value(info, "RvtFileInfo_Location"),
            Originator = Value(info, "RvtFileInfo_Originator"),
            SubDiscipline = Value(info, "RvtFileInfo_SubDiscipline"),
            DocumentType = Value(info, "RvtFileInfo_DocumentType"),
            Program = Value(info, "RvtFileInfo_Program"),
            SubProgram = Value(info, "RvtFileInfo_SubProgram"),
            ModifiedUtc = Value(info, ModifiedName),
            Source = "revit"
        };
    }

    public static bool Write(Document document, FileInfoRecord record, string transactionName, out string? error)
    {
        error = null;
        record.ModifiedUtc = DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ");
        record.Source = "revit";
        using var transaction = new Transaction(document, transactionName);
        try
        {
            transaction.Start();
            var info = document.ProjectInformation;
            Set(info, "RvtFileInfo_Discipline", record.Discipline);
            Set(info, "RvtFileInfo_Location", record.Location);
            Set(info, "RvtFileInfo_Originator", record.Originator);
            Set(info, "RvtFileInfo_SubDiscipline", record.SubDiscipline);
            Set(info, "RvtFileInfo_DocumentType", record.DocumentType);
            Set(info, "RvtFileInfo_Program", record.Program);
            Set(info, "RvtFileInfo_SubProgram", record.SubProgram);
            Set(info, ModifiedName, record.ModifiedUtc);
            transaction.Commit();
            return true;
        }
        catch (Exception ex)
        {
            if (transaction.HasStarted() && transaction.GetStatus() == TransactionStatus.Started) transaction.RollBack();
            error = ex.Message;
            AddinLog.Write(ex.ToString());
            return false;
        }
    }

    static string Value(Element element, string name) => element.LookupParameter(name)?.AsString() ?? "";

    static void Set(Element element, string name, string value)
    {
        var parameter = element.LookupParameter(name) ?? throw new InvalidOperationException($"Missing parameter {name}.");
        parameter.Set(value ?? "");
    }

    static string Header() =>
        "# This is a Revit shared parameter file.\r\n*META\tVERSION\tMINVERSION\r\nMETA\t2\t1\r\n*GROUP\tID\tNAME\r\n*PARAM\tGUID\tNAME\tDATATYPE\tDATACATEGORY\tGROUP\tVISIBLE\tDESCRIPTION\tUSERMODIFIABLE\tHIDEWHENNOVALUE\r\n";
}
