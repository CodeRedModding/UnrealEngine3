namespace MacPackager
{
    partial class ToolsHub
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ToolsHub));
			this.ReadyToPackageButton = new System.Windows.Forms.Button();
			this.CancelThisFormButton = new System.Windows.Forms.Button();
			this.tabControl1 = new System.Windows.Forms.TabControl();
			this.tabPage1 = new System.Windows.Forms.TabPage();
			this.label2 = new System.Windows.Forms.Label();
			this.BundleIconFileEdit = new System.Windows.Forms.TextBox();
			this.label3 = new System.Windows.Forms.Label();
			this.BrowseIconButton = new System.Windows.Forms.Button();
			this.label22 = new System.Windows.Forms.Label();
			this.DeployPathEdit = new System.Windows.Forms.TextBox();
			this.label21 = new System.Windows.Forms.Label();
			this.BrowseDeployPathButton = new System.Windows.Forms.Button();
			this.linkLabel4 = new System.Windows.Forms.LinkLabel();
			this.label1 = new System.Windows.Forms.Label();
			this.label17 = new System.Windows.Forms.Label();
			this.label18 = new System.Windows.Forms.Label();
			this.AppleReferenceHyperlink = new System.Windows.Forms.LinkLabel();
			this.EditManuallyButton = new System.Windows.Forms.Button();
			this.BundleIdentifierEdit = new System.Windows.Forms.TextBox();
			this.BundleNameEdit = new System.Windows.Forms.TextBox();
			this.label20 = new System.Windows.Forms.Label();
			this.tabPage2 = new System.Windows.Forms.TabPage();
			this.linkLabel1 = new System.Windows.Forms.LinkLabel();
			this.linkLabel2 = new System.Windows.Forms.LinkLabel();
			this.linkLabel3 = new System.Windows.Forms.LinkLabel();
			this.label13 = new System.Windows.Forms.Label();
			this.ImportCertificateButton2 = new System.Windows.Forms.Button();
			this.CertificatePresentCheck2 = new System.Windows.Forms.PictureBox();
			this.PickDestFolderDialog = new System.Windows.Forms.FolderBrowserDialog();
			this.tabControl1.SuspendLayout();
			this.tabPage1.SuspendLayout();
			this.tabPage2.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.CertificatePresentCheck2)).BeginInit();
			this.SuspendLayout();
			// 
			// ReadyToPackageButton
			// 
			this.ReadyToPackageButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.ReadyToPackageButton.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.ReadyToPackageButton.Location = new System.Drawing.Point(436, 483);
			this.ReadyToPackageButton.Name = "ReadyToPackageButton";
			this.ReadyToPackageButton.Size = new System.Drawing.Size(109, 29);
			this.ReadyToPackageButton.TabIndex = 5;
			this.ReadyToPackageButton.Text = "Ready to Package";
			this.ReadyToPackageButton.UseVisualStyleBackColor = true;
			this.ReadyToPackageButton.Click += new System.EventHandler(this.ReadyToPackageButton_Click);
			// 
			// CancelThisFormButton
			// 
			this.CancelThisFormButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.CancelThisFormButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelThisFormButton.Location = new System.Drawing.Point(16, 483);
			this.CancelThisFormButton.Name = "CancelThisFormButton";
			this.CancelThisFormButton.Size = new System.Drawing.Size(109, 29);
			this.CancelThisFormButton.TabIndex = 6;
			this.CancelThisFormButton.Text = "Cancel";
			this.CancelThisFormButton.UseVisualStyleBackColor = true;
			this.CancelThisFormButton.Click += new System.EventHandler(this.CancelThisFormButton_Click);
			// 
			// tabControl1
			// 
			this.tabControl1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
						| System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.tabControl1.Controls.Add(this.tabPage1);
			this.tabControl1.Controls.Add(this.tabPage2);
			this.tabControl1.Location = new System.Drawing.Point(16, 12);
			this.tabControl1.Name = "tabControl1";
			this.tabControl1.Padding = new System.Drawing.Point(9, 6);
			this.tabControl1.SelectedIndex = 2;
			this.tabControl1.Size = new System.Drawing.Size(530, 454);
			this.tabControl1.TabIndex = 18;
			// 
			// tabPage1
			// 
			this.tabPage1.Controls.Add(this.label2);
			this.tabPage1.Controls.Add(this.BundleIconFileEdit);
			this.tabPage1.Controls.Add(this.label3);
			this.tabPage1.Controls.Add(this.BrowseIconButton);
			this.tabPage1.Controls.Add(this.label22);
			this.tabPage1.Controls.Add(this.DeployPathEdit);
			this.tabPage1.Controls.Add(this.label21);
			this.tabPage1.Controls.Add(this.BrowseDeployPathButton);
			this.tabPage1.Controls.Add(this.linkLabel4);
			this.tabPage1.Controls.Add(this.label1);
			this.tabPage1.Controls.Add(this.label17);
			this.tabPage1.Controls.Add(this.label18);
			this.tabPage1.Controls.Add(this.AppleReferenceHyperlink);
			this.tabPage1.Controls.Add(this.EditManuallyButton);
			this.tabPage1.Controls.Add(this.BundleIdentifierEdit);
			this.tabPage1.Controls.Add(this.BundleNameEdit);
			this.tabPage1.Controls.Add(this.label20);
			this.tabPage1.Location = new System.Drawing.Point(4, 28);
			this.tabPage1.Name = "tabPage1";
			this.tabPage1.Padding = new System.Windows.Forms.Padding(3);
			this.tabPage1.Size = new System.Drawing.Size(522, 422);
			this.tabPage1.TabIndex = 0;
			this.tabPage1.Text = "Basic Settings";
			this.tabPage1.UseVisualStyleBackColor = true;
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(10, 361);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(187, 13);
			this.label2.TabIndex = 35;
			this.label2.Text = "Leave empty to use default UDK icon.";
			// 
			// BundleIconFileEdit
			// 
			this.BundleIconFileEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.BundleIconFileEdit.Location = new System.Drawing.Point(9, 377);
			this.BundleIconFileEdit.Name = "BundleIconFileEdit";
			this.BundleIconFileEdit.Size = new System.Drawing.Size(370, 20);
			this.BundleIconFileEdit.TabIndex = 34;
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.label3.Location = new System.Drawing.Point(6, 337);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(167, 20);
			this.label3.TabIndex = 33;
			this.label3.Text = "Select application icon";
			// 
			// BrowseIconButton
			// 
			this.BrowseIconButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.BrowseIconButton.Location = new System.Drawing.Point(385, 363);
			this.BrowseIconButton.Name = "BrowseIconButton";
			this.BrowseIconButton.Size = new System.Drawing.Size(129, 34);
			this.BrowseIconButton.TabIndex = 32;
			this.BrowseIconButton.Text = "Browse...";
			this.BrowseIconButton.UseVisualStyleBackColor = true;
			this.BrowseIconButton.Click += new System.EventHandler(this.BrowseIconButton_Click);
			// 
			// label22
			// 
			this.label22.AutoSize = true;
			this.label22.Location = new System.Drawing.Point(10, 298);
			this.label22.Name = "label22";
			this.label22.Size = new System.Drawing.Size(333, 13);
			this.label22.TabIndex = 31;
			this.label22.Text = "The path where the resulting application archive should be copied to.";
			// 
			// DeployPathEdit
			// 
			this.DeployPathEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.DeployPathEdit.Location = new System.Drawing.Point(9, 314);
			this.DeployPathEdit.Name = "DeployPathEdit";
			this.DeployPathEdit.Size = new System.Drawing.Size(370, 20);
			this.DeployPathEdit.TabIndex = 30;
			// 
			// label21
			// 
			this.label21.AutoSize = true;
			this.label21.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.label21.Location = new System.Drawing.Point(6, 274);
			this.label21.Name = "label21";
			this.label21.Size = new System.Drawing.Size(281, 20);
			this.label21.TabIndex = 29;
			this.label21.Text = "Select destination path for deployment";
			// 
			// BrowseDeployPathButton
			// 
			this.BrowseDeployPathButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.BrowseDeployPathButton.Location = new System.Drawing.Point(385, 300);
			this.BrowseDeployPathButton.Name = "BrowseDeployPathButton";
			this.BrowseDeployPathButton.Size = new System.Drawing.Size(129, 34);
			this.BrowseDeployPathButton.TabIndex = 28;
			this.BrowseDeployPathButton.Text = "Browse...";
			this.BrowseDeployPathButton.UseVisualStyleBackColor = true;
			this.BrowseDeployPathButton.Click += new System.EventHandler(this.BrowseDeployPathButton_Click);
			// 
			// linkLabel4
			// 
			this.linkLabel4.AutoSize = true;
			this.linkLabel4.Location = new System.Drawing.Point(17, 248);
			this.linkLabel4.Name = "linkLabel4";
			this.linkLabel4.Size = new System.Drawing.Size(160, 13);
			this.linkLabel4.TabIndex = 27;
			this.linkLabel4.TabStop = true;
			this.linkLabel4.Tag = "https://udn.epicgames.com/Three/UDKInfoPListAppleMac";
			this.linkLabel4.Text = "UDN guide to Info.plist overrides";
			this.linkLabel4.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.HyperlinkClicked);
			// 
			// label1
			// 
			this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(2, 211);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(105, 13);
			this.label1.TabIndex = 26;
			this.label1.Text = "For more information:";
			// 
			// label17
			// 
			this.label17.AutoSize = true;
			this.label17.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.label17.Location = new System.Drawing.Point(6, 3);
			this.label17.Name = "label17";
			this.label17.Size = new System.Drawing.Size(224, 20);
			this.label17.TabIndex = 25;
			this.label17.Text = "Customize settings in Info.plist";
			// 
			// label18
			// 
			this.label18.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.label18.Location = new System.Drawing.Point(5, 93);
			this.label18.Name = "label18";
			this.label18.Size = new System.Drawing.Size(411, 92);
			this.label18.TabIndex = 24;
			this.label18.Text = resources.GetString("label18.Text");
			// 
			// AppleReferenceHyperlink
			// 
			this.AppleReferenceHyperlink.AutoSize = true;
			this.AppleReferenceHyperlink.Location = new System.Drawing.Point(17, 229);
			this.AppleReferenceHyperlink.Name = "AppleReferenceHyperlink";
			this.AppleReferenceHyperlink.Size = new System.Drawing.Size(219, 13);
			this.AppleReferenceHyperlink.TabIndex = 22;
			this.AppleReferenceHyperlink.TabStop = true;
			this.AppleReferenceHyperlink.Tag = "http://developer.apple.com/library/mac/#documentation/general/Reference/InfoPlist" +
				"KeyReference/Introduction/Introduction.html";
			this.AppleReferenceHyperlink.Text = "Apple Reference Documentation for Info.plist";
			this.AppleReferenceHyperlink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.HyperlinkClicked);
			// 
			// EditManuallyButton
			// 
			this.EditManuallyButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.EditManuallyButton.Location = new System.Drawing.Point(385, 228);
			this.EditManuallyButton.Name = "EditManuallyButton";
			this.EditManuallyButton.Size = new System.Drawing.Size(129, 33);
			this.EditManuallyButton.TabIndex = 21;
			this.EditManuallyButton.Text = "Find in Explorer...";
			this.EditManuallyButton.UseVisualStyleBackColor = true;
			this.EditManuallyButton.Click += new System.EventHandler(this.EditManuallyButton_Click);
			// 
			// BundleIdentifierEdit
			// 
			this.BundleIdentifierEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.BundleIdentifierEdit.Location = new System.Drawing.Point(9, 188);
			this.BundleIdentifierEdit.Name = "BundleIdentifierEdit";
			this.BundleIdentifierEdit.Size = new System.Drawing.Size(505, 20);
			this.BundleIdentifierEdit.TabIndex = 19;
			// 
			// BundleNameEdit
			// 
			this.BundleNameEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.BundleNameEdit.Location = new System.Drawing.Point(9, 56);
			this.BundleNameEdit.Name = "BundleNameEdit";
			this.BundleNameEdit.Size = new System.Drawing.Size(505, 20);
			this.BundleNameEdit.TabIndex = 18;
			// 
			// label20
			// 
			this.label20.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.label20.Location = new System.Drawing.Point(6, 36);
			this.label20.Name = "label20";
			this.label20.Size = new System.Drawing.Size(411, 31);
			this.label20.TabIndex = 16;
			this.label20.Text = "The bundle name (CFBundleName).";
			// 
			// tabPage2
			// 
			this.tabPage2.Controls.Add(this.linkLabel1);
			this.tabPage2.Controls.Add(this.linkLabel2);
			this.tabPage2.Controls.Add(this.linkLabel3);
			this.tabPage2.Controls.Add(this.label13);
			this.tabPage2.Controls.Add(this.ImportCertificateButton2);
			this.tabPage2.Controls.Add(this.CertificatePresentCheck2);
			this.tabPage2.Location = new System.Drawing.Point(4, 28);
			this.tabPage2.Name = "tabPage2";
			this.tabPage2.Padding = new System.Windows.Forms.Padding(3);
			this.tabPage2.Size = new System.Drawing.Size(522, 422);
			this.tabPage2.TabIndex = 2;
			this.tabPage2.Text = "Mac App Store Settings";
			this.tabPage2.UseVisualStyleBackColor = true;
			// 
			// linkLabel1
			// 
			this.linkLabel1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.linkLabel1.LinkArea = new System.Windows.Forms.LinkArea(67, 40);
			this.linkLabel1.Location = new System.Drawing.Point(19, 45);
			this.linkLabel1.Name = "linkLabel1";
			this.linkLabel1.Size = new System.Drawing.Size(494, 30);
			this.linkLabel1.TabIndex = 50;
			this.linkLabel1.TabStop = true;
			this.linkLabel1.Tag = "http://developer.apple.com/certificates/";
			this.linkLabel1.Text = "Once you are approved by Apple, use Developer Certificate Utility (http://develop" +
				"er.apple.com/certificates/) to generate signing certificates.";
			this.linkLabel1.UseCompatibleTextRendering = true;
			this.linkLabel1.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.HyperlinkClicked);
			// 
			// linkLabel2
			// 
			this.linkLabel2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.linkLabel2.LinkArea = new System.Windows.Forms.LinkArea(101, 40);
			this.linkLabel2.Location = new System.Drawing.Point(19, 15);
			this.linkLabel2.Name = "linkLabel2";
			this.linkLabel2.Size = new System.Drawing.Size(494, 30);
			this.linkLabel2.TabIndex = 49;
			this.linkLabel2.TabStop = true;
			this.linkLabel2.Tag = "http://developer.apple.com/programs/mac";
			this.linkLabel2.Text = "To submit your UDK game to Mac App Store, new users need to register with Apple a" +
				"s a Mac developer (http://developer.apple.com/programs/mac/).";
			this.linkLabel2.UseCompatibleTextRendering = true;
			this.linkLabel2.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.HyperlinkClicked);
			// 
			// linkLabel3
			// 
			this.linkLabel3.AutoSize = true;
			this.linkLabel3.Location = new System.Drawing.Point(16, 398);
			this.linkLabel3.Name = "linkLabel3";
			this.linkLabel3.Size = new System.Drawing.Size(256, 13);
			this.linkLabel3.TabIndex = 48;
			this.linkLabel3.TabStop = true;
			this.linkLabel3.Tag = "http://udn.epicgames.com/Three/AppleiOSProvisioningSetup#Transferring Existing Pr" +
				"ovisioning.html";
			this.linkLabel3.Text = "More information for this process is available on UDN";
			this.linkLabel3.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.HyperlinkClicked);
			// 
			// label13
			// 
			this.label13.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.label13.Location = new System.Drawing.Point(16, 102);
			this.label13.Name = "label13";
			this.label13.Size = new System.Drawing.Size(404, 97);
			this.label13.TabIndex = 44;
			this.label13.Text = resources.GetString("label13.Text");
			// 
			// ImportCertificateButton2
			// 
			this.ImportCertificateButton2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.ImportCertificateButton2.Location = new System.Drawing.Point(19, 202);
			this.ImportCertificateButton2.Name = "ImportCertificateButton2";
			this.ImportCertificateButton2.Size = new System.Drawing.Size(404, 32);
			this.ImportCertificateButton2.TabIndex = 2;
			this.ImportCertificateButton2.Text = "Import a certificate...";
			this.ImportCertificateButton2.UseVisualStyleBackColor = true;
			this.ImportCertificateButton2.Click += new System.EventHandler(this.ImportCertificateButton_Click);
			// 
			// CertificatePresentCheck2
			// 
			this.CertificatePresentCheck2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.CertificatePresentCheck2.Image = global::MacPackager.Properties.Resources.GreenCheck;
			this.CertificatePresentCheck2.Location = new System.Drawing.Point(481, 202);
			this.CertificatePresentCheck2.Name = "CertificatePresentCheck2";
			this.CertificatePresentCheck2.Size = new System.Drawing.Size(32, 32);
			this.CertificatePresentCheck2.TabIndex = 35;
			this.CertificatePresentCheck2.TabStop = false;
			// 
			// ToolsHub
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelThisFormButton;
			this.ClientSize = new System.Drawing.Size(562, 524);
			this.Controls.Add(this.tabControl1);
			this.Controls.Add(this.CancelThisFormButton);
			this.Controls.Add(this.ReadyToPackageButton);
			this.HelpButton = true;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.KeyPreview = true;
			this.Name = "ToolsHub";
			this.Text = "Mac Packager Tools Hub";
			this.Load += new System.EventHandler(this.ConfigureMacGame_Load);
			this.HelpButtonClicked += new System.ComponentModel.CancelEventHandler(this.ToolsHub_HelpButtonClicked);
			this.Shown += new System.EventHandler(this.ToolsHub_Shown);
			this.KeyUp += new System.Windows.Forms.KeyEventHandler(this.ToolsHub_KeyUp);
			this.tabControl1.ResumeLayout(false);
			this.tabPage1.ResumeLayout(false);
			this.tabPage1.PerformLayout();
			this.tabPage2.ResumeLayout(false);
			this.tabPage2.PerformLayout();
			((System.ComponentModel.ISupportInitialize)(this.CertificatePresentCheck2)).EndInit();
			this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button ReadyToPackageButton;
        private System.Windows.Forms.Button CancelThisFormButton;
        private System.Windows.Forms.TabControl tabControl1;
		private System.Windows.Forms.TabPage tabPage1;
		private System.Windows.Forms.PictureBox CertificatePresentCheck2;
		private System.Windows.Forms.Button ImportCertificateButton2;
		private System.Windows.Forms.Label label13;
		private System.Windows.Forms.LinkLabel linkLabel3;
		private System.Windows.Forms.TabPage tabPage2;
		private System.Windows.Forms.LinkLabel linkLabel4;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label17;
		private System.Windows.Forms.Label label18;
		private System.Windows.Forms.LinkLabel AppleReferenceHyperlink;
		private System.Windows.Forms.Button EditManuallyButton;
		private System.Windows.Forms.TextBox BundleIdentifierEdit;
		private System.Windows.Forms.TextBox BundleNameEdit;
		private System.Windows.Forms.Label label20;
		private System.Windows.Forms.FolderBrowserDialog PickDestFolderDialog;
		private System.Windows.Forms.Button BrowseDeployPathButton;
		private System.Windows.Forms.TextBox DeployPathEdit;
		private System.Windows.Forms.Label label21;
		private System.Windows.Forms.Label label22;
		private System.Windows.Forms.LinkLabel linkLabel2;
		private System.Windows.Forms.LinkLabel linkLabel1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TextBox BundleIconFileEdit;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.Button BrowseIconButton;
    }
}