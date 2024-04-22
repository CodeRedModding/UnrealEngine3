// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;

namespace Controller
{
	public partial class BuildState
	{
		/// <summary>
		/// The tool configuration class contains the locations of all the tools used to build targets. Typically, these do not change often,
		/// but are configurable for special cases.
		/// </summary>
		public class ToolConfiguration
		{
			private BuildState Parent = null;

			private bool ValidateDevEnv( FileInfo Info, string RequiredVersion )
			{
				if( !Info.Exists )
				{
					return false;
				}

				FileInfo ExeInfo = new FileInfo( Path.ChangeExtension( Info.FullName, ".exe" ) );
				if( !ExeInfo.Exists )
				{
					return false;
				}

				FileVersionInfo VersionInfo = FileVersionInfo.GetVersionInfo( ExeInfo.FullName );
				if( VersionInfo.ProductVersion != RequiredVersion )
				{
					return false;
				}

				return true;
			}

			/// <summary>
			/// Location of DevEnv 2008
			/// </summary>
			public string MSVC9Application 
			{
				get
				{
					return LocalMSVC9Application;
				}
				set
				{
					if( value != null )
					{
						FileInfo Info = new FileInfo( value );
						if( ValidateDevEnv( Info, "9.0.30729.1" ) )
						{
							LocalMSVC9Application = Info.FullName;
							Parent.Parent.Log( "ToolConfig: Validated Visual Studio 2008 SP1 location (" + LocalMSVC9Application + ")", Color.Blue );
						}
						else
						{
							Parent.Parent.Log( "ERROR: Invalid Visual Studio 2008 detected", Color.Red );
						}
					}
					else
					{
						Parent.Parent.Log( " ... ignoring Visual Studio 2008 SP1", Color.Blue );
					}
				}
			}
			private string LocalMSVC9Application = "";

			/// <summary>
			/// Location of the MSBuild application for Visual Studio 2008
			/// </summary>
			public string MSBuild9Application
			{
				get
				{
					return LocalMSBuild9Application;
				}
				set
				{
					FileInfo Info = new FileInfo( value );
					if( Info.Exists )
					{
						LocalMSBuild9Application = Info.FullName;
						Parent.Parent.Log( "ToolConfig: Validated MSBuild 2008 location (" + LocalMSBuild9Application + ")", Color.Blue );
					}
					else
					{
						Parent.Parent.Log( "ERROR: Invalid MSBuild location (please install Visual Studio 2008 SP1)", Color.Red );
					}
				}
			}
			private string LocalMSBuild9Application = "";

			/// <summary>
			/// Location of DevEnv 2010
			/// </summary>
			public string MSVC10Application 
			{
				get
				{
					return LocalMSVC10Application;
				}
				set
				{
					if( value != null )
					{
						FileInfo Info = new FileInfo( value );
						if( ValidateDevEnv( Info, "10.0.40219.1" ) )
						{
							LocalMSVC10Application = Info.FullName;
							Parent.Parent.Log( "ToolConfig: Validated Visual Studio 2010 SP1 location (" + LocalMSVC10Application + ")", Color.Blue );
						}
						else
						{
							Parent.Parent.Log( "ERROR: Invalid Visual Studio 2010 detected", Color.Red );
						}
					}
					else
					{
						Parent.Parent.Log( " ... ignoring Visual Studio 2010 SP1", Color.Blue );
					}
				}
			}
			private string LocalMSVC10Application = "";

			/// <summary>
			/// Location of the MSBuild application for Visual Studio 2010
			/// </summary>
			public string MSBuild10Application
			{
				get
				{
					return LocalMSBuild10Application;
				}
				set
				{
					FileInfo Info = new FileInfo( value );
					if( Info.Exists )
					{
						LocalMSBuild10Application = Info.FullName;
						Parent.Parent.Log( "ToolConfig: Validated MSBuild 2010 location (" + LocalMSBuild10Application + ")", Color.Blue );
					}
					else
					{
						Parent.Parent.Log( "ERROR: Invalid MSBuild location (please install Visual Studio 2010 SP1)", Color.Red );
					}
				}
			}
			private string LocalMSBuild10Application = "";

			/// <summary>
			/// Location of signing tool - C:\Program Files\Microsoft SDKs\Windows\v7.0A\Bin
			/// </summary>
			public string SignToolName 			
			{
				get
				{
					return LocalSignToolName;
				}
				set
				{
					FileInfo Info = new FileInfo( value );
					if( Info.Exists )
					{
						LocalSignToolName = Info.FullName;
						Parent.Parent.Log( "ToolConfig: Validated signing tool location (" + LocalSignToolName + ")", Color.Blue );
					}
					else
					{
						Parent.Parent.Log( "ERROR: Invalid sign tool location (please install Windows SDK 7.0A)", Color.Red );
					}
				}
			}
			private string LocalSignToolName = "";

			/// <summary>
			/// Location of cat tool - C:\Program Files\Microsoft SDKs\Windows\v7.0A\Bin
			/// </summary>
			public string CatToolName
			{
				get
				{
					return LocalCatToolName;
				}
				set
				{
					FileInfo Info = new FileInfo( value );
					if( Info.Exists )
					{
						LocalCatToolName = Info.FullName;
						Parent.Parent.Log( "ToolConfig: Validated cat tool location (" + LocalCatToolName + ")", Color.Blue );
					}
					else
					{
						Parent.Parent.Log( "ERROR: Invalid sign tool location (please install Windows SDK 7.0A)", Color.Red );
					}
				}
			}
			private string LocalCatToolName = "";

			/// <summary>
			/// Location of ZDPP tool - GFWLSDK_DIR\tools\ZeroDayPiracyProtection\ZdpSdkTool.exe
			/// </summary>
			public string ZDPPToolName
			{
				get
				{
					return LocalZDPPToolName;
				}
				set
				{
					if( value != null )
					{
						FileInfo Info = new FileInfo( value );
						if( Info.Exists )
						{
							LocalZDPPToolName = Info.FullName;
							Parent.Parent.Log( "ToolConfig: Validated ZDPP location (" + LocalZDPPToolName + ")", Color.Blue );
						}
						else
						{
							Parent.Parent.Log( "ERROR: Invalid ZDPP location (please install Games for Windows Live! SDK)", Color.Red );
						}
					}
					else
					{
						Parent.Parent.Log( " ... ignoring ZDPP", Color.Blue );
					}
				}
			}
			private string LocalZDPPToolName = "";

			/// <summary>
			/// Location of source server perl script
			/// </summary>
			public string SourceServerCmd
			{
				get
				{
					return LocalSourceServerCmd;
				}
				set
				{
					FileInfo Info = new FileInfo( value );
					if( Info.Exists )
					{
						LocalSourceServerCmd = Info.FullName;
						Parent.Parent.Log( "ToolConfig: Validated source server command location (" + LocalSourceServerCmd + ")", Color.Blue );
					}
					else
					{
						Parent.Parent.Log( "ERROR: Invalid source server command location (please install Debugging Tools for Windows)", Color.Red );
					}
				}
			}
			private string LocalSourceServerCmd = "";

			/// <summary>
			/// Location of XDK tools
			/// </summary>
			public string BlastTool
			{
				get
				{
					return LocalBlastTool;
				}
				set
				{
					if( value != null )
					{
						FileInfo Info = new FileInfo( value );
						if( Info.Exists )
						{
							LocalBlastTool = Info.FullName;
							Parent.Parent.Log( "ToolConfig: Validated symbol store location (" + LocalBlastTool + ")", Color.Blue );
						}
						else
						{
							Parent.Parent.Log( "ERROR: Invalid Blast location (please install the XDK)", Color.Red );
						}
					}
					else
					{
						Parent.Parent.Log( " ... ignoring Blast", Color.Blue );
					}
				}
			}
			private string LocalBlastTool = "";

			/// <summary>
			/// Location of the Steam Content Tool
			/// </summary>
			public string SteamContentToolLocation
			{
				get
				{
					return LocalSteamContentToolLocation;
				}
				set
				{
					if( value != null )
					{
						FileInfo Info = new FileInfo( value );
						if( Info.Exists )
						{
							LocalSteamContentToolLocation = Info.FullName;
							Parent.Parent.Log( "ToolConfig: Validated Steam Content Tool location (" + LocalSteamContentToolLocation + ")", Color.Blue );
						}
						else
						{
							Parent.Parent.Log( "ERROR: Invalid Steam Content Tool location", Color.Red );
						}
					}
					else
					{
						Parent.Parent.Log( " ... ignoring Steam Content Tool", Color.Blue );
					}
				}
			}
			private string LocalSteamContentToolLocation = "";

			/// <summary>
			/// Location of the Steam Content Server
			/// </summary>
			public string SteamContentServerLocation
			{
				get
				{
					return LocalSteamContentServerLocation;
				}
				set
				{
					if( value != null )
					{
						FileInfo Info = new FileInfo( value );
						if( Info.Exists )
						{
							LocalSteamContentServerLocation = Info.FullName;
							Parent.Parent.Log( "ToolConfig: Validated Steam Content Server location (" + LocalSteamContentToolLocation + ")", Color.Blue );
						}
						else
						{
							Parent.Parent.Log( "ERROR: Invalid Steam Content Server location", Color.Red );
						}
					}
					else
					{
						Parent.Parent.Log( " ... ignoring Steam Content Server", Color.Blue );
					}
				}
			}
			private string LocalSteamContentServerLocation = "";

			/// <summary>
			/// The container for all the tool locations
			/// </summary>
			/// <param name="InParent">The owning build</param>
			/// <param name="BranchDef">The current branch</param>
			public ToolConfiguration( BuildState InParent, Main.BranchDefinition BranchDef )
			{
				Parent = InParent;

				MSVC9Application = Environment.GetEnvironmentVariable( "VS90COMNTOOLS" ) + "../IDE/Devenv.com";
				MSBuild9Application = Environment.GetEnvironmentVariable( "FrameworkDir" ) + "/v3.5/MSBuild.exe";
				MSVC10Application = Environment.GetEnvironmentVariable( "VS100COMNTOOLS" ) + "../IDE/Devenv.com";
				MSBuild10Application = Environment.GetEnvironmentVariable( "FrameworkDir" ) + "/v4.0.30319/MSBuild.exe";
				SignToolName = "C:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Bin/SignTool.exe";
				CatToolName = "C:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Bin/MakeCat.exe";
				ZDPPToolName = Environment.GetEnvironmentVariable( "GFWLSDK_DIR" ) + "tools/ZeroDayPiracyProtection/ZdpSdkTool.exe";
				SourceServerCmd = "C:/Program Files/Debugging Tools for Windows (x64)/srcsrv/p4index.cmd";
				BlastTool = Environment.GetEnvironmentVariable( "XEDK" ) + "\\bin\\win32\\blast.exe";

				if( BranchDef.Version < 10 )
				{
					SteamContentToolLocation = "Development/External/Steamworks/sdk/tools/ContentTool.com";
					SteamContentServerLocation = "Development/External/Steamworks/sdk/tools/contentserver/contentserver.exe";
				}
				else
				{
					string SteamVersion = "v120";
					SteamContentToolLocation = "Engine/Source/ThirdParty/Steamworks/Steam" + SteamVersion + "/sdk/tools/SteamPipe/ContentBuilder/builder/steamcmd.exe";
				}
			}
		}

		/// <summary>
		/// The version configuration class contains all the information to handle all version info. There are several version files to handle this;
		/// some specific to consoles, some to mobile.
		/// </summary>
		public class VersionConfiguration
		{
			private BuildState Parent = null;

			/// <summary>
			/// The master file that contains the version to use
			/// </summary>
			public string EngineVersionFile 
			{
				get
				{
					return LocalEngineVersionFile;
				}
				set
				{
					FileInfo Info = new FileInfo( value );
					if( Info.Exists )
					{
						LocalEngineVersionFile = Parent.GetBranchRelativePath( Info.FullName );
						Parent.Parent.Log( "VersionConfig: Validated engine version file location (" + LocalEngineVersionFile + ")", Color.Blue );
					}
					else
					{
						Parent.Parent.Log( "ERROR: Invalid engine version file location! (" + Info.FullName + ")", Color.Red );
					}
				}
			}
			private string LocalEngineVersionFile = "";

			/// <summary>
			/// The misc version files that the engine version is propagated to
			/// </summary>
			public List<string> MiscVersionFiles 
			{
				get
				{
					return LocalMiscVersionFiles;
				}
				set
				{
					List<string> ValidVersionFiles = new List<string>();
					foreach( string VersionFile in value )
					{
						try
						{
							FileInfo Info = new FileInfo( VersionFile );
							if( Info.Exists )
							{
								ValidVersionFiles.Add( Parent.GetBranchRelativePath( Info.FullName ) );
							}
						}
						catch
						{
						}
					}

					if( ValidVersionFiles.Count == value.Count )
					{
						LocalMiscVersionFiles = ValidVersionFiles;
						Parent.Parent.Log( "VersionConfig: Validated " + LocalMiscVersionFiles.Count + " misc version files locations", Color.Blue );
					}
					else
					{
						Parent.Parent.Log( "ERROR: Invalid misc version files locations!", Color.Red );
					}
				}
			}
			private List<string> LocalMiscVersionFiles = null;

			/// <summary>
			/// The console specific version files that the engine version is propagated to
			/// </summary>
			public List<string> ConsoleVersionFiles 
			{
				get
				{
					return LocalConsoleVersionFiles;
				}
				set
				{
					List<string> ValidVersionFiles = new List<string>();
					foreach( string VersionFile in value )
					{
						try
						{
							FileInfo Info = new FileInfo( VersionFile );
							if( Info.Exists )
							{
								ValidVersionFiles.Add( Parent.GetBranchRelativePath( Info.FullName ) );
							}
						}
						catch
						{
						}
					}

					if( ValidVersionFiles.Count == value.Count )
					{
						LocalConsoleVersionFiles = ValidVersionFiles;
						Parent.Parent.Log( "VersionConfig: Validated " + LocalConsoleVersionFiles.Count + " console version files locations", Color.Blue );
					}
					else
					{
						Parent.Parent.Log( "ERROR: Invalid console version files locations!", Color.Red );
					}
				}
			}
			private List<string> LocalConsoleVersionFiles = null;

			/// <summary>
			/// The mobile specific version files that the engine version is propagated to
			/// </summary>
			public List<string> MobileVersionFiles 
			{
				get
				{
					return LocalMobileVersionFiles;
				}
				set
				{
					List<string> ValidVersionFiles = new List<string>();
					foreach( string VersionFile in value )
					{
						try
						{
							FileInfo Info = new FileInfo( VersionFile );
							if( Info.Exists )
							{
								ValidVersionFiles.Add( Parent.GetBranchRelativePath( Info.FullName ) );
							}
						}
						catch
						{
						}
					}

					if( ValidVersionFiles.Count == value.Count )
					{
						LocalMobileVersionFiles = ValidVersionFiles;
						Parent.Parent.Log( "VersionConfig: Validated " + LocalMobileVersionFiles.Count + " mobile version files locations", Color.Blue );
					}
					else
					{
						Parent.Parent.Log( "ERROR: Invalid mobile version files locations!", Color.Red );
					}
				}
			}
			private List<string> LocalMobileVersionFiles = null;

			/// <summary>
			/// The container for all version files
			/// </summary>
			/// <param name="InParent">The owning build</param>
			/// <param name="BranchDef">The current branch</param>
			public VersionConfiguration( BuildState InParent, Main.BranchDefinition BranchDef )
			{
				Parent = InParent;

				if( BranchDef.Version < 10 )
				{
					EngineVersionFile = "Development/Src/Core/Src/UnObjVer.cpp";
					MiscVersionFiles = new List<string>() { "Development/Src/Launch/Resources/Version.h", "Binaries/build.properties" };
					ConsoleVersionFiles = new List<string>();
					MobileVersionFiles = new List<string>() { "Development/Src/IPhone/Resources/IPhone-Info.plist", "UDKGame/Build/IPhone/IPhone-Info.plist" };
				}
				else
				{
					EngineVersionFile = "Engine/Source/Runtime/Core/Private/UObject/ObjectVersion.cpp";
					MiscVersionFiles = new List<string>() { "Engine/Source/Runtime/Launch/Resources/Version.h", "Engine/Build/build.properties" };
					ConsoleVersionFiles = new List<string>();
					MobileVersionFiles = new List<string>();
				}

			}
		}
	}
}