/*=============================================================================
	D3D9Query.cpp: D3D query RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"

FOcclusionQueryRHIRef FD3D9DynamicRHI::CreateOcclusionQuery()
{
	TRefCountPtr<IDirect3DQuery9> OcclusionQuery;
	VERIFYD3D9RESULT(Direct3DDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION,(IDirect3DQuery9**)OcclusionQuery.GetInitReference()));
	return new FD3D9OcclusionQuery(OcclusionQuery);
}

void FD3D9DynamicRHI::ResetOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(OcclusionQuery,OcclusionQuery);

	OcclusionQuery->bResultIsCached = FALSE;
}

UBOOL FD3D9DynamicRHI::GetOcclusionQueryResult(FOcclusionQueryRHIParamRef OcclusionQueryRHI,DWORD& OutNumPixels,UBOOL bWait)
{
	DYNAMIC_CAST_D3D9RESOURCE(OcclusionQuery,OcclusionQuery);

	UBOOL bSuccess = TRUE;
	if(!OcclusionQuery->bResultIsCached)
	{
		bSuccess = GetQueryData(OcclusionQuery->Resource,&OcclusionQuery->Result,sizeof(OcclusionQuery->Result),bWait);
		OcclusionQuery->bResultIsCached = bSuccess;
	}

	OutNumPixels = OcclusionQuery->Result;

	return bSuccess;
}

UBOOL FD3D9DynamicRHI::GetQueryData(IDirect3DQuery9* Query,void* Data,SIZE_T DataSize,UBOOL bWait)
{
	if( !Query )
	{
		return FALSE;
	}

	// Request the data from the query.
	HRESULT Result = Query->GetData(Data,DataSize,D3DGETDATA_FLUSH);

	// Isn't the query finished yet, and can we wait for it?
	if ( Result == S_FALSE && bWait )
	{
		SCOPE_CYCLE_COUNTER( STAT_OcclusionResultTime );
		DWORD IdleStart = appCycles();
		DOUBLE StartTime = appSeconds();
		do 
		{
			Result = Query->GetData(Data,DataSize,D3DGETDATA_FLUSH);

			if((appSeconds() - StartTime) > 0.5)
			{
				debugf(TEXT("Timed out while waiting for GPU to catch up. (500 ms)"));
				return FALSE;
			}
		} while ( Result == S_FALSE );
		GRenderThreadIdle += appCycles() - IdleStart;
	}

	if ( Result == S_OK )
	{
		return TRUE;
	}
	else if ( Result == S_FALSE && !bWait )
	{
		// Return failure if the query isn't complete, and waiting wasn't requested.
		return FALSE;
	}
	else if ( Result == D3DERR_DEVICELOST )
	{
		bDeviceLost = 1;
		return FALSE;
	}
	else if ( Result == E_FAIL )
	{
		return FALSE;
	}
	else
	{
		VERIFYD3D9RESULT(Result);
		return FALSE;
	}
}

void FD3D9EventQuery::IssueEvent()
{
	if( Query )
	{
		Query->Issue(D3DISSUE_END);
	}
}

void FD3D9EventQuery::WaitForCompletion()
{
	if( Query )
	{
		UBOOL bRenderingIsFinished = FALSE;
		while ( D3DRHI->GetQueryData(Query,&bRenderingIsFinished,sizeof(bRenderingIsFinished),TRUE) && !bRenderingIsFinished )
		{
		}
	}
}

void FD3D9EventQuery::InitDynamicRHI()
{
	VERIFYD3D9RESULT(D3DRHI->GetDevice()->CreateQuery(D3DQUERYTYPE_EVENT,Query.GetInitReference()));
	// Initialize the query by issuing an initial event.
	IssueEvent();
}

void FD3D9EventQuery::ReleaseDynamicRHI()
{
	Query = NULL;
}



/*=============================================================================
 * class FD3D9BufferedGPUTiming
 *=============================================================================*/

/** Whether the static variables have been initialized. */
UBOOL FD3D9BufferedGPUTiming::GAreGlobalsInitialized = FALSE;
/** Whether GPU timing measurements are supported by the driver. */
UBOOL FD3D9BufferedGPUTiming::GIsSupported = FALSE;
/** Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported. */
QWORD FD3D9BufferedGPUTiming::GTimingFrequency = 0;

/**
 * Constructor.
 *
 * @param InD3DRHI			RHI interface
 * @param InBufferSize		Number of buffered measurements
 */
FD3D9BufferedGPUTiming::FD3D9BufferedGPUTiming( FD3D9DynamicRHI* InD3DRHI, INT InBufferSize )
:	D3DRHI( InD3DRHI )
,	BufferSize( InBufferSize )
,	CurrentTimestamp( -1 )
,	NumIssuedTimestamps( 0 )
,	StartTimestamps( NULL )
,	EndTimestamps( NULL )
,	bIsTiming( FALSE )
{
}

/**
 * Initializes the static variables, if necessary.
 */
void FD3D9BufferedGPUTiming::StaticInitialize()
{
	// Are the static variables initialized?
	if ( !GAreGlobalsInitialized )
	{
		// Get the GPU timestamp frequency.
		GTimingFrequency = 0;
		TRefCountPtr<IDirect3DQuery9> FreqQuery;
		HRESULT D3DResult;
		D3DResult = D3DRHI->GetDevice()->CreateQuery( D3DQUERYTYPE_TIMESTAMPFREQ, FreqQuery.GetInitReference() );
		if ( D3DResult == D3D_OK )
		{
			D3DResult = FreqQuery->Issue(D3DISSUE_END);
		}
		if ( D3DResult == D3D_OK )
		{
			D3DResult = FreqQuery->GetData( &GTimingFrequency, sizeof(GTimingFrequency), D3DGETDATA_FLUSH );
			DOUBLE StartTime = appSeconds();
			while ( D3DResult == S_FALSE && (appSeconds() - StartTime) < 0.1 )
			{
				appSleep( 0.005f );
				D3DResult = FreqQuery->GetData( &GTimingFrequency, sizeof(GTimingFrequency), D3DGETDATA_FLUSH );
			}
		}
		FreqQuery = NULL;

		if ( GTimingFrequency != 0 )
		{
			GIsSupported = TRUE;
		}
		else
		{
			GIsSupported = FALSE;
		}

		GAreGlobalsInitialized = TRUE;
	}
}

/**
 * Initializes all D3D resources and if necessary, the static variables.
 */
void FD3D9BufferedGPUTiming::InitDynamicRHI()
{
	if ( !GAreGlobalsInitialized )
	{
		StaticInitialize();
	}

	CurrentTimestamp = 0;
	NumIssuedTimestamps = 0;
	bIsTiming = FALSE;

	// Now initialize the queries for this timing object.
	if ( GIsSupported )
	{
		StartTimestamps = new TRefCountPtr<IDirect3DQuery9>[ BufferSize ];
		EndTimestamps = new TRefCountPtr<IDirect3DQuery9>[ BufferSize ];
		for ( INT TimestampIndex = 0; TimestampIndex < BufferSize; ++TimestampIndex )
		{
			HRESULT D3DResult;
			D3DResult = D3DRHI->GetDevice()->CreateQuery(D3DQUERYTYPE_TIMESTAMP,StartTimestamps[TimestampIndex].GetInitReference());
			GIsSupported = GIsSupported && (D3DResult == D3D_OK);
			D3DResult = D3DRHI->GetDevice()->CreateQuery(D3DQUERYTYPE_TIMESTAMP,EndTimestamps[TimestampIndex].GetInitReference());
			GIsSupported = GIsSupported && (D3DResult == D3D_OK);
		}
	}
}

/**
 * Releases all D3D resources.
 */
void FD3D9BufferedGPUTiming::ReleaseDynamicRHI()
{
	if ( StartTimestamps && EndTimestamps )
	{
		for ( INT TimestampIndex = 0; TimestampIndex < BufferSize; ++TimestampIndex )
		{
			StartTimestamps[TimestampIndex] = NULL;
			EndTimestamps[TimestampIndex] = NULL;
		}
		delete [] StartTimestamps;
		delete [] EndTimestamps;
		StartTimestamps = NULL;
		EndTimestamps = NULL;
	}
}

/**
 * Start a GPU timing measurement.
 */
void FD3D9BufferedGPUTiming::StartTiming()
{
	// Issue a timestamp query for the 'start' time.
	HRESULT D3DResult;
	if ( GIsSupported && !bIsTiming )
	{
		INT NewTimestampIndex = (CurrentTimestamp + 1) % BufferSize;
		D3DResult = StartTimestamps[NewTimestampIndex]->Issue(D3DISSUE_END);
		if ( D3DResult == S_OK )
		{
			CurrentTimestamp = NewTimestampIndex;
			bIsTiming = TRUE;
		}
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FD3D9BufferedGPUTiming::EndTiming()
{
	// Issue a timestamp query for the 'end' time.
	HRESULT D3DResult;
	if ( GIsSupported && bIsTiming )
	{
		checkSlow( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );
		D3DResult = EndTimestamps[CurrentTimestamp]->Issue(D3DISSUE_END);
		if ( D3DResult == S_OK )
		{
			NumIssuedTimestamps = Min<INT>(NumIssuedTimestamps + 1, BufferSize);
			bIsTiming = FALSE;
		}
	}
}

/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for appCycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
QWORD FD3D9BufferedGPUTiming::GetTiming()
{
	if ( GIsSupported )
	{
		checkSlow( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );
		QWORD StartTime, EndTime;
		HRESULT D3DResultStart, D3DResultEnd;

		// Quickly check the most recent measurements to see if any of them has been resolved.
		INT TimestampIndex = CurrentTimestamp;
		for ( INT IssueIndex = 1; IssueIndex < NumIssuedTimestamps; ++IssueIndex )
		{
			D3DResultEnd = EndTimestamps[TimestampIndex]->GetData(&EndTime, sizeof(EndTime), 0 );
			if ( D3DResultEnd == S_OK )
			{
				D3DResultStart = StartTimestamps[TimestampIndex]->GetData(&StartTime, sizeof(StartTime), 0 );
				if ( D3DResultStart == S_OK )
				{
					return EndTime - StartTime;
				}
			}

			TimestampIndex = (TimestampIndex + BufferSize - 1) % BufferSize;
		}

		if ( NumIssuedTimestamps > 0 )
		{
			// None of the (NumIssuedTimestamps - 1) measurements were ready yet,
			// so check the oldest measurement more thoroughly.
			// This really only happens if occlusion and frame sync event queries are disabled, otherwise those will block until the GPU catches up to 1 frame behind
			const UBOOL bBlocking = ( NumIssuedTimestamps == BufferSize );
			UBOOL queryFinished = D3DRHI->GetQueryData( EndTimestamps[TimestampIndex], &EndTime, sizeof(EndTime), bBlocking );

			if (queryFinished)
			{
				queryFinished = D3DRHI->GetQueryData( StartTimestamps[TimestampIndex], &StartTime, sizeof(EndTime), bBlocking );

				if (queryFinished)
				{
					return EndTime - StartTime;
				}
			}
		}
	}
	return 0;
}

FD3D9DisjointTimeStampQuery::FD3D9DisjointTimeStampQuery(class FD3D9DynamicRHI* InD3DRHI) :
D3DRHI(InD3DRHI)
{

}

void FD3D9DisjointTimeStampQuery::StartTracking()
{
	DisjointQuery->Issue(D3DISSUE_BEGIN);
}

void FD3D9DisjointTimeStampQuery::EndTracking()
{
	DisjointQuery->Issue(D3DISSUE_END);
}

UBOOL FD3D9DisjointTimeStampQuery::WasDisjoint()
{
	BOOL bDisjoint;

	HRESULT D3DResult = DisjointQuery->GetData((void*)&bDisjoint, sizeof(BOOL), D3DGETDATA_FLUSH);

	const DOUBLE StartTime = appSeconds();

	while (D3DResult == S_FALSE && (appSeconds() - StartTime) < 0.5)
	{
		appSleep(0.005f);
		D3DResult = DisjointQuery->GetData((void*)&bDisjoint, sizeof(BOOL), D3DGETDATA_FLUSH);
	}

	return ((D3DResult == S_FALSE) || (bDisjoint == TRUE));
}

void FD3D9DisjointTimeStampQuery::InitDynamicRHI()
{
	VERIFYD3D9RESULT(D3DRHI->GetDevice()->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, DisjointQuery.GetInitReference()));
}

void FD3D9DisjointTimeStampQuery::ReleaseDynamicRHI()
{

}