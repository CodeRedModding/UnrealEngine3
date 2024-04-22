// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Deployment.Application;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;

namespace Builder.UnrealSync
{
	public partial class LogViewer : Form
	{
		private UnrealControls.OutputWindowDocument OutputDocument = new UnrealControls.OutputWindowDocument();
		private UnrealSync2 Main = null;
		private string BaseDirectory = "";

		public LogViewer( UnrealSync2 InMain )
		{
			Main = InMain;

			InitializeComponent();

			BaseDirectory = Application.StartupPath;
			if( ApplicationDeployment.IsNetworkDeployed )
			{
				BaseDirectory = ApplicationDeployment.CurrentDeployment.DataDirectory;
			}

			OutputDocument.Clear();
			LogViewWindow.Document = OutputDocument;

			PopulateListView();
		}

		private void ProcessLog( string LogFileText )
		{
			string[] Lines = LogFileText.Split( Environment.NewLine.ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
			List<string> Errors = new List<string>();

			foreach( string Line in Lines )
			{
				if( Line.Contains( "ERROR: " ) )
				{
					// Collect all errors
					Errors.Add( Line );
				}
				else if( Line.Contains( "Total errors: 0" ) )
				{
					// ... but clear out if everything ended up hunky dorey
					Errors.Clear();
				}
			}

			if( Errors.Count > 0 )
			{
				OutputDocument.AppendText( Color.Red, "Sync did NOT complete successfully!" + Environment.NewLine );
				OutputDocument.AppendText( Color.Red, Environment.NewLine );
				OutputDocument.AppendText( Color.Red, "The following errors were found:" + Environment.NewLine );
				foreach( string Error in Errors )
				{
					OutputDocument.AppendText( Color.Red, Error + Environment.NewLine );
				}
			}
			else
			{
				OutputDocument.AppendText( Color.Green, "Sync completed successfully!" + Environment.NewLine );
			}

			OutputDocument.AppendText( Color.Black, Environment.NewLine );
		}

		private void ShowLogFile( string LogFileName )
		{
			FileInfo Info = new FileInfo( LogFileName );
			if( Info.Exists )
			{
				OutputDocument.Clear();
				try
				{
					string AllText = File.ReadAllText( LogFileName );

					// Write out a summary of the log at the top of the window
					ProcessLog( AllText );

					// Add the vanilla log to the end
					OutputDocument.AppendText( Color.Black, AllText );
				}
				catch
				{
				}
			}
		}

		private void PopulateListView()
		{
			List<string> Files = new List<string>();

			DirectoryInfo DirInfo = new DirectoryInfo( BaseDirectory );
			foreach( FileInfo Info in DirInfo.GetFiles( "[*.txt" ) )
			{
				if( Info.Length > 0 )
				{
					Files.Add( Info.Name );
				}
			}

			Files.Sort();
			Files.Reverse();

			LogSelectionListBox.Items.Clear();
			LogSelectionListBox.Items.AddRange( Files.ToArray() );

			if( Files.Count > 0 )
			{
				string LogFileName = Path.Combine( BaseDirectory, Files[0] );
				ShowLogFile( LogFileName );
			}
		}

		private void LogSelectionIndexChanged( object sender, EventArgs e )
		{
			string LogFileName = Path.Combine( BaseDirectory, LogSelectionListBox.Text );
			ShowLogFile( LogFileName );
		}

		private void LogViewerClosed( object sender, FormClosedEventArgs e )
		{
			Main.ViewLogs = null;
		}
	}
}
