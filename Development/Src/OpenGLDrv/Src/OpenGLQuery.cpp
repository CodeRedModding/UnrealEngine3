/*=============================================================================
	OpenGLQuery.cpp: OpenGL query RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

FOcclusionQueryRHIRef FOpenGLDynamicRHI::CreateOcclusionQuery()
{
	GLuint QueryID;
	glGenQueries(1, &QueryID);

	return new FOpenGLOcclusionQuery(QueryID);
}

void FOpenGLDynamicRHI::ResetOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(OcclusionQuery,OcclusionQuery);

	OcclusionQuery->bResultIsCached = FALSE;
}

void FOpenGLDynamicRHI::BeginOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(OcclusionQuery,OcclusionQuery);

	if (CachedState.OcclusionQuery != 0)
	{
		glEndQuery(GL_SAMPLES_PASSED_ARB);
	}

	glBeginQuery(GL_SAMPLES_PASSED_ARB, OcclusionQuery->Resource);
	CachedState.OcclusionQuery = OcclusionQuery->Resource;
}

void FOpenGLDynamicRHI::EndOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	glEndQuery(GL_SAMPLES_PASSED_ARB);
	CachedState.OcclusionQuery = 0;
}

UBOOL FOpenGLDynamicRHI::GetOcclusionQueryResult(FOcclusionQueryRHIParamRef OcclusionQueryRHI,DWORD& OutNumPixels,UBOOL bWait)
{
	DYNAMIC_CAST_OPENGLRESOURCE(OcclusionQuery,OcclusionQuery);

	UBOOL bSuccess = TRUE;

	if(!OcclusionQuery->bResultIsCached)
	{
		// Check if the query is finished
		GLint Result = 0;
		glGetQueryObjectiv(OcclusionQuery->Resource, GL_QUERY_RESULT_AVAILABLE, &Result);

		// Isn't the query finished yet, and can we wait for it?
		if (Result == GL_FALSE && bWait)
		{
			SCOPE_CYCLE_COUNTER( STAT_OcclusionResultTime );
			DWORD IdleStart = appCycles();
			DOUBLE StartTime = appSeconds();
			do 
			{
				glGetQueryObjectiv(OcclusionQuery->Resource, GL_QUERY_RESULT_AVAILABLE, &Result);

				if ((appSeconds() - StartTime) > 0.5)
				{
					debugf(TEXT("Timed out while waiting for GPU to catch up. (500 ms)"));
					break;
				}
			} while (Result == GL_FALSE);
			GRenderThreadIdle += appCycles() - IdleStart;
		}

		if (Result == GL_TRUE)
		{
			glGetQueryObjectuiv(OcclusionQuery->Resource, GL_QUERY_RESULT, &OcclusionQuery->Result);
		}
		else if (Result == GL_FALSE && bWait)
		{
			bSuccess = FALSE;
		}
	}

	OutNumPixels = (DWORD)OcclusionQuery->Result;
	OcclusionQuery->bResultIsCached = bSuccess;

	return bSuccess;
}
