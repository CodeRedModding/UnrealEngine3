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
    public partial class NewClientDlg : Form
    {
        private P4Connection p4 = null;

        public NewClientDlg(P4Connection InP4)
        {
            InitializeComponent();

            p4 = InP4;
        }

        private void button_SelectTemplate_Click(object sender, EventArgs e)
        {
            ClientsDlg Dlg = new ClientsDlg(p4);

            if (Dlg.ShowDialog() == DialogResult.OK)
            {

            }
        }
    }
}
