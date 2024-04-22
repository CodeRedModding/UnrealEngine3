/*=============================================================================
	ES2RHIDebug.cpp: OpenGL ES 2.0 RHI debugging functionality
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Engine.h"
#include "ES2RHIPrivate.h"

#if WITH_ES2_RHI

/** The current debug mode we are in */
EThreeTouchMode GThreeTouchMode = ThreeTouchMode_None;

/** Runtime render control variables */
UBOOL GMobilePrepass = FALSE;


/**
 * Print out information about a GL object (mostly for shader compiling output)
 *
 * @param Obj GL object (shader most likely)
 * @param Description Readable text to describe the object
 * @return Response from appMsgf if there was a failure, or -1 if no issues
 */
INT PrintInfoLog(GLuint Obj, const TCHAR* Description)
{
	INT Result = -1;
#if !FINAL_RELEASE && !FLASH
	int InfologLength = 0;
	int MaxLength;
	ANSICHAR* InfoLog = NULL;

	if( glIsShader( Obj ) )
	{
		GLint Compiled;
		glGetShaderiv( Obj, GL_COMPILE_STATUS, &Compiled );
		if (Compiled == 0)
		{
			debugf(TEXT("Shader '%s' compile status = %d"), Description, Compiled );
			glGetShaderiv( Obj, GL_INFO_LOG_LENGTH, &MaxLength );
			InfoLog = (ANSICHAR*)appMalloc(MaxLength);
			glGetShaderInfoLog( Obj, MaxLength, &InfologLength, InfoLog );
		}
	}
	else
	{
		GLint Linked;
		glGetProgramiv( Obj, GL_LINK_STATUS, &Linked );
		if (Linked == 0)
		{
			debugf(TEXT("Program '%s' link status = %d"), Description, Linked );
			glGetProgramiv( Obj, GL_INFO_LOG_LENGTH, &MaxLength );
			InfoLog = (ANSICHAR*)appMalloc(MaxLength);
			glGetProgramInfoLog( Obj, MaxLength, &InfologLength, InfoLog );
		}
	}

	if ( InfologLength > 0 )
	{
#if _WINDOWS
		FString MessageText;
		MessageText += FString::Printf(TEXT("Error compiling shader [%s]: %s\n"), Description, ANSI_TO_TCHAR(InfoLog));
		MessageText += TEXT("    0 : Starting line number for errors in code-generated defines\n");
		MessageText += TEXT(" 1000 : Starting line number for errors in Prefix_Common.msf\n");
		MessageText += TEXT(" 2000 : Starting line number for errors in Prefix_[Pixel|Vertex]Shader.msf\n");
		MessageText += TEXT("10000 : Starting line number for errors in main shader source file\n");
		MessageText += TEXT("\n");
		MessageText += TEXT("Would you like to recompile the shader? (Selecting no will crash the app)");

		Result = appMsgf(AMT_YesNo, *MessageText);
		if (Result == ART_No)
#endif
		{
			debugf( TEXT("ES2 RHI: Error compiling shader: %s\n"), ANSI_TO_TCHAR(InfoLog));
			GLog->Flush();
			appErrorf(TEXT("Failed to compile shader code: %s\n\n%s\n"), Description, ANSI_TO_TCHAR(InfoLog));
		}
	}

	appFree(InfoLog);
#endif

	return Result;
}

#endif
