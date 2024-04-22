/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;

namespace UnrealLoc
{
    public class ObjectEntry
    {
        private UnrealLoc Main;
        private LanguageInfo Lang;
        public ObjectEntryHandler Owner;
        private LocEntryHandler ObjectLocEntryHandler;

        public string ObjectName { get; set; }
        public bool HasNewLocEntries { get; set; }
		public int ObjectLineNumber { get; set; }

        public int GetLocEntryCount()
        {
            return ( ObjectLocEntryHandler.GetCount() );
        }

        public List<LocEntry> GetLocEntries()
        {
            return ( ObjectLocEntryHandler.GetLocEntries() );
        }

        public ObjectEntry( UnrealLoc InMain, LanguageInfo InLang, ObjectEntryHandler InOwner, string InName )
        {
            ObjectName = InName;
            Main = InMain;
            Lang = InLang;
            Owner = InOwner;
			ObjectLineNumber = Owner.LineCount;
			ObjectLocEntryHandler = new LocEntryHandler( Main, Lang, this );
        }

        public ObjectEntry( UnrealLoc InMain, LanguageInfo InLang, ObjectEntryHandler InOwner, ObjectEntry DefaultOE )
        {
            ObjectName = DefaultOE.ObjectName;
            Main = InMain;
            Lang = InLang;
            Owner = InOwner;
			ObjectLineNumber = 0;
			ObjectLocEntryHandler = new LocEntryHandler( Main, Lang, this );
        }

        public void GenerateLocEntries( ObjectEntry DefaultOE )
        {
            ObjectLocEntryHandler.GenerateLocEntries( DefaultOE );
        }

        public void RemoveOrphans()
        {
            ObjectLocEntryHandler.RemoveOrphans();
        }

        public bool WriteLocFiles( StreamWriter File )
        {
            Main.Log( UnrealLoc.VerbosityLevel.Verbose, " ... creating loc object: [" + ObjectName + "]", Color.Black );

            File.WriteLine( "[" + ObjectName + "]" );
            ObjectLocEntryHandler.WriteLocFiles( File );
            File.WriteLine();

            return ( true );
        }

        public bool WriteDiffLocFiles( StreamWriter File )
        {
            Main.Log( UnrealLoc.VerbosityLevel.Verbose, " ... creating loc diff object: [" + ObjectName + "]", Color.Black );

            File.WriteLine( "[" + ObjectName + "]" );
            ObjectLocEntryHandler.WriteDiffLocFiles( File );
            File.WriteLine();

            return ( true );
        }

        public void AddLine( string Line )
        {
            ObjectLocEntryHandler.AddLocEntry( Line );
        }

        public void ReplaceLine( string Line, bool AddNew )
        {
            ObjectLocEntryHandler.ReplaceLocEntry( Line, AddNew );
        }
    }

    public class ObjectEntryHandler
    {
        private UnrealLoc Main = null;
        private LanguageInfo Lang = null;
        private List<ObjectEntry> ObjectEntries;
        public FileEntry Owner = null;
        public int LineCount;

        public ObjectEntryHandler( UnrealLoc InMain, LanguageInfo InLang, FileEntry InFileEntry )
        {
            Main = InMain;
            Lang = InLang;
            Owner = InFileEntry;
            ObjectEntries = new List<ObjectEntry>();
        }

        public List<ObjectEntry> GetObjectEntries()
        {
            return ( ObjectEntries );
        }

        public int GetObjectCount()
        {
            return ( ObjectEntries.Count );
        }

        private ObjectEntry CreateObject( ObjectEntry DefaultOE )
        {
            ObjectEntry ObjectElement = new ObjectEntry( Main, Lang, this, DefaultOE );
            ObjectEntries.Add( ObjectElement );

            Lang.ObjectsCreated++;

            Main.Log( UnrealLoc.VerbosityLevel.Verbose, " ... created object '" + ObjectElement.ObjectName + "'", Color.Blue );
            return ( ObjectElement );
        }

        public bool AddObject( string Name, ref ObjectEntry ObjectElement )
        {
            Main.Log( UnrealLoc.VerbosityLevel.Verbose, " ...... adding '" + Name + "'", Color.Black );

            foreach( ObjectEntry OE in ObjectEntries )
            {
                if( OE.ObjectName == Name )
                {
                    ObjectElement = OE;
                    return( false );
                }
            }

            ObjectElement = new ObjectEntry( Main, Lang, this, Name );
            ObjectEntries.Add( ObjectElement );
            return ( true );
        }

        private ObjectEntry ObjectExists( string DefaultObjectName )
        {
            foreach( ObjectEntry OE in ObjectEntries )
            {
                if( OE.ObjectName == DefaultObjectName )
                {
                    return ( OE );
                }
            }

            return ( null );
        }

        public bool GenerateLocObjects( FileEntry DefaultFileEntry )
        {
            List<ObjectEntry> DefaultObjectEntries = DefaultFileEntry.GetObjectEntries();

            foreach( ObjectEntry DefaultOE in DefaultObjectEntries )
            {
                ObjectEntry LocOE = ObjectExists( DefaultOE.ObjectName );
                if( LocOE == null )
                {
                    LocOE = CreateObject( DefaultOE );
                }

                LocOE.GenerateLocEntries( DefaultOE );
            }

            return ( true );
        }

        public void RemoveOrphans()
        {
            foreach( ObjectEntry OE in ObjectEntries )
            {
                OE.RemoveOrphans();
            }

			List<ObjectEntry> OEs = ObjectEntries;
			ObjectEntries = new List<ObjectEntry>();

			foreach( ObjectEntry OE in OEs )
			{
				if( OE.GetLocEntries().Count > 0 )
				{
					ObjectEntries.Add( OE );
				}
				else
				{
					Main.Warning( Lang, "Removed orphan object: '" + OE.ObjectName + "' for file '" + Owner.RelativeName + "' at line: " + LineCount );
				}
			}

			Lang.NumOrphansRemoved += OEs.Count - ObjectEntries.Count;

        }

        public bool WriteLocFiles( StreamWriter File )
        {
            foreach( ObjectEntry OE in ObjectEntries )
            {
                OE.WriteLocFiles( File );
            }

            return ( true );
        }

        public bool WriteDiffLocFiles( StreamWriter File )
        {
            foreach( ObjectEntry OE in ObjectEntries )
            {
                if( OE.HasNewLocEntries )
                {
                    OE.WriteDiffLocFiles( File );
                }
            }

            return ( true );
        }

        private bool IsObjectName( string Line )
        {
            if( Line.StartsWith( "[" ) )
            {
                if( Line.EndsWith( "]" ) )
                {
                    return ( true );
                }
            }

            return ( false );
        }

        private bool IsComment( string Line )
        {
            if( Line.StartsWith( ";" ) )
            {
                return ( true );
            }

            if( Line.StartsWith( "//" ) )
            {
                return ( true );
            }

            return ( false );
        }

        private string ReadLine( StreamReader Reader )
        {
            LineCount++;
            string Line = "";
            char NextChar;
            do 
            {
                NextChar = ( char )Reader.Read();
                if( NextChar == 0xffff )
                {
                    break;
                }
                else if( NextChar != 0xd && NextChar != 0xa )
                {
                    Line += NextChar;
                }
                else if( NextChar == 0xd )
                {
                    if( Reader.Peek() != 0xa )
                    {
                        Main.Warning( Lang, "Spurious character 13 in line " + LineCount.ToString() + " in '"+ Owner.RelativeName + "'; removing." );
                    }
                }
            }
            while( NextChar != 0xa );

            return( Line.Trim() );
        }

		private class DecoderFallbackOverride : DecoderFallback
		{
			public override DecoderFallbackBuffer CreateFallbackBuffer()
			{
				throw new NotImplementedException();
			}

			public override int MaxCharCount
			{
				get { throw new NotImplementedException(); }
			}
		}

		private bool ValidateEncoding( byte[] Data, Encoding Encoder )
		{
			bool bIsValidEncoding = true;
			try
			{
				Encoding WorkEncoder = ( Encoding )Encoder.Clone();
				WorkEncoder.DecoderFallback = new DecoderFallbackOverride();
				string Temp = WorkEncoder.GetString( Data );
			}
			catch
			{
				bIsValidEncoding = false;
			}

			return ( bIsValidEncoding );
		}

		private Encoding DetectEncoding( string FileName )
		{
			Encoding DetectedEncoding = null;

			FileStream BinaryFile = new FileStream( FileName, FileMode.Open, FileAccess.Read );
			if( BinaryFile != null )
			{
				byte[] Data = new byte[5];
				int BytesRead = BinaryFile.Read( Data, 0, 5 );

				if( BytesRead >= 4 && Data[0] == 0xff && Data[1] == 0xfe && Data[2] == 0 && Data[3] == 0 )
				{
					DetectedEncoding = Encoding.UTF32;
				}
				else if( BytesRead >= 2 && Data[0] == 0xff && Data[1] == 0xfe )
				{
					DetectedEncoding = Encoding.Unicode;
				}
				else if( BytesRead >= 2 && Data[0] == 0xfe && Data[1] == 0xff )
				{
					DetectedEncoding = Encoding.BigEndianUnicode;
				}
				else if( BytesRead >= 3 && Data[0] == 0xef && Data[1] == 0xbb && Data[2] == 0xbf )
				{
					DetectedEncoding = Encoding.UTF8;
				}
				else
				{
					// Read in the entire file for subsequent tests
					Data = new byte[BinaryFile.Length];
					BinaryFile.Read( Data, 0, ( int )BinaryFile.Length );
					BinaryFile.Close();

					// Encoding types to test
					List<Encoding> EncodingTypes = new List<Encoding>()
					{
						Encoding.ASCII,
						Encoding.UTF8
					};

					foreach( Encoding EncodingType in EncodingTypes )
					{
						if( ValidateEncoding( Data, EncodingType ) )
						{
							DetectedEncoding = EncodingType;
							break;
						}
					}
				}
			}

			return ( DetectedEncoding );
		}

		private StreamReader OpenLocFile( string FileName, Encoding EncodingType )
        {
            StreamReader Reader = null;
            Main.Log( UnrealLoc.VerbosityLevel.Informative, " ... loading as " + EncodingType.EncodingName + " '" + FileName + "'", Color.Black );

            try
            {
				Reader = new StreamReader( FileName, EncodingType );
            }
            catch( Exception E )
            {
                Main.Error( Lang, "Failed to open file '" + FileName + "' with error '" + E.Message + "'" );
            }

            return ( Reader );
        }

        public bool FindObjects()
        {
			Encoding EncodingType = DetectEncoding( Owner.RelativeName );
			if( EncodingType == null )
			{
				Main.Error( Lang, "Invalid encoding type for '" + Owner.RelativeName + "' (ASCII, UTF-8, UTF-8 (No BOM), UTF-16, UTF-16 (Big Endian) and UTF-32 are allowed)" );
				return ( false );
			}

			StreamReader Reader = OpenLocFile( Owner.RelativeName, EncodingType );
            if( Reader == null )
            {
                return ( false );
            }

            LineCount = 0;
            bool FoundObjectName = false;
            bool NameLoaded = false;
            string Line = "";

            while( !Reader.EndOfStream )
            {
                if( !NameLoaded )
                {
                    Line = ReadLine( Reader );
                }

                string ObjectName = "Unknown";
                ObjectEntry ObjectElement = null;
                NameLoaded = false;

                if( Line.Length > 0 )
                {
                    if( !IsComment( Line ) )
                    {
                        if( IsObjectName( Line ) )
                        {
                            FoundObjectName = true;

                            ObjectName = Line.Substring( 1, Line.Length - 2 );
                            AddObject( ObjectName, ref ObjectElement );

                            while( !Reader.EndOfStream )
                            {
                                Line = ReadLine( Reader );
                                if( Line.Length > 0 )
                                {
                                    if( !IsComment( Line ) )
                                    {
										if( IsObjectName( Line ) )
                                        {
                                            NameLoaded = true;
                                            break;
                                        }

										ObjectElement.AddLine( Line );
                                    }
                                }
                            }

                            Main.Log( UnrealLoc.VerbosityLevel.Complex, " ......... added " + ObjectElement.GetLocEntryCount() + " keys.", Color.Black );
                        }
                        else
                        {
                            if( !FoundObjectName )
                            {
                                Main.Error( Lang, "Data found before object name in file " + Owner.RelativeName );
                            }
                        }
                    }
                }
            }

            Reader.Close();
            return ( true );
        }

        public bool ImportText( string FileName )
        {
			Encoding EncodingType = DetectEncoding( FileName );
			if( EncodingType == null )
			{
				return ( false );
			}

			StreamReader Reader = OpenLocFile( FileName, EncodingType );
            if( Reader == null )
            {
                return ( false );
            }

            LineCount = 0;
            bool FoundObjectName = false;
            bool NameLoaded = false;
            bool AddNew = ( Lang.LangID == "INT" );
            string Line = "";

            while( !Reader.EndOfStream )
            {
                if( !NameLoaded )
                {
                    Line = ReadLine( Reader );
                }

                string ObjectName = "Unknown";
                ObjectEntry ObjectElement = null;
                NameLoaded = false;

                if( Line.Length > 0 )
                {
                    if( !IsComment( Line ) )
                    {
                        if( IsObjectName( Line ) )
                        {
                            FoundObjectName = true;

                            ObjectName = Line.Substring( 1, Line.Length - 2 );
                            AddObject( ObjectName, ref ObjectElement );

                            while( !Reader.EndOfStream )
                            {
                                Line = ReadLine( Reader );
                                if( Line.Length > 0 )
                                {
                                    if( !IsComment( Line ) )
                                    {
                                        if( IsObjectName( Line ) )
                                        {
                                            NameLoaded = true;
                                            break;
                                        }

                                        ObjectElement.ReplaceLine( Line, AddNew );
                                    }
                                }
                            }
                        }
                        else
                        {
                            if( !FoundObjectName )
                            {
                                Main.Error( Lang, "Data found before object name in file " + FileName );
                            }
                        }
                    }
                }
            }

            Reader.Close();
            return ( true );
        }
    }
}
