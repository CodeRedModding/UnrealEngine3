namespace Builder.UnrealSync
{
	partial class LogViewer
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose( bool disposing )
		{
			if( disposing && ( components != null ) )
			{
				components.Dispose();
			}
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(LogViewer));
			this.LogSelectionListBox = new System.Windows.Forms.ListBox();
			this.LogViewWindow = new UnrealControls.OutputWindowView();
			this.SuspendLayout();
			// 
			// LogSelectionListBox
			// 
			this.LogSelectionListBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
			this.LogSelectionListBox.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.LogSelectionListBox.FormattingEnabled = true;
			this.LogSelectionListBox.ItemHeight = 14;
			this.LogSelectionListBox.Location = new System.Drawing.Point(3, 3);
			this.LogSelectionListBox.Name = "LogSelectionListBox";
			this.LogSelectionListBox.Size = new System.Drawing.Size(340, 704);
			this.LogSelectionListBox.TabIndex = 0;
			this.LogSelectionListBox.SelectedIndexChanged += new System.EventHandler(this.LogSelectionIndexChanged);
			// 
			// LogViewWindow
			// 
			this.LogViewWindow.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.LogViewWindow.BackColor = System.Drawing.SystemColors.Window;
			this.LogViewWindow.Cursor = System.Windows.Forms.Cursors.Arrow;
			this.LogViewWindow.Document = null;
			this.LogViewWindow.FindTextBackColor = System.Drawing.Color.Yellow;
			this.LogViewWindow.FindTextForeColor = System.Drawing.Color.Black;
			this.LogViewWindow.FindTextLineHighlight = System.Drawing.Color.FromArgb(((int)(((byte)(239)))), ((int)(((byte)(248)))), ((int)(((byte)(255)))));
			this.LogViewWindow.Font = new System.Drawing.Font("Courier New", 9F);
			this.LogViewWindow.ForeColor = System.Drawing.SystemColors.WindowText;
			this.LogViewWindow.Location = new System.Drawing.Point(349, 3);
			this.LogViewWindow.Name = "LogViewWindow";
			this.LogViewWindow.Size = new System.Drawing.Size(960, 706);
			this.LogViewWindow.TabIndex = 3;
			// 
			// LogViewer
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(1310, 721);
			this.Controls.Add(this.LogViewWindow);
			this.Controls.Add(this.LogSelectionListBox);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "LogViewer";
			this.Text = "LogViewer";
			this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.LogViewerClosed);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.ListBox LogSelectionListBox;
		private UnrealControls.OutputWindowView LogViewWindow;
	}
}