using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace UnSetup
{
    public partial class ProjectSelect : Form
    {
        public ProjectSelect()
        {
            InitializeComponent();

            Text = Program.Util.GetPhrase("PSProjectSelect");
            ProjectSelectTitleLabel.Text = Text;
            ProjectSelectOKButton.Text = Program.Util.GetPhrase("IENext");
			ProjectSelectCancelButton.Text = Program.Util.GetPhrase("GQCancel");
            ProjectSelectDescTextBox1.Text = Program.Util.GetPhrase("PSGameUT3");
            ProjectSelectDescTextBox2.Text = Program.Util.GetPhrase("PSGameEmpty");

            ProjectSelectRadioButton1.Text = Program.Util.GetPhrase("PSGameUT3Title");
            ProjectSelectRadioButton2.Text = Program.Util.GetPhrase("PSGameEmptyTitle");

            ProjectSelectFooterLineLabel.Text = Program.Util.UnSetupVersionString;
        }

        private void ProjectSelectOKButton_Click(object sender, EventArgs e)
        {
			if (this.ProjectSelectRadioButton1.Checked)
			{
				Program.Util.bIsCustomProject = false;
			}
			else if (this.ProjectSelectRadioButton2.Checked)
			{
				Program.Util.bIsCustomProject = true;
			}
			else
			{
				// Default case
				Program.Util.bIsCustomProject = false;
			}


            DialogResult = DialogResult.OK;
            Close();
        }

    }
}
