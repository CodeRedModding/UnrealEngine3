//Copyright (c) Microsoft Corporation.  All rights reserved.

namespace UnrealscriptDevSuite
{
    partial class OptionsPage
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

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
			this.cbForceCompileExe = new System.Windows.Forms.CheckBox();
			this.pnExe = new System.Windows.Forms.Panel();
			this.btnFindCompileEXE = new System.Windows.Forms.Button();
			this.ckAppendDebug = new System.Windows.Forms.CheckBox();
			this.tbCompileEXE = new System.Windows.Forms.TextBox();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.tbAdditionalCmds = new System.Windows.Forms.TextBox();
			this.ckUseCOMFile = new System.Windows.Forms.CheckBox();
			this.ckClearOutputWindow = new System.Windows.Forms.CheckBox();
			this.pnExe.SuspendLayout();
			this.SuspendLayout();
			// 
			// cbForceCompileExe
			// 
			this.cbForceCompileExe.AutoSize = true;
			this.cbForceCompileExe.Location = new System.Drawing.Point(23, 12);
			this.cbForceCompileExe.Name = "cbForceCompileExe";
			this.cbForceCompileExe.Size = new System.Drawing.Size(197, 17);
			this.cbForceCompileExe.TabIndex = 0;
			this.cbForceCompileExe.Text = "Force Compile to use a specific EXE";
			this.cbForceCompileExe.UseVisualStyleBackColor = true;
			this.cbForceCompileExe.CheckedChanged += new System.EventHandler(this.cbForceCompileExe_CheckedChanged);
			// 
			// pnExe
			// 
			this.pnExe.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.pnExe.Controls.Add(this.btnFindCompileEXE);
			this.pnExe.Controls.Add(this.ckAppendDebug);
			this.pnExe.Controls.Add(this.tbCompileEXE);
			this.pnExe.Controls.Add(this.label1);
			this.pnExe.Enabled = false;
			this.pnExe.Location = new System.Drawing.Point(14, 21);
			this.pnExe.Name = "pnExe";
			this.pnExe.Size = new System.Drawing.Size(368, 87);
			this.pnExe.TabIndex = 1;
			// 
			// btnFindCompileEXE
			// 
			this.btnFindCompileEXE.Location = new System.Drawing.Point(333, 29);
			this.btnFindCompileEXE.Name = "btnFindCompileEXE";
			this.btnFindCompileEXE.Size = new System.Drawing.Size(25, 23);
			this.btnFindCompileEXE.TabIndex = 1;
			this.btnFindCompileEXE.Text = "...";
			this.btnFindCompileEXE.UseVisualStyleBackColor = true;
			this.btnFindCompileEXE.Click += new System.EventHandler(this.btnFindCompileEXE_Click);
			// 
			// ckAppendDebug
			// 
			this.ckAppendDebug.AutoSize = true;
			this.ckAppendDebug.Enabled = false;
			this.ckAppendDebug.Location = new System.Drawing.Point(12, 60);
			this.ckAppendDebug.Name = "ckAppendDebug";
			this.ckAppendDebug.Size = new System.Drawing.Size(342, 17);
			this.ckAppendDebug.TabIndex = 4;
			this.ckAppendDebug.Text = "Append \"DEBUG-\" to executable name when VS is in debug mode";
			this.ckAppendDebug.UseVisualStyleBackColor = true;
			// 
			// tbCompileEXE
			// 
			this.tbCompileEXE.Location = new System.Drawing.Point(12, 30);
			this.tbCompileEXE.Name = "tbCompileEXE";
			this.tbCompileEXE.ReadOnly = true;
			this.tbCompileEXE.Size = new System.Drawing.Size(315, 20);
			this.tbCompileEXE.TabIndex = 0;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(9, 14);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(154, 13);
			this.label1.TabIndex = 2;
			this.label1.Text = "Executable to use for compiling";
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(7, 124);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(234, 13);
			this.label2.TabIndex = 0;
			this.label2.Text = "Additional Command line to use during Compiling";
			// 
			// tbAdditionalCmds
			// 
			this.tbAdditionalCmds.Location = new System.Drawing.Point(10, 140);
			this.tbAdditionalCmds.Name = "tbAdditionalCmds";
			this.tbAdditionalCmds.Size = new System.Drawing.Size(372, 20);
			this.tbAdditionalCmds.TabIndex = 2;
			// 
			// ckUseCOMFile
			// 
			this.ckUseCOMFile.AutoSize = true;
			this.ckUseCOMFile.Checked = true;
			this.ckUseCOMFile.CheckState = System.Windows.Forms.CheckState.Checked;
			this.ckUseCOMFile.Location = new System.Drawing.Point(10, 175);
			this.ckUseCOMFile.Name = "ckUseCOMFile";
			this.ckUseCOMFile.Size = new System.Drawing.Size(167, 17);
			this.ckUseCOMFile.TabIndex = 4;
			this.ckUseCOMFile.Text = "Use .COM file instead of .EXE";
			this.ckUseCOMFile.UseVisualStyleBackColor = true;
			// 
			// ckClearOutputWindow
			// 
			this.ckClearOutputWindow.AutoSize = true;
			this.ckClearOutputWindow.Checked = true;
			this.ckClearOutputWindow.CheckState = System.Windows.Forms.CheckState.Checked;
			this.ckClearOutputWindow.Location = new System.Drawing.Point(10, 198);
			this.ckClearOutputWindow.Name = "ckClearOutputWindow";
			this.ckClearOutputWindow.Size = new System.Drawing.Size(229, 17);
			this.ckClearOutputWindow.TabIndex = 4;
			this.ckClearOutputWindow.Text = "Clear Output Window when compile begins";
			this.ckClearOutputWindow.UseVisualStyleBackColor = true;
			// 
			// OptionsPage
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.ckClearOutputWindow);
			this.Controls.Add(this.ckUseCOMFile);
			this.Controls.Add(this.cbForceCompileExe);
			this.Controls.Add(this.tbAdditionalCmds);
			this.Controls.Add(this.pnExe);
			this.Controls.Add(this.label2);
			this.Name = "OptionsPage";
			this.Size = new System.Drawing.Size(395, 289);
			this.pnExe.ResumeLayout(false);
			this.pnExe.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

        }

        #endregion

		private System.Windows.Forms.CheckBox cbForceCompileExe;
		private System.Windows.Forms.Panel pnExe;
		private System.Windows.Forms.Button btnFindCompileEXE;
		private System.Windows.Forms.TextBox tbCompileEXE;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TextBox tbAdditionalCmds;
		private System.Windows.Forms.CheckBox ckAppendDebug;
		private System.Windows.Forms.CheckBox ckUseCOMFile;
		private System.Windows.Forms.CheckBox ckClearOutputWindow;
    }
}
