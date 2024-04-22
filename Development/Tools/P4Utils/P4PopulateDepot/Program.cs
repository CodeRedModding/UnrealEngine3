// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace P4PopulateDepot
{
	static class Program
	{

		static public Utils Util = null;

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main()
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);

			string TLLN = System.Threading.Thread.CurrentThread.CurrentUICulture.ThreeLetterWindowsLanguageName;
			string ParentTLLN = System.Threading.Thread.CurrentThread.CurrentUICulture.Parent.ThreeLetterWindowsLanguageName;
			Util = new Utils(TLLN, ParentTLLN);

			string ManifestFilePath = Path.Combine(Utils.GetProjectRoot(), "Binaries\\InstallData\\Manifest.xml");
			string ManifestOptionsPath = Path.Combine(Utils.GetProjectRoot(), "Binaries\\UnSetup.Manifests.xml");

			if (File.Exists(ManifestFilePath))
			{
				Program.Util.LoadManifestInfo(ManifestFilePath);
			}
			else
			{
				string MBText = Program.Util.GetPhrase("ERRMissingFileMSG") + ManifestFilePath;
				string MBCap = Program.Util.GetPhrase("ERRMissingFile");
				MessageBox.Show(MBText, MBCap, MessageBoxButtons.OK, MessageBoxIcon.Error);
				return;
			}

			if (File.Exists(ManifestOptionsPath))
			{
				Program.Util.LoadManifestOptions(ManifestOptionsPath);

			}
			else
			{
				string MBText = Program.Util.GetPhrase("ERRMissingFileMSG") + ManifestOptionsPath;
				string MBCap = Program.Util.GetPhrase("ERRMissingFile");
				MessageBox.Show(MBText, MBCap, MessageBoxButtons.OK, MessageBoxIcon.Error);
				return;
			}

			MainDialog MainDlg = new MainDialog();
			DialogResult MainDlgResult = MainDlg.ShowDialog();

		}
	}
}
