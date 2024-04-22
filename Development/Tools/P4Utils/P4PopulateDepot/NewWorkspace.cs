// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace P4PopulateDepot
{
	public partial class NewWorkspaceDialog : Form
	{
		private bool bIsErrorWorskpaceName = false;
		private bool bIsErrorWorkspaceRoot = false;
		private string ErrorMsgWorkspaceName = "";
		private string ErrorMsgWorkspaceRoot = "";

		private bool bDisableTxtChangeEventHandlers = false;

		public string SelectedWorkspace
		{
			get
			{
				return NWWorkspaceNameTextBox.Text;
			}
		}

		public NewWorkspaceDialog()
		{
			InitializeComponent();

			Text = Program.Util.GetPhrase("NWNewWorkspace");

			WSNewOKButton.Text = Program.Util.GetPhrase("GQOK");
			WSNewCancelButton.Text = Program.Util.GetPhrase("GQCancel");

			NWWorkspaceNameErrLabel.Visible = false;
			NWWorkspaceRootErrLabel.Visible = false;
			NWMainErrorDescription.Visible = false;

			// Start the user off one level above the root directory if it is not the root of the drive.
			DirectoryInfo StartDir = new DirectoryInfo(Utils.GetProjectRoot());
			// If the project root has a parent who is not the root of the drive, we will use that for an initial value.
			if (StartDir.Parent != null && StartDir.Parent.Parent != null)
			{
				StartDir = StartDir.Parent;
			}

			bDisableTxtChangeEventHandlers = true;
			NWWorkspaceRootTextBox.Text = StartDir.FullName;
			bDisableTxtChangeEventHandlers = false;
		}

		/// <summary>
		/// Runs through a number of tests to see if there are any errors detected with the user's input values.  If so, sets the appropriate error values.
		/// </summary>
		/// <returns></returns>
		private bool AreErrorsDetected()
		{
			bIsErrorWorskpaceName = false;
			bIsErrorWorkspaceRoot = false;
			ErrorMsgWorkspaceName = "";
			ErrorMsgWorkspaceRoot = "";

			//@todo Consider doing some additional validation checks for illegal characters, etc.
			if(NWWorkspaceNameTextBox.Text == string.Empty)
			{
				bIsErrorWorskpaceName = true;
				ErrorMsgWorkspaceName = Program.Util.GetPhrase("ERRWorkspaceEmpty");
			}
			
			if(Program.Util.WorkspaceExists(NWWorkspaceNameTextBox.Text))
			{
				bIsErrorWorskpaceName = true;
				ErrorMsgWorkspaceName = Program.Util.GetPhrase("ERRWorkspaceExists");
			}
			
			if(NWWorkspaceRootTextBox.Text == string.Empty ||
				!Program.Util.IsSubDirectory(new DirectoryInfo(Utils.GetProjectRoot()), new DirectoryInfo(NWWorkspaceRootTextBox.Text)))
			{
				bIsErrorWorkspaceRoot = true;
				ErrorMsgWorkspaceRoot = Program.Util.GetPhrase("ERRWorkspaceBadRoot");
			}

			return (bIsErrorWorkspaceRoot || bIsErrorWorskpaceName);
		}


		/// <summary>
		/// Used to update the UI Error widgets.
		/// </summary>
		private void UpdateErrorUI()
		{
			NWWorkspaceNameErrLabel.Visible = false;
			NWWorkspaceRootErrLabel.Visible = false;
			NWMainErrorDescription.Visible = false;


			if(bIsErrorWorkspaceRoot)
			{
				NWWorkspaceRootErrLabel.Visible = true;
				NWMainErrorDescription.Visible = true;
				NWMainErrorDescription.Text = ErrorMsgWorkspaceRoot;
			}

			if(bIsErrorWorskpaceName)
			{
				NWWorkspaceNameErrLabel.Visible = true;
				NWMainErrorDescription.Visible = true;
				NWWorkspaceRootErrLabel.Visible = false;
				NWMainErrorDescription.Text = ErrorMsgWorkspaceName;
			}
		}


		private void WSNewOKButton_Click(object sender, EventArgs e)
		{
			bool bAreErrorsDetected = AreErrorsDetected();

			UpdateErrorUI();

			if(!bAreErrorsDetected)
			{
				if(Program.Util.CreateNewWorkspace(NWWorkspaceNameTextBox.Text,NWWorkspaceRootTextBox.Text))
				{
					DialogResult = DialogResult.OK;
					Close();
				}
				else
				{
					bIsErrorWorskpaceName = true;
                    ErrorMsgWorkspaceName = Program.Util.GetPhrase("NWCreateFail");
					UpdateErrorUI();
				}
			}
		}


		private void NWWorkspaceRootTextBox_TextChanged(object sender, EventArgs e)
		{
			if (bDisableTxtChangeEventHandlers)
			{
				return;
			}

			if(bIsErrorWorkspaceRoot == true)
			{
				bIsErrorWorkspaceRoot = false;
			}				
			AreErrorsDetected();
			UpdateErrorUI();
		}

		private void NWWorkspaceNameTextBox_TextChanged(object sender, EventArgs e)
		{
			if (bDisableTxtChangeEventHandlers)
			{
				return;
			}

			if(bIsErrorWorskpaceName == true)
			{
				bIsErrorWorskpaceName = false;
				UpdateErrorUI();
			}
		}

		private void ConnectionWorkspaceBrowseButton_Click(object sender, EventArgs e)
		{
			FolderBrowserDialog folderDlg = new FolderBrowserDialog();

			DirectoryInfo CurDir = new DirectoryInfo(Utils.GetProjectRoot());

			// If the project root has a parent who is not the root of the drive, we will use that for an initial value.
			folderDlg.SelectedPath = (CurDir.Parent != null && CurDir.Parent.Parent != null) ? CurDir.Parent.FullName : CurDir.FullName;

			DialogResult result = folderDlg.ShowDialog();
			if(result == DialogResult.OK)
			{
				NWWorkspaceRootTextBox.Text = folderDlg.SelectedPath;
			}
		}
	}

}
