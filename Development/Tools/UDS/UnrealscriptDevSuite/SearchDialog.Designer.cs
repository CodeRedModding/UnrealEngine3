namespace UnrealscriptDevSuite
{
	partial class SearchDialog
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

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.label1 = new System.Windows.Forms.Label();
			this.tbSearchFor = new System.Windows.Forms.TextBox();
			this.panel1 = new System.Windows.Forms.Panel();
			this.label2 = new System.Windows.Forms.Label();
			this.rbMatchCase = new System.Windows.Forms.RadioButton();
			this.rbWholeWords = new System.Windows.Forms.RadioButton();
			this.label3 = new System.Windows.Forms.Label();
			this.cbDirection = new System.Windows.Forms.ComboBox();
			this.button1 = new System.Windows.Forms.Button();
			this.ckFind2 = new System.Windows.Forms.CheckBox();
			this.panel1.SuspendLayout();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(13, 13);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(59, 13);
			this.label1.TabIndex = 0;
			this.label1.Text = "Find What:";
			// 
			// tbSearchFor
			// 
			this.tbSearchFor.Location = new System.Drawing.Point(16, 29);
			this.tbSearchFor.Name = "tbSearchFor";
			this.tbSearchFor.Size = new System.Drawing.Size(299, 20);
			this.tbSearchFor.TabIndex = 1;
			// 
			// panel1
			// 
			this.panel1.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.panel1.Controls.Add(this.cbDirection);
			this.panel1.Controls.Add(this.label3);
			this.panel1.Controls.Add(this.rbWholeWords);
			this.panel1.Controls.Add(this.rbMatchCase);
			this.panel1.Location = new System.Drawing.Point(16, 65);
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(299, 107);
			this.panel1.TabIndex = 2;
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(22, 57);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(66, 13);
			this.label2.TabIndex = 3;
			this.label2.Text = "Find Options";
			// 
			// rbMatchCase
			// 
			this.rbMatchCase.AutoSize = true;
			this.rbMatchCase.Location = new System.Drawing.Point(8, 52);
			this.rbMatchCase.Name = "rbMatchCase";
			this.rbMatchCase.Size = new System.Drawing.Size(82, 17);
			this.rbMatchCase.TabIndex = 0;
			this.rbMatchCase.TabStop = true;
			this.rbMatchCase.Text = "Match Case";
			this.rbMatchCase.UseVisualStyleBackColor = true;
			// 
			// rbWholeWords
			// 
			this.rbWholeWords.AutoSize = true;
			this.rbWholeWords.Location = new System.Drawing.Point(8, 75);
			this.rbWholeWords.Name = "rbWholeWords";
			this.rbWholeWords.Size = new System.Drawing.Size(90, 17);
			this.rbWholeWords.TabIndex = 0;
			this.rbWholeWords.TabStop = true;
			this.rbWholeWords.Text = "Whole Words";
			this.rbWholeWords.UseVisualStyleBackColor = true;
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(4, 8);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(49, 13);
			this.label3.TabIndex = 1;
			this.label3.Text = "Direction";
			// 
			// cbDirection
			// 
			this.cbDirection.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.cbDirection.FormattingEnabled = true;
			this.cbDirection.Items.AddRange(new object[] {
            "Parents and Children",
            "Parents",
            "Children"});
			this.cbDirection.Location = new System.Drawing.Point(8, 24);
			this.cbDirection.Name = "cbDirection";
			this.cbDirection.Size = new System.Drawing.Size(272, 21);
			this.cbDirection.TabIndex = 2;
			// 
			// button1
			// 
			this.button1.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.button1.Location = new System.Drawing.Point(240, 178);
			this.button1.Name = "button1";
			this.button1.Size = new System.Drawing.Size(75, 23);
			this.button1.TabIndex = 4;
			this.button1.Text = "Find All";
			this.button1.UseVisualStyleBackColor = true;
			// 
			// ckFind2
			// 
			this.ckFind2.AutoSize = true;
			this.ckFind2.Location = new System.Drawing.Point(16, 178);
			this.ckFind2.Name = "ckFind2";
			this.ckFind2.Size = new System.Drawing.Size(82, 17);
			this.ckFind2.TabIndex = 5;
			this.ckFind2.Text = "Use FIND 2";
			this.ckFind2.UseVisualStyleBackColor = true;
			// 
			// SearchDialog
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(327, 206);
			this.Controls.Add(this.ckFind2);
			this.Controls.Add(this.button1);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.panel1);
			this.Controls.Add(this.tbSearchFor);
			this.Controls.Add(this.label1);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
			this.Name = "SearchDialog";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Search Lineage";
			this.panel1.ResumeLayout(false);
			this.panel1.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.TextBox tbSearchFor;
		private System.Windows.Forms.Panel panel1;
		private System.Windows.Forms.RadioButton rbMatchCase;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.ComboBox cbDirection;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.RadioButton rbWholeWords;
		private System.Windows.Forms.Button button1;
		private System.Windows.Forms.CheckBox ckFind2;
	}
}