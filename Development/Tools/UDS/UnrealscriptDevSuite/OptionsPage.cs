//Copyright (c) Microsoft Corporation.  All rights reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Text;
using System.Windows.Forms;

namespace UnrealscriptDevSuite
{
    public partial class OptionsPage : UserControl, EnvDTE.IDTToolsOptionsPage
    {
        static OptionPageProperties _propertiesObject = new OptionPageProperties();
		UDSSettings Settings;

        public OptionsPage()
        {
            InitializeComponent();
			Settings = new UDSSettings();
        }

        #region IDTToolsOptionsPage Members

        public void GetProperties(ref object PropertiesObject)
        {
            //Return an object which is accessed by the method call DTE.Properties(category, subcategory)
            PropertiesObject = _propertiesObject;
        }

        public void OnAfterCreated(EnvDTE.DTE DTEObject)
        {
			cbForceCompileExe.Checked = Settings.bForceCompileEXE;
			tbCompileEXE.Text = Settings.CompileExeFilename;
			ckAppendDebug.Checked = Settings.bAppendDebug;
			tbAdditionalCmds.Text = Settings.AdditionalCompileCmdLine;
			ckUseCOMFile.Checked = Settings.bUseCOMFile;
			ckClearOutputWindow.Checked = Settings.bClearOutputWindow;
        }

        public void OnCancel()
        {
        }

        public void OnHelp()
        {
            System.Windows.Forms.MessageBox.Show("TODO: Display Help");
        }

        public void OnOK()
        {
			Settings.bForceCompileEXE = cbForceCompileExe.Checked;
			Settings.CompileExeFilename = tbCompileEXE.Text;
			Settings.bAppendDebug = ckAppendDebug.Checked;
			Settings.AdditionalCompileCmdLine = tbAdditionalCmds.Text;
			Settings.bUseCOMFile = ckUseCOMFile.Checked;
			Settings.bClearOutputWindow = ckClearOutputWindow.Checked;
        }

        #endregion

		private void cbForceCompileExe_CheckedChanged( object sender, EventArgs e )
		{
			pnExe.Enabled = cbForceCompileExe.Checked;
		}

		private void btnFindCompileEXE_Click( object sender, EventArgs e )
		{
			System.Windows.Forms.OpenFileDialog dlgOpenFile = new System.Windows.Forms.OpenFileDialog();
			dlgOpenFile.DefaultExt = ".exe";
			if (dlgOpenFile.ShowDialog() == DialogResult.OK)
			{
				tbCompileEXE.Text = dlgOpenFile.FileName;
			}
		}
    }

    [System.Runtime.InteropServices.ComVisible(true)]
    [System.Runtime.InteropServices.ClassInterface(System.Runtime.InteropServices.ClassInterfaceType.AutoDual)]
    public class OptionPageProperties
    {
		private UDSSettings Settings;

		public bool bForceCompileEXE
		{
			get
			{
				return Settings.bForceCompileEXE;
			}
			set
			{
				Settings.bForceCompileEXE = value;
			}
		}

		public string CompileExeFilename
		{
			get
			{
				return Settings.CompileExeFilename;
			}
			set
			{
				Settings.CompileExeFilename = value;
			}
		}

		public bool bAppendDebug
		{
			get
			{
				return Settings.bAppendDebug;
			}
			set
			{
				Settings.bAppendDebug = value;
			}
		}

		public string AdditionalCompileCmdLine
		{
			get
			{
				return Settings.AdditionalCompileCmdLine;
			}
			set
			{
				Settings.AdditionalCompileCmdLine = value;
			}
		}

		public bool bUseCOMFile
		{
			get
			{
				return Settings.bUseCOMFile;
			}
			set
			{
				Settings.bUseCOMFile = value;
			}
		}

		public bool bClearOutputWindow
		{
			get
			{
				return Settings.bClearOutputWindow;
			}
			set
			{
				Settings.bClearOutputWindow = value;
			}
		}

		public OptionPageProperties()
		{
			Settings = new UDSSettings();
		}
	}
}

//Property can be accessed through the object model with code such as the following macro:
//    Sub ToolsOptionsPageProperties()
//        MsgBox(DTE.Properties("My Category", "My Subcategory - Visual C#").Item("MyProperty").Value)
//        DTE.Properties("My Category", "My Subcategory - Visual C#").Item("MyProperty").Value = False
//        MsgBox(DTE.Properties("My Category", "My Subcategory - Visual C#").Item("MyProperty").Value)
//    End Sub