/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "Stdafx.h"
#include "SyncFileSocket.h"
#include "TOCSettings.h"
#include "TOCInfo.h"
#include "Buffer.h"

using namespace System::Text;
using namespace System::IO;

namespace ConsoleInterface
{
	bool SyncFileSocket::Connect(String ^Host, TOCSettings ^BuildSettings)
	{
		if(Host == nullptr)
		{
			throw gcnew ArgumentNullException(L"Host");
		}

		bool bRet = true;

		try
		{
			if(mSocket != nullptr)
			{
				if(mSocket->Connected)
				{
					mSocket->Close();
				}
				
				delete mSocket;
				mSocket = nullptr;
			}

			mSocket = gcnew Socket(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
			mSocket->Connect(Host, 21408);
		}
		catch(Exception ^ex)
		{
			BuildSettings->WriteLine(Color::Red, ex->ToString());
			bRet = false;
		}

		return bRet;
	}

	bool SyncFileSocket::SendBytes(array<Byte> ^Bytes)
	{
		if(mSocket == nullptr || !mSocket->Connected)
		{
			return false;
		}

		return mSocket->Send(Bytes) == Bytes->Length;
	}

	bool SyncFileSocket::SendBytesAsync(Buffer ^Bytes, int NumBytes)
	{
		if(mSocket == nullptr || !mSocket->Connected)
		{
			return false;
		}

		mSocket->BeginSend(Bytes->Array, Bytes->Offset, NumBytes, SocketFlags::None, gcnew AsyncCallback(this, &ConsoleInterface::SyncFileSocket::OnSend), Bytes);

		return true;
	}

	void SyncFileSocket::OnSend(IAsyncResult ^Result)
	{
		SocketError Error;
		Buffer ^Buf = (Buffer^)Result->AsyncState;

		mSocket->EndSend(Result, Error);

		if(Buf->Release() == 0)
		{
			mBufferPoolMgr->ReturnBuffer(Buf);
		}
	}

	bool SyncFileSocket::SendInt32(int Value)
	{
		return SendBytes(BitConverter::GetBytes(Value));
	}

	bool SyncFileSocket::SendUInt32(unsigned int Value)
	{
		return SendBytes(BitConverter::GetBytes(Value));
	}

	bool SyncFileSocket::SendString(String ^Value)
	{
		if(Value == nullptr)
		{
			throw gcnew ArgumentNullException(L"Value");
		}

		if(!SendInt32(Value->Length))
		{
			return false;
		}

		return SendBytes(Encoding::UTF8->GetBytes(Value));
	}

	void SyncFileSocket::SyncFiles(List<TOCInfo^> ^TOC, array<SyncFileSocket^> ^Connections)
	{
		if(Connections->Length == 0)
		{
			return;
		}

		InitializeCRCTable();

		for each(TOCInfo ^CurTOCEntry in TOC)
		{
			try
			{
				FileStream ^FilePtr = gcnew FileStream(CurTOCEntry->FileName, FileMode::Open, FileAccess::Read);
				int FileSize = (int)FilePtr->Length;
				unsigned int CRC = 0;

				for each(SyncFileSocket ^CurSocket in Connections)
				{
					CurSocket->SendString(L"FILE");
					CurSocket->SendString(CurTOCEntry->FileName);
					CurSocket->SendInt32(FileSize);
				}

				while(FileSize > 0)
				{
					Buffer ^FileBuf = mBufferPoolMgr->GetBuffer();
					int BytesRead = FilePtr->Read(FileBuf->Array, FileBuf->Offset, FileBuf->Count);

					if(BytesRead == 0)
					{
						break;
					}

					FileSize -= BytesRead;

					CalcCRC(FileBuf, BytesRead, CRC);

					for each(SyncFileSocket ^CurSocket in Connections)
					{
						FileBuf->AddRef();
						CurSocket->SendBytesAsync(FileBuf, BytesRead);
					}
				}

				for each(SyncFileSocket ^CurSocket in Connections)
				{
					CurSocket->SendUInt32(CRC);
				}
			}
			catch(Exception^)
			{

			}
		}
	}

	/// <summary>
	/// Update the CRC based on the passed in bytes
	/// </summary>
	/// <param name="Bytes">Data to calc CC on</param>
	/// <param name="NumBytes">Size of bytes</param>
	/// <param name="CRC">Existing CRC to update</param>
	void SyncFileSocket::CalcCRC(Buffer ^Bytes, int NumBytes, unsigned int %CRC)
	{
		CRC = ~CRC;

		for(int ByteIndex = 0; ByteIndex < NumBytes; ByteIndex++)
		{
			CRC = (CRC << 8) ^ mCRCTable[(CRC >> 24) ^ Bytes[ByteIndex]];
		}

		CRC = ~CRC;
	}

	/// <summary>
	/// Fill out the CRC table
	/// </summary>
	void SyncFileSocket::InitializeCRCTable()
	{
		const unsigned int CRC32_POLY = 0x04c11db7;

		mCRCTable = gcnew array<unsigned int>(256);

		// Init CRC table.
		for(unsigned int CRCIndex = 0; CRCIndex < 256; CRCIndex++)
		{
			for(unsigned int c = CRCIndex << 24, j = 8; j != 0; j--)
			{
				mCRCTable[CRCIndex] = c = (c & 0x80000000) != 0 ? (c << 1) ^ CRC32_POLY : (c << 1);
			}
		}
	}

	bool SyncFileSocket::ReceiveBytes(array<Byte> ^Bytes, int NumBytes)
	{
		if(mSocket == nullptr || !mSocket->Connected)
		{
			return false;
		}

		int NumBytesRecv = 0;
		while(NumBytesRecv < NumBytes)
		{
			NumBytesRecv += mSocket->Receive(Bytes, NumBytesRecv, NumBytes - NumBytesRecv, SocketFlags::None);

			if(!mSocket->Connected)
			{
				return false;
			}
		}

		return true;
	}

	bool SyncFileSocket::ReceiveInt32(int %OutValue)
	{
		array<Byte> ^Buf = gcnew array<Byte>(4);

		bool bResult = ReceiveBytes(Buf, Buf->Length);

		if(bResult)
		{
			OutValue = BitConverter::ToInt32(Buf, 0);
		}

		return bResult;
	}

	bool SyncFileSocket::ReceiveString(String ^%OutValue)
	{
		int Length = 0;
		bool bResult = false;
		OutValue = String::Empty;

		if(ReceiveInt32(Length))
		{
			array<Byte> ^Buf = gcnew array<Byte>(Length + 1);

			bResult = ReceiveBytes(Buf, Length);
			Buf[Length] = 0;

			if(bResult)
			{
				OutValue = Encoding::UTF8->GetString(Buf);
			}
		}

		return bResult;
	}
}
