/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Xml;
using System.Xml.Serialization;
using Stats;

namespace StatsViewer
{
	/// <summary>
	/// Loads stats data from binary files
	/// </summary>
	public class BinaryStatsDataLoader
	{

		/// <summary>
		/// Types of data found in a binary stats file
		/// </summary>
		enum DataChunkTagType
		{
			/// Unknown or invalid data tag type
			Invalid = 0,

			/// Frame data
			FrameData,

			/// Group descriptions
			GroupDescriptions,

			/// Stat descriptions
			StatDescriptions
		};


		/// <summary>
		/// Load the stats file from binary data
		/// </summary>
		/// <param name="FileName">File name to load</param>
		/// <returns>Returns true if everything loaded OK</returns>
		static public bool LoadStatsFile( string FileName, out StatFile OutStatFile, out string OutErrorMessage )
		{
			OutStatFile = null;
			OutErrorMessage = null;

			// Open the file for reading
			Stream StatsFileStream;
			{
				try
				{
					StatsFileStream =
						new FileStream(
							FileName,
							FileMode.Open,
							FileAccess.Read,
							FileShare.None,
							256 * 1024,	// Buffer size
							false );	// Async?
				}

				catch( Exception E )
				{
					OutErrorMessage = "Error while opening binary file: " + E.ToString();
					return false;
				}
			}


			// Load up the entire file
			Byte[] StatsData;
			{
				try
				{
					// Allocate memory for the data
					StatsData = new Byte[ StatsFileStream.Length ];

					// Load it from disk!
					StatsFileStream.Read( StatsData, 0, ( int )StatsFileStream.Length );
				}
				catch( System.Exception E )
				{
					OutErrorMessage = "Error while loading binary file stream: " + E.ToString();
					return false;
				}
			}


			// Allocate and load the data!
			OutStatFile = new StatFile();
			int BufferPos = 0;
			{
				const string ExpectedHeaderTag = "USTATS";

				// NOTE: If you change the binary stats file format, update this version number as well
				//       as the version number in UnStatsNotifyProviders.cpp!
				const int MinSupportedStatsFileVersion = 1;

				int StatsFileVersion = 0;


				try
				{
					// Header
					{
						// Load the header tag
						string HeaderTag = Encoding.ASCII.GetString( StatsData, BufferPos, ExpectedHeaderTag.Length );
						BufferPos += ExpectedHeaderTag.Length;
						if( HeaderTag != ExpectedHeaderTag )
						{
							OutErrorMessage = "Invalid stats file.  Header tag string is missing";
							return false;
						}

						// Load the version
						StatsFileVersion = ByteStreamConverter.ToInt( StatsData, ref BufferPos );
                        if (StatsFileVersion >= 0x01000000)
                        {
                            ByteStreamConverter.BigEndian = false;
                            BufferPos -= 4;
                            StatsFileVersion = ByteStreamConverter.ToInt(StatsData, ref BufferPos);
                        }
						if( StatsFileVersion < MinSupportedStatsFileVersion )
						{
							OutErrorMessage = "Stats file version mismatch!  Expected at least version " + MinSupportedStatsFileVersion + ", but found version " + StatsFileVersion;
							return false;
						}

						// Load 'Seconds per Cycle'
						OutStatFile.SecondsPerCycle = ByteStreamConverter.ToDouble( StatsData, ref BufferPos );
					}


					if( StatsFileVersion < 3 )
					{
						// Stat descriptions
						{
							int DescCount = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
							for( int CurDescIndex = 0; CurDescIndex < DescCount; ++CurDescIndex )
							{
								Stat NewStatDesc = new Stat();

								NewStatDesc.StatId = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
								NewStatDesc.Name = ByteStreamConverter.ToFString( StatsData, ref BufferPos );

								// NOTE: ParsedType will be copied to Type in the FixupData phase
								NewStatDesc.ParsedType = ByteStreamConverter.ToByte( StatsData, ref BufferPos );

								NewStatDesc.GroupId = ByteStreamConverter.ToWord( StatsData, ref BufferPos );

								// Add to list
								OutStatFile.AppendStatDescription( NewStatDesc );
							}
						}


						// Group descriptions
						{
							int GroupCount = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
							for( int CurDescIndex = 0; CurDescIndex < GroupCount; ++CurDescIndex )
							{
								Group NewGroup = new Group();

								NewGroup.GroupId = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
								NewGroup.Name = ByteStreamConverter.ToFString( StatsData, ref BufferPos );

								// Add to list
								OutStatFile.AppendGroupDescription( NewGroup );
							}
						}
					}
				}
				catch( System.Exception E )
				{
					OutErrorMessage = "Error while parsing binary data from file: " + E.ToString();
					return false;
				}

					
				try
				{
					// Data chunks
					{
						while( BufferPos < StatsData.Length - 1 )
						{
							// Default to loading frame data
							DataChunkTagType DataChunkTag = DataChunkTagType.FrameData;
							if( StatsFileVersion >= 3 )		// Version 3 added data tags
							{
								// Parse data chunk type
								DataChunkTag = ( DataChunkTagType )ByteStreamConverter.ToInt( StatsData, ref BufferPos );
							}


							if( DataChunkTag == DataChunkTagType.FrameData )
							{
								// A new frame!
								Frame NewFrame = new Frame();

								// Frame number
								NewFrame.FrameNumber = ByteStreamConverter.ToInt( StatsData, ref BufferPos );


								// Viewpoint information
								if( StatsFileVersion >= 2 )	// Version 2 added viewport location info
								{
									NewFrame.ViewLocationX = ByteStreamConverter.ToFloat( StatsData, ref BufferPos );
									NewFrame.ViewLocationY = ByteStreamConverter.ToFloat( StatsData, ref BufferPos );
									NewFrame.ViewLocationZ = ByteStreamConverter.ToFloat( StatsData, ref BufferPos );

									NewFrame.ViewRotationYaw = ByteStreamConverter.ToInt( StatsData, ref BufferPos );
									NewFrame.ViewRotationPitch = ByteStreamConverter.ToInt( StatsData, ref BufferPos );
									NewFrame.ViewRotationRoll = ByteStreamConverter.ToInt( StatsData, ref BufferPos );
								}


								// Cycle stats
								int CycleStatCount = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
								for( int CurStatIndex = 0; CurStatIndex < CycleStatCount; ++CurStatIndex )
								{
									Stat NewCycleStat = new Stat();

									NewCycleStat.StatId = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
									NewCycleStat.InstanceId = ByteStreamConverter.ToInt( StatsData, ref BufferPos );
									NewCycleStat.ParentInstanceId = ByteStreamConverter.ToInt( StatsData, ref BufferPos );
									NewCycleStat.ThreadId = ByteStreamConverter.ToInt( StatsData, ref BufferPos );
									NewCycleStat.Value = ByteStreamConverter.ToInt( StatsData, ref BufferPos );
									NewCycleStat.CallsPerFrame = ByteStreamConverter.ToWord( StatsData, ref BufferPos );

									NewFrame.AppendStat( NewCycleStat );
								}

								// Integer counter stats
								int IntegerStatCount = ByteStreamConverter.ToWord( StatsData, ref BufferPos ); ;
								for( int CurStatIndex = 0; CurStatIndex < IntegerStatCount; ++CurStatIndex )
								{
									Stat NewIntegerStat = new Stat();

									NewIntegerStat.StatId = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
									NewIntegerStat.Value = ByteStreamConverter.ToInt( StatsData, ref BufferPos );

									NewFrame.AppendStat( NewIntegerStat );
								}

								// Float counter stats
								int FloatStatCount = ByteStreamConverter.ToWord( StatsData, ref BufferPos ); ;
								for( int CurStatIndex = 0; CurStatIndex < FloatStatCount; ++CurStatIndex )
								{
									Stat NewFloatStat = new Stat();

									NewFloatStat.StatId = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
									NewFloatStat.Value = ByteStreamConverter.ToDouble( StatsData, ref BufferPos );

									NewFrame.AppendStat( NewFloatStat );
								}

								// Add to list
								OutStatFile.AppendFrame( NewFrame );
							}
							else if( DataChunkTag == DataChunkTagType.GroupDescriptions )
							{
								// Group descriptions
								{
									int GroupCount = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
									for( int CurDescIndex = 0; CurDescIndex < GroupCount; ++CurDescIndex )
									{
										Group NewGroup = new Group();

										NewGroup.GroupId = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
										NewGroup.Name = ByteStreamConverter.ToFString( StatsData, ref BufferPos );

										// Add to list
										OutStatFile.AppendGroupDescription( NewGroup );
									}
								}
							}
							else if( DataChunkTag == DataChunkTagType.StatDescriptions )
							{
								// Stat descriptions
								{
									int DescCount = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
									for( int CurDescIndex = 0; CurDescIndex < DescCount; ++CurDescIndex )
									{
										Stat NewStatDesc = new Stat();

										NewStatDesc.StatId = ByteStreamConverter.ToWord( StatsData, ref BufferPos );
										NewStatDesc.Name = ByteStreamConverter.ToFString( StatsData, ref BufferPos );

										// NOTE: ParsedType will be copied to Type in the FixupData phase
										NewStatDesc.ParsedType = ByteStreamConverter.ToByte( StatsData, ref BufferPos );

										NewStatDesc.GroupId = ByteStreamConverter.ToWord( StatsData, ref BufferPos );

										// Add to list
										OutStatFile.AppendStatDescription( NewStatDesc );
									}
								}

							}
							else
							{
								OutErrorMessage = "Unrecognized chunk tag in file: " + DataChunkTag;
								throw new Exception();
							}
						}
					}

				}
				catch( System.Exception E )
				{
					// We encountered an error while parsing the frames section of the file.  Because it
					// may simply be an EOF, we'll handle this as gracefully as possible!
					OutErrorMessage = "Warning: A problem was encountered while parsing data (possible EOF.)  Data may appear incomplete.  Exception: " + E.ToString();
				}

			}


			// We're done loading now, so close the file
			StatsFileStream.Close();


			OutStatFile.FixupRecentItems();

			return true;
		}
	}



	/// <summary>
	/// Loads stats data from XML files
	/// </summary>
	public class XmlStatsDataLoader
	{
		/// <summary>
		/// Whether the last load attempt worked or not
		/// </summary>
		private bool bLoadedOk;


		/// <summary>
		/// Load the stats file from xml data
		/// </summary>
		/// <param name="FileName">File name to load</param>
		/// <returns>Returns true if everything loaded OK</returns>
		public bool LoadStatsFile( string FileName, out StatFile OutStatFile )
		{
			OutStatFile = null;

			bLoadedOk = true;

			try
			{
				// Get the XML data stream to read from
				Stream XmlStream =
					new FileStream( FileName, FileMode.Open, FileAccess.Read, FileShare.None, 256 * 1024, false );

				// Creates an instance of the XmlSerializer class so we can
				// read the object graph
				XmlSerializer ObjSer = new XmlSerializer( typeof( StatFile ) );

				// Add our callbacks for a busted XML file
				ObjSer.UnknownNode += new XmlNodeEventHandler( XmlSerializer_UnknownNode );
				ObjSer.UnknownAttribute += new XmlAttributeEventHandler( XmlSerializer_UnknownAttribute );

				// Create an object graph from the XML data
				OutStatFile = (StatFile)ObjSer.Deserialize( XmlStream );

				// Done with the file so close it
				XmlStream.Close();
			}

			catch( Exception E )
			{
				bLoadedOk = false;
				Console.WriteLine( "Exception parsing XML: " + E.ToString() );
			}

			return bLoadedOk;
		}


		/// <summary>
		/// Logs the node information for debugging purposes
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e">The info about the bad node type</param>
		protected void XmlSerializer_UnknownNode( object sender, XmlNodeEventArgs e )
		{
			bLoadedOk = false;
			Console.WriteLine( "Unknown Node:" + e.Name + "\t" + e.Text );
		}

		/// <summary>
		/// Logs the bad attribute information for debugging purposes
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e">The attribute info</param>
		protected void XmlSerializer_UnknownAttribute( object sender, XmlAttributeEventArgs e )
		{
			bLoadedOk = false;
			System.Xml.XmlAttribute attr = e.Attr;
			Console.WriteLine( "Unknown attribute " + attr.Name + "='" + attr.Value + "'" );
		}
	}


	/// <summary>
	/// A double-buffered version of Windows.Forms.Panel that is faster to paint on and doesn't flicker
	/// </summary>
	public class DoubleBufferedPanel : System.Windows.Forms.Panel
	{
		public DoubleBufferedPanel()
			: base()
		{
			// Enable double-buffering
			DoubleBuffered = true;
			// 			SetStyle( ControlStyles.DoubleBuffer | ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint, true );
			// 			UpdateStyles();
		}
	}

}
