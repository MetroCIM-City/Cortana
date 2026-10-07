using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using RvtFileInfo.Store;

namespace RvtFileInfo.RevitAddin;

public sealed class FileInfoWindow : Window
{
    public FileInfoRecord? Result { get; private set; }
    readonly (string Label, Func<FileInfoRecord, string> Get, Action<FileInfoRecord, string> Set, ComboBox Box)[] _rows;

    public FileInfoWindow(FileInfoRecord current)
    {
        Title = "File Info";
        Width = 460;
        SizeToContent = SizeToContent.Height;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        ResizeMode = ResizeMode.NoResize;
        var picklists = LoadPicklists();
        var grid = new Grid { Margin = new Thickness(16) };
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(140) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        var fields = new (string Label, Func<FileInfoRecord, string> Get, Action<FileInfoRecord, string> Set)[]
        {
            ("Discipline", record => record.Discipline, (record, value) => record.Discipline = value),
            ("Location", record => record.Location, (record, value) => record.Location = value),
            ("Originator", record => record.Originator, (record, value) => record.Originator = value),
            ("Sub Discipline", record => record.SubDiscipline, (record, value) => record.SubDiscipline = value),
            ("Document Type", record => record.DocumentType, (record, value) => record.DocumentType = value),
            ("Program", record => record.Program, (record, value) => record.Program = value),
            ("Sub Program", record => record.SubProgram, (record, value) => record.SubProgram = value)
        };
        var rows = new List<(string, Func<FileInfoRecord, string>, Action<FileInfoRecord, string>, ComboBox)>();
        for (var i = 0; i < fields.Length; i++)
        {
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            var label = new TextBlock { Text = fields[i].Label, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 6, 8, 6) };
            var box = new ComboBox { IsEditable = true, Margin = new Thickness(0, 6, 0, 6), Text = fields[i].Get(current) };
            if (picklists.TryGetValue(fields[i].Label, out var options))
            {
                foreach (var option in options) box.Items.Add(option);
            }
            Grid.SetRow(label, i);
            Grid.SetColumn(label, 0);
            Grid.SetRow(box, i);
            Grid.SetColumn(box, 1);
            grid.Children.Add(label);
            grid.Children.Add(box);
            rows.Add((fields[i].Label, fields[i].Get, fields[i].Set, box));
        }
        _rows = rows.ToArray();
        grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Margin = new Thickness(0, 12, 0, 0) };
        var ok = new Button { Content = "OK", Width = 80, Margin = new Thickness(0, 0, 8, 0), IsDefault = true };
        var cancel = new Button { Content = "Cancel", Width = 80, IsCancel = true };
        ok.Click += (_, _) => Accept();
        buttons.Children.Add(ok);
        buttons.Children.Add(cancel);
        Grid.SetRow(buttons, fields.Length);
        Grid.SetColumnSpan(buttons, 2);
        grid.Children.Add(buttons);
        Content = grid;
    }

    void Accept()
    {
        var record = new FileInfoRecord();
        foreach (var row in _rows)
        {
            var text = row.Box.Text ?? "";
            if (text.Length > 256)
            {
                MessageBox.Show(this, $"{row.Label} must be 256 characters or fewer.", "File Info");
                return;
            }
            row.Set(record, text.Trim());
        }
        Result = record;
        DialogResult = true;
    }

    static Dictionary<string, string[]> LoadPicklists()
    {
        try
        {
            var path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "RvtFileInfo", "picklists.json");
            if (!File.Exists(path)) return new Dictionary<string, string[]>();
            var json = File.ReadAllText(path);
            return JsonSerializer.Deserialize<Dictionary<string, string[]>>(json) ?? new Dictionary<string, string[]>();
        }
        catch
        {
            return new Dictionary<string, string[]>();
        }
    }
}
