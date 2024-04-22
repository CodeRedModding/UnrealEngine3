namespace P4PopulateDepot
{
	partial class Progress
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Progress));
			this.RedistProgressHeaderLine = new System.Windows.Forms.Label();
			this.ProgressAnimatedPictureBox = new System.Windows.Forms.PictureBox();
			this.LabelDetail = new System.Windows.Forms.Label();
			this.LabelDescription = new System.Windows.Forms.Label();
			this.LabelHeading = new System.Windows.Forms.Label();
			((System.ComponentModel.ISupportInitialize)(this.ProgressAnimatedPictureBox)).BeginInit();
			this.SuspendLayout();
			// 
			// RedistProgressHeaderLine
			// 
			this.RedistProgressHeaderLine.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.RedistProgressHeaderLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.RedistProgressHeaderLine.Location = new System.Drawing.Point(-3, 70);
			this.RedistProgressHeaderLine.Name = "RedistProgressHeaderLine";
			this.RedistProgressHeaderLine.Size = new System.Drawing.Size(800, 2);
			this.RedistProgressHeaderLine.TabIndex = 9;
			// 
			// ProgressAnimatedPictureBox
			// 
			this.ProgressAnimatedPictureBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ProgressAnimatedPictureBox.ErrorImage = null;
			this.ProgressAnimatedPictureBox.InitialImage = null;
			this.ProgressAnimatedPictureBox.Location = new System.Drawing.Point(12, 189);
			this.ProgressAnimatedPictureBox.Name = "ProgressAnimatedPictureBox";
			this.ProgressAnimatedPictureBox.Size = new System.Drawing.Size(772, 86);
			this.ProgressAnimatedPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.CenterImage;
			this.ProgressAnimatedPictureBox.TabIndex = 5;
			this.ProgressAnimatedPictureBox.TabStop = false;
			// 
			// LabelDetail
			// 
			this.LabelDetail.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.LabelDetail.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.LabelDetail.Location = new System.Drawing.Point(12, 120);
			this.LabelDetail.Name = "LabelDetail";
			this.LabelDetail.Size = new System.Drawing.Size(772, 76);
			this.LabelDetail.TabIndex = 8;
			this.LabelDetail.Text = "Preparing changelist for submission.";
			// 
			// LabelDescription
			// 
			this.LabelDescription.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.LabelDescription.Font = new System.Drawing.Font("Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.LabelDescription.Location = new System.Drawing.Point(14, 87);
			this.LabelDescription.Name = "LabelDescription";
			this.LabelDescription.Size = new System.Drawing.Size(770, 28);
			this.LabelDescription.TabIndex = 7;
			this.LabelDescription.Text = "Adding files to Perforce depot";
			this.LabelDescription.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// LabelHeading
			// 
			this.LabelHeading.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.LabelHeading.BackColor = System.Drawing.Color.White;
			this.LabelHeading.Font = new System.Drawing.Font("Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.LabelHeading.Location = new System.Drawing.Point(-3, 2);
			this.LabelHeading.Name = "LabelHeading";
			this.LabelHeading.Size = new System.Drawing.Size(800, 68);
			this.LabelHeading.TabIndex = 6;
			this.LabelHeading.Text = "Please Wait";
			this.LabelHeading.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// Progress
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 16F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(794, 281);
			this.ControlBox = false;
			this.Controls.Add(this.RedistProgressHeaderLine);
			this.Controls.Add(this.ProgressAnimatedPictureBox);
			this.Controls.Add(this.LabelDetail);
			this.Controls.Add(this.LabelDescription);
			this.Controls.Add(this.LabelHeading);
			this.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "Progress";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "Progress";
			this.Load += new System.EventHandler(this.Progress_Load);
			((System.ComponentModel.ISupportInitialize)(this.ProgressAnimatedPictureBox)).EndInit();
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Label RedistProgressHeaderLine;
		private System.Windows.Forms.PictureBox ProgressAnimatedPictureBox;
		public System.Windows.Forms.Label LabelDetail;
		public System.Windows.Forms.Label LabelDescription;
		public System.Windows.Forms.Label LabelHeading;

	}
}