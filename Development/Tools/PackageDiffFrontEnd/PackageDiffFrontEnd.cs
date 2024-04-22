/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using System.Data;
using System.Diagnostics;
using System.Threading;
using System.IO;
using System.Xml;
using System.Xml.Serialization;
using Pipes;

namespace PackageDiffFrontEnd
{
	/// <summary>
	/// This form serves as a front-end for the diffpackages commandlet.
	/// 
	/// It provides an interface for choosing the game (wargame, examplegame, etc.), configuration (debug/release), and command-line parameters to use
	/// for running the diffpackages commandlet.
	/// 
	/// It can also take the names of the files to be diffed as command-line parameters so that it can be used as the "diff" app in the p4win program for
	/// unreal map and package files.
	/// 
	/// Much of this code is pretty sloppy and the rest of it was shamelessly ripped straight from the CookerFrontEnd, nor is the end result what anyone
	/// would call a "good-looking" app....but hey - it's my first time dabbling in C# so cut me some slack  :P
	/// </summary>
	public class PackageDiffWindow : System.Windows.Forms.Form
	{
		#region Variables
		private const string UNI_COLOR_MAGIC = "`~[~`";

		private System.Windows.Forms.GroupBox grp_Games;
		private System.Windows.Forms.GroupBox grp_Options;
		private System.Windows.Forms.Label lbl_SelectGame;
		private System.Windows.Forms.Label lbl_SelectConfig;
		private System.Windows.Forms.ComboBox cmb_SelectGame;
		private System.Windows.Forms.ComboBox cmb_SelectConfig;
		private System.Windows.Forms.CheckBox cb_ObjectProps;
		private System.Windows.Forms.CheckBox cb_NonEditProps;
		private System.Windows.Forms.Panel pnl_Filenames;
		private System.Windows.Forms.TextBox txt_fileA;
		private System.Windows.Forms.TextBox txt_FileB;
		private System.Windows.Forms.ToolBar tb_main;
		private System.Windows.Forms.ToolBarButton btn_start;
		private System.Windows.Forms.ToolBarButton btn_stop;
		private System.Windows.Forms.RichTextBox txt_LogWindow;
		private System.Windows.Forms.StatusBar stat_main;
		private System.Windows.Forms.Panel pnl_Parameters;
		private System.Windows.Forms.Panel pnl_LogWindow;

		/// <summary>
		/// Indicates whether the DiffPackages front end is active.
		/// </summary>
		public bool Running = false;

		/// <summary>
		/// Indicates whether the DiffPackages commandlet is currently running
		/// </summary>
		private bool CommandletActive;
		private int TickCount = 0;
		private int LastTickCount = 0;

		/** the filenames of the first file to be diffed */
		private string FirstFilename;

		/** the filename of the second file to be diffed */
		private string SecondFilename;

		/** The process corresponding to the diffpackages commandlet, for reading output and canceling.	*/
		private Process DiffPackagesCommandlet;

		/** A Timer object used to read output from the commandlet every N ms.				*/
		private System.Windows.Forms.Timer DiffPackagesCommandletTimer;

		/** Thread safe array used to get data from the command console and display.		*/
		private ArrayList LogTextArray = new ArrayList();

		/** Thread that polls the console placing the output in the OutputTextArray.		*/
		private Thread LogPolling;

		/** Instance of pipes helper class */
		private NamedPipe LogPipe;
		private System.Windows.Forms.ImageList ToolbarButtonImages;
		private System.ComponentModel.IContainer components;
		#endregion


		public PackageDiffWindow()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			//
			// TODO: Add any constructor code after InitializeComponent call
			//
		}

		public bool Init()
		{
			// Read the application settings
			InitializeControlValues();

			// Read the settings first and then set the default with those if possible
			//
			// TODO: Implement persistent storage of selected values
			//
// 			if (ReadSettings() == false)
			{
				cmb_SelectGame.SelectedItem		= "ExampleGame";
				cmb_SelectConfig.SelectedItem		= "Release";
			}

			// Create a Timer object.
			DiffPackagesCommandletTimer = new System.Windows.Forms.Timer();
			DiffPackagesCommandletTimer.Interval = 100;
			DiffPackagesCommandletTimer.Tick += new System.EventHandler( this.OnTimer );

			Show();
			Running = true;

			UpdateControls();
			return( true );
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			Running = false;
			if( disposing )
			{
				if (components != null) 
				{
					components.Dispose();
				}
			}
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(PackageDiffWindow));
            this.grp_Games = new System.Windows.Forms.GroupBox();
            this.cmb_SelectConfig = new System.Windows.Forms.ComboBox();
            this.cmb_SelectGame = new System.Windows.Forms.ComboBox();
            this.lbl_SelectGame = new System.Windows.Forms.Label();
            this.lbl_SelectConfig = new System.Windows.Forms.Label();
            this.grp_Options = new System.Windows.Forms.GroupBox();
            this.cb_NonEditProps = new System.Windows.Forms.CheckBox();
            this.cb_ObjectProps = new System.Windows.Forms.CheckBox();
            this.pnl_Filenames = new System.Windows.Forms.Panel();
            this.txt_FileB = new System.Windows.Forms.TextBox();
            this.txt_fileA = new System.Windows.Forms.TextBox();
            this.tb_main = new System.Windows.Forms.ToolBar();
            this.btn_start = new System.Windows.Forms.ToolBarButton();
            this.btn_stop = new System.Windows.Forms.ToolBarButton();
            this.ToolbarButtonImages = new System.Windows.Forms.ImageList(this.components);
            this.txt_LogWindow = new System.Windows.Forms.RichTextBox();
            this.pnl_LogWindow = new System.Windows.Forms.Panel();
            this.stat_main = new System.Windows.Forms.StatusBar();
            this.pnl_Parameters = new System.Windows.Forms.Panel();
            this.grp_Games.SuspendLayout();
            this.grp_Options.SuspendLayout();
            this.pnl_Filenames.SuspendLayout();
            this.pnl_LogWindow.SuspendLayout();
            this.pnl_Parameters.SuspendLayout();
            this.SuspendLayout();
            // 
            // grp_Games
            // 
            this.grp_Games.Controls.Add(this.cmb_SelectConfig);
            this.grp_Games.Controls.Add(this.cmb_SelectGame);
            this.grp_Games.Controls.Add(this.lbl_SelectGame);
            this.grp_Games.Controls.Add(this.lbl_SelectConfig);
            this.grp_Games.Location = new System.Drawing.Point(5, 5);
            this.grp_Games.Name = "grp_Games";
            this.grp_Games.Size = new System.Drawing.Size(303, 80);
            this.grp_Games.TabIndex = 0;
            this.grp_Games.TabStop = false;
            this.grp_Games.Text = "Setup";
            // 
            // cmb_SelectConfig
            // 
            this.cmb_SelectConfig.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.cmb_SelectConfig.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cmb_SelectConfig.Items.AddRange(new object[] {
            "Release",
            "Debug"});
            this.cmb_SelectConfig.Location = new System.Drawing.Point(72, 48);
            this.cmb_SelectConfig.Name = "cmb_SelectConfig";
            this.cmb_SelectConfig.Size = new System.Drawing.Size(224, 21);
            this.cmb_SelectConfig.TabIndex = 1;
            // 
            // cmb_SelectGame
            // 
            this.cmb_SelectGame.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.cmb_SelectGame.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cmb_SelectGame.ItemHeight = 13;
            this.cmb_SelectGame.Items.AddRange(new object[] {
            "ExampleGame",
            "UTGame",
            "GearGame"});
            this.cmb_SelectGame.Location = new System.Drawing.Point(72, 16);
            this.cmb_SelectGame.Name = "cmb_SelectGame";
            this.cmb_SelectGame.Size = new System.Drawing.Size(224, 21);
            this.cmb_SelectGame.TabIndex = 0;
            // 
            // lbl_SelectGame
            // 
            this.lbl_SelectGame.Location = new System.Drawing.Point(10, 16);
            this.lbl_SelectGame.Name = "lbl_SelectGame";
            this.lbl_SelectGame.Size = new System.Drawing.Size(86, 21);
            this.lbl_SelectGame.TabIndex = 1;
            this.lbl_SelectGame.Text = "Game";
            // 
            // lbl_SelectConfig
            // 
            this.lbl_SelectConfig.Location = new System.Drawing.Point(10, 48);
            this.lbl_SelectConfig.Name = "lbl_SelectConfig";
            this.lbl_SelectConfig.Size = new System.Drawing.Size(86, 21);
            this.lbl_SelectConfig.TabIndex = 2;
            this.lbl_SelectConfig.Text = "Config";
            // 
            // grp_Options
            // 
            this.grp_Options.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.grp_Options.Controls.Add(this.cb_NonEditProps);
            this.grp_Options.Controls.Add(this.cb_ObjectProps);
            this.grp_Options.Location = new System.Drawing.Point(313, 5);
            this.grp_Options.Name = "grp_Options";
            this.grp_Options.Size = new System.Drawing.Size(303, 80);
            this.grp_Options.TabIndex = 1;
            this.grp_Options.TabStop = false;
            this.grp_Options.Text = "Options";
            // 
            // cb_NonEditProps
            // 
            this.cb_NonEditProps.CheckAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.cb_NonEditProps.Checked = true;
            this.cb_NonEditProps.CheckState = System.Windows.Forms.CheckState.Checked;
            this.cb_NonEditProps.Dock = System.Windows.Forms.DockStyle.Top;
            this.cb_NonEditProps.Location = new System.Drawing.Point(3, 40);
            this.cb_NonEditProps.Name = "cb_NonEditProps";
            this.cb_NonEditProps.Size = new System.Drawing.Size(297, 24);
            this.cb_NonEditProps.TabIndex = 1;
            this.cb_NonEditProps.Text = "Include Non-Editable Properties";
            // 
            // cb_ObjectProps
            // 
            this.cb_ObjectProps.CheckAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.cb_ObjectProps.Dock = System.Windows.Forms.DockStyle.Top;
            this.cb_ObjectProps.Location = new System.Drawing.Point(3, 16);
            this.cb_ObjectProps.Name = "cb_ObjectProps";
            this.cb_ObjectProps.Size = new System.Drawing.Size(297, 24);
            this.cb_ObjectProps.TabIndex = 0;
            this.cb_ObjectProps.Text = "Include Object Properties";
            // 
            // pnl_Filenames
            // 
            this.pnl_Filenames.Controls.Add(this.txt_FileB);
            this.pnl_Filenames.Controls.Add(this.txt_fileA);
            this.pnl_Filenames.Dock = System.Windows.Forms.DockStyle.Top;
            this.pnl_Filenames.Location = new System.Drawing.Point(0, 132);
            this.pnl_Filenames.Name = "pnl_Filenames";
            this.pnl_Filenames.Padding = new System.Windows.Forms.Padding(5);
            this.pnl_Filenames.Size = new System.Drawing.Size(622, 32);
            this.pnl_Filenames.TabIndex = 2;
            // 
            // txt_FileB
            // 
            this.txt_FileB.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.txt_FileB.Location = new System.Drawing.Point(313, 5);
            this.txt_FileB.Name = "txt_FileB";
            this.txt_FileB.ReadOnly = true;
            this.txt_FileB.Size = new System.Drawing.Size(303, 20);
            this.txt_FileB.TabIndex = 1;
            this.txt_FileB.Text = "second file here";
            this.txt_FileB.TextChanged += new System.EventHandler(this.Filename_TextChanged);
            // 
            // txt_fileA
            // 
            this.txt_fileA.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.txt_fileA.Location = new System.Drawing.Point(5, 5);
            this.txt_fileA.Name = "txt_fileA";
            this.txt_fileA.ReadOnly = true;
            this.txt_fileA.Size = new System.Drawing.Size(303, 20);
            this.txt_fileA.TabIndex = 0;
            this.txt_fileA.Text = "first file here";
            this.txt_fileA.TextChanged += new System.EventHandler(this.Filename_TextChanged);
            // 
            // tb_main
            // 
            this.tb_main.Buttons.AddRange(new System.Windows.Forms.ToolBarButton[] {
            this.btn_start,
            this.btn_stop});
            this.tb_main.ButtonSize = new System.Drawing.Size(16, 16);
            this.tb_main.DropDownArrows = true;
            this.tb_main.ImageList = this.ToolbarButtonImages;
            this.tb_main.Location = new System.Drawing.Point(0, 0);
            this.tb_main.Name = "tb_main";
            this.tb_main.ShowToolTips = true;
            this.tb_main.Size = new System.Drawing.Size(622, 42);
            this.tb_main.TabIndex = 5;
            this.tb_main.TabStop = true;
            this.tb_main.Wrappable = false;
            this.tb_main.ButtonClick += new System.Windows.Forms.ToolBarButtonClickEventHandler(this.mainToolbar_ButtonClick);
            // 
            // btn_start
            // 
            this.btn_start.ImageIndex = 0;
            this.btn_start.Name = "btn_start";
            this.btn_start.Text = "Start";
            this.btn_start.ToolTipText = "Begins the package comparison";
            // 
            // btn_stop
            // 
            this.btn_stop.ImageIndex = 1;
            this.btn_stop.Name = "btn_stop";
            this.btn_stop.Text = "Stop";
            this.btn_stop.ToolTipText = "Cancels the current comparison";
            // 
            // ToolbarButtonImages
            // 
            this.ToolbarButtonImages.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("ToolbarButtonImages.ImageStream")));
            this.ToolbarButtonImages.TransparentColor = System.Drawing.Color.White;
            this.ToolbarButtonImages.Images.SetKeyName(0, "");
            this.ToolbarButtonImages.Images.SetKeyName(1, "");
            // 
            // txt_LogWindow
            // 
            this.txt_LogWindow.AcceptsTab = true;
            this.txt_LogWindow.AutoWordSelection = true;
            this.txt_LogWindow.CausesValidation = false;
            this.txt_LogWindow.Dock = System.Windows.Forms.DockStyle.Fill;
            this.txt_LogWindow.Location = new System.Drawing.Point(0, 0);
            this.txt_LogWindow.Name = "txt_LogWindow";
            this.txt_LogWindow.ReadOnly = true;
            this.txt_LogWindow.Size = new System.Drawing.Size(622, 203);
            this.txt_LogWindow.TabIndex = 6;
            this.txt_LogWindow.TabStop = false;
            this.txt_LogWindow.Text = "";
            this.txt_LogWindow.WordWrap = false;
            // 
            // pnl_LogWindow
            // 
            this.pnl_LogWindow.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
                        | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.pnl_LogWindow.Controls.Add(this.txt_LogWindow);
            this.pnl_LogWindow.Location = new System.Drawing.Point(0, 163);
            this.pnl_LogWindow.Name = "pnl_LogWindow";
            this.pnl_LogWindow.Size = new System.Drawing.Size(622, 203);
            this.pnl_LogWindow.TabIndex = 7;
            // 
            // stat_main
            // 
            this.stat_main.Location = new System.Drawing.Point(0, 367);
            this.stat_main.Name = "stat_main";
            this.stat_main.Size = new System.Drawing.Size(622, 22);
            this.stat_main.SizingGrip = false;
            this.stat_main.TabIndex = 8;
            this.stat_main.Text = "Ready";
            // 
            // pnl_Parameters
            // 
            this.pnl_Parameters.Controls.Add(this.grp_Options);
            this.pnl_Parameters.Controls.Add(this.grp_Games);
            this.pnl_Parameters.Dock = System.Windows.Forms.DockStyle.Top;
            this.pnl_Parameters.Location = new System.Drawing.Point(0, 42);
            this.pnl_Parameters.Name = "pnl_Parameters";
            this.pnl_Parameters.Padding = new System.Windows.Forms.Padding(5);
            this.pnl_Parameters.Size = new System.Drawing.Size(622, 90);
            this.pnl_Parameters.TabIndex = 0;
            // 
            // PackageDiffWindow
            // 
            this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
            this.ClientSize = new System.Drawing.Size(622, 389);
            this.Controls.Add(this.pnl_LogWindow);
            this.Controls.Add(this.pnl_Filenames);
            this.Controls.Add(this.pnl_Parameters);
            this.Controls.Add(this.stat_main);
            this.Controls.Add(this.tb_main);
            this.MinimumSize = new System.Drawing.Size(630, 415);
            this.Name = "PackageDiffWindow";
            this.Text = "Unreal Package Diff";
            this.Closing += new System.ComponentModel.CancelEventHandler(this.PackageDiffWindow_Closing);
            this.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.PackageDiffWindow_KeyPress);
            this.Resize += new System.EventHandler(this.PackageDiffWindow_Resize);
            this.grp_Games.ResumeLayout(false);
            this.grp_Options.ResumeLayout(false);
            this.pnl_Filenames.ResumeLayout(false);
            this.pnl_Filenames.PerformLayout();
            this.pnl_LogWindow.ResumeLayout(false);
            this.pnl_Parameters.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();

		}
		#endregion

		#region Initialization of control options
		/// <summary>
		/// Logs the bad XML information for debugging purposes
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e">The attribute info</param>
		protected void XmlSerializer_UnknownAttribute(object sender, XmlAttributeEventArgs e)
		{
			System.Xml.XmlAttribute attr = e.Attr;
			Console.WriteLine("Unknown attribute " + attr.Name + "='" + attr.Value + "'");
		}

		/// <summary>
		/// Logs the node information for debugging purposes
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e">The info about the bad node type</param>
		protected void XmlSerializer_UnknownNode(object sender, XmlNodeEventArgs e)
		{
			Console.WriteLine("Unknown Node:" + e.Name + "\t" + e.Text);
		}

		/// <summary>
		/// Saves the specified package diff settings to a XML file
		/// </summary>
		/// <param name="Cfg">The settings to write out</param>
		private void SavePackageDiffSettings(PackageDiffSettings Cfg)
		{
			Stream XmlStream = null;
			try
			{
				string Name = Application.ExecutablePath.Substring(0,Application.ExecutablePath.Length - 4);
				Name += "_cfg.xml";
				// Get the XML data stream to read from
				XmlStream = new FileStream(Name,FileMode.Create,
					FileAccess.Write,FileShare.None,256 * 1024,false);
				// Creates an instance of the XmlSerializer class so we can
				// save the settings object
				XmlSerializer ObjSer = new XmlSerializer(typeof(PackageDiffSettings));
				// Write the object graph as XML data
				ObjSer.Serialize(XmlStream,Cfg);
			}
			catch (Exception e)
			{
				Console.WriteLine(e.ToString());
			}
			finally
			{
				if (XmlStream != null)
				{
					// Done with the file so close it
					XmlStream.Close();
				}
			}
		}

		/// <summary>
		/// Reads the XML file containing the available control values and returns an object
		/// containing those values
		/// </summary>
		/// <returns>The new object settings, or null if the file is missing</returns>
		private PackageDiffSettings ReadPackageDiffSettings()
		{
			Stream XmlStream = null;
			PackageDiffSettings PackageDiffCfg = null;
			try
			{
				string Name = Application.ExecutablePath.Substring(0,Application.ExecutablePath.Length - 4);
				Name += "_cfg.xml";

				// Get the XML data stream to read from
				XmlStream = new FileStream(Name,FileMode.Open,FileAccess.Read,FileShare.None,256 * 1024,false);

				// Creates an instance of the XmlSerializer class so we can
				// read the settings object
				XmlSerializer ObjSer = new XmlSerializer(typeof(PackageDiffSettings));
				
				// Add our callbacks for a busted XML file
				ObjSer.UnknownNode += new XmlNodeEventHandler(XmlSerializer_UnknownNode);
				ObjSer.UnknownAttribute += new XmlAttributeEventHandler(XmlSerializer_UnknownAttribute);
				
				// Create an object graph from the XML data
				PackageDiffCfg = (PackageDiffSettings)ObjSer.Deserialize(XmlStream);
			}
			catch (Exception e)
			{
				Console.WriteLine(e.ToString());
			}
			finally
			{
				if (XmlStream != null)
				{
					// Done with the file so close it
					XmlStream.Close();
				}
			}
			return PackageDiffCfg;
		}

		/// <summary>
		/// Reads the supported game type and configuration settings. Saves a
		/// set of defaults if missing
		/// </summary>
		protected void InitializeControlValues()
		{
			// Read the XML file
			PackageDiffSettings DiffCfg = ReadPackageDiffSettings();
			if (DiffCfg == null)
			{
				// Create a set of defaults
				DiffCfg = new PackageDiffSettings(1);

				// Save these settings so they are there next time
				SavePackageDiffSettings(DiffCfg);
			}

			// Clear any previous data
			cmb_SelectGame.Items.Clear();
			cmb_SelectConfig.Items.Clear();

			// Initialize the "Select Game" combo with the list of available games
			foreach (string Item in DiffCfg.AvailableGames)
			{
				cmb_SelectGame.Items.Add(Item);
			}

			// Initialize the "Select Configuration" combo with the list of support configs
			foreach (string Item in DiffCfg.AvailableConfigurations)
			{
				cmb_SelectConfig.Items.Add(Item);
			}

			// now set the values of the textboxes which contain the names of the files we're diffing.
			string[] cmdparms = Environment.GetCommandLineArgs();
			if( cmdparms.Length >= 3 )
			{
				FirstFilename = cmdparms[1];
				SecondFilename = cmdparms[2];
				txt_fileA.ReadOnly = true;
				txt_FileB.ReadOnly = true;
			}
			else
			{
				// no files were specified on the command-line; allow the user to enter filenames themselves
				txt_fileA.ReadOnly = false;
				txt_FileB.ReadOnly = false;
			}

			txt_fileA.Text = FirstFilename;
			txt_FileB.Text = SecondFilename;
		}

		#endregion

		#region Log window helpers.
		void AddLine( string Line, Color TextColor )
		{
			Monitor.Enter( txt_LogWindow );

			txt_LogWindow.Focus();
			txt_LogWindow.SelectionColor = TextColor;
			txt_LogWindow.SelectionStart = txt_LogWindow.TextLength;
			txt_LogWindow.AppendText( Line );

			Monitor.Exit( txt_LogWindow );
		}

		/**
		 * Performs output gathering on another thread
		 */
		private void PollForOutput()
		{
			LogPipe = new NamedPipe();
			LogPipe.Connect( DiffPackagesCommandlet );

			while( DiffPackagesCommandlet != null )
			{
				try
				{
					string StdOutLine = LogPipe.Read();
					
					// Add that line to the output array in a thread-safe way
					Monitor.Enter( LogTextArray );
					if( StdOutLine.Length > 0 )
					{
						LogTextArray.Add( StdOutLine/*.Replace( "\r\n", "" )*/ );
					}
				}
				catch( ThreadAbortException )
				{
				}
				finally
				{
					Monitor.Exit( LogTextArray );
				}
			}

			LogPipe.Disconnect();
			LogPolling = null;
		}

		int GetTicked()
		{	
			int	Ticked = TickCount - LastTickCount;
			LastTickCount = TickCount;
			return( Ticked );
		}

		private void OnTimer( object sender, System.EventArgs e )
		{
			TickCount++;
		}
		#endregion

		#region Commandlet helpers
		/// <summary>
		/// Start up the given executable
		/// </summary>
		/// <param name="Executable">The string name of the executable to run.</param>
		/// <param name="CommandLIne">Any command line parameters to pass to the program.</param>
		/// <returns>The running process if successful, null on failure</returns>
		private Process CreateCommandletProcess( string Executable, string CommandLine )
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();

			// Prepare a ProcessStart structure 
			StartInfo.FileName = Executable;
			StartInfo.Arguments = CommandLine;
			StartInfo.UseShellExecute = false;
			// Redirect the output.
			StartInfo.RedirectStandardOutput = true;
			StartInfo.RedirectStandardError = true;
			StartInfo.CreateNoWindow = true;

            AddLine("Running: " + Executable + " " + CommandLine + Environment.NewLine, Color.OrangeRed);

			// Spawn the process
			Process NewProcess = null;
			// Try to start the process, handling thrown exceptions as a failure.
			try
			{
				NewProcess = Process.Start( StartInfo );
			}
			catch
			{
				return( null );
			}

			return( NewProcess );
		}

		/// <summary>
		/// Verifies that the filename textboxes contain values
		/// </summary>
		/// <param name="bVerifyFilesExist">Specify true to also check that the values in the filename textboxes correspond to actual files</param>
		/// <returns>true if both filename textboxes contain valid values</returns>
		private bool ValidateFilenames()
		{
			bool bValid = false;
			if ( txt_fileA.Text.Length > 0 && txt_FileB.Text.Length > 0 )
			{
				// can't use File.Exists because it is perfectly valid for the user to use just package names
//  				if ( File.Exists(txt_fileA.Text) && File.Exists(txt_FileB.Text) )
				{
					bValid = true;
				}
			}

			return bValid;
		}

		/// <summary>
		/// Gets the name of the game executable to use for running the commandlet.
		/// </summary>
		/// <returns>the name of the game executable to use for running the commandlet (i.e. DEBUG-ExampleGame.exe)</returns>
		private string GetCommandletExecutableName()
		{
			// Figure out executable and command line.
			string Executable = Application.StartupPath + "\\";

			if( cmb_SelectConfig.SelectedItem.ToString() == "Debug" )
			{
				Executable += "Debug-";
			}

			Executable += cmb_SelectGame.SelectedItem.ToString() + ".exe";
			return( Executable );	
		}

		/// <summary>
		/// Returns the command line parameters to use when spawning the commandlet.
		/// </summary>
		/// <returns>the commandline to use for starting the commandlet</returns>
		private string GetCommandletCommandLine()
		{
			// Base command
			//@todo ronp - retrieve the names of the files we're diffing from this app's command-line
			string CommandLine = "DiffPackages " + FirstFilename + " " + SecondFilename;

			if ( cb_ObjectProps.Checked )
			{
				CommandLine += " -all";
			}
			else if ( cb_NonEditProps.Checked )
			{
				CommandLine += " -most";
			}

			// we always want to have the latest .inis (this stops one from firing off the cooker and then coming back and seeing the "update .inis" message 
			// and then committing seppuku)
			CommandLine += " -updateInisAuto";
			return( CommandLine );
		}
		#endregion

		#region Main functions

		/// <summary>
		/// Equivalent to Unreal's "Tick" function.
		/// </summary>
		public void RunFrame()
		{
			if( DiffPackagesCommandlet == null )
			{
				return;
			}

			if( !DiffPackagesCommandlet.HasExited )
			{
				// Lock the array for thread safety.
				Monitor.Enter( LogTextArray );
	
				// Iterate over all of the strings in the array.
				foreach( string Line in LogTextArray )
				{
					Color TextColor = Color.Blue;
					// Figure out which color to use for line.
					if( Line.StartsWith( UNI_COLOR_MAGIC ) )
					{
						// Ignore any special console colours
						continue;
					}
					else if( Line.StartsWith( "Warning" ) )
					{
						TextColor = Color.Orange;
					}
					else if( Line.StartsWith( "Error" ) )
					{
						TextColor = Color.Red;
					}

					// Add the line to the log window with the appropriate color.
					AddLine( Line, TextColor );
				}

				// Empty the array and release the lock
				LogTextArray.Clear();
				Monitor.Exit( LogTextArray );
			}
			else
			{
				StopCommandlet();
				AddLine( "\r\n[COMMANDLET FINISHED]\r\n", Color.Green );
			}
		}

		/// <summary>
		/// Updates the enabled state of the toolbar buttons and filename text controls based on certain conditions.
		/// </summary>
		private void UpdateControls()
		{
			// if the commandlet is currently running, don't allow the user to press the "Start" button
			// or change the value of the filename textboxes
			if ( CommandletActive )
			{
				// disable the start button
				btn_start.Enabled = false;

				// enable the stop button
				btn_stop.Enabled = true;

				// disable the filename combos
				txt_fileA.Enabled = false;
				txt_FileB.Enabled = false;
			}
			else
			{
				// otherwise, enable the filename combos
				txt_fileA.Enabled = true;
				txt_FileB.Enabled = true;

				// if we don't have valid text in both filename textboxes, can't run a diff!
				btn_start.Enabled = ValidateFilenames();
				btn_stop.Enabled = false;
			}
		}

		private void LayoutContainerChildren( Control LeftChildControl, Control RightChildControl, int NewChildX, int NewChildWidth, int Padding )
		{
			LeftChildControl.SetBounds(NewChildX, LeftChildControl.Location.Y, NewChildWidth, LeftChildControl.Size.Height);

			NewChildX += NewChildWidth + Padding;
			RightChildControl.SetBounds(NewChildX, RightChildControl.Location.Y, NewChildWidth, RightChildControl.Size.Width);
		}

		private void LayoutFilenamePanel()
		{
			int NewChildX = pnl_Filenames.Location.X + pnl_Filenames.DockPadding.Left;
			int ParentWidth = pnl_Filenames.Width - (pnl_Filenames.DockPadding.Left + pnl_Filenames.DockPadding.Right);
			int NewChildWidth = (ParentWidth - pnl_Filenames.DockPadding.All) / 2;

			LayoutContainerChildren( txt_fileA, txt_FileB, NewChildX, NewChildWidth, pnl_Filenames.DockPadding.All );
		}

		private void LayoutParameterPanel()
		{
			int NewChildX = pnl_Parameters.Location.X + pnl_Parameters.DockPadding.Left;
			int ParentWidth = pnl_Parameters.Width - (pnl_Parameters.DockPadding.Left + pnl_Parameters.DockPadding.Right);
			int NewChildWidth = (ParentWidth - pnl_Parameters.DockPadding.All) / 2;

			LayoutContainerChildren( grp_Games, grp_Options, NewChildX, NewChildWidth, pnl_Parameters.DockPadding.All );
		}

		/// <summary>
		/// Called when the execution status of the commandlet is changed.  Enables/disables various controls and
		/// create a listener to route the output from the commandlet into our log window.
		/// </summary>
		/// <param name="On">Whether the commandlet is currently being executed.</param>
		private void SetCommandletRunning( bool On )
		{
			if( On && ValidateFilenames() )
			{
				// Kick off the thread that monitors the commandlet's ouput
				ThreadStart ThreadDelegate = new ThreadStart( PollForOutput );
				LogPolling = new Thread( ThreadDelegate );
				LogPolling.Start();

				// start the timer to read output
				DiffPackagesCommandletTimer.Start();
				CommandletActive = true;
			}
			else
			{
				// Stop the timer now.
				DiffPackagesCommandletTimer.Stop();
				CommandletActive = false;
			}

			UpdateControls();
		}

		/**
		 * Spawn a copy of the commandlet and trap the output
		 */
		private void RunCommandlet()
		{
			AddLine( "[COMMANDLET STARTED]\r\n", Color.Green );

			string Executable = GetCommandletExecutableName();
			string CommandLine = GetCommandletCommandLine();
			
			// Launch the commandlet.
			DiffPackagesCommandlet = CreateCommandletProcess( Executable, CommandLine );

			// Launch successful.
			if( DiffPackagesCommandlet != null )
			{
				SetCommandletRunning( true );
			}
			else
			{
				SetCommandletRunning( false );
				// Failed to launch executable.
				MessageBox.Show(
					this,
					"Failed to launch commandlet (" + Executable + "). Check your settings and try again.",
					"Launch Failed",
					MessageBoxButtons.OK,
					MessageBoxIcon.Error );
			}		
		}

		private void StopCommandlet()
		{
			if( DiffPackagesCommandlet != null && !DiffPackagesCommandlet.HasExited )
			{
				// Need to null this in a threadsafe way
				Monitor.Enter( DiffPackagesCommandlet );
				// Since we entered it means the thread is done with it
				Monitor.Exit( DiffPackagesCommandlet );

				DiffPackagesCommandlet.Kill();
				DiffPackagesCommandlet.WaitForExit();
			}
			DiffPackagesCommandlet = null;

			SetCommandletRunning( false );
		}

		
		private void StartDiffing()
		{
			// clear the contents of the log window.
			txt_LogWindow.Clear();

			// update our persistent values based on the current text in the filename textboxes
			FirstFilename = txt_fileA.Text;
			SecondFilename = txt_FileB.Text;

			// start the commandlet
			RunCommandlet();
		}

		/// <summary>
		/// Displays a confirmation prompt to the user and requests the commandlet process be terminated.
		/// </summary>
		/// <returns></returns>
		private bool StopDiffing()
		{
			DialogResult result = MessageBox.Show( "Are you sure you want to cancel the diff?", "Stop Diff", MessageBoxButtons.YesNo, MessageBoxIcon.Question );
			if( result == DialogResult.Yes )
			{
				StopCommandlet();
				AddLine( "\r\n[COMMANDLET ABORTED]\r\n", Color.Green );
				return( true );
			}

			return( false );
		}
		
		#endregion

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main() 
		{
			// create the form
			PackageDiffWindow DiffPackageFrontEnd = new PackageDiffWindow();

			// perform basic initialization
			if ( !DiffPackageFrontEnd.Init() )
			{
				return;
			}

			// main loop
			while ( DiffPackageFrontEnd.Running )
			{
				// Process system messages (allow OnTimer to be called)
				Application.DoEvents();

				// Tick changes once every 100ms
				if( DiffPackageFrontEnd.GetTicked() != 0 )
				{
					DiffPackageFrontEnd.RunFrame();
				}

				// Yield to system
				System.Threading.Thread.Sleep( 5 );
			}
		}

		/// <summary>
		/// Called when one of the toolbar buttons is clicked.  Starts or stops the commandlet.
		/// </summary>
		private void mainToolbar_ButtonClick(object sender, System.Windows.Forms.ToolBarButtonClickEventArgs e)
		{
			if ( e.Button == btn_start && !CommandletActive )
			{
				StartDiffing();
			}
			else if ( e.Button == btn_stop && CommandletActive )
			{
				StopDiffing();
			}
		}

		/// <summary>
		/// Called when the form is closing
		/// </summary>
		private void PackageDiffWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
		{
			if( DiffPackagesCommandlet != null )
			{
				if( !StopDiffing() )
				{
					// Don't exit!
					e.Cancel = true;
				}
			}		
		}

		/// <summary>
		/// Called when the text in one of the filename textboxes is changed.
		/// </summary>
		private void Filename_TextChanged(object sender, System.EventArgs e)
		{
			UpdateControls();
		}

		private void PackageDiffWindow_Resize(object sender, System.EventArgs e)
		{
			LayoutParameterPanel();
			LayoutFilenamePanel();
		}

		private void PackageDiffWindow_KeyPress(object sender, System.Windows.Forms.KeyPressEventArgs e)
		{
			if ( e.KeyChar == (char)13 )
			{
				if ( CommandletActive )
				{
					StopDiffing();
				}
				else
				{
					StartDiffing();
				}
				e.Handled = true;
			}
		}
	}
}
