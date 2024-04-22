using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;

namespace UnrealConsole
{
    public partial class ThunkingProgressDialog : Form
    {
        Process ThunkedProcess = new Process();
        int LastRolloutHeight = 100;
        int RolloutPadding = 10;

        bool bIsDraggingForm = false;
        Point MouseDragOrigin;

        private UnrealControls.OutputWindowDocument MyDocument = new UnrealControls.OutputWindowDocument();

        string[] Args;

        List<string> QueuedLines = new List<string>();

        public ThunkingProgressDialog()
        {
            InitializeComponent();
        }

        public void StartApp(string[] InArgs)
        {
            Args = InArgs;
            Show();
        }

        void ShowRollout()
        {
            if (!DetailsView.Visible)
            {
                DetailsView.Visible = true;
                Height += LastRolloutHeight + RolloutPadding;

                ToggleDetailsViewButton.Text = "Hide Details";
            }
        }

        void HideRollout()
        {
            if (DetailsView.Visible)
            {
                LastRolloutHeight = DetailsView.Height;

                DetailsView.Visible = false;
                Height -= LastRolloutHeight + RolloutPadding;

                ToggleDetailsViewButton.Text = "Show Details";
            }
        }

        void OutputReceivedCallback(Object Sender, DataReceivedEventArgs Line)
        {
            if ((Line != null) && (Line.Data != null) && (Line.Data != ""))
            {
                lock (QueuedLines)
                {
                    QueuedLines.Add(Line.Data);
                }
            }
        }

        /// <summary>
        /// Hides the titlebar, but allows the window to still have a border and a Text property (showed in alt-tab, etc...)
        /// </summary>
        protected override CreateParams CreateParams
        {
            get
            {
                const int WS_CAPTION = 0x00C00000;
                const int WS_SIZEBOX = 0x00040000;
                var Result = base.CreateParams;
                Result.Style &= ~WS_CAPTION;
                Result.Style |= WS_SIZEBOX;

                return Result;
            }

        }
        private void ThunkingProgressDialog_Load(object sender, EventArgs e)
        {
            // Initialize the GUI
            ErrorOccurredButton.Visible = false;
            RolloutPadding = DetailsView.Top - ToggleDetailsViewButton.Bottom;
            HideRollout();
            DetailsView.Document = MyDocument;

            // Parse the command line arguments
            string ExecutablePath = null;
            string Description = null;
            string WorkingDirectory = null;
            string ParentWorkingDirectory = null;

            int ArgIndex = 1;
            while (ArgIndex < Args.Length)
            {
                string Argument = Args[ArgIndex];
                ArgIndex++;

                if (Argument[0] == '-')
                {
                    if (ArgIndex < Args.Length)
                    {
                        switch (Argument.ToLowerInvariant())
                        {
                            case "-cwd":
                                // Current working directory for child
                                WorkingDirectory = Args[ArgIndex];
                                ArgIndex++;
                                break;
                            case "-pwd":
                                // Parent working directory (used for relative paths to exe)
                                ParentWorkingDirectory = Args[ArgIndex];
                                ArgIndex++;
                                break;
                        }
                    }
                }
                else
                {
                    ExecutablePath = Argument;
                    break;
                }
            }

            // The remaining arguments are for the inner program
            string CommandLine = "";
            if (ArgIndex < Args.Length)
            {
                CommandLine = String.Join(" ", Args, ArgIndex, Args.Length - ArgIndex);
            }

            // Path fixups
            if (ParentWorkingDirectory == null)
            {
                ParentWorkingDirectory = Directory.GetCurrentDirectory();
            }

            if (!Path.IsPathRooted(ExecutablePath))
            {
                ExecutablePath = Path.Combine(ParentWorkingDirectory, ExecutablePath);
            }

            if (WorkingDirectory == null)
            {
                WorkingDirectory = Path.GetDirectoryName(ExecutablePath);
            }
            else if (!Path.IsPathRooted(WorkingDirectory))
            {
                WorkingDirectory = Path.Combine(ParentWorkingDirectory, WorkingDirectory);
            }

            // Setup the last bits of GUI
            if (Description == null)
            {
                Description = Path.GetFileNameWithoutExtension(ExecutablePath);
            }
            ApplicationNameLabel.Text = Description;
            Text = Description;
            CurrentStepLabel.Text = "";

            // Start the executable
            MyDocument.AppendLine(Color.DarkBlue, String.Format("Running {0} {1}...", Path.GetFileName(ExecutablePath), CommandLine));
            ThunkedProcess.StartInfo.FileName = ExecutablePath;
            ThunkedProcess.StartInfo.UseShellExecute = false;
            ThunkedProcess.StartInfo.Arguments = CommandLine;
            ThunkedProcess.StartInfo.RedirectStandardOutput = true;
            ThunkedProcess.StartInfo.RedirectStandardError = true;
            ThunkedProcess.StartInfo.CreateNoWindow = true;
            ThunkedProcess.StartInfo.WorkingDirectory = WorkingDirectory;
            ThunkedProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedCallback);
            ThunkedProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedCallback);

            try
            {
                ThunkedProcess.Start();

                ThunkedProcess.BeginOutputReadLine();
                ThunkedProcess.BeginErrorReadLine();

                timer1.Enabled = true;
            }
            catch (Exception ex)
            {
                ThunkedProcess = null;

                MyDocument.AppendLine(null, "");
                MyDocument.AppendLine(Color.DarkBlue, String.Format("Failed to start program {0}", ex.Message));

                ApplicationHasExited(1);
            }
        }

        void ApplicationHasExited(int ExitCode)
        {
            Program.ExitCode = ExitCode;

            if (Program.ExitCode > 0)
            {
                // Error occurred: pop out the details display and show the 'error occurred, OK' button
                ToggleDetailsViewButton.Visible = false;
                ErrorOccurredButton.Visible = true;

                progressBar1.BackColor = Color.Red;
                progressBar1.Style = ProgressBarStyle.Blocks;
                progressBar1.Value = 0;
                progressBar1.MarqueeAnimationSpeed = 0;
                
                System.Console.Beep();

                ShowRollout();

                // Bring this window to the foreground
                this.Activate();
            }
            else
            {
                // Successfully completed, close silently
                Close();
            }
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            // Flush pending output
            lock (QueuedLines)
            {
                string NewPhaseStatus = null;

                foreach (string Line in QueuedLines)
                {
                    // Check for special lines that adjust color
                    Color? LineColor = null;
                    if (Line.ToUpperInvariant().Contains("ERROR:"))
                    {
                        LineColor = Color.DarkRed;
                    }
                    else if (Line.ToUpperInvariant().Contains("WARNING:"))
                    {
                        LineColor = Color.DarkOliveGreen;
                    }

                    // Add the line
                    MyDocument.AppendLine(LineColor, Line);
                    Console.WriteLine(Line);
                    NewPhaseStatus = Line;
                }
                QueuedLines.Clear();

                // Update the phase status
                if (NewPhaseStatus != null)
                {
                    CurrentStepLabel.Text = NewPhaseStatus;
                }
            }

            // Check for application completion
            if ((ThunkedProcess != null) && (ThunkedProcess.HasExited))
            {
                MyDocument.AppendLine(null, "");
                MyDocument.AppendLine(Color.DarkBlue, String.Format("Program exited with return code {0}", ThunkedProcess.ExitCode));

                ApplicationHasExited(ThunkedProcess.ExitCode);
                ThunkedProcess = null;
            }
        }

        private void ToggleDetailsViewButton_Click(object sender, EventArgs e)
        {
            if (DetailsView.Visible)
            {
                HideRollout();
            }
            else
            {
                ShowRollout();
            }
        }

        private void ErrorOccuredButton_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void ThunkingProgressDialog_FormClosing(object sender, FormClosingEventArgs e)
        {
            if ((ThunkedProcess != null) && (ThunkedProcess.Handle != null) && (!ThunkedProcess.HasExited))
            {
                switch (e.CloseReason)
                {
                    case CloseReason.TaskManagerClosing:
                    case CloseReason.WindowsShutDown:
                        // In these cases, we kill the spawned process
                        ThunkedProcess.Kill();
                        Program.ExitCode = 1;
                        break;
                    default:
                        // Otherwise, cancel the close.  The user is going to have to wait.
                        e.Cancel = true;
                        break;
                }
            }
        }

        private void ThunkingProgressDialog_MouseMove(object sender, MouseEventArgs e)
        {
            if (bIsDraggingForm)
            {
                Point CurrentPos = PointToScreen(e.Location);
                Location = new Point(CurrentPos.X - MouseDragOrigin.X, CurrentPos.Y - MouseDragOrigin.Y);
            }
        }

        private void ThunkingProgressDialog_MouseDown(object sender, MouseEventArgs e)
        {
            bIsDraggingForm = true;
            MouseDragOrigin = e.Location;
        }

        private void ThunkingProgressDialog_MouseUp(object sender, MouseEventArgs e)
        {
            bIsDraggingForm = false;
        }
    }
}
