namespace UnrealConsole
{
    partial class ThunkingProgressDialog
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ThunkingProgressDialog));
            this.DetailsView = new UnrealControls.OutputWindowView();
            this.progressBar1 = new System.Windows.Forms.ProgressBar();
            this.CurrentStepLabel = new System.Windows.Forms.Label();
            this.ToggleDetailsViewButton = new System.Windows.Forms.Button();
            this.ApplicationNameLabel = new System.Windows.Forms.Label();
            this.timer1 = new System.Windows.Forms.Timer(this.components);
            this.ErrorOccurredButton = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // DetailsView
            // 
            this.DetailsView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
                        | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.DetailsView.BackColor = System.Drawing.SystemColors.Window;
            this.DetailsView.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.DetailsView.Document = null;
            this.DetailsView.FindTextBackColor = System.Drawing.Color.Yellow;
            this.DetailsView.FindTextForeColor = System.Drawing.Color.Black;
            this.DetailsView.FindTextLineHighlight = System.Drawing.Color.FromArgb(((int)(((byte)(239)))), ((int)(((byte)(248)))), ((int)(((byte)(255)))));
            this.DetailsView.Font = new System.Drawing.Font("Courier New", 9F);
            this.DetailsView.ForeColor = System.Drawing.SystemColors.WindowText;
            this.DetailsView.Location = new System.Drawing.Point(12, 146);
            this.DetailsView.Name = "DetailsView";
            this.DetailsView.Size = new System.Drawing.Size(456, 330);
            this.DetailsView.TabIndex = 3;
            // 
            // progressBar1
            // 
            this.progressBar1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.progressBar1.Location = new System.Drawing.Point(32, 50);
            this.progressBar1.Name = "progressBar1";
            this.progressBar1.Size = new System.Drawing.Size(415, 23);
            this.progressBar1.Style = System.Windows.Forms.ProgressBarStyle.Marquee;
            this.progressBar1.TabIndex = 4;
            // 
            // CurrentStepLabel
            // 
            this.CurrentStepLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.CurrentStepLabel.Location = new System.Drawing.Point(29, 85);
            this.CurrentStepLabel.Name = "CurrentStepLabel";
            this.CurrentStepLabel.Size = new System.Drawing.Size(418, 19);
            this.CurrentStepLabel.TabIndex = 5;
            this.CurrentStepLabel.Text = "Step X: Doing foo";
            // 
            // ToggleDetailsViewButton
            // 
            this.ToggleDetailsViewButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.ToggleDetailsViewButton.Location = new System.Drawing.Point(342, 107);
            this.ToggleDetailsViewButton.Name = "ToggleDetailsViewButton";
            this.ToggleDetailsViewButton.Size = new System.Drawing.Size(126, 23);
            this.ToggleDetailsViewButton.TabIndex = 7;
            this.ToggleDetailsViewButton.Text = "Show Details";
            this.ToggleDetailsViewButton.UseVisualStyleBackColor = true;
            this.ToggleDetailsViewButton.Click += new System.EventHandler(this.ToggleDetailsViewButton_Click);
            // 
            // ApplicationNameLabel
            // 
            this.ApplicationNameLabel.AutoSize = true;
            this.ApplicationNameLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.ApplicationNameLabel.Location = new System.Drawing.Point(28, 18);
            this.ApplicationNameLabel.Name = "ApplicationNameLabel";
            this.ApplicationNameLabel.Size = new System.Drawing.Size(181, 20);
            this.ApplicationNameLabel.TabIndex = 8;
            this.ApplicationNameLabel.Text = "Thunk Application Name";
            // 
            // timer1
            // 
            this.timer1.Interval = 50;
            this.timer1.Tick += new System.EventHandler(this.timer1_Tick);
            // 
            // ErrorOccurredButton
            // 
            this.ErrorOccurredButton.Location = new System.Drawing.Point(12, 107);
            this.ErrorOccurredButton.Name = "ErrorOccurredButton";
            this.ErrorOccurredButton.Size = new System.Drawing.Size(156, 23);
            this.ErrorOccurredButton.TabIndex = 9;
            this.ErrorOccurredButton.Text = "Step Failed!  Press to Close";
            this.ErrorOccurredButton.UseVisualStyleBackColor = true;
            this.ErrorOccurredButton.Click += new System.EventHandler(this.ErrorOccuredButton_Click);
            // 
            // ThunkingProgressDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(480, 489);
            this.ControlBox = false;
            this.Controls.Add(this.ErrorOccurredButton);
            this.Controls.Add(this.ApplicationNameLabel);
            this.Controls.Add(this.ToggleDetailsViewButton);
            this.Controls.Add(this.CurrentStepLabel);
            this.Controls.Add(this.progressBar1);
            this.Controls.Add(this.DetailsView);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.SizableToolWindow;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MinimumSize = new System.Drawing.Size(8, 146);
            this.Name = "ThunkingProgressDialog";
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Show;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Progress Bar";
            this.Load += new System.EventHandler(this.ThunkingProgressDialog_Load);
            this.MouseUp += new System.Windows.Forms.MouseEventHandler(this.ThunkingProgressDialog_MouseUp);
            this.MouseDown += new System.Windows.Forms.MouseEventHandler(this.ThunkingProgressDialog_MouseDown);
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.ThunkingProgressDialog_FormClosing);
            this.MouseMove += new System.Windows.Forms.MouseEventHandler(this.ThunkingProgressDialog_MouseMove);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private UnrealControls.OutputWindowView DetailsView;
        private System.Windows.Forms.ProgressBar progressBar1;
        private System.Windows.Forms.Label CurrentStepLabel;
        private System.Windows.Forms.Button ToggleDetailsViewButton;
        private System.Windows.Forms.Label ApplicationNameLabel;
        private System.Windows.Forms.Timer timer1;
        private System.Windows.Forms.Button ErrorOccurredButton;
    }
}