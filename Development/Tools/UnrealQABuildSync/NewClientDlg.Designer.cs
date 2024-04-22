namespace UnrealQABuildSync
{
    partial class NewClientDlg
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
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.textBox_WorkspaceName = new System.Windows.Forms.TextBox();
            this.textBox_WorkspaceRootFolder = new System.Windows.Forms.TextBox();
            this.button_RootFolder = new System.Windows.Forms.Button();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.radioButton_None = new System.Windows.Forms.RadioButton();
            this.radioButton_Use = new System.Windows.Forms.RadioButton();
            this.checkBox_EditClient = new System.Windows.Forms.CheckBox();
            this.textBox_Templete = new System.Windows.Forms.TextBox();
            this.button_SelectTemplate = new System.Windows.Forms.Button();
            this.groupBox1.SuspendLayout();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(23, 20);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(143, 12);
            this.label1.TabIndex = 0;
            this.label1.Text = "Client workspace name:";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(23, 49);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(167, 12);
            this.label2.TabIndex = 1;
            this.label2.Text = "Client workspace root folder:";
            // 
            // textBox_WorkspaceName
            // 
            this.textBox_WorkspaceName.Location = new System.Drawing.Point(196, 17);
            this.textBox_WorkspaceName.Name = "textBox_WorkspaceName";
            this.textBox_WorkspaceName.Size = new System.Drawing.Size(270, 21);
            this.textBox_WorkspaceName.TabIndex = 0;
            // 
            // textBox_WorkspaceRootFolder
            // 
            this.textBox_WorkspaceRootFolder.Location = new System.Drawing.Point(196, 46);
            this.textBox_WorkspaceRootFolder.Name = "textBox_WorkspaceRootFolder";
            this.textBox_WorkspaceRootFolder.Size = new System.Drawing.Size(270, 21);
            this.textBox_WorkspaceRootFolder.TabIndex = 1;
            // 
            // button_RootFolder
            // 
            this.button_RootFolder.Location = new System.Drawing.Point(482, 46);
            this.button_RootFolder.Name = "button_RootFolder";
            this.button_RootFolder.Size = new System.Drawing.Size(95, 20);
            this.button_RootFolder.TabIndex = 2;
            this.button_RootFolder.Text = "Browse...";
            this.button_RootFolder.UseVisualStyleBackColor = true;
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.button_SelectTemplate);
            this.groupBox1.Controls.Add(this.textBox_Templete);
            this.groupBox1.Controls.Add(this.radioButton_Use);
            this.groupBox1.Controls.Add(this.radioButton_None);
            this.groupBox1.Location = new System.Drawing.Point(63, 92);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(469, 88);
            this.groupBox1.TabIndex = 5;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Templete";
            // 
            // radioButton_None
            // 
            this.radioButton_None.AutoSize = true;
            this.radioButton_None.Location = new System.Drawing.Point(31, 28);
            this.radioButton_None.Name = "radioButton_None";
            this.radioButton_None.Size = new System.Drawing.Size(53, 16);
            this.radioButton_None.TabIndex = 4;
            this.radioButton_None.TabStop = true;
            this.radioButton_None.Text = "None";
            this.radioButton_None.UseVisualStyleBackColor = true;
            // 
            // radioButton_Use
            // 
            this.radioButton_Use.AutoSize = true;
            this.radioButton_Use.Location = new System.Drawing.Point(31, 50);
            this.radioButton_Use.Name = "radioButton_Use";
            this.radioButton_Use.Size = new System.Drawing.Size(45, 16);
            this.radioButton_Use.TabIndex = 5;
            this.radioButton_Use.TabStop = true;
            this.radioButton_Use.Text = "Use";
            this.radioButton_Use.UseVisualStyleBackColor = true;
            // 
            // checkBox_EditClient
            // 
            this.checkBox_EditClient.AutoSize = true;
            this.checkBox_EditClient.Location = new System.Drawing.Point(94, 193);
            this.checkBox_EditClient.Name = "checkBox_EditClient";
            this.checkBox_EditClient.Size = new System.Drawing.Size(91, 16);
            this.checkBox_EditClient.TabIndex = 8;
            this.checkBox_EditClient.Text = "Edit client...";
            this.checkBox_EditClient.UseVisualStyleBackColor = true;
            // 
            // textBox_Templete
            // 
            this.textBox_Templete.Location = new System.Drawing.Point(91, 49);
            this.textBox_Templete.Name = "textBox_Templete";
            this.textBox_Templete.Size = new System.Drawing.Size(277, 21);
            this.textBox_Templete.TabIndex = 6;
            // 
            // button_SelectTemplate
            // 
            this.button_SelectTemplate.Location = new System.Drawing.Point(374, 49);
            this.button_SelectTemplate.Name = "button_SelectTemplate";
            this.button_SelectTemplate.Size = new System.Drawing.Size(78, 21);
            this.button_SelectTemplate.TabIndex = 7;
            this.button_SelectTemplate.Text = "Browse...";
            this.button_SelectTemplate.UseVisualStyleBackColor = true;
            this.button_SelectTemplate.Click += new System.EventHandler(this.button_SelectTemplate_Click);
            // 
            // NewClientDlg
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 12F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(599, 260);
            this.Controls.Add(this.checkBox_EditClient);
            this.Controls.Add(this.groupBox1);
            this.Controls.Add(this.button_RootFolder);
            this.Controls.Add(this.textBox_WorkspaceRootFolder);
            this.Controls.Add(this.textBox_WorkspaceName);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.Name = "NewClientDlg";
            this.Text = "NewClientDlg";
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.TextBox textBox_WorkspaceName;
        private System.Windows.Forms.TextBox textBox_WorkspaceRootFolder;
        private System.Windows.Forms.Button button_RootFolder;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.RadioButton radioButton_Use;
        private System.Windows.Forms.RadioButton radioButton_None;
        private System.Windows.Forms.CheckBox checkBox_EditClient;
        private System.Windows.Forms.Button button_SelectTemplate;
        private System.Windows.Forms.TextBox textBox_Templete;

    }
}