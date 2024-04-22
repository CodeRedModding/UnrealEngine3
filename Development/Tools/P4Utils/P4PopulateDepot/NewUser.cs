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
    public partial class NewUserDialog : Form
    {
        private string ErrorMsgUserName = "";
        private bool bIsErrorUserName = false;
        private string ErrorMsgPass = "";
        private bool bIsErrorPass = false;

        public string SelectedUser
        {
            get { return NUUserNameTextBox.Text; }
        }

        public string SelectedPassword
        {
            get
            {
                return NUPasswordTextBox1.Text;
            }
        }

        public NewUserDialog()
        {
            InitializeComponent();

            Text = Program.Util.GetPhrase("NUNewUser");

            NUOKButton.Text = Program.Util.GetPhrase("GQOK");
            NUCancelButton.Text = Program.Util.GetPhrase("GQCancel");

            ClearErrors();

        }

        public void ClearErrors()
        {
            NUUserNameErrLabel.Visible = false;
            NUPasswordErrLabel.Visible = false;
            NUEmailErrLabel.Visible = false;
            NUMainErrorDescription.Visible = false;
        }

        private bool AreErrorsDetected()
        {
            bIsErrorUserName = false;
            bIsErrorPass = false;

            if (NUUserNameTextBox.Text == string.Empty)
            {
                bIsErrorUserName = true;
                ErrorMsgUserName = Program.Util.GetPhrase("NUUserEmpty");
            }
            else if (NUUserNameTextBox.Text.Contains(" "))
            {
                bIsErrorUserName = true;
                ErrorMsgUserName = Program.Util.GetPhrase("NUUserSpace");
            }
            else if (Program.Util.UserExists(NUUserNameTextBox.Text))
            {
                bIsErrorUserName = true;
                ErrorMsgUserName = Program.Util.GetPhrase("NUUserExists");
            }

            if (NUPasswordTextBox1.Text != string.Empty || NUPasswordTextBox1.Text != string.Empty)
            {
                if (NUPasswordTextBox1.Text != NUPasswordTextBox2.Text)
                {
                    bIsErrorPass = true;
                    ErrorMsgPass = Program.Util.GetPhrase("NUPassMissmatch");
                }
            }

            return (bIsErrorUserName || bIsErrorPass);
        }

        private void UpdateErrorUI()
        {
            ClearErrors();

            if (bIsErrorUserName)
            {
                NUUserNameErrLabel.Visible = true;
                NUMainErrorDescription.Text = ErrorMsgUserName;
                NUMainErrorDescription.Visible = true;
            }

            if (bIsErrorPass)
            {
                NUPasswordErrLabel.Visible = true;
                NUMainErrorDescription.Text = ErrorMsgPass;
                NUMainErrorDescription.Visible = true;
            }
        }

        private void NUOKButton_Click(object sender, EventArgs e)
        {
            bool bAreErrorsDetected = AreErrorsDetected();
            UpdateErrorUI();

            if (!bAreErrorsDetected)
            {
                Cursor CurRestore = Cursor.Current;
                Cursor.Current = Cursors.WaitCursor;
                bool bIsUserCreated = Program.Util.CreateNewUser(NUUserNameTextBox.Text, NUUserNameTextBox.Text, NUPasswordTextBox1.Text, NUEmailTextBox.Text );
                Cursor.Current = CurRestore;
                
                if (bIsUserCreated)
                {
                    DialogResult = DialogResult.OK;
                    Close();
                }
                else
                {
                    bIsErrorUserName = true;
                    ErrorMsgUserName = Program.Util.GetPhrase("NUPassMissmatch");
                    UpdateErrorUI();
                }
            }
        }
    }
}
