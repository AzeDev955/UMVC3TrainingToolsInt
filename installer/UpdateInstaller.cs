using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;

internal static class UpdateInstaller
{
    private const string ApplyUpdate = "--apply-update";
    private const string InstallUpdate = "--install-update";
    private const int RestoreFailed = 2;
    private const int CloseTimeoutMs = 30000;
    private const int CloseMessage = 0x0010;
    private const uint OwnerWindow = 4;
    private delegate bool WindowCallback(IntPtr window, IntPtr state);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(WindowCallback callback, IntPtr state);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr window);

    [DllImport("user32.dll")]
    private static extern IntPtr GetWindow(IntPtr window, uint command);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool PostMessage(IntPtr window, int message, IntPtr wParam, IntPtr lParam);

    internal static bool IsUpdateCommand(string command)
    {
        return command == ApplyUpdate || command == InstallUpdate;
    }

    internal static int Run(string[] arguments)
    {
        return arguments[0] == ApplyUpdate ? Apply(arguments) : InstallElevated(arguments);
    }

    private static int Apply(string[] arguments)
    {
        int processId;
        long creationTime, toolWindow;
        if (arguments.Length != 4 || !int.TryParse(arguments[1], out processId)
            || !long.TryParse(arguments[2], out creationTime) || !long.TryParse(arguments[3], out toolWindow))
            return 1;

        string executable = null;
        bool closed = false;
        bool installed = false;
        try
        {
            using (Process game = Process.GetProcessById(processId))
            {
                if (game.StartTime.ToUniversalTime().ToFileTimeUtc() != creationTime)
                    throw new InvalidOperationException(Text("La sesión de MVC3 ya cambió.", "The MVC3 session has changed."));
                executable = SteamGames.ResolveExecutable(game.MainModule.FileName);
                if (executable == null)
                    throw new InvalidOperationException(Text("No se encontró la instalación de MVC3.", "The MVC3 installation could not be found."));
                CloseGame(game, new IntPtr(toolWindow));
                closed = true;
            }

            int result = InstallWithPermissions(executable);
            if (result == RestoreFailed)
                return result;
            if (result != 0)
                throw new IOException(Text("Se canceló o falló la instalación.", "The installation was cancelled or failed."));

            installed = true;
            if (!ModInstaller.FindRunningGame(executable).HasValue)
                ModInstaller.LaunchGame(executable);
            return 0;
        }
        catch (InstallRecoveryException error)
        {
            ShowError(Text("No se pudieron restaurar todos los archivos.", "Some files could not be restored.") + "\n" + error.BackupFolder);
            return RestoreFailed;
        }
        catch (Exception error)
        {
            string details = error.Message;
            if (closed && !installed)
            {
                try
                {
                    if (!ModInstaller.FindRunningGame(executable).HasValue)
                        ModInstaller.LaunchGame(executable);
                }
                catch (Exception restartError) { details += "\n" + restartError.Message; }
            }
            ShowError((installed
                ? Text("La actualización se instaló. No se pudo volver a abrir MVC3.", "The update was installed. MVC3 could not be restarted.")
                : Text("No se pudo completar la actualización.", "The update could not be completed.")) + "\n\n" + details);
            return 1;
        }
    }

    private static void CloseGame(Process game, IntPtr toolWindow)
    {
        if (game.HasExited)
            return;
        uint target = (uint)game.Id;
        bool requested = false;
        EnumWindows(delegate(IntPtr window, IntPtr state)
        {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId != target || window == toolWindow || !IsWindowVisible(window) || GetWindow(window, OwnerWindow) != IntPtr.Zero)
                return true;
            requested = PostMessage(window, CloseMessage, IntPtr.Zero, IntPtr.Zero);
            return !requested;
        }, IntPtr.Zero);
        if (!game.HasExited && (!requested || !game.WaitForExit(CloseTimeoutMs)))
            throw new IOException(Text("MVC3 no respondió a la solicitud de cierre.", "MVC3 did not respond to the close request."));
    }

    private static int InstallWithPermissions(string executable)
    {
        try
        {
            ModInstaller.Install(executable);
            return 0;
        }
        catch (UnauthorizedAccessException)
        {
            var start = new ProcessStartInfo(Assembly.GetExecutingAssembly().Location)
            {
                Arguments = InstallUpdate + " \"" + executable + "\"",
                UseShellExecute = true,
                Verb = "runas"
            };
            using (Process elevated = Process.Start(start))
            {
                if (elevated == null)
                    return 1;
                elevated.WaitForExit();
                return elevated.ExitCode;
            }
        }
    }

    private static int InstallElevated(string[] arguments)
    {
        string executable = arguments.Length == 2 ? SteamGames.ResolveExecutable(arguments[1]) : null;
        if (executable == null)
            return 1;
        try
        {
            ModInstaller.Install(executable);
            return 0;
        }
        catch (InstallRecoveryException error)
        {
            ShowError(Text("No se pudieron restaurar todos los archivos.", "Some files could not be restored.") + "\n" + error.BackupFolder);
            return RestoreFailed;
        }
        catch (Exception error)
        {
            ShowError(error.Message);
            return 1;
        }
    }

    private static string Text(string spanish, string english)
    {
        return CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "es" ? spanish : english;
    }

    private static void ShowError(string message)
    {
        MessageBox.Show(message, "UMVC3 Training Tools", MessageBoxButtons.OK, MessageBoxIcon.Warning);
    }
}
