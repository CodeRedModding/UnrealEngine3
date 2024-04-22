// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace CodeSizeProfiler
{
    public partial class MainWindow : Form
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        private void OpenButton_Click(object sender, EventArgs e)
        {
			bool bShouldPerformWork = true;

			if( RequiresUndecorationCheckbox.Checked )
			{
				if( MessageBox.Show(
					"Redirecting output in C# seems to be slow. You're much better of manually running undname on the map " +
					"file to create an undecorated version.\n\n" +
					"Undname can be found at C:\\Program Files (x86)\\Microsoft Visual Studio 9.0\\VC\\bin\\undname.exe\n\n" +
					"Press Yes to continue, No to cancel.", "Confirm slow operation", MessageBoxButtons.YesNo) == DialogResult.No )
				{
					bShouldPerformWork = false;
				}
			}

			OpenFileDialog OpenMapFileDialog = new OpenFileDialog();
			if( bShouldPerformWork )
			{			
				OpenMapFileDialog.Title = "Open the map file";
				OpenMapFileDialog.Filter = "Map data (*.map)|*.map";
				OpenMapFileDialog.RestoreDirectory = false;
				OpenMapFileDialog.SupportMultiDottedExtensions = true;
				bShouldPerformWork = OpenMapFileDialog.ShowDialog() == DialogResult.OK;
			}

			if( bShouldPerformWork )
			{
				var FunctionList = MapFileParser.ParseFileIntoFunctionList( OpenMapFileDialog.FileName, RequiresUndecorationCheckbox.Checked );
				MapFileParser.ParseFunctionListIntoTreeView( FunctionList, FunctionTreeView );
			}
        }
    }
}
