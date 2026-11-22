using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.Win32;

internal static class SteamGames
{
    private const string DefaultFolder = "ULTIMATE MARVEL VS. CAPCOM 3";
    private const string VdfValue = @"""\s*""((?:\\.|[^""\\])*)""";

    internal static List<string> Find()
    {
        var steamRoots = new List<string>();
        using (var userKey = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam"))
        using (var machine = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32))
        using (var machineKey = machine.OpenSubKey(@"Software\Valve\Steam"))
        {
            steamRoots.Add(userKey == null ? null : userKey.GetValue("SteamPath") as string);
            steamRoots.Add(machineKey == null ? null : machineKey.GetValue("InstallPath") as string);
        }
        steamRoots.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Steam"));
        steamRoots.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Steam"));

        var libraries = new List<string>();
        foreach (var root in steamRoots.Where(Directory.Exists).Distinct(StringComparer.OrdinalIgnoreCase))
        {
            libraries.Add(root);
            libraries.AddRange(ReadValues(Path.Combine(root, "steamapps", "libraryfolders.vdf"), @"(?:path|\d+)"));
            libraries.AddRange(ReadValues(Path.Combine(root, "config", "libraryfolders.vdf"), @"(?:path|\d+)"));
        }
        foreach (var drive in DriveInfo.GetDrives().Where(drive => drive.DriveType == DriveType.Fixed))
        {
            libraries.Add(Path.Combine(drive.Name, "SteamLibrary"));
            libraries.Add(Path.Combine(drive.Name, "Steam"));
            libraries.Add(Path.Combine(drive.Name, @"Games\SteamLibrary"));
        }
        return FindInLibraries(libraries);
    }

    internal static List<string> FindInLibraries(IEnumerable<string> libraries)
    {
        var games = new List<string>();
        foreach (var library in libraries.Where(Directory.Exists).Distinct(StringComparer.OrdinalIgnoreCase))
        {
            var steamApps = Path.Combine(library, "steamapps");
            var folders = ReadValues(Path.Combine(steamApps, "appmanifest_357190.acf"), "installdir")
                .Where(folder => folder.Length > 0 && folder.IndexOfAny(Path.GetInvalidFileNameChars()) < 0
                    && folder != "." && folder != "..")
                .Concat(new[] { DefaultFolder });
            foreach (var folder in folders)
            {
                string executable = ResolveExecutable(Path.Combine(steamApps, "common", folder));
                if (executable != null)
                    games.Add(executable);
            }
        }
        return games.Distinct(StringComparer.OrdinalIgnoreCase).ToList();
    }

    internal static IEnumerable<string> ReadValues(string file, string key)
    {
        try
        {
            string pattern = @"(?im)^\s*""" + key + VdfValue;
            return Regex.Matches(File.ReadAllText(file), pattern).Cast<Match>()
                .Select(match => match.Groups[1].Value.Replace(@"\\", @"\"))
                .ToList();
        }
        catch (IOException) { return Enumerable.Empty<string>(); }
        catch (UnauthorizedAccessException) { return Enumerable.Empty<string>(); }
    }

    internal static string ResolveExecutable(string input)
    {
        try
        {
            string path = (input ?? "").Trim().Trim('"');
            if (!Path.IsPathRooted(path))
                return null;
            path = Path.GetFullPath(path);
            if (Directory.Exists(path))
                path = Path.Combine(path, "umvc3.exe");
            return string.Equals(Path.GetFileName(path), "umvc3.exe", StringComparison.OrdinalIgnoreCase)
                && File.Exists(path) && Directory.Exists(Path.Combine(Path.GetDirectoryName(path), "nativePCx64"))
                ? path : null;
        }
        catch (ArgumentException) { return null; }
        catch (NotSupportedException) { return null; }
        catch (IOException) { return null; }
    }
}

internal static class ModInstaller
{
    private const string Loader = "dinput8.dll";
    private const string LicenseFolder = @"Licenses\UMVC3TrainingToolsInt\";
    private static readonly string[] ModFiles =
    {
        @"Resources\3TTAppText.csv", @"Resources\FontToUse.ttf", @"Resources\Apache License.txt",
        LicenseFolder + "UMVC3TrainingToolsInt.txt", LicenseFolder + "ImGui.txt",
        LicenseFolder + "Utils.txt", LicenseFolder + "MinHook.txt", LicenseFolder + "HDE.txt",
        "UMVC3TrainingToolsInt.asi"
    };

    private sealed class FileChange
    {
        internal string Destination;
        internal string Backup;
        internal bool Installed;
    }

    internal static int? FindRunningGame(string executable)
    {
        var processes = Process.GetProcessesByName("umvc3");
        try
        {
            foreach (var process in processes)
            {
                try
                {
                    if (string.Equals(process.MainModule.FileName, executable, StringComparison.OrdinalIgnoreCase))
                        return process.Id;
                }
                catch (InvalidOperationException) { }
                catch (Win32Exception)
                {
                    if (!process.HasExited)
                        return process.Id;
                }
            }
            return null;
        }
        finally
        {
            foreach (var process in processes)
                process.Dispose();
        }
    }

    internal static string Install(string executable)
    {
        string folder = Path.GetDirectoryName(executable);
        var files = new List<string>(ModFiles);
        bool installLoader = !File.Exists(Path.Combine(folder, Loader));
        if (installLoader)
        {
            files.Add(LicenseFolder + "Ultimate ASI Loader.txt");
            files.Add(Loader);
        }

        string id = Guid.NewGuid().ToString("N");
        string staging = Path.Combine(folder, ".umvc3-training-install-" + id);
        string backups = Path.Combine(folder, "UMVC3TrainingToolsInt-Backups",
            DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + id.Substring(0, 8));
        var changed = new List<string>();
        var applied = new List<FileChange>();
        var assembly = Assembly.GetExecutingAssembly();
        try
        {
            foreach (string relative in files)
            {
                string destination = Path.Combine(folder, relative);
                byte[] content;
                using (var resource = assembly.GetManifestResourceStream(relative.Replace('\\', '/')))
                using (var data = new MemoryStream())
                {
                    if (resource == null)
                        throw new InvalidDataException(relative);
                    resource.CopyTo(data);
                    content = data.ToArray();
                }
                var original = new FileInfo(destination);
                if (original.Exists && (relative == Loader || (original.Length == content.Length
                    && File.ReadAllBytes(destination).SequenceEqual(content))))
                    continue;

                string staged = Path.Combine(staging, relative);
                Directory.CreateDirectory(Path.GetDirectoryName(staged));
                File.WriteAllBytes(staged, content);
                changed.Add(relative);
            }

            foreach (string relative in changed)
            {
                string destination = Path.Combine(folder, relative);
                string staged = Path.Combine(staging, relative);
                Directory.CreateDirectory(Path.GetDirectoryName(destination));
                var change = new FileChange { Destination = destination };
                if (File.Exists(destination))
                {
                    if (relative == Loader)
                        continue;
                    change.Backup = Path.Combine(backups, relative + ".bak");
                    Directory.CreateDirectory(Path.GetDirectoryName(change.Backup));
                    File.Move(destination, change.Backup);
                }
                applied.Add(change);
                File.Move(staged, destination);
                change.Installed = true;
            }
        }
        catch (Exception error)
        {
            bool restored = true;
            for (int index = applied.Count - 1; index >= 0; --index)
            {
                var file = applied[index];
                try
                {
                    if (file.Installed)
                        File.Delete(file.Destination);
                    if (file.Backup != null)
                        File.Move(file.Backup, file.Destination);
                }
                catch (IOException) { restored = false; }
                catch (UnauthorizedAccessException) { restored = false; }
            }
            if (!restored)
                throw new InstallRecoveryException(backups, error);
            throw;
        }
        finally
        {
            try { if (Directory.Exists(staging)) Directory.Delete(staging, true); }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
        return applied.Any(file => file.Backup != null) ? backups : null;
    }
}

internal sealed class InstallRecoveryException : IOException
{
    internal string BackupFolder { get; private set; }

    internal InstallRecoveryException(string backupFolder, Exception error)
        : base(error.Message, error)
    {
        BackupFolder = backupFolder;
    }
}

internal sealed class InstallerWindow : Form
{
    private readonly ComboBox executable = new ComboBox();
    private readonly ComboBox language = new ComboBox();
    private readonly Label heading = new Label();
    private readonly Label pathLabel = new Label();
    private readonly Label languageLabel = new Label();
    private readonly Label status = new Label();
    private readonly LinkLabel backupLink = new LinkLabel();
    private readonly Button browse = new Button();
    private readonly Button install = new Button();
    private readonly Button play = new Button();
    private readonly Button close = new Button();
    private bool installing;
    private Status state = Status.Searching;
    private int runningPid;

    private enum Status
    {
        Searching, Detected, Manual, Invalid, Running, Installing, Complete, Permission, Failed, Recovery, InvalidPackage,
        GameStarted, GameAlreadyRunning, LaunchFailed
    }

    internal InstallerWindow()
    {
        Text = "UMVC3 Training Tools";
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        AutoScaleMode = AutoScaleMode.Font;
        Font = new Font("Segoe UI", 10);
        ClientSize = new Size(580, 256);

        heading.Font = new Font(Font, FontStyle.Bold);
        heading.SetBounds(18, 19, 315, 25);
        languageLabel.SetBounds(350, 20, 80, 26);
        language.SetBounds(436, 17, 126, 29);
        language.DropDownStyle = ComboBoxStyle.DropDownList;
        language.Items.AddRange(new[] { "Español", "English" });
        language.SelectedIndex = CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "es" ? 0 : 1;
        pathLabel.SetBounds(18, 61, 544, 24);
        executable.SetBounds(18, 87, 432, 29);
        browse.SetBounds(461, 85, 101, 31);
        status.SetBounds(18, 124, 544, 44);
        backupLink.SetBounds(18, 173, 544, 24);
        backupLink.Visible = false;
        install.SetBounds(244, 209, 98, 31);
        play.SetBounds(354, 209, 98, 31);
        close.SetBounds(464, 209, 98, 31);

        browse.Click += Browse;
        install.Click += Install;
        play.Click += Play;
        close.Click += (sender, args) => Close();
        executable.TextChanged += (sender, args) => UpdatePlayButton();
        backupLink.LinkClicked += OpenBackup;
        language.SelectedIndexChanged += (sender, args) => Localize();
        Controls.AddRange(new Control[] { heading, languageLabel, language, pathLabel, executable, browse, status,
            backupLink, install, play, close });
        AcceptButton = install;
        CancelButton = close;
        Shown += DetectGame;
        FormClosing += (sender, args) => { if (installing) args.Cancel = true; };
        Localize();
        UpdatePlayButton();
    }

    private string TextInLanguage(string spanish, string english)
    {
        return language.SelectedIndex == 0 ? spanish : english;
    }

    private void Localize()
    {
        heading.Text = TextInLanguage("Instalar UMVC3 Training Tools", "Install UMVC3 Training Tools");
        languageLabel.Text = TextInLanguage("Idioma", "Language");
        pathLabel.Text = TextInLanguage("Ubicación de umvc3.exe", "Location of umvc3.exe");
        browse.Text = TextInLanguage("Examinar…", "Browse…");
        backupLink.Text = TextInLanguage("Abrir carpeta de la copia de seguridad", "Open backup folder");
        install.Text = TextInLanguage("Instalar", "Install");
        play.Text = TextInLanguage("Jugar", "Play");
        close.Text = TextInLanguage("Cerrar", "Close");
        SetStatus(state);
    }

    private void SetStatus(Status next)
    {
        state = next;
        switch (state)
        {
            case Status.Searching:
                status.Text = TextInLanguage("Buscando UMVC3 en las bibliotecas de Steam…", "Searching Steam libraries for UMVC3…");
                break;
            case Status.Detected:
                status.Text = TextInLanguage("Instalación de Steam detectada.", "Steam installation detected.");
                break;
            case Status.Manual:
                status.Text = TextInLanguage("Selecciona el ejecutable con Examinar.", "Select the executable with Browse.");
                break;
            case Status.Invalid:
                status.Text = TextInLanguage("Selecciona el umvc3.exe de una instalación válida.", "Select umvc3.exe from a valid installation.");
                break;
            case Status.Running:
                status.Text = string.Format(TextInLanguage("UMVC3 sigue abierto (PID {0}). Ciérralo para instalar.",
                    "UMVC3 is still running (PID {0}). Close it before installing."), runningPid);
                break;
            case Status.Installing:
                status.Text = TextInLanguage("Instalando UMVC3 Training Tools…", "Installing UMVC3 Training Tools…");
                break;
            case Status.Complete:
                status.Text = TextInLanguage("Instalación completada. Pulsa Jugar para abrir UMVC3.",
                    "Installation complete. Click Play to open UMVC3.");
                break;
            case Status.Permission:
                status.Text = TextInLanguage("Carpeta protegida. Ejecuta el instalador como administrador.",
                    "Protected folder. Run the installer as administrator.");
                break;
            case Status.Failed:
                status.Text = TextInLanguage("No se pudo completar la instalación.", "Installation could not be completed.");
                break;
            case Status.Recovery:
                status.Text = TextInLanguage("No se pudieron restaurar todos los archivos. Revisa la copia de seguridad.",
                    "Some files could not be restored. Check the backup folder.");
                break;
            case Status.InvalidPackage:
                status.Text = TextInLanguage("Falta un archivo en el instalador. Descarga otra copia.",
                    "The installer is missing a file. Download a fresh copy.");
                break;
            case Status.GameStarted:
                status.Text = TextInLanguage("UMVC3 se está iniciando.", "UMVC3 is starting.");
                break;
            case Status.GameAlreadyRunning:
                status.Text = TextInLanguage("UMVC3 ya está abierto.", "UMVC3 is already running.");
                break;
            case Status.LaunchFailed:
                status.Text = TextInLanguage("No se pudo abrir UMVC3.", "Could not launch UMVC3.");
                break;
        }
    }

    private async void DetectGame(object sender, EventArgs args)
    {
        List<string> games = null;
        try { games = await Task.Run(() => SteamGames.Find()); }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
        catch (System.Security.SecurityException) { }
        if (IsDisposed || state != Status.Searching)
            return;
        if (games != null && games.Count > 0)
        {
            executable.Items.AddRange(games.ToArray());
            if (string.IsNullOrWhiteSpace(executable.Text))
                executable.SelectedIndex = 0;
            SetStatus(Status.Detected);
        }
        else
            SetStatus(Status.Manual);
    }

    private void Browse(object sender, EventArgs args)
    {
        using (var dialog = new OpenFileDialog { Title = TextInLanguage("Selecciona umvc3.exe", "Select umvc3.exe"),
            Filter = "Ultimate Marvel vs. Capcom 3 (umvc3.exe)|umvc3.exe", FileName = "umvc3.exe" })
        {
            string current = SteamGames.ResolveExecutable(executable.Text);
            if (current != null)
                dialog.InitialDirectory = Path.GetDirectoryName(current);
            if (dialog.ShowDialog(this) == DialogResult.OK)
                executable.Text = dialog.FileName;
        }
    }

    private async void Install(object sender, EventArgs args)
    {
        string path = SteamGames.ResolveExecutable(executable.Text);
        if (path == null)
        {
            SetStatus(Status.Invalid);
            executable.Focus();
            return;
        }
        installing = true;
        executable.Enabled = browse.Enabled = install.Enabled = play.Enabled = close.Enabled = false;
        ShowBackup(null);
        SetStatus(Status.Installing);
        try
        {
            int? game = await Task.Run(() => ModInstaller.FindRunningGame(path));
            if (game.HasValue)
            {
                runningPid = game.Value;
                SetStatus(Status.Running);
                return;
            }
            ShowBackup(await Task.Run(() => ModInstaller.Install(path)));
            SetStatus(Status.Complete);
        }
        catch (UnauthorizedAccessException)
        {
            SetStatus(Status.Permission);
        }
        catch (InvalidDataException)
        {
            SetStatus(Status.InvalidPackage);
        }
        catch (InstallRecoveryException error)
        {
            ShowBackup(error.BackupFolder);
            SetStatus(Status.Recovery);
        }
        catch (IOException error)
        {
            SetStatus(Status.Failed);
            MessageBox.Show(this, status.Text + "\n\n" + error.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally
        {
            installing = false;
            executable.Enabled = browse.Enabled = install.Enabled = close.Enabled = true;
            UpdatePlayButton();
            if (state == Status.Complete && play.Enabled)
                play.Focus();
            else
                close.Focus();
        }
    }

    private string GetPlayableExecutable()
    {
        string path = SteamGames.ResolveExecutable(executable.Text);
        return path != null && File.Exists(Path.Combine(Path.GetDirectoryName(path), "UMVC3TrainingToolsInt.asi"))
            ? path : null;
    }

    private void UpdatePlayButton()
    {
        play.Enabled = !installing && GetPlayableExecutable() != null;
    }

    private void Play(object sender, EventArgs args)
    {
        string path = GetPlayableExecutable();
        if (installing || path == null)
        {
            UpdatePlayButton();
            return;
        }

        try
        {
            if (ModInstaller.FindRunningGame(path).HasValue)
            {
                SetStatus(Status.GameAlreadyRunning);
                return;
            }

            var start = new ProcessStartInfo(path)
            {
                WorkingDirectory = Path.GetDirectoryName(path),
                UseShellExecute = false
            };
            using (Process.Start(start))
                SetStatus(Status.GameStarted);
        }
        catch (Win32Exception error)
        {
            SetStatus(Status.LaunchFailed);
            MessageBox.Show(this, status.Text + "\n\n" + error.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void ShowBackup(string path)
    {
        backupLink.Tag = path;
        backupLink.Visible = path != null;
    }

    private void OpenBackup(object sender, LinkLabelLinkClickedEventArgs args)
    {
        string path = (string)backupLink.Tag;
        if (path == null)
            return;
        try { Process.Start(new ProcessStartInfo(path) { UseShellExecute = true }); }
        catch (Win32Exception)
        {
            MessageBox.Show(this, path, backupLink.Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }

    [STAThread]
    private static void Main()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new InstallerWindow());
    }
}
