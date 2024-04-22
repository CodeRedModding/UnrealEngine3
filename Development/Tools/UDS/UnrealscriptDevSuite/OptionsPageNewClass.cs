using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Text;
using System.IO;
using System.Windows.Forms;

namespace UnrealscriptDevSuite
{
	public partial class OptionsPageNewClass : UserControl, EnvDTE.IDTToolsOptionsPage
	{
		static OptionPageProperties _propertiesObject = new OptionPageProperties();
		UDSSettings Settings;

	
		public OptionsPageNewClass()
		{
			InitializeComponent();
			Settings = new UDSSettings();
		}

		#region IDTToolsOptionsPage Members

		public void GetProperties( ref object PropertiesObject )
		{
			//Return an object which is accessed by the method call DTE.Properties(category, subcategory)
			PropertiesObject = _propertiesObject;
		}

		public void OnAfterCreated( EnvDTE.DTE DTEObject )
		{
			tbNewClassHeader.Text = Settings.NewClassHeader;
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
			if (tbNewClassHeader.Modified)
			{
				Settings.NewClassHeader = tbNewClassHeader.Text;
			}
		}

		#endregion

		private void btnImport_Click( object sender, EventArgs e )
		{
			System.Windows.Forms.OpenFileDialog dlgOpenFile = new System.Windows.Forms.OpenFileDialog();
			dlgOpenFile.DefaultExt = ".txt";
			if ( dlgOpenFile.ShowDialog() == DialogResult.OK )
			{
				tbNewClassHeader.Text = File.ReadAllText(dlgOpenFile.FileName);
				tbNewClassHeader.Modified = true;
			}
		}

	}

    [System.Runtime.InteropServices.ComVisible(true)]
    [System.Runtime.InteropServices.ClassInterface(System.Runtime.InteropServices.ClassInterfaceType.AutoDual)]
    public class OptionPageNewClassProperties
    {
		private UDSSettings Settings;

		public string CompileExeFilename
		{
			get
			{
				return Settings.NewClassHeader;
			}
			set
			{
				Settings.NewClassHeader = value;
			}
		}
		public OptionPageNewClassProperties()
		{
			Settings = new UDSSettings();
		}
	}
}
