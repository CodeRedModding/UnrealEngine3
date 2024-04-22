namespace P4PopulateDepot
{
    partial class NewUserDialog
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(NewUserDialog));
            this.FooterLine = new System.Windows.Forms.Label();
            this.NUOKButton = new System.Windows.Forms.Button();
            this.NUCancelButton = new System.Windows.Forms.Button();
            this.NUUserNameErrLabel = new System.Windows.Forms.Label();
            this.NUUserNameTextBox = new System.Windows.Forms.TextBox();
            this.NUUserNameLabel = new System.Windows.Forms.Label();
            this.NUPasswordErrLabel = new System.Windows.Forms.Label();
            this.NUPasswordTextBox1 = new System.Windows.Forms.TextBox();
            this.NUPasswordLabel = new System.Windows.Forms.Label();
            this.NUPasswordTextBox2 = new System.Windows.Forms.TextBox();
            this.NUPasswordInfoShort = new System.Windows.Forms.Label();
            this.NUEmailErrLabel = new System.Windows.Forms.Label();
            this.NUEmailTextBox = new System.Windows.Forms.TextBox();
            this.NUEmailLabel = new System.Windows.Forms.Label();
            this.NUMainErrorDescription = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // FooterLine
            // 
            this.FooterLine.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.FooterLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.FooterLine.Location = new System.Drawing.Point(-4, 201);
            this.FooterLine.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.FooterLine.Name = "FooterLine";
            this.FooterLine.Size = new System.Drawing.Size(561, 2);
            this.FooterLine.TabIndex = 16;
            // 
            // NUOKButton
            // 
            this.NUOKButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.NUOKButton.AutoSize = true;
            this.NUOKButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.NUOKButton.Location = new System.Drawing.Point(332, 212);
            this.NUOKButton.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.NUOKButton.Name = "NUOKButton";
            this.NUOKButton.Size = new System.Drawing.Size(100, 32);
            this.NUOKButton.TabIndex = 14;
            this.NUOKButton.Tag = "";
            this.NUOKButton.Text = "OK";
            this.NUOKButton.UseVisualStyleBackColor = true;
            this.NUOKButton.Click += new System.EventHandler(this.NUOKButton_Click);
            // 
            // NUCancelButton
            // 
            this.NUCancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.NUCancelButton.AutoSize = true;
            this.NUCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.NUCancelButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.NUCancelButton.Location = new System.Drawing.Point(438, 212);
            this.NUCancelButton.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.NUCancelButton.Name = "NUCancelButton";
            this.NUCancelButton.Size = new System.Drawing.Size(100, 32);
            this.NUCancelButton.TabIndex = 15;
            this.NUCancelButton.Tag = "";
            this.NUCancelButton.Text = "Cancel";
            this.NUCancelButton.UseVisualStyleBackColor = true;
            // 
            // NUUserNameErrLabel
            // 
            this.NUUserNameErrLabel.AutoSize = true;
            this.NUUserNameErrLabel.ForeColor = System.Drawing.Color.Firebrick;
            this.NUUserNameErrLabel.Image = global::P4PopulateDepot.Properties.Resources.red_arrow_right;
            this.NUUserNameErrLabel.Location = new System.Drawing.Point(114, 55);
            this.NUUserNameErrLabel.Name = "NUUserNameErrLabel";
            this.NUUserNameErrLabel.Size = new System.Drawing.Size(12, 16);
            this.NUUserNameErrLabel.TabIndex = 23;
            this.NUUserNameErrLabel.Text = " ";
            // 
            // NUUserNameTextBox
            // 
            this.NUUserNameTextBox.Location = new System.Drawing.Point(126, 52);
            this.NUUserNameTextBox.Name = "NUUserNameTextBox";
            this.NUUserNameTextBox.Size = new System.Drawing.Size(386, 23);
            this.NUUserNameTextBox.TabIndex = 22;
            // 
            // NUUserNameLabel
            // 
            this.NUUserNameLabel.AutoSize = true;
            this.NUUserNameLabel.Location = new System.Drawing.Point(31, 55);
            this.NUUserNameLabel.Name = "NUUserNameLabel";
            this.NUUserNameLabel.Size = new System.Drawing.Size(71, 16);
            this.NUUserNameLabel.TabIndex = 21;
            this.NUUserNameLabel.Text = "User Name";
            // 
            // NUPasswordErrLabel
            // 
            this.NUPasswordErrLabel.AutoSize = true;
            this.NUPasswordErrLabel.ForeColor = System.Drawing.Color.Firebrick;
            this.NUPasswordErrLabel.Image = global::P4PopulateDepot.Properties.Resources.red_arrow_right;
            this.NUPasswordErrLabel.Location = new System.Drawing.Point(114, 89);
            this.NUPasswordErrLabel.Name = "NUPasswordErrLabel";
            this.NUPasswordErrLabel.Size = new System.Drawing.Size(12, 16);
            this.NUPasswordErrLabel.TabIndex = 26;
            this.NUPasswordErrLabel.Text = " ";
            // 
            // NUPasswordTextBox1
            // 
            this.NUPasswordTextBox1.Location = new System.Drawing.Point(126, 86);
            this.NUPasswordTextBox1.Name = "NUPasswordTextBox1";
            this.NUPasswordTextBox1.PasswordChar = '*';
            this.NUPasswordTextBox1.Size = new System.Drawing.Size(190, 23);
            this.NUPasswordTextBox1.TabIndex = 25;
            // 
            // NUPasswordLabel
            // 
            this.NUPasswordLabel.AutoSize = true;
            this.NUPasswordLabel.Location = new System.Drawing.Point(31, 89);
            this.NUPasswordLabel.Name = "NUPasswordLabel";
            this.NUPasswordLabel.Size = new System.Drawing.Size(71, 16);
            this.NUPasswordLabel.TabIndex = 24;
            this.NUPasswordLabel.Text = "Password*";
            // 
            // NUPasswordTextBox2
            // 
            this.NUPasswordTextBox2.Location = new System.Drawing.Point(322, 86);
            this.NUPasswordTextBox2.Name = "NUPasswordTextBox2";
            this.NUPasswordTextBox2.PasswordChar = '*';
            this.NUPasswordTextBox2.Size = new System.Drawing.Size(190, 23);
            this.NUPasswordTextBox2.TabIndex = 27;
            // 
            // NUPasswordInfoShort
            // 
            this.NUPasswordInfoShort.AutoSize = true;
            this.NUPasswordInfoShort.Location = new System.Drawing.Point(123, 112);
            this.NUPasswordInfoShort.Name = "NUPasswordInfoShort";
            this.NUPasswordInfoShort.Size = new System.Drawing.Size(131, 16);
            this.NUPasswordInfoShort.TabIndex = 28;
            this.NUPasswordInfoShort.Text = "Enter password twice";
            // 
            // NUEmailErrLabel
            // 
            this.NUEmailErrLabel.AutoSize = true;
            this.NUEmailErrLabel.ForeColor = System.Drawing.Color.Firebrick;
            this.NUEmailErrLabel.Image = global::P4PopulateDepot.Properties.Resources.red_arrow_right;
            this.NUEmailErrLabel.Location = new System.Drawing.Point(114, 139);
            this.NUEmailErrLabel.Name = "NUEmailErrLabel";
            this.NUEmailErrLabel.Size = new System.Drawing.Size(12, 16);
            this.NUEmailErrLabel.TabIndex = 31;
            this.NUEmailErrLabel.Text = " ";
            // 
            // NUEmailTextBox
            // 
            this.NUEmailTextBox.Location = new System.Drawing.Point(126, 136);
            this.NUEmailTextBox.Name = "NUEmailTextBox";
            this.NUEmailTextBox.Size = new System.Drawing.Size(386, 23);
            this.NUEmailTextBox.TabIndex = 30;
            // 
            // NUEmailLabel
            // 
            this.NUEmailLabel.AutoSize = true;
            this.NUEmailLabel.Location = new System.Drawing.Point(31, 139);
            this.NUEmailLabel.Name = "NUEmailLabel";
            this.NUEmailLabel.Size = new System.Drawing.Size(39, 16);
            this.NUEmailLabel.TabIndex = 29;
            this.NUEmailLabel.Text = "Email";
            // 
            // NUMainErrorDescription
            // 
            this.NUMainErrorDescription.ForeColor = System.Drawing.Color.Firebrick;
            this.NUMainErrorDescription.Location = new System.Drawing.Point(5, 171);
            this.NUMainErrorDescription.Name = "NUMainErrorDescription";
            this.NUMainErrorDescription.Size = new System.Drawing.Size(539, 18);
            this.NUMainErrorDescription.TabIndex = 33;
            this.NUMainErrorDescription.Text = "Error Text";
            // 
            // NewUserDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(550, 258);
            this.Controls.Add(this.NUMainErrorDescription);
            this.Controls.Add(this.NUEmailErrLabel);
            this.Controls.Add(this.NUEmailTextBox);
            this.Controls.Add(this.NUEmailLabel);
            this.Controls.Add(this.NUPasswordInfoShort);
            this.Controls.Add(this.NUPasswordTextBox2);
            this.Controls.Add(this.NUPasswordErrLabel);
            this.Controls.Add(this.NUPasswordTextBox1);
            this.Controls.Add(this.NUPasswordLabel);
            this.Controls.Add(this.NUUserNameErrLabel);
            this.Controls.Add(this.NUUserNameTextBox);
            this.Controls.Add(this.NUUserNameLabel);
            this.Controls.Add(this.FooterLine);
            this.Controls.Add(this.NUOKButton);
            this.Controls.Add(this.NUCancelButton);
            this.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.Name = "NewUserDialog";
            this.Text = "New User";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label FooterLine;
        private System.Windows.Forms.Button NUOKButton;
        private System.Windows.Forms.Button NUCancelButton;
        private System.Windows.Forms.Label NUUserNameErrLabel;
        private System.Windows.Forms.TextBox NUUserNameTextBox;
        private System.Windows.Forms.Label NUUserNameLabel;
        private System.Windows.Forms.Label NUPasswordErrLabel;
        private System.Windows.Forms.TextBox NUPasswordTextBox1;
        private System.Windows.Forms.Label NUPasswordLabel;
        private System.Windows.Forms.TextBox NUPasswordTextBox2;
        private System.Windows.Forms.Label NUPasswordInfoShort;
        private System.Windows.Forms.Label NUEmailErrLabel;
        private System.Windows.Forms.TextBox NUEmailTextBox;
        private System.Windows.Forms.Label NUEmailLabel;
        private System.Windows.Forms.Label NUMainErrorDescription;
    }
}