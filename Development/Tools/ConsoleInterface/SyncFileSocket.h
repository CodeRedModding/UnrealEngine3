/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

#include "BufferPoolMgr.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Net;
using namespace System::Net::Sockets;

namespace ConsoleInterface
{
	// forward declarations
	ref class TOCSettings;
	ref class TOCInfo;
	ref class BufferPoolMgr;
	ref class Buffer;

	public ref class SyncFileSocket
	{
	private:
		Socket ^mSocket;

		static array<unsigned int> ^mCRCTable;
		static BufferPoolMgr ^mBufferPoolMgr = gcnew BufferPoolMgr();

	private:
		void OnSend(IAsyncResult ^Result);

	protected:
		/// <summary>
		/// Update the CRC based on the passed in bytes
		/// </summary>
		/// <param name="Bytes">Data to calc CC on</param>
		/// <param name="NumBytes">Size of bytes</param>
		/// <param name="CRC">Existing CRC to update</param>
		static void CalcCRC(Buffer ^Bytes, int NumBytes, unsigned int %CRC);

		/// <summary>
		/// Fill out the CRC table
		/// </summary>
		static void InitializeCRCTable();

	public:
		bool Connect(String ^Host, TOCSettings ^BuildSettings);
		bool SendBytes(array<Byte> ^Bytes);
		bool SendBytesAsync(Buffer ^Bytes, int NumBytes);
		bool SendInt32(int Value);
		bool SendUInt32(unsigned int Value);
		bool SendString(String ^Value);
		bool ReceiveBytes(array<Byte> ^Bytes, int NumBytes);
		bool ReceiveInt32(int %OutValue);
		bool ReceiveString(String ^%OutValue);

		static void SyncFiles(List<TOCInfo^> ^TOC, array<SyncFileSocket^> ^Connections);
	};
}
