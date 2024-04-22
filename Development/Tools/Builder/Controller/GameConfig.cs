// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Controller
{
    public partial class GameConfig
    {
		public GameConfig( string InGame, string InPlatform, string InConfiguration, List<string> InDefines, bool InLocal, bool InUse64BitBinaries )
        {
            RootGameName = InGame;
			GameName = RootGameName + "Game";
            Platform = InPlatform;
            Configuration = InConfiguration;
            IsLocal = InLocal;
			Use64BitBinaries = InUse64BitBinaries;

			Defines = new List<string>();
			if( InDefines != null )
			{
				Defines.AddRange( InDefines );
			}

			switch( Platform.ToLower() )
			{
			case "win32server":
			case "pcserver":
				SubPlatform = "Server";
				break;
			case "win32dll":
				SubPlatform = "DLL";
				break;
			default:
				SubPlatform = "";
				break;
			}
		}

        public GameConfig( int Count, GameConfig Game )
        {
            switch( Count )
            {
			case 1:
				GameName = "A Game";
				break;
			
			default:
				GameName = Count.ToString() + " Games";
				break;
			}

            Platform = Game.Platform;
            Configuration = Game.Configuration;
        }

        private List<string> GetPCBinaryName( int BranchVersion, string Extension )
        {
			List<string> Configs = new List<string>();

			switch( BranchVersion )
			{
			case 1:
			case 10:
				if( Configuration.ToLower() == "release" || Configuration.ToLower() == "development" )
				{
					Configs.Add( "Binaries/" + GetUBTPlatform() + "/" + GameName + SubPlatform + GetDefined() + Extension );
				}
				else if( Configuration.ToLower() == "debug" || Configuration.ToLower() == "shipping" || Configuration.ToLower() == "test" )
				{
					// Some shipping binaries are special cased
					if( Configuration.ToLower() == "shipping" && GameName.ToLower() == "udkgame" )
					{
						Configs.Add( "Binaries/" + GetUBTPlatform() + "/UDK" + Extension );
					}
					else
					{
						Configs.Add( "Binaries/" + GetUBTPlatform() + "/" + GameName + SubPlatform + "-" + GetUBTPlatform() + "-" + Configuration + Extension );
					}
				}

				break;

			default:
				if( Configuration.ToLower() == "release" || Configuration.ToLower() == "development" )
				{
					Configs.Add( "Binaries/" + GetUBTPlatform() + "/" + GameName + SubPlatform + GetDefined() + Extension );
				}
				else if( Configuration.ToLower() == "debug" )
				{
					Configs.Add( "Binaries/" + GetUBTPlatform() + "/DEBUG-" + GameName + SubPlatform + Extension );
				}
				else if( Configuration.ToLower() == "releaseshippingpc" || Configuration.ToLower() == "shipping" )
				{
					if( GameName.ToLower() == "udkgame" )
					{
						Configs.Add( "Binaries/" + GetUBTPlatform() + "/UDK" + Extension );
					}
					else
					{
						Configs.Add( "Binaries/" + GetUBTPlatform() + "/ShippingPC-" + GameName + SubPlatform + Extension );
					}
				}
				break;
			}

            return ( Configs );
        }

        private List<string> GetXbox360BinaryName( int Branchversion, string Extension )
        {
			List<string> Configs = new List<string>();

			switch( Branchversion )
			{
			case 1:
				Configs.Add( "Binaries/Xbox360/" + GameName + "-" + Platform + "-" + Configuration + GetDefined() + Extension );
				break;

			default:
				Configs.Add( GameName + "-" + Configuration + GetDefined() + Extension );
				break;
			}

            return ( Configs );
        }

		private string GetIPhoneBinaryName( int Branchversion, string Extension )
		{
			string DecoratedName;

			switch( Branchversion )
			{
				case 1:
					DecoratedName = "Binaries/IPhone/" + GameName + "-IPhone-" + Configuration + Extension;
					break;

				default:
					DecoratedName = "Binaries/IPhone/" + Configuration + "-iphoneos/" + GameName.ToUpper() + "/" + GameName.ToUpper() + Extension;
					break;
			}

			return DecoratedName;
		}
		
		public List<string> GetExecutableNames( int BranchVersion )
        {
			List<string> Configs = new List<string>();

			if( Platform.ToLower() == "win32" || Platform.ToLower() == "win64" || Platform.ToLower() == "win32server" )
			{
				Configs = GetPCBinaryName( BranchVersion, ".exe" );
			}
			else if( Platform.ToLower() == "win32dll" )
			{
				Configs = GetPCBinaryName( BranchVersion, ".dll" );
			}
			else if( Platform.ToLower() == "ps3" )
			{
				Configs.Add( "Binaries/PS3/" + GameName.ToUpper() + "-" + Platform.ToUpper() + "-" + Configuration.ToUpper() + GetDefined() + ".elf" );
			}
			else if( Platform.ToLower() == "ngp" )
			{
				Configs.Add( "Binaries/NGP/" + GameName.ToUpper() + "-" + Platform.ToUpper() + "-" + Configuration.ToUpper() + GetDefined() + ".elf" );
			}
			else if( Platform.ToLower() == "xbox360" )
			{
				Configs = GetXbox360BinaryName( BranchVersion, ".xex" );
			}
			else if( Platform.ToLower() == "sonyps3" )
			{
				Configs.Add( "Binaries/PS3/" + GameName.ToUpper() + "-" + Configuration + GetDefined() + ".elf" );
			}
			else if( Platform.ToLower() == "androidx86" || Platform.ToLower() == "androidarm" )
			{
				Configs.Add( "Binaries/Android/" + GameName + "-" + Platform + "-" + Configuration + ".so" );
			}
			else if( Platform.ToLower() == "iphone" || Platform.ToLower() == "iphonedevice" )
			{
				Configs.Add( GetIPhoneBinaryName( BranchVersion, "" ) );
				Configs.Add( GetIPhoneBinaryName( BranchVersion, ".stub" ) );
			}
			else if( Platform.ToLower() == "mac" )
			{
				Configs.Add( "Binaries/Mac/" + GameName + "-" + Platform + "-" + Configuration );
				Configs.Add( "Binaries/Mac/" + GameName + "-" + Platform + "-" + Configuration + ".app.stub" );
			}
			else if( Platform.ToLower() == "wiiu" )
			{
				// HACK: pass in the symbol file as an executable to work around flawed WiiU tools
				Configs.Add( "Binaries/WiiU/" + GameName + "-" + Platform + "-" + Configuration + ".elf" );
				Configs.Add( "Binaries/WiiU/" + GameName + "-" + Platform + "-" + Configuration + ".rpx" );
			}
			else if( Platform.ToLower() == "flash" )
			{
				Configs.Add( "Binaries/Flash/" + GameName + "-" + Platform + "-" + Configuration + ".swf" );
			}

			return ( Configs );
        }

        public string GetComName( int BranchVersion, ref string CommandLine )
        {
            string ComName = "";
			CommandLine = "";

			string DecoratedGameName = GameName;

			switch( BranchVersion )
			{
			case 10:
				ComName = "Engine\\Binaries\\Win64\\UE4.exe";
				CommandLine = RootGameName + " -run=";
				break;

			case 1:
				string Bitness = "Win64";
				if( !Use64BitBinaries )
				{
					Bitness = "Win32";
				}

				switch( Configuration.ToLower() )
				{
				default:
				case "debug":
				case "test":
					ComName = "Binaries\\" + Bitness + "\\" + DecoratedGameName + "-" + Bitness + "-" + Configuration + ".exe";
					break;

				case "release":
				case "development":
					ComName = "Binaries\\" + Bitness + "\\" + DecoratedGameName + ".exe";
					break;

				case "shipping":
					if( GameName.ToLower() == "udkgame" )
					{
						ComName = "Binaries\\" + Bitness + "\\UDK.exe";
					}
					else
					{
						ComName = "Binaries\\" + Bitness + "\\" + DecoratedGameName + "-" + Bitness + "-" + Configuration + ".exe";
					}
					break;
				}

				break;

			default:
				if( Configuration.ToLower() == "release" || Configuration.ToLower() == "development" )
				{
					DecoratedGameName = GameName;
				}
				else if( Configuration.ToLower() == "releaseltcg" )
				{
					DecoratedGameName = "LTCG-" + GameName;
				}
				else if( Configuration.ToLower() == "debug" )
				{
					DecoratedGameName = "DEBUG-" + GameName;
				}
				else if( Configuration.ToLower() == "releaseshippingpc" || Configuration.ToLower() == "shipping" )
				{
					if( GameName.ToLower() == "udkgame" )
					{
						DecoratedGameName = "UDK";
					}
					else if( GameName.ToLower() == "mobilegame" )
					{
						DecoratedGameName = "UDKMobile";
					}
					else
					{
						DecoratedGameName = "ShippingPC-" + GameName;
					}
				}

				if( Platform.ToLower() == "win32"
					|| Platform.ToLower() == "win32server"
					|| Platform.ToLower() == "pcconsole"
					|| Platform.ToLower() == "win64"
					|| Platform.ToLower() == "win32_sm4"
					|| Platform.ToLower() == "win64_sm4"
					|| Platform.ToLower() == "win32_sm5"
					|| Platform.ToLower() == "win64_sm5"
					|| Platform.ToLower() == "win32_ogl"
					|| Platform.ToLower() == "win64_ogl"
					|| Platform.ToLower() == "xbox360"
					|| Platform.ToLower() == "ps3"
					|| Platform.ToLower() == "sonyps3"
					|| Platform.ToLower() == "ngp"
					|| Platform.ToLower() == "iphone"
					|| Platform.ToLower() == "iphonedevice"
					|| Platform.ToLower() == "androidarm"
					|| Platform.ToLower() == "androidx86"
					|| Platform.ToLower() == "mac"
					|| Platform.ToLower() == "wiiu"
					)
				{
					if( Use64BitBinaries )
					{
						ComName = "Binaries/Win64/" + DecoratedGameName + ".exe";
					}
					else
					{
						ComName = "Binaries/Win32/" + DecoratedGameName + ".exe";
					}
				}
				else
				{
					ComName = "Binaries/" + DecoratedGameName + ".exe";
				}
				break;
			}

            return ( ComName );
        }

		public string GetUDKComName( string ComName, string Branch, string EncodedFolderName, string RootFolder )
		{
			// Add the absolute base path
			string ComPathName = Path.Combine( RootFolder, Path.Combine( EncodedFolderName, Branch ) );

			ComName = Path.Combine( ComPathName, ComName );

			return ( ComName );
		}

        public List<string> GetSymbolFileNames( int BranchVersion )
        {
			List<string> Configs = new List<string>();

			if( Platform.ToLower() == "win32" || Platform.ToLower() == "win64" || Platform.ToLower() == "win32server" || Platform.ToLower() == "win32dll" )
            {
                Configs = GetPCBinaryName( BranchVersion, ".pdb" );
            }
			else if( Platform.ToLower() == "ps3" || Platform.ToLower() == "sonyps3" )
			{
			}
			else if( Platform.ToLower() == "xbox360" )
            {
				Configs = GetXbox360BinaryName( BranchVersion, ".pdb" );
				string AltDebugFile = Path.ChangeExtension( Configs[0], ".xdb" );
				Configs.Add( AltDebugFile );
            }
			else if( Platform.ToLower() == "ngp" )
			{
			}
			else if( Platform.ToLower() == "androidarm" || Platform.ToLower() == "androidx86" )
 			{
			}
			else if( Platform.ToLower() == "iphone" || Platform.ToLower() == "iphonedevice" )
			{
				Configs.Add( GetIPhoneBinaryName( BranchVersion, ".app.dSYM.zip" ) );
			}
			else if( Platform.ToLower() == "wiiu" )
			{
				// HACK: Put the elf back once UBT is fixed properly
			}

            return ( Configs );
        }

		public List<string> GetFilesToClean( int BranchVersion )
		{
			List<string> BinaryFiles = GetExecutableNames( BranchVersion );

			switch( Platform.ToLower() )
			{
			case "win32":
			case "win64":
			case "xbox360":
				string RootName = BinaryFiles[0].Remove( BinaryFiles[0].Length - 4 );
				BinaryFiles.Add( RootName + ".exp" );
				BinaryFiles.Add( RootName + ".lib" );
				break;
			}

			BinaryFiles.AddRange( GetSymbolFileNames( BranchVersion ) );

			return ( BinaryFiles );
		}

		public string GetCookedFolderPlatform()
		{
			string CookedFolder = Platform;

			if( CookedFolder.ToLower() == "pc" || CookedFolder.ToLower() == "win32" || CookedFolder.ToLower() == "win64" ||
				CookedFolder.ToLower() == "pc_sm4" || CookedFolder.ToLower() == "win32_sm4" || CookedFolder.ToLower() == "win64_sm4" ||
				CookedFolder.ToLower() == "pc_sm5" || CookedFolder.ToLower() == "win32_sm5" || CookedFolder.ToLower() == "win64_sm5" ||
				CookedFolder.ToLower() == "pc_ogl" || CookedFolder.ToLower() == "win32_ogl" || CookedFolder.ToLower() == "win64_ogl" )
			{
				CookedFolder = "PC";
			}
			else if( CookedFolder.ToLower() == "pcconsole" || CookedFolder.ToLower() == "win32console" || CookedFolder.ToLower() == "win64console" )
			{
				CookedFolder = "PCConsole";
			}
			else if( CookedFolder.ToLower() == "pcserver" || CookedFolder.ToLower() == "win32server" || CookedFolder.ToLower() == "win64server" )
			{
				CookedFolder = "PCServer";
			}

			// The following should fall through:
			//
			// Xbox360
			// PS3
			// iPhone
			// Android
			// Mac
			// WiiU

			return ( CookedFolder );
		}

		public string GetShaderPlatform( int BranchVersion )
		{
			string ShaderPlatform = Platform;
			if( ShaderPlatform.ToLower() == "pc" || ShaderPlatform.ToLower() == "win32" || ShaderPlatform.ToLower() == "win64" )
			{
				ShaderPlatform = "PC";
			}
			else if( ShaderPlatform.ToLower() == "pc_sm4" || ShaderPlatform.ToLower() == "win32_sm4" || ShaderPlatform.ToLower() == "win64_sm4" )
			{
				ShaderPlatform = "PC_SM4";
			}
			else if( ShaderPlatform.ToLower() == "pc_sm5" || ShaderPlatform.ToLower() == "win32_sm5" || ShaderPlatform.ToLower() == "win64_sm5" )
			{
				ShaderPlatform = "PC_SM5";
			}
			else if( ShaderPlatform.ToLower() == "pc_ogl" || ShaderPlatform.ToLower() == "win32_ogl" || ShaderPlatform.ToLower() == "win64_ogl" )
			{
				ShaderPlatform = "PC_OGL";
			}
			else if( ShaderPlatform.ToLower() == "xbox360" || ShaderPlatform.ToLower() == "xenon" )
			{
				if( BranchVersion > 0 )
				{
					ShaderPlatform = "Xbox360";
				}
				else
				{
					ShaderPlatform = "Xenon";
				}
			}
			else if( ShaderPlatform.ToLower() == "sonyps3" || ShaderPlatform.ToLower() == "ps3" )
			{
				ShaderPlatform = "PS3";
			}
			else if( ShaderPlatform.ToLower() == "wiiu" )
			{
				ShaderPlatform = "WiiU";
			}

			return ( ShaderPlatform );
		}

        private List<string> GetShaderNames( string ShaderType, string Extension )
        {
			List<string> ShaderNames = new List<string>();
			string ShaderName = GameName + "/Content/" + ShaderType + "ShaderCache";

			if( Platform.ToLower() == "pc" || Platform.ToLower() == "win32" || Platform.ToLower() == "win64" || Platform.ToLower() == "pcserver" || Platform.ToLower() == "pcconsole" )
            {
				ShaderNames.Add( ShaderName + "-PC-D3D-SM3" + Extension );
				// Alternate
				ShaderNames.Add( ShaderName + "-PC-D3D-SM5" + Extension );
				ShaderNames.Add( ShaderName + "-PC-OpenGL" + Extension );
			}
			else if( Platform.ToLower() == "pc_sm5" || Platform.ToLower() == "win32_sm5" || Platform.ToLower() == "win64_sm5" )
			{
				ShaderNames.Add( ShaderName + "-PC-D3D-SM5" + Extension );
				// Alternate
				ShaderNames.Add( ShaderName + "-PC-D3D-SM3" + Extension );
				ShaderNames.Add( ShaderName + "-PC-OpenGL" + Extension );
			}
			else if( Platform.ToLower() == "pc_ogl" || Platform.ToLower() == "win32_ogl" || Platform.ToLower() == "win64_ogl" )
			{
				ShaderNames.Add( ShaderName + "-PC-OpenGL" + Extension );
				// Alternate
				ShaderNames.Add( ShaderName + "-PC-D3D-SM3" + Extension );
				ShaderNames.Add( ShaderName + "-PC-D3D-SM5" + Extension );
			}
            else if( Platform.ToLower() == "xbox360" )
            {
				ShaderNames.Add( ShaderName + "-Xbox360" + Extension );
            }
            else if( Platform.ToLower() == "ps3" )
            {
				ShaderNames.Add( ShaderName + "-PS3" + Extension );
            }
			else if( Platform.ToLower() == "wiiu" )
			{
				ShaderNames.Add( ShaderName + "-WiiU" + Extension );
			}

            return ( ShaderNames );
        }

        public string GetRefShaderName()
        {
			List<string> ShaderNames = GetShaderNames( "Ref", ".upk" );
            return ( ShaderNames[0] );
        }

        public string GetLocalShaderName()
        {
			List<string> ShaderNames = GetShaderNames( "Local", ".upk" );
			return ( ShaderNames[0] );
        }

		public string[] GetLocalShaderNames()
		{
			List<string> LocalShaderCaches = GetShaderNames( "Local", ".upk" );
			return ( LocalShaderCaches.ToArray() );
		}

        public string[] GetGlobalShaderNames()
        {
			List<string> GlobalShaderCaches = GetShaderNames( "Global", ".bin" );

			return ( GlobalShaderCaches.ToArray() );
		}
        
		public string GetGADCheckpointFileName()
		{
			return ( GameName + "/Content/GameAssetDatabase.checkpoint" );
		}

		public string GetConfigFolderName()
        {
			string Folder = GameName + "/config";
            return ( Folder );
        }

		public string GetScreenShotFolderName()
		{
			string Folder = GameName + "/Screenshots/" + GetCookedFolderPlatform();

			return ( Folder );
		}

		public string GetArtistSyncRulesFileName()
		{
			string RulesFileName = GameName + "/Build/ArtistSyncRules.xml";
			return RulesFileName;
		}

		public string GetAFTReferenceFolder( string TargetType, string MapName )
		{
			string Folder = GameName + "/QA/" + GetCookedFolderPlatform() + "/" + TargetType + "/" + MapName;
			return ( Folder );
		}

		public string GetConnCacheName()
		{
			string ConnCache = GameName + "/Content/RefConnCache.upk";
			return ConnCache;
		}

        public string GetCookedFolderName( int BranchVersion )
        {
			string Folder = GameName + "/Cooked" + GetCookedFolderPlatform();
            return Folder;
        }

		public string GetOriginalCookedFolderName( int BranchVersion )
		{
			string Folder = GetCookedFolderName( BranchVersion ) + "Original";
			return Folder;
		}

		public string[] GetDLCCookedFolderNames( string DLCName, int BranchVersion )
		{
			List<string> CookedFolders = new List<string>();

			string CookedPlatform = GetCookedFolderPlatform();
			CookedFolders.Add( GameName + "/DLC/" + CookedPlatform + "/" + DLCName + "/Content/" + GameName + "/Cooked" + CookedPlatform );
			CookedFolders.Add( GameName + "/DLC/" + CookedPlatform + "/" + DLCName + "/Online" );

			return ( CookedFolders.ToArray() );
		}

		public string GetCookedConfigFolderName( int BranchVersion )
        {
			string Folder = GameName + "/Config/" + GetCookedFolderPlatform() + "/Cooked";
            return ( Folder );
        }

		public string GetWrangledFolderName()
		{
			string Folder = GameName + "/CutdownPackages";
			return ( Folder );
		}

		public string GetDMSFolderName()
		{
			string Folder = GameName + "/Logs/MapSummaryData";
			return ( Folder );
		}

		public string GetTUFolderName( string TUName )
		{
			string Folder = GameName + "/TU/" + Platform + "/" + TUName + "/TUExecutable";
			return ( Folder );
		}

		public string GetPatchesFolderName()
		{
			string Folder = GameName + "/Patches";
			return ( Folder );
		}

		public string GetMobileShadersFolderName()
		{
			string Folder = GameName + "/Shaders/ES2";
			return ( Folder );
		}

        public string GetPatchFolderName()
        {
			string Folder = GameName + "/Build/" + Platform + "/Patch";
            return ( Folder );
        }

		public string GetScriptFolder()
		{
			string Folder = GameName + "/Script";
			return Folder;
		}

		public string GetHashesFileName()
		{
			string Folder = GameName + "/Build/Hashes.sha";
			return ( Folder );
		}

		public string GetDLCFolderName( string DLCName )
		{
			string FileName = GameName + "/DLC/" + Platform + "/" + DLCName;
			return ( FileName );
		}

		public string GetDLCFileName( string DLCName )
		{
			string FileName = GetDLCFolderName( DLCName ) + "/" + DLCName + ".xlast";
			return ( FileName );
		}

		public string GetDialogFileName( string DLCName, string Language, string RootName )
        {
            string DialogName;

			if( DLCName.Length > 0 )
			{
				if( Language == "INT" )
				{
					DialogName = GameName + "/Content/DLC/Sounds/" + Language + "/" + RootName + ".upk";
				}
				else
				{
					DialogName = GameName + "/Content/DLC/Sounds/" + Language + "/" + RootName + "_" + Language + ".upk";
				}
			}
			else
			{
				if( Language == "INT" )
				{
					DialogName = GameName + "/Content/Sounds/" + Language + "/" + RootName + ".upk";
				}
				else
				{
					DialogName = GameName + "/Content/Sounds/" + Language + "/" + RootName + "_" + Language + ".upk";
				}
			}

            return ( DialogName );
        }

        public string GetFontFileName( string Language, string RootName )
        {
            string FontName;

            if( Language == "INT" )
            {
				FontName = GameName + "/Content/" + RootName + ".upk";
            }
            else
            {
				FontName = GameName + "/Content/" + RootName + "_" + Language + ".upk";
            }
            return ( FontName );
        }

        public string GetPackageFileName( string Language, string RootName )
        {
            string PackageName;

            if( Language == "INT" )
            {
                PackageName = RootName + ".upk";
            }
            else
            {
                PackageName = RootName + "_" + Language + ".upk";
            }
            return ( PackageName );
        }

        public string GetLayoutFileName( string SkuName, string[] Languages, string[] TextLanguages )
        {
			string Extension = "." + Platform;
			string Folder = Platform;

			switch( Platform.ToLower() )
			{
			case "xbox360":
				Extension = ".XGD";
				Folder = "Xbox360";
				break;

			case "ps3":
				Extension = ".GP3";
				Folder = "PS3";
				break;

			case "pcconsole":
				Extension = ".InstallDesignerProject";
				Folder = "PCConsole";
				break;

			default:
				break;
			}

			string LayoutName = GameName + "/Build/" + Folder + "/Layout";

			if( SkuName.Length > 0 )
			{
				LayoutName += "_" + SkuName.ToUpper();
			}

			if( Languages.Length > 0 || TextLanguages.Length > 0 )
			{
				LayoutName += "_";
				foreach( string Lang in Languages )
				{
					char FirstLetter = Lang.ToUpper()[0];
					LayoutName += FirstLetter;
				}

				foreach( string TextLang in TextLanguages )
				{
					char FirstLetter = TextLang.ToUpper()[0];
					LayoutName += FirstLetter;
				}
			}

			LayoutName += Extension;

            return ( LayoutName );
        }

		public List<string> GetCatNames( int BranchVersion )
		{
			List<string> ExeNames = GetExecutableNames( BranchVersion );
			List<string> CatNames = new List<string>();

			CatNames.Add( ExeNames[0] + ".cat" );
			CatNames.Add( ExeNames[0] + ".cdf" );
			return ( CatNames );
		}

		public string GetCfgName( int BranchVersion )
		{
			List<string> ExeNames = GetExecutableNames( BranchVersion );
			return ( ExeNames[0] + ".cfg" );
		}

		public string GetContentSpec()
		{
			string ContentSpec = GameName + "/Content/....upk";
			return ( ContentSpec );
		}

		public string GetSteamIDSource()
		{
			return GameName + "/Build/Steam/steam_appid.txt";
		}

		public string GetSteamIDestinationFolder()
		{
			return "Binaries/" + Platform;
		}

        public string GetTitle()
        {
            return ( "UnrealEngine3-" + Platform );
        }

        override public string ToString()
        {
            return ( GameName + " (" + Platform + " - " + Configuration + ")" ); 
        }

        public bool Similar( GameConfig Game )
        {
            return( Game.Configuration == Configuration && Game.Platform == Platform );
        }

        public void DeleteCutdownPackages( Main Parent )
        {
            Parent.DeleteDirectory( GameName + "/CutdownPackages", 0 );
        }

		private string GetDefined()
		{
			if( Defines.Contains( "USE_MALLOC_PROFILER=1" ) )
			{
				return ( ".MProf" );
			}

			return ( "" );
		}

        private string GetUBTPlatform()
        {
            switch( Platform.ToLower() )
            {
                case "pc":
				case "win32server":
				case "pcserver":
				case "win32dll":
					return ( "Win32" );
            }
            
            return( Platform );
        }

		public string GetUBTCommandLine( int BranchVersion, bool bIsPrimaryBuild, bool bAllowXGE, bool bAllowPCH, bool bBuildTool )
		{
			string UBTPlatform = GetUBTPlatform();
			string UBTConfig = Configuration;

			// Use the raw game name if we're building a tool
			string Name = RootGameName;
			if( !bBuildTool )
			{
				// ... append Game if we're compiling a game
				Name += "Game";
			}

			List<string> Executables = GetExecutableNames( BranchVersion );
			string UBTCommandLine = UBTPlatform + " " + UBTConfig + " " + Name + SubPlatform + " -verbose -nofastiteration -forcedebuginfo";

			// Don't specify the output parameter for UE4
			if( Executables.Count > 0 && BranchVersion < 10 )
			{
				UBTCommandLine += " -noCommandDependencies";
				UBTCommandLine += " -output ../../" + Executables[0];
			}
			else
			{
				UBTCommandLine += " -skipActionHistory";
			}

			if( !bAllowPCH )
			{
				UBTCommandLine += " -nopch";
			}

			if( bIsPrimaryBuild )
			{
				// For primary builds, keep everything local, with all debug info
				//UBTCommandLine += " -noxge";

				// For iPhone platforms, always generate the dSYM
				if( UBTPlatform.ToLower() == "iphone" )
				{
					UBTCommandLine += " -gendsym";
				}
			}
			else
			{
				// For non-primary builds, allow distributed builds, don't generate any debug info, and don't use PCHs or PDBs
				UBTCommandLine += " -nodebuginfo -nopdb";

				// XGE defaults to enabled, only disable when instructed to do so
				if( bAllowXGE == false )
				{
					UBTCommandLine += " -noxge";
				}
			}

			return UBTCommandLine;
		}

        // Name of the game
        public string GameName;
		// Name of the game (without the "Game")
		public string RootGameName;
		// Platform
		public string Platform;
		// SubPlatform
		public string SubPlatform;
		// Build configuration eg. release
		public string Configuration;
        // Whether the build is compiled locally
        public bool IsLocal;
		// Whether to use the x64 or x86 binaries
		public bool Use64BitBinaries;
		// The defines associated with this build
		public List<string> Defines;
	}
}
