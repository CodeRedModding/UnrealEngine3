// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace Builder.UnrealSync
{
	public partial class BranchConfigure : Form
	{
		private UnrealSync2 Main = null;

		public BranchConfigure( UnrealSync2 InMain )
		{
			Main = InMain;

			InitializeComponent();

			PopulateBranchDataGrid();
		}

		public void BranchConfigureOKButtonClicked( object sender, EventArgs e )
		{
			foreach( DataGridViewRow Row in BranchDataGridView.Rows )
			{
				BranchSpec Branch = ( BranchSpec )Row.Tag;
				Branch.bDisplayInMenu = ( bool )Row.Cells[0].Value;

				string SyncTime = ( string )Row.Cells[3].Value;
				Branch.SyncTime = Main.GetSyncTime( SyncTime );

				string SyncType = ( string )Row.Cells[4].Value;
				Branch.SyncType = ( ESyncType )Enum.Parse( typeof( ESyncType ), SyncType );
			}

			Main.ButtonOKClick( null, null );
			Close();
		}

		private void PopulateBranchDataGrid()
		{
			DataGridViewCheckBoxCell CheckBoxCell = null;
			DataGridViewTextBoxCell TextBoxCell = null;
			DataGridViewComboBoxCell ComboBoxCell = null;

			List<string> UniqueClientSpecs = Main.GetUniqueClientSpecs( true );
			List<DataGridViewCellStyle> CellStyles = Main.GetGridStyles( UniqueClientSpecs.Count );

			// Clear out the existing rows
			BranchDataGridView.Rows.Clear();

			// The appropriate entries are already added
			DataGridViewComboBoxColumn SyncTimeColumn = ( DataGridViewComboBoxColumn )BranchDataGridView.Columns["SyncTime"];

			// Create the datagrid view
			foreach( BranchSpec Branch in Main.BranchSpecs )
			{
				DataGridViewRow Row = new DataGridViewRow();

				Row.Tag = ( object )Branch;
				Row.DefaultCellStyle = CellStyles[UniqueClientSpecs.IndexOf( Branch.ClientSpec )];

				// Checkbox to display in the context menu
				CheckBoxCell = new DataGridViewCheckBoxCell();
				CheckBoxCell.Value = Branch.bDisplayInMenu;
				Row.Cells.Add( CheckBoxCell );

				// TextBox for the branch root
				TextBoxCell = new DataGridViewTextBoxCell();
				TextBoxCell.Value = Branch.ClientSpec + " (" + Branch.Root + ")";
				Row.Cells.Add( TextBoxCell );

				// TextBox for the branch name
				TextBoxCell = new DataGridViewTextBoxCell();
				TextBoxCell.Value = Branch.Name;
				Row.Cells.Add( TextBoxCell );

				// ComboBox for the time to sync
				ComboBoxCell = new DataGridViewComboBoxCell();
				ComboBoxCell.Items.AddRange( SyncTimeColumn.Items );
				if( Branch.SyncTime == DateTime.MaxValue )
				{
					ComboBoxCell.Value = "Never";
				}
				else
				{
					ComboBoxCell.Value = Main.GetSyncTimeString( Branch.SyncTime );
				}
				Row.Cells.Add( ComboBoxCell );

				// ComboBox for the type of sync
				ComboBoxCell = new DataGridViewComboBoxCell();
				foreach( string Sync in Enum.GetNames( typeof( ESyncType ) ) )
				{
					if( Sync != ESyncType.ArtistSyncGame.ToString() )
					{
						ComboBoxCell.Items.Add( Sync );
					}
				}
				ComboBoxCell.Value = Branch.SyncType.ToString();
				Row.Cells.Add( ComboBoxCell );

				// TextBox for the head change
				TextBoxCell = new DataGridViewTextBoxCell();
				TextBoxCell.Value = Branch.HeadChangelist;
				Row.Cells.Add( TextBoxCell );

				// TextBox for the last good CIS
				TextBoxCell = new DataGridViewTextBoxCell();
				TextBoxCell.Value = Branch.LatestGoodCISChangelist;
				Row.Cells.Add( TextBoxCell );

				// TextBox for the label of the latest build
				TextBoxCell = new DataGridViewTextBoxCell();
				TextBoxCell.Value = Branch.LatestBuildLabel;
				Row.Cells.Add( TextBoxCell );

				// TextBox for the label of the most recent QA build
				TextBoxCell = new DataGridViewTextBoxCell();
				TextBoxCell.Value = Branch.LatestQABuildLabel;
				Row.Cells.Add( TextBoxCell );

				BranchDataGridView.Rows.Add( Row );
			}
		}

		private void BranchConfigureClosed( object sender, FormClosedEventArgs e )
		{
			Main.ConfigureBranches = null;
		}
	}
}
