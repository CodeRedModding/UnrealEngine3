using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;
using System.Security.Cryptography.X509Certificates;
using System.Diagnostics;
using System.Text.RegularExpressions;

namespace MacPackager
{
    public partial class ToolsHub : Form
    {
        protected Dictionary<bool, Bitmap> CheckStateImages = new Dictionary<bool, Bitmap>();

        public static string CertificateRequestFilter = "Certificate Request (*.csr)|*.csr|All Files (*.*)|*.*";
        public static string CertificatesFilter = "Code signing certificates (*.cer;*.p12)|*.cer;*.p12|All files (*.*)|*.*";
        public static string KeysFilter = "Keys (*.key;*.p12)|*.key;*.p12|All files (*.*)|*.*";
        public static string JustXmlKeysFilter = "XML Public/Private Key Pair (*.key)|*.key|All Files (*.*)|*.*";
        public static string PListFilter = "Property Lists (*.plist)|*.plist|All Files (*.*)|*.*";
        public static string JustP12Certificates = "Code signing certificates (*.p12)|*.p12|All files (*.*)|*.*";
		public static string IcnsFilter = "Mac OS X Icon (*.icns)|*.icns|All Files (*.*)|*.*";

		//@TODO
		public static string ChoosingFilesToInstallDirectory = null;
		
		/// <summary>
        /// Shows a file dialog, preserving the current working directory of the application
        /// </summary>
        /// <param name="Filter">Filter string for the dialog</param>
        /// <param name="Title">Display title for the dialog</param>
        /// <param name="DefaultExtension">Default extension</param>
        /// <param name="StartingFilename">Initial filename (can be null)</param>
        /// <param name="SavedDirectory">Starting directory for the dialog (will be updated on a successful pick)</param>
        /// <param name="OutFilename">(out) The picked filename, or null if the dialog was cancelled </param>
        /// <returns>True on a successful pick (filename is in OutFilename)</returns>
        public static bool ShowOpenFileDialog(string Filter, string Title, string DefaultExtension, string StartingFilename, ref string SavedDirectory, out string OutFilename)
        {
            // Save off the current working directory
            string CWD = Directory.GetCurrentDirectory();

            // Show the dialog
            System.Windows.Forms.OpenFileDialog OpenDialog = new System.Windows.Forms.OpenFileDialog();
            OpenDialog.DefaultExt = DefaultExtension;
            OpenDialog.FileName = (StartingFilename != null) ? StartingFilename : "";
            OpenDialog.Filter = Filter;
            OpenDialog.Title = Title;
            OpenDialog.InitialDirectory = (SavedDirectory != null) ? SavedDirectory : CWD;

            bool bDialogSucceeded = OpenDialog.ShowDialog() == DialogResult.OK;

            // Restore the current working directory
            Directory.SetCurrentDirectory(CWD);

            if (bDialogSucceeded)
            {
                SavedDirectory = Path.GetDirectoryName(OpenDialog.FileName);
                OutFilename = OpenDialog.FileName;

                return true;
            }
            else
            {
                OutFilename = null;
                return false;
            }
        }

		public static bool ShowOpenFolderDialog(string Title, out string OutFolderName)
		{
			System.Windows.Forms.FolderBrowserDialog FolderDialog = new System.Windows.Forms.FolderBrowserDialog();
			FolderDialog.Description = Title;

			bool bDialogSucceeded = FolderDialog.ShowDialog() == DialogResult.OK;

			if (bDialogSucceeded)
			{
				OutFolderName = FolderDialog.SelectedPath;
			}
			else
			{
				OutFolderName = null;
			}

			return bDialogSucceeded;
		}

        public ToolsHub()
        {
            InitializeComponent();

            CheckStateImages.Add(false, MacPackager.Properties.Resources.GreyCheck);
            CheckStateImages.Add(true, MacPackager.Properties.Resources.GreenCheck);

            Text = Config.AppDisplayName + " Wizard";
        }

        public static ToolsHub CreateShowingTools()
        {
            ToolsHub Result = new ToolsHub();
			if (!Program.bPackagingForMAS)
			{
				Result.tabControl1.TabPages.Remove(Result.tabPage2);
			}
            Result.tabControl1.SelectTab(Result.tabPage1);
            return Result;
        }

        private void ToolsHub_Shown(object sender, EventArgs e)
        {
            // Check the status of the various steps
            UpdateStatus();
        }

        void UpdateStatus()
        {
			X509Certificate2 Cert;
			bool bOverridesExists;
			CodeSignatureBuilder.FindRequiredFiles(out Cert, out bOverridesExists);

			CertificatePresentCheck2.Image = CheckStateImages[Cert != null];

			ReadyToPackageButton.Enabled = (bOverridesExists && (Cert != null)) || !Program.bPackagingForMAS;
		}

        private void CreateCSRButton_Click(object sender, EventArgs e)
        {
        }

        public static void ShowError(string Message)
        {
            MessageBox.Show(Message, Config.AppDisplayName, MessageBoxButtons.OK, MessageBoxIcon.Error);
        }

        public static void TryInstallingCertificate_PromptForKey()
        {
            try
            {
                string CertificateFilename;
                if (ShowOpenFileDialog(CertificatesFilter, "Choose a code signing certificate to import", "", "", ref ChoosingFilesToInstallDirectory, out CertificateFilename))
                {
                    // Load the certificate
                    string CertificatePassword = "";
                    X509Certificate2 Cert = null;
                    try
                    {
                        Cert = new X509Certificate2(CertificateFilename, CertificatePassword, X509KeyStorageFlags.PersistKeySet | X509KeyStorageFlags.Exportable | X509KeyStorageFlags.MachineKeySet);
                    }
                    catch (System.Security.Cryptography.CryptographicException ex)
                    {
                        // Try once with a password
                        if (PasswordDialog.RequestPassword(out CertificatePassword))
                        {
                            Cert = new X509Certificate2(CertificateFilename, CertificatePassword, X509KeyStorageFlags.PersistKeySet | X509KeyStorageFlags.Exportable | X509KeyStorageFlags.MachineKeySet);
                        }
                        else
                        {
                            // User cancelled dialog, rethrow
                            throw ex;
                        }
                    }

                    // If the certificate doesn't have a private key pair, ask the user to provide one
                    if (!Cert.HasPrivateKey)
                    {
                        string ErrorMsg = "Certificate does not include a private key and cannot be used to code sign";

                        // Prompt for a key pair
                        if (MessageBox.Show("Next, please choose the key pair that you made when generating the certificate request.",
                            Config.AppDisplayName,
                            MessageBoxButtons.OK,
                            MessageBoxIcon.Information) == DialogResult.OK)
                        {
                            string KeyFilename;
                            if (ShowOpenFileDialog(KeysFilter, "Choose the key pair that belongs with the signing certificate", "", "", ref ChoosingFilesToInstallDirectory, out KeyFilename))
                            {
                                Cert = CryptoAdapter.CombineKeyAndCert(CertificateFilename, KeyFilename);

                                if (Cert.HasPrivateKey)
                                {
                                    ErrorMsg = null;
                                }
                            }
                        }

                        if (ErrorMsg != null)
                        {
                            throw new Exception(ErrorMsg);
                        }
                    }

                    // Add the certificate to the store
                    X509Store Store = new X509Store();
                    Store.Open(OpenFlags.ReadWrite);
                    Store.Add(Cert);
                    Store.Close();
                }
            }
            catch (Exception ex)
            {
                string ErrorMsg = String.Format("Failed to load or install certificate due to an error: '{0}'", ex.Message);
                Program.Error(ErrorMsg);
                MessageBox.Show(ErrorMsg, Config.AppDisplayName, MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void ImportCertificateButton_Click(object sender, EventArgs e)
        {
            TryInstallingCertificate_PromptForKey();
            UpdateStatus();
        }

        private void EditPlistButton_Click(object sender, EventArgs e)
        {
            UpdateStatus();
        }

        private void CancelThisFormButton_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.Cancel;
            Close();
        }

        private void ReadyToPackageButton_Click(object sender, EventArgs e)
        {
			if (SaveConfig())
			{
				DialogResult = DialogResult.OK;
				Close();
			}
        }

        void OpenHelpWebsite()
        {
			string Target = "https://udn.epicgames.com/Three/UnrealMacPackager";
            ProcessStartInfo PSI = new ProcessStartInfo(Target);
            Process.Start(PSI);
        }

        private void ToolsHub_HelpButtonClicked(object sender, CancelEventArgs e)
        {
            OpenHelpWebsite();
        }

        private void ToolsHub_KeyUp(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.F1)
            {
                OpenHelpWebsite();
            }
        }
        
        private void HyperlinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            string Target = (sender as LinkLabel).Tag as string;
            ProcessStartInfo PSI = new ProcessStartInfo(Target);
            Process.Start(PSI);
        }

		void ReloadPList()
		{
			string Filename = Config.GetPlistOverrideFilename();
			string SourcePList = Config.ReadOrCreatePlist(Filename);
			Utilities.PListHelper Helper = new Utilities.PListHelper(SourcePList);

			string BundleID;
			string BundleName;
			string DeployPath;
			string BundleIconFile;

			if (!Helper.GetString("CFBundleIdentifier", out BundleID) ||
				BundleID == "")
			{
				BundleID = "com.YourCompany.GameNameNoSpaces";
				Helper.SetString("CFBundleIdentifier", BundleID);
			}

			if (!Helper.GetString("CFBundleName", out BundleName) ||
				BundleName == "")
			{
				BundleName = "UDKGame";
				Helper.SetString("CFBundleName", BundleName);
			}

			if (!Helper.GetString("MacPackagerDeployPath", out DeployPath) ||
				DeployPath == "")
			{
				DeployPath = "../../Binaries/Mac";
				Helper.SetString("MacPackagerDeployPath", DeployPath);
			}

			if (!Helper.GetString("CFBundleIconFile", out BundleIconFile) ||
				BundleIconFile == "" )
			{
				BundleIconFile = "";
				Helper.SetString("CFBundleIconFile", BundleIconFile);
			}

			BundleIdentifierEdit.Text = BundleID;
			BundleNameEdit.Text = BundleName;
			DeployPathEdit.Text = DeployPath;
			BundleIconFileEdit.Text = BundleIconFile;
		}

		private void SaveChanges()
		{
			// Development settings
			{
				// Open the existing plist override file
				string DevFilename = Config.GetPlistOverrideFilename();
				string SourcePList = Config.ReadOrCreatePlist(DevFilename);
				Utilities.PListHelper Helper = new Utilities.PListHelper(SourcePList);

				Helper.SetString("CFBundleIdentifier", BundleIdentifierEdit.Text);
				Helper.SetString("CFBundleName", BundleNameEdit.Text);
				Helper.SetString("MacPackagerDeployPath", DeployPathEdit.Text);
				Helper.SetString("CFBundleIconFile", BundleIconFileEdit.Text);

				// Save the modified plist
				Config.SavePList(Helper, DevFilename);
			}
		}

		private void ConfigureMacGame_Load(object sender, EventArgs e)
		{
			ReloadPList();
		}

		private void EditManuallyButton_Click(object sender, EventArgs e)
		{
			SaveChanges();

			string Filename = Config.GetPlistOverrideFilename();
			Process.Start("explorer.exe", String.Format("/select,\"{0}\"", Path.GetFullPath(Filename)));
		}

		private bool SaveConfig()
		{
			// Validate the edited values
			Regex AcceptableID = new Regex("[^a-zA-Z0-9.\\-]");
			if (AcceptableID.IsMatch(BundleIdentifierEdit.Text))
			{
				MessageBox.Show("Error: Bundle Identifier must contain only characters in the range [a-z] [A-Z] [0-9] . and -");
				return false;
			}

			if (BundleIdentifierEdit.Text.Length < 1)
			{
				ShowError("Error: Bundle Identifier must be at least 1 character long");
				return false;
			}

			if ((BundleNameEdit.Text.Length < 1) || (BundleNameEdit.Text.Length >= 16))
			{
				ShowError("Error: Bundle Name must be between 1 and 15 characters long");
				return false;
			}

			SaveChanges();

			return true;
		}

		private void BrowseDeployPathButton_Click(object sender, EventArgs e)
		{
			string NewDeployPath;
			if (ShowOpenFolderDialog("Select deploy path", out NewDeployPath))
			{
				DeployPathEdit.Text = NewDeployPath;
			}

			SaveChanges();
		}

		private void BrowseIconButton_Click(object sender, EventArgs e)
		{
            string IconsFilename;
			if (ShowOpenFileDialog(IcnsFilter, "Choose an application icon", "", "", ref ChoosingFilesToInstallDirectory, out IconsFilename))
			{
				BundleIconFileEdit.Text = IconsFilename;
			}
		}
	}
}