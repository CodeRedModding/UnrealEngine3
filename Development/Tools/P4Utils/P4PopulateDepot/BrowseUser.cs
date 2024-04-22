using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using P4API;

namespace P4PopulateDepot
{
    public partial class BrowseUserDialog : Form
    {
        private string ListviewSelectedUser = string.Empty;
        private bool bNoItemsFound = true;
        public string SelectedUser
        {
            get
            {
                return ListviewSelectedUser;
            }
        }

        public BrowseUserDialog()
        {
            InitializeComponent();

            Text = Program.Util.GetPhrase("BUBrowseUsers");
            BUBrowseDescriptionTextBox.Text = Program.Util.GetPhrase("BUDescription");
            BUOKButton.Text = Program.Util.GetPhrase("GQOK");
            BUCancelButton.Text = Program.Util.GetPhrase("GQCancel");

            InitializeListView();

            UpdateList();
        }

        private void InitializeListView()
        {
            string ColumnStr1 = Program.Util.GetPhrase("BUUserName");
            int ColumnWidth1 = 160;
            BUListViewAlwaysSel.Columns.Insert(0, ColumnStr1, ColumnWidth1, HorizontalAlignment.Left);

            string ColumnStr2 = Program.Util.GetPhrase("BUEmail");
            int ColumnWidth2 = 220;
            BUListViewAlwaysSel.Columns.Insert(1, ColumnStr2, ColumnWidth2, HorizontalAlignment.Left);

            //string ColumnStr3 = Program.Util.GetPhrase("BULastAccessed");
            //int ColumnWidth3 = 146;
            //BUListViewAlwaysSel.Columns.Insert(2, ColumnStr3, ColumnWidth3, HorizontalAlignment.Left);

            string ColumnStr3 = Program.Util.GetPhrase("BUFullName");
            int ColumnWidth3 = 346;
            BUListViewAlwaysSel.Columns.Insert(2, ColumnStr3, ColumnWidth3, HorizontalAlignment.Left);

            BUListViewAlwaysSel.Scrollable = true;
        }

        /// <summary>
        /// Clears and updates the user list view with new content.
        /// </summary>
        private void UpdateList()
        {
            P4RecordSet P4Users = Program.Util.GetConnectionUsers();

            BUListViewAlwaysSel.BeginUpdate();
            if (BUListViewAlwaysSel.Items.Count > 0)
            {
                BUListViewAlwaysSel.Items.Clear();
            }

            if (P4Users != null)
            {
                foreach (P4Record UserRecord in P4Users)
                {
                    string[] EntryItems = new string[] { UserRecord["User"], UserRecord["Email"], UserRecord["FullName"] };
                    ListViewItem Lvi = new ListViewItem(EntryItems);
                    BUListViewAlwaysSel.Items.Add(Lvi);
                }
            }

            if (BUListViewAlwaysSel.Items.Count > 0)
            {
                bNoItemsFound = false;
                BUListViewAlwaysSel.Items[0].Selected = true;
                BUListViewAlwaysSel.ForeColor = System.Drawing.SystemColors.WindowText;
            }
            else
            {
                bNoItemsFound = true;
                BUListViewAlwaysSel.ForeColor = System.Drawing.SystemColors.GrayText;
                string[] EntryItems = new string[] { Program.Util.GetPhrase("BUNoneAvailable"), string.Empty, string.Empty };
                ListViewItem Lvi = new ListViewItem(EntryItems);
                BUListViewAlwaysSel.Items.Add(Lvi);
            }
            BUListViewAlwaysSel.EndUpdate();
        }

        private void BUOKButton_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.OK;
            Close();
        }

        private void BUListViewAlwaysSel_DoubleClick(object sender, EventArgs e)
        {
            if (bNoItemsFound)
            {
                ListviewSelectedUser = string.Empty;
            }
            else if (BUListViewAlwaysSel.SelectedItems.Count > 0)
            {
                ListviewSelectedUser = BUListViewAlwaysSel.SelectedItems[0].SubItems[0].Text;
                DialogResult = DialogResult.OK;
                Close();
            }
        }

        private void BUListViewAlwaysSel_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (bNoItemsFound)
            {
                ListviewSelectedUser = string.Empty;
            }
            else if (BUListViewAlwaysSel.SelectedItems.Count > 0)
            {
                ListviewSelectedUser = BUListViewAlwaysSel.SelectedItems[0].SubItems[0].Text;
            }	
        }


    }
}
