// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;
using System.IO;
using System.Xml.Serialization;
using System.Xml;

namespace CookerSync
{
	/// <summary>
	/// Summary description for Class1.
	/// </summary>
	partial class CookerSyncApp
	{
		const int ERR_CODE = 1;

		static void WriteLine(string Msg, params object[] Parms)
		{
			string Final = string.Format(Msg, Parms);

			System.Diagnostics.Debug.WriteLine(Final);
			Console.WriteLine(Final);
		}

		static void ShowUsage()
		{
			WriteLine("Copies game content to one or more locations");
			WriteLine("");
			WriteLine("CookerSync <Name> [-Options] [-b <Dir>] [-p <Platform>] [-x <TagSet>] [Destinations...]");
			WriteLine("");
			WriteLine("  Name           : Name of the game to be synchronized");
			WriteLine("  Destinations   : One or more targets (Xbox only) or UNC paths (any platform)");
			WriteLine("                 : Destination required for PC or PS3. If Xbox360, default Xbox is used.");
			WriteLine("");
			WriteLine("Options:");
			WriteLine("  -a, -allownotarget : Disable the error that is given by default when running without any valid targets");
			WriteLine("  -b, -base          : Specify base directory <Dir> to be used");
			WriteLine("                       If not specified defaults to: UnrealEngine3");
			WriteLine("  -c, -crc           : Generate CRC when creating the TOC");
			WriteLine("  -d, -deftarget     : If no targets are specified try and use the default target.");
			WriteLine("  -f, -force         : Force copying of all files regardless of time stamp");
			WriteLine("  -h, -help          : This help text");
			WriteLine("  -i, -invert        : Inverted copying from destination to source (infers -notoc)");
			WriteLine("  -l, -log           : More verbose logging output");
			WriteLine("  -m, -merge         : Use existing TOC file for CRCs");
			WriteLine("  -n, -nosync        : Do not sync files, preview only");
			WriteLine("  -nd, -nodest       : Ignore all destinations (useful for just generating TOCs");
			WriteLine("  -ni, -noninter     : Non interactive mode; don't prompt the user for input.");
			WriteLine("  -notoc             : Tells CookerSync to not save the Table of Contents (TOC) to disk.");
			WriteLine("  -o, -reboot        : Reboot all targets before copying.");
			WriteLine("  -p, -platform      : Specify platform <Platform> to be used");
			WriteLine("                       Can be one of: Xbox360, Xenon, PS3, PC, NGP, IPhone, Android, WiiU, Flash [Default is Xbox360]");
			WriteLine("  -r, -region        : Three letter code determining packages to copy [Default is INT]");
			WriteLine("                     : Used for copying loc packages, all packages of the type _LOC.upk");
			WriteLine("  -s, -sleep         : Specifies sleep (in ms) between each copy and verify to ease network hogging");
			WriteLine("                     : Default is 25ms");
			WriteLine("  -t, -texformat     : Specifies the texture format extension to the CookedAndroid directory (DXT, PVRTC, ATITC, ETC)");
			WriteLine("                     : Default is no extension (will sync from CookedAndroid, not CookedAndroid_XXX)");
			WriteLine("  -v, -verify        : Verify that the CRCs match between source and destination");
			WriteLine("                       Use in combination with -merge and/or -crc [copying to PC only]");
			WriteLine("  -var Key=Value     : Passes in additional replacement key/value pairs");
			WriteLine("  -x, -tagset        : Only sync files in tag set <TagSet>. See CookerSync.xml for the tag sets.");
			WriteLine("  -y, -syncdir       : Force the next parameter to be a directory destination (useful if syncing to a subdirectory)");

			WriteLine("");
			WriteLine("Examples:");
			WriteLine("  Copy ExampleGame to the default xbox:");
			WriteLine("\tCookerSync Example");
			WriteLine("");
			WriteLine("  Copy ExampleGame to the xbox named tango and xbox named bravo:");
			WriteLine("\tCookerSync Example tango bravo");
			WriteLine("");
			WriteLine("  Copy ExampleGame for PS3 to the to PC path \\\\Share\\");
			WriteLine("\tCookerSync Example -p PS3 \\\\Share\\");
			WriteLine("");
			WriteLine("  Copy ExampleGame to the xbox named tango and used UnrealEngine3-Demo as");
			WriteLine("  the Xbox base directory:");
			WriteLine("\tCookerSync Example tango -base UnrealEngine3-Demo");
			WriteLine("");
			WriteLine("  Copy ExampleGame to the PC Path C:\\DVD\\ generate CRCs for the Table of");
			WriteLine("  contents and verify the CRC on the destination side:");
			WriteLine("\tCookerSync Example -crc -v C:\\DVD\\");
			WriteLine("");
			WriteLine("  Preview the copy of ExampleGame to xbox named tango and PC path \\\\Share\\:");
			WriteLine("\tCookerSync Example -n tango \\\\Share\\");
			WriteLine("");
			WriteLine("  Verify the copy of ExampleGame at C:\\DVD\\ without performing any copying:");
			WriteLine("\tCookerSync Example -n -f -crc -v C:\\DVD\\");
			WriteLine("");
			WriteLine("  Verify the copy of ExampleGame at C:\\DVD\\ using the existing TOC file:");
			WriteLine("\tCookerSync Example -v -m -crc -n -f C:\\DVD\\");
			WriteLine("");
			WriteLine("  Only generate the TOC with CRCs for ExampleGame:");
			WriteLine("\tCookerSync Example -crc -nd");
			WriteLine("");
			WriteLine("  Merge CRCs from existing TOC file and generate CRCs for any missing files");
			WriteLine("  or files that have mismatched lengths.  Write the resulting TOC to disk");
			WriteLine("  without doing anything else:");
			WriteLine("\tCookerSync Example -m -crc -n -nd");
		}

		static void Output(object sender, ConsoleInterface.OutputEventArgs e)
		{
			Console.Write(e.Message);
			System.Diagnostics.Debug.Write(e.Message);
		}

		static void Log( Color TextColour, string Line )
		{
			Console.WriteLine( Line );
			System.Diagnostics.Debug.WriteLine( Line );
		}

		/// <summary>
		/// Main entry point
		/// </summary>
		/// <param name="Args">Command line arguments</param>
		/// <returns></returns>
		[STAThread]
		static int Main(string[] Args)
		{
			// default to successful
			bool bWasSuccessful = true;

			Application.EnableVisualStyles();

			if(Args.Length == 0)
			{
				ShowUsage();
				return 1;
			}

			if(Args[0].StartsWith("-"))
			{
				ShowUsage();
				return 1;
			}

			// Make sure the current directory is set to the directory that CookerSync is in (Binaries)
			Environment.CurrentDirectory = System.IO.Path.GetDirectoryName( Application.ExecutablePath );
			string CWD = Environment.CurrentDirectory.ToLower();
			if(!CWD.EndsWith("binaries"))
			{
				WriteLine("Error: CookerSync must be run from the 'Binaries' folder");
				return 1;
			}

			ConsoleInterface.TOCSettings PlatSettings = new ConsoleInterface.TOCSettings(new ConsoleInterface.OutputHandlerDelegate(Output));
			string PlatformToSyncName = "Xbox360";
			string SyncTagSet = "";
			List<string> Languages = new List<string>();
			List<string> TextureExtensions = null;
			bool bNoTargetIsFailureCondition = true;
			bool bUseDefaultTarget = false;

			// if any params were specified, parse them
			if(Args.Length > 0)
			{
				PlatSettings.GameName = Args[0];
				if (PlatSettings.GameName.EndsWith("Game"))
				{
					PlatSettings.GameName = PlatSettings.GameName.Remove(PlatSettings.GameName.Length - 4);
				}

				for( int ArgIndex = 1; ArgIndex < Args.Length; ArgIndex++ )
				{
					if( ( String.Compare( Args[ArgIndex], "-a", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-allownotarget", true ) == 0 ) )
					{
						bNoTargetIsFailureCondition = false;
					}
					else if( ( String.Compare( Args[ArgIndex], "-b", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-base", true ) == 0 ) )
					{
						// make sure there is another param after this one
						if( Args.Length > ArgIndex + 1 )
						{
							// the next param is the base directory
							PlatSettings.TargetBaseDirectory = Args[ArgIndex + 1];

							// skip over it
							ArgIndex++;
						}
						else
						{
							WriteLine( "Error: No branch specified (use -h to see usage)." );
							return ERR_CODE;
						}
					}
					else if( ( String.Compare( Args[ArgIndex], "-c", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-crc", true ) == 0 ) )
					{
						PlatSettings.ComputeCRC = true;
					}
					else if( ( String.Compare( Args[ArgIndex], "-d", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-deftarget", true ) == 0 ) )
					{
						bUseDefaultTarget = true;
					}
					else if( ( String.Compare( Args[ArgIndex], "-f", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-force", true ) == 0 ) )
					{
						PlatSettings.Force = true;
					}
					else if( ( String.Compare( Args[ArgIndex], "-h", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-help", true ) == 0 ) )
					{
						ShowUsage();
						return 0;
					}
					else if( ( String.Compare( Args[ArgIndex], "-i", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-invert", true ) == 0 ) )
					{
						PlatSettings.bInvertedCopy = true;
						PlatSettings.GenerateTOC = false;
					}
					else if( ( String.Compare( Args[ArgIndex], "-l", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-log", true ) == 0 ) )
					{
						PlatSettings.VerboseOutput = true;
					}
					else if( ( String.Compare( Args[ArgIndex], "-m", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-merge", true ) == 0 ) )
					{
						PlatSettings.MergeExistingCRC = true;
					}
					else if( ( String.Compare( Args[ArgIndex], "-n", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-nosync", true ) == 0 ) )
					{
						PlatSettings.NoSync = true;
					}
					else if ((String.Compare(Args[ArgIndex], "-nd", true) == 0) || (String.Compare(Args[ArgIndex], "-nodest", true) == 0))
					{
						PlatSettings.NoDest = true;
					}
					else if (String.Compare(Args[ArgIndex], "-ni", true) == 0 || String.Compare(Args[ArgIndex], "-noninter", true) == 0)
					{
						PlatSettings.bNonInteractive = true;
					}
					else if( String.Compare( Args[ArgIndex], "-notoc", true ) == 0 )
					{
						PlatSettings.GenerateTOC = false;
					}
					else if( String.Compare( Args[ArgIndex], "-o", true ) == 0 || String.Compare( Args[ArgIndex], "-reboot", true ) == 0 )
					{
						PlatSettings.bRebootBeforeCopy = true;
					}
					else if( ( String.Compare( Args[ArgIndex], "-p", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-platform", true ) == 0 ) )
					{
						// make sure there is another param after this one
						if( Args.Length > ArgIndex + 1 )
						{
							// the next param is the base directory
							PlatformToSyncName = Args[ArgIndex + 1];

							if( String.Compare( PlatformToSyncName, "xenon", true ) == 0 )
							{
								PlatformToSyncName = ConsoleInterface.PlatformType.Xbox360.ToString();
							}

							// skip over it
							ArgIndex++;
						}
						else
						{
							WriteLine( "Error: No platform specified (use -h to see usage)." );
							return ERR_CODE;
						}
					}
					else if( ( String.Compare( Args[ArgIndex], "-r", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-region", true ) == 0 ) )
					{
						// make sure there is another param after this one
						if( Args.Length > ArgIndex + 1 )
						{
							// the next param is the language
							Languages.Add( Args[ArgIndex + 1].ToUpper() );

							// skip over it
							ArgIndex++;
						}
						else
						{
							WriteLine( "Error: No region specified (use -h to see usage)." );
							return ERR_CODE;
						}
					}
					else if( ( String.Compare( Args[ArgIndex], "-v", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-verify", true ) == 0 ) )
					{
						PlatSettings.VerifyCopy = true;
					}
					else if( ( String.Compare( Args[ArgIndex], "-var", true ) == 0 ) )
					{
						// make sure there is another param after this one
						if( Args.Length > ArgIndex + 1 )
						{
							string[] Tokens = Args[ArgIndex + 1].Split( "=".ToCharArray() );

							// make sure we split properly 
							if( Tokens.Length == 2 )
							{
								PlatSettings.GenericVars.Add( Tokens[0], Tokens[1] );
							}

							// skip over it
							ArgIndex++;
						}
						else
						{
							WriteLine( "Error: Incorrect variable setting (use -h to see usage)." );
							return ERR_CODE;
						}
					}
					else if( ( String.Compare( Args[ArgIndex], "-x", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-tagset", true ) == 0 ) )
					{
						// make sure there is another param after this one
						if( Args.Length > ArgIndex + 1 )
						{
							// next param is the tagset
							SyncTagSet = Args[ArgIndex + 1];

							// skip over it
							ArgIndex++;
						}
						else
						{
							WriteLine( "Error: No tag set specified (use -h to see usage)." );
							return ERR_CODE;
						}
					}
					else if( ( String.Compare( Args[ArgIndex], "-y", true ) == 0 ) || ( String.Compare( Args[ArgIndex], "-syncdir", true ) == 0 ) )
					{
						// make sure there is another param after this one
						if (Args.Length > ArgIndex + 1)
						{
							// next param is a directory to sync to
							PlatSettings.DestinationPaths.Add(Args[ArgIndex + 1]);

							// skip over it
							ArgIndex++;
						}
						else
						{
							WriteLine("Error: No directory specified (use -h to see usage).");
							return ERR_CODE;
						}

					}
					else if ((String.Compare(Args[ArgIndex], "-t", true) == 0) || (String.Compare(Args[ArgIndex], "-texformat", true) == 0))
					{
						// make sure there is another param after this one
						if (Args.Length > ArgIndex + 1)
						{
							// make sure we can store it
							if (TextureExtensions == null)
							{
								TextureExtensions = new List<string>();
							}

							// the next param is a texture format
							TextureExtensions.Add(Args[ArgIndex + 1]);

							// skip over it
							ArgIndex++;
						}
						else
						{
							WriteLine("Error: No tex format specified (use -h to see usage).");
							return ERR_CODE;
						}
					}
					else if (Args[ArgIndex].StartsWith("-"))
					{
						WriteLine("Error: '" + Args[ArgIndex] + "' is not a valid option (use -h to see usage).");
						return ERR_CODE;
					}
					else
					{
						// is this a PC destination path or zip file?
						if (IsDirectory(Args[ArgIndex]))
						{
							if (Args[ArgIndex].ToLower().EndsWith(".zip"))
							{
								PlatSettings.ZipFiles.Add(Args[ArgIndex]);
							}
							else if (Args[ArgIndex].ToLower().EndsWith(".iso"))
							{
								PlatSettings.IsoFiles.Add(Args[ArgIndex]);
							}
							else
							{
								PlatSettings.DestinationPaths.Add(Args[ArgIndex]);
							}
						}
						else
						{
							PlatSettings.TargetsToSync.Add(Args[ArgIndex]);
							bWasSuccessful = true;
						}
					}
				}
			}

			if(Languages.Count > 0)
			{
				// Make sure INT is the last language
				if( Languages.Contains( "INT" ) )
				{
					Languages.Remove( "INT" );
					Languages.Add( "INT" );
				}

				PlatSettings.Languages = Languages.ToArray();
			}
			else
			{
				PlatSettings.Languages = new string[] { "INT" };
			}

			// copy the texture extension list to the platform settings
			if (TextureExtensions != null)
			{
				PlatSettings.TextureExtensions = TextureExtensions.ToArray();
			}

			// Check for a single destination path when -invert is specified
			if( PlatSettings.bInvertedCopy )
			{
				if( PlatSettings.DestinationPaths.Count != 1 )
				{
					WriteLine( "Error: Only one destination allowed with inverted copy!" );
					bWasSuccessful = false;
				}

				if( PlatSettings.TargetsToSync.Count > 0 )
				{
					WriteLine( "Error: Inverted copy only supported from PC to PC! (not consoles)" );
					bWasSuccessful = false;
				}

				if( PlatSettings.ZipFiles.Count > 0 )
				{
					WriteLine( "Error: Inverted copy only supported from PC to PC! (not zips)" );
					bWasSuccessful = false;
				}

				if( PlatSettings.IsoFiles.Count > 0 )
				{
					WriteLine( "Error: Inverted copy only supported from PC to PC! (not isos)" );
					bWasSuccessful = false;
				}
			}

			if( SyncTagSet.Length == 0 )
			{
				WriteLine( "Warning: Using default tag set: ConsoleSync" );
				SyncTagSet = "ConsoleSync";
			}

			// Load in CookerSync.xml
			if( bWasSuccessful && !ConsoleInterface.DLLInterface.LoadCookerSyncManifest() )
			{
				WriteLine( "Error: Failed to load CookerSync.xml!" );
				bWasSuccessful = false;
			}

			// Load in the platform support dlls
			ICollection<ConsoleInterface.Platform> Platforms = ConsoleInterface.DLLInterface.Platforms;
			ConsoleInterface.Platform PlatformToSync = null;

			if( bWasSuccessful && PlatformToSyncName != null )
			{
				for( uint CurPlatformBit = 1; CurPlatformBit < ( uint )ConsoleInterface.PlatformType.All && CurPlatformBit != 0; CurPlatformBit <<= 1 )
				{
					// Get the platform bit
					ConsoleInterface.PlatformType CurPlatform = ( ConsoleInterface.PlatformType )CurPlatformBit;

					// Does the name match the platform we are requesting?
					if( CurPlatform.ToString().Equals( PlatformToSyncName, StringComparison.OrdinalIgnoreCase ) )
					{
						if( ConsoleInterface.DLLInterface.LoadPlatforms( CurPlatform ) != CurPlatform || !ConsoleInterface.DLLInterface.TryGetPlatform( CurPlatform, ref PlatformToSync ) )
						{
							WriteLine( "Error: Could not load target platform \'{0}\'", PlatformToSyncName );
							bWasSuccessful = false;
						}

						break;
					}
				}

				if( PlatformToSync == null )
				{
					WriteLine( "Error: Platform \'{0}\' is not a valid platform!", PlatformToSyncName );
					bWasSuccessful = false;
				}
				else
				{
					// Set whether the target platform has a case sensitive filesystem (so synced files are uppercased)
					foreach( ConsoleInterface.PlatformDescription PlatformDesc in ConsoleInterface.DLLInterface.Settings.KnownPlatforms )
					{
						if( PlatformDesc.Name.ToLower() == PlatformToSyncName )
						{
							PlatSettings.bCaseSensitiveFileSystem = PlatformDesc.bCaseSensitiveFileSystem;
							break;
						}
					}
				}
			}
			else
			{
				WriteLine("Error: No platform name supplied!");
				bWasSuccessful = false;
			}

			// the only platform at the moment that needs to sync is Xbox 360
			if(bWasSuccessful && PlatformToSync.NeedsToSync)
			{
				if( PlatformToSync.EnumerateAvailableTargets() == 0 )
				{
					WriteLine("Warning: The platform \'{0}\' does not have any targets.", PlatformToSyncName);
				}
			}

			// Set the default target if required
			if(PlatSettings.TargetsToSync.Count == 0 && bUseDefaultTarget)
			{
				PlatformToSync.EnumerateAvailableTargets();
				ConsoleInterface.PlatformTarget DefTarget = PlatformToSync.DefaultTarget;

				if(DefTarget != null)
				{
					PlatSettings.TargetsToSync.Add(DefTarget.TargetManagerName);
				}
			}

			// validate that we have a destination if needed
			if( PlatSettings.TargetsToSync.Count == 0 
				&& PlatSettings.DestinationPaths.Count == 0 
				&& PlatSettings.ZipFiles.Count == 0
				&& PlatSettings.IsoFiles.Count == 0
				&& !PlatSettings.NoDest
				&& bNoTargetIsFailureCondition )
			{
				WriteLine("Error: No destination found. Use -h for help.");
				bWasSuccessful = false;
			}

			if(bWasSuccessful)
			{
				// I hate doing this but I guess I should unless we want to change the cmd line all of the
				// build machines are using? (or at least the cmd line QA is using :p)
				if(!PlatSettings.GameName.EndsWith("Game", StringComparison.OrdinalIgnoreCase))
				{
					PlatSettings.GameName += "Game";
				}

				if(!Directory.Exists(Path.Combine(Directory.GetCurrentDirectory(), "..\\" + PlatSettings.GameName )))
				{
					WriteLine("Error: '" + Args[0] + "' is not a valid game name.");
					bWasSuccessful = false;
				}
				else
				{
					PlatSettings.GameOptions = UnrealControls.XmlHandler.ReadXml<ConsoleInterface.GameSettings>( "..\\" + PlatSettings.GameName + "\\Build\\CookerSync_game.xml" );

					try
					{
						// try to sync to the given xbox
						bWasSuccessful = PlatformToSync.TargetSync( PlatSettings, SyncTagSet, SaveIso, SaveZip );
					}
					catch(System.Exception e)
					{
						bWasSuccessful = false;
						WriteLine(e.ToString());
					}
				}
			}

			// Fix for pure virtual call error.
			foreach( ConsoleInterface.Platform P in ConsoleInterface.DLLInterface.Platforms )
			{
				P.Dispose();
			}

			// did we succeed?
			return bWasSuccessful ? 0 : ERR_CODE;
		}

		static private bool IsDirectory(String Directory)
		{
			if(Directory.IndexOf('\\') == -1)
			{
				return false;
			}

			return true;
		}
	}
}

