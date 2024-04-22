namespace UnrealscriptDevSuite
{
	partial class AddClass
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
			this.tvSolutionView = new System.Windows.Forms.TreeView();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.cbParentClass = new System.Windows.Forms.ComboBox();
			this.label3 = new System.Windows.Forms.Label();
			this.tbAdditionalParams = new System.Windows.Forms.TextBox();
			this.panel1 = new System.Windows.Forms.Panel();
			this.tbNewClassName = new System.Windows.Forms.TextBox();
			this.label4 = new System.Windows.Forms.Label();
			this.btnOK = new System.Windows.Forms.Button();
			this.btnCancel = new System.Windows.Forms.Button();
			this.panel1.SuspendLayout();
			this.SuspendLayout();
			// 
			// tvSolutionView
			// 
			this.tvSolutionView.Anchor = ( (System.Windows.Forms.AnchorStyles)( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.tvSolutionView.HideSelection = false;
			this.tvSolutionView.Location = new System.Drawing.Point(12, 37);
			this.tvSolutionView.Name = "tvSolutionView";
			this.tvSolutionView.Size = new System.Drawing.Size(226, 264);
			this.tvSolutionView.TabIndex = 2;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(12, 14);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(121, 13);
			this.label1.TabIndex = 1;
			this.label1.Text = "Destination in Solution...";
			// 
			// label2
			// 
			this.label2.Anchor = ( (System.Windows.Forms.AnchorStyles)( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(259, 30);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(79, 13);
			this.label2.TabIndex = 0;
			this.label2.Text = "Class Definition";
			// 
			// cbParentClass
			// 
			this.cbParentClass.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.cbParentClass.FormattingEnabled = true;
			this.cbParentClass.Location = new System.Drawing.Point(221, 10);
			this.cbParentClass.Name = "cbParentClass";
			this.cbParentClass.Size = new System.Drawing.Size(163, 21);
			this.cbParentClass.TabIndex = 1;
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(171, 13);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(44, 13);
			this.label3.TabIndex = 2;
			this.label3.Text = "extends";
			// 
			// tbAdditionalParams
			// 
			this.tbAdditionalParams.Anchor = ( (System.Windows.Forms.AnchorStyles)( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.tbAdditionalParams.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.tbAdditionalParams.Location = new System.Drawing.Point(248, 98);
			this.tbAdditionalParams.Multiline = true;
			this.tbAdditionalParams.Name = "tbAdditionalParams";
			this.tbAdditionalParams.Size = new System.Drawing.Size(385, 155);
			this.tbAdditionalParams.TabIndex = 1;
			// 
			// panel1
			// 
			this.panel1.Anchor = ( (System.Windows.Forms.AnchorStyles)( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.panel1.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.panel1.Controls.Add(this.tbNewClassName);
			this.panel1.Controls.Add(this.cbParentClass);
			this.panel1.Controls.Add(this.label3);
			this.panel1.Location = new System.Drawing.Point(244, 37);
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(389, 38);
			this.panel1.TabIndex = 0;
			// 
			// tbNewClassName
			// 
			this.tbNewClassName.Location = new System.Drawing.Point(4, 10);
			this.tbNewClassName.Name = "tbNewClassName";
			this.tbNewClassName.Size = new System.Drawing.Size(161, 20);
			this.tbNewClassName.TabIndex = 0;
			// 
			// label4
			// 
			this.label4.Anchor = ( (System.Windows.Forms.AnchorStyles)( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.label4.AutoSize = true;
			this.label4.Location = new System.Drawing.Point(255, 88);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(137, 13);
			this.label4.TabIndex = 6;
			this.label4.Text = "Additional Class Parameters";
			// 
			// btnOK
			// 
			this.btnOK.Anchor = ( (System.Windows.Forms.AnchorStyles)( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.btnOK.Location = new System.Drawing.Point(477, 276);
			this.btnOK.Name = "btnOK";
			this.btnOK.Size = new System.Drawing.Size(75, 23);
			this.btnOK.TabIndex = 3;
			this.btnOK.Text = "Create";
			this.btnOK.UseVisualStyleBackColor = true;
			this.btnOK.Click += new System.EventHandler(this.btnOK_Click);
			// 
			// btnCancel
			// 
			this.btnCancel.Anchor = ( (System.Windows.Forms.AnchorStyles)( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.btnCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.btnCancel.Location = new System.Drawing.Point(558, 276);
			this.btnCancel.Name = "btnCancel";
			this.btnCancel.Size = new System.Drawing.Size(75, 23);
			this.btnCancel.TabIndex = 4;
			this.btnCancel.Text = "Cancel";
			this.btnCancel.UseVisualStyleBackColor = true;
			// 
			// AddClass
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(644, 306);
			this.Controls.Add(this.btnCancel);
			this.Controls.Add(this.btnOK);
			this.Controls.Add(this.label4);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.panel1);
			this.Controls.Add(this.tbAdditionalParams);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.tvSolutionView);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.SizableToolWindow;
			this.MinimumSize = new System.Drawing.Size(660, 340);
			this.Name = "AddClass";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Add Class";
			this.panel1.ResumeLayout(false);
			this.panel1.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TreeView tvSolutionView;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.ComboBox cbParentClass;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.TextBox tbAdditionalParams;
		private System.Windows.Forms.Panel panel1;
		private System.Windows.Forms.TextBox tbNewClassName;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.Button btnOK;
		private System.Windows.Forms.Button btnCancel;
	}
}