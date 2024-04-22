/*=============================================================================
	AVIWriter.h: Helper class for creating AVI files.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _AVIWRITER_HEADER_
#define _AVIWRITER_HEADER_

/** An event for synchronizing direct show encoding with the game thread */
extern FEvent* GCaptureSyncEvent;

class FAVIWriter
{
protected:
	/** Protected constructor to avoid abuse. */
	FAVIWriter() 
		: BufferSize(0),
		MovieCaptureIndex(0),
		bCapturing(FALSE),
		bReadyForCapture(FALSE),
		bMatineeFinished(FALSE),
		CaptureViewport(NULL),
		FrameNumber(0)
	{

	}

	TArray<BYTE> Buffer;
	INT BufferSize;
	INT MovieCaptureIndex;
	UBOOL bCapturing;
	UBOOL bReadyForCapture;
	UBOOL bMatineeFinished;
	FViewport* CaptureViewport;
	INT FrameNumber;

public:
	static FAVIWriter* GetInstance();

	INT GetBufferSize()
	{
		return BufferSize;
	}

	INT GetFrameNumber()
	{
		return FrameNumber;
	}

	UBOOL IsCapturing()
	{
		return bCapturing;
	}

	UBOOL IsCapturedMatineeFinished()
	{
		return bMatineeFinished;
	}

	void SetCapturedMatineeFinished(UBOOL finished = TRUE)
	{
		bMatineeFinished = finished;
	}

	FViewport* GetViewport()
	{
		return CaptureViewport;
	}

	void GetBuffer(TArray<BYTE>& outBytes)
	{
		outBytes = Buffer;
	}

	virtual void StartCapture(FViewport* Viewport = NULL) = 0;
	virtual void StopCapture() = 0;
	virtual void Close() = 0;
	virtual void Update() = 0;

};
#endif	//#ifndef _AVIWRITER_HEADER_