namespace UnrealscriptDevSuite
{
	partial class OptionsPageNewClass
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
			if ( disposing && ( components != null ) )
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
			this.tbNewClassHeader = new System.Windows.Forms.TextBox();
			this.btnImport = new System.Windows.Forms.Button();
			this.panel1 = new System.Windows.Forms.Panel();
			this.panel2 = new System.Windows.Forms.Panel();
			this.panel1.SuspendLayout();
			this.panel2.SuspendLayout();
			this.SuspendLayout();
			// 
			// tbNewClassHeader
			// 
			this.tbNewClassHeader.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tbNewClassHeader.Location = new System.Drawing.Point(0, 0);
			this.tbNewClassHeader.Multiline = true;
			this.tbNewClassHeader.Name = "tbNewClassHeader";
			this.tbNewClassHeader.ScrollBars = System.Windows.Forms.ScrollBars.Both;
			this.tbNewClassHeader.Size = new System.Drawing.Size(395, 260);
			this.tbNewClassHeader.TabIndex = 0;
			// 
			// btnImport
			// 
			this.btnImport.Anchor = ( (System.Windows.Forms.AnchorStyles)( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.btnImport.Location = new System.Drawing.Point(317, 3);
			this.btnImport.Name = "btnImport";
			this.btnImport.Size = new System.Drawing.Size(75, 23);
			this.btnImport.TabIndex = 1;
			this.btnImport.Text = "Import";
			this.btnImport.UseVisualStyleBackColor = true;
			this.btnImport.Click += new System.EventHandler(this.btnImport_Click);
			// 
			// panel1
			// 
			this.panel1.Controls.Add(this.tbNewClassHeader);
			this.panel1.Controls.Add(this.panel2);
			this.panel1.Dock = System.Windows.Forms.DockStyle.Fill;
			this.panel1.Location = new System.Drawing.Point(0, 0);
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(395, 289);
			this.panel1.TabIndex = 2;
			// 
			// panel2
			// 
			this.panel2.Controls.Add(this.btnImport);
			this.panel2.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.panel2.Location = new System.Drawing.Point(0, 260);
			this.panel2.Name = "panel2";
			this.panel2.Size = new System.Drawing.Size(395, 29);
			this.panel2.TabIndex = 2;
			// 
			// OptionsPageNewClass
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.panel1);
			this.Name = "OptionsPageNewClass";
			this.Size = new System.Drawing.Size(395, 289);
			this.panel1.ResumeLayout(false);
			this.panel1.PerformLayout();
			this.panel2.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.TextBox tbNewClassHeader;
		private System.Windows.Forms.Button btnImport;
		private System.Windows.Forms.Panel panel1;
		private System.Windows.Forms.Panel panel2;
	}
}
