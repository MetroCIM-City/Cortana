using Autodesk.Revit.Attributes;
using Autodesk.Revit.DB;
using Autodesk.Revit.UI;
using RvtFileInfo.Store;

namespace RvtFileInfo.RevitAddin;

[Transaction(TransactionMode.Manual)]
[Regeneration(RegenerationOption.Manual)]
public sealed class EditFileInfoCommand : IExternalCommand
{
    public Result Execute(ExternalCommandData data, ref string message, ElementSet elements)
    {
        var document = data.Application.ActiveUIDocument?.Document;
        if (document == null || document.IsFamilyDocument)
        {
            message = "Open a project document first.";
            return Result.Cancelled;
        }
        var owner = ProjectInfoParameters.OtherOwner(document);
        if (owner != null)
        {
            TaskDialog.Show("File Info", $"Project Information is owned by {owner}. Ask them to relinquish it, then try again.");
            return Result.Cancelled;
        }
        if (!ProjectInfoParameters.Ensure(document, out var error))
        {
            message = error ?? "Could not create file info parameters.";
            return Result.Failed;
        }
        var current = ProjectInfoParameters.Read(document);
        var window = new FileInfoWindow(current);
        var helper = new System.Windows.Interop.WindowInteropHelper(window) { Owner = data.Application.MainWindowHandle };
        _ = helper.Owner;
        if (window.ShowDialog() != true || window.Result == null) return Result.Cancelled;
        if (!ProjectInfoParameters.Write(document, window.Result, "RvtFileInfo: update", out error))
        {
            message = error ?? "Could not save file info.";
            return Result.Failed;
        }
        if (!string.IsNullOrEmpty(document.PathName))
            FileStoreSync.ExportOrQueue(document, window.Result);
        return Result.Succeeded;
    }
}
