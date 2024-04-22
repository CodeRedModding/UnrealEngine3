/*=============================================================================
	GlobalShader.cpp: Global shader implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "GlobalShader.h"
#include "GlobalShaderNGP.h"

IMPLEMENT_SHADER_TYPE(,FVertexShaderNGP,TEXT("SimpleVS"),TEXT("main"),SF_Vertex,0,0);
IMPLEMENT_SHADER_TYPE(,FPixelShaderNGP,TEXT("SimpleFS"),TEXT("main"),SF_Pixel,0,0);

TMap<FProgramKey,FVertexShaderNGP*> GGlobalVertexShaderMapNGP;
TMap<FProgramKey,FPixelShaderNGP*> GGlobalPixelShaderMapNGP;

#if WITH_GFx && NGP
QWORD GProgramKeyGFxBlendMask = FProgramKeyData::GetFieldMask(FProgramKeyData::PKDT_GFxBlendMode);
INT GProgramKeyGFxBlendShift = FProgramKeyData::GetFieldShift(FProgramKeyData::PKDT_GFxBlendMode);
#endif

struct FInterpolatorUsage
{
	struct FInterpolator
	{
		/** Original interpolator variable type (e.g. "float2" or "float4" */
		FString ScannedType;
		/** Original interpolator variable name */
		FString ScannedName;
		/** Original interpolator precision (0=low, 1=medium, 2=high) */
		INT ScannedPrecision;
		/** Number of array elements (0 or 1 means not an array) */
		INT NumArrayElements;
	};

	/** Color interpolators (low precision), max 2. */
	TArray<FInterpolator> Colors;
	/** Texcoord interpolators (medium/high precision), max 10. */
	TArray<FInterpolator> TexCoords;
};


/**
 * Checks if a character is one of the characters in the specified string.
 *
 * @param Char				Character to test
 * @param PossibleMatches	String of characters to test against
 * @return					TRUE if a match was found
 */
UBOOL MatchesChar( const TCHAR Char, const TCHAR* PossibleMatches )
{
	while ( *PossibleMatches )
	{
		if ( *PossibleMatches == Char )
		{
			return TRUE;
		}
		PossibleMatches++;
	}
	return FALSE;
}

/**
 * Parses a string for a token, delimited by any of the characters in the specified string.
 *
 * @param String		String to parse
 * @param Delimiters	String of possible delimiter characters
 * @param StartIndex	Index of the first character in the string to start searching at
 * @param LeftIndex		[out] Index to the first character in the found token
 * @param RightIndex	[out] Index of the first delimiter after the found token
 * @return				TRUE if a token was found
 */
UBOOL ParseToken( const TCHAR* String, const TCHAR* Delimiters, INT StartIndex, INT& LeftIndex, INT& RightIndex )
{
	// Skip leading delimiters.
	LeftIndex = StartIndex;
	while ( String[LeftIndex] && MatchesChar( String[LeftIndex], Delimiters ) )
	{
		LeftIndex++;
	}

	// Scan for ending delimiters.
	RightIndex = LeftIndex;
	while ( String[RightIndex] && !MatchesChar( String[RightIndex], Delimiters ) )
	{
		RightIndex++;
	}

	return String[LeftIndex] != 0;
}

/**
 * Parses shader source code to find the next interpolator keyword and gather information about it.
 *
 * @param ShaderSource		Shader source code to parse
 * @param Frequency			Whether it's a vertex or pixel shader
 * @param StartIndex		Index to the first character in the string to start searching at
 * @param ScannedType		[out] Interpolator variable type
 * @param ScannedName		[out] Interpolator variable name
 * @param Precision			[out] Interpolator precision (0=low, 1=medium, 2=high)
 * @return					Index of the next character in the string to continue searching from, or -1 if no more interpolators were found.
 */
INT NGPFindInterpolator( const FString& ShaderSource, EShaderFrequency Frequency, INT StartIndex, FInterpolatorUsage::FInterpolator& Usage )
{
	const TCHAR* Delimiters = TEXT(" _,;(){}\r\n\t");
	const TCHAR* NameDelimiters = TEXT(" ,;(){}\r\n\t");
	const TCHAR* Keyword = (Frequency == SF_Vertex) ? TEXT("OUT_VARYING_") : TEXT("IN_VARYING_");
	const INT KeywordLength = appStrlen( Keyword );
	const TCHAR* ArrayKeyword = (Frequency == SF_Vertex) ? TEXT("OUT_VARYING2_") : TEXT("IN_VARYING2_");
	const INT ArrayKeywordLength = appStrlen( ArrayKeyword );

	INT Offset = -1;
	INT ArrayLeftIndex = ShaderSource.InStr( ArrayKeyword, FALSE, FALSE, StartIndex );
	INT ArrayElements = 1;
	if (ArrayLeftIndex >= 0)
	{
		// parse out the array count
		Offset = ArrayLeftIndex + ArrayKeywordLength;
		INT NextUnderscore = ShaderSource.InStr(TEXT("_"), FALSE, FALSE, Offset);
		checkf(NextUnderscore != INDEX_NONE, TEXT("Failed to parse the varying array string properly"));

		// get the number as a string
		FString ElementsString = ShaderSource.Mid(Offset, NextUnderscore - Offset);
		Usage.NumArrayElements = appAtoi(*ElementsString);

		// skip over it
		Offset = NextUnderscore + 1;
	}
	else
	{
		INT LeftIndex = ShaderSource.InStr( Keyword, FALSE, FALSE, StartIndex );
		if (LeftIndex >= 0)
		{
			Offset = LeftIndex + KeywordLength;
			Usage.NumArrayElements = 1;
		}
	}

	// did we find anything to skip over?
	if ( Offset != -1 )
	{
		if ( appStrncmp( *ShaderSource + Offset, TEXT("HIGH_"), 5 ) == 0 )
		{
			Usage.ScannedPrecision = 2;
			Offset += 5;
		}
		else if ( appStrncmp( *ShaderSource + Offset, TEXT("MEDIUM_"), 7 ) == 0 )
		{
			Usage.ScannedPrecision = 1;
			Offset += 7;
		}
		else if ( appStrncmp( *ShaderSource + Offset, TEXT("LOW_"), 4 ) == 0 )
		{
			Usage.ScannedPrecision = 0;
			Offset += 4;
		}
		else
		{
			appErrorf(TEXT("Failed to parse the varying string properly [%s]"), *ShaderSource.Mid(Offset, 40));
		}

		if ( Offset > 0 )
		{
			INT Index1 = 0, Index2 = 0;
			if ( ParseToken( *ShaderSource, Delimiters, Offset, Index1, Index2 ) )
			{
				Usage.ScannedType = ShaderSource.Mid( Index1, Index2 - Index1 );
				if ( ShaderSource[Index2] && ParseToken( *ShaderSource, NameDelimiters, Index2 + 1, Index1, Index2 ) )
				{
					Usage.ScannedName = ShaderSource.Mid( Index1, Index2 - Index1 );
					return Index2;
				}
			}
		}
	}
	return INDEX_NONE;
}

/**
 * Parses the shader source code and gather all interpolator usage.
 *
 * @param ShaderSource		Shader source code to parse
 * @param Frequency			Whether it's a vertex or pixel shader
 * @param Interpolators		[out] Upon return, contains all interpolators used by the shader
 */
void NGPParseInterpolatorUsage( FString& ShaderSource, EShaderFrequency Frequency, FInterpolatorUsage& Interpolators )
{
	FInterpolatorUsage::FInterpolator Usage;
	Interpolators.Colors.Empty();
	Interpolators.TexCoords.Empty();
	INT Index = NGPFindInterpolator( ShaderSource, Frequency, 0, Usage );
	while ( Index >= 0 )
	{
		if ( Usage.ScannedPrecision == 0 && Interpolators.Colors.Num() < 2 )
		{
			Interpolators.Colors.AddItem( Usage );
		}
		else
		{
			Interpolators.TexCoords.AddItem( Usage );
		}
		Index = NGPFindInterpolator( ShaderSource, Frequency, Index, Usage );
	}
}

/**
 * Modifies the shader source code by replacing interpolator keywords with the provided register assignments.
 *
 * @param ShaderSource		[in/out] Shader source code that will be modified
 * @param Frequency			Whether it's a vertex or pixel shader
 * @param Interpolators		Interpolators to use (previously set up by NGPParseInterpolatorUsage)
 */
void NGPApplyInterpolatorUsage( FString& ShaderSource, EShaderFrequency Frequency, FInterpolatorUsage& Interpolators )
{
	const TCHAR* UpperDirection = (Frequency == SF_Vertex) ? TEXT("OUT") : TEXT("IN");
	const TCHAR* LowerDirection = (Frequency == SF_Vertex) ? TEXT("out") : TEXT("in");
	const TCHAR* Precision[3] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH") };

	// Assign low precision interpolators.
	for ( INT InterpolatorIndex=0; InterpolatorIndex < Interpolators.Colors.Num(); InterpolatorIndex++ )
	{
		const FInterpolatorUsage::FInterpolator& Usage = Interpolators.Colors(InterpolatorIndex);
		FString Keyword = FString::Printf( TEXT("%s_VARYING_%s_%s_%s"), UpperDirection, Precision[Usage.ScannedPrecision], *Usage.ScannedType, *Usage.ScannedName );
		checkf(Usage.NumArrayElements <= 1, TEXT("Low precision varying arrays are not currently supported"));
		FString Interpolator = FString::Printf( TEXT("%s %s %s : COLOR%d"), LowerDirection, *Usage.ScannedType, *Usage.ScannedName, InterpolatorIndex );
		ShaderSource.ReplaceInline( *Keyword, *Interpolator );
	}

	// Assign medium/high precision interpolators.
	INT RunningTexCoord = 0;
	for ( INT InterpolatorIndex=0; InterpolatorIndex < Interpolators.TexCoords.Num(); InterpolatorIndex++ )
	{
		const FInterpolatorUsage::FInterpolator& Usage = Interpolators.TexCoords(InterpolatorIndex);
		FString Keyword;
		FString Interpolator;
		// handle array interpolators, and use use up a tex coord for each element
		if (Usage.NumArrayElements > 1)
		{
			Keyword = FString::Printf( TEXT("%s_VARYING2_%d_%s_%s_%s"), UpperDirection, Usage.NumArrayElements, Precision[Usage.ScannedPrecision], *Usage.ScannedType, *Usage.ScannedName );
			Interpolator = FString::Printf( TEXT("%s %s %s[%d] : TEXCOORD%d"), LowerDirection, *Usage.ScannedType, *Usage.ScannedName, Usage.NumArrayElements, RunningTexCoord );
			RunningTexCoord += Usage.NumArrayElements;
		}
		else
		{
			Keyword = FString::Printf( TEXT("%s_VARYING_%s_%s_%s"), UpperDirection, Precision[Usage.ScannedPrecision], *Usage.ScannedType, *Usage.ScannedName );
			Interpolator  = FString::Printf( TEXT("%s %s %s : TEXCOORD%d"), LowerDirection, *Usage.ScannedType, *Usage.ScannedName, RunningTexCoord );
			RunningTexCoord += 1;
		}
		ShaderSource.ReplaceInline( *Keyword, *Interpolator );
	}

	//@TODO: Scan for mismatching interpolators.
	//@TODO: Check for register overflow.
	//@TODO: Pack into proper byte4 and float4 types.
}

void NGPBeginCompileShader( TArray<FNGPShaderCompileInfo>& ShaderCompileInfos, const TCHAR* VertexFileName, const TCHAR* PixelFileName, const FProgramKey& ProgramKey )
{
	// Scan for interpolators.
	FString VertexShader;
	FString PixelShader;
	if ( appLoadFileToString( VertexShader, VertexFileName ) && appLoadFileToString( PixelShader, PixelFileName ) )
	{
		FInterpolatorUsage Interpolators;
		NGPParseInterpolatorUsage( VertexShader, SF_Vertex, Interpolators );
		NGPApplyInterpolatorUsage( VertexShader, SF_Vertex, Interpolators );
		NGPApplyInterpolatorUsage( PixelShader, SF_Pixel, Interpolators );

		// Overwrite the files with the new shader sources.
		appSaveStringToFile( VertexShader, VertexFileName );
		appSaveStringToFile( PixelShader, PixelFileName );

		{
			// Begin compiling the vertex shader asynchronously.
			FNGPShaderCompileInfo* Info = new (ShaderCompileInfos) FNGPShaderCompileInfo(ProgramKey, SF_Vertex);
			FShaderTarget Target;
			Target.Platform = SP_NGP;
			Target.Frequency = SF_Vertex;
			FShaderCompilerEnvironment Environment;
			::BeginCompileShader( 0, NULL, NULL, VertexFileName, TEXT("main"), Target, Environment );
		}

		{
			// Begin compiling the pixel shader asynchronously.
			FNGPShaderCompileInfo* Info = new (ShaderCompileInfos) FNGPShaderCompileInfo(ProgramKey, SF_Pixel);
			FShaderTarget Target;
			Target.Platform = SP_NGP;
			Target.Frequency = SF_Pixel;
			FShaderCompilerEnvironment Environment;
			::BeginCompileShader( 0, NULL, NULL, PixelFileName, TEXT("main"), Target, Environment );
		}
	}
}

void NGPFinishCompileShaders( const TArray<FNGPShaderCompileInfo>& ShaderCompileInfos )
{
	TArray<TRefCountPtr<FShaderCompileJob> > CompilationResults;
	UBOOL bDebugDump = FALSE;
	GShaderCompilingThreadManager->FinishCompiling(CompilationResults, TEXT("Global"), TRUE, bDebugDump);

	// Create the shaders.
	check( ShaderCompileInfos.Num() == CompilationResults.Num() );
	GGlobalVertexShaderMapNGP.Empty();
	GGlobalPixelShaderMapNGP.Empty();
	for ( INT ShaderIndex=0; ShaderIndex < CompilationResults.Num(); ++ShaderIndex )
	{
		const FShaderCompileJob& CompileJob = *CompilationResults(ShaderIndex);
		if ( CompileJob.bSucceeded )
		{
			const FNGPShaderCompileInfo& ShaderInfo = ShaderCompileInfos(ShaderIndex);
			check( CompileJob.Target.Frequency == ShaderInfo.Frequency );
			if ( CompileJob.Target.Frequency == SF_Vertex )
			{
				FVertexShaderNGP* VertexShader = new FVertexShaderNGP( FVertexShaderNGP::CompiledShaderInitializerType(NULL,CompileJob.Output) );
				VertexShader->Setup( ShaderInfo );
				GGlobalVertexShaderMapNGP.Set( VertexShader->GetProgramKey(), VertexShader );
			}
			else
			{
				FPixelShaderNGP* PixelShader = new FPixelShaderNGP( FPixelShaderNGP::CompiledShaderInitializerType(NULL,CompileJob.Output) );
				PixelShader->Setup( ShaderInfo );
				GGlobalPixelShaderMapNGP.Set( PixelShader->GetProgramKey(), PixelShader );
			}
//			CompileJob.Output.ParameterMap.VerifyBindingsAreComplete( *CompileJob.SourceFilename, EShaderFrequency(CompileJob.Target.Frequency), CompileJob.VFType );
		}
	}
}

void SerializeGlobalShadersNGP( FArchive& Ar )
{
	BYTE Platform = SP_NGP;
	Ar << Platform;

	if (Ar.IsSaving())
	{
		INT NumVertexShaders = GGlobalVertexShaderMapNGP.Num();
		INT NumPixelShaders = GGlobalPixelShaderMapNGP.Num();
		Ar << NumVertexShaders;
		Ar << NumPixelShaders;

		// Save the vertex shaders.
		for(TMap<FProgramKey,FVertexShaderNGP*>::TIterator ShaderIt(GGlobalVertexShaderMapNGP); ShaderIt; ++ShaderIt)
		{
			FShader* Shader = ShaderIt.Value();
			BYTE Frequency = Shader->GetTarget().Frequency;
			Ar << Frequency;

			// Write a placeholder value for the skip offset.
			INT SkipOffset = Ar.Tell();
			Ar << SkipOffset;

			Shader->Serialize( Ar );

			// Write the actual offset of the end of the shader data over the placeholder value written above.
			INT EndOffset = Ar.Tell();
			Ar.Seek(SkipOffset);
			Ar << EndOffset;
			Ar.Seek(EndOffset);
		}

		// Save the pixel shaders.
		for(TMap<FProgramKey,FPixelShaderNGP*>::TIterator ShaderIt(GGlobalPixelShaderMapNGP); ShaderIt; ++ShaderIt)
		{
			FShader* Shader = ShaderIt.Value();
			BYTE Frequency = Shader->GetTarget().Frequency;
			Ar << Frequency;

			// Write a placeholder value for the skip offset.
			INT SkipOffset = Ar.Tell();
			Ar << SkipOffset;

			Shader->Serialize( Ar );

			// Write the actual offset of the end of the shader data over the placeholder value written above.
			INT EndOffset = Ar.Tell();
			Ar.Seek(SkipOffset);
			Ar << EndOffset;
			Ar.Seek(EndOffset);
		}
	}
	else
	{
		INT NumVertexShaders = 0;
		INT NumPixelShaders = 0;
		Ar << NumVertexShaders;
		Ar << NumPixelShaders;

		GGlobalVertexShaderMapNGP.Empty();
		GGlobalPixelShaderMapNGP.Empty();
		for ( INT ShaderIndex=0; ShaderIndex < (NumVertexShaders + NumPixelShaders); ++ShaderIndex )
		{
			BYTE Frequency = SF_Vertex;
			INT NextOffset = 0;
			Ar << Frequency;
			Ar << NextOffset;
			if (Frequency == SF_Vertex)
			{
				FVertexShaderNGP* Shader = new FVertexShaderNGP;
				Shader->Serialize( Ar );
				GGlobalVertexShaderMapNGP.Set( Shader->GetProgramKey(), Shader );
			}
			else
			{
				FPixelShaderNGP* Shader = new FPixelShaderNGP;
				Shader->Serialize( Ar );
				GGlobalPixelShaderMapNGP.Set( Shader->GetProgramKey(), Shader );
			}
		}
	}
}


FShaderNGP::FShaderNGP( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
	:	FGlobalShader( Initializer )
{
}

void FShaderNGP::Setup( const FNGPShaderCompileInfo& Info )
{
	ProgramKey = Info.ProgramKey;
}

FShaderNGP* FShaderNGP::FindShader( const FProgramKey& InProgramKey, EShaderFrequency Frequency )
{
    FProgramKey SearchProgramKey = InProgramKey;
#if WITH_GFx && NGP
    SearchProgramKey.Data[1] &= ~GProgramKeyGFxBlendMask;
#endif
    if ( Frequency == SF_Vertex )
    {
        FVertexShaderNGP** Shader = GGlobalVertexShaderMapNGP.Find( SearchProgramKey );
        return Shader ? *Shader : NULL;
    }
    else
    {
        FPixelShaderNGP** Shader = GGlobalPixelShaderMapNGP.Find( SearchProgramKey );
        return Shader ? *Shader : NULL;
    }
}

UBOOL FShaderNGP::ShouldCache(EShaderPlatform Platform)
{
	return (Platform == SP_NGP);
}

UBOOL FShaderNGP::Serialize(FArchive& Ar)
{
	UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << ProgramKey;
	return bShaderHasOutdatedParameters;
}


/**
 * Vertex shader for NGP.
 */
FVertexShaderNGP::FVertexShaderNGP( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
:	FShaderNGP( Initializer )
{
	//@TODO: Bind parameters
}

UBOOL FVertexShaderNGP::Serialize(FArchive& Ar)
{
	UBOOL bShaderHasOutdatedParameters = FShaderNGP::Serialize(Ar);
	//@TODO: Serialize parameters
	return bShaderHasOutdatedParameters;
}

/**
 * Pixel shader for NGP.
 */
FPixelShaderNGP::FPixelShaderNGP( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
:	FShaderNGP( Initializer )
{
	//@TODO: Bind parameters
}

UBOOL FPixelShaderNGP::Serialize(FArchive& Ar)
{
	UBOOL bShaderHasOutdatedParameters = FShaderNGP::Serialize(Ar);
	//@TODO: Serialize parameters
	return bShaderHasOutdatedParameters;
}
