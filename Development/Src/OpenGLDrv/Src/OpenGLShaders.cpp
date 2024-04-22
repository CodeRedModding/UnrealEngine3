/*=============================================================================
	OpenGLShaders.cpp: OpenGL shader RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

#if _WINDOWS
#include <mmintrin.h>
#elif PLATFORM_MACOSX
#include <xmmintrin.h>
#endif

static UBOOL GIsProgramCacheUpToDate = FALSE;
static void UpdateProgramCache();
TMap<DWORD,FVertexShaderRHIRef> GLoadedVertexShaders;
TMap<DWORD,FPixelShaderRHIRef> GLoadedPixelShaders;

FVertexShaderRHIRef FOpenGLDynamicRHI::CreateVertexShader(const TArray<BYTE>& Code)
{
	DWORD CodeCrc = appMemCrc(&Code(0), Code.Num());
	FVertexShaderRHIRef VertexShaderRHI = GLoadedVertexShaders.FindRef(CodeCrc);
	if (VertexShaderRHI)
	{
		return VertexShaderRHI;
	}

	FOpenGLVertexShader *Shader = new FOpenGLVertexShader(Code);
	Shader->Compile();
	GLoadedVertexShaders.Set(Shader->CodeCrc, Shader);
	return Shader;
}

FPixelShaderRHIRef FOpenGLDynamicRHI::CreatePixelShader(const TArray<BYTE>& Code)
{
	DWORD CodeCrc = appMemCrc(&Code(0), Code.Num());
	FPixelShaderRHIRef PixelShaderRHI = GLoadedPixelShaders.FindRef(CodeCrc);
	if (PixelShaderRHI)
	{
		return PixelShaderRHI;
	}

	FOpenGLPixelShader *Shader = new FOpenGLPixelShader(Code);
	Shader->Compile();
	GLoadedPixelShaders.Set(Shader->CodeCrc, Shader);
	return Shader;
}

/**
* Key used to map a set of unique vertex/pixel shader combinations to
* a program resource
*/
class FGLSLProgramKey
{
public:

	FGLSLProgramKey(
		FVertexShaderRHIParamRef InVertexShader,
		FPixelShaderRHIParamRef InPixelShader,
		DWORD* InStreamStrides
		)
		:	VertexShader(InVertexShader)
		,	PixelShader(InPixelShader)
	{
		for( UINT Idx=0; Idx < MaxVertexElementCount; ++Idx )
		{
			StreamStrides[Idx] = (BYTE)InStreamStrides[Idx];
		}
	}

	/**
	* Equality is based on render and depth stencil targets 
	* @param Other - instance to compare against
	* @return TRUE if equal
	*/
	friend UBOOL operator ==(const FGLSLProgramKey& A,const FGLSLProgramKey& B)
	{
		return	A.VertexShader == B.VertexShader && A.PixelShader == B.PixelShader && !appMemcmp(A.StreamStrides, B.StreamStrides, sizeof(A.StreamStrides));
	}

	/**
	* Get the hash for this type. 
	* @param Key - struct to hash
	* @return DWORD hash based on type
	*/
	friend DWORD GetTypeHash(const FGLSLProgramKey &Key)
	{
		return GetTypeHash(Key.VertexShader) ^ GetTypeHash(Key.PixelShader) ^ appMemCrc(Key.StreamStrides,sizeof(Key.StreamStrides));
	}

	FVertexShaderRHIRef VertexShader;
	FPixelShaderRHIRef PixelShader;
	/** assuming strides are always smaller than 8-bits */
	BYTE StreamStrides[MaxVertexElementCount];
};

typedef TMap<FGLSLProgramKey,GLuint> FGLSLProgramCache;

/** Lazily initialized program cache singleton. */
static FGLSLProgramCache& GetGLSLProgramCache()
{
	static FGLSLProgramCache GLSLProgramCache;
	return GLSLProgramCache;
}

static GLuint GetGLSLProgram(FVertexShaderRHIParamRef VertexShaderRHI, FPixelShaderRHIParamRef PixelShaderRHI, DWORD* StreamStrides)
{
	DYNAMIC_CAST_OPENGLRESOURCE(VertexShader,VertexShader);
	DYNAMIC_CAST_OPENGLRESOURCE(PixelShader,PixelShader);

	GLuint Program = GetGLSLProgramCache().FindRef(FGLSLProgramKey(VertexShaderRHI, PixelShaderRHI, StreamStrides));

	DWORD EmptyStrides[MaxVertexElementCount] = {0};
	if (!Program)
	{
		Program = GetGLSLProgramCache().FindRef(FGLSLProgramKey(VertexShaderRHI, PixelShaderRHI, EmptyStrides));
		if (Program)
		{
			GetGLSLProgramCache().Remove(FGLSLProgramKey(VertexShaderRHI, PixelShaderRHI, EmptyStrides));
			GetGLSLProgramCache().Set(FGLSLProgramKey(VertexShaderRHI, PixelShaderRHI, StreamStrides), Program);
			return Program;
		}
	}

	if (!Program && appMemcmp(StreamStrides, EmptyStrides, MaxVertexElementCount) == 0)
	{
		FGLSLProgramCache &Cache = GetGLSLProgramCache();
		for (FGLSLProgramCache::TConstIterator It(Cache); It; ++It)
		{
			FGLSLProgramKey Key = It.Key();
			FVertexShaderRHIParamRef VertexShader2RHI = Key.VertexShader;
			FPixelShaderRHIParamRef PixelShader2RHI = Key.PixelShader;
			DYNAMIC_CAST_OPENGLRESOURCE(VertexShader,VertexShader2);
			DYNAMIC_CAST_OPENGLRESOURCE(PixelShader,PixelShader2);
			if (VertexShader2->CodeCrc == VertexShader->CodeCrc && PixelShader2->CodeCrc == PixelShader->CodeCrc)
			{
				return It.Value();
			}
		}
	}

	if (!Program)
	{
		Program = glCreateProgram();

		glAttachShader(Program, VertexShader->Resource);
		glAttachShader(Program, PixelShader->Resource);

		glBindAttribLocation(Program, GLAttr_Position, "_Un_AttrPosition0");
		glBindAttribLocation(Program, GLAttr_Tangent, "_Un_AttrTangent0");
		glBindAttribLocation(Program, GLAttr_Color0, "_Un_AttrColor0");
		glBindAttribLocation(Program, GLAttr_Color1, "_Un_AttrColor1");
		glBindAttribLocation(Program, GLAttr_Binormal, "_Un_AttrBinormal0");
		glBindAttribLocation(Program, GLAttr_Normal, "_Un_AttrNormal0");
		glBindAttribLocation(Program, GLAttr_Weights, "_Un_AttrBlendWeight0");
		glBindAttribLocation(Program, GLAttr_Bones, "_Un_AttrBlendIndices0");
		glBindAttribLocation(Program, GLAttr_TexCoord0, "_Un_AttrTexCoord0");
		glBindAttribLocation(Program, GLAttr_TexCoord1, "_Un_AttrTexCoord1");
		glBindAttribLocation(Program, GLAttr_TexCoord2, "_Un_AttrTexCoord2");
		glBindAttribLocation(Program, GLAttr_TexCoord3, "_Un_AttrTexCoord3");
		glBindAttribLocation(Program, GLAttr_TexCoord4, "_Un_AttrTexCoord4");
		glBindAttribLocation(Program, GLAttr_TexCoord5, "_Un_AttrTexCoord5");
		glBindAttribLocation(Program, GLAttr_TexCoord6, "_Un_AttrTexCoord6");
		glBindAttribLocation(Program, GLAttr_TexCoord7, "_Un_AttrTexCoord7");

		glLinkProgram(Program);

#if _DEBUG
		GLint LinkStatus;
		glGetProgramiv(Program, GL_LINK_STATUS, &LinkStatus);
		if (LinkStatus != GL_TRUE)
		{
			GLint LogLength;
			glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);
			if (LogLength > 1)
			{
				ANSICHAR *LinkLog = (ANSICHAR *)appMalloc(LogLength);
				glGetProgramInfoLog(Program, LogLength, NULL, LinkLog);
				appErrorf(TEXT("Failed to link GLSL program. Link log:\n%s"), ANSI_TO_TCHAR(LinkLog));
				appFree(LinkLog);
			}
			else
			{
				appErrorf(TEXT("Failed to link GLSL program. No link log."));
			}

			glDeleteProgram(Program);
			Program = 0;
		}
		else
#endif
		{
			GetGLSLProgramCache().Set(FGLSLProgramKey(VertexShaderRHI, PixelShaderRHI, StreamStrides), Program);
		}
	}

	return Program;
}

/**
 * Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
 * @param VertexDeclaration - existing vertex decl
 * @param StreamStrides - optional stream strides
 * @param VertexShader - existing vertex shader
 * @param PixelShader - existing pixel shader
 * @param MobileGlobalShaderType - global shader type to use for mobile
 */
FBoundShaderStateRHIRef FOpenGLDynamicRHI::CreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	DWORD* StreamStrides,
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	EMobileGlobalShaderType /*MobileGlobalShaderType*/
	)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateBoundShaderStateTime);

	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		NULL,
		StreamStrides,
		VertexShaderRHI,
		PixelShaderRHI
		);
	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		return new FOpenGLBoundShaderState(this,VertexDeclarationRHI,StreamStrides,VertexShaderRHI,PixelShaderRHI);
	}
}

template <ERHIResourceTypes ResourceTypeEnum, GLenum Type>
UBOOL TOpenGLShader<ResourceTypeEnum, Type>::Compile()
{
	if (Resource)
	{
		return TRUE;
	}

	Resource = glCreateShader(Type);
	GLchar *CodePtr = (GLchar *)&Code(0);

#if !OPENGL_USE_BINDABLE_UNIFORMS
	while (ANSICHAR *Bindable = strstr(CodePtr, "bindable uniform"))
	{
		Bindable[0] = '/';
		Bindable[1] = '*';
		Bindable[7] = '*';
		Bindable[8] = '/';
	}
#endif

	GLint CodeLength = strlen(CodePtr);
	glShaderSource(Resource, 1, (const GLchar **)&CodePtr, &CodeLength);
	glCompileShader(Resource);

	GLint CompileStatus;
	glGetShaderiv(Resource, GL_COMPILE_STATUS, &CompileStatus);
	if (CompileStatus != GL_TRUE)
	{
		GLint LogLength;
		glGetShaderiv(Resource, GL_INFO_LOG_LENGTH, &LogLength);
		if (LogLength > 1)
		{
			ANSICHAR *CompileLog = (ANSICHAR *)appMalloc(LogLength);
			glGetShaderInfoLog(Resource, LogLength, NULL, CompileLog);
			appErrorf(TEXT("Failed to compile shader. Compile log:\n%s"), ANSI_TO_TCHAR(CompileLog));
			appFree(CompileLog);
		}
		else
		{
			appErrorf(TEXT("Failed to compile shader. No compile log."));
		}

		glDeleteShader(Resource);
		Resource = 0;

		return FALSE;
	}
	else
	{
		Code.Empty();

		return TRUE;
	}
}

FOpenGLBoundShaderState::FOpenGLBoundShaderState(
	class FOpenGLDynamicRHI* InOpenGLRHI,
	FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
	DWORD* InStreamStrides,
	FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI
	)
:	CacheLink(NULL,InStreamStrides,InVertexShaderRHI,InPixelShaderRHI,this)
,	Resource(0)
,	OpenGLRHI(InOpenGLRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(VertexDeclaration,InVertexDeclaration);
	DYNAMIC_CAST_OPENGLRESOURCE(VertexShader,InVertexShader);
	DYNAMIC_CAST_OPENGLRESOURCE(PixelShader,InPixelShader);

	VertexDeclaration = InVertexDeclaration;
	check(IsValidRef(VertexDeclaration));
	VertexShader = InVertexShader;
	InVertexShader->Compile();

	if (InPixelShader)
	{
		InPixelShader->Compile();
		PixelShader = InPixelShader;
	}
	else
	{
		// use special null pixel shader when PixelSahder was set to NULL
		FPixelShaderRHIParamRef NullPixelShaderRHI = TShaderMapRef<FNULLPixelShader>(GetGlobalShaderMap())->GetPixelShader();
		DYNAMIC_CAST_OPENGLRESOURCE(PixelShader,NullPixelShader);
		NullPixelShader->Compile();
		PixelShader = NullPixelShader;
	}

	Setup(InStreamStrides);
}

void FOpenGLBoundShaderState::Setup(DWORD* InStreamStrides)
{
	UpdateProgramCache();

	AddRef();

	Resource = GetGLSLProgram(VertexShader, PixelShader, InStreamStrides);

	if (OpenGLRHI->CachedState.Program != Resource)
	{
		glUseProgram(Resource);
		OpenGLRHI->CachedState.Program = Resource;
		CheckOpenGLErrors();
	}

	GLint NumActiveUniforms = 0;
	glGetProgramiv(Resource, GL_ACTIVE_UNIFORMS, &NumActiveUniforms);

	if (NumActiveUniforms > 0)
	{
		GLint MaxUniformNameLength = 0;
		glGetProgramiv(Resource, GL_ACTIVE_UNIFORM_MAX_LENGTH, &MaxUniformNameLength);

		GLchar *UniformName = (GLchar *)appMalloc(MaxUniformNameLength);

		for (GLint UniformIndex = 0; UniformIndex < NumActiveUniforms; UniformIndex++)
		{
			GLint UniformSize = 0;
			GLenum UniformType = GL_NONE;
			glGetActiveUniform(Resource, UniformIndex, MaxUniformNameLength, NULL, &UniformSize, &UniformType, UniformName);
			CheckOpenGLErrors();

			if (UniformType == GL_SAMPLER_1D || UniformType == GL_SAMPLER_2D
				|| UniformType == GL_SAMPLER_3D || UniformType == GL_SAMPLER_1D_SHADOW
				|| UniformType == GL_SAMPLER_2D_SHADOW || UniformType == GL_SAMPLER_CUBE)
			{
				for (INT SamplerIndex = 0; SamplerIndex < 16; SamplerIndex++)
				{
					FString VSamplerName = FString::Printf(TEXT("VSampler%d"), SamplerIndex);
					FString PSamplerName = FString::Printf(TEXT("PSampler%d"), SamplerIndex);
					if (VSamplerName == ANSI_TO_TCHAR(UniformName))
					{
						GLint Location = glGetUniformLocation(Resource, UniformName);
						glUniform1i(Location, 15 - SamplerIndex);
						break;
					}
					else if (PSamplerName == ANSI_TO_TCHAR(UniformName))
					{
						GLint Location = glGetUniformLocation(Resource, UniformName);
						glUniform1i(Location, SamplerIndex);
						break;
					}
				}
			}
			else
			{
				SetupUniformArray(UniformName);
			}
		}

		appFree(UniformName);
	}
}

void FOpenGLBoundShaderState::SetupUniformArray(const GLchar *Name)
{
	INT ArrayNum = UniformArray_VLocal;
	const char *SearchFormat = "VConstFloat[%d]";
	INT ElementSize = sizeof(FVector4);

	if (strstr(Name, "VConstFloat"))
	{
	}
	else if (strstr(Name, "VConstGlobal"))
	{
		ArrayNum = UniformArray_VGlobal;
		SearchFormat = "VConstGlobal[%d]";
	}
	else if (strstr(Name, "VConstBones"))
	{
		ArrayNum = UniformArray_VBones;
		SearchFormat = "VConstBones[%d]";
	}
	else if (strstr(Name, "VConstBool"))
	{
		ArrayNum = UniformArray_VBool;
		SearchFormat = "VConstBool[%d]";
		ElementSize = sizeof(UINT);
	}
	else if (strstr(Name, "PConstFloat"))
	{
		ArrayNum = UniformArray_PLocal;
		SearchFormat = "PConstFloat[%d]";
	}
	else if (strstr(Name, "PConstGlobal"))
	{
		ArrayNum = UniformArray_PGlobal;
		SearchFormat = "PConstGlobal[%d]";
	}
	else if (strstr(Name, "PConstBool"))
	{
		ArrayNum = UniformArray_PBool;
		SearchFormat = "PConstBool[%d]";
		ElementSize = sizeof(UINT);
	}
	else
	{
		debugf(TEXT("Unhandled uniform %s in program %d"), ANSI_TO_TCHAR(Name), Resource);
		return;
	}

	FUniformArray &Array = UniformArrays[ArrayNum];
	Array.Location = glGetUniformLocation(Resource, Name);

	if (Array.Location != -1)
	{
		for (INT Index = 0; Index < 256; Index++)
		{
			char ConstText[255];
#if _WINDOWS
			sprintf_s(ConstText, 255, SearchFormat, Index);
#else
			snprintf(ConstText, 255, SearchFormat, Index);
#endif
			GLint ElementLocation = glGetUniformLocation(Resource, ConstText);
			if (ElementLocation == -1)
			{
				break;
			}
			else
			{
				Array.CacheSize += ElementSize;
			}
		}
		Array.Cache = appMalloc(Array.CacheSize);
		appMemzero(Array.Cache, Array.CacheSize);
	}
}

void FOpenGLBoundShaderState::Bind()
{
	if (OpenGLRHI->CachedState.Program != Resource)
	{
		glUseProgram(Resource);
		OpenGLRHI->CachedState.Program = Resource;
		CheckOpenGLErrors();
	}
}

void FOpenGLBoundShaderState::UpdateUniforms(INT ArrayNum, void *Data, UINT Size)
{
	GLint Location = UniformArrays[ArrayNum].Location;
	void *Cache = UniformArrays[ArrayNum].Cache;
	UINT CacheSize = UniformArrays[ArrayNum].CacheSize;
	Size = Min<UINT>(Size, CacheSize);

	if (Location != -1)
	{
		switch (ArrayNum)
		{
		case UniformArray_VLocal:
		case UniformArray_VGlobal:
		case UniformArray_VBones:
		case UniformArray_PLocal:
		case UniformArray_PGlobal:
		{
			UINT Count = Size / sizeof(FVector4);
			FLOAT *DataAddr = (FLOAT *)Data;
			FLOAT *CacheAddr = (FLOAT *)Cache;
			INT PastRegisterChanged = 0;

			for (UINT Index = 0; Index < Count; Index++)
			{
				// _WIN64 has ENABLE_VECTORINTRINSICS disabled at this moment, so we use SSE directly
				if (_mm_movemask_ps(_mm_cmpneq_ps(_mm_loadu_ps(CacheAddr ), _mm_loadu_ps(DataAddr))))
				{
					appMemcpy(CacheAddr, DataAddr, 4 * sizeof(FLOAT));
					PastRegisterChanged++;
				}
				else if (PastRegisterChanged)
				{
					glUniform4fv(Location - PastRegisterChanged, PastRegisterChanged, (const GLfloat *)DataAddr - (4 * PastRegisterChanged));
					PastRegisterChanged = 0;
				}
				DataAddr += 4;
				CacheAddr += 4;
				Location++;
			}

			if (PastRegisterChanged)
			{
				glUniform4fv(Location - PastRegisterChanged, PastRegisterChanged, (const GLfloat *)DataAddr - (4 * PastRegisterChanged));
			}
			break;
		}

		case UniformArray_VBool:
		case UniformArray_PBool:
			if (appMemcmp(Cache, Data, Size) != 0)
			{
				glUniform1iv(Location, Size / sizeof(UINT), (const GLint *)Data);
				appMemcpy(Cache, Data, Size);
			}
			break;
		}
	}
}

static TMultiMap<FString, FString> ShaderTypePairs;

static void PrepareShaderTypePairsMap()
{
	if (ShaderTypePairs.Num() > 0)
	{
		return;
	}

	ShaderTypePairs.Add(TEXT("FResolveVertexShader"), TEXT("FResolveSingleSamplePixelShader"));
	ShaderTypePairs.Add(TEXT("FResolveVertexShader"), TEXT("FResolveDepthPixelShader"));
	ShaderTypePairs.Add(TEXT("TMeshPaintVertexShader"), TEXT("TMeshPaintPixelShader"));
	ShaderTypePairs.Add(TEXT("TMeshPaintDilateVertexShader"), TEXT("TMeshPaintDilatePixelShader"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_CxformMultiply2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_Cxform2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraud>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureDFA>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGBMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGB>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColorMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_TextTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_CxformTextureMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_CxformTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxPixelShader<GFx_PS_SolidColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrixMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrix>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox1BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox1Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadowonly>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_T2>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_CxformMultiply2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_Cxform2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraud>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureDFA>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGBMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGB>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColorMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_TextTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_CxformTextureMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_CxformTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxPixelShader<GFx_PS_SolidColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrixMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrix>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox1BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox1Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadowonly>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTexNoAlpha>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_CxformMultiply2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_Cxform2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraud>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureDFA>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGBMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGB>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColorMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_TextTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_CxformTextureMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_CxformTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxPixelShader<GFx_PS_SolidColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrixMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrix>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox1BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox1Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadowonly>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32_NoTex>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_CxformMultiply2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_Cxform2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraud>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureDFA>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGBMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGB>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColorMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_TextTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_CxformTextureMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_CxformTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxPixelShader<GFx_PS_SolidColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrixMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrix>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox1BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox1Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadowonly>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iCF32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_CxformMultiply2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_Cxform2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraud>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureDFA>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGBMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGB>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColorMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_TextTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_CxformTextureMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_CxformTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxPixelShader<GFx_PS_SolidColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrixMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrix>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox1BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox1Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadowonly>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_XY16iC32>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_CxformMultiply2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_Cxform2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraud>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureDFA>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGBMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGB>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColorMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_TextTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_CxformTextureMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_CxformTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxPixelShader<GFx_PS_SolidColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrixMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrix>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox1BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox1Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadowonly>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Glyph>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_CxformMultiply2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiplyNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_Cxform2Texture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraudNoAddAlpha>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_CxformGouraud>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureDFA>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGBMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureSRGB>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColorMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_TextTextureColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_TextTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_CxformTextureMultiply>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_CxformTexture>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxPixelShader<GFx_PS_SolidColor>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrixMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FCMatrix>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox1BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox1Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2BlurMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Blur>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowonlyHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadowonly>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2ShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2Shadow>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlightKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowKnockout>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMulHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowMul>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadowHighlight>"));
	ShaderTypePairs.Add(TEXT("FGFxVertexShader<GFx_VS_Strip>"), TEXT("FGFxFilterPixelShader<FS2_FBox2InnerShadow>"));
	ShaderTypePairs.Add(TEXT("FFluidVertexShader"), TEXT("FFluidApplyPixelShader"));
	ShaderTypePairs.Add(TEXT("FFluidVertexShader"), TEXT("FFluidNormalPixelShader"));
	ShaderTypePairs.Add(TEXT("FFluidVertexShader"), TEXT("FFluidSimulatePixelShader"));
	ShaderTypePairs.Add(TEXT("FFluidVertexShader"), TEXT("FApplyForcePixelShader"));
	ShaderTypePairs.Add(TEXT("FVelocityVertexShader"), TEXT("FVelocityPixelShader"));
	ShaderTypePairs.Add(TEXT("FGammaCorrectionVertexShader"), TEXT("FGammaCorrectionPixelShader"));
	ShaderTypePairs.Add(TEXT("FTextureDensityVertexShader"), TEXT("FTextureDensityPixelShader"));
	ShaderTypePairs.Add(TEXT("FHitProxyVertexShader"), TEXT("FHitProxyPixelShader"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<1>"), TEXT("TFilterPixelShader<1>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<2>"), TEXT("TFilterPixelShader<2>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<3>"), TEXT("TFilterPixelShader<3>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<4>"), TEXT("TFilterPixelShader<4>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<5>"), TEXT("TFilterPixelShader<5>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<6>"), TEXT("TFilterPixelShader<6>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<7>"), TEXT("TFilterPixelShader<7>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<8>"), TEXT("TFilterPixelShader<8>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<9>"), TEXT("TFilterPixelShader<9>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<10>"), TEXT("TFilterPixelShader<10>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<11>"), TEXT("TFilterPixelShader<11>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<12>"), TEXT("TFilterPixelShader<12>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<13>"), TEXT("TFilterPixelShader<13>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<14>"), TEXT("TFilterPixelShader<14>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<15>"), TEXT("TFilterPixelShader<15>"));
	ShaderTypePairs.Add(TEXT("TFilterVertexShader<16>"), TEXT("TFilterPixelShader<16>"));
	ShaderTypePairs.Add(TEXT("FApplyLightShaftsVertexShader"), TEXT("FApplyLightShaftsPixelShader"));
	ShaderTypePairs.Add(TEXT("FSimpleF32VertexShader"), TEXT("FSimpleF32PixelShader"));
	ShaderTypePairs.Add(TEXT("FLightFunctionVertexShader"), TEXT("FLightFunctionPixelShader"));
	ShaderTypePairs.Add(TEXT("FHitMaskVertexShader"), TEXT("FHitMaskPixelShader"));
	ShaderTypePairs.Add(TEXT("FFogVolumeApplyVertexShader"), TEXT("FFogVolumeApplyPixelShader"));
	ShaderTypePairs.Add(TEXT("TFogIntegralVertexShader<FConeDensityPolicy>"), TEXT("TFogIntegralPixelShader<FConeDensityPolicy>"));
	ShaderTypePairs.Add(TEXT("TFogIntegralVertexShader<FSphereDensityPolicy>"), TEXT("FSimpleF32PixelShader"));
	ShaderTypePairs.Add(TEXT("TFogIntegralPixelShader<FSphereDensityPolicy>"), TEXT("TFogIntegralPixelShader<FSphereDensityPolicy>"));
	ShaderTypePairs.Add(TEXT("TFogIntegralVertexShader<FLinearHalfspaceDensityPolicy>"), TEXT("TFogIntegralPixelShader<FLinearHalfspaceDensityPolicy>"));
	ShaderTypePairs.Add(TEXT("TFogIntegralVertexShader<FConstantDensityPolicy>"), TEXT("TFogIntegralPixelShader<FConstantDensityPolicy>"));
	ShaderTypePairs.Add(TEXT("FDownsampleLightShaftsVertexShader"), TEXT("TDownsampleLightShaftsPixelShader<LS_Directional>"));
	ShaderTypePairs.Add(TEXT("FDownsampleLightShaftsVertexShader"), TEXT("TDownsampleLightShaftsPixelShader<LS_Spot>"));
	ShaderTypePairs.Add(TEXT("FDownsampleLightShaftsVertexShader"), TEXT("TDownsampleLightShaftsPixelShader<LS_Point>"));
	ShaderTypePairs.Add(TEXT("TDepthOnlyVertexShader<1>"), TEXT("TDepthOnlySolidPixelShader"));
	ShaderTypePairs.Add(TEXT("TDepthOnlyVertexShader<1>"), TEXT("TDepthOnlyScreenDoorPixelShader"));
	ShaderTypePairs.Add(TEXT("TDepthOnlyVertexShader<0>"), TEXT("TDepthOnlySolidPixelShader"));
	ShaderTypePairs.Add(TEXT("TDepthOnlyVertexShader<0>"), TEXT("TDepthOnlyScreenDoorPixelShader"));
	ShaderTypePairs.Add(TEXT("FSimpleElementVertexShader"), TEXT("FSimpleElementColorChannelMaskPixelShader"));
	ShaderTypePairs.Add(TEXT("FSimpleElementVertexShader"), TEXT("FSimpleElementHitProxyPixelShader"));
	ShaderTypePairs.Add(TEXT("FSimpleElementVertexShader"), TEXT("FSimpleElementDistanceFieldGammaPixelShader"));
	ShaderTypePairs.Add(TEXT("FSimpleElementVertexShader"), TEXT("FSimpleElementMaskedGammaPixelShader"));
	ShaderTypePairs.Add(TEXT("FSimpleElementVertexShader"), TEXT("FSimpleElementGammaPixelShader"));
	ShaderTypePairs.Add(TEXT("FSimpleElementVertexShader"), TEXT("FSimpleElementPixelShader"));
	ShaderTypePairs.Add(TEXT("FEdgePreservingFilterVertexShader"), TEXT("TEdgePreservingFilterPixelShader"));
	ShaderTypePairs.Add(TEXT("FHistoryUpdateVertexShader"), TEXT("FStaticHistoryUpdatePixelShader"));
	ShaderTypePairs.Add(TEXT("FAmbientOcclusionVertexShader"), TEXT("TAmbientOcclusionPixelShaderFLowQualityAOFALSETRUE"));
	ShaderTypePairs.Add(TEXT("FAmbientOcclusionVertexShader"), TEXT("TAmbientOcclusionPixelShaderFLowQualityAOTRUETRUE"));
	ShaderTypePairs.Add(TEXT("FAmbientOcclusionVertexShader"), TEXT("TAmbientOcclusionPixelShaderFLowQualityAOFALSEFALSE"));
	ShaderTypePairs.Add(TEXT("FAmbientOcclusionVertexShader"), TEXT("TAmbientOcclusionPixelShaderFLowQualityAOTRUEFALSE"));
	ShaderTypePairs.Add(TEXT("FAmbientOcclusionVertexShader"), TEXT("TAmbientOcclusionPixelShaderFDefaultQualityAOFALSETRUE"));
	ShaderTypePairs.Add(TEXT("FAmbientOcclusionVertexShader"), TEXT("TAmbientOcclusionPixelShaderFDefaultQualityAOTRUETRUE"));
	ShaderTypePairs.Add(TEXT("FAmbientOcclusionVertexShader"), TEXT("TAmbientOcclusionPixelShaderFDefaultQualityAOFALSEFALSE"));
	ShaderTypePairs.Add(TEXT("FAmbientOcclusionVertexShader"), TEXT("TAmbientOcclusionPixelShaderFDefaultQualityAOTRUEFALSE"));
	ShaderTypePairs.Add(TEXT("FDownsampleDepthVertexShader"), TEXT("TDownsampleDepthPixelShader"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11211"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11210"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11201"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11200"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader11000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10211"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10210"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10201"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10200"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader10000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01211"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01210"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01201"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01200"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader01000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00211"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00210"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00201"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00200"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("HalfResPixelShader00000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader22111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader22110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader22101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader22100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader22011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader22010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader22001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader22000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader21111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader21110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader21101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader21100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader21011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader21010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader21001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader21000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader20111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader20110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader20101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader20100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader20011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader20010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader20001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader20000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader12111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader12110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader12101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader12100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader12011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader12010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader12001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader12000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader11111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader11110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader11101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader11100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader11011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader11010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader11001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader11000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader10111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader10110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader10101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader10100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader10011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader10010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader10001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader10000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader02111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader02110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader02101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader02100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader02011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader02010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader02001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader02000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader01111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader01110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader01101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader01100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader01011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader01010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader01001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader01000"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader00111"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader00110"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader00101"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader00100"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader00011"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader00010"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader00001"));
	ShaderTypePairs.Add(TEXT("FUberPostProcessVertexShader"), TEXT("PostProcessBlendPixelShader00000"));
	ShaderTypePairs.Add(TEXT("FBokehDOFVertexShader"), TEXT("FBokehDOFPixelShader"));
	ShaderTypePairs.Add(TEXT("FDistortionApplyScreenVertexShader"), TEXT("FDistortionApplyScreenPixelShader"));
	ShaderTypePairs.Add(TEXT("TDistortionMeshVertexShader<FDistortMeshAccumulatePolicy>"), TEXT("TDistortionMeshPixelShader<FDistortMeshAccumulatePolicy>"));
	ShaderTypePairs.Add(TEXT("FDOFAndBloomBlendVertexShader"), TEXT("FDOFAndBloomBlendPixelShader"));
	ShaderTypePairs.Add(TEXT("TDOFAndBloomGatherVertexShader<NumFPFilterSamples>"), TEXT("TMotionBlurGatherPixelShader<NumFPFilterSamples>"));
	ShaderTypePairs.Add(TEXT("TDOFAndBloomGatherVertexShader<NumFPFilterSamples>"), TEXT("TBloomGatherPixelShader<NumFPFilterSamples>"));
	ShaderTypePairs.Add(TEXT("TDOFAndBloomGatherVertexShader<NumFPFilterSamples>"), TEXT("TDOFGatherPixelShader1"));
	ShaderTypePairs.Add(TEXT("TDOFAndBloomGatherVertexShader<NumFPFilterSamples>"), TEXT("TDOFGatherPixelShader0"));
	ShaderTypePairs.Add(TEXT("FOneColorVertexShader"), TEXT("FOneColorPixelShader"));
	ShaderTypePairs.Add(TEXT("FSplashVertexShader"), TEXT("FSplashPixelShader"));
	ShaderTypePairs.Add(TEXT("FVertexShaderNGP"), TEXT("FPixelShaderNGP"));
	ShaderTypePairs.Add(TEXT("FReflectionMaskVertexShader"), TEXT("FReflectionMaskPixelShader"));
	ShaderTypePairs.Add(TEXT("FLUTBlenderVertexShader"), TEXT("FLUTBlenderPixelShader<5>"));
	ShaderTypePairs.Add(TEXT("FLUTBlenderVertexShader"), TEXT("FLUTBlenderPixelShader<4>"));
	ShaderTypePairs.Add(TEXT("FLUTBlenderVertexShader"), TEXT("FLUTBlenderPixelShader<3>"));
	ShaderTypePairs.Add(TEXT("FLUTBlenderVertexShader"), TEXT("FLUTBlenderPixelShader<2>"));
	ShaderTypePairs.Add(TEXT("FLUTBlenderVertexShader"), TEXT("FLUTBlenderPixelShader<1>"));
	ShaderTypePairs.Add(TEXT("FRadialBlurVertexShader"), TEXT("FRadialBlurPixelShader"));
	ShaderTypePairs.Add(TEXT("FRadialBlurVertexShader"), TEXT("FRadialBlurVelocityPixelShader"));
	ShaderTypePairs.Add(TEXT("FScreenVertexShader"), TEXT("FScreenPixelShader"));
	ShaderTypePairs.Add(TEXT("FRestoreColorAndDepthVertexShader"), TEXT("FRestoreDownsamplingColorAndDepthPixelShader"));
	ShaderTypePairs.Add(TEXT("FRestoreColorAndDepthVertexShader"), TEXT("FRestoreColorAndDepthPixelShader"));
	ShaderTypePairs.Add(TEXT("FRestoreColorAndDepthVertexShader"), TEXT("FRestoreDownsamplingDepthOnlyPixelShader"));
	ShaderTypePairs.Add(TEXT("FRestoreColorAndDepthVertexShader"), TEXT("FRestoreDepthOnlyPixelShader"));
	ShaderTypePairs.Add(TEXT("FImageReflectionVertexShader"), TEXT("FImageReflectionPerSamplePixelShader"));
	ShaderTypePairs.Add(TEXT("FImageReflectionVertexShader"), TEXT("TImageReflectionPixelShader<FALSE>"));
	ShaderTypePairs.Add(TEXT("FImageReflectionVertexShader"), TEXT("TImageReflectionPixelShader<TRUE>"));
	ShaderTypePairs.Add(TEXT("FImageReflectionVertexShader"), TEXT("TReflectionStaticShadowingPixelShader<FALSE>"));
	ShaderTypePairs.Add(TEXT("FImageReflectionVertexShader"), TEXT("TReflectionStaticShadowingPixelShader<TRUE>"));
	ShaderTypePairs.Add(TEXT("TLightMapDensityVertexShader<FDummyLightMapTexturePolicy>"), TEXT("TLightMapDensityPixelShader<FDummyLightMapTexturePolicy>"));
	ShaderTypePairs.Add(TEXT("TLightMapDensityVertexShader<FSimpleLightMapTexturePolicy>"), TEXT("TLightMapDensityPixelShader<FSimpleLightMapTexturePolicy>"));
	ShaderTypePairs.Add(TEXT("TLightMapDensityVertexShader<FDirectionalLightMapTexturePolicy>"), TEXT("TLightMapDensityPixelShader<FDirectionalLightMapTexturePolicy>"));
	ShaderTypePairs.Add(TEXT("TLightMapDensityVertexShader<FNoLightMapPolicy>"), TEXT("TLightMapDensityPixelShader<FNoLightMapPolicy>"));

	ShaderTypePairs.Add(TEXT("TLightVertexShaderFPointLightPolicyFNoStaticShadowingPolicy"), TEXT("TLightPixelShaderFPointLightPolicyFNoStaticShadowingPolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFPointLightPolicyFShadowTexturePolicy"), TEXT("TLightPixelShaderFPointLightPolicyFShadowTexturePolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFPointLightPolicyFSignedDistanceFieldShadowTexturePolicy"), TEXT("TLightPixelShaderFPointLightPolicyFSignedDistanceFieldShadowTexturePolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFPointLightPolicyFShadowVertexBufferPolicy"), TEXT("TLightPixelShaderFPointLightPolicyFShadowVertexBufferPolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFDirectionalLightPolicyFNoStaticShadowingPolicy"), TEXT("TLightPixelShaderFDirectionalLightPolicyFNoStaticShadowingPolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFDirectionalLightPolicyFShadowTexturePolicy"), TEXT("TLightPixelShaderFDirectionalLightPolicyFShadowTexturePolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFDirectionalLightPolicyFSignedDistanceFieldShadowTexturePolicy"), TEXT("TLightPixelShaderFDirectionalLightPolicyFSignedDistanceFieldShadowTexturePolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFDirectionalLightPolicyFShadowVertexBufferPolicy"), TEXT("TLightPixelShaderFDirectionalLightPolicyFShadowVertexBufferPolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFSphericalHarmonicLightPolicyFNoStaticShadowingPolicy"), TEXT("TLightPixelShaderFSphericalHarmonicLightPolicyFNoStaticShadowingPolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFSphericalHarmonicLightPolicyFShadowTexturePolicy"), TEXT("TLightPixelShaderFSphericalHarmonicLightPolicyFShadowTexturePolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFSphericalHarmonicLightPolicyFSignedDistanceFieldShadowTexturePolicy"), TEXT("TLightPixelShaderFSphericalHarmonicLightPolicyFSignedDistanceFieldShadowTexturePolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFSphericalHarmonicLightPolicyFShadowVertexBufferPolicy"), TEXT("TLightPixelShaderFSphericalHarmonicLightPolicyFShadowVertexBufferPolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFSpotLightPolicyFNoStaticShadowingPolicy"), TEXT("TLightPixelShaderFSpotLightPolicyFNoStaticShadowingPolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFSpotLightPolicyFShadowTexturePolicy"), TEXT("TLightPixelShaderFSpotLightPolicyFShadowTexturePolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFSpotLightPolicyFSignedDistanceFieldShadowTexturePolicy"), TEXT("TLightPixelShaderFSpotLightPolicyFSignedDistanceFieldShadowTexturePolicy"));
	ShaderTypePairs.Add(TEXT("TLightVertexShaderFSpotLightPolicyFShadowVertexBufferPolicy"), TEXT("TLightPixelShaderFSpotLightPolicyFShadowVertexBufferPolicy"));

	ShaderTypePairs.Add(TEXT("FSubsurfaceScatteringVertexShader"), TEXT("FSubsurfaceScatteringPixelShader9"));
	ShaderTypePairs.Add(TEXT("FSubsurfaceScatteringVertexShader"), TEXT("FPerFragmentSubsurfaceScatteringPixelShader9"));
	ShaderTypePairs.Add(TEXT("FSubsurfaceScatteringVertexShader"), TEXT("FPerPixelSubsurfaceScatteringPixelShader9"));
	ShaderTypePairs.Add(TEXT("FTemporalAAVertexShader"), TEXT("FTemporalAAPixelShader"));
	ShaderTypePairs.Add(TEXT("FTemporalAAMaskVertexShader"), TEXT("FTemporalAAMaskExpandPixelShader"));
	ShaderTypePairs.Add(TEXT("FTemporalAAMaskVertexShader"), TEXT("FTemporalAAMaskSetupPixelShader"));
	ShaderTypePairs.Add(TEXT("FTemporalAAMaskVertexShader"), TEXT("FTemporalAAMaskPixelShader"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<FALSE>"), TEXT("TDeferredLightPerSamplePixelShaderFALSEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<FALSE>"), TEXT("TDeferredLightPerSamplePixelShaderTRUEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<FALSE>"), TEXT("TDeferredLightPerSamplePixelShaderTRUETRUE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<FALSE>"), TEXT("TDeferredLightPixelShaderFALSEFALSEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<FALSE>"), TEXT("TDeferredLightPixelShaderFALSETRUEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<FALSE>"), TEXT("TDeferredLightPixelShaderFALSETRUETRUE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<FALSE>"), TEXT("TDeferredLightPixelShaderTRUEFALSEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<FALSE>"), TEXT("TDeferredLightPixelShaderTRUETRUEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<FALSE>"), TEXT("TDeferredLightPixelShaderTRUETRUETRUE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<TRUE>"), TEXT("TDeferredLightPerSamplePixelShaderFALSEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<TRUE>"), TEXT("TDeferredLightPerSamplePixelShaderTRUEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<TRUE>"), TEXT("TDeferredLightPerSamplePixelShaderTRUETRUE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<TRUE>"), TEXT("TDeferredLightPixelShaderFALSEFALSEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<TRUE>"), TEXT("TDeferredLightPixelShaderFALSETRUEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<TRUE>"), TEXT("TDeferredLightPixelShaderFALSETRUETRUE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<TRUE>"), TEXT("TDeferredLightPixelShaderTRUEFALSEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<TRUE>"), TEXT("TDeferredLightPixelShaderTRUETRUEFALSE"));
	ShaderTypePairs.Add(TEXT("TDeferredLightVertexShader<TRUE>"), TEXT("TDeferredLightPixelShaderTRUETRUETRUE"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalLightMapTexturePolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFShadowedDynamicLightDirectionalVertexLightMapPolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightLightMapPolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightAndMultiTypeLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightAndMultiTypeLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightAndMultiTypeLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightAndMultiTypeLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightAndMultiTypeLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightAndMultiTypeLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightAndMultiTypeLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightAndMultiTypeLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightAndMultiTypeLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightAndMultiTypeLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightAndMultiTypeLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightAndMultiTypeLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightAndMultiTypeLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightAndMultiTypeLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightAndMultiTypeLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightAndMultiTypeLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightAndMultiTypeLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightAndMultiTypeLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSHLightAndMultiTypeLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFSHLightAndMultiTypeLightMapPolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFDynamicallyShadowedMultiTypeLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFDynamicallyShadowedMultiTypeLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFDynamicallyShadowedMultiTypeLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFDynamicallyShadowedMultiTypeLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFDynamicallyShadowedMultiTypeLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFDynamicallyShadowedMultiTypeLightLightMapPolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleLightMapTexturePolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleLightMapTexturePolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleLightMapTexturePolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleLightMapTexturePolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleLightMapTexturePolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleLightMapTexturePolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleLightMapTexturePolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleLightMapTexturePolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleLightMapTexturePolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleLightMapTexturePolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleLightMapTexturePolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightMapTexturePolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightMapTexturePolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightMapTexturePolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightMapTexturePolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightMapTexturePolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightMapTexturePolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightMapTexturePolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightMapTexturePolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightMapTexturePolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalLightMapTexturePolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleVertexLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleVertexLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleVertexLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleVertexLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleVertexLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleVertexLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleVertexLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleVertexLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleVertexLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFSimpleVertexLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFSimpleVertexLightMapPolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalVertexLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalVertexLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalVertexLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalVertexLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalVertexLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalVertexLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalVertexLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalVertexLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalVertexLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFDirectionalVertexLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFNoLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFNoLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFNoLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFNoLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFNoLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFNoLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFNoLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFNoLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFNoLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFNoLightMapPolicySkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFNoLightMapPolicyFConeDensityPolicy"), TEXT("TBasePassPixelShaderFNoLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFNoLightMapPolicyFSphereDensityPolicy"), TEXT("TBasePassPixelShaderFNoLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFNoLightMapPolicyFLinearHalfspaceDensityPolicy"), TEXT("TBasePassPixelShaderFNoLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFNoLightMapPolicyFConstantDensityPolicy"), TEXT("TBasePassPixelShaderFNoLightMapPolicyNoSkyLight"));
	ShaderTypePairs.Add(TEXT("TBasePassVertexShaderFNoLightMapPolicyFNoDensityPolicy"), TEXT("TBasePassPixelShaderFNoLightMapPolicyNoSkyLight"));

	ShaderTypePairs.Add(TEXT("TShadowDepthVertexShaderVertexShadowDepth_PerspectiveCorrect"), TEXT("TShadowDepthPixelShaderPixelShadowDepth_PerspectiveCorrectFALSE"));
	ShaderTypePairs.Add(TEXT("TShadowDepthVertexShaderVertexShadowDepth_PerspectiveCorrect"), TEXT("TShadowDepthPixelShaderPixelShadowDepth_NonPerspectiveCorrectFALSE"));
	ShaderTypePairs.Add(TEXT("TShadowDepthVertexShaderVertexShadowDepth_OutputDepthToColor"), TEXT("TShadowDepthPixelShaderPixelShadowDepth_PerspectiveCorrectFALSE"));
	ShaderTypePairs.Add(TEXT("TShadowDepthVertexShaderVertexShadowDepth_OutputDepthToColor"), TEXT("TShadowDepthPixelShaderPixelShadowDepth_NonPerspectiveCorrectFALSE"));
	ShaderTypePairs.Add(TEXT("TShadowDepthVertexShaderVertexShadowDepth_OutputDepth"), TEXT("TShadowDepthPixelShaderPixelShadowDepth_PerspectiveCorrectFALSE"));
	ShaderTypePairs.Add(TEXT("TShadowDepthVertexShaderVertexShadowDepth_OutputDepth"), TEXT("TShadowDepthPixelShaderPixelShadowDepth_NonPerspectiveCorrectFALSE"));
}

/**
* Key used to map a set of unique vertex/pixel shader CRC combinations to
* a flag which tells if given pair is already cached.
*/
class FShaderPairsCacheKey
{
public:

	FShaderPairsCacheKey(
		DWORD InVertexShaderCrc,
		DWORD InPixelShaderCrc
		)
		:	VertexShaderCrc(InVertexShaderCrc)
		,	PixelShaderCrc(InPixelShaderCrc)
	{
	}

	/**
	* Equality is based on render and depth stencil targets 
	* @param Other - instance to compare against
	* @return TRUE if equal
	*/
	friend UBOOL operator ==(const FShaderPairsCacheKey& A,const FShaderPairsCacheKey& B)
	{
		return A.VertexShaderCrc == B.VertexShaderCrc && A.PixelShaderCrc == B.PixelShaderCrc;
	}

	/**
	* Get the hash for this type. 
	* @param Key - struct to hash
	* @return DWORD hash based on type
	*/
	friend DWORD GetTypeHash(const FShaderPairsCacheKey &Key)
	{
		return GetTypeHash(Key.VertexShaderCrc) ^ GetTypeHash(Key.PixelShaderCrc);
	}

	DWORD VertexShaderCrc;
	DWORD PixelShaderCrc;
};

typedef TMap<FShaderPairsCacheKey,UBOOL> FShaderPairsCache;

/** Lazily initialized cache singleton. */
static FShaderPairsCache& GetShaderPairsCache()
{
	static FShaderPairsCache ShaderPairsCache;
	return ShaderPairsCache;
}

void AddMaterialToOpenGLProgramCache(const FString &MaterialName, const FMaterialResource *MaterialResource)
{
	// Make sure the pairs map is initialized
	PrepareShaderTypePairsMap();

	TMap<FGuid,FShader*> Shaders;

	// We skip engine materials to keep the cache much smaller. The ones that are actually used in-game mostly create very early
	// so this doesn't impact gameplay performance.
	UBOOL bSkip = MaterialName.StartsWith(TEXT("EngineDebugMaterials")) || MaterialName.StartsWith(TEXT("EngineMaterials"));
	if (!bSkip)
	{
		MaterialResource->GetShaderMap()->GetShaderList(Shaders);
	}

	for(TMap<FGuid,FShader*>::TIterator ShaderIt(Shaders);ShaderIt;++ShaderIt)
	{
		FShader* CurrentShader = ShaderIt.Value();
		check(CurrentShader);
		if (CurrentShader->GetTarget().Platform == SP_PCOGL && CurrentShader->GetTarget().Frequency == SF_Vertex)
		{
			const TArray<BYTE> &VertexShaderCode = CurrentShader->GetCode();
			DWORD VertexShaderCodeCrc = appMemCrc(&VertexShaderCode(0), VertexShaderCode.Num());
			FString VertexShaderTypeName = CurrentShader->GetType()->GetName();

			TArray<FString> PixelShaderTypesToCache;
			ShaderTypePairs.MultiFind(VertexShaderTypeName, PixelShaderTypesToCache);

			if (PixelShaderTypesToCache.Num() == 0)
			{
				warnf(TEXT("Material %s: skipping pair for %s (%u)"), *MaterialName, *VertexShaderTypeName, VertexShaderCodeCrc);
				for(TMap<FGuid,FShader*>::TIterator ShaderIt2(Shaders);ShaderIt2;++ShaderIt2)
				{
					FShader* CurrentShader2 = ShaderIt2.Value();
					check(CurrentShader2);

					if (CurrentShader2->GetTarget().Platform == SP_PCOGL && CurrentShader2->GetTarget().Frequency == SF_Pixel)
					{
						UBOOL bAdd = FALSE;
						FString PixelShaderTypeName = CurrentShader2->GetType()->GetName();
						warnf(TEXT("Candidate: %s"), *PixelShaderTypeName);
					}
				}
				continue;
			}

			for(TMap<FGuid,FShader*>::TIterator ShaderIt2(Shaders);ShaderIt2;++ShaderIt2)
			{
				FShader* CurrentShader2 = ShaderIt2.Value();
				check(CurrentShader2);

				if (CurrentShader2->GetTarget().Platform == SP_PCOGL && CurrentShader2->GetTarget().Frequency == SF_Pixel)
				{
					UBOOL bAdd = FALSE;
					FString PixelShaderTypeName = CurrentShader2->GetType()->GetName();

					for (TArray<FString>::TConstIterator Iter(PixelShaderTypesToCache); Iter; ++Iter)
					{
						const FString& TypeToCache = *Iter;
						if (TypeToCache == PixelShaderTypeName)
						{
							bAdd = TRUE;
							break;
						}
					}

					if (bAdd)
					{
						const TArray<BYTE> &PixelShaderCode = CurrentShader2->GetCode();
						DWORD PixelShaderCodeCrc = appMemCrc(&PixelShaderCode(0), PixelShaderCode.Num());

						UBOOL bAlreadyCached = GetShaderPairsCache().HasKey(FShaderPairsCacheKey(VertexShaderCodeCrc, PixelShaderCodeCrc));
						if (!bAlreadyCached)
						{
							GetShaderPairsCache().Set(FShaderPairsCacheKey(VertexShaderCodeCrc, PixelShaderCodeCrc), FALSE);
							GIsProgramCacheUpToDate = FALSE;
						}
					}
				}
			}
		}
	}
}

void UpdateProgramCache()
{
	if (GIsProgramCacheUpToDate)
	{
		return;
	}

	TLookupMap<FShaderPairsCacheKey> Keys;
	GetShaderPairsCache().GetKeys(Keys);

	for (TLookupMap<FShaderPairsCacheKey>::TConstIterator KeyIt(Keys); KeyIt; ++KeyIt)
	{
		const FShaderPairsCacheKey &Key = KeyIt.Key();
		UBOOL bAlreadyCreated = GetShaderPairsCache().FindRef(Key);
		if (!bAlreadyCreated)
		{
			FVertexShaderRHIRef VertexShaderRHI = GLoadedVertexShaders.FindRef(Key.VertexShaderCrc);
			FPixelShaderRHIRef PixelShaderRHI = GLoadedPixelShaders.FindRef(Key.PixelShaderCrc);
			if (VertexShaderRHI && PixelShaderRHI)
			{
				static DWORD EmptyStrides[MaxVertexElementCount] = {0};
				GetGLSLProgram(VertexShaderRHI, PixelShaderRHI, EmptyStrides);
			}

			GetShaderPairsCache().Set(Key, TRUE);
		}
	}

	GIsProgramCacheUpToDate = TRUE;
}
