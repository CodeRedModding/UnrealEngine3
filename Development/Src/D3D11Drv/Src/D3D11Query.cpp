/*=============================================================================
	D3D11Query.cpp: D3D query RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"

FOcclusionQueryRHIRef FD3D11DynamicRHI::CreateOcclusionQuery()
{
	TRefCountPtr<ID3D11Query> OcclusionQuery;
	D3D11_QUERY_DESC Desc;
	Desc.Query = D3D11_QUERY_OCCLUSION;
	Desc.MiscFlags = 0;
	VERIFYD3D11RESULT(Direct3DDevice->CreateQuery(&Desc,OcclusionQuery.GetInitReference()));
	return new FD3D11OcclusionQuery(OcclusionQuery);
}

void FD3D11DynamicRHI::ResetOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(OcclusionQuery,OcclusionQuery);

	OcclusionQuery->bResultIsCached = FALSE;
}

UBOOL FD3D11DynamicRHI::GetOcclusionQueryResult(FOcclusionQueryRHIParamRef OcclusionQueryRHI,DWORD& OutNumPixels,UBOOL bWait)
{
	DYNAMIC_CAST_D3D11RESOURCE(OcclusionQuery,OcclusionQuery);

	UBOOL bSuccess = TRUE;
	if(!OcclusionQuery->bResultIsCached)
	{
		bSuccess = GetQueryData(OcclusionQuery->Resource,&OcclusionQuery->Result,sizeof(OcclusionQuery->Result),bWait);
		OcclusionQuery->bResultIsCached = bSuccess;
	}

	// D3D11_QUERY_OCCLUSION returns the number of samples that pass the depth / stencil test, convert to number of pixels
	OutNumPixels = (DWORD)OcclusionQuery->Result / Clamp(GSystemSettings.MaxMultiSamples, 1, 16);

	return bSuccess;
}

UBOOL FD3D11DynamicRHI::GetQueryData(ID3D11Query* Query,void* Data,SIZE_T DataSize,UBOOL bWait)
{
	// Request the data from the query.
	HRESULT Result = Direct3DDeviceIMContext->GetData(Query,Data,DataSize,0);

	// Isn't the query finished yet, and can we wait for it?
	if ( Result == S_FALSE && bWait )
	{
		SCOPE_CYCLE_COUNTER( STAT_OcclusionResultTime );
		DWORD IdleStart = appCycles();
		DOUBLE StartTime = appSeconds();
		do 
		{
			Result = Direct3DDeviceIMContext->GetData(Query,Data,DataSize,0);

			if((appSeconds() - StartTime) > 0.5)
			{
				debugf(TEXT("Timed out while waiting for GPU to catch up. (500 ms)"));
				return FALSE;
			}
		} while ( Result == S_FALSE );
		GRenderThreadIdle += appCycles() - IdleStart;
	}

	if( Result == S_OK )
	{
		return TRUE;
	}
	else if(Result == S_FALSE && !bWait)
	{
		// Return failure if the query isn't complete, and waiting wasn't requested.
		return FALSE;
	}
	else if( Result == DXGI_ERROR_DEVICE_REMOVED || Result == DXGI_ERROR_DEVICE_RESET || Result == DXGI_ERROR_DRIVER_INTERNAL_ERROR )
	{
		bDeviceRemoved = TRUE;
		return FALSE;
	}
	else
	{
		VERIFYD3D11RESULT(Result);
		return FALSE;
	}
}

void FD3D11EventQuery::IssueEvent()
{
	D3DRHI->GetDeviceContext()->End(Query);
}

void FD3D11EventQuery::WaitForCompletion()
{
	UBOOL bRenderingIsFinished = FALSE;
	while(
		D3DRHI->GetQueryData(Query,&bRenderingIsFinished,sizeof(bRenderingIsFinished),TRUE) &&
		!bRenderingIsFinished
		)
	{};
}

void FD3D11EventQuery::InitDynamicRHI()
{
	D3D11_QUERY_DESC QueryDesc;
	QueryDesc.Query = D3D11_QUERY_EVENT;
	QueryDesc.MiscFlags = 0;
	VERIFYD3D11RESULT(D3DRHI->GetDevice()->CreateQuery(&QueryDesc,Query.GetInitReference()));

	// Initialize the query by issuing an initial event.
	IssueEvent();
}
void FD3D11EventQuery::ReleaseDynamicRHI()
{
	Query = NULL;
}

/*=============================================================================
 * class FD3D11BufferedGPUTiming
 *=============================================================================*/

/** Whether the static variables have been initialized. */
UBOOL FD3D11BufferedGPUTiming::GAreGlobalsInitialized = FALSE;
/** Whether GPU timing measurements are supported by the driver. */
UBOOL FD3D11BufferedGPUTiming::GIsSupported = FALSE;
/** Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported. */
QWORD FD3D11BufferedGPUTiming::GTimingFrequency = 0;

/**
 * Constructor.
 *
 * @param InD3DRHI			RHI interface
 * @param InBufferSize		Number of buffered measurements
 */
FD3D11BufferedGPUTiming::FD3D11BufferedGPUTiming( FD3D11DynamicRHI* InD3DRHI, INT InBufferSize )
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
void FD3D11BufferedGPUTiming::StaticInitialize()
{
	// Are the static variables initialized?
	if ( !GAreGlobalsInitialized )
	{
		// Get the GPU timestamp frequency.
		GTimingFrequency = 0;
		TRefCountPtr<ID3D11Query> FreqQuery;
		ID3D11DeviceContext *D3D11DeviceContext = D3DRHI->GetDeviceContext();
		HRESULT D3DResult;

		D3D11_QUERY_DESC QueryDesc;
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		QueryDesc.MiscFlags = 0;

		D3DResult = D3DRHI->GetDevice()->CreateQuery(&QueryDesc, FreqQuery.GetInitReference() );
		if ( D3DResult == S_OK )
		{
			D3D11DeviceContext->Begin(FreqQuery);
			D3D11DeviceContext->End(FreqQuery);

			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT FreqQueryData;

			D3DResult = D3D11DeviceContext->GetData(FreqQuery,&FreqQueryData,sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT),0);
			DOUBLE StartTime = appSeconds();
			while ( D3DResult == S_FALSE && (appSeconds() - StartTime) < 0.1 )
			{
				appSleep( 0.005f );
				D3DResult = D3D11DeviceContext->GetData(FreqQuery,&FreqQueryData,sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT),0);
			}


			if(D3DResult == S_OK)
			{
				GTimingFrequency = FreqQueryData.Frequency;
				checkSlow(!FreqQueryData.Disjoint);
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
void FD3D11BufferedGPUTiming::InitDynamicRHI()
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
		StartTimestamps = new TRefCountPtr<ID3D11Query>[ BufferSize ];
		EndTimestamps = new TRefCountPtr<ID3D11Query>[ BufferSize ];
		for ( INT TimestampIndex = 0; TimestampIndex < BufferSize; ++TimestampIndex )
		{
			HRESULT D3DResult;

			D3D11_QUERY_DESC QueryDesc;
			QueryDesc.Query = D3D11_QUERY_TIMESTAMP;
			QueryDesc.MiscFlags = 0;

			D3DResult = D3DRHI->GetDevice()->CreateQuery(&QueryDesc,StartTimestamps[TimestampIndex].GetInitReference());
			GIsSupported = GIsSupported && (D3DResult == S_OK);
			D3DResult = D3DRHI->GetDevice()->CreateQuery(&QueryDesc,EndTimestamps[TimestampIndex].GetInitReference());
			GIsSupported = GIsSupported && (D3DResult == S_OK);
		}
	}
}

/**
 * Releases all D3D resources.
 */
void FD3D11BufferedGPUTiming::ReleaseDynamicRHI()
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
void FD3D11BufferedGPUTiming::StartTiming()
{
	// Issue a timestamp query for the 'start' time.
	if ( GIsSupported && !bIsTiming )
	{
		INT NewTimestampIndex = (CurrentTimestamp + 1) % BufferSize;
		D3DRHI->GetDeviceContext()->End(StartTimestamps[NewTimestampIndex]);
		CurrentTimestamp = NewTimestampIndex;
		bIsTiming = TRUE;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FD3D11BufferedGPUTiming::EndTiming()
{
	// Issue a timestamp query for the 'end' time.
	if ( GIsSupported && bIsTiming )
	{
		checkSlow( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );
		D3DRHI->GetDeviceContext()->End(EndTimestamps[CurrentTimestamp]);
		NumIssuedTimestamps = Min<INT>(NumIssuedTimestamps + 1, BufferSize);
		bIsTiming = FALSE;
	}
}

/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for appCycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
QWORD FD3D11BufferedGPUTiming::GetTiming(UBOOL bGetCurrentResultsAndBlock)
{
	if ( GIsSupported )
	{
		checkSlow( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );
		QWORD StartTime, EndTime;
		HRESULT D3DResult;

		INT TimestampIndex = CurrentTimestamp;
		if (!bGetCurrentResultsAndBlock)
		{
			// Quickly check the most recent measurements to see if any of them has been resolved.  Do not flush these queries.
			for ( INT IssueIndex = 1; IssueIndex < NumIssuedTimestamps; ++IssueIndex )
			{
				D3DResult = D3DRHI->GetDeviceContext()->GetData(EndTimestamps[TimestampIndex],&EndTime,sizeof(EndTime),D3D11_ASYNC_GETDATA_DONOTFLUSH);
				if ( D3DResult == S_OK )
				{
					D3DResult = D3DRHI->GetDeviceContext()->GetData(StartTimestamps[TimestampIndex],&StartTime,sizeof(StartTime),D3D11_ASYNC_GETDATA_DONOTFLUSH);
					if ( D3DResult == S_OK && EndTime > StartTime)
					{
						return EndTime - StartTime;
					}
				}

				TimestampIndex = (TimestampIndex + BufferSize - 1) % BufferSize;
			}
		}
		
		if ( NumIssuedTimestamps > 0 || bGetCurrentResultsAndBlock )
		{
			// None of the (NumIssuedTimestamps - 1) measurements were ready yet,
			// so check the oldest measurement more thoroughly.
			// This really only happens if occlusion and frame sync event queries are disabled, otherwise those will block until the GPU catches up to 1 frame behind
			const UBOOL bBlocking = ( NumIssuedTimestamps == BufferSize ) || bGetCurrentResultsAndBlock;
			const UINT AsyncFlags = bBlocking ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH;
			DWORD IdleStart = appCycles();
			DOUBLE StartTimeoutTime = appSeconds();

			SCOPE_CYCLE_COUNTER( STAT_OcclusionResultTime );
			// If we are blocking, retry until the GPU processes the time stamp command
			do 
			{
				D3DResult = D3DRHI->GetDeviceContext()->GetData( EndTimestamps[TimestampIndex], &EndTime, sizeof(EndTime), AsyncFlags );

				if ((appSeconds() - StartTimeoutTime) > 0.5)
				{
					debugf(TEXT("Timed out while waiting for GPU to catch up. (500 ms)"));
					return 0;
				}
			} while ( D3DResult == S_FALSE && bBlocking );
			GRenderThreadIdle += appCycles() - IdleStart;

			if ( D3DResult == S_OK )
			{
				IdleStart = appCycles();
				StartTimeoutTime = appSeconds();
				do 
				{
					D3DResult = D3DRHI->GetDeviceContext()->GetData( StartTimestamps[TimestampIndex], &StartTime, sizeof(StartTime), AsyncFlags );

					if ((appSeconds() - StartTimeoutTime) > 0.5)
					{
						debugf(TEXT("Timed out while waiting for GPU to catch up. (500 ms)"));
						return 0;
					}
				} while ( D3DResult == S_FALSE && bBlocking );
				GRenderThreadIdle += appCycles() - IdleStart;

				if ( D3DResult == S_OK && EndTime > StartTime )
				{
					return EndTime - StartTime;
				}
			}
		}
	}
	return 0;
}

FD3D11DisjointTimeStampQuery::FD3D11DisjointTimeStampQuery(class FD3D11DynamicRHI* InD3DRHI) :
	D3DRHI(InD3DRHI)
{

}

void FD3D11DisjointTimeStampQuery::StartTracking()
{
	ID3D11DeviceContext* D3D11DeviceContext = D3DRHI->GetDeviceContext();
	D3D11DeviceContext->Begin(DisjointQuery);
}

void FD3D11DisjointTimeStampQuery::EndTracking()
{
	ID3D11DeviceContext* D3D11DeviceContext = D3DRHI->GetDeviceContext();
	D3D11DeviceContext->End(DisjointQuery);
}

UBOOL FD3D11DisjointTimeStampQuery::WasDisjoint()
{
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointQueryData;

	ID3D11DeviceContext* D3D11DeviceContext = D3DRHI->GetDeviceContext();
	HRESULT D3DResult = D3D11DeviceContext->GetData(DisjointQuery, &DisjointQueryData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);

	const DOUBLE StartTime = appSeconds();
	while (D3DResult == S_FALSE && (appSeconds() - StartTime) < 0.5)
	{
		appSleep(0.005f);
		D3DResult = D3D11DeviceContext->GetData(DisjointQuery, &DisjointQueryData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);
	}

	return ((D3DResult == S_FALSE) || (DisjointQueryData.Disjoint == TRUE));
}

void FD3D11DisjointTimeStampQuery::InitDynamicRHI()
{
	D3D11_QUERY_DESC QueryDesc;
	QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	QueryDesc.MiscFlags = 0;

	VERIFYD3D11RESULT(D3DRHI->GetDevice()->CreateQuery(&QueryDesc, DisjointQuery.GetInitReference()));
}

void FD3D11DisjointTimeStampQuery::ReleaseDynamicRHI()
{

}
