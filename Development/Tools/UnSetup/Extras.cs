/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;


namespace UnSetup
{
	public partial class Utils
	{

		/// <summary>
		/// Determines if the passed in address is local to this machine.
		/// </summary>
		/// <param name="host">The address to check.  This can be an IP, 'localhost', or DNS lookup.</param>
		/// <returns>
		///   <c>true</c> if the passed in address points to the local machine; otherwise, <c>false</c>.
		/// </returns>
		public bool IsLocalAddress(string host)
		{
			try
			{
				// Get host IP addresses
				IPAddress[] hostIPs = Dns.GetHostAddresses(host);

				// Get local IP addresses
				IPAddress[] localIPs = Dns.GetHostAddresses(Dns.GetHostName());

				foreach (IPAddress hostIP in hostIPs)
				{
					// Check if it is localhost
					if (IPAddress.IsLoopback(hostIP))
					{
						return true;
					}

					// Check if it matches local addresses
					foreach (IPAddress localIP in localIPs)
					{
						if (hostIP.Equals(localIP))
						{
							return true;
						}
					}
				}
			}
			catch (Exception E)
			{
				Console.WriteLine(E.Message);
			}

			return false;
		}

		/// <summary>
		/// Searches a passed in string and finds the first instance of a substring that is wrapped in two arbitrary passed in delimiters.
		/// </summary>
		/// <param name="InputString">The input string to search.</param>
		/// <param name="LeftDelimiter">The delimiter that appears on the left side of the desired substring.</param>
		/// <param name="RightDelimiter">The delimiter that appears on the right side of the desired substring.</param>
		/// <returns>The method returns the first instance of a substring between the provided delimiters or the empty string if no match is found.</returns>
		public string GetDelimitedSubtring(string InputString, string LeftDelimiter, string RightDelimiter)
		{
			string result = string.Empty;
			Regex expression = new Regex(Regex.Escape(LeftDelimiter) + "(.*?)" + Regex.Escape(RightDelimiter));
			Match match = expression.Match(InputString);
			if (match.Success)
			{
				result = match.Groups[1].Value;
			}
			return result;
		}

		/// <summary>
		/// Searches the folders specified in the Windows environment PATH for env path for a file .
		/// </summary>
		/// <param name="SearchFile">The file name to search for.</param>
		/// <returns>The method returns a string representing the full path for the first matching file that is found in the PATH or the empty string if no matches were found. </returns>
		public string SearchEnvPath(string SearchFile)
		{

			string SysPath = Environment.GetEnvironmentVariable("PATH", EnvironmentVariableTarget.Machine);
			foreach (string CheckPath in SysPath.Split(';'))
			{
				string FullPath = string.Empty;
				try
				{
					// We try to build a file path based on the tokens we find in the PATH environment variable.  If Combine throws an exception 
					//  due to invalid characters or for any reason we will skip it.
					FullPath = Path.Combine(CheckPath, SearchFile);
				}
				catch(Exception)
				{}

				if (FullPath != string.Empty && File.Exists(FullPath))
				{
					return FullPath;
				}
			}
			return string.Empty;

		}

		/// <summary>
		/// Gets the Perforce Visual Client version number from the p4v.exe ProductVersion info.
		/// </summary>
		/// <returns>This method returns a the perforce visual client(p4v.exe) product version. </returns>
		public string GetP4ClientVersion()
		{
			// Here we search for the p4v install in the windows path so we can extract the product
			//  version from the executable.
			string PerforceClientPath = SearchEnvPath("p4v.exe");

			if (PerforceClientPath == String.Empty)
			{
				// We could not find p4v in the PATH so we return the empty string.
				return String.Empty;
			}

			return GetFileProductVersion(PerforceClientPath);
		}

		/// <summary>
		/// Gets the locally installed Perforce Server version.  If no install is detected locally it will try
		/// to retrieve remote server info.
		/// </summary>
		/// <returns>
		/// Returns the server version info for local server, will fall back to remote server version if p4d.exe is
		/// not found locally.  If neither a local server or remote server are detected, the empty string will be returned.
		/// </returns>
		public string GetP4ServerVersion()
		{
			// Search for the p4d.exe in the windows path so we can extract the product
			//  version from the executable.
			string PerforceClientPath = SearchEnvPath("p4d.exe");

			if (PerforceClientPath != String.Empty)
			{
				// If we found p4d, we return the file product version.
				return GetFileProductVersion(PerforceClientPath);
			}

			// If we could not find p4d installed locally, we'll check to see if an active connection is setup and
			//  try to retrieve info about the server on the remote machine.
			string ServerVersion;
			string ServerAddress;
			string ServerRoot;
			GetP4ServerInfo(out ServerVersion, out ServerAddress, out ServerRoot);

			// When we get the server version via p4 info, it looks like this so we need to extract the major.minor number
			//  which we assume is at the 2nd index if we were to split on '/': P4D/NTX86/2012.1/459601 (2012/05/09)
			string[] SplitVersion = ServerVersion.Split('/');
			if (SplitVersion.Length >= 3)
			{
				return SplitVersion[2];
			}

			// If we can't find server info we return the empty string and assume it is not installed.
			return string.Empty;

		}

		/// <summary>
		/// Gets information about a setup Perforce Server connection using the "P4 Info" commandline command.
		/// </summary>
		/// <param name="ServerVersion">The "Server version" returned by the p4 Info command or the empty string if no connection detected.</param>
		/// <param name="ServerAddress">The "Server address" returned by the p4 Info command or the empty string if no connection detected.</param>
		/// <param name="ServerRoot">The "Server root" returned by the p4 Info command or the empty string if no connection detected.</param>
		public void GetP4ServerInfo(out string ServerVersion, out string ServerAddress, out string ServerRoot)
		{
			ServerVersion = string.Empty;
			ServerAddress = string.Empty;
			ServerRoot = string.Empty;

			StringBuilder ProcessOutput = new StringBuilder();
			try
			{
				Process P4InfoProcess = new Process();
				P4InfoProcess.StartInfo.FileName = "p4";
				P4InfoProcess.StartInfo.Arguments = "info -s";
				P4InfoProcess.StartInfo.CreateNoWindow = true;
				P4InfoProcess.StartInfo.UseShellExecute = false;
				P4InfoProcess.StartInfo.RedirectStandardOutput = true;
				P4InfoProcess.StartInfo.RedirectStandardError = false;

				P4InfoProcess.OutputDataReceived += (o, e) => ProcessOutput.AppendLine(e.Data);
				P4InfoProcess.Start();
				P4InfoProcess.BeginOutputReadLine();

				// When the server connection is not available, the default operation time out is
				//  roughly 15 seconds.  Since we don't want to make the user wait that long we add a
				//  more reasonable timeout on our process and assume no connection if we don't get a response
				//  within this time.
				P4InfoProcess.WaitForExit(5000);
				if (!P4InfoProcess.HasExited)
				{
					P4InfoProcess.Kill();
				}

			}
			catch (Exception)
			{
			}

			if (ProcessOutput.Length > 0)
			{
				ServerVersion = GetDelimitedSubtring(ProcessOutput.ToString(), "Server version: ", "\r\n");
				ServerAddress = GetDelimitedSubtring(ProcessOutput.ToString(), "Server address: ", "\r\n");
				ServerRoot = GetDelimitedSubtring(ProcessOutput.ToString(), "Server root: ", "\r\n");
			}
		}

		/// <summary>
		/// Gets the version info for a particular perforce installer from its path.
		/// </summary>
		/// <param name="Path">The path to the perforce directory containing the installers. ex: (RootInstallPath)\Binaries\Redist\Perforce\Client2012.1</param>
		/// <param name="Installer">The installer type "Client" or "Server".</param>
		/// <param name="MajorVersion">The major version that was parsed from the path or the empty string if no match found.</param>
		/// <param name="MinorVersion">The minor version that was parsed from the path or the empty string if no match found.</param>
		public void GetVersionFromPath(string Path, string Installer, out int MajorVersion, out int MinorVersion)
		{
			string RegexPattern = Installer + "(?<MajorVer>[0-9]+).(?<MinorVer>[0-9]+)";
			Regex VersionRegex = new Regex(RegexPattern, RegexOptions.IgnoreCase);
			Match Matches = VersionRegex.Match(Path);
			if (Matches.Success)
			{
				MajorVersion = int.Parse(Matches.Groups["MajorVer"].Value);
				MinorVersion = int.Parse(Matches.Groups["MinorVer"].Value);
			}
			else
			{
				MajorVersion = -1;
				MinorVersion = -1;
			}

		}

		/// <summary>
		/// Gets the appropriate OS specific name of the p4 installer based on the passed in installer type.
		/// </summary>
		/// <param name="Installer">The installer type to get the exe name for: "Client" or "Server".</param>
		/// <returns>Returns the x64 or x86 version of the installer name based on OS.</returns>
		public string GetP4InstallerExeName(string Installer)
		{
			if (Installer == "Client")
			{
				if (Environment.Is64BitOperatingSystem)
				{
					return "P4vinst64.exe";

				}
				else
				{
					return "P4vinst.exe";
				}
			}
			else
			{
				if (Environment.Is64BitOperatingSystem)
				{
					return "Perforce64.exe";

				}
				else
				{
					return "Perforce.exe";
				}
			}
		}

		/// <summary>
		/// Gets the p4 installer path for the newest detected version of the specified installer type.
		/// </summary>
		/// <param name="InstallRootFolder">The install root folder.</param>
		/// <param name="Installer">The installer type to get the path for: "Client" or "Server".</param>
		/// <returns>Returns the full path to the latest detected version of the specified installer or the empty string if not found.</returns>
		public string GetP4InstallerPath(string InstallRootFolder, string Installer)
		{
			if (String.IsNullOrEmpty(InstallRootFolder) || String.IsNullOrEmpty(Installer))
			{
				return string.Empty;
			}

			string PerforceDir = Path.Combine(InstallRootFolder, "Binaries\\Redist\\Perforce");

			if (!Directory.Exists(PerforceDir))
			{
				return string.Empty;
			}

			// Loop through all the folders in the directory and looking for the directory "<Installer>YYYY.Z" with the highest version number.  YYYY represents the major
			//  version number and Z represents the minor version number.ex. Client2012.1
			string SearchPattern = Installer + "*";
			string[] InstallDirs = Directory.GetDirectories(PerforceDir, SearchPattern);

			int MajorVersionNum = -1;
			int MinorVersionNum = -1;
			string InstallerPath = String.Empty;

			foreach (string Dir in InstallDirs)
			{
				int CurMajorVersion;
				int CurMinorVersion;
				GetVersionFromPath(Dir, Installer, out CurMajorVersion, out CurMinorVersion);


				if (CurMajorVersion > MajorVersionNum)
				{
					InstallerPath = Dir;
					MajorVersionNum = CurMajorVersion;
					MinorVersionNum = CurMinorVersion;
				}
				else if (CurMajorVersion == MajorVersionNum && CurMinorVersion > MinorVersionNum)
				{
					InstallerPath = Dir;
					MajorVersionNum = CurMajorVersion;
					MinorVersionNum = CurMinorVersion;
				}
			}

			if (InstallerPath != string.Empty)
			{
				InstallerPath = Path.Combine(InstallerPath, GetP4InstallerExeName(Installer));
			}


			return InstallerPath;
		}

		/// <summary>
		/// Checks to see if the Perforce visual client installer is a newer version than the currently installed client.
		/// </summary>
		/// <param name="InstallRootFolder">The install root folder.</param>
		/// <returns>
		///   <c>true</c> if the client installer in the install folder is newer than current installed client; otherwise, <c>false</c>.
		/// </returns>
		public bool DoesP4ClientNeedUpgrade(string InstallRootFolder)
		{
			string P4ClientInstallerPath = GetP4InstallerPath(InstallRootFolder, "Client");

			// We could not find the installer path so we can't perform upgrade
			if (!File.Exists(P4ClientInstallerPath))
			{
				return false;
			}

			// Find the installer version numbers
			int InstallerMajorVer;
			int InstallerMinorVer;
			GetVersionFromPath(P4ClientInstallerPath, "Client", out InstallerMajorVer, out InstallerMinorVer);

			// Next get the version for the installed client
			string CurrentClientVer = GetP4ClientVersion();

			// We will get a product version like this where the first two numbers represent the major and minor version number: "2012.1.47.5402"
			string[] SplitVersions = CurrentClientVer.Split('.');
			if (SplitVersions.Length < 2)
			{
				return false;
			}
			int CurrentMajorVer = int.Parse(SplitVersions[0]);
			int CurrentMinorVer = int.Parse(SplitVersions[1]);

			// If the installer version is newer than the currently installed client, we know we need an upgrade.
			if ((InstallerMajorVer > CurrentMajorVer) ||
				(InstallerMajorVer == CurrentMajorVer && InstallerMinorVer > CurrentMinorVer))
			{
				return true;
			}

			return false;
		}

		/// <summary>
		/// Checks to see if the local Perforce server, or active connection perforce server is older than the version of the perforce installer.
		/// </summary>
		/// <param name="InstallRootFolder">The install root folder.</param>
		/// <returns>
		///   <c>true</c> if the server installer is newer than current installed server or active connection server; otherwise, <c>false</c>.
		/// </returns>
		public bool DoesP4ServerNeedUpgrade(string InstallRootFolder)
		{
			string P4ClientInstallerPath = GetP4InstallerPath(InstallRootFolder, "Server");

			// We could not find the installer path so we can't perform upgrade
			if (!File.Exists(P4ClientInstallerPath))
			{
				return false;
			}

			// Find the installer version numbers
			int InstallerMajorVer;
			int InstallerMinorVer;
			GetVersionFromPath(P4ClientInstallerPath, "Server", out InstallerMajorVer, out InstallerMinorVer);

			// Next, get the version for the installed server, or the active connection server
			string CurrentServerVer = GetP4ServerVersion();

			string[] SplitVersions = CurrentServerVer.Split('.');
			if (SplitVersions.Length < 2)
			{
				return false;
			}

			int CurrentMajorVer = int.Parse(SplitVersions[0]);
			int CurrentMinorVer = int.Parse(SplitVersions[1]);

			// If the installer version is newer than the currently installed client, we know we need an upgrade.
			if ((InstallerMajorVer > CurrentMajorVer) ||
				(InstallerMajorVer == CurrentMajorVer && InstallerMinorVer > CurrentMinorVer))
			{
				return true;
			}

			return false;
		}

		/// <summary>
		/// This method is used to get a file product version.
		/// </summary>
		/// <param name="FilePath">The file path.</param>
		/// <returns>String representing the file product version or the empty string if the file is not found.</returns>
		public string GetFileProductVersion(string FilePath)
		{
			string FileProductVersion = String.Empty;
			if (File.Exists(FilePath))
			{
				FileVersionInfo VersionInfo = FileVersionInfo.GetVersionInfo(FilePath);
				FileProductVersion = VersionInfo.ProductVersion;
			}
			return FileProductVersion;
		}
	}
}
