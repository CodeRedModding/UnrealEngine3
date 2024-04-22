// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Xml;
using System.Xml.Serialization;

namespace P4PopulateDepot
{
	public partial class MainDialog : Form
	{
		private Point ButtonLocationLeft;
		private Point ButtonLocationMiddle;
		private Point ButtonLocationRight;

		private bool bIgnoreConnectionUpdates = false;
		private volatile bool ShowProgress = false;

		private RadioButton[] CompleteRadioButtons = null;

		static MainDialog CurForm = null;

		static System.Windows.Forms.Timer UpdateTimer = new System.Windows.Forms.Timer();

		public MainDialog()
		{
			InitializeComponent();

			CompleteRadioButtons = new RadioButton[]
			{
				CompleteRadioButton0,
				CompleteRadioButton1
			};

			Text = Program.Util.GetPhrase("TitleWelcome");

			ButtonLocationLeft = MainDialogBackButton.Location;
			ButtonLocationMiddle = MainDialogNextButton.Location;
			ButtonLocationRight = MainDialogCancelButton.Location;

			WelcomePageDescriptonTextBox.SelectedText = Program.Util.GetPhrase("WPDescription");

			ConnectionDescriptionLabel.Text = Program.Util.GetPhrase("CONDescription");
			ConnectionServerLabel.Text = Program.Util.GetPhrase("CONServer");
			ConnectionUserLabel.Text = Program.Util.GetPhrase("CONUser");
			ConnectionWorkspaceLabel.Text = Program.Util.GetPhrase("CONWorkspace");
			ConnectionWorkspaceRootLabel.Text = Program.Util.GetPhrase("CONWorkspaceRoot");
			ConnectionUserBrowseButton.Text = Program.Util.GetPhrase("CONBrowse");
            ConnectionUserNewButton.Text = Program.Util.GetPhrase("CONNew");
			ConnectionWorkspaceBrowseButton.Text = Program.Util.GetPhrase("CONBrowse");
			ConnectionWorkspaceNewButton.Text = Program.Util.GetPhrase("CONNew");
            ConnectionPassTextBox.Text = Program.Util.GetPhrase("CONPassInfo");
            ConnectionPassTextBox.UseSystemPasswordChar = false;
            ConnectionPassTextBox.ForeColor = System.Drawing.SystemColors.GrayText;

			PreviewServerLabel.Text = Program.Util.GetPhrase("CONServer");
			PreviewUserLabel.Text = Program.Util.GetPhrase("CONUser");
			PreviewDescriptionLabel.Text = Program.Util.GetPhrase("PREVDescription");
			PreviewWorkspaceLabel.Text = Program.Util.GetPhrase("CONWorkspace");
			PreviewNumFilesLabel.Text = Program.Util.GetPhrase("PREVNumFiles");
			PreviewNumFoldersLabel.Text = Program.Util.GetPhrase("PREVNumFolders");
			PreviewSizeFilesText.Text = Program.Util.GetPhrase("PREVFileSize");
			PreviewLocalSourceLabel.Text =  Program.Util.GetPhrase("PREVSrcLabel");
			PreviewPerforceTargetLabel.Text =  Program.Util.GetPhrase("PREVTargetLabel");
			PreviewLocalSourceText.Text = Utils.GetProjectRoot();

			UpdateTimer.Tick += new EventHandler(UpdateTimerEventProcessor);
			UpdateTimer.Interval = 1500;
			
			UpdatePage();
			CurForm = this;
		}

		private static void UpdateTimerEventProcessor(Object InObject, EventArgs InArgs)
		{
			UpdateTimer.Stop();
			CurForm.UpdatePage();
		}

		/// <summary>
		/// Restores all the UI buttons to their default states.
		/// </summary>
		private void SetButtonDefaults()
		{
			MainDialogCancelButton.Text = Program.Util.GetPhrase("GQCancel");
			MainDialogNextButton.Text = Program.Util.GetPhrase("GQNext");
			MainDialogBackButton.Text = Program.Util.GetPhrase("GQBack");
			
			MainDialogBackButton.Location =	ButtonLocationLeft;
			MainDialogNextButton.Location =	ButtonLocationMiddle;
			MainDialogCancelButton.Location = ButtonLocationRight; 

			MainDialogBackButton.Visible = true;
			MainDialogNextButton.Visible = true;
			MainDialogCancelButton.Visible = true;

			MainDialogBackButton.Enabled = true;
			if (MainDialogTabControl.SelectedTab.Name == "P4ConnectionPage")
			{
				MainDialogNextButton.Enabled = false;
                ConnectionUserBrowseButton.Enabled = false;
                ConnectionUserNewButton.Enabled = false;
				ConnectionWorkspaceBrowseButton.Enabled = false;
				ConnectionWorkspaceNewButton.Enabled = false;
			}
			else
			{
				MainDialogNextButton.Enabled = true;
			}
			
			MainDialogCancelButton.Enabled = true;
		}

		/// <summary>
		/// Returns all the UI error widgets to their default states.
		/// </summary>
		private void SetErrorDefaults()
		{
			ConnectionServerErrLabel.Visible = false;
			ConnectionUserErrLabel.Visible = false;
			ConnectionWorkspaceErrLabel.Visible = false;
			ConnectionPassErrLabel.Visible = false;

			ConnectionErrorDescription.Visible = false;
		}


		/// <summary>
		/// Determines whether a workspace is valid for this project.
		/// </summary>
		/// <returns>
		///   <c>true</c> if the workspace is valid, or <c>false</c> otherwise.
		/// </returns>
		private bool IsWorkspaceValid()
		{
			string ConnWSRoot = Program.Util.GetClientRoot(ConnectionUserTextBox.Text, ConnectionWorkspaceTextBox.Text);
			ConnectionWorkspaceRootTextBox.Text = ConnWSRoot;

			bool bIsProjectUnderWorkspaceRoot = (!string.IsNullOrEmpty(ConnWSRoot) && Program.Util.IsSubDirectory(new DirectoryInfo(Utils.GetProjectRoot()), new DirectoryInfo(ConnWSRoot)));
			
			if (!bIsProjectUnderWorkspaceRoot)
			{
				ConnectionWorkspaceErrLabel.Visible = true;
				ConnectionErrorDescription.Text = Program.Util.GetPhrase("ERRWorkspace");
				ConnectionErrorDescription.Visible = true;
				return false;
			}
			return true;
		}

        /// <summary>
        /// The password field is setup to use a number of defaults that we do not wish to use when the user enters actual text.  We 
        /// clear default values here.
        /// </summary>
        private void ClearPassDefaults()
        {
            if (ConnectionPassTextBox.Text == Program.Util.GetPhrase("CONPassInfo"))
            {
                bIgnoreConnectionUpdates = true;
                ConnectionPassTextBox.Text = string.Empty;
                bIgnoreConnectionUpdates = false;
            }

            if (ConnectionPassTextBox.UseSystemPasswordChar != true)
            {
                ConnectionPassTextBox.UseSystemPasswordChar = true;
            }

            if (ConnectionPassTextBox.ForeColor == System.Drawing.SystemColors.GrayText)
            {
                ConnectionPassTextBox.ForeColor = System.Drawing.SystemColors.WindowText;
            }
        }


		/// <summary>
		/// Updates the UI components of the active utility panel.
		/// </summary>
		private void UpdatePage()
		{
			switch (MainDialogTabControl.SelectedTab.Name)
			{
				case "WelcomePage":
					{
						// Update buttons
						SetButtonDefaults();
						MainDialogBackButton.Visible = false;

						// Update contents
						TitleLabel.Text = Program.Util.GetPhrase("TitleWelcome");

						break;
					}

				case "P4ConnectionPage":
					{
						// We break out of the connection update if it is disabled.
						if (bIgnoreConnectionUpdates == true)
						{
							break;
						}

						// Update buttons
						SetButtonDefaults();

						// Clear any error info
						SetErrorDefaults();

						// Update contents
						TitleLabel.Text = Program.Util.GetPhrase("TitleConnection");
						Application.DoEvents();

						// P4 operations can take a bit of time so we'll bring up the wait cursor.
						Cursor CurRestore = Cursor.Current;
						Cursor.Current = Cursors.WaitCursor;

						{
							if(Program.Util.P4ConInfo.Port != ConnectionServerTextBox.Text) Program.Util.P4ConInfo.Port = ConnectionServerTextBox.Text;
							if(Program.Util.P4ConInfo.User != ConnectionUserTextBox.Text) Program.Util.P4ConInfo.User = ConnectionUserTextBox.Text;
							if(Program.Util.P4ConInfo.Client != ConnectionWorkspaceTextBox.Text) Program.Util.P4ConInfo.Client = ConnectionWorkspaceTextBox.Text;
                            string PasswordValue = (ConnectionPassTextBox.Text == Program.Util.GetPhrase("CONPassInfo")) ? string.Empty : ConnectionPassTextBox.Text;
                            if (Program.Util.P4ConInfo.Password != PasswordValue) Program.Util.P4ConInfo.Password = PasswordValue;

							// If the connection has been invalidated or there is no connection, we try to establish one.
							if (!Program.Util.P4ConInfo.P4IsConnectionValid(false, false))
							{
								Program.Util.P4ConInfo.P4Init();
							}

                            
                            bIgnoreConnectionUpdates = true;
                            if (Program.Util.P4ConInfo.Port != null) ConnectionServerTextBox.Text = Program.Util.P4ConInfo.Port;
                            if (Program.Util.P4ConInfo.User != null) ConnectionUserTextBox.Text = Program.Util.P4ConInfo.User;
                            if (Program.Util.P4ConInfo.Client != null) ConnectionWorkspaceTextBox.Text = Program.Util.P4ConInfo.Client;
                            bIgnoreConnectionUpdates = false;

                            Application.DoEvents();

							// Run through a series of tests to figure out what kind of connection we have and display appropriate messages.
							if (Program.Util.P4ConInfo.P4IsConnectionValid(true, true))
							{
								if (IsWorkspaceValid())
								{
									MainDialogNextButton.Enabled = true;
								}
                                ConnectionUserBrowseButton.Enabled = true;
                                ConnectionUserNewButton.Enabled = true;
								ConnectionWorkspaceBrowseButton.Enabled = true;
								ConnectionWorkspaceNewButton.Enabled = true;
							}
							else if (Program.Util.P4ConInfo.P4IsConnectionValid(true, false))
							{
								if (ConnectionWorkspaceTextBox.Text != string.Empty)
								{
									// The workspace/client is not recognized by this connection
									ConnectionWorkspaceErrLabel.Visible = true;
                                    ConnectionErrorDescription.Text = Program.Util.GetPhrase("ERRConnectWorkspace");
                                    ConnectionErrorDescription.Visible = true;
								}
                                ConnectionUserBrowseButton.Enabled = true;
                                ConnectionUserNewButton.Enabled = true;
								ConnectionWorkspaceBrowseButton.Enabled = true;
								ConnectionWorkspaceNewButton.Enabled = true;
							}
							else if (Program.Util.P4ConInfo.P4IsConnectionValid(false, false))
							{
                                ConnectionUserBrowseButton.Enabled = true;
                                ConnectionUserNewButton.Enabled = true;
								bool UserExists = (!string.IsNullOrEmpty(ConnectionUserTextBox.Text) && Program.Util.UserExists(ConnectionUserTextBox.Text));
								if (!UserExists)
								{
									ConnectionUserErrLabel.Visible = true;
                                    ConnectionErrorDescription.Text = Program.Util.GetPhrase("ERRUser");
                                    ConnectionErrorDescription.Visible = true;
								}
								else
								{
                                    // The pass is invalid so we will display the error and clear any default text that might be in there
                                    ClearPassDefaults();
									ConnectionPassErrLabel.Visible = true;
                                    ConnectionErrorDescription.Text = Program.Util.GetPhrase("ERRPass");
                                    ConnectionErrorDescription.Visible = true;
								}
							}
							else
							{
								if (ConnectionServerTextBox.Text != string.Empty)
								{
									ConnectionServerErrLabel.Visible = true;
                                    ConnectionErrorDescription.Text = Program.Util.GetPhrase("ERRServer");
                                    ConnectionErrorDescription.Visible = true;
								}
							}
						}

						Cursor.Current = CurRestore;

						break;
					}
				case "P4OperationPreviewPage":
					{
						// Update buttons
						SetButtonDefaults();

						// Update contents
						TitleLabel.Text = Program.Util.GetPhrase("TitlePreview");

						PreviewServerText.Text = Program.Util.P4ConInfo.Port;
						PreviewUserText.Text = Program.Util.P4ConInfo.User;
						PreviewWorkspaceText.Text = Program.Util.P4ConInfo.Client;

						PreviewNumFilesText.Text = Program.Util.ManifestInfo.FileCount().ToString();
						PreviewNumFoldersText.Text = Program.Util.ManifestInfo.FolderCount().ToString();
						PreviewSizeFilesText.Text = Program.Util.FormatBytes(Program.Util.TotalSize(Program.Util.ManifestInfo));

						PreviewPerforceTargetText.Text = Program.Util.GetClientFolderMapping();

						break;
					}
				case "CompletePage":
					{
						// Update buttons
						SetButtonDefaults();
						MainDialogNextButton.Visible = false;
						MainDialogBackButton.Visible = false;

						MainDialogCancelButton.Text = Program.Util.GetPhrase("GQFinished");

						Utils.GameManifestOptions Game = Program.Util.UDKSettings.GameInfo[0];
						CompleteRadioButton0.Text = Program.Util.GetPhrase("IFLaunch") + Game.Name;
						CompleteRadioButton1.Text = Program.Util.GetPhrase("R2D");

						// Update contents
						TitleLabel.Text = Program.Util.GetPhrase("TitleComplete");

						CompleteRadioButton0.Checked = true;

						break;
					}
			}
		}

		private void MainDialogNextButton_Click(object sender, EventArgs e)
		{
			switch (MainDialogTabControl.SelectedTab.Name)
			{
				case "P4ConnectionPage":
					{
						break;
					}

				case "P4OperationPreviewPage":
					{
						ShowProgress = true;
						P4BackgroundWorker.RunWorkerAsync();

						this.Enabled = false;
						Application.DoEvents();

						bool bIsSubmitSuccessful = Program.Util.SubmitFiles();
						ShowProgress = false;

						// Close the progress bar
						this.Enabled = true;

						if (bIsSubmitSuccessful)
						{
							Program.Util.EnableEditorSourceControlSupport();
						}
						else
						{
							if (!Program.Util.P4ConInfo.P4IsConnectionValid(true, true))
							{
								// The connection is no longer valid, go back a screen so the user can re-establish connection.
								if (MainDialogTabControl.SelectedIndex > 0)
								{
									MainDialogTabControl.SelectedIndex -= 1;
								}
								return;
							}
							else
							{
								// We won't progress to the next screen if the operation fails.  Hitting next again will retry
								return;
							}
						}
						break;
					}

				default:
					break;
			}

			if( MainDialogTabControl.SelectedIndex + 1 < MainDialogTabControl.TabCount )
			{
				MainDialogTabControl.SelectedIndex += 1;
			}
		}

		private void MainDialogBackButton_Click(object sender, EventArgs e)
		{
			if(MainDialogTabControl.SelectedIndex > 0 )
			{
				MainDialogTabControl.SelectedIndex -= 1;
			}
		}

		private void MainDialogTabControl_SelectedIndexChanged(object sender, EventArgs e)
		{
			UpdatePage();
		}

		private void ConnectionWorkspaceBrowseButton_Click(object sender, EventArgs e)
		{
			if (Program.Util.P4ConInfo.P4IsConnectionValid(true, false))
			{
				BrowseWorkspaceDialog BW = new BrowseWorkspaceDialog();
				BW.StartPosition = FormStartPosition.CenterParent;
				DialogResult DlgResult = BW.ShowDialog(this);

				if (DlgResult == DialogResult.OK)
				{
					if (ConnectionWorkspaceTextBox.Text != BW.SelectedWorkspace)
					{
						SetErrorDefaults();
						MainDialogNextButton.Enabled = false;
						bIgnoreConnectionUpdates = true;
						ConnectionWorkspaceTextBox.Text = BW.SelectedWorkspace;
						ConnectionWorkspaceRootTextBox.Text = string.Empty;
						bIgnoreConnectionUpdates = false;
						UpdatePage();
					}
				}
			}
		}

        private void ConnectionUserBrowseButton_Click(object sender, EventArgs e)
        {
			if (Program.Util.P4ConInfo.P4IsConnectionValid(false, false))
			{
                BrowseUserDialog BU = new BrowseUserDialog();
                BU.StartPosition = FormStartPosition.CenterParent;
                DialogResult DlgResult = BU.ShowDialog(this);

                if (DlgResult == DialogResult.OK)
                {
                    if (ConnectionUserTextBox.Text != BU.SelectedUser)
                    {
                        SetErrorDefaults();
                        MainDialogNextButton.Enabled = false;
                        bIgnoreConnectionUpdates = true;
                        ConnectionUserTextBox.Text = BU.SelectedUser;
                        bIgnoreConnectionUpdates = false;
                        UpdatePage();
                    }
                }
            }
        }

		private void ConnectionWorkspaceNewButton_Click(object sender, EventArgs e)
		{
			if (Program.Util.P4ConInfo.P4IsConnectionValid(true, false))
			{
				NewWorkspaceDialog NW = new NewWorkspaceDialog();
				NW.StartPosition = FormStartPosition.CenterParent;
				DialogResult DlgResult = NW.ShowDialog(this);
				if (DlgResult == DialogResult.OK)
				{
					if (ConnectionWorkspaceTextBox.Text != NW.SelectedWorkspace)
					{
						SetErrorDefaults();
						MainDialogNextButton.Enabled = false;
						bIgnoreConnectionUpdates = true;
						ConnectionWorkspaceTextBox.Text = NW.SelectedWorkspace;
						ConnectionWorkspaceRootTextBox.Text = string.Empty;
						bIgnoreConnectionUpdates = false;
						UpdatePage();
					}
				}
			}
		}

        private void ConnectionUserNewButton_Click(object sender, EventArgs e)
        {
            NewUserDialog NU = new NewUserDialog();
            NU.StartPosition = FormStartPosition.CenterParent;
            DialogResult DlgResult = NU.ShowDialog(this);
            if (DlgResult == DialogResult.OK)
            {
                if (ConnectionUserTextBox.Text != NU.SelectedUser)
                {
                    SetErrorDefaults();
                    MainDialogNextButton.Enabled = false;
                    bIgnoreConnectionUpdates = true;
                    ConnectionUserTextBox.Text = NU.SelectedUser;
                    bIgnoreConnectionUpdates = false;
                    UpdatePage();
                }
            }
        }

		private void ConnectionServerTextBox_TextChanged(object sender, EventArgs e)
		{
			if (bIgnoreConnectionUpdates) return;
			SetErrorDefaults();
			MainDialogNextButton.Enabled = false;
            ConnectionUserBrowseButton.Enabled = false;
            ConnectionUserNewButton.Enabled = false;
			ConnectionWorkspaceBrowseButton.Enabled = false;
			ConnectionWorkspaceNewButton.Enabled = false;
			UpdateTimer.Stop();
			UpdateTimer.Start();
		}

		private void ConnectionUserTextBox_TextChanged(object sender, EventArgs e)
		{
			if (bIgnoreConnectionUpdates) return;
			SetErrorDefaults();
			MainDialogNextButton.Enabled = false;
            ConnectionUserBrowseButton.Enabled = false;
            ConnectionUserNewButton.Enabled = false;
			ConnectionWorkspaceBrowseButton.Enabled = false;
			ConnectionWorkspaceNewButton.Enabled = false;
			UpdateTimer.Stop();
			UpdateTimer.Start();
		}

        private void ConnectionPassTextBox_TextChanged(object sender, EventArgs e)
        {
            if (bIgnoreConnectionUpdates) return;
            SetErrorDefaults();
            MainDialogNextButton.Enabled = false;
            ConnectionUserBrowseButton.Enabled = false;
            ConnectionUserNewButton.Enabled = false;
            ConnectionWorkspaceBrowseButton.Enabled = false;
            ConnectionWorkspaceNewButton.Enabled = false;
            UpdateTimer.Stop();
            UpdateTimer.Start();
        }

		private void ConnectionWorkspaceTextBox_TextChanged(object sender, EventArgs e)
		{
			if (bIgnoreConnectionUpdates) return;
			SetErrorDefaults();
			MainDialogNextButton.Enabled = false;
			ConnectionWorkspaceRootTextBox.Text = string.Empty;
			UpdateTimer.Stop();
			UpdateTimer.Start();
		}

		private void P4BackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
		{
			Program.Util.CreateProgressBar(Program.Util.GetPhrase("PROGRESSHeading"), Program.Util.GetPhrase("PROGRESSHeading"), string.Empty, string.Empty);

			while (ShowProgress == true)
			{
				Thread.Sleep(50);
				Application.DoEvents();
			}

			Program.Util.DestroyProgressBar();
		}

		private void MainDialogCancelButton_MouseClick(object sender, MouseEventArgs e)
		{
			switch (MainDialogTabControl.SelectedTab.Name)
			{
				case "CompletePage":
					{
						if (CompleteRadioButton0.Checked)
						{
							// Launch the editor
							Utils.GameManifestOptions Game = Program.Util.UDKSettings.GameInfo[0];
							Utils.Launch(Game.AppToElevate, Utils.GetProjectRoot(), Game.AppCommandLine);
						}
						this.Close();
						return;
					}

				default:
					break;
			}
		}

        private void ConnectionServerHelpButton_Click(object sender, EventArgs e)
        {
            string MBText = Program.Util.GetPhrase("CONServerInfo");
            string MBCap = Program.Util.GetPhrase("CONServerInfoHeading");
            MessageBox.Show(new Form() { TopMost = true }, MBText, MBCap, MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void MainDialog_Shown(object sender, EventArgs e)
        {
            // Ensure that this dialog does not hide under other applications when started.
            this.TopMost = true;
            this.Focus();
            this.BringToFront();
            this.TopMost = false;
        }

        private void ConnectionPassTextBox_Enter(object sender, EventArgs e)
        {
            ClearPassDefaults();
        }

	}
}
