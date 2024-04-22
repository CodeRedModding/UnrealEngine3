/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;

namespace StatsViewer
{
	/// <summary>
	/// Summary description for ViewFramesByCriteriaDialog.
	/// </summary>
	public class ViewFramesByCriteriaDialog : System.Windows.Forms.Form
	{
		#region Windows Form Designer generated code
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label StatNameLabel;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.RadioButton GreaterThanButton;
		private System.Windows.Forms.RadioButton LessThanButton;
		private System.Windows.Forms.RadioButton EqualToButton;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TextBox ValueEdit;
		private System.Windows.Forms.Button OkButton;
		private System.Windows.Forms.Button CancelBtn;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;
		#endregion

		/// <summary>
		/// Constructor that sets the stat text
		/// </summary>
		public ViewFramesByCriteriaDialog(string StatName)
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();
			// Default to greater than
			GreaterThanButton.Checked = true;
			// Set the stat we are searching for
			StatNameLabel.Text = StatName;
		}

		/// <summary>
		/// Checks the dialog's button state and returns the search type
		/// </summary>
		/// <returns>The search type</returns>
		public SearchByType GetSearchType()
		{
			SearchByType Return;
			// Check the buttons and figure out which is selected
			if (GreaterThanButton.Checked == true)
			{
				Return = SearchByType.GreaterThan;
			}
			else if (LessThanButton.Checked == true)
			{
				Return = SearchByType.LessThan;
			}
			else
			{
				Return = SearchByType.EqualTo;
			}
			return Return;
		}

		/// <summary>
		/// Returns the value to use when doing comparisons
		/// </summary>
		/// <returns>Returns the value that the user specified</returns>
		public double GetSearchValue()
		{
			return Convert.ToDouble(ValueEdit.Text);
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if(components != null)
				{
					components.Dispose();
				}
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
			System.Resources.ResourceManager resources = new System.Resources.ResourceManager(typeof(ViewFramesByCriteriaDialog));
			this.label1 = new System.Windows.Forms.Label();
			this.StatNameLabel = new System.Windows.Forms.Label();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.EqualToButton = new System.Windows.Forms.RadioButton();
			this.LessThanButton = new System.Windows.Forms.RadioButton();
			this.GreaterThanButton = new System.Windows.Forms.RadioButton();
			this.label2 = new System.Windows.Forms.Label();
			this.ValueEdit = new System.Windows.Forms.TextBox();
			this.OkButton = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.groupBox1.SuspendLayout();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.Location = new System.Drawing.Point(16, 16);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(32, 16);
			this.label1.TabIndex = 0;
			this.label1.Text = "Stat:";
			// 
			// StatNameLabel
			// 
			this.StatNameLabel.Location = new System.Drawing.Point(48, 16);
			this.StatNameLabel.Name = "StatNameLabel";
			this.StatNameLabel.Size = new System.Drawing.Size(280, 16);
			this.StatNameLabel.TabIndex = 1;
			this.StatNameLabel.Text = "Stat name";
			// 
			// groupBox1
			// 
			this.groupBox1.Controls.Add(this.EqualToButton);
			this.groupBox1.Controls.Add(this.LessThanButton);
			this.groupBox1.Controls.Add(this.GreaterThanButton);
			this.groupBox1.Location = new System.Drawing.Point(16, 40);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(312, 48);
			this.groupBox1.TabIndex = 2;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Compare using";
			// 
			// EqualToButton
			// 
			this.EqualToButton.Location = new System.Drawing.Point(224, 16);
			this.EqualToButton.Name = "EqualToButton";
			this.EqualToButton.Size = new System.Drawing.Size(80, 24);
			this.EqualToButton.TabIndex = 2;
			this.EqualToButton.Text = "&Equal To";
			// 
			// LessThanButton
			// 
			this.LessThanButton.Location = new System.Drawing.Point(128, 16);
			this.LessThanButton.Name = "LessThanButton";
			this.LessThanButton.Size = new System.Drawing.Size(88, 24);
			this.LessThanButton.TabIndex = 1;
			this.LessThanButton.Text = "&Less Than";
			// 
			// GreaterThanButton
			// 
			this.GreaterThanButton.Location = new System.Drawing.Point(16, 16);
			this.GreaterThanButton.Name = "GreaterThanButton";
			this.GreaterThanButton.TabIndex = 0;
			this.GreaterThanButton.Text = "&Greater Than";
			// 
			// label2
			// 
			this.label2.Location = new System.Drawing.Point(16, 106);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(40, 23);
			this.label2.TabIndex = 3;
			this.label2.Text = "Value:";
			// 
			// ValueEdit
			// 
			this.ValueEdit.Location = new System.Drawing.Point(56, 104);
			this.ValueEdit.Name = "ValueEdit";
			this.ValueEdit.Size = new System.Drawing.Size(272, 20);
			this.ValueEdit.TabIndex = 4;
			this.ValueEdit.Text = "0.0";
			// 
			// OkButton
			// 
			this.OkButton.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.OkButton.Location = new System.Drawing.Point(176, 144);
			this.OkButton.Name = "OkButton";
			this.OkButton.TabIndex = 5;
			this.OkButton.Text = "&Ok";
			// 
			// CancelBtn
			// 
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(256, 144);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.TabIndex = 6;
			this.CancelBtn.Text = "&Cancel";
			// 
			// ViewFramesByCriteriaDialog
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(344, 182);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkButton);
			this.Controls.Add(this.ValueEdit);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.groupBox1);
			this.Controls.Add(this.StatNameLabel);
			this.Controls.Add(this.label1);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "ViewFramesByCriteriaDialog";
			this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "View Frames By...";
			this.groupBox1.ResumeLayout(false);
			this.ResumeLayout(false);

		}
		#endregion
	}
}
