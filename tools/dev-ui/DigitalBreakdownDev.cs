using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

internal sealed class DevForm : Form
{
    private readonly string repoRoot;
    private readonly string dbdevPath;
    private readonly Label sourceLabel = new Label();
    private readonly Label desktopLabel = new Label();
    private readonly Label androidLabel = new Label();
    private readonly Label releaseLabel = new Label();
    private readonly Label operationLabel = new Label();
    private readonly TextBox outputBox = new TextBox();
    private readonly Panel advancedPanel = new Panel();
    private readonly Button advancedToggle = new Button();
    private bool busy;

    public DevForm()
    {
        repoRoot = FindRepoRoot(AppDomain.CurrentDomain.BaseDirectory);
        dbdevPath = repoRoot == null ? null : Path.Combine(repoRoot, "tools", "dbdev.ps1");

        Text = "Digital Breakdown Dev";
        StartPosition = FormStartPosition.CenterScreen;
        ClientSize = new Size(620, 690);
        MinimumSize = new Size(620, 690);
        BackColor = Color.FromArgb(9, 14, 11);
        ForeColor = Color.FromArgb(166, 255, 186);
        Font = new Font("Consolas", 10.0f, FontStyle.Regular);
        FormBorderStyle = FormBorderStyle.Sizable;

        BuildUi();
        Shown += async delegate { await RefreshStatusAsync(); };
    }

    private void BuildUi()
    {
        var title = new Label
        {
            Text = "DIGITAL BREAKDOWN DEV",
            Font = new Font("Consolas", 18.0f, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(24, 22)
        };
        Controls.Add(title);

        var subtitle = new Label
        {
            Text = "local iteration / device test / published release",
            AutoSize = true,
            ForeColor = Color.FromArgb(110, 175, 125),
            Location = new Point(27, 57)
        };
        Controls.Add(subtitle);

        var statusPanel = new Panel
        {
            BorderStyle = BorderStyle.FixedSingle,
            Location = new Point(24, 90),
            Size = new Size(570, 126),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
        };
        Controls.Add(statusPanel);

        ConfigureStatusLabel(sourceLabel, "LOCAL", 12);
        ConfigureStatusLabel(desktopLabel, "DESKTOP", 39);
        ConfigureStatusLabel(androidLabel, "STYLO 4", 66);
        ConfigureStatusLabel(releaseLabel, "RELEASE", 93);
        statusPanel.Controls.Add(sourceLabel);
        statusPanel.Controls.Add(desktopLabel);
        statusPanel.Controls.Add(androidLabel);
        statusPanel.Controls.Add(releaseLabel);

        var runDesktop = MakePrimaryButton("RUN LOCAL DESKTOP", 238);
        runDesktop.Click += async delegate { await RunCommandAsync("desktop-run", "Building and launching local desktop..."); };
        Controls.Add(runDesktop);

        var testAndroid = MakePrimaryButton("TEST ON STYLO 4", 302);
        testAndroid.Click += async delegate { await RunCommandAsync("android-stream", "Building, installing, and streaming Android..."); };
        Controls.Add(testAndroid);

        var runRelease = MakePrimaryButton("RUN LATEST RELEASE", 366);
        runRelease.Click += async delegate { await RunCommandAsync("release-windows", "Downloading and launching verified release..."); };
        Controls.Add(runRelease);

        operationLabel.Text = "READY";
        operationLabel.AutoSize = false;
        operationLabel.TextAlign = ContentAlignment.MiddleLeft;
        operationLabel.Location = new Point(24, 431);
        operationLabel.Size = new Size(570, 28);
        operationLabel.ForeColor = Color.FromArgb(110, 175, 125);
        operationLabel.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        Controls.Add(operationLabel);

        advancedToggle.Text = "ADVANCED +";
        advancedToggle.FlatStyle = FlatStyle.Flat;
        advancedToggle.FlatAppearance.BorderColor = Color.FromArgb(72, 125, 82);
        advancedToggle.BackColor = BackColor;
        advancedToggle.ForeColor = ForeColor;
        advancedToggle.Location = new Point(24, 468);
        advancedToggle.Size = new Size(570, 36);
        advancedToggle.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        advancedToggle.Click += delegate
        {
            advancedPanel.Visible = !advancedPanel.Visible;
            advancedToggle.Text = advancedPanel.Visible ? "ADVANCED -" : "ADVANCED +";
        };
        Controls.Add(advancedToggle);

        advancedPanel.Location = new Point(24, 512);
        advancedPanel.Size = new Size(570, 55);
        advancedPanel.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        advancedPanel.Visible = false;
        Controls.Add(advancedPanel);

        AddAdvancedButton("REFRESH", 0, async delegate { await RefreshStatusAsync(); });
        AddAdvancedButton("SYNC", 114, async delegate { await RunCommandAsync("sync", "Syncing GitHub main..."); });
        AddAdvancedButton("DIAGNOSTICS", 228, async delegate { await RunCommandAsync("diagnostics", "Collecting diagnostics..."); });
        AddAdvancedButton("OPEN LOGS", 342, delegate { OpenLogs(); return Task.CompletedTask; });
        AddAdvancedButton("ACTIONS", 456, delegate { OpenUrl("https://github.com/indrolend/digital-breakdown-apk/actions/workflows/native-release.yml"); return Task.CompletedTask; });

        outputBox.Multiline = true;
        outputBox.ReadOnly = true;
        outputBox.ScrollBars = ScrollBars.Vertical;
        outputBox.BackColor = Color.FromArgb(4, 8, 6);
        outputBox.ForeColor = Color.FromArgb(138, 215, 154);
        outputBox.BorderStyle = BorderStyle.FixedSingle;
        outputBox.Font = new Font("Consolas", 9.0f);
        outputBox.Location = new Point(24, 578);
        outputBox.Size = new Size(570, 86);
        outputBox.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
        Controls.Add(outputBox);
    }

    private void ConfigureStatusLabel(Label label, string name, int y)
    {
        label.Text = name.PadRight(10) + "checking...";
        label.AutoSize = false;
        label.Location = new Point(12, y);
        label.Size = new Size(544, 22);
        label.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
    }

    private Button MakePrimaryButton(string text, int y)
    {
        var button = new Button
        {
            Text = text,
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(14, 29, 18),
            ForeColor = ForeColor,
            Font = new Font("Consolas", 12.0f, FontStyle.Bold),
            Location = new Point(24, y),
            Size = new Size(570, 50),
            Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
            Cursor = Cursors.Hand
        };
        button.FlatAppearance.BorderColor = Color.FromArgb(86, 172, 104);
        button.FlatAppearance.MouseOverBackColor = Color.FromArgb(22, 45, 27);
        return button;
    }

    private void AddAdvancedButton(string text, int x, Func<Task> action)
    {
        var button = new Button
        {
            Text = text,
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(11, 20, 14),
            ForeColor = ForeColor,
            Location = new Point(x, 0),
            Size = new Size(106, 42)
        };
        button.FlatAppearance.BorderColor = Color.FromArgb(72, 125, 82);
        button.Click += async delegate { await action(); };
        advancedPanel.Controls.Add(button);
    }

    private async Task RefreshStatusAsync()
    {
        if (busy) return;
        if (repoRoot == null || !File.Exists(dbdevPath))
        {
            sourceLabel.Text = "LOCAL     repository not found";
            desktopLabel.Text = "DESKTOP   unavailable";
            androidLabel.Text = "STYLO 4   unavailable";
            releaseLabel.Text = "RELEASE   unavailable";
            operationLabel.Text = "Place DigitalBreakdownDev.exe inside the repository or a child folder.";
            return;
        }

        var result = await InvokeDbdevAsync("status");
        outputBox.Text = result.Output;
        ParseStatus(result.Output);
        operationLabel.Text = result.ExitCode == 0 ? "READY" : "STATUS FAILED";
    }

    private void ParseStatus(string output)
    {
        string commit = ValueFor(output, "Commit");
        string desktop = ValueFor(output, "Desktop");
        string android = ValueFor(output, "Android");

        sourceLabel.Text = "LOCAL     " + (string.IsNullOrEmpty(commit) ? "unknown" : commit);
        desktopLabel.Text = "DESKTOP   " + (string.IsNullOrEmpty(desktop) ? "unknown" : (desktop == "not built" ? "not built" : "ready"));
        androidLabel.Text = "STYLO 4   " + (string.IsNullOrEmpty(android) ? "unknown" : android);

        string statePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "DigitalBreakdown", "release-state.json");
        releaseLabel.Text = "RELEASE   " + (File.Exists(statePath) ? "downloaded" : "available online");
    }

    private static string ValueFor(string output, string key)
    {
        if (string.IsNullOrEmpty(output)) return null;
        foreach (string raw in output.Replace("\r", "").Split('\n'))
        {
            int colon = raw.IndexOf(':');
            if (colon < 0) continue;
            if (raw.Substring(0, colon).Trim().Equals(key, StringComparison.OrdinalIgnoreCase))
                return raw.Substring(colon + 1).Trim();
        }
        return null;
    }

    private async Task RunCommandAsync(string command, string progress)
    {
        if (busy) return;
        if (repoRoot == null || !File.Exists(dbdevPath))
        {
            MessageBox.Show(this, "Repository or tools\\dbdev.ps1 was not found.", "Digital Breakdown Dev", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        busy = true;
        SetButtonsEnabled(false);
        operationLabel.Text = progress;
        outputBox.Clear();

        try
        {
            var result = await InvokeDbdevAsync(command);
            outputBox.Text = result.Output;
            operationLabel.Text = result.ExitCode == 0 ? "SUCCESS" : "FAILED — SEE OUTPUT";
            if (result.ExitCode != 0)
                MessageBox.Show(this, result.Output, "Operation failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        catch (Exception ex)
        {
            outputBox.Text = ex.ToString();
            operationLabel.Text = "FAILED — SEE OUTPUT";
        }
        finally
        {
            busy = false;
            SetButtonsEnabled(true);
            await RefreshStatusAsync();
        }
    }

    private Task<CommandResult> InvokeDbdevAsync(string command)
    {
        return Task.Run(delegate
        {
            var psi = new ProcessStartInfo
            {
                FileName = "powershell.exe",
                Arguments = "-NoProfile -ExecutionPolicy Bypass -File \"" + dbdevPath + "\" " + command,
                WorkingDirectory = repoRoot,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            var sb = new StringBuilder();
            using (var process = new Process { StartInfo = psi })
            {
                process.Start();
                sb.Append(process.StandardOutput.ReadToEnd());
                sb.Append(process.StandardError.ReadToEnd());
                process.WaitForExit();
                return new CommandResult(process.ExitCode, sb.ToString().Trim());
            }
        });
    }

    private void SetButtonsEnabled(bool enabled)
    {
        foreach (Control control in Controls)
            if (control is Button) control.Enabled = enabled;
        foreach (Control control in advancedPanel.Controls)
            if (control is Button) control.Enabled = enabled;
    }

    private void OpenLogs()
    {
        string path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "DigitalBreakdownDev", "logs");
        Directory.CreateDirectory(path);
        Process.Start("explorer.exe", "\"" + path + "\"");
    }

    private static void OpenUrl(string url)
    {
        Process.Start(new ProcessStartInfo(url) { UseShellExecute = true });
    }

    private static string FindRepoRoot(string start)
    {
        DirectoryInfo current = new DirectoryInfo(start);
        for (int i = 0; current != null && i < 8; i++, current = current.Parent)
        {
            if (File.Exists(Path.Combine(current.FullName, "tools", "dbdev.ps1")))
                return current.FullName;
        }
        return null;
    }

    private sealed class CommandResult
    {
        public readonly int ExitCode;
        public readonly string Output;
        public CommandResult(int exitCode, string output)
        {
            ExitCode = exitCode;
            Output = output;
        }
    }
}

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new DevForm());
    }
}
