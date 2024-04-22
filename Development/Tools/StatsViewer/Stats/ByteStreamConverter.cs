/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Text;

namespace Stats
{
	/// <summary>
	/// Converts a chunk of a byte stream into a data type. Requires the /unsafe
	/// compiler option so that pointers can be used
	/// </summary>
	public class ByteStreamConverter
	{
		/// <summary>
		/// Don't create an instance as this is a static only class
		/// </summary>
		public ByteStreamConverter()
		{
		}

        public static bool BigEndian = true;

        public static unsafe int GetInt(Byte[] Data, ref int Offset)
        {
            int TempValue = 0;
            if (BigEndian)
            {
                TempValue =
                    Data[Offset++] << 24 |
                    Data[Offset++] << 16 |
                    Data[Offset++] <<  8 |
                    Data[Offset++] <<  0;
            }
            else 
            {
                TempValue =
                    Data[Offset++] << 0  |
                    Data[Offset++] << 8  |
                    Data[Offset++] << 16 |
                    Data[Offset++] << 24;
            }
            return TempValue;
        }
		/// <summary>
		/// Converts 4 bytes of a byte stream into a double
		/// </summary>
		/// <param name="Data">The byte stream to convert</param>
		/// <param name="Offset">The offset into the stream to start at</param>
		/// <returns>The converted value</returns>
		public static unsafe double ToDouble(Byte[] Data,ref int Offset)
		{
			// Promote to double
			double ConvertedValue = ToFloat( Data, ref Offset );
			return ConvertedValue;
		}

		/// <summary>
		/// Converts 4 bytes of a byte stream into a float
		/// </summary>
		/// <param name="Data">The byte stream to convert</param>
		/// <param name="Offset">The offset into the stream to start at</param>
		/// <returns>The converted value</returns>
		public static unsafe float ToFloat( Byte[] Data, ref int Offset )
		{
			// Move the data from NBO to our byte ordering
            int TempValue = GetInt(Data, ref Offset);

			// Get a pointer so we can do bitwise casting
			int* pTempValue = &TempValue;
			float TempFloat = *(float*)pTempValue;

			return TempFloat;
		}

		/// <summary>
		/// Converts 4 bytes of the byte stream into an int
		/// </summary>
		/// <param name="Data">The byte stream to convert</param>
		/// <param name="Offset">The offset into the stream to start at</param>
		/// <returns>The converted value</returns>
		public static int ToInt(Byte[] Data,ref int Offset)
		{
            int Value = GetInt(Data, ref Offset);
			return Value;
		}

		/// <summary>
		/// Converts 2 bytes of the byte stream into a word
		/// </summary>
		/// <param name="Data">The byte stream to convert</param>
		/// <param name="Offset">The offset into the stream to start at</param>
		/// <returns>The converted value</returns>
		public static int ToWord( Byte[] Data, ref int Offset )
		{
            int Value;
            if (BigEndian)
            {
                Value = 
                Data[Offset++] << 8 |
                Data[Offset++] << 0;
            }
            else
            {
                Value =
                Data[Offset++] << 0 |
                Data[Offset++] << 8;
            }
			return Value;
		}

		/// <summary>
		/// Retrieves a byte
		/// </summary>
		/// <param name="Data">The byte stream to convert</param>
		/// <param name="Offset">The offset into the stream to start at</param>
		/// <returns>The converted value</returns>
		public static int ToByte( Byte[] Data, ref int Offset )
		{
			return Data[ Offset++ ];
		}
		
		/// <summary>
		/// Creates a string from a chunk of data. Reads the length of the string
		/// then returns a string of that size from the data in the buffer.
		/// </summary>
		/// <param name="Data">The stream to read the string from</param>
		/// <param name="Offset">The offset into the stream to build the string from</param>
		/// <returns>The string that built from the byte data</returns>
		public static string ToString(Byte[] Data,ref int Offset)
		{
			int StringLen = ToInt(Data,ref Offset);

			// Build the string
			string BuiltString = Encoding.ASCII.GetString(Data,Offset,StringLen);

			// Update the offset
			Offset += StringLen;
			return BuiltString;
		}


		/// <summary>
		/// Creates a string from a chunk of data. Reads the length of the string
		/// then returns a string of that size from the data in the buffer.
		/// </summary>
		/// <param name="Data">The stream to read the string from</param>
		/// <param name="Offset">The offset into the stream to build the string from</param>
		/// <returns>The string that built from the byte data</returns>
		public static string ToFString( Byte[] Data, ref int Offset )
		{
			int StringLen = ToInt( Data, ref Offset );

			// Build the string
			string BuiltString = Encoding.ASCII.GetString( Data, Offset, Math.Max( 0, StringLen - 1 ) );

			// Update the offset
			Offset += StringLen;
			return BuiltString;
		}
	}
}
