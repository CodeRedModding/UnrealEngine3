namespace ProfNodeVisualizer
{
    partial class Form1
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
            this.ProfNodeView = new System.Windows.Forms.TreeView();
            this.contextMenuStrip1 = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.exportFromHereToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.menuStrip1 = new System.Windows.Forms.MenuStrip();
            this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.loadFileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.exitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.ToolStripTextBox1 = new System.Windows.Forms.ToolStripTextBox();
            this.statusStrip1 = new System.Windows.Forms.StatusStrip();
            this.ToolStripStatusLabel1 = new System.Windows.Forms.ToolStripStatusLabel();
            this.TimeToCursorLabel = new System.Windows.Forms.ToolStripStatusLabel();
            this.optionsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.setRootDisplayThresholdToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.contextMenuStrip1.SuspendLayout();
            this.menuStrip1.SuspendLayout();
            this.statusStrip1.SuspendLayout();
            this.SuspendLayout();
            // 
            // ProfNodeView
            // 
            this.ProfNodeView.AllowDrop = true;
            this.ProfNodeView.ContextMenuStrip = this.contextMenuStrip1;
            this.ProfNodeView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.ProfNodeView.Location = new System.Drawing.Point(0, 27);
            this.ProfNodeView.Margin = new System.Windows.Forms.Padding(3, 3, 3, 20);
            this.ProfNodeView.Name = "ProfNodeView";
            this.ProfNodeView.Size = new System.Drawing.Size(767, 420);
            this.ProfNodeView.TabIndex = 0;
            this.ProfNodeView.NodeMouseClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(this.ProfNodeViewNodeMouseClick);
            this.ProfNodeView.DragDrop += new System.Windows.Forms.DragEventHandler(this.ProfNodeViewDragDrop);
            this.ProfNodeView.DragOver += new System.Windows.Forms.DragEventHandler(this.ProfNodeViewDragOver);
            // 
            // contextMenuStrip1
            // 
            this.contextMenuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.exportFromHereToolStripMenuItem});
            this.contextMenuStrip1.Name = "contextMenuStrip1";
            this.contextMenuStrip1.Size = new System.Drawing.Size(176, 26);
            // 
            // exportFromHereToolStripMenuItem
            // 
            this.exportFromHereToolStripMenuItem.Name = "exportFromHereToolStripMenuItem";
            this.exportFromHereToolStripMenuItem.Size = new System.Drawing.Size(175, 22);
            this.exportFromHereToolStripMenuItem.Text = "Export From Here...";
            this.exportFromHereToolStripMenuItem.Click += new System.EventHandler(this.ExportFromHereToolStripMenuItemClick);
            // 
            // menuStrip1
            // 
            this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.ToolStripTextBox1,
            this.optionsToolStripMenuItem});
            this.menuStrip1.Location = new System.Drawing.Point(0, 0);
            this.menuStrip1.Name = "menuStrip1";
            this.menuStrip1.Size = new System.Drawing.Size(767, 27);
            this.menuStrip1.TabIndex = 1;
            this.menuStrip1.Text = "menuStrip1";
            // 
            // fileToolStripMenuItem
            // 
            this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.loadFileToolStripMenuItem,
            this.exitToolStripMenuItem});
            this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
            this.fileToolStripMenuItem.Size = new System.Drawing.Size(37, 23);
            this.fileToolStripMenuItem.Text = "&File";
            // 
            // loadFileToolStripMenuItem
            // 
            this.loadFileToolStripMenuItem.Name = "loadFileToolStripMenuItem";
            this.loadFileToolStripMenuItem.Size = new System.Drawing.Size(130, 22);
            this.loadFileToolStripMenuItem.Text = "&Load File...";
            this.loadFileToolStripMenuItem.Click += new System.EventHandler(this.LoadFileToolStripMenuItemClick);
            // 
            // exitToolStripMenuItem
            // 
            this.exitToolStripMenuItem.Name = "exitToolStripMenuItem";
            this.exitToolStripMenuItem.Size = new System.Drawing.Size(130, 22);
            this.exitToolStripMenuItem.Text = "E&xit";
            this.exitToolStripMenuItem.Click += new System.EventHandler(this.ExitToolStripMenuItemClick);
            // 
            // ToolStripTextBox1
            // 
            this.ToolStripTextBox1.Alignment = System.Windows.Forms.ToolStripItemAlignment.Right;
            this.ToolStripTextBox1.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.ToolStripTextBox1.ForeColor = System.Drawing.SystemColors.WindowFrame;
            this.ToolStripTextBox1.Name = "ToolStripTextBox1";
            this.ToolStripTextBox1.Size = new System.Drawing.Size(125, 23);
            this.ToolStripTextBox1.Text = "Type to search...";
            this.ToolStripTextBox1.KeyDown += new System.Windows.Forms.KeyEventHandler(this.ToolStripSearchBoxKeyDown);
            this.ToolStripTextBox1.Click += new System.EventHandler(this.ToolStripTextBoxClick);
            // 
            // statusStrip1
            // 
            this.statusStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.ToolStripStatusLabel1,
            this.TimeToCursorLabel});
            this.statusStrip1.Location = new System.Drawing.Point(0, 425);
            this.statusStrip1.Name = "statusStrip1";
            this.statusStrip1.Size = new System.Drawing.Size(767, 22);
            this.statusStrip1.TabIndex = 2;
            this.statusStrip1.Text = "statusStrip1";
            // 
            // ToolStripStatusLabel1
            // 
            this.ToolStripStatusLabel1.Name = "ToolStripStatusLabel1";
            this.ToolStripStatusLabel1.Size = new System.Drawing.Size(132, 17);
            this.ToolStripStatusLabel1.Text = "Root Display Threshold:";
            this.ToolStripStatusLabel1.Click += new System.EventHandler(this.ToolStripStatusLabelClick);
            // 
            // TimeToCursorLabel
            // 
            this.TimeToCursorLabel.Name = "TimeToCursorLabel";
            this.TimeToCursorLabel.Size = new System.Drawing.Size(89, 17);
            this.TimeToCursorLabel.Text = "Time to Cursor:";
            // 
            // optionsToolStripMenuItem
            // 
            this.optionsToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.setRootDisplayThresholdToolStripMenuItem});
            this.optionsToolStripMenuItem.Name = "optionsToolStripMenuItem";
            this.optionsToolStripMenuItem.Size = new System.Drawing.Size(61, 23);
            this.optionsToolStripMenuItem.Text = "&Options";
            // 
            // setRootDisplayThresholdToolStripMenuItem
            // 
            this.setRootDisplayThresholdToolStripMenuItem.Name = "setRootDisplayThresholdToolStripMenuItem";
            this.setRootDisplayThresholdToolStripMenuItem.Size = new System.Drawing.Size(224, 22);
            this.setRootDisplayThresholdToolStripMenuItem.Text = "Set &Root Display Threshold...";
            this.setRootDisplayThresholdToolStripMenuItem.Click += new System.EventHandler(this.SetRootDisplayThresholdToolStripMenuItemClick);
            // 
            // Form1
            // 
            this.AllowDrop = true;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(767, 447);
            this.Controls.Add(this.statusStrip1);
            this.Controls.Add(this.ProfNodeView);
            this.Controls.Add(this.menuStrip1);
            this.MainMenuStrip = this.menuStrip1;
            this.Name = "Form1";
            this.Text = "ProfNode Visualizer";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.FormFormClosed);
            this.contextMenuStrip1.ResumeLayout(false);
            this.menuStrip1.ResumeLayout(false);
            this.menuStrip1.PerformLayout();
            this.statusStrip1.ResumeLayout(false);
            this.statusStrip1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TreeView ProfNodeView;
        private System.Windows.Forms.MenuStrip menuStrip1;
        private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem loadFileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem exitToolStripMenuItem;
        private System.Windows.Forms.ToolStripTextBox ToolStripTextBox1;
        private System.Windows.Forms.ContextMenuStrip contextMenuStrip1;
        private System.Windows.Forms.ToolStripMenuItem exportFromHereToolStripMenuItem;
        private System.Windows.Forms.StatusStrip statusStrip1;
        private System.Windows.Forms.ToolStripStatusLabel ToolStripStatusLabel1;
        private System.Windows.Forms.ToolStripStatusLabel TimeToCursorLabel;
        private System.Windows.Forms.ToolStripMenuItem optionsToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem setRootDisplayThresholdToolStripMenuItem;
    }
}

