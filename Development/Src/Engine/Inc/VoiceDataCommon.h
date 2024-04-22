/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#define _DEBUG_VOICE_PACKET_ENCODING 0

#ifndef MAX_VOICE_DATA_SIZE
	#define MAX_VOICE_DATA_SIZE 100
#endif

#ifndef MAX_SPLITSCREEN_TALKERS
	#define MAX_SPLITSCREEN_TALKERS 4
#endif

/** Defines the data involved in a voice packet */
struct FVoicePacket
{
	/** The unique net id of the talker sending the data */
	FUniqueNetId Sender;
	/** The data that is to be sent/processed */
	BYTE Buffer[MAX_VOICE_DATA_SIZE];
	/** The current amount of space used in the buffer for this packet */
	WORD Length;
	/** Number of references outstanding to this object */
	BYTE RefCount;
	/** Determines whether this packet is ref counted or not (not UBOOL for packing reasons) */
	BYTE bShouldUseRefCount;

private:
	/** Hidden so that only the DecRef() and FVoiceData can delete this object */
	~FVoicePacket()
	{
	}

	friend struct FVoiceData;

	/**
	 * Reads the sender information in a NBO friendly way
	 *
	 * @param ReadBuffer the buffer to read the data from
	 *
	 * @return the number of bytes read
	 */
	inline WORD ReadSender(BYTE* ReadBuffer)
	{
		QWORD Q = 0;
		Q = ((QWORD)ReadBuffer[0] << 56) |
			((QWORD)ReadBuffer[1] << 48) |
			((QWORD)ReadBuffer[2] << 40) |
			((QWORD)ReadBuffer[3] << 32) |
			((QWORD)ReadBuffer[4] << 24) |
			((QWORD)ReadBuffer[5] << 16) |
			((QWORD)ReadBuffer[6] << 8) |
			(QWORD)ReadBuffer[7];
		(QWORD&)Sender = Q;
		return sizeof(FUniqueNetId);
	}

	/**
	 * Reads the voice data length in a NBO friendly way
	 *
	 * @param ReadBuffer the buffer to read the data from
	 *
	 * @return the number of bytes read
	 */
	inline WORD ReadLength(BYTE* ReadBuffer)
	{
		Length = ((WORD)ReadBuffer[0] << 8) |
			(WORD)ReadBuffer[1];
		return sizeof(WORD);
	}

	/**
	 * Writes the sender information in a NBO friendly way
	 *
	 * @param WriteBuffer the buffer to write the data to
	 *
	 * @return the number of bytes written
	 */
	inline WORD WriteSender(BYTE* WriteBuffer)
	{
		QWORD Q = (QWORD&)Sender;
		WriteBuffer[0] = (Q >> 56) & 0xFF;
		WriteBuffer[1] = (Q >> 48) & 0xFF;
		WriteBuffer[2] = (Q >> 40) & 0xFF;
		WriteBuffer[3] = (Q >> 32) & 0xFF;
		WriteBuffer[4] = (Q >> 24) & 0xFF;
		WriteBuffer[5] = (Q >> 16) & 0xFF;
		WriteBuffer[6] = (Q >> 8) & 0xFF;
		WriteBuffer[7] = Q & 0xFF;
		return sizeof(FUniqueNetId);
	}

	/**
	 * Writes the voice data length in a NBO friendly way
	 *
	 * @param WriteBuffer the buffer to write the data to
	 *
	 * @return the number of bytes written
	 */
	inline WORD WriteLength(BYTE* WriteBuffer)
	{
		WriteBuffer[0] = Length >> 8;
		WriteBuffer[1] = Length & 0xFF;
		return sizeof(WORD);
	}

public:
	/** Zeros members and validates the assumptions */
	FVoicePacket(void) :
		Sender((QWORD)0),
		Length(0),
		RefCount(0),
		bShouldUseRefCount(0)
	{
	}

	/**
	 * Inits the packet
	 *
	 * @param InRefCount the starting ref count to use
	 */
	FVoicePacket(BYTE InRefCount) :
		Sender((QWORD)0),
		Length(0),
		RefCount(InRefCount),
		bShouldUseRefCount(TRUE)
	{
		check(RefCount < 255 && RefCount > 0);
	}

	/**
	 * Copies another packet and inits the ref count
	 *
	 * @param InRefCount the starting ref count to use
	 */
	FVoicePacket(const FVoicePacket& Other,BYTE InRefCount) :
		Sender(Other.Sender),
		Length(Other.Length),
		RefCount(InRefCount),
		bShouldUseRefCount(TRUE)
	{
		check(RefCount < 255 && RefCount > 0);
		// Copy the contents of the voice packet
		appMemcpy(Buffer,Other.Buffer,Other.Length);
	}

	/**
	 * Increments the ref count
	 */
	FORCEINLINE void AddRef(void)
	{
		check(RefCount < 255);
		if (bShouldUseRefCount)
		{
			RefCount++;
		}
	}

	/** 
	 * Decrements the ref count and deletes the object if needed
	 */
	FORCEINLINE void DecRef(void)
	{
		check(RefCount > 0 && bShouldUseRefCount);
		// Delete self if unreferenced
		if (bShouldUseRefCount && --RefCount == 0)
		{
			delete this;
		}
	}

	/**
	 * Reads the data for this object from the buffer and returns
	 * the number of bytes read from the byte stream
	 *
	 * @param ReadBuffer the source data to parse
	 *
	 * @return the amount of data read from the buffer
	 */
	inline WORD ReadFromBuffer(BYTE* ReadBuffer)
	{
		checkSlow(ReadBuffer);
		WORD SizeRead = 0;
		// Copy the unique net id and packet size info
		SizeRead = ReadSender(ReadBuffer);
		ReadBuffer += SizeRead;
		SizeRead = ReadLength(ReadBuffer);
		ReadBuffer += SizeRead;
		// If the size is valid, copy the voice buffer
		check(Length <= MAX_VOICE_DATA_SIZE);
		appMemcpy(Buffer,ReadBuffer,Length);
#if _DEBUG_VOICE_PACKET_ENCODING
		// Read and verify the CRC
		ReadBuffer += Length;
		DWORD CRC = *(DWORD*)ReadBuffer;
		check(CRC == appMemCrc(Buffer,Length));
		return sizeof(FUniqueNetId) + sizeof(WORD) + sizeof(DWORD) + Length;
#else
		return sizeof(FUniqueNetId) + sizeof(WORD) + Length;
#endif
	}

	/**
	 * Writes the data for this object to the buffer and returns
	 * the number of bytes written. Assumes there is enough space
	 */
	inline WORD WriteToBuffer(BYTE* WriteAt)
	{
		WORD SizeWritten = 0;
		// Copy the unique net id and packet size info
		SizeWritten = WriteSender(WriteAt);
		WriteAt += SizeWritten;
		SizeWritten = WriteLength(WriteAt);
		WriteAt += SizeWritten;
		// Block copy the raw voice data
		appMemcpy(WriteAt,Buffer,Length);
#if _DEBUG_VOICE_PACKET_ENCODING
		// Send a CRC of the packet
		WriteAt += Length;
		*(DWORD*)WriteAt = appMemCrc(Buffer,Length);
		return sizeof(FUniqueNetId) + sizeof(WORD) + sizeof(DWORD) + Length;
#else
		return sizeof(FUniqueNetId) + sizeof(WORD) + Length;
#endif
	}

	/** Returns the amount of space this packet will consume in a buffer */
	FORCEINLINE WORD GetTotalPacketSize(void)
	{
#if _DEBUG_VOICE_PACKET_ENCODING
		return sizeof(FUniqueNetId) + sizeof(WORD) + sizeof(DWORD) + Length;
#else
		return sizeof(FUniqueNetId) + sizeof(WORD) + Length;
#endif
	}


	/**
	 * Serializes the voice packet data into/from an archive
	 *
	 * @param Ar the archive to serialize with
	 * @param VoicePacket the voice data to serialize
	 */
	friend FArchive& operator<<(FArchive& Ar,FVoicePacket& VoicePacket);
};

/** Make the tarray of voice packets a bit more readable */
typedef TArray<FVoicePacket*> FVoicePacketList;

/** Holds the global voice packet data state */
struct FVoiceData
{
	/** Data used by the local talkers before sent */
	FVoicePacket LocalPackets[MAX_SPLITSCREEN_TALKERS];
	/** Holds the set of received packets that need to be processed by XHV */
	FVoicePacketList RemotePackets;
	/** Holds the next packet index for transmitting local packets */
	DWORD NextVoicePacketIndex;

	/** Just zeros the packet data */
	FVoiceData(void)
	{
		appMemzero(LocalPackets,sizeof(FVoicePacket) * MAX_SPLITSCREEN_TALKERS);
	}
};

/** Global packet data to be shared between the net layer and the subsystem layer */
extern FVoiceData GVoiceData;
