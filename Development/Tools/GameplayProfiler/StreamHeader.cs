/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;

namespace GameplayProfiler
{
	/**
	 * Header written by capture tool
	 */
	public class StreamHeader
	{
		/** Magic number at beginning of data file.					    */
		public const UInt32 ExpectedMagic = 0x07210322;
		public const UInt32 VersionedExpectedMagic = 0x03220721;

		/** Boolean indicating whether the 'magic' number was good.		*/
		public bool bMagicIsGood;

		/** Magic to ensure we're opening the right file.			    */
		public UInt32 Magic;

		/** Version number of file.										*/
		public UInt32 VersionNumber;

		/** Seconds per cycle, used for conversion into "real" time.    */
		public Double SecondsPerCycle;

		/** Offset in file for name table.							    */
		public UInt32 NameTableOffset;
		/** Number of name table entries.							    */
		public UInt32 NameTableEntries;

		/** Offset in file for class hierarchy							*/
		public UInt32 ClassHierarchyOffset;
		/** Number of unique classes in hierarchy.						*/
		public UInt32 ClassCount;

		/**
		 * Constructor, serializing header from passed in stream.
		 * 
		 * @param	BinaryStream	Stream to serialize header from.
		 */
		public StreamHeader(BinaryReader BinaryStream)
		{
			// Serialize the file format magic first.
			Magic = BinaryStream.ReadUInt32();

			bMagicIsGood = (Magic == ExpectedMagic) || (Magic == VersionedExpectedMagic);
			// Stop serializing data if magic number doesn't match. Most likely endian issue.
			if (bMagicIsGood == true)
			{
				if (Magic == VersionedExpectedMagic)
				{
					VersionNumber = BinaryStream.ReadUInt32();
				}
				else
				{
					VersionNumber = 0;
				}
				SecondsPerCycle = BinaryStream.ReadDouble();

				// Name table offset in file and number of entries.
				NameTableOffset = BinaryStream.ReadUInt32();
				NameTableEntries = BinaryStream.ReadUInt32();
				// Class hierarchy offset in file and number of entries.
				ClassHierarchyOffset = BinaryStream.ReadUInt32();
				ClassCount = BinaryStream.ReadUInt32();
			}
		}

		/**
		 * Reads the header information from the passed in stream and returns it. It also returns
		 * a BinaryReader that is endian-appropriate for the data stream. 
		 *
		 * @param	ParserStream		source stream of data to read from
		 * @param	BinaryStream [out]	binary reader used for reading from stream
		 * @return	serialized header
		 */
		public static StreamHeader ReadHeader( Stream ParserStream, out BinaryReader BinaryStream  )
		{
			// Create a binary stream for data, we might toss this later for are an endian swapping one.
			BinaryStream = new BinaryReader(ParserStream,System.Text.Encoding.ASCII);

			// Serialize header.
			StreamHeader Header = new StreamHeader(BinaryStream);

			// Determine whether read file has magic header. If no, try again byteswapped.
			if (Header.bMagicIsGood == false)
			{
				// Seek back to beginning of stream before we retry.
				ParserStream.Seek(0,SeekOrigin.Begin);

				// Use big endian reader. It transparently endian swaps data on read.
				BinaryStream = new BinaryReaderBigEndian(ParserStream);
				
				// Serialize header a second time.
				Header = new StreamHeader(BinaryStream);
			}

			// At this point we should have a valid header. If no, throw an exception.
			if (Header.bMagicIsGood == false)
			{
				throw new InvalidDataException();
			}

			return Header;
		}
	}
}
