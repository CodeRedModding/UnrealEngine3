// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;
using P4API;

namespace P4PopulateDepot
{
	/// <summary>
	/// Class that contains a number of utility functions.
	/// </summary>
	public partial class Utils
	{

		public class P4ConnectionInfo
		{
			public volatile P4Connection P4Con = null;

			private string p4Client = null; // Clientspec
			private string p4Port = null;
			private string p4User = null;
			private string p4Pass = null;
			//private string p4Passwd = null;

			private bool bIsConnectionInvalidated = false;
			public bool IsConnectionInvalidated
			{
				get { return bIsConnectionInvalidated; }
			}

			public string Client
			{
				get { return p4Client; }
				set
				{
					if(p4Client != value)
					{
						p4Client = value;
						bIsConnectionInvalidated = true;
					}
				}
			}

			public string Password
			{
				get { return p4Pass; }
				set
				{
					if (p4Pass != value)
					{
						p4Pass = value;
						bIsConnectionInvalidated = true;
					}
				}
			}

			public string Port
			{
				get 
                { 
                    return p4Port; 
                }
				set
				{
					if(p4Port != value)
					{
						p4Port = value;
						bIsConnectionInvalidated = true;
					}
				}
			}
			public string User
			{
				get { return p4User; }
				set
				{
					if(p4User != value)
					{
						p4User = value;
						bIsConnectionInvalidated = true;
					}
				}
			}


			/// <summary>
			/// Will initialize a Perforce server connection with the provided settings.  This will disconnect from any existing connections before proceeding.
			/// </summary>
			public void P4Init()
			{
				if(P4Con != null)
				{
					P4Disconnect();
				}

                try
                {
                    P4Con = new P4Connection();
                    P4Con.ExceptionLevel = P4ExceptionLevels.ExceptionOnBothErrorsAndWarnings;

                    if (!string.IsNullOrWhiteSpace(Port))
                    {
                        P4Con.Port = Port;
                    }
                    else if (!string.IsNullOrWhiteSpace(P4Con.Port))
                    {
                        // The server address (Port) value can appear in non-intuitive ways.  For the cases where we start off with
                        // an address like 127.0.0.1:1666 or just 1666 (both valid if the server is local), we will translate to a more user
                        // friendly Host-Name:1666
                        P4Con.Port = ConditionServerAddress(P4Con.Port);
                    }
                    if (!string.IsNullOrWhiteSpace(User)) P4Con.User = User;
                    if (!string.IsNullOrWhiteSpace(Client)) P4Con.Client = Client;
                    if (!string.IsNullOrWhiteSpace(Password)) P4Con.Password = Password;

                    P4Con.CWD = GetProjectRoot();
                }
                catch (System.BadImageFormatException)
                {
                    string MBText = "Perforce assembly failed to load properly and the application can not continue.  This usually means the project needs to be built using the x86 configuration.";
                    //MBText += "  Exception Info: " + Ex.ToString();
                    string MBCap = Program.Util.GetPhrase("ERRError");
                    MessageBox.Show(new Form() { TopMost = true }, MBText, MBCap, MessageBoxButtons.OK, MessageBoxIcon.Error);
                    Application.Exit();
                }

				try
				{
					P4Con.Connect();
					if (Password != null && !P4Con.IsValidConnection(true, false))
					{
						P4Con.Login(Password);
					}
				}
				catch (Exception)
				{
				}
				

				if(P4Con.IsValidConnection(true, true))
				{
					Port = P4Con.Port;
					User = P4Con.User;
					Client = P4Con.Client;
					
					bIsConnectionInvalidated = false;
				}
				else if (P4Con.IsValidConnection(true, false))
				{
					Port = P4Con.Port;
					User = P4Con.User;
					bIsConnectionInvalidated = false;
				}
				else if (P4Con.IsValidConnection(false, false))
				{
					Port = P4Con.Port;

					if (!string.IsNullOrEmpty(P4Con.User) && Program.Util.UserExists(P4Con.User))
					{
						User = P4Con.User;
					}

					bIsConnectionInvalidated = false;
				}
			}

			/// <summary>
			/// Disconnects any active Perforce server connections.
			/// </summary>
			public void P4Disconnect()
			{
				if(P4Con != null)
				{
					P4Con.Disconnect();
					P4Con = null;
				}
			}


			/// <summary>
			/// Checks to see if a Perforce server connection is valid.  If both parameters are false, this will only check to see if the server exists.
			/// </summary>
			/// <param name="CheckLogin">if set to <c>true</c> this function will check if the login status is valid.</param>
			/// <param name="CheckClient">if set to <c>true</c> this function will check if the workspace(clientspec) is valid.</param>
			/// <returns><c>true</c> if the connection is valid, or <c>false</c> otherwise</returns>
			public bool P4IsConnectionValid(bool CheckLogin, bool CheckClient)
			{
				bool bIsConnectionValid = false;
				if(!bIsConnectionInvalidated && P4Con != null)
				{
					try
					{
						bIsConnectionValid = P4Con.IsValidConnection(CheckLogin, CheckClient);
					}
					catch(Exception)
					{
					}
				}
				return bIsConnectionValid;
			}

            /// <summary>
            /// Conditions some of the valid server addresses to a more user readable format.
            /// </summary>
            /// <param name="InServerAddress">A string representing a server address.</param>
            /// <returns>A more user readable server address or if no changes are made it will pass back the string that was passed in</returns>
            private string ConditionServerAddress(string InServerAddress)
            {
                string Result = InServerAddress;

                if (Result.StartsWith("127.0.0.1"))
                {
                    // If the address starts with the IP representation for localhost we'll replace it with host info
                    if(!string.IsNullOrWhiteSpace(P4Con.Host))
                    {
                        Result = Result.Replace("127.0.0.1", P4Con.Host);
                    }
                    else
                    {
                        Result = Result.Replace("127.0.0.1", "localhost");
                    }
                }
                else if (IsPort(Result))
                {
                    // If the address is the port only(ex. 1666), we will add the host info. In the end it will look like (LocalHost:Port)
                    if (!string.IsNullOrWhiteSpace(P4Con.Host))
                    {
                        Result = P4Con.Host + ":" + Result;
                    }
                    else
                    {
                        Result = "localhost:" + Result;
                    }
                }

                return Result;
            }

            private bool IsPort(string InString)
            {
                if (string.IsNullOrWhiteSpace(InString))
                {
                    return false;
                }

                Regex NumRegex = new Regex(@"^[0-9]+$", RegexOptions.Compiled | RegexOptions.IgnoreCase);

                if (NumRegex.IsMatch(InString))
                {
                    try
                    {
                        if (Convert.ToInt32(InString) <= 65535)
                            return true;
                    }
                    catch (Exception)
                    {
                    }
                }

                return false;
            }
		}
		
		public P4ConnectionInfo P4ConInfo = new P4ConnectionInfo();
		string CLNamePrepare = "[Initial Submit] - Generated by UE3 P4 Primer Utility";


		/// <summary>
		/// Checks to see if the user exists on the Perforce server.
		/// </summary>
		/// <param name="UserStr">The username to check.</param>
		/// <returns><c>true</c> if the user exists, or <c>false</c> otherwise</returns>
		public bool UserExists(string UserStr)
		{
			bool bUserExistsOnServer = false;
			if (!string.IsNullOrEmpty(UserStr))
			{
				if (P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(false, false))
				{
					try
					{
						P4RecordSet P4Output = P4ConInfo.P4Con.Run("users", UserStr);
						foreach (P4Record record in P4Output)
						{
							if (P4ConInfo.P4Con.IsServerCaseSensitive())
							{
								if (record["User"] == UserStr)
								{
									bUserExistsOnServer = true;
									break;
								}
							}
							else
							{
								if (record["User"].ToLower() == UserStr.ToLower())
								{
									bUserExistsOnServer = true;
									break;
								}
							}
						}
					}
					catch (Exception)
					{
					}
				}
			}
			return bUserExistsOnServer;
		}

		/// <summary>
		/// Gets the mapped workspace(clientspec) root on the local machine.  This function only returns the expected mapping and
		/// does not check to see if the folder exists locally.
		/// </summary>
		/// <param name="UserName">Name of the user the workspace belongs to.</param>
		/// <param name="ClientName">The workspace name to get the root for.</param>
		/// <returns>A string representing the local folder the workspace maps to, or null if the mapping could not be worked out</returns>
		public string GetClientRoot(string UserName, string ClientName)
		{
			string RetVal = string.Empty;
			if(!string.IsNullOrEmpty(UserName) && !string.IsNullOrEmpty(ClientName))
			{
				if(P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(true, false))
				{
					try
					{
						P4RecordSet P4Output = P4ConInfo.P4Con.Run("clients", "-u", UserName, "-e", ClientName);
						foreach(P4Record record in P4Output)
						{
							if(record["Host"].ToLower() == Environment.MachineName.ToLower())
							{
                                RetVal = record["Root"].Replace("/", "\\"); ;
							}
						}
					}
					catch(Exception Ex)
					{
						Console.WriteLine("Exception: " + Ex.ToString());
					}
				}
			}
			// Returns the client spec root for a provided clientspec if there is a valid connection.
			return RetVal;
		}


        /// <summary>
        /// Used to obtain a list of users from the Perforce server.
        /// </summary>
        /// <returns>Returns a P4RecordSet with the user info or null if no user info found. </returns>
        public P4RecordSet GetConnectionUsers()
        {
            //@todo For some server configs this does not work, we can not seem to browse the user list unless we have entered a valid user name with permissions.

            P4RecordSet P4Output = null;
            if (P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(false, false))
            {
                bool Retry = false;
                try
                { 
                    P4Output = P4ConInfo.P4Con.Run("users");
                }
                catch (Exception Ex)
                {
                    Console.WriteLine("Exception: " + Ex.ToString());
                    Retry = true;
                }

                if (Retry)
                {
                    P4ConInfo.User = string.Empty;
                    P4ConInfo.P4Init();
                    try
                    {
                        P4Output = P4ConInfo.P4Con.Run("users");
                    }
                    catch (Exception Ex)
                    {
                        Console.WriteLine("Exception: " + Ex.ToString());
                    }
                }
            }

            return P4Output;
        }


		/// <summary>
		/// Used to obtain a list of workspaces on the Perforce server.
		/// </summary>
		/// <param name="bUserOnly">if set to <c>true</c> this will limit the returned workspaces to those belonging to the current user.</param>
		/// <param name="bContainsCurrentProjOnly">if set to <c>true</c> this will limit the returned workspaces to those that include the current project in their mapping.</param>
		/// <returns>Key/Value pairs where the key represents the client name and the value represents the workspace root as obtained form the 'p4 clients' command. </returns>
		public Dictionary<string, string> GetConnectionWorkspaces(bool bUserOnly, bool bContainsCurrentProjOnly)
		{
			Dictionary<string, string> Workspaces = new Dictionary<string, string>();

			if (P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(false, false))
			{
				try
				{
					string Host = P4ConInfo.P4Con.Host;
					P4RecordSet P4Output = null;
					if(bUserOnly)
					{
						// Make sure user is logged in
						if(P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(true, false))
						{
							string UserName = P4ConInfo.P4Con.User;
							P4Output = P4ConInfo.P4Con.Run("clients", "-u", UserName);
						}
					}
					else
					{
						P4Output = P4ConInfo.P4Con.Run("clients");
					}

					if(P4Output != null)
					{
						foreach(P4Record record in P4Output)
						{
							if(record["Host"].ToLower() == Host.ToLower())
							{
								bool bAddIt = true;
								if (bContainsCurrentProjOnly)
								{
									string ConnWSRoot = Program.Util.GetClientRoot(P4ConInfo.P4Con.User, record["client"]);

									bool bIsProjectUnderWorkspaceRoot = (!string.IsNullOrEmpty(ConnWSRoot) && Program.Util.IsSubDirectory(new DirectoryInfo(Utils.GetProjectRoot()), new DirectoryInfo(ConnWSRoot)));

									if (!bIsProjectUnderWorkspaceRoot)
									{
										bAddIt = false;
									}
								}
								
								if (bAddIt)
								{
									Workspaces.Add(record["client"], record["Root"]);
								}
							}
						}
					}

				}
				catch (Exception Ex)
				{
					Console.WriteLine("Exception: " + Ex.ToString());
				}
			}

			return Workspaces;
		}


		/// <summary>
		/// Checks to see if a specific workspace exists on the Perforce Server.
		/// </summary>
		/// <param name="WorkspaceName">Name of the workspace to check for.</param>
		/// <returns><c>true</c> if the workspace exists, or <c>false</c> otherwise</returns>
		public bool WorkspaceExists(string WorkspaceName)
		{
			if (WorkspaceName == string.Empty)
			{
				return false;
			}

			bool bWorkspaceExistsAlready = false;

			if(P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(true, false))
			{
				Dictionary<string, string> Workspaces = Program.Util.GetConnectionWorkspaces(false, false);


				bool bIsServerCaseSensitive = P4ConInfo.P4Con.IsServerCaseSensitive();
				foreach(KeyValuePair<string, string> Workspace in Workspaces)
				{
					if(!bIsServerCaseSensitive && Workspace.Key.ToLower() == WorkspaceName.ToLower())
					{
						bWorkspaceExistsAlready = true;
					}
					else if(Workspace.Key == WorkspaceName)
					{
						bWorkspaceExistsAlready = true;
					}
				}
			}
			return bWorkspaceExistsAlready;
		}


		/// <summary>
		/// Creates a new workspace on the perforce server if one with the provided name does not exist already.
		/// </summary>
		/// <param name="WorkspaceName">Name of the workspace.</param>
		/// <param name="WorkspaceRoot">The workspace local root directory.</param>
		/// <returns><c>true</c> if the workspace was created on the server, or <c>false</c> otherwise</returns>
		public bool CreateNewWorkspace(string WorkspaceName, string WorkspaceRoot)
		{
			if (WorkspaceName == string.Empty || WorkspaceRoot == string.Empty)
			{
				return false;
			}
		
			if (P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(true, false))
			{
				if(WorkspaceExists(WorkspaceName))
				{
					// We bail early if the workspace already exists.
					return false;
				}

				try
				{
					string Client = WorkspaceName;
					string Owner = P4ConInfo.P4Con.User;
					string Host = P4ConInfo.P4Con.Host;
					string Root = WorkspaceRoot;
					string[] View = {@"//depot/... //" + WorkspaceName + "/..."};

					P4Form WorkspaceForm = P4ConInfo.P4Con.Fetch_Form("client");
					WorkspaceForm.Fields["Client"] = Client;
					WorkspaceForm.Fields["Owner"] = Owner;
					WorkspaceForm.Fields["Host"] = Host;
					WorkspaceForm.Fields["Root"] = Root;

					WorkspaceForm.ArrayFields["View"] = View;

					P4ConInfo.P4Con.Save_Form(WorkspaceForm);

				}
				catch (Exception Ex)
				{
					Console.WriteLine("Exception: " + Ex.ToString());
					return false;
				}

				return true;
			}
			return false;
		}

        /// <summary>
        /// Creates a new user on the perforce server if one with the provided name does not exist already.
        /// </summary>
        /// <param name="UserName">User name.</param>
        /// <param name="FullName">The full name or description of the user.</param>
        /// <param name="Password">Password for the user.</param>
        /// <param name="Email">User Email address.</param>
        /// <returns><c>true</c> if the user was created on the server, or <c>false</c> otherwise</returns>
        public bool CreateNewUser(string UserName, string FullName, string Password, string Email)
        {
            if (UserName == string.Empty)
            {
                return false;
            }

            if (P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(false, false))
            {
                if (UserExists(UserName))
                {
                    // We bail early if the user name already exists.
                    return false;
                }

                P4ConInfo.User = UserName;
                P4ConInfo.P4Init();

                if (P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(true, false) && UserExists(UserName))
                {
                    try
                    {
                        P4Form UserForm = P4ConInfo.P4Con.Fetch_Form("user");
                        //UserForm.Fields["User"] = UserName;
                        if(FullName != string.Empty) UserForm.Fields["FullName"] = FullName;
                        if (Password != string.Empty) UserForm.Fields["Password"] = Password;
                        if (Email != string.Empty) UserForm.Fields["Email"] = Email;

                        P4ConInfo.P4Con.Save_Form(UserForm);

                    }
                    catch (Exception Ex)
                    {
                        Console.WriteLine("Exception: " + Ex.ToString());
                        return false;
                    }
                    return true;
                }
            }
            return false;
        }


		/// <summary>
		/// Revert any pending changelists that are specific to the P4PopulateDepot utility and revert any files that they contain.
		/// </summary>
		public void RemovePendingChangelists()
		{
			try
			{
				P4RecordSet P4Output = null;
				UpdateProgressBar(string.Empty, GetPhrase("PROGRESSCLSeach"));

				// Get the list of pending changes.  Make sure we don't have any pending changelists from a previous tool run.  If they do exist, we will clean it up.
				P4Output = P4ConInfo.P4Con.Run("changes", "-l", "-s", "pending", "-u", P4ConInfo.P4Con.User, "-c", P4ConInfo.P4Con.Client);

				if (P4Output != null)
				{
					foreach (P4Record record in P4Output)
					{
						if (record["desc"].StartsWith(CLNamePrepare))
						{
							// Check to see if there are any files in the CL to revert
							P4RecordSet FilesInExistingCl = P4ConInfo.P4Con.Run("opened", "-m", "1", "-c", record["change"]);
							if (FilesInExistingCl != null && FilesInExistingCl.Records.Length > 0)
							{
								UpdateProgressBar(string.Empty, GetPhrase("PROGRESSCLRevertFiles"));
								// Revert all files in the changelist
								P4ConInfo.P4Con.Run("revert", "-c", record["change"], "//depot/...");
							}

							// Delete the changelist
							UpdateProgressBar(string.Empty, GetPhrase("PROGRESSCLDelete"));
							P4ConInfo.P4Con.Run("change", "-d", record["change"]);
						}
					}
				}
			}
			catch (Exception Ex)
			{
				Console.WriteLine("Exception: " + Ex.ToString());
			}
		}


		/// <summary>
		/// This function will handle the submit step for this utility.  It will manage changelists, add files, set file flags, and perform 
		/// the submit.  This function will warn via dialog boxes and cleanup on failures if it is able.
		/// </summary>
		/// <returns><c>true</c> if the submit succeeded, or <c>false</c> otherwise</returns>
		public bool SubmitFiles()
		{
			// Get the game info so we can pull out the game name when needed.
			Utils.GameManifestOptions Game = Program.Util.UDKSettings.GameInfo[0];

			// Used to set the p4 file types properly
			Dictionary<string, string> FileTypeFlags = new Dictionary<string, string>();
			FileTypeFlags.Add(".upk", "binary+l");
			FileTypeFlags.Add(".fla", "binary+l");
			FileTypeFlags.Add(".bik", "binary+l");
			FileTypeFlags.Add(".png", "binary+l");
			FileTypeFlags.Add(".jpg", "binary+l");
			FileTypeFlags.Add(".gif", "binary+l");
			FileTypeFlags.Add(".bmp", "binary+l");
			FileTypeFlags.Add(".ico", "binary+l");
			FileTypeFlags.Add(".u", "binary+w");
			FileTypeFlags.Add(".swf", "binary");
			FileTypeFlags.Add(".usf", "text");

			UpdateProgressBar(GetPhrase("PROGRESSChecking"), string.Empty);

			if (P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(true, true))
			{
				try
				{
					// This section checks for an existing CLs with our description and removes them.
					{
						RemovePendingChangelists();
					}

					P4PendingChangelist ChangeList = null;
					// This section creates our pending CL that we will be using
					{
						UpdateProgressBar(GetPhrase("PROGRESSPrepare"), GetPhrase("PROGRESSCLCreate"));
						ChangeList = P4ConInfo.P4Con.CreatePendingChangelist(CLNamePrepare);
					}

					// This section loops through all the entries in the folder structure and opens them for add
					{
						List<string> Files = GetAllManifestFilePaths();
						UpdateProgressBar(string.Empty, GetPhrase("PROGRESSCLAddFiles"));
						foreach (string FilePath in Files)
						{
							FileInfo LocalFileInfo = new FileInfo(FilePath);

							if (LocalFileInfo.DirectoryName.EndsWith("Config") && LocalFileInfo.Name.StartsWith(Game.Name))
							{
								// We make a special case for config files that start with the game name, these are generated and we
								//  do not want to check them into source control.
								continue;
							}

							P4RecordSet P4AddOutput;
							string Flags = string.Empty;

							if (FileTypeFlags.ContainsKey(LocalFileInfo.Extension.ToLower()) && !LocalFileInfo.Name.StartsWith("RefShaderCache"))
							{
								Flags = FileTypeFlags[LocalFileInfo.Extension.ToLower()];
							}
							else if (LocalFileInfo.DirectoryName.Contains("\\Localization\\") && LocalFileInfo.Extension.ToLower().EndsWith(LocalFileInfo.Directory.Name.ToLower()))
							{
								Flags = "utf16";
							}

							if (FileTypeFlags.ContainsKey(LocalFileInfo.Extension) && !LocalFileInfo.Name.StartsWith("RefShaderCache"))
							{
								P4AddOutput = P4ConInfo.P4Con.Run("add", "-f", "-c", ChangeList.Number.ToString(), "-t", FileTypeFlags[LocalFileInfo.Extension], FilePath);
							}
							else
							{
								P4AddOutput = P4ConInfo.P4Con.Run("add", "-f", "-c", ChangeList.Number.ToString(), FilePath);
							}
							
							bool bIsKnownErrorCase = false;
							string ErrorString = null;
							if (P4AddOutput == null)
							{
								bIsKnownErrorCase = true;
							}
							else if (P4AddOutput.Messages.Length > 0)
							{
								foreach (string AMessage in P4AddOutput.Messages)
								{
									if (AMessage.Contains("use 'reopen'") || AMessage.Contains("currently opened for"))
									{
										ErrorString = GetPhrase("ERRFileInCL") + FilePath;
										bIsKnownErrorCase = true;
										break;
									}
									else if(AMessage.Contains("can't add existing file"))
									{
										ErrorString = GetPhrase("ERRFileExistsInDepot") + FilePath;
										bIsKnownErrorCase = true;
										break;
									}
								}
							}

							if (bIsKnownErrorCase == true)
							{
								string MBText = ErrorString;
								string MBCap = GetPhrase("ERRError");
								MessageBox.Show(new Form() { TopMost = true }, MBText, MBCap, MessageBoxButtons.OK, MessageBoxIcon.Error);

								RemovePendingChangelists();
								return false;
							}
						}
					}

					// Finally we submit the changelist
					{
                        UpdateProgressBar(GetPhrase("PROGRESSSubmitTitle"), GetPhrase("PROGRESSTimeDesc"));

						P4UnParsedRecordSet P4SubmitOutput;
						P4SubmitOutput = ChangeList.Submit();
						if (!(	P4SubmitOutput != null && 
								P4SubmitOutput.Errors.Length == 0 && 
								P4SubmitOutput.Messages.Length > 0 &&
								P4SubmitOutput.Messages[P4SubmitOutput.Messages.Length - 1].EndsWith("submitted."))
							)
						{
							string MBText = GetPhrase("ERRGenericSubmit");
							string MBCap = GetPhrase("ERRError");

							MessageBox.Show(new Form() { TopMost = true }, MBText, MBCap, MessageBoxButtons.OK, MessageBoxIcon.Error);
							RemovePendingChangelists();
							return false;
						}
					}
				}
				catch (Exception Ex)
				{
					Console.WriteLine("Exception: " + Ex.ToString());
					string MBText = GetPhrase("ERRGenericSubmit");
					string MBCap = GetPhrase("ERRError");
					
					MessageBox.Show(new Form() { TopMost = true }, MBText, MBCap, MessageBoxButtons.OK, MessageBoxIcon.Error);
					RemovePendingChangelists();

					return false;
				}
			}
			else
			{
				string MBText = GetPhrase("ERRLostConn");
				string MBCap = GetPhrase("ERRError");
				MessageBox.Show(new Form() { TopMost = true }, MBText, MBCap, MessageBoxButtons.OK, MessageBoxIcon.Error);
				return false;
			}

			return true;
		}


		/// <summary>
		/// Uses the 'where' Perforce command to obtain the local folder mapping for the valid connection's workspace.
		/// </summary>
		/// <returns>A strin representing the workspace mapping, or null if the operation failed</returns>
		public string GetClientFolderMapping()
		{
			string P4Mapping = string.Empty;
			P4RecordSet P4Output = null;
			if (P4ConInfo.P4Con != null && P4ConInfo.P4Con.IsValidConnection(true, true))
			{
				try
				{
					P4Output = P4ConInfo.P4Con.Run("where", Path.Combine(GetProjectRoot(), "..."));
					if (P4Output != null && P4Output.Records.Length>0)
					{
						P4Mapping = P4Output[0]["depotFile"];
					}
				}
				catch (Exception)
				{
				}
			}
			return P4Mapping;
		}
	}
}
