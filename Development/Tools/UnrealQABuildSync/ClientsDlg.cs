using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using P4API;

namespace UnrealQABuildSync
{
    public partial class ClientsDlg : Form
    {
        private P4Connection p4 = null;

        public ClientsDlg(P4Connection InP4)
        {
            InitializeComponent();

            p4 = InP4;
            this.button_OK.DialogResult = DialogResult.OK;
            this.button_Cancel.DialogResult = DialogResult.Cancel;

            InitializeListView();
        }

        public void InitializeListView()
        {
            // Select the item and subitems when selection is made.
            this.listView_Clients.FullRowSelect = true;
            // Display grid lines.
            this.listView_Clients.GridLines = true;
            // Sort the items in the list in ascending order.
            this.listView_Clients.Sorting = SortOrder.Ascending;


            this.listView_Clients.Columns.Add("Client", 20 * (int)listView_Clients.Font.SizeInPoints, HorizontalAlignment.Left);
            this.listView_Clients.Columns.Add("Owner", 10 * (int)listView_Clients.Font.SizeInPoints, HorizontalAlignment.Left);
            this.listView_Clients.Columns.Add("Host", 10 * (int)listView_Clients.Font.SizeInPoints, HorizontalAlignment.Left);
            this.listView_Clients.Columns.Add("Date", 10 * (int)listView_Clients.Font.SizeInPoints, HorizontalAlignment.Left);
            this.listView_Clients.Columns.Add("Root", 20 * (int)listView_Clients.Font.SizeInPoints, HorizontalAlignment.Left);
            this.listView_Clients.Columns.Add("Description", 40 * (int)listView_Clients.Font.SizeInPoints, HorizontalAlignment.Left);

            ListViewItem listItem = null;
            P4RecordSet Workspaces = p4.Run("workspaces");
            foreach (P4Record workspace in Workspaces)
            {
                listItem = new ListViewItem();
                listItem.Text = workspace["client"];
                listItem.SubItems.Add( workspace["Owner"] );
                listItem.SubItems.Add( workspace["Host"] );
                listItem.SubItems.Add( p4.ConvertDate(workspace["Update"]).ToString());
                listItem.SubItems.Add( workspace["Root"] );
                listItem.SubItems.Add(workspace["Description"]);
                this.listView_Clients.Items.Add(listItem);
            }
        } 
    }
}
