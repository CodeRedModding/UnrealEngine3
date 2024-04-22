// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Net;
using System.Threading;

using Controller.Models;

namespace Controller
{
	public partial class SandboxedAction
	{
		public MODES SCC_CheckConsistency()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_CheckConsistency ), false );

				SCC.CheckConsistency( Builder, Builder.GetCurrentCommandLine() );

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_CheckConsistency;
				Builder.Write( "Error: exception while calling CheckConsistency" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_Sync()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_Sync ), false );

				// Update database with head revision
				int HeadChangelist = SCC.GetLatestChangelist( Builder, "//" + Builder.BranchDef.CurrentClient.ClientName + "/" + Builder.BranchDef.Branch + "/..." );
				if( HeadChangelist > 0 )
				{
					BuilderLinq.UpdateBranchConfigInt( Builder.BranchDef.ID, "HeadChangelist", HeadChangelist );
				}

				// Find label to sync to
				Builder.SyncedLabel = Parent.GetLabelToSync();
				string LabelName = Builder.SyncedLabel.TrimStart( "@".ToCharArray() );

				if( Builder.LabelInfo.RevisionType == RevisionType.Label || Builder.LabelInfo.RevisionType == RevisionType.BuilderLabel )
				{
					if( !SCC.GetLabelInfo( Builder, LabelName, null ) )
					{
						State = COMMANDS.SCC_Sync;
						Builder.Write( "P4ERROR: Sync - Non existent label, cannot sync to: " + LabelName );
						return MODES.Finalise;
					}
				}

				// Name of a build type that we wish to get the last good build from
				SCC.SyncToRevision( Builder, Builder.SyncedLabel, "/..." );
	
				// Don't get the list of changelists for CIS type builds or jobs
				if( Builder.LabelInfo.RevisionType == RevisionType.Label 
					|| Builder.LabelInfo.RevisionType == RevisionType.BuilderLabel 
					|| Builder.LabelInfo.RevisionType == RevisionType.Head )
				{
					SCC.GetChangesSinceLastBuild( Builder );
				}

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_Sync;
				Builder.Write( "Error: exception while calling sync" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_SyncToHead()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_SyncToHead ), false );

				SCC.SyncToHead( Builder, Builder.GetCurrentCommandLine() );

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_SyncToHead;
				Builder.Write( "Error: exception while calling sync to head" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_ArtistSync()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_ArtistSync ), false );

				if( Builder.LabelInfo.Game.Length == 0 )
				{
					Builder.Write( "ERROR: Must artist sync to a specific game!" );
				}
				else
				{
					// Name of a build type that we wish to get the last good build from
					Builder.SyncedLabel = Parent.GetLabelToSync();

					// Don't get the list of changelists for CIS type builds or jobs
					if( Builder.LabelInfo.RevisionType == RevisionType.Label )
					{
						// Hack the get changes to return all changes to #head
						Builder.LabelInfo.RevisionType = RevisionType.Head;
						SCC.GetChangesSinceLastBuild( Builder );
						Builder.LabelInfo.RevisionType = RevisionType.Label;
					}

					GameConfig Config = Builder.CreateGameConfig();
					SCC.SyncToHead( Builder, Config.GetArtistSyncRulesFileName() );

					ArtistSyncRules Rules = UnrealControls.XmlHandler.ReadXml<ArtistSyncRules>( Config.GetArtistSyncRulesFileName() );
					if( Rules.PromotionLabel.Length > 0 )
					{
						CleanArtistSyncFiles( Rules.FilesToClean );

						foreach( string SyncCommand in Rules.Rules )
						{
							if( SyncCommand.Contains( "%LABEL_TO_SYNC_TO%" ) )
							{
								SCC.SyncToRevision( Builder, Builder.SyncedLabel, SyncCommand.Replace( "%LABEL_TO_SYNC_TO%", "" ) );
							}
							else
							{
								SCC.SyncToRevision( Builder, "#head", SyncCommand );
							}
						}
					}
				}

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_ArtistSync;
				Builder.Write( "Error: exception while calling artist sync" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_GetChanges()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_GetChanges ), false );

				// Name of a build type that we wish to get the last good build from
				Builder.SyncedLabel = Parent.GetLabelToSync();

				SCC.GetChangesSinceLastBuild( Builder );

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_GetChanges;
				Builder.Write( "Error: exception while getting user changes" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_SyncSingleChangeList()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_SyncSingleChangeList ), false );

				// Name of a build type that we wish to get the last good build from
				SCC.SyncSingleChangeList( Builder );

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_SyncSingleChangeList;
				Builder.Write( "Error: exception while calling sync single changelist" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_Checkout()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_Checkout ), false );

				SCC.CheckoutFileSpec( Builder, Builder.GetCurrentCommandLine() );

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_Checkout;
				Builder.Write( "Error: exception while calling checkout" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_OpenForDelete()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_OpenForDelete ), false );

				SCC.OpenForDeleteFileSpec( Builder, Builder.GetCurrentCommandLine() );

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_OpenForDelete;
				Builder.Write( "Error: exception while calling open for delete" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_CheckoutGame( GameConfig Config, bool CheckoutConfig )
		{
			List<string> Executables = Config.GetExecutableNames( Builder.BranchDef.Version );
			foreach( string Executable in Executables )
			{
				SCC.CheckoutFileSpec( Builder, Executable );
			}

			List<string> SymbolFiles = Config.GetSymbolFileNames( Builder.BranchDef.Version );
			foreach( string SymbolFile in SymbolFiles )
			{
				SCC.CheckoutFileSpec( Builder, SymbolFile );
			}

			if( CheckoutConfig )
			{
				string CfgFileName = Config.GetCfgName( Builder.BranchDef.Version );
				SCC.CheckoutFileSpec( Builder, CfgFileName );
			}

			List<string> FilesToClean = Config.GetFilesToClean( Builder.BranchDef.Version );
			foreach( string FileToClean in FilesToClean )
			{
				try
				{
					File.Delete( FileToClean );
				}
				catch
				{
					Parent.SendWarningMail( "Failed to delete", FileToClean, false );
				}
			}

			return MODES.Finalise;
		}

		public MODES SCC_CheckoutDLC()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_CheckoutDLC ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 1 )
				{
					Builder.Write( "Error: incorrect number of parameters for CheckoutDLC <DLCName>." );
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();
					string DLCFileName = Config.GetDLCFileName( Parms[0] );

					SCC.CheckoutFileSpec( Builder, DLCFileName );
				}

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_CheckoutDLC;
				Builder.Write( "Error: exception while calling checkout DLC." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		private MODES SCC_GenericCheckout( COMMANDS CheckoutType )
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( CheckoutType ), false );

				GameConfig Config = null;
				switch( CheckoutType )
				{
				case COMMANDS.SCC_CheckoutGame:
					Config = Builder.AddCheckedOutGame();
					SCC_CheckoutGame( Config, false );
					break;

				case COMMANDS.SCC_CheckoutGFWLGame:
					Config = Builder.AddCheckedOutGame();
					SCC_CheckoutGame( Config, true );
					break;

				case COMMANDS.SCC_CheckoutGADCheckpoint:
					Config = Builder.CreateGameConfig();
					string FileName = Config.GetGADCheckpointFileName();
					SCC.CheckoutFileSpec( Builder, FileName );
					break;

				case COMMANDS.SCC_CheckoutLayout:
					Config = Builder.CreateGameConfig();
					string LayoutFileName = Config.GetLayoutFileName( Builder.SkuName, Builder.GetLanguages().ToArray(), Builder.GetTextLanguages().ToArray() );
					SCC.CheckoutFileSpec( Builder, LayoutFileName );
					break;

				case COMMANDS.SCC_CheckoutHashes:
					Config = Builder.CreateGameConfig();
					string HashesFileName = Config.GetHashesFileName();
					SCC.CheckoutFileSpec( Builder, HashesFileName );
					break;

				case COMMANDS.SCC_CheckoutAFTScreenshots:
					Config = Builder.CreateGameConfig();
					string AFTReferenceFolder = Config.GetAFTReferenceFolder( "DevKit", Builder.AFTTestMap );
					SCC.CheckoutFileSpec( Builder, AFTReferenceFolder + "/*.png" );
					break;

				case COMMANDS.SCC_CheckoutConnCache:
					Config = Builder.CreateGameConfig();
					string ConnCacheName = Config.GetConnCacheName();
					SCC.CheckoutFileSpec( Builder, ConnCacheName );
					break;

				case COMMANDS.SCC_CheckoutShader:
					Config = Builder.CreateGameConfig();
					string ShaderFile = Config.GetRefShaderName();
					SCC.CheckoutFileSpec( Builder, ShaderFile );
					break;
				}

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = CheckoutType;
				Builder.Write( "Error: exception while calling generic checkout." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_CheckoutGame()
		{
			return SCC_GenericCheckout( COMMANDS.SCC_CheckoutGame );
		}

		public MODES SCC_CheckoutGFWLGame()
		{
			return SCC_GenericCheckout( COMMANDS.SCC_CheckoutGFWLGame );
		}

		public MODES SCC_CheckoutGADCheckpoint()
		{
			return SCC_GenericCheckout( COMMANDS.SCC_CheckoutGADCheckpoint );
		}

		public MODES SCC_CheckoutShader()
		{
			return SCC_GenericCheckout( COMMANDS.SCC_CheckoutShader );
		}

		public MODES SCC_CheckoutLayout()
		{
			return SCC_GenericCheckout( COMMANDS.SCC_CheckoutLayout );
		}

		public MODES SCC_CheckoutHashes()
		{
			return SCC_GenericCheckout( COMMANDS.SCC_CheckoutHashes );
		}

		public MODES SCC_CheckoutAFTScreenshots()
		{
			return SCC_GenericCheckout( COMMANDS.SCC_CheckoutAFTScreenshots );
		}

		public MODES SCC_CheckoutConnCache()
		{
			return SCC_GenericCheckout( COMMANDS.SCC_CheckoutConnCache );
		}

		public MODES SCC_CheckoutDialog()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_CheckoutDialog ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 2 )
				{
					Builder.Write( "Error: not enough parameters for CheckoutDialog." );
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig( Parms[0] );
					Queue<string> Languages = Builder.GetLanguages();
					Queue<string> ValidLanguages = new Queue<string>();

					foreach( string Language in Languages )
					{
						string DialogFile = Config.GetDialogFileName( Builder.DLCName, Language, Parms[1] );
						if( SCC.CheckoutFileSpec( Builder, DialogFile ) )
						{
							ValidLanguages.Enqueue( Language );
						}
					}

					Builder.SetValidLanguages( ValidLanguages );
				}

				// Some files are allowed to not exist (and fail checkout)
				State = COMMANDS.None;
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_CheckoutDialog;
				Builder.Write( "Error: exception while calling checkout dialog" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_CheckoutFonts()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_CheckoutFonts ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 2 )
				{
					Builder.Write( "Error: not enough parameters for CheckoutFonts" );
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig( Parms[0] );
					Queue<string> Languages = Builder.GetLanguages();
					Queue<string> ValidLanguages = new Queue<string>();

					foreach( string Language in Languages )
					{
						string FontFile = Config.GetFontFileName( Language, Parms[1] );
						if( SCC.CheckoutFileSpec( Builder, FontFile ) )
						{
							ValidLanguages.Enqueue( Language );
						}
					}

					Builder.SetValidLanguages( ValidLanguages );
				}

				// Some files are allowed to not exist (and fail checkout)
				State = COMMANDS.None;
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_CheckoutFonts;
				Builder.Write( "Error: exception while calling checkout dialog" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_CheckoutLocPackage()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_CheckoutLocPackage ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 1 )
				{
					Builder.Write( "Error: not enough parameters for CheckoutLocPackage <Package>" );
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig( "Example" );
					Queue<string> Languages = Builder.GetLanguages();
					Queue<string> ValidLanguages = new Queue<string>();

					foreach( string Language in Languages )
					{
						string PackageFile = Config.GetPackageFileName( Language, Parms[0] );
						if( SCC.CheckoutFileSpec( Builder, PackageFile ) )
						{
							ValidLanguages.Enqueue( Language );
						}
					}

					Builder.SetValidLanguages( ValidLanguages );
				}

				// Some files are allowed to not exist (and fail checkout)
				State = COMMANDS.None;
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_CheckoutFonts;
				Builder.Write( "Error: exception while calling checkout dialog" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_CheckoutGDF()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_CheckoutGDF ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 2 )
				{
					Builder.Write( "Error: incorrect number of parameters for CheckoutGDF <Game> <GDFRootPath>" );
					State = COMMANDS.SCC_CheckoutGDF;
				}
				else
				{
					Queue<string> Languages = Builder.GetLanguages();

					foreach( string Lang in Languages )
					{
						string GDFFileName = Parms[1] + "/" + Lang.ToUpper() + "/" + Parms[0] + "Game.gdf.xml";
						SCC.CheckoutFileSpec( Builder, GDFFileName );
					}
				}

				// Some files are allowed to not exist (and fail checkout)
				State = COMMANDS.None;
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_CheckoutGDF;
				Builder.Write( "Error: exception while calling checkout GDF" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_CheckoutCat()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_CheckoutCat ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 1 )
				{
					Builder.Write( "Error: not enough parameters for CheckoutCat" );
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig( Parms[0] );

					List<string> CatNames = Config.GetCatNames( Builder.BranchDef.Version );
					foreach( string CatName in CatNames )
					{
						SCC.CheckoutFileSpec( Builder, CatName );
					}
				}

				State = COMMANDS.None;
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_CheckoutCat;
				Builder.Write( "Error: exception while calling checkoutcat" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_GetNextChangelist()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_GetNextChangelist ), false );

				if( Builder.GetCurrentCommandLine().Length == 0 )
				{
					Builder.LabelInfo.Changelist = SCC.GetNextChangelist( Builder, "//" + Builder.BranchDef.CurrentClient.ClientName + "/" + Builder.BranchDef.Branch + "/..." );
				}
				else
				{
					Builder.LabelInfo.Changelist = SCC.GetNextChangelist( Builder, "//" + Builder.BranchDef.CurrentClient.ClientName + "/" + Builder.BranchDef.Branch + "/" + Builder.GetCurrentCommandLine() );
				}

				if( Builder.LabelInfo.Changelist > 0 )
				{
					Builder.LabelInfo.RevisionType = RevisionType.ChangeList;
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_GetNextChangelist;
				Builder.Write( "Error: exception getting the next changelist" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_Submit()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_Submit ), false );
				SCC.Submit( Builder, false );

				State = SCC.GetErrorLevel();
				if( State == COMMANDS.SCC_Submit )
				{
					// Interrogate P4 and resolve any conflicts
					List<string> FilesNotAtHeadRevision = SCC.AutoResolveFiles( Builder );

					State = SCC.GetErrorLevel();
					if( State == COMMANDS.None )
					{
						SCC.Submit( Builder, true );
						State = SCC.GetErrorLevel();

						// If everything submitted properly and we're protecting the (previous) head revision
						if( State == COMMANDS.None &&
							Builder.IsRestoringNewerChanges &&
							FilesNotAtHeadRevision.Count > 0 )
						{
							foreach( string NextFileToRestore in FilesNotAtHeadRevision )
							{
								// Back out the (just checked in) head revision and then check in the
								// previous one, restoring the newer changes that were at head
								SCC.BackOutHeadRevision( Builder, NextFileToRestore );
							}
							SCC.Submit( Builder, true );
							State = SCC.GetErrorLevel();
						}
					}
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_Submit;
				Builder.Write( "Error: exception while calling submit" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_CreateNewLabel()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_CreateNewLabel ), false );

				SCC.CreateNewLabel( Builder );
				Builder.Dependency = Builder.LabelInfo.GetLabelName();

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_LabelCreateNew;
				Builder.Write( "Error: exception while creating a new Perforce label" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_LabelUpdateDescription()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_LabelUpdateDescription ), false );

				SCC.UpdateLabelDescription( Builder );

				BuilderLinq.UpdateVersioning( Builder.LabelInfo.GetLabelName(), Builder.LabelInfo.BuildVersion.Build, Builder.LabelInfo.Changelist );

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_LabelUpdateDescription;
				Builder.Write( "Error: exception while updating a Perforce label" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_Revert()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_Revert ), false );

				SCC.DeleteEmptyChangelists( Builder );

				SCC.Revert( Builder, "..." );

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_Revert;
				Builder.Write( "Error: exception while calling revert" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_RevertFileSpec()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_RevertFileSpec ), false );

				Parent.Log( "[STATUS] Reverting: '" + Builder.GetCurrentCommandLine() + "'", Color.Magenta );
				SCC.Revert( Builder, Builder.GetCurrentCommandLine() );

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_RevertFileSpec;
				Builder.Write( "Error: exception while calling revert" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SCC_Tag()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_Tag ), false );

				SCC.Tag( Builder );

				State = SCC.GetErrorLevel();
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_Tag;
				Builder.Write( "Error: exception while tagging build" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES GenerateManifest()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.GenerateManifest ), false );

				string[] Parameters = Builder.SplitCommandline();
				if( Parameters.Length > 0 )
				{
					string Executable = "Engine/Intermediate/BuildData/UnrealBuildTool/Development/UnrealBuildTool.exe";
					GameConfig Config = Builder.CreateGameConfig( Parameters[0] );

					string CommandLine = Config.GetUBTCommandLine( Builder.BranchDef.Version, true, false, false, Builder.LabelInfo.bIsTool ) + " -GenerateManifest";
					for( int Parameter = 1; Parameter < Parameters.Length; Parameter++ )
					{
						CommandLine += " " + Parameters[Parameter];
					}

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, GetCWD( Builder.BranchDef.Version ), false );
					State = CurrentBuild.GetErrorLevel();
				}
				else
				{
					Builder.Write( "Error: must specify game or tool name" );
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.GenerateManifest;
				Builder.Write( "Error: exception while generating manifest" );
				Builder.CloseLog();
			}

			return MODES.Monitor;			
		}

		/// <summary>
		/// A container for a binary files (dll, exe) with its associated debug info.
		/// </summary>
		public class FileManifest
		{
			public readonly List<string> FileManifestItems = new List<string>();

			public FileManifest()
			{
			}
		}

		private void HandleSourceControl( string ClientFileSpec )
		{
			// Does the file exist in P4?
			if( SCC.ClientFileExistsInDepot( Builder, ClientFileSpec ) )
			{
				// ... if it does, check it out
				SCC.CheckoutFileSpec( Builder, ClientFileSpec );
			}
			else
			{
				// ... if it doesn't, mark for add
				SCC.MarkForAddFileSpec( Builder, ClientFileSpec );
			}
		}

		public MODES SCC_CheckoutManifest()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SCC_CheckoutManifest ), false );

				string ManifestName = "Engine/Intermediate/BuildData/Manifest.xml";
				FileInfo ManifestInfo = new FileInfo( ManifestName );
				if( !ManifestInfo.Exists )
				{
					Builder.Write( "Error: unable to find manifest: '" + ManifestInfo.FullName + "'" );
					State = COMMANDS.SCC_CheckoutManifest;
					return MODES.Finalise;
				}

				FileManifest Manifest = UnrealControls.XmlHandler.ReadXml<FileManifest>( ManifestInfo.FullName );
				foreach( string Item in Manifest.FileManifestItems )
				{
					HandleSourceControl( Item );
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SCC_CheckoutManifest;
				Builder.Write( "Error: exception while checking out files in a manifest" );
				Builder.CloseLog();
			}

			return MODES.Finalise;	
		}
	}
}

