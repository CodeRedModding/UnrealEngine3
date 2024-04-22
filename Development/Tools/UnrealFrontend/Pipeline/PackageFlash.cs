/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Color = System.Drawing.Color;
using System.IO;
using System.Threading;

namespace UnrealFrontend.Pipeline
{
	public class PackageFlash : Pipeline.CommandletStep
	{
		public override bool SupportsClean { get { return false; } }

		public override string StepName { get { return "Package"; } }

		public override String StepNameToolTip { get { return "Package Flash swf."; } }

		public override bool SupportsReboot { get { return false; } }

		public override String ExecuteDesc { get { return "Package Flash swf"; } }

		public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
		{
			return PackageFlash_Internal( ProcessManager, InProfile );
		}

		public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
		{
			Session.Current.SessionLog.AddLine(Color.DarkMagenta, "PackageFlash does not support Clean and Execute.");
			return true;
		}

		public String GetCookerOption(Profile InProfile, String InSettingName, String InDefaultValue)
		{
			String[] SplitSettings = InProfile.Cooking_AdditionalOptions.Split(' ');
			foreach (String Option in SplitSettings)
			{
				if (Option.StartsWith(InSettingName))
				{
					String OptionValue = Option.Substring(InSettingName.Length + 1);
					return OptionValue;
				}
			}
			return InDefaultValue;
		}

		bool PackageFlash_Internal(IProcessManager ProcessManager, Profile InProfile)
		{
			ConsoleInterface.TOCSettings BuildSettings = Pipeline.Sync.CreateTOCSettings(InProfile, false, Profile.Languages.All);
			if( BuildSettings == null )
			{
				return false;
			}

			string CommandLine;
			string ExecutablePath = "";
			bool bSuccess = true;

			ConsoleInterface.Platform CurPlat = InProfile.TargetPlatform;
			string PlatformName = CurPlat.Type.ToString();

			// Write the UE3CommandLine.txt file
			if (!Pipeline.Sync.UpdateMobileCommandlineFile(InProfile))
			{
				return false;
			}
			// Initalize a few system variables
            string AlchemySDK = "c:\\alchemy";
			if( Environment.GetEnvironmentVariable( "ALCHEMY_ROOT" ) != null )
			{
				AlchemySDK = Environment.GetEnvironmentVariable( "ALCHEMY_ROOT" );
			}

			string JavaSDK = Path.Combine( AlchemySDK, "jdk1.7.0" );
			if( Environment.GetEnvironmentVariable( "JAVA_ROOT" ) != null )
			{
				JavaSDK = Environment.GetEnvironmentVariable( "JAVA_ROOT" );
			}

            string AddPath = Path.Combine( AlchemySDK, "sdk\\usr\\bin" );
            AddPath += ";" + Path.Combine( JavaSDK, "bin" );
            AddPath += ";" + Path.Combine( AlchemySDK, "cygwin\\bin");
			Environment.SetEnvironmentVariable( "PATH", AddPath + ";%PATH%" );

			// Set up the location to build the VFS
			string BuildRoot = "..\\" + BuildSettings.GameName + "\\Build\\" + PlatformName + "\\";
			string WebsiteRoot = BuildRoot + "Website\\";
			try
			{
				if( Directory.Exists( WebsiteRoot ) )
				{
					Directory.Delete( Path.GetFullPath( WebsiteRoot ), true );
				}
			}
			catch (System.Exception ex)
			{
				Console.WriteLine(ex.ToString());				
			}
			Directory.CreateDirectory( WebsiteRoot );
	
			// Sync necessary content to a raw VFS directory
			if (!Pipeline.Sync.RunCookerSync(ProcessManager, InProfile, false, false))
			{
				return false;
			}
			ProcessManager.WaitForActiveProcessesToComplete();

			// Clean up the cooked content prior to packaging

			// Move staging files into place
			if( bSuccess )
			{
				try
				{
					string SrcExecutableName = string.Format( "Flash\\{0}-{1}-{2}.swf", BuildSettings.GameName, PlatformName, GetMobileConfigurationString( InProfile.LaunchConfiguration ) );
					string DstExecutableName = string.Format( BuildRoot + "Website\\{0}.swf", BuildSettings.GameName );
					FileInfo MiscFileInfo;
					if( File.Exists( SrcExecutableName ) )
					{
						// Files needed for the final website
						File.Copy( SrcExecutableName, DstExecutableName, true );
						File.SetAttributes( DstExecutableName, File.GetAttributes( DstExecutableName ) & ~FileAttributes.ReadOnly );

                        File.Copy(BuildRoot + "expressInstall.swf", BuildRoot + "Website\\expressInstall.swf", true);
                        MiscFileInfo = new FileInfo(BuildRoot + "Website\\expressInstall.swf");
                        MiscFileInfo.IsReadOnly = false;

                        Directory.CreateDirectory(BuildRoot + "Website\\js");

                        File.Copy(BuildRoot + "js\\swfobject.js", BuildRoot + "Website\\js\\swfobject.js", true);
                        MiscFileInfo = new FileInfo(BuildRoot + "Website\\js\\swfobject.js");
                        MiscFileInfo.IsReadOnly = false;

						File.Copy( BuildRoot + "Bootstrap.swf", BuildRoot + "Website\\Bootstrap.swf", true );
						MiscFileInfo = new FileInfo( BuildRoot + "Website\\Bootstrap.swf" );
						MiscFileInfo.IsReadOnly = false;

                        File.Copy(BuildRoot + BuildSettings.GameName + "Preloader.swf", BuildRoot + "Website\\" + BuildSettings.GameName + "Preloader.swf", true);
                        MiscFileInfo = new FileInfo(BuildRoot + "Website\\" + BuildSettings.GameName + "Preloader.swf");
                        MiscFileInfo.IsReadOnly = false;

                        File.Copy(BuildRoot + "Splash.swf", BuildRoot + "Website\\Splash.swf", true);
                        MiscFileInfo = new FileInfo(BuildRoot + "Website\\Splash.swf");
                        MiscFileInfo.IsReadOnly = false;
						
						File.Copy( BuildRoot + BuildSettings.GameName + ".html", BuildRoot + "Website\\" + BuildSettings.GameName + ".html", true );
						MiscFileInfo = new FileInfo( BuildRoot + "Website\\" + BuildSettings.GameName + ".html" );
						MiscFileInfo.IsReadOnly = false;
					}
					else
					{
						bSuccess = false;
					}
				}
				catch (System.Exception ex)
				{
					Console.WriteLine(ex.ToString());				
				}
			}
			// Create VFS files
			if( bSuccess )
			{
				try
				{
                    ExecutablePath = ".\\Flash\\FlashPackager.exe";

                    Console.WriteLine("ExecutablePath: " + ExecutablePath);
                    
                    CommandLine = string.Format("genfs --type=http FlashBuild FlashVFS VFS");
					bSuccess = ProcessManager.StartProcess( ExecutablePath, CommandLine, WebsiteRoot, InProfile.TargetPlatform );
					if( bSuccess )
					{
						bSuccess = ProcessManager.WaitForActiveProcessesToComplete();
					}
				}
				catch (System.Exception ex)
				{
					Console.WriteLine(ex.ToString());				
				}
			}
			
            // Uncompress the main SWF
            if (bSuccess)
            {
                try
                {
                    ExecutablePath = "Flash\\FlashPackager.exe";
                    CommandLine = string.Format("swfcompress -u {0}.swf", BuildSettings.GameName);
                    bSuccess = ProcessManager.StartProcess(ExecutablePath, CommandLine, WebsiteRoot, InProfile.TargetPlatform);
                    if (bSuccess)
                    {
                        bSuccess = ProcessManager.WaitForActiveProcessesToComplete();
                    }
                }
                catch (System.Exception ex)
                {
                    Console.WriteLine(ex.ToString());
                }
            }

            // Up the script stuck timeout in the main swf
            if (bSuccess)
            {
                try
                {
                    ExecutablePath = "Flash\\FlashPackager.exe";
                    CommandLine = string.Format("removescripttimeout {0}.swf {0}.swf", BuildSettings.GameName);
                    bSuccess = ProcessManager.StartProcess(ExecutablePath, CommandLine, WebsiteRoot, InProfile.TargetPlatform);
                    if (bSuccess)
                    {
                        bSuccess = ProcessManager.WaitForActiveProcessesToComplete();
                    }
                }
                catch (System.Exception ex)
                {
                    Console.WriteLine(ex.ToString());
                }
            }

            // Compress the main SWF
            if (bSuccess)
            {
                try
                {
                    ExecutablePath = "Flash\\FlashPackager.exe";
                    CommandLine = string.Format("swfcompress -c -v 17 {0}.swf", BuildSettings.GameName);
                    bSuccess = ProcessManager.StartProcess(ExecutablePath, CommandLine, WebsiteRoot, InProfile.TargetPlatform);
                    if (bSuccess)
                    {
                        bSuccess = ProcessManager.WaitForActiveProcessesToComplete();
                    }
                }
                catch (System.Exception ex)
                {
                    Console.WriteLine(ex.ToString());
                }
            }

			// Clean up unnecessary files
			if( bSuccess )
			{
				try
				{
					// The staging directory for the VFS creation
					Directory.Delete( WebsiteRoot + "FlashBuild", true );
				}
				catch (System.Exception ex)
				{
					Console.WriteLine(ex.ToString());				
				}
			}

			// Copy the website to its final location
			if( bSuccess )
			{
				try
				{
					string DstDirectory = string.Format( "Flash\\Website-{0}-{1}-{2}", BuildSettings.GameName, PlatformName, GetMobileConfigurationString( InProfile.LaunchConfiguration ) );
					if( Directory.Exists( DstDirectory ) )
					{
						Directory.Delete( DstDirectory, true );
                        Thread.Sleep(250);
					}
					Directory.Move( WebsiteRoot, DstDirectory );
				}
				catch( System.Exception ex )
				{
					Console.WriteLine( ex.ToString() );
				}
			}
			
			return bSuccess;
		}


		public static string GetMobileConfigurationString( Profile.Configuration Config )
		{
			string RetVal = string.Empty;

			switch (Config)
			{
				case Profile.Configuration.Debug_32:
				case Profile.Configuration.Debug_64:
					{
						RetVal = "Debug";
						break;
					}
				case Profile.Configuration.Release_32:
				case Profile.Configuration.Release_64:
					{
						RetVal = "Release";
						break;
					}
				case Profile.Configuration.Shipping_32:
				case Profile.Configuration.Shipping_64:
					{
						RetVal = "Shipping";
						break;
					}
				case Profile.Configuration.Test_32:
				case Profile.Configuration.Test_64:
					{
						RetVal = "Test";
						break;
					}
			}

			return RetVal;
		}
	}


}
