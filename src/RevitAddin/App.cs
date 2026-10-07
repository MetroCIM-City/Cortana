using System.IO;
using System.Reflection;
using System.Windows.Media.Imaging;
using Autodesk.Revit.UI;

namespace RvtFileInfo.RevitAddin;

public sealed class App : IExternalApplication
{
    FileStoreSync? _sync;

    public Result OnStartup(UIControlledApplication application)
    {
        try
        {
            _sync = new FileStoreSync(application).RegisterAsCurrent();
            var controlled = application.ControlledApplication;
            controlled.DocumentOpened += _sync.OnOpened;
            controlled.DocumentSaved += _sync.OnSaved;
            controlled.DocumentSavedAs += _sync.OnSavedAs;
            controlled.DocumentSynchronizedWithCentral += _sync.OnSynced;
            AddRibbon(application);
            return Result.Succeeded;
        }
        catch (Exception ex)
        {
            AddinLog.Write(ex.ToString());
            return Result.Failed;
        }
    }

    public Result OnShutdown(UIControlledApplication application)
    {
        if (_sync != null)
        {
            var controlled = application.ControlledApplication;
            controlled.DocumentOpened -= _sync.OnOpened;
            controlled.DocumentSaved -= _sync.OnSaved;
            controlled.DocumentSavedAs -= _sync.OnSavedAs;
            controlled.DocumentSynchronizedWithCentral -= _sync.OnSynced;
        }
        return Result.Succeeded;
    }

    static void AddRibbon(UIControlledApplication application)
    {
        const string tab = "File Info";
        try { application.CreateRibbonTab(tab); } catch { }
        var panel = application.CreateRibbonPanel(tab, "Metadata");
        var button = new PushButtonData(
            "EditFileInfo",
            "Edit File Info",
            Assembly.GetExecutingAssembly().Location,
            typeof(EditFileInfoCommand).FullName)
        {
            AvailabilityClassName = typeof(ProjectDocAvailability).FullName,
            ToolTip = "Edit Discipline, Location, Originator, Sub Discipline, Document Type, Program, and Sub Program.",
            Image = LoadIcon("icon16.png"),
            LargeImage = LoadIcon("icon32.png")
        };
        panel.AddItem(button);
    }

    static BitmapImage LoadIcon(string file)
    {
        var name = $"RvtFileInfo.RevitAddin.Resources.{file}";
        using var source = Assembly.GetExecutingAssembly().GetManifestResourceStream(name)
            ?? throw new InvalidOperationException($"Missing ribbon icon {name}.");
        using var copy = new MemoryStream();
        source.CopyTo(copy);
        copy.Position = 0;
        var image = new BitmapImage();
        image.BeginInit();
        image.StreamSource = copy;
        image.CacheOption = BitmapCacheOption.OnLoad;
        image.EndInit();
        image.Freeze();
        return image;
    }
}

public sealed class ProjectDocAvailability : IExternalCommandAvailability
{
    public bool IsCommandAvailable(UIApplication application, Autodesk.Revit.DB.CategorySet selectedCategories)
    {
        var document = application.ActiveUIDocument?.Document;
        return document != null && !document.IsFamilyDocument;
    }
}
