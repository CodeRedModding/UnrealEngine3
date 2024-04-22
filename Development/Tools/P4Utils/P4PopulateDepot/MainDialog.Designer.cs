namespace P4PopulateDepot
{
	partial class MainDialog
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainDialog));
            this.MainDialogNextButton = new System.Windows.Forms.Button();
            this.MainDialogBackButton = new System.Windows.Forms.Button();
            this.InstallFinishedFooterLine = new System.Windows.Forms.Label();
            this.TitleLabel = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.MainDialogCancelButton = new System.Windows.Forms.Button();
            this.P4BackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.MainDialogTabControl = new P4PopulateDepot.TabControlTabless();
            this.WelcomePage = new System.Windows.Forms.TabPage();
            this.WelcomePageDescriptonTextBox = new System.Windows.Forms.RichTextBox();
            this.P4ConnectionPage = new System.Windows.Forms.TabPage();
            this.ConnectionUserNewButton = new System.Windows.Forms.Button();
            this.ConnectionServerHelpButton = new System.Windows.Forms.Button();
            this.ConnectionErrorDescription = new System.Windows.Forms.Label();
            this.ConnectionPassErrLabel = new System.Windows.Forms.Label();
            this.ConnectionPassTextBox = new System.Windows.Forms.TextBox();
            this.ConnectionPassLabel = new System.Windows.Forms.Label();
            this.ConnectionWorkspaceErrLabel = new System.Windows.Forms.Label();
            this.ConnectionUserErrLabel = new System.Windows.Forms.Label();
            this.ConnectionServerErrLabel = new System.Windows.Forms.Label();
            this.ConnectionWorkspaceNewButton = new System.Windows.Forms.Button();
            this.ConnectionWorkspaceBrowseButton = new System.Windows.Forms.Button();
            this.ConnectionUserBrowseButton = new System.Windows.Forms.Button();
            this.ConnectionWorkspaceRootTextBox = new System.Windows.Forms.TextBox();
            this.ConnectionWorkspaceTextBox = new System.Windows.Forms.TextBox();
            this.ConnectionUserTextBox = new System.Windows.Forms.TextBox();
            this.ConnectionServerTextBox = new System.Windows.Forms.TextBox();
            this.ConnectionWorkspaceRootLabel = new System.Windows.Forms.Label();
            this.ConnectionWorkspaceLabel = new System.Windows.Forms.Label();
            this.ConnectionUserLabel = new System.Windows.Forms.Label();
            this.ConnectionServerLabel = new System.Windows.Forms.Label();
            this.ConnectionDescriptionLabel = new System.Windows.Forms.Label();
            this.P4OperationPreviewPage = new System.Windows.Forms.TabPage();
            this.PreviewPerforceTargetText = new System.Windows.Forms.Label();
            this.PreviewPerforceTargetLabel = new System.Windows.Forms.Label();
            this.PreviewLocalSourceText = new System.Windows.Forms.Label();
            this.PreviewLocalSourceLabel = new System.Windows.Forms.Label();
            this.PreviewNumFoldersText = new System.Windows.Forms.Label();
            this.PreviewNumFoldersLabel = new System.Windows.Forms.Label();
            this.PreviewSizeFilesText = new System.Windows.Forms.Label();
            this.PreviewNumFilesText = new System.Windows.Forms.Label();
            this.PreviewSizeFilesLabel = new System.Windows.Forms.Label();
            this.PreviewNumFilesLabel = new System.Windows.Forms.Label();
            this.PreviewWorkspaceText = new System.Windows.Forms.Label();
            this.PreviewUserText = new System.Windows.Forms.Label();
            this.PreviewServerText = new System.Windows.Forms.Label();
            this.PreviewWorkspaceLabel = new System.Windows.Forms.Label();
            this.PreviewUserLabel = new System.Windows.Forms.Label();
            this.PreviewServerLabel = new System.Windows.Forms.Label();
            this.PreviewDescriptionLabel = new System.Windows.Forms.Label();
            this.CompletePage = new System.Windows.Forms.TabPage();
            this.CompleteRadioButton1 = new System.Windows.Forms.RadioButton();
            this.CompleteRadioButton0 = new System.Windows.Forms.RadioButton();
            this.CompleteContentLabel = new System.Windows.Forms.Label();
            this.MainDialogTabControl.SuspendLayout();
            this.WelcomePage.SuspendLayout();
            this.P4ConnectionPage.SuspendLayout();
            this.P4OperationPreviewPage.SuspendLayout();
            this.CompletePage.SuspendLayout();
            this.SuspendLayout();
            // 
            // MainDialogNextButton
            // 
            this.MainDialogNextButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.MainDialogNextButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.MainDialogNextButton.Location = new System.Drawing.Point(576, 327);
            this.MainDialogNextButton.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.MainDialogNextButton.Name = "MainDialogNextButton";
            this.MainDialogNextButton.Size = new System.Drawing.Size(100, 32);
            this.MainDialogNextButton.TabIndex = 1;
            this.MainDialogNextButton.Text = "Next";
            this.MainDialogNextButton.UseVisualStyleBackColor = true;
            this.MainDialogNextButton.Click += new System.EventHandler(this.MainDialogNextButton_Click);
            // 
            // MainDialogBackButton
            // 
            this.MainDialogBackButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.MainDialogBackButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.MainDialogBackButton.Location = new System.Drawing.Point(470, 327);
            this.MainDialogBackButton.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.MainDialogBackButton.Name = "MainDialogBackButton";
            this.MainDialogBackButton.Size = new System.Drawing.Size(100, 32);
            this.MainDialogBackButton.TabIndex = 35;
            this.MainDialogBackButton.Text = "Back";
            this.MainDialogBackButton.UseVisualStyleBackColor = true;
            this.MainDialogBackButton.Click += new System.EventHandler(this.MainDialogBackButton_Click);
            // 
            // InstallFinishedFooterLine
            // 
            this.InstallFinishedFooterLine.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.InstallFinishedFooterLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.InstallFinishedFooterLine.Location = new System.Drawing.Point(-3, 317);
            this.InstallFinishedFooterLine.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.InstallFinishedFooterLine.Name = "InstallFinishedFooterLine";
            this.InstallFinishedFooterLine.Size = new System.Drawing.Size(800, 2);
            this.InstallFinishedFooterLine.TabIndex = 36;
            // 
            // TitleLabel
            // 
            this.TitleLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.TitleLabel.BackColor = System.Drawing.Color.White;
            this.TitleLabel.Font = new System.Drawing.Font("Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.TitleLabel.Image = global::P4PopulateDepot.Properties.Resources.Banner;
            this.TitleLabel.Location = new System.Drawing.Point(-3, 0);
            this.TitleLabel.Name = "TitleLabel";
            this.TitleLabel.Size = new System.Drawing.Size(800, 68);
            this.TitleLabel.TabIndex = 37;
            this.TitleLabel.Text = "Title";
            this.TitleLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // label1
            // 
            this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.label1.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.label1.Location = new System.Drawing.Point(-3, 69);
            this.label1.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(800, 2);
            this.label1.TabIndex = 38;
            // 
            // MainDialogCancelButton
            // 
            this.MainDialogCancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.MainDialogCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.MainDialogCancelButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.MainDialogCancelButton.Location = new System.Drawing.Point(682, 327);
            this.MainDialogCancelButton.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.MainDialogCancelButton.Name = "MainDialogCancelButton";
            this.MainDialogCancelButton.Size = new System.Drawing.Size(100, 32);
            this.MainDialogCancelButton.TabIndex = 40;
            this.MainDialogCancelButton.Text = "Cancel";
            this.MainDialogCancelButton.UseVisualStyleBackColor = true;
            this.MainDialogCancelButton.MouseClick += new System.Windows.Forms.MouseEventHandler(this.MainDialogCancelButton_MouseClick);
            // 
            // P4BackgroundWorker
            // 
            this.P4BackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.P4BackgroundWorker_DoWork);
            // 
            // MainDialogTabControl
            // 
            this.MainDialogTabControl.Controls.Add(this.WelcomePage);
            this.MainDialogTabControl.Controls.Add(this.P4ConnectionPage);
            this.MainDialogTabControl.Controls.Add(this.P4OperationPreviewPage);
            this.MainDialogTabControl.Controls.Add(this.CompletePage);
            this.MainDialogTabControl.Location = new System.Drawing.Point(1, 71);
            this.MainDialogTabControl.Name = "MainDialogTabControl";
            this.MainDialogTabControl.SelectedIndex = 0;
            this.MainDialogTabControl.Size = new System.Drawing.Size(796, 245);
            this.MainDialogTabControl.TabIndex = 39;
            this.MainDialogTabControl.SelectedIndexChanged += new System.EventHandler(this.MainDialogTabControl_SelectedIndexChanged);
            // 
            // WelcomePage
            // 
            this.WelcomePage.BackColor = System.Drawing.SystemColors.Control;
            this.WelcomePage.Controls.Add(this.WelcomePageDescriptonTextBox);
            this.WelcomePage.Location = new System.Drawing.Point(4, 25);
            this.WelcomePage.Name = "WelcomePage";
            this.WelcomePage.Padding = new System.Windows.Forms.Padding(3);
            this.WelcomePage.Size = new System.Drawing.Size(788, 216);
            this.WelcomePage.TabIndex = 0;
            this.WelcomePage.Text = "WelcomePage";
            // 
            // WelcomePageDescriptonTextBox
            // 
            this.WelcomePageDescriptonTextBox.BackColor = System.Drawing.SystemColors.Control;
            this.WelcomePageDescriptonTextBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.WelcomePageDescriptonTextBox.Location = new System.Drawing.Point(67, 40);
            this.WelcomePageDescriptonTextBox.Name = "WelcomePageDescriptonTextBox";
            this.WelcomePageDescriptonTextBox.ReadOnly = true;
            this.WelcomePageDescriptonTextBox.Size = new System.Drawing.Size(642, 158);
            this.WelcomePageDescriptonTextBox.TabIndex = 0;
            this.WelcomePageDescriptonTextBox.TabStop = false;
            this.WelcomePageDescriptonTextBox.Text = "";
            // 
            // P4ConnectionPage
            // 
            this.P4ConnectionPage.BackColor = System.Drawing.SystemColors.Control;
            this.P4ConnectionPage.Controls.Add(this.ConnectionUserNewButton);
            this.P4ConnectionPage.Controls.Add(this.ConnectionServerHelpButton);
            this.P4ConnectionPage.Controls.Add(this.ConnectionErrorDescription);
            this.P4ConnectionPage.Controls.Add(this.ConnectionPassErrLabel);
            this.P4ConnectionPage.Controls.Add(this.ConnectionPassTextBox);
            this.P4ConnectionPage.Controls.Add(this.ConnectionPassLabel);
            this.P4ConnectionPage.Controls.Add(this.ConnectionWorkspaceErrLabel);
            this.P4ConnectionPage.Controls.Add(this.ConnectionUserErrLabel);
            this.P4ConnectionPage.Controls.Add(this.ConnectionServerErrLabel);
            this.P4ConnectionPage.Controls.Add(this.ConnectionWorkspaceNewButton);
            this.P4ConnectionPage.Controls.Add(this.ConnectionWorkspaceBrowseButton);
            this.P4ConnectionPage.Controls.Add(this.ConnectionUserBrowseButton);
            this.P4ConnectionPage.Controls.Add(this.ConnectionWorkspaceRootTextBox);
            this.P4ConnectionPage.Controls.Add(this.ConnectionWorkspaceTextBox);
            this.P4ConnectionPage.Controls.Add(this.ConnectionUserTextBox);
            this.P4ConnectionPage.Controls.Add(this.ConnectionServerTextBox);
            this.P4ConnectionPage.Controls.Add(this.ConnectionWorkspaceRootLabel);
            this.P4ConnectionPage.Controls.Add(this.ConnectionWorkspaceLabel);
            this.P4ConnectionPage.Controls.Add(this.ConnectionUserLabel);
            this.P4ConnectionPage.Controls.Add(this.ConnectionServerLabel);
            this.P4ConnectionPage.Controls.Add(this.ConnectionDescriptionLabel);
            this.P4ConnectionPage.Location = new System.Drawing.Point(4, 25);
            this.P4ConnectionPage.Name = "P4ConnectionPage";
            this.P4ConnectionPage.Padding = new System.Windows.Forms.Padding(3);
            this.P4ConnectionPage.Size = new System.Drawing.Size(788, 216);
            this.P4ConnectionPage.TabIndex = 1;
            this.P4ConnectionPage.Text = "P4ConnectionPage";
            // 
            // ConnectionUserNewButton
            // 
            this.ConnectionUserNewButton.Location = new System.Drawing.Point(597, 72);
            this.ConnectionUserNewButton.Name = "ConnectionUserNewButton";
            this.ConnectionUserNewButton.Size = new System.Drawing.Size(81, 23);
            this.ConnectionUserNewButton.TabIndex = 25;
            this.ConnectionUserNewButton.Text = "New...";
            this.ConnectionUserNewButton.UseVisualStyleBackColor = true;
            this.ConnectionUserNewButton.Click += new System.EventHandler(this.ConnectionUserNewButton_Click);
            // 
            // ConnectionServerHelpButton
            // 
            this.ConnectionServerHelpButton.Location = new System.Drawing.Point(510, 35);
            this.ConnectionServerHelpButton.Name = "ConnectionServerHelpButton";
            this.ConnectionServerHelpButton.Size = new System.Drawing.Size(24, 23);
            this.ConnectionServerHelpButton.TabIndex = 24;
            this.ConnectionServerHelpButton.Text = "?";
            this.ConnectionServerHelpButton.UseVisualStyleBackColor = true;
            this.ConnectionServerHelpButton.Click += new System.EventHandler(this.ConnectionServerHelpButton_Click);
            // 
            // ConnectionErrorDescription
            // 
            this.ConnectionErrorDescription.ForeColor = System.Drawing.Color.Firebrick;
            this.ConnectionErrorDescription.Location = new System.Drawing.Point(42, 209);
            this.ConnectionErrorDescription.Name = "ConnectionErrorDescription";
            this.ConnectionErrorDescription.Size = new System.Drawing.Size(713, 18);
            this.ConnectionErrorDescription.TabIndex = 23;
            this.ConnectionErrorDescription.Text = "Error Text";
            // 
            // ConnectionPassErrLabel
            // 
            this.ConnectionPassErrLabel.AutoSize = true;
            this.ConnectionPassErrLabel.ForeColor = System.Drawing.Color.Firebrick;
            this.ConnectionPassErrLabel.Image = global::P4PopulateDepot.Properties.Resources.red_arrow_right;
            this.ConnectionPassErrLabel.Location = new System.Drawing.Point(174, 109);
            this.ConnectionPassErrLabel.Name = "ConnectionPassErrLabel";
            this.ConnectionPassErrLabel.Size = new System.Drawing.Size(12, 16);
            this.ConnectionPassErrLabel.TabIndex = 18;
            this.ConnectionPassErrLabel.Text = " ";
            // 
            // ConnectionPassTextBox
            // 
            this.ConnectionPassTextBox.ForeColor = System.Drawing.SystemColors.GrayText;
            this.ConnectionPassTextBox.Location = new System.Drawing.Point(186, 106);
            this.ConnectionPassTextBox.Name = "ConnectionPassTextBox";
            this.ConnectionPassTextBox.Size = new System.Drawing.Size(318, 23);
            this.ConnectionPassTextBox.TabIndex = 7;
            this.ConnectionPassTextBox.UseSystemPasswordChar = true;
            this.ConnectionPassTextBox.TextChanged += new System.EventHandler(this.ConnectionPassTextBox_TextChanged);
            this.ConnectionPassTextBox.Enter += new System.EventHandler(this.ConnectionPassTextBox_Enter);
            // 
            // ConnectionPassLabel
            // 
            this.ConnectionPassLabel.AutoSize = true;
            this.ConnectionPassLabel.Location = new System.Drawing.Point(79, 109);
            this.ConnectionPassLabel.Name = "ConnectionPassLabel";
            this.ConnectionPassLabel.Size = new System.Drawing.Size(63, 16);
            this.ConnectionPassLabel.TabIndex = 16;
            this.ConnectionPassLabel.Text = "Password";
            // 
            // ConnectionWorkspaceErrLabel
            // 
            this.ConnectionWorkspaceErrLabel.AutoSize = true;
            this.ConnectionWorkspaceErrLabel.ForeColor = System.Drawing.Color.Firebrick;
            this.ConnectionWorkspaceErrLabel.Image = global::P4PopulateDepot.Properties.Resources.red_arrow_right;
            this.ConnectionWorkspaceErrLabel.Location = new System.Drawing.Point(174, 144);
            this.ConnectionWorkspaceErrLabel.Name = "ConnectionWorkspaceErrLabel";
            this.ConnectionWorkspaceErrLabel.Size = new System.Drawing.Size(12, 16);
            this.ConnectionWorkspaceErrLabel.TabIndex = 14;
            this.ConnectionWorkspaceErrLabel.Text = " ";
            // 
            // ConnectionUserErrLabel
            // 
            this.ConnectionUserErrLabel.AutoSize = true;
            this.ConnectionUserErrLabel.ForeColor = System.Drawing.Color.Firebrick;
            this.ConnectionUserErrLabel.Image = global::P4PopulateDepot.Properties.Resources.red_arrow_right;
            this.ConnectionUserErrLabel.Location = new System.Drawing.Point(174, 75);
            this.ConnectionUserErrLabel.Name = "ConnectionUserErrLabel";
            this.ConnectionUserErrLabel.Size = new System.Drawing.Size(12, 16);
            this.ConnectionUserErrLabel.TabIndex = 13;
            this.ConnectionUserErrLabel.Text = " ";
            // 
            // ConnectionServerErrLabel
            // 
            this.ConnectionServerErrLabel.AutoSize = true;
            this.ConnectionServerErrLabel.ForeColor = System.Drawing.Color.Firebrick;
            this.ConnectionServerErrLabel.Image = global::P4PopulateDepot.Properties.Resources.red_arrow_right;
            this.ConnectionServerErrLabel.Location = new System.Drawing.Point(174, 38);
            this.ConnectionServerErrLabel.Name = "ConnectionServerErrLabel";
            this.ConnectionServerErrLabel.Size = new System.Drawing.Size(12, 16);
            this.ConnectionServerErrLabel.TabIndex = 12;
            this.ConnectionServerErrLabel.Text = " ";
            // 
            // ConnectionWorkspaceNewButton
            // 
            this.ConnectionWorkspaceNewButton.Location = new System.Drawing.Point(597, 141);
            this.ConnectionWorkspaceNewButton.Name = "ConnectionWorkspaceNewButton";
            this.ConnectionWorkspaceNewButton.Size = new System.Drawing.Size(81, 23);
            this.ConnectionWorkspaceNewButton.TabIndex = 11;
            this.ConnectionWorkspaceNewButton.Text = "New...";
            this.ConnectionWorkspaceNewButton.UseVisualStyleBackColor = true;
            this.ConnectionWorkspaceNewButton.Click += new System.EventHandler(this.ConnectionWorkspaceNewButton_Click);
            // 
            // ConnectionWorkspaceBrowseButton
            // 
            this.ConnectionWorkspaceBrowseButton.Location = new System.Drawing.Point(510, 141);
            this.ConnectionWorkspaceBrowseButton.Name = "ConnectionWorkspaceBrowseButton";
            this.ConnectionWorkspaceBrowseButton.Size = new System.Drawing.Size(81, 23);
            this.ConnectionWorkspaceBrowseButton.TabIndex = 10;
            this.ConnectionWorkspaceBrowseButton.Text = "Browse...";
            this.ConnectionWorkspaceBrowseButton.UseVisualStyleBackColor = true;
            this.ConnectionWorkspaceBrowseButton.Click += new System.EventHandler(this.ConnectionWorkspaceBrowseButton_Click);
            // 
            // ConnectionUserBrowseButton
            // 
            this.ConnectionUserBrowseButton.Location = new System.Drawing.Point(510, 72);
            this.ConnectionUserBrowseButton.Name = "ConnectionUserBrowseButton";
            this.ConnectionUserBrowseButton.Size = new System.Drawing.Size(81, 23);
            this.ConnectionUserBrowseButton.TabIndex = 9;
            this.ConnectionUserBrowseButton.Text = "Browse...";
            this.ConnectionUserBrowseButton.UseVisualStyleBackColor = true;
            this.ConnectionUserBrowseButton.Click += new System.EventHandler(this.ConnectionUserBrowseButton_Click);
            // 
            // ConnectionWorkspaceRootTextBox
            // 
            this.ConnectionWorkspaceRootTextBox.Location = new System.Drawing.Point(186, 175);
            this.ConnectionWorkspaceRootTextBox.Name = "ConnectionWorkspaceRootTextBox";
            this.ConnectionWorkspaceRootTextBox.ReadOnly = true;
            this.ConnectionWorkspaceRootTextBox.Size = new System.Drawing.Size(318, 23);
            this.ConnectionWorkspaceRootTextBox.TabIndex = 8;
            this.ConnectionWorkspaceRootTextBox.TabStop = false;
            // 
            // ConnectionWorkspaceTextBox
            // 
            this.ConnectionWorkspaceTextBox.ForeColor = System.Drawing.SystemColors.WindowText;
            this.ConnectionWorkspaceTextBox.Location = new System.Drawing.Point(186, 141);
            this.ConnectionWorkspaceTextBox.Name = "ConnectionWorkspaceTextBox";
            this.ConnectionWorkspaceTextBox.Size = new System.Drawing.Size(318, 23);
            this.ConnectionWorkspaceTextBox.TabIndex = 8;
            this.ConnectionWorkspaceTextBox.TextChanged += new System.EventHandler(this.ConnectionWorkspaceTextBox_TextChanged);
            // 
            // ConnectionUserTextBox
            // 
            this.ConnectionUserTextBox.Location = new System.Drawing.Point(186, 72);
            this.ConnectionUserTextBox.Name = "ConnectionUserTextBox";
            this.ConnectionUserTextBox.Size = new System.Drawing.Size(318, 23);
            this.ConnectionUserTextBox.TabIndex = 6;
            this.ConnectionUserTextBox.TextChanged += new System.EventHandler(this.ConnectionUserTextBox_TextChanged);
            // 
            // ConnectionServerTextBox
            // 
            this.ConnectionServerTextBox.Location = new System.Drawing.Point(186, 35);
            this.ConnectionServerTextBox.Name = "ConnectionServerTextBox";
            this.ConnectionServerTextBox.Size = new System.Drawing.Size(318, 23);
            this.ConnectionServerTextBox.TabIndex = 5;
            this.ConnectionServerTextBox.TextChanged += new System.EventHandler(this.ConnectionServerTextBox_TextChanged);
            // 
            // ConnectionWorkspaceRootLabel
            // 
            this.ConnectionWorkspaceRootLabel.AutoSize = true;
            this.ConnectionWorkspaceRootLabel.Location = new System.Drawing.Point(79, 178);
            this.ConnectionWorkspaceRootLabel.Name = "ConnectionWorkspaceRootLabel";
            this.ConnectionWorkspaceRootLabel.Size = new System.Drawing.Size(101, 16);
            this.ConnectionWorkspaceRootLabel.TabIndex = 4;
            this.ConnectionWorkspaceRootLabel.Text = "Workspace Root";
            // 
            // ConnectionWorkspaceLabel
            // 
            this.ConnectionWorkspaceLabel.AutoSize = true;
            this.ConnectionWorkspaceLabel.Location = new System.Drawing.Point(79, 144);
            this.ConnectionWorkspaceLabel.Name = "ConnectionWorkspaceLabel";
            this.ConnectionWorkspaceLabel.Size = new System.Drawing.Size(71, 16);
            this.ConnectionWorkspaceLabel.TabIndex = 3;
            this.ConnectionWorkspaceLabel.Text = "Workspace";
            // 
            // ConnectionUserLabel
            // 
            this.ConnectionUserLabel.AutoSize = true;
            this.ConnectionUserLabel.Location = new System.Drawing.Point(79, 75);
            this.ConnectionUserLabel.Name = "ConnectionUserLabel";
            this.ConnectionUserLabel.Size = new System.Drawing.Size(34, 16);
            this.ConnectionUserLabel.TabIndex = 2;
            this.ConnectionUserLabel.Text = "User";
            // 
            // ConnectionServerLabel
            // 
            this.ConnectionServerLabel.AutoSize = true;
            this.ConnectionServerLabel.Location = new System.Drawing.Point(79, 38);
            this.ConnectionServerLabel.Name = "ConnectionServerLabel";
            this.ConnectionServerLabel.Size = new System.Drawing.Size(46, 16);
            this.ConnectionServerLabel.TabIndex = 1;
            this.ConnectionServerLabel.Text = "Server";
            // 
            // ConnectionDescriptionLabel
            // 
            this.ConnectionDescriptionLabel.AutoSize = true;
            this.ConnectionDescriptionLabel.Location = new System.Drawing.Point(39, 10);
            this.ConnectionDescriptionLabel.Name = "ConnectionDescriptionLabel";
            this.ConnectionDescriptionLabel.Size = new System.Drawing.Size(251, 16);
            this.ConnectionDescriptionLabel.TabIndex = 0;
            this.ConnectionDescriptionLabel.Text = "Enter your Perforce connection info below.";
            // 
            // P4OperationPreviewPage
            // 
            this.P4OperationPreviewPage.BackColor = System.Drawing.SystemColors.Control;
            this.P4OperationPreviewPage.Controls.Add(this.PreviewPerforceTargetText);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewPerforceTargetLabel);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewLocalSourceText);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewLocalSourceLabel);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewNumFoldersText);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewNumFoldersLabel);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewSizeFilesText);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewNumFilesText);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewSizeFilesLabel);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewNumFilesLabel);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewWorkspaceText);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewUserText);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewServerText);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewWorkspaceLabel);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewUserLabel);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewServerLabel);
            this.P4OperationPreviewPage.Controls.Add(this.PreviewDescriptionLabel);
            this.P4OperationPreviewPage.Location = new System.Drawing.Point(4, 25);
            this.P4OperationPreviewPage.Name = "P4OperationPreviewPage";
            this.P4OperationPreviewPage.Padding = new System.Windows.Forms.Padding(3);
            this.P4OperationPreviewPage.Size = new System.Drawing.Size(788, 216);
            this.P4OperationPreviewPage.TabIndex = 2;
            this.P4OperationPreviewPage.Text = "P4OperationPreviewPage";
            // 
            // PreviewPerforceTargetText
            // 
            this.PreviewPerforceTargetText.Location = new System.Drawing.Point(184, 151);
            this.PreviewPerforceTargetText.Name = "PreviewPerforceTargetText";
            this.PreviewPerforceTargetText.Size = new System.Drawing.Size(593, 16);
            this.PreviewPerforceTargetText.TabIndex = 20;
            this.PreviewPerforceTargetText.Text = "//depot";
            // 
            // PreviewPerforceTargetLabel
            // 
            this.PreviewPerforceTargetLabel.AutoSize = true;
            this.PreviewPerforceTargetLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.PreviewPerforceTargetLabel.Location = new System.Drawing.Point(69, 151);
            this.PreviewPerforceTargetLabel.Name = "PreviewPerforceTargetLabel";
            this.PreviewPerforceTargetLabel.Size = new System.Drawing.Size(111, 16);
            this.PreviewPerforceTargetLabel.TabIndex = 19;
            this.PreviewPerforceTargetLabel.Text = "Perforce Target";
            // 
            // PreviewLocalSourceText
            // 
            this.PreviewLocalSourceText.Location = new System.Drawing.Point(184, 122);
            this.PreviewLocalSourceText.Name = "PreviewLocalSourceText";
            this.PreviewLocalSourceText.Size = new System.Drawing.Size(593, 16);
            this.PreviewLocalSourceText.TabIndex = 18;
            this.PreviewLocalSourceText.Text = "C:\\";
            // 
            // PreviewLocalSourceLabel
            // 
            this.PreviewLocalSourceLabel.AutoSize = true;
            this.PreviewLocalSourceLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.PreviewLocalSourceLabel.Location = new System.Drawing.Point(69, 122);
            this.PreviewLocalSourceLabel.Name = "PreviewLocalSourceLabel";
            this.PreviewLocalSourceLabel.Size = new System.Drawing.Size(90, 16);
            this.PreviewLocalSourceLabel.TabIndex = 17;
            this.PreviewLocalSourceLabel.Text = "Local Source";
            // 
            // PreviewNumFoldersText
            // 
            this.PreviewNumFoldersText.AutoSize = true;
            this.PreviewNumFoldersText.Location = new System.Drawing.Point(555, 72);
            this.PreviewNumFoldersText.Name = "PreviewNumFoldersText";
            this.PreviewNumFoldersText.Size = new System.Drawing.Size(15, 16);
            this.PreviewNumFoldersText.TabIndex = 16;
            this.PreviewNumFoldersText.Text = "1";
            // 
            // PreviewNumFoldersLabel
            // 
            this.PreviewNumFoldersLabel.AutoSize = true;
            this.PreviewNumFoldersLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.PreviewNumFoldersLabel.Location = new System.Drawing.Point(434, 72);
            this.PreviewNumFoldersLabel.Name = "PreviewNumFoldersLabel";
            this.PreviewNumFoldersLabel.Size = new System.Drawing.Size(89, 16);
            this.PreviewNumFoldersLabel.TabIndex = 15;
            this.PreviewNumFoldersLabel.Text = "Folder Count";
            // 
            // PreviewSizeFilesText
            // 
            this.PreviewSizeFilesText.AutoSize = true;
            this.PreviewSizeFilesText.Location = new System.Drawing.Point(555, 95);
            this.PreviewSizeFilesText.Name = "PreviewSizeFilesText";
            this.PreviewSizeFilesText.Size = new System.Drawing.Size(40, 16);
            this.PreviewSizeFilesText.TabIndex = 14;
            this.PreviewSizeFilesText.Text = "14 KB";
            // 
            // PreviewNumFilesText
            // 
            this.PreviewNumFilesText.AutoSize = true;
            this.PreviewNumFilesText.Location = new System.Drawing.Point(555, 49);
            this.PreviewNumFilesText.Name = "PreviewNumFilesText";
            this.PreviewNumFilesText.Size = new System.Drawing.Size(15, 16);
            this.PreviewNumFilesText.TabIndex = 13;
            this.PreviewNumFilesText.Text = "1";
            // 
            // PreviewSizeFilesLabel
            // 
            this.PreviewSizeFilesLabel.AutoSize = true;
            this.PreviewSizeFilesLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.PreviewSizeFilesLabel.Location = new System.Drawing.Point(434, 95);
            this.PreviewSizeFilesLabel.Name = "PreviewSizeFilesLabel";
            this.PreviewSizeFilesLabel.Size = new System.Drawing.Size(70, 16);
            this.PreviewSizeFilesLabel.TabIndex = 12;
            this.PreviewSizeFilesLabel.Text = "Total Size";
            // 
            // PreviewNumFilesLabel
            // 
            this.PreviewNumFilesLabel.AutoSize = true;
            this.PreviewNumFilesLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.PreviewNumFilesLabel.Location = new System.Drawing.Point(434, 49);
            this.PreviewNumFilesLabel.Name = "PreviewNumFilesLabel";
            this.PreviewNumFilesLabel.Size = new System.Drawing.Size(70, 16);
            this.PreviewNumFilesLabel.TabIndex = 11;
            this.PreviewNumFilesLabel.Text = "File Count";
            // 
            // PreviewWorkspaceText
            // 
            this.PreviewWorkspaceText.Location = new System.Drawing.Point(154, 95);
            this.PreviewWorkspaceText.Name = "PreviewWorkspaceText";
            this.PreviewWorkspaceText.Size = new System.Drawing.Size(255, 16);
            this.PreviewWorkspaceText.TabIndex = 10;
            this.PreviewWorkspaceText.Text = "WorkspaceName";
            // 
            // PreviewUserText
            // 
            this.PreviewUserText.Location = new System.Drawing.Point(154, 72);
            this.PreviewUserText.Name = "PreviewUserText";
            this.PreviewUserText.Size = new System.Drawing.Size(255, 16);
            this.PreviewUserText.TabIndex = 9;
            this.PreviewUserText.Text = "UserName";
            // 
            // PreviewServerText
            // 
            this.PreviewServerText.Location = new System.Drawing.Point(154, 49);
            this.PreviewServerText.Name = "PreviewServerText";
            this.PreviewServerText.Size = new System.Drawing.Size(255, 16);
            this.PreviewServerText.TabIndex = 8;
            this.PreviewServerText.Text = "127.0.0.1:1666";
            // 
            // PreviewWorkspaceLabel
            // 
            this.PreviewWorkspaceLabel.AutoSize = true;
            this.PreviewWorkspaceLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.PreviewWorkspaceLabel.Location = new System.Drawing.Point(69, 95);
            this.PreviewWorkspaceLabel.Name = "PreviewWorkspaceLabel";
            this.PreviewWorkspaceLabel.Size = new System.Drawing.Size(80, 16);
            this.PreviewWorkspaceLabel.TabIndex = 7;
            this.PreviewWorkspaceLabel.Text = "Workspace";
            // 
            // PreviewUserLabel
            // 
            this.PreviewUserLabel.AutoSize = true;
            this.PreviewUserLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.PreviewUserLabel.Location = new System.Drawing.Point(69, 72);
            this.PreviewUserLabel.Name = "PreviewUserLabel";
            this.PreviewUserLabel.Size = new System.Drawing.Size(37, 16);
            this.PreviewUserLabel.TabIndex = 6;
            this.PreviewUserLabel.Text = "User";
            // 
            // PreviewServerLabel
            // 
            this.PreviewServerLabel.AutoSize = true;
            this.PreviewServerLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.PreviewServerLabel.Location = new System.Drawing.Point(69, 49);
            this.PreviewServerLabel.Name = "PreviewServerLabel";
            this.PreviewServerLabel.Size = new System.Drawing.Size(52, 16);
            this.PreviewServerLabel.TabIndex = 5;
            this.PreviewServerLabel.Text = "Server";
            // 
            // PreviewDescriptionLabel
            // 
            this.PreviewDescriptionLabel.AutoSize = true;
            this.PreviewDescriptionLabel.Location = new System.Drawing.Point(39, 14);
            this.PreviewDescriptionLabel.Name = "PreviewDescriptionLabel";
            this.PreviewDescriptionLabel.Size = new System.Drawing.Size(447, 16);
            this.PreviewDescriptionLabel.TabIndex = 4;
            this.PreviewDescriptionLabel.Text = "Ready to add files.  To begin adding files to the Perforce Server, click \"Next\".";
            // 
            // CompletePage
            // 
            this.CompletePage.BackColor = System.Drawing.SystemColors.Control;
            this.CompletePage.Controls.Add(this.CompleteRadioButton1);
            this.CompletePage.Controls.Add(this.CompleteRadioButton0);
            this.CompletePage.Controls.Add(this.CompleteContentLabel);
            this.CompletePage.Location = new System.Drawing.Point(4, 25);
            this.CompletePage.Name = "CompletePage";
            this.CompletePage.Padding = new System.Windows.Forms.Padding(3);
            this.CompletePage.Size = new System.Drawing.Size(788, 216);
            this.CompletePage.TabIndex = 3;
            this.CompletePage.Text = "CompletePage";
            // 
            // CompleteRadioButton1
            // 
            this.CompleteRadioButton1.AutoSize = true;
            this.CompleteRadioButton1.Location = new System.Drawing.Point(330, 83);
            this.CompleteRadioButton1.Name = "CompleteRadioButton1";
            this.CompleteRadioButton1.Size = new System.Drawing.Size(128, 20);
            this.CompleteRadioButton1.TabIndex = 34;
            this.CompleteRadioButton1.TabStop = true;
            this.CompleteRadioButton1.Text = "Return to Desktop";
            this.CompleteRadioButton1.UseVisualStyleBackColor = true;
            // 
            // CompleteRadioButton0
            // 
            this.CompleteRadioButton0.AutoSize = true;
            this.CompleteRadioButton0.Location = new System.Drawing.Point(330, 57);
            this.CompleteRadioButton0.Name = "CompleteRadioButton0";
            this.CompleteRadioButton0.Size = new System.Drawing.Size(128, 20);
            this.CompleteRadioButton0.TabIndex = 33;
            this.CompleteRadioButton0.TabStop = true;
            this.CompleteRadioButton0.Text = "Return to Desktop";
            this.CompleteRadioButton0.UseVisualStyleBackColor = true;
            // 
            // CompleteContentLabel
            // 
            this.CompleteContentLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.CompleteContentLabel.Location = new System.Drawing.Point(9, 6);
            this.CompleteContentLabel.Name = "CompleteContentLabel";
            this.CompleteContentLabel.Size = new System.Drawing.Size(770, 23);
            this.CompleteContentLabel.TabIndex = 29;
            this.CompleteContentLabel.Text = "Finished submitting Unreal Development Kit files to the Perforce depot";
            this.CompleteContentLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // MainDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(794, 372);
            this.Controls.Add(this.MainDialogCancelButton);
            this.Controls.Add(this.MainDialogTabControl);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.TitleLabel);
            this.Controls.Add(this.InstallFinishedFooterLine);
            this.Controls.Add(this.MainDialogBackButton);
            this.Controls.Add(this.MainDialogNextButton);
            this.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "MainDialog";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Title";
            this.Shown += new System.EventHandler(this.MainDialog_Shown);
            this.MainDialogTabControl.ResumeLayout(false);
            this.WelcomePage.ResumeLayout(false);
            this.P4ConnectionPage.ResumeLayout(false);
            this.P4ConnectionPage.PerformLayout();
            this.P4OperationPreviewPage.ResumeLayout(false);
            this.P4OperationPreviewPage.PerformLayout();
            this.CompletePage.ResumeLayout(false);
            this.CompletePage.PerformLayout();
            this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Button MainDialogNextButton;
		private System.Windows.Forms.Button MainDialogBackButton;
		private System.Windows.Forms.Label InstallFinishedFooterLine;
		private System.Windows.Forms.Label TitleLabel;
		private System.Windows.Forms.Label label1;
		private TabControlTabless MainDialogTabControl;
		private System.Windows.Forms.TabPage WelcomePage;
		private System.Windows.Forms.TabPage P4ConnectionPage;
		private System.Windows.Forms.TabPage P4OperationPreviewPage;
		private System.Windows.Forms.TabPage CompletePage;
		private System.Windows.Forms.Button MainDialogCancelButton;
		private System.Windows.Forms.RichTextBox WelcomePageDescriptonTextBox;
		private System.Windows.Forms.Button ConnectionWorkspaceNewButton;
		private System.Windows.Forms.Button ConnectionWorkspaceBrowseButton;
		private System.Windows.Forms.Button ConnectionUserBrowseButton;
		private System.Windows.Forms.TextBox ConnectionWorkspaceRootTextBox;
		private System.Windows.Forms.TextBox ConnectionWorkspaceTextBox;
		private System.Windows.Forms.TextBox ConnectionUserTextBox;
		private System.Windows.Forms.TextBox ConnectionServerTextBox;
		private System.Windows.Forms.Label ConnectionWorkspaceRootLabel;
		private System.Windows.Forms.Label ConnectionWorkspaceLabel;
		private System.Windows.Forms.Label ConnectionUserLabel;
		private System.Windows.Forms.Label ConnectionServerLabel;
		private System.Windows.Forms.Label ConnectionDescriptionLabel;
		private System.Windows.Forms.Label ConnectionWorkspaceErrLabel;
		private System.Windows.Forms.Label ConnectionUserErrLabel;
		private System.Windows.Forms.Label ConnectionServerErrLabel;
		private System.Windows.Forms.Label PreviewWorkspaceText;
		private System.Windows.Forms.Label PreviewUserText;
		private System.Windows.Forms.Label PreviewServerText;
		private System.Windows.Forms.Label PreviewWorkspaceLabel;
		private System.Windows.Forms.Label PreviewUserLabel;
		private System.Windows.Forms.Label PreviewServerLabel;
		private System.Windows.Forms.Label PreviewDescriptionLabel;
		private System.Windows.Forms.Label PreviewSizeFilesText;
		private System.Windows.Forms.Label PreviewNumFilesText;
		private System.Windows.Forms.Label PreviewSizeFilesLabel;
		private System.Windows.Forms.Label PreviewNumFilesLabel;
		private System.Windows.Forms.Label PreviewNumFoldersText;
		private System.Windows.Forms.Label PreviewNumFoldersLabel;
		private System.Windows.Forms.Label ConnectionPassErrLabel;
		private System.Windows.Forms.TextBox ConnectionPassTextBox;
		private System.Windows.Forms.Label ConnectionPassLabel;
		private System.ComponentModel.BackgroundWorker P4BackgroundWorker;
		private System.Windows.Forms.RadioButton CompleteRadioButton1;
		private System.Windows.Forms.RadioButton CompleteRadioButton0;
		private System.Windows.Forms.Label CompleteContentLabel;
		private System.Windows.Forms.Label ConnectionErrorDescription;
		private System.Windows.Forms.Label PreviewPerforceTargetText;
		private System.Windows.Forms.Label PreviewPerforceTargetLabel;
		private System.Windows.Forms.Label PreviewLocalSourceText;
		private System.Windows.Forms.Label PreviewLocalSourceLabel;
        private System.Windows.Forms.Button ConnectionServerHelpButton;
        private System.Windows.Forms.Button ConnectionUserNewButton;
	}
}

