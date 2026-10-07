using System.Timers;
using Autodesk.Revit.DB;
using Autodesk.Revit.DB.Events;
using Autodesk.Revit.UI;
using RvtFileInfo.Store;

namespace RvtFileInfo.RevitAddin;

public sealed class FileStoreSync : IExternalEventHandler
{
    const int SharingViolation = unchecked((int)0x80070020);
    readonly ExternalEvent _event;
    readonly System.Timers.Timer _timer;
    readonly object _gate = new();
    readonly Queue<Pending> _pending = new();
    bool _dialogShown;

    public FileStoreSync(UIControlledApplication application)
    {
        _ = application;
        _event = ExternalEvent.Create(this);
        _timer = new System.Timers.Timer(2000) { AutoReset = false };
        _timer.Elapsed += (_, _) => _event.Raise();
    }

    public void Execute(UIApplication app)
    {
        Pending? item = null;
        lock (_gate)
        {
            if (_pending.Count > 0) item = _pending.Dequeue();
        }
        if (item == null) return;
        try
        {
            NativeStore.Write(item.Path, item.Record, "revit");
        }
        catch (StoreException ex) when (ex.HResult == SharingViolation && item.Attempts < 3)
        {
            item.Attempts++;
            lock (_gate) _pending.Enqueue(item);
            _timer.Start();
        }
        catch (Exception ex)
        {
            AddinLog.Write(ex.ToString());
            if (!_dialogShown)
            {
                _dialogShown = true;
                TaskDialog.Show("File Info", "The file info values could not be written next to the Revit file. They remain in the model and will be tried again on the next save.");
            }
        }
    }

    public string GetName() => "RvtFileInfo export";

    public void OnOpened(object? sender, DocumentOpenedEventArgs args) => ImportIfNewer(args.Document);

    public void OnSaved(object? sender, DocumentSavedEventArgs args) => ExportModel(args.Document);

    public void OnSavedAs(object? sender, DocumentSavedAsEventArgs args) => ExportModel(args.Document);

    public void OnSynced(object? sender, DocumentSynchronizedWithCentralEventArgs args) => ExportModel(args.Document);

    static void ExportModel(Document document)
    {
        if (document.IsFamilyDocument || string.IsNullOrEmpty(document.PathName)) return;
        if (document.ProjectInformation.LookupParameter(ProjectInfoParameters.ModifiedName) == null) return;
        ExportOrQueue(document, ProjectInfoParameters.Read(document));
    }

    public static void ExportOrQueue(Document document, FileInfoRecord record)
    {
        if (document.IsFamilyDocument || string.IsNullOrEmpty(document.PathName)) return;
        var sync = Current;
        try
        {
            NativeStore.Write(document.PathName, record, "revit");
        }
        catch (StoreException ex) when (ex.HResult == SharingViolation && sync != null)
        {
            lock (sync._gate) sync._pending.Enqueue(new Pending(document.PathName, record, 1));
            sync._timer.Start();
        }
        catch (Exception ex)
        {
            AddinLog.Write(ex.ToString());
        }
    }

    static FileStoreSync? Current { get; set; }

    void ImportIfNewer(Document document)
    {
        try
        {
            if (document.IsFamilyDocument || document.IsLinked || string.IsNullOrEmpty(document.PathName)) return;
            var stored = NativeStore.Read(document.PathName);
            if (stored == null) return;
            if (!ProjectInfoParameters.Ensure(document, out _)) return;
            var model = ProjectInfoParameters.Read(document);
            if (!SyncDecision.ShouldImport(stored.ModifiedUtc, model.ModifiedUtc)) return;
            ProjectInfoParameters.Write(document, stored, "Import file info", out _);
        }
        catch (Exception ex)
        {
            AddinLog.Write(ex.ToString());
        }
    }

    public FileStoreSync RegisterAsCurrent()
    {
        Current = this;
        return this;
    }

    sealed class Pending
    {
        public Pending(string path, FileInfoRecord record, int attempts)
        {
            Path = path;
            Record = record;
            Attempts = attempts;
        }

        public string Path { get; }
        public FileInfoRecord Record { get; }
        public int Attempts { get; set; }
    }
}
