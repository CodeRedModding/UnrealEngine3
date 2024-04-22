using System;
using System.IO;

namespace AIProfiler
{
	/** Class representing a profiler stream file header */
	class StreamHeader
	{
		/** Magic number expected for the .aiprof file */
		public const UInt32 ExpectedMagic = 0xE408F9A2;

		/** Version number expected for the .aiprof file */
		public const UInt32 ExpectedVersionNumber = 1;

		/** Magic number of the file header */
		public UInt32 Magic;

		/** Version number of the file header */
		public UInt32 VersionNumber;

		/** Offset into the file for where the name table begins */
		public UInt32 NameTableOffset;

		/** Number of name table entries */
		public UInt32 NameTableEntries;

		/** Offset into the file for where the controller info table begins */
		public UInt32 ControllerInfoTableOffset;

		/** Number of controller info entries */
		public UInt32 ControllerInfoEntries;

		/**
		 * Constructor
		 * 
		 * @param	BinaryStream	BinaryReader to read profiling data from)
		 */
		public StreamHeader(BinaryReader BinaryStream)
		{
			// Read in the magic and version number of the file
			Magic = BinaryStream.ReadUInt32();
			VersionNumber = BinaryStream.ReadUInt32();

			if (Magic == ExpectedMagic && VersionNumber == ExpectedVersionNumber)
			{
				NameTableOffset = BinaryStream.ReadUInt32();
				NameTableEntries = BinaryStream.ReadUInt32();
				ControllerInfoTableOffset = BinaryStream.ReadUInt32();
				ControllerInfoEntries = BinaryStream.ReadUInt32();
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
		public static StreamHeader ReadHeader(Stream ParserStream, out BinaryReader BinaryStream)
		{
			// Create a binary stream for data, we might toss this later for an endian swapping one
			BinaryStream = new BinaryReader(ParserStream, System.Text.Encoding.ASCII);

			// Serialize header
			StreamHeader Header = new StreamHeader(BinaryStream);

			// Determine whether read file has magic header. If no, try again byteswapped.
			if (Header.Magic != StreamHeader.ExpectedMagic)
			{
				// Seek back to beginning of stream before we retry.
				ParserStream.Seek(0, SeekOrigin.Begin);

				// Use big endian reader. It transparently endian swaps data on read.
				BinaryStream = new BinaryReaderBigEndian(ParserStream);

				// Serialize header a second time.
				Header = new StreamHeader(BinaryStream);
			}

			// At this point we should have a valid header. If no, throw an exception
			if (Header.Magic != StreamHeader.ExpectedMagic)
			{
				throw new InvalidDataException("File magic number mis-match!");
			}
			if (Header.VersionNumber != StreamHeader.ExpectedVersionNumber)
			{
				throw new InvalidDataException("File version mismatch!");
			}

			return Header;
		}
	}
}
