namespace UnrealDVDLayout
{
    partial class GroupRegExp
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
            this.GroupRegExpLabel = new System.Windows.Forms.Label();
            this.GroupRegExpExpressionCombo = new System.Windows.Forms.ComboBox();
            this.GroupRegExpGroupCombo = new System.Windows.Forms.ComboBox();
            this.GroupRegExpGroupLabel = new System.Windows.Forms.Label();
            this.GroupRegExpOKButton = new System.Windows.Forms.Button();
            this.GroupRegExpCancelButton = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // GroupRegExpLabel
            // 
            this.GroupRegExpLabel.AutoSize = true;
            this.GroupRegExpLabel.Location = new System.Drawing.Point( 113, 18 );
            this.GroupRegExpLabel.Name = "GroupRegExpLabel";
            this.GroupRegExpLabel.Size = new System.Drawing.Size( 270, 13 );
            this.GroupRegExpLabel.TabIndex = 0;
            this.GroupRegExpLabel.Text = "Enter a regular expression to generate or add to a group";
            // 
            // GroupRegExpExpressionCombo
            // 
            this.GroupRegExpExpressionCombo.FormattingEnabled = true;
            this.GroupRegExpExpressionCombo.Location = new System.Drawing.Point( 28, 52 );
            this.GroupRegExpExpressionCombo.Name = "GroupRegExpExpressionCombo";
            this.GroupRegExpExpressionCombo.Size = new System.Drawing.Size( 435, 21 );
            this.GroupRegExpExpressionCombo.TabIndex = 1;
            // 
            // GroupRegExpGroupCombo
            // 
            this.GroupRegExpGroupCombo.FormattingEnabled = true;
            this.GroupRegExpGroupCombo.Location = new System.Drawing.Point( 100, 116 );
            this.GroupRegExpGroupCombo.Name = "GroupRegExpGroupCombo";
            this.GroupRegExpGroupCombo.Size = new System.Drawing.Size( 295, 21 );
            this.GroupRegExpGroupCombo.TabIndex = 2;
            // 
            // GroupRegExpGroupLabel
            // 
            this.GroupRegExpGroupLabel.AutoSize = true;
            this.GroupRegExpGroupLabel.Location = new System.Drawing.Point( 218, 88 );
            this.GroupRegExpGroupLabel.Name = "GroupRegExpGroupLabel";
            this.GroupRegExpGroupLabel.Size = new System.Drawing.Size( 36, 13 );
            this.GroupRegExpGroupLabel.TabIndex = 3;
            this.GroupRegExpGroupLabel.Text = "Group";
            // 
            // GroupRegExpOKButton
            // 
            this.GroupRegExpOKButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.GroupRegExpOKButton.Location = new System.Drawing.Point( 28, 169 );
            this.GroupRegExpOKButton.Name = "GroupRegExpOKButton";
            this.GroupRegExpOKButton.Size = new System.Drawing.Size( 174, 23 );
            this.GroupRegExpOKButton.TabIndex = 4;
            this.GroupRegExpOKButton.Text = "Create/Modify Group";
            this.GroupRegExpOKButton.UseVisualStyleBackColor = true;
            this.GroupRegExpOKButton.Click += new System.EventHandler( this.GroupRegExpOKButton_Click );
            // 
            // GroupRegExpCancelButton
            // 
            this.GroupRegExpCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.GroupRegExpCancelButton.Location = new System.Drawing.Point( 388, 169 );
            this.GroupRegExpCancelButton.Name = "GroupRegExpCancelButton";
            this.GroupRegExpCancelButton.Size = new System.Drawing.Size( 75, 23 );
            this.GroupRegExpCancelButton.TabIndex = 5;
            this.GroupRegExpCancelButton.Text = "Cancel";
            this.GroupRegExpCancelButton.UseVisualStyleBackColor = true;
            // 
            // GroupRegExp
            // 
            this.AcceptButton = this.GroupRegExpOKButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF( 6F, 13F );
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this.GroupRegExpCancelButton;
            this.ClientSize = new System.Drawing.Size( 498, 228 );
            this.ControlBox = false;
            this.Controls.Add( this.GroupRegExpCancelButton );
            this.Controls.Add( this.GroupRegExpOKButton );
            this.Controls.Add( this.GroupRegExpGroupLabel );
            this.Controls.Add( this.GroupRegExpGroupCombo );
            this.Controls.Add( this.GroupRegExpExpressionCombo );
            this.Controls.Add( this.GroupRegExpLabel );
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.Fixed3D;
            this.Name = "GroupRegExp";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Create a Group from a Regular Expression";
            this.ResumeLayout( false );
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label GroupRegExpLabel;
        private System.Windows.Forms.ComboBox GroupRegExpExpressionCombo;
        private System.Windows.Forms.ComboBox GroupRegExpGroupCombo;
        private System.Windows.Forms.Label GroupRegExpGroupLabel;
        private System.Windows.Forms.Button GroupRegExpOKButton;
        private System.Windows.Forms.Button GroupRegExpCancelButton;
    }
}