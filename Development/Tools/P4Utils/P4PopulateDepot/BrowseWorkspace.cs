// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace P4PopulateDepot
{
	public partial class BrowseWorkspaceDialog : Form
	{
		private string ListviewSelectedWorkspace = string.Empty;
        private bool bNoItemsFound = true;
		public string SelectedWorkspace
		{
			get
			{
				return ListviewSelectedWorkspace;
			}
		}

		public BrowseWorkspaceDialog()
		{
			InitializeComponent();

			Text = Program.Util.GetPhrase("BWBrowseWorkspaces");
			WSBrowseDescriptionTextBox.Text = Program.Util.GetPhrase("BWDescription");
			WSBrowseOKButton.Text = Program.Util.GetPhrase("GQOK");
			WSBrowseCancelButton.Text = Program.Util.GetPhrase("GQCancel");
            ShowAllCheckbox.Text = Program.Util.GetPhrase("BWShowAll");

			InitializeListView();

			UpdateList();
		}

		/// <summary>
		/// Clears and updates the workspace list view with new content.
		/// </summary>
		private void UpdateList()
		{
			Dictionary<string, string> Clients = Program.Util.GetConnectionWorkspaces(true, !ShowAllCheckbox.Checked);
			WSBrowseListViewAlwaysSel.BeginUpdate();
			if (WSBrowseListViewAlwaysSel.Items.Count > 0)
			{
				WSBrowseListViewAlwaysSel.Items.Clear();
			}

			foreach (KeyValuePair<string, string> Client in Clients)
			{
				string[] myItems = new string[] { Client.Key, Client.Value.Replace('/', '\\') };
				ListViewItem Lvi = new ListViewItem(myItems);
				WSBrowseListViewAlwaysSel.Items.Add(Lvi);
			}

            if (WSBrowseListViewAlwaysSel.Items.Count > 0)
            {
                bNoItemsFound = false;
                WSBrowseListViewAlwaysSel.Items[0].Selected = true;
                WSBrowseListViewAlwaysSel.ForeColor = System.Drawing.SystemColors.WindowText;
            }
            else
            {
                bNoItemsFound = true;
                WSBrowseListViewAlwaysSel.ForeColor = System.Drawing.SystemColors.GrayText;
                string[] myItems = new string[] { Program.Util.GetPhrase("BWNoneAvailable") , string.Empty };
                ListViewItem Lvi = new ListViewItem(myItems);
                WSBrowseListViewAlwaysSel.Items.Add(Lvi);
            }

			WSBrowseListViewAlwaysSel.EndUpdate();
		}


		private void InitializeListView()
		{
			string ColumnStr1 = Program.Util.GetPhrase("BWWorkspaceName");
			int ColumnWidth1 = 200;
			WSBrowseListViewAlwaysSel.Columns.Insert(0, ColumnStr1, ColumnWidth1, HorizontalAlignment.Left);

			int ColumnWidth2 = 446;
			string ColumnStr2 = Program.Util.GetPhrase("BWWorkspaceRoot");
			WSBrowseListViewAlwaysSel.Columns.Insert(1, ColumnStr2, ColumnWidth2, HorizontalAlignment.Left);
			WSBrowseListViewAlwaysSel.Scrollable = true;
		}

		private void WSBrowseOKButton_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.OK;
			Close();
		}


		private void WSBrowseListViewAlwaysSel_SelectedIndexChanged(object sender, EventArgs e)
		{
            if (bNoItemsFound)
            {
                ListviewSelectedWorkspace = string.Empty;
            }
            else if (WSBrowseListViewAlwaysSel.SelectedItems.Count > 0)
			{
				ListviewSelectedWorkspace = WSBrowseListViewAlwaysSel.SelectedItems[0].SubItems[0].Text;
			}	
		}

		private void WSBrowseListViewAlwaysSel_DoubleClick(object sender, EventArgs e)
		{
            if (bNoItemsFound)
            {
                ListviewSelectedWorkspace = string.Empty;
            }
            else if (WSBrowseListViewAlwaysSel.SelectedItems.Count > 0)
            {
                ListviewSelectedWorkspace = WSBrowseListViewAlwaysSel.SelectedItems[0].SubItems[0].Text;
                DialogResult = DialogResult.OK;
                Close();
            }
		}

		private void ShowAllCheckbox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateList();
		}
	}
}
