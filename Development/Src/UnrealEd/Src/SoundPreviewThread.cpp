/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "SoundPreviewThread.h"
#include "EngineSoundClasses.h"

/**
 *
 * @param PreviewCount	number of sounds to preview
 * @param Node			sound node to perform compression/decompression
 * @param Info			preview info class
 */
FSoundPreviewThread::FSoundPreviewThread( INT PreviewCount, USoundNodeWave *Node, FPreviewInfo *Info ) : 
	Count( PreviewCount ),
	SoundNode( Node ),
	PreviewInfo( Info ),
	TaskFinished( FALSE ),
	CancelCalculations( FALSE )
{
}

UBOOL FSoundPreviewThread::Init( void )
{
	return TRUE;
}

DWORD FSoundPreviewThread::Run( void )
{
	// Get the original wave
	SoundNode->RemoveAudioResource();
	SoundNode->InitAudioResource( SoundNode->RawData );

	for( Index = 0; Index < Count && !CancelCalculations; Index++ )
	{
		SoundNodeWaveQualityPreview( SoundNode, PreviewInfo + Index );
	}

	SoundNode->RemoveAudioResource();
	TaskFinished = TRUE;
	return 0;
}

void FSoundPreviewThread::Stop( void )
{
	CancelCalculations = TRUE;
}

void FSoundPreviewThread::Exit( void )
{
}

