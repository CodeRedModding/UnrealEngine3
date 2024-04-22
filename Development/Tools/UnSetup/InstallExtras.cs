/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;

namespace UnSetup
{
	public partial class InstallExtras : Form
	{
		private string InstallFolder = string.Empty;

		public InstallExtras(string DestFolder)
		{
			InstallFolder = DestFolder;

			InitializeComponent();

			Text = Program.Util.GetPhrase("IEInstallExtras");
			InstallExtrasTitleLabel.Text = Text;
			InstallExtrasOKButton.Text = Program.Util.GetPhrase("IENext");
			InstallP4Server.Text = Program.Util.GetPhrase("IEServerInstall");
			InstallP4Client.Text = Program.Util.GetPhrase("IEClientInstall");
			p4DescriptionTextBox.Text = Program.Util.GetPhrase("IEPerforceDescription");

			InstallOptionsFooterLineLabel.Text = Program.Util.UnSetupVersionString;

		}

		private void InstallExtrasOKClicked( object sender, EventArgs e )
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		private void OnLoad( object sender, EventArgs e )
		{
			Utils.CenterFormToPrimaryMonitor( this );
		}

		private void InstallP4Server_Click(object sender, EventArgs e)
		{
			string InstallerPath = Program.Util.GetP4InstallerPath(InstallFolder, "Server");
			if (!File.Exists(InstallerPath))
			{
				DialogResult result = MessageBox.Show(Program.Util.GetPhrase("IEMissingInstaller") + InstallerPath,
					"Missing Installer",
					MessageBoxButtons.OK,
					MessageBoxIcon.Error);
				return;
			}

			//General flow of the function below
			//	If we don't find local p4d version info and we don't have connection info to the server we assume it is not installed
			//		It is assumed the user will want to launch the installer.
			//	If we do have p4d version info
			//		If the server is local
			//			If the server needs an upgrade
			//				Prompt with info on upgrade and ask if the user really wants to launch the p4d installer now.
			//			If server does not need upgrade
			//				Prompt saying they have p4d installed already.  Ask if they really want to launch the installer anyway.
			//		If the server is remote
			//			If the server needs upgrade
			//				Prompt the user telling them we have detected a connection to an older server running remotely and tell them we can not upgrade from here but point them to instructions.  Ask them if they want to launch installer anyway.
			//			If the server does not need upgrade
			//				Prompt the user telling them we have detected a connection to an up to date server running remotely.  Ask them if they want to launch the installer anyway.
			//	Prior to launching the installer locally, we always prompt to give user info about this and point them to a guide that will help them with choosing an appropriate location.

			bool bLaunchInstaller = false;
			bool bShowLocalInstallInfo = false;
			bool bIsServerLocal = false;

			// Obtain the locally installed p4d version or the active connection.
			string ServerVersion = Program.Util.GetP4ServerVersion();

			// Check for existing server connection info and retrieve it, this may not be the same info we have for the locally installed p4d in the ServerVersion variable.
			string ConnectionServerVersion;
			string ConnectionServerAddress;
			string ConnectionServerRoot;
			Program.Util.GetP4ServerInfo(out ConnectionServerVersion, out ConnectionServerAddress, out ConnectionServerRoot);
			
			if(ConnectionServerVersion == String.Empty && ServerVersion == String.Empty)
			{
				// Could not obtain info about a p4 server, so we assume there is none and proceed with install
				bShowLocalInstallInfo = true;
				bLaunchInstaller = true;
			}
			else if (ConnectionServerVersion == string.Empty && ServerVersion != String.Empty)
			{
				// If the P4 Info server version comes back empty but we obtain version info via GetP4ServerVersion, we know there is no
				//  active perforce connection setup but we do have p4d installed.  In this case we assume there is a local server
				bIsServerLocal = true;
			}
			else if (ConnectionServerVersion != string.Empty)
			{
				// If the server version number comes back non-empty, we know there is a connection setup

				// Check to see if the server is local
				if( ConnectionServerAddress != string.Empty && Program.Util.IsLocalAddress(ConnectionServerAddress))
				{
					// The ip address points to us so now we check to see if the server root path exists
					if( ConnectionServerRoot != string.Empty && Directory.Exists(ConnectionServerRoot))
					{
						bIsServerLocal = true;
					}
				}	
			}

			if(!bLaunchInstaller)
			{
				// Check to see if the version is older than the installer provided
				bool bServerNeedsUpgrade = Program.Util.DoesP4ServerNeedUpgrade(InstallFolder);

				if (bIsServerLocal )
				{
					if (!bServerNeedsUpgrade)
					{
						// User is on the latest version of the installer.  We will inform them and see if they still want to launch the installer.
						GenericQuery YesNoDlg = new GenericQuery("IEP4ServerUpToDateCaption", "IEP4ServerUpToDateLocalMessage", true, "GQNo", true, "GQYes");
						YesNoDlg.OverrideDescriptionAlignment(HorizontalAlignment.Left);
						DialogResult DlgResult = YesNoDlg.ShowDialog();
						if (DlgResult == DialogResult.OK)
						{
							bLaunchInstaller = true;
						}
					}
					else
					{
						// The local server is an older version.  We will show a message with some upgrade info and launch the installer if requested.
						GenericQuery YesNoDlg = new GenericQuery("IEP4ServerOutOfDateCaption", "IEP4ServerOutOfDateLocalMessage", true, "GQNo", true, "GQYes");
						YesNoDlg.OverrideDescriptionAlignment(HorizontalAlignment.Left);
						DialogResult DlgResult = YesNoDlg.ShowDialog();
						if (DlgResult == DialogResult.OK)
						{
							bLaunchInstaller = true;
						}
					}
				}
				else
				{
					// Server is not local.  

					if (!bServerNeedsUpgrade)
					{
						GenericQuery YesNoDlg = new GenericQuery("IEp4ServerUpToDateCaption", "IEP4ServerUpToDateRemoteMessage", true, "GQNo", true, "GQYes");
						YesNoDlg.OverrideDescriptionAlignment(HorizontalAlignment.Left);
						DialogResult DlgResult = YesNoDlg.ShowDialog();
						if (DlgResult == DialogResult.OK)
						{
							bShowLocalInstallInfo = true;
							bLaunchInstaller = true;
						}
					}
					else
					{
						GenericQuery YesNoDlg = new GenericQuery("IEP4ServerOutOfDateCaption", "IEP4ServerOutOfDateRemoteMessage", true, "GQNo", true, "GQYes");
						YesNoDlg.OverrideDescriptionAlignment(HorizontalAlignment.Left);
						DialogResult DlgResult = YesNoDlg.ShowDialog();
						if (DlgResult == DialogResult.OK)
						{
							bShowLocalInstallInfo = true;
							bLaunchInstaller = true;
						}

					}
				}
			}


			if( bShowLocalInstallInfo && bLaunchInstaller )
			{
				GenericQuery YesNoDlg = new GenericQuery("IEP4ServerLocalConfirmCaption", "IEP4ServerLocalConfirmMessage", true, "GQNo", true, "GQYes");
				YesNoDlg.OverrideDescriptionAlignment(HorizontalAlignment.Left);
				DialogResult DlgResult = YesNoDlg.ShowDialog();
				if (DlgResult == DialogResult.Cancel)
				{
					bLaunchInstaller = false;
				}
			}

			if (bLaunchInstaller)
			{
				try
				{
					ProcessStartInfo StartInfo = new ProcessStartInfo(InstallerPath);
					StartInfo.WorkingDirectory = Path.GetDirectoryName(InstallerPath);
					Process.Start(StartInfo);
				}
				catch
				{
				}
			}
		}

		private void InstallP4Client_Click(object sender, EventArgs e)
		{
			bool bLaunchInstaller = false;

			string InstallerPath = Program.Util.GetP4InstallerPath(InstallFolder, "Client");
			if (!File.Exists(InstallerPath))
			{
				DialogResult result = MessageBox.Show(Program.Util.GetPhrase("IEMissingInstaller") + InstallerPath,
					"Missing Installer",
					MessageBoxButtons.OK,
					MessageBoxIcon.Error);
				return;
			}

			// Try to obtain the client version number, if this returns the empty string we assume
			//  it is not installed.  If it is installed, we check to see if it needs an upgrade.
			if ( Program.Util.GetP4ClientVersion()== string.Empty ||
				Program.Util.DoesP4ClientNeedUpgrade(InstallFolder))
			{
				bLaunchInstaller = true;
			}

			// If the user already has the client installed at a version number that matches the provided installer, we will
			//  prompt them to see if they want to launch the installer anyways.
			if (bLaunchInstaller == false)
			{
				GenericQuery YesNoDlg = new GenericQuery("IEP4ClientUpToDateCaption", "IEP4ClientUpToDateMessage", true, "GQNo", true, "GQYes");
				YesNoDlg.OverrideDescriptionAlignment(HorizontalAlignment.Left);
				DialogResult DlgResult = YesNoDlg.ShowDialog();
				if (DlgResult == DialogResult.OK)
				{
					bLaunchInstaller = true;
				}
			}

			if (bLaunchInstaller)
			{
				try
				{
					ProcessStartInfo StartInfo = new ProcessStartInfo(InstallerPath);
					StartInfo.WorkingDirectory = Path.GetDirectoryName(InstallerPath);
					Process.Start(StartInfo);
				}
				catch
				{
				}
			}
		}

		private void P4ServerInfoTextbox_LinkClicked(object sender, LinkClickedEventArgs e)
		{
			Process.Start("rundll32.exe", "url.dll,FileProtocolHandler " + e.LinkText);
		}

		private void P4ClientTextbox_LinkClicked(object sender, LinkClickedEventArgs e)
		{
			Process.Start("rundll32.exe", "url.dll,FileProtocolHandler " + e.LinkText);
		}

		private void p4DescriptionTextBox_LinkClicked(object sender, LinkClickedEventArgs e)
		{
			Process.Start("rundll32.exe", "url.dll,FileProtocolHandler " + e.LinkText);
		}
	}
}
