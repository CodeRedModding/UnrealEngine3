/*=============================================================================
	UnConsoleTools.h: Definition of platform agnostic helper functions
					 implemented in a separate DLL.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// make sure that 64-bit DLLs match up with UE3 alignment
// (we use 8 so the DLLs don't need to disable warning C4121)
#pragma pack(push, 8)

#ifndef TEXT
#define TEXT(x) L##x
#endif

// define the strings that are returned by the console .dlls so that the engine can refer to them without risk if 
// the platform-specific support class is ever changed to return a slightly different string
#ifndef CONSOLESUPPORT_NAME_PC
#define CONSOLESUPPORT_NAME_PC			TEXT("PC")
#endif

#ifndef CONSOLESUPPORT_NAME_360
#define CONSOLESUPPORT_NAME_360			TEXT("Xbox360")
#endif

#ifndef CONSOLESUPPORT_NAME_360_SHORT
#define CONSOLESUPPORT_NAME_360_SHORT	TEXT("360")
#endif

#ifndef CONSOLESUPPORT_NAME_PS3
#define CONSOLESUPPORT_NAME_PS3			TEXT("PS3")
#endif

#ifndef CONSOLESUPPORT_NAME_IPHONE
#define CONSOLESUPPORT_NAME_IPHONE		TEXT("IPhone")
#endif

#ifndef CONSOLESUPPORT_NAME_ANDROID
#define CONSOLESUPPORT_NAME_ANDROID		TEXT("Android")
#endif

#ifndef CONSOLESUPPORT_NAME_NGP
#define CONSOLESUPPORT_NAME_NGP			TEXT("NGP")
#endif

#ifndef CONSOLESUPPORT_NAME_MAC
#define CONSOLESUPPORT_NAME_MAC			TEXT("Mac")
#endif

#ifndef CONSOLESUPPORT_NAME_WIIU
#define CONSOLESUPPORT_NAME_WIIU		TEXT("WiiU")
#endif

#ifndef CONSOLESUPPORT_NAME_FLASH
#define CONSOLESUPPORT_NAME_FLASH	TEXT("Flash")
#endif

// this is only defined when being included by File.h in the UnrealEngine solution.
#ifndef SUPPORT_NAMES_ONLY

#ifndef __UNCONSOLETOOLS_H__
#define __UNCONSOLETOOLS_H__

#ifdef CONSOLETOOLS_EXPORTS
#define CONSOLETOOLS_API __declspec(dllexport)
#else
#define CONSOLETOOLS_API 
#endif

#define DEFAULT_SYMBOL_SERVER			TEXT( "\\\\epicgames.net\\root\\UE3\\Builder\\BuilderSymbols" )

/** 
 * A non UObject based structure used to pass data about a sound node wave around the 
 * engine and tools.
 */
struct FSoundQualityInfo
{
	INT				Quality;						// Quality value ranging from 1 [poor] to 100 [very good]
	INT				bForceRealTimeDecompression;	// Whether to favour size over decompression speed
	INT				bLoopingSound;				// Whether to do the additional processing required for looping sounds
	DWORD			NumChannels;					// Number of distinct audio channels
	DWORD			SampleRate;					// Number of PCM samples per second
	DWORD			SampleDataSize;				// Size of sample data in bytes
	FLOAT			Duration;					// The length of the sound in seconds
	TCHAR			Name[128];
};
								 
/**
 * Abstract sound cooker interface.
 */
class FConsoleSoundCooker
{
public:
	/**
	 * Constructor
	 */
	FConsoleSoundCooker( void )
	{
	}

	/**
	 * Virtual destructor
	 */
	virtual ~FConsoleSoundCooker( void )
	{
	}

	/**
	 * Cooks the source data for the platform and stores the cooked data internally.
	 *
	 * @param	SrcBuffer		Pointer to source buffer
	 * @param	QualityInfo		All the information the compressor needs to compress the audio
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	virtual bool Cook( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo ) = 0;
	
	/**
	 * Cooks upto 8 mono files into a multichannel file (eg. 5.1). The front left channel is required, the rest are optional.
	 *
	 * @param	SrcBuffers		Pointers to source buffers
	 * @param	QualityInfo		All the information the compressor needs to compress the audio
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	virtual bool CookSurround( const BYTE* SrcBuffers[8], FSoundQualityInfo* QualityInfo ) = 0;

	/**
	 * Returns the size of the cooked data in bytes.
	 *
	 * @return The size in bytes of the cooked data including any potential header information.
	 */
	virtual UINT GetCookedDataSize( void ) = 0;

	/**
	 * Copies the cooked ata into the passed in buffer of at least size GetCookedDataSize()
	 *
	 * @param CookedData		Buffer of at least GetCookedDataSize() bytes to copy data to.
	 */
	virtual void GetCookedData( BYTE* CookedData ) = 0;
	
	/** 
	 * Recompresses raw PCM to the the platform dependent format, and then back to PCM. Used for quality previewing.
	 *
	 * @param	SrcData			Uncompressed PCM data
	 * @param	DstData			Uncompressed PCM data after being compressed		
	 * @param	QualityInfo		All the information the compressor needs to compress the audio	
	 */
	virtual INT Recompress( const BYTE* SrcBuffer, BYTE* DestBuffer, FSoundQualityInfo* QualityInfo ) = 0;

	/**
	 * Queries for any warning or error messages resulting from the cooking phase
	 *
	 * @return Warning or error message string, or NULL if nothing to report
	 */
	virtual const wchar_t* GetCookErrorMessages( void ) const 
	{ 
		return( NULL ); 
	}
};

/**
 * Abstract 2D texture cooker interface.
 * 
 * Expected usage is:
 * Init(...)
 * for( MipLevels )
 * {
 *     Dst = appMalloc( GetMipSize( MipLevel ) );
 *     CookMip( ... )
 * }
 *
 * and repeat.
 */
struct CONSOLETOOLS_API FConsoleTextureCooker
{
	/**
	 * Virtual destructor
	 */
	virtual ~FConsoleTextureCooker()
	{
	}

	/**
	 * Associates texture parameters with cooker.
	 *
	 * @param UnrealFormat	Unreal pixel format
	 * @param Width			Width of texture (in pixels)
	 * @param Height		Height of texture (in pixels)
	 * @param NumMips		Number of miplevels
	 * @param CreateFlags	Platform-specific creation flags
	 */
	virtual void Init( DWORD UnrealFormat, UINT Width, UINT Height, UINT NumMips, DWORD CreateFlags ) = 0;

	/**
	 * Returns the platform specific size of a miplevel.
	 *
	 * @param	Level		Miplevel to query size for
	 * @return	Returns	the size in bytes of Miplevel 'Level'
	 */
	virtual UINT GetMipSize( UINT Level ) = 0;

	/**
	 * Cooks the specified miplevel, and puts the result in Dst which is assumed to
	 * be of at least GetMipSize size.
	 *
	 * @param Level			Miplevel to cook
	 * @param Src			Src pointer
	 * @param Dst			Dst pointer, needs to be of at least GetMipSize size
	 * @param SrcRowPitch	Row pitch of source data
	 */
	virtual void CookMip( UINT Level, void* Src, void* Dst, UINT SrcRowPitch ) = 0;

	/**
	* Returns the index of the first mip level that resides in the packed miptail
	* 
	* @return index of first level of miptail 
	*/
	virtual INT GetMipTailBase() = 0;

	/**
	* Cooks all mip levels that reside in the packed miptail. Dst is assumed to 
	* be of size GetMipSize (size of the miptail level).
	*
	* @param Src - ptrs to mip data for each source mip level
	* @param SrcRowPitch - array of row pitch entries for each source mip level
	* @param Dst - ptr to miptail destination
	*/
	virtual void CookMipTail( void** Src, UINT* SrcRowPitch, void* Dst ) = 0;

	/**
	 *  Gets the platform-specific size required for the given texture.
	 *
	 *	@param	UnrealFormat	Unreal pixel format
	 *	@param	Width			Width of texture (in pixels)
	 *	@param	Height			Height of texture (in pixels)
	 *	@param	NumMips			Number of miplevels
	 *	@param	CreateFlags		Platform-specific creation flags
	 *
	 *	@return	INT				The size of the memory allocation needed for the texture.
	 */
	virtual INT GetPlatformTextureSize(DWORD /*UnrealFormat*/, UINT /*Width*/, UINT /*Height*/, UINT /*NumMips*/, DWORD /*CreateFlags*/)
	{
		return 0;
	}

	/**
	 *  Gets the platform-specific size required for the given cubemap texture.
	 *
	 *	@param	UnrealFormat	Unreal pixel format
	 *	@param	Size			Size of the cube edge (in pixels)
	 *	@param	NumMips			Number of miplevels
	 *	@param	CreateFlags		Platform-specific creation flags
	 *
	 *	@return	INT				The size of the memory allocation needed for the texture.
	 */
	virtual INT GetPlatformCubeTextureSize(DWORD /*UnrealFormat*/, UINT /*Size*/, UINT /*NumMips*/, DWORD /*CreateFlags*/)
	{
		return 0;
	}
};

/** The information about a mesh fragment that is needed to cook it. */
struct FMeshFragmentCookInfo
{
	// Input buffers
	const WORD* Indices; /** Input index buffer, formatted as a triangle list (not strips!) */
	UINT NumTriangles; /** Number of triangles (groups of three indices) in the Indices array */
	bool bEnableEdgeGeometrySupport; /** If true, additional processing will be performed to support Playstation EDGE Geometry processing at runtime (pre-vertex shader culling) */

	bool bUseFullPrecisionUVs; /** If true, the mesh uses full 32-bit texture coordinates.  If false, assume 16-bit UVs. */
	const float* PositionVertices; /** position only -- used only to compute triangle centroids */
	UINT MinVertexIndex; /** minimum index referenced in the Indices array */
	UINT MaxVertexIndex; /** ...maximum index referenced in the Indices array */
	UINT NewMinVertexIndex; /** The triangles generated by the cooker should be biased so that index 0 corresponds to this value. */
};

struct FMeshFragmentCookOutputInfo
{
	// Sizes reserved for various output buffers. Must be sufficiently large!
	UINT NumVerticesReserved; /** Rule of thumb: 1.10 * NumVertices */
	UINT NumTrianglesReserved; /** Rule of thumb: 1.05 * NumTriangles */
	UINT NumPartitionsReserved; /** Rule of thumb: NumTriangles / 100 */

	// output buffers
	WORD* NewIndices;
	UINT NewNumTriangles;
	// VertexRemapTable[N] will contain the index of the vertex in the old vertex buffer that should be the Nth
	// vertex in the new vertex buffer.  The table will have NewNumVertices entries.
	INT* VertexRemapTable;
	UINT NewNumVertices;

	/**
	 * On the PS3, libedgegeom partitions each mesh fragment into a number of SPU-sized partitions,
	 * each of which will be processed by a single SPURS job when it is rendered.  The per-partition data
	 * is stored here.
	 */
	UINT NumPartitions; /** Number of entries in the following arrays */
	UINT* PartitionIoBufferSize; /** The SPURS I/O buffer size to use for each partition's job. */
	UINT* PartitionScratchBufferSize; /** The SPURS scratch buffer size to use for each partition's job. */
	WORD* PartitionCommandBufferHoleSize; /** Size (in bytes) to reserve in the GCM command buffer for each partition. */
	short* PartitionIndexBias; /** A negative number for each partition, equal to the minimum value referenced in the partition's index buffer. */
	WORD* PartitionNumVertices; /** Number of vertices included in each partition. */
	WORD* PartitionNumTriangles; /** Number of triangles included in each partition. */
	WORD* PartitionFirstVertex;
	UINT* PartitionFirstTriangle;
};

/** The information about a mesh fragment that is needed to cook it. */
struct FSkeletalMeshFragmentCookInfo
{
	// Input buffers
	const DWORD* Indices; /** Input index buffer, formatted as a triangle list (not strips!) */
	UINT NumTriangles; /** Number of triangles (groups of three indices) in the Indices array */
	bool bEnableEdgeGeometrySupport; /** If true, additional processing will be performed to support Playstation EDGE Geometry processing at runtime (pre-vertex shader culling) */

	bool bUseFullPrecisionUVs; /** If true, the mesh uses full 32-bit texture coordinates.  If false, assume 16-bit UVs. */
	const float* PositionVertices; /** position only -- used only to compute triangle centroids */
	UINT MinVertexIndex; /** minimum index referenced in the Indices array */
	UINT MaxVertexIndex; /** ...maximum index referenced in the Indices array */
	UINT NewMinVertexIndex; /** The triangles generated by the cooker should be biased so that index 0 corresponds to this value. */
};

struct FSkeletalMeshFragmentCookOutputInfo
{
	// Sizes reserved for various output buffers. Must be sufficiently large!
	UINT NumVerticesReserved; /** Rule of thumb: 1.10 * NumVertices */
	UINT NumTrianglesReserved; /** Rule of thumb: 1.05 * NumTriangles */
	UINT NumPartitionsReserved; /** Rule of thumb: NumTriangles / 100 */

	// output buffers
	DWORD* NewIndices;
	UINT NewNumTriangles;
	// VertexRemapTable[N] will contain the index of the vertex in the old vertex buffer that should be the Nth
	// vertex in the new vertex buffer.  The table will have NewNumVertices entries.
	INT* VertexRemapTable;
	UINT NewNumVertices;

	/**
	 * On the PS3, libedgegeom partitions each mesh fragment into a number of SPU-sized partitions,
	 * each of which will be processed by a single SPURS job when it is rendered.  The per-partition data
	 * is stored here.
	 */
	UINT NumPartitions; /** Number of entries in the following arrays */
	UINT* PartitionIoBufferSize; /** The SPURS I/O buffer size to use for each partition's job. */
	UINT* PartitionScratchBufferSize; /** The SPURS scratch buffer size to use for each partition's job. */
	WORD* PartitionCommandBufferHoleSize; /** Size (in bytes) to reserve in the GCM command buffer for each partition. */
	short* PartitionIndexBias; /** A negative number for each partition, equal to the minimum value referenced in the partition's index buffer. */
	DWORD* PartitionNumVertices; /** Number of vertices included in each partition. */
	WORD* PartitionNumTriangles; /** Number of triangles included in each partition. */
	DWORD* PartitionFirstVertex;
	UINT* PartitionFirstTriangle;
};

/**
 * Abstract skeletal mesh cooker interface.
 * 
 * Expected usage is:
 * Init(...)
 * CookMeshElement( ... )
 *
 * and repeat.
 */
struct CONSOLETOOLS_API FConsoleSkeletalMeshCooker
{
	/**
	 * Virtual destructor
	 */
	virtual ~FConsoleSkeletalMeshCooker()
	{
	}

	virtual void Init( void ) = 0;

	/**
	 * Cooks a mesh element.
	 * @param ElementInfo - Information about the element being cooked
	 * @param OutInfo - Upon return, contains the optimized mesh data
	 */
	virtual void CookMeshElement(
		const FSkeletalMeshFragmentCookInfo& ElementInfo,
		FSkeletalMeshFragmentCookOutputInfo& OutInfo
		) = 0;
};

/**
 * Abstract static mesh cooker interface.
 * 
 * Expected usage is:
 * Init(...)
 * CookMeshElement( ... )
 *
 * and repeat.
 */
struct CONSOLETOOLS_API FConsoleStaticMeshCooker
{
	/**
	 * Virtual destructor
	 */
	virtual ~FConsoleStaticMeshCooker()
	{
	}

	virtual void Init( void ) = 0;

	/**
	 * Cooks a mesh element.
	 * @param ElementInfo - Information about the element being cooked
	 * @param OutInfo - Upon return, contains the optimized mesh data
	 */
	virtual void CookMeshElement(
		FMeshFragmentCookInfo& ElementInfo,
		FMeshFragmentCookOutputInfo& OutInfo
		) = 0;
};

/**
 * Base class to handle precompiling shaders offline in a per-platform manner
 */
class FConsoleShaderPrecompiler
{
public:
	/**
	 * Virtual destructor
	 */
	virtual ~FConsoleShaderPrecompiler()
	{
	}

	/**
	 * Precompile the shader with the given name. Must be implemented
	 *
	 * @param ShaderPath			Pathname to the source shader file ("..\Engine\Shaders\BasePassPixelShader.usf")
	 * @param EntryFunction			Name of the startup function ("pixelShader")
	 * @param bIsVertexShader		True if the vertex shader is being precompiled
	 * @param CompileFlags			Default is 0, otherwise members of the D3DXSHADER enumeration
	 * @param Definitions			Space separated string that contains shader defines ("FRAGMENT_SHADER=1 VERTEX_LIGHTING=0")
	 * @param bDumpingShaderPDBs	True if shader PDB's should be saved to ShaderPDBPath
	 * @param ShaderPDBPath			Path to save shader PDB's, can be on the local machine if not using runtime compiling.
	 * @param BytecodeBuffer		Block of memory to fill in with the compiled bytecode
	 * @param BytecodeSize			Size of the returned bytecode
	 * @param ConstantBuffer		String buffer to return the shader definitions with [Name,RegisterIndex,RegisterCount] ("WorldToLocal,100,4 Scale,101,1"), NULL = Error case
	 * @param Errors				String buffer any output/errors
	 * 
	 * @return true if successful
	 */
	virtual bool PrecompileShader(
		const char* ShaderPath, 
		const char* EntryFunction, 
		bool bIsVertexShader, 
		DWORD CompileFlags, 
		const char* Definitions, 
		const char* IncludeDirectory,
		char* const* IncludeFileNames,
		char* const* IncludeFileContents,
		int NumIncludes,
		bool bDumpingShaderPDBs,
		const char* ShaderPDBPath,
		unsigned char* BytecodeBufer, 
		int& BytecodeSize, 
		char* ConstantBuffer, 
		char* Errors
		) = 0;

	/**
	 * Preprocess the shader with the given name. Must be implemented
	 *
	 * @param ShaderPath		Pathname to the source shader file ("..\Engine\Shaders\BasePassPixelShader.usf")
	 * @param Definitions		Space separated string that contains shader defines ("FRAGMENT_SHADER=1 VERTEX_LIGHTING=0")
	 * @param ShaderText		Block of memory to fill in with the preprocessed shader output
	 * @param ShaderTextSize	Size of the returned text
	 * @param Errors			String buffer any output/errors
	 * 
	 * @return true if successful
	 */
	virtual bool PreprocessShader(
		const char* ShaderPath, 
		const char* Definitions, 
		const char* IncludeDirectory,
		char* const* IncludeFileNames,
		char* const* IncludeFileContents,
		int NumIncludes,
		unsigned char* ShaderText, 
		int& ShaderTextSize, 
		char* Errors) = 0;

	/**
	 * Disassemble the shader with the given byte code. Must be implemented
	 *
	 * @param ShaderByteCode	The null terminated shader byte code to be disassembled
	 * @param ShaderText		Block of memory to fill in with the preprocessed shader output
	 * @param ShaderTextSize	Size of the returned text
	 * 
	 * @return true if successful
	 */
	virtual bool DisassembleShader(const DWORD *ShaderByteCode, unsigned char* ShaderText, int& ShaderTextSize) = 0;

	/**
	 * Create a command line to compile the shader with the given parameters. Must be implemented
	 *
	 * @param ShaderPath		Pathname to the source shader file ("..\Engine\Shaders\BasePassPixelShader.usf")
	 * @param IncludePath		Pathname to extra include directory (can be NULL)
	 * @param EntryFunction		Name of the startup function ("Main") (can be NULL)
	 * @param bIsVertexShader	True if the vertex shader is being precompiled
	 * @param CompileFlags		Default is 0, otherwise members of the D3DXSHADER enumeration
	 * @param Definitions		Space separated string that contains shader defines ("FRAGMENT_SHADER=1 VERTEX_LIGHTING=0") (can be NULL)
	 * @param CommandStr		Block of memory to fill in with the null terminated command line string
	 * 
	 * @return true if successful
	 */
	virtual bool CreateShaderCompileCommandLine(const char* ShaderPath, const char* IncludePath, const char* EntryFunction, bool bIsVertexShader, DWORD CompileFlags, const char* Definitions, char* CommandStr, bool bPreprocessed) = 0;
};

/**
 * This is used to capture all of the module information needed to load pdb's.
 */
#ifndef QWORD
#if defined( __GNUC__ ) | NGP
typedef uint64_t			QWORD;		// 64-bit unsigned.
#else
typedef unsigned __int64	QWORD;		// 64-bit unsigned.
#endif
#endif

/**
 * Base class to encapsulate parsing of symbols
 *
 * This is currently an unused template as all symbol lookup is done through the much simpler managed interface
 */
class FConsoleSymbolParser
{
public:
	FConsoleSymbolParser()
	{
	}

	/**
	 * Virtual destructor
	 */
	virtual ~FConsoleSymbolParser()
	{
	}

	/**
	 * Loads symbols for an executable.
	 *
	 * @param	ExePath		The path to the executable whose symbols are going to be loaded.
	 * @param	bEnhanced	Whether to do a more detailed (but slower) symbol lookup
	 * @param	UserData		Arbitrary object for passing data to the symbol lookup (an array of loaded modules for the Xbox360)
	 * @param	Count		The count of objects passed in via UserData
	 */
	virtual bool LoadSymbols( const wchar_t* ExePath, const wchar_t* SearchPath, bool bEnhanced, void* UserData, int Count ) = 0;

	/**
	 * Unloads any currently loaded symbols.
	 */
	virtual void UnloadSymbols() = 0;

	/**
	 * Retrieves the symbol info for an address.
	 *
	 * @param	Address			The address to retrieve the symbol information for.
	 * @param	OutFileName		The file that the instruction at Address belongs to.
	 * @param	OutFunction		The name of the function that owns the instruction pointed to by Address.
	 * @param	OutLineNumber	The line number within OutFileName that is represented by Address.
	 * @return	True if the function succeeds.
	 */
	virtual bool ResolveAddressToSymboInfo( QWORD Address, wchar_t OutFileName[256], wchar_t OutFunction[256], int& OutLineNumber ) = 0;
};

#define MENU_SEPARATOR_LABEL L"--"

// NOTE: Also defined in Core.h for object propagation
#ifndef INVALID_TARGETHANDLE
typedef void* TARGETHANDLE;
#define INVALID_TARGETHANDLE ((TARGETHANDLE)-1)
#endif

// If we're compiling for the consoles, don't compile the delegate declarations as they are not needed and probably won't work
#if CONSOLE || PLATFORM_MACOSX
typedef void* CrashCallbackPtr;
typedef void* TTYEventCallbackPtr;
typedef void* ColoredTTYEventCallbackPtr;
typedef void* ProfilerEventCallbackPtr;
#else
typedef void (__stdcall *CrashCallbackPtr)(const wchar_t *GameName, const wchar_t *CallStack, const wchar_t *MiniDumpLocation);
typedef void (__stdcall *TTYEventCallbackPtr)(const wchar_t *Txt);
typedef void (__stdcall *ColoredTTYEventCallbackPtr)(DWORD Color, const wchar_t *Txt);
typedef void (__stdcall *ProfilerEventCallbackPtr)(const wchar_t *Txt);
#endif

/**
 * Base abstract class for a platform-specific support class. Every platform
 * will override the member functions to provide platform-speciific code.
 * This is so that access to platform-specific code is not given to all
 * licensees of the Unreal Engine. This is to protect the NDAs of the
 * console manufacturers from Unreal Licensees who don't have a license with
 * that manufacturer.
 *
 * The ConsoleSupport subclasses will be defined in DLLs, so Epic can control 
 * the code/proprietary information simply by controlling who gets what DLLs.
 */
class FConsoleSupport
{
public:
	enum EProfileType
	{
		PT_Invalid,
		PT_Script,
		PT_GameTick,
		PT_RenderTick,
		PT_Memory,
		PT_UE3Stats,
		PT_MemLeaks,
		PT_FPSCharts,
		PT_BugIt,
		PT_MiscFile,
		PT_Network,
	};

	enum ETargetState
	{
		TS_Unconnected = 0,
		TS_Connected,
		TS_Rebooting,
		TS_Running,
		TS_Crashed,
		TS_Asserted,
		TS_RIP,
	};

	enum ETargetType
	{
		TART_Unknown = 0,
		TART_Remote,
		TART_TestKit,
		TART_DevKit,
		TART_ReviewerKit,
		TART_IOS32,
		TART_IOS3x,
		TART_IOS40,
		TART_IOS41
	};

	/** Available platform types; mirror of UnFile.h */
	enum EPlatformType
	{
		EPlatformType_Unknown = 0x00,
		EPlatformType_Windows = 0x01,
		EPlatformType_WindowsServer = 0x02,
		EPlatformType_Xbox360 = 0x04,
		EPlatformType_PS3 = 0x08,
		EPlatformType_Linux = 0x10,
		EPlatformType_MacOSX = 0x20,
		EPlatformType_WindowsConsole = 0x40,
		EPlatformType_IPhone = 0x80,
		EPlatformType_NGP = 0x100,
		EPlatformType_Android = 0x200,
		EPlatformType_WiiU = 0x400,
		EPlatformType_Flash = 0x800,
		EPlatformType_Max
	};

	enum ECrashReportFilter
	{
		CRF_None = 0,
		CRF_Debug = 1,
		CRF_Release = 1 << 1,
		CRF_Shipping = 1 << 2,
		CRF_Test = 1 << 3,
		CRF_All = CRF_Debug | CRF_Release | CRF_Shipping | CRF_Test
	};

	enum EDumpType
	{
		DMPT_Normal = 0,
		DMPT_WithFullMemory,
	};

	/**
	 * Virtual destructor
	 */
	virtual ~FConsoleSupport()
	{
	}

	/** Initialize the DLL with some information about the game/editor
	 *
	 * @param	GameName		The name of the current game ("ExampleGame", "UDKGame", etc)
	 * @param	Configuration	The name of the configuration to run ("Debug", "Release", etc)
	 */
	virtual void Initialize(const wchar_t*, const wchar_t*) = 0;

	/**
	 * This function exists to delete an instance of FConsoleSupport that has been allocated from a *Tools.dll. Do not call this function from the destructor.
	 */
	virtual void Destroy()
	{
	}

	/**
	 * Return a void* (actually an HMODULE) to the loaded DLL instance
	 *
	 * @return	The HMODULE of the DLL
	 */
	virtual void* GetModule() = 0;

	/**
	 * Return a string name descriptor for this game (required to implement)
	 *
	 * @return	The name of the game
	 */
	virtual const wchar_t* GetGameName() = 0;

	/**
	 * Return a string name descriptor for this configuration (required to implement)
	 *
	 * @return	The name of the configuration
	 */
	virtual const wchar_t* GetConfiguration() = 0;

	/**
	 * Return a string name descriptor for this platform (required to implement)
	 *
	 * @return	The name of the platform
	 */
	virtual const wchar_t* GetPlatformName() = 0;

	/**
	 * Returns the platform of the specified target.
	 */
	virtual FConsoleSupport::EPlatformType GetPlatformType() = 0;

	/**
	 * @return true if this platform needs to have files copied from PC->target (required to implement)
	 */
	virtual bool PlatformNeedsToCopyFiles() = 0;
	
	/**
	 * Return whether or not this console Intel byte order (required to implement)
	 *
	 * @return	True if the console is Intel byte order
	 */
	virtual bool GetIntelByteOrder() = 0;
	
	/**
	 * Retrieves the handle of the default console.
	 */
	virtual TARGETHANDLE GetDefaultTarget() = 0;

	/**
	 * Starts the (potentially async) process of enumerating all available targets
	 */
	virtual void EnumerateAvailableTargets() = 0;

	/**
	 * Retrieves a handle to each available target.
	 *
	 * @param	OutTargetList	An array to copy all of the target handles into.
	 * @return	int				Number of targets retrieved	
	 */
	virtual int GetTargets(TARGETHANDLE*) = 0;

	/**
	 * Returns the type of the specified target.
	 */
	virtual ETargetType GetTargetType(TARGETHANDLE) = 0;

	/**
	 * Get the name of the specified target
	 *
	 * @param	Handle The handle of the console to retrieve the information from.
	 * @return Name of the target, or NULL if the Index is out of bounds
	 */
	virtual const wchar_t* GetTargetName(TARGETHANDLE) = 0;

	/**
	 * Gets the name of a console as displayed by the target manager.
	 *
	 * @param	Handle	The handle to the target to set the filter for.
	 */
	virtual const wchar_t* GetUnresolvedTargetName(TARGETHANDLE Handle)
	{
		return GetTargetName(Handle);
	}

	/**
	 * Return the default IP address to use when sending data into the game for object propagation
	 * Note that this is probably different than the IP address used to debug (reboot, run executable, etc)
	 * the console. (required to implement)
	 *
	 * @param Handle The handle of the console to retrieve the information from.
	 *
	 * @return	The in-game IP address of the console, in an Intel byte order 32 bit integer
	 */
	virtual unsigned int GetIPAddress(TARGETHANDLE) = 0;

	/**
	  * Retrieves the IP address for the debug channel at the specific target.
	 *
	  * @param	Handle	The handle to the target to retrieve the IP address for.
	 */
	virtual unsigned int GetDebugChannelIPAddress(TARGETHANDLE Handle)
	{
		return GetIPAddress(Handle);
	}

	/**
	 * Gets the dump type the current target is set to.
	 *
	 * @param	Handle	The handle to the target to get the dump type from.
	 */
	virtual EDumpType GetDumpType(TARGETHANDLE)
	{
		return DMPT_Normal;
	}

	/**
	 * Sets the dump type of the current target.
	 *
	 * @param	DmpType		The new dump type of the target.
	 * @param	Handle		The handle to the target to set the dump type for.
	 */
	virtual void SetDumpType(TARGETHANDLE, EDumpType) 
	{
	}

	/**
	 * Sets flags controlling how crash reports will be filtered.
	 * 
	 * @param	Handle	The handle to the target to set the filter for.
	 * @param	Filter	Flags controlling how crash reports will be filtered.
	 */
	virtual bool SetCrashReportFilter(TARGETHANDLE, ECrashReportFilter)
	{
		return true;
	}

	/**
	 * Sets the callback function for handling crashes.
	 *
	 * @param	Callback	Pointer to a callback function or NULL if handling crashes is to be disabled.
	 * @param	Handle		The handle to the target that will register the callback.
	 */
	virtual void SetCrashCallback(TARGETHANDLE Handle, CrashCallbackPtr Callback) = 0;

	/**
	 * Sets the callback function for TTY output.
	 * 
	 * @param	Callback	Pointer to a callback function or NULL if TTY output is to be disabled.
	 * @param	Handle		The handle to the target that will register the callback.
	 */
	virtual void SetTTYCallback(TARGETHANDLE Handle, TTYEventCallbackPtr Callback) = 0;

	/**
	 * Retrieve the state of the console (running, not running, crashed, etc)
	 *
	 * @param Handle The handle of the console to retrieve the information from.
	 * 
	 * @return the current state
	 */
	virtual ETargetState GetTargetState(TARGETHANDLE) = 0;

	/**
	 * Send a text command to the target
	 *
	 * @param Handle			The handle of the console to retrieve the information from.
	 * @param Command Command to send to the target
	 */
	virtual void SendConsoleCommand(TARGETHANDLE Handle, const wchar_t* Command) = 0;

	/**
	 * Allow for target to perform periodic operations
	 *
	 * @param Handle The handle of the console to heartbeat
	 */
	virtual void Heartbeat(TARGETHANDLE)
	{
	}
	
	/**
	 * Have the console take a screenshot and dump to a file
	 *
	 * @param Handle The handle of the console to retrieve the information from.
	 * @param Filename Location to place the .bmp file
	 *
	 * @return true if successful
	 */
	virtual bool ScreenshotBMP(TARGETHANDLE, const wchar_t*)
	{
		return false;
	}

	/**
	 * Open an internal connection to a target. 
	 *
	 * @param Handle The handle of the console to connect to.
	 *
	 * @return false for failure.
	 */
	virtual bool ConnectToTarget(TARGETHANDLE) = 0;

	/**
	 * Called after an atomic operation to close any open connections
	 *
	 * @param Handle The handle of the console to disconnect.
	 */
	virtual void DisconnectFromTarget(TARGETHANDLE) = 0;

	/**
	 * Exits the current instance of the game and reboots the target if appropriate. Must be implemented
	 *
	 * @param Handle The handle of the console to reset
	 * 
	 * @return true if successful
	 */
	virtual bool ResetTargetToShell(TARGETHANDLE Handle, bool bWaitForReset) = 0;

	/**
	 * Determines if the given file needs to be copied
	 * 
	 * @param Handle The handle of the console to retrieve the information from.
	 * @param SourceFilename	Path of the source file on PC
	 * @param DestFilename		Platform-independent destination filename (ie, no xe:\\ for Xbox, etc)
	 * @param bReverse			If true, then copying from platform (dest) to PC (src);
	 *
	 * @return true if successful
	 */
	virtual bool NeedsToCopyFile(TARGETHANDLE, const wchar_t*, const wchar_t*, bool)
	{
		return false;
	}

	/**
	 * Copies a single file from PC to target (and create the folder structure as required)
	 *
	 * @param Handle		The handle of the console to retrieve the information from.
	 * @param SourceFilename Path of the source file on PC
	 * @param DestFilename Platform-independent destination filename (ie, no xe:\\ for Xbox, etc)
	 *
	 * @return true if successful
	 */
	virtual bool CopyFileToTarget(TARGETHANDLE, const wchar_t*, const wchar_t*)
	{
		return false;
	}

	/**
	 *	Copies a single file from the target to the PC (and create the folder structure as required)
	 *
	 *  @param	Handle			The handle of the console to retrieve the information from.
	 *	@param	SourceFilename	Platform-independent source filename (ie, no xe:\\ for Xbox, etc)
	 *	@param	DestFilename	Path of the destination file on PC
	 *
	 *	@return	bool			true if successful, false otherwise
	 */
	virtual bool RetrieveFileFromTarget(TARGETHANDLE, const wchar_t*, const wchar_t*)
	{ 
		return false;
	}
	
	/**
	 *	Runs an instance of the selected game on the target console
	 *
	 *  @param	Handle					The handle of the console to retrieve the information from.
	 *	@param	GameName				The root game name (e.g. Gear)
	 *	@param	Configuration			The configuration to run (e.g. Release)
	 *	@param	URL						The command line to pass to the running instance
	 *	@param	BaseDirectory			The base directory to run from on the console
	 *
	 *	@return	bool					true if successful, false otherwise
	 */
	virtual bool RunGameOnTarget(TARGETHANDLE Handle, const wchar_t* GameName, const wchar_t* Configuration, const wchar_t* URL, const wchar_t* BaseDirectory) = 0;

	/**
	 * Returns the global sound cooker object.
	 *
	 * @return global sound cooker object, or NULL if none exists
	 */
	virtual FConsoleSoundCooker* GetGlobalSoundCooker( void ) = 0;

	/**
	 * Returns the global texture cooker object.
	 *
	 * @return global sound cooker object, or NULL if none exists
	 */
	virtual FConsoleTextureCooker* GetGlobalTextureCooker( void ) = 0;

	/**
	 * Returns the global skeletal mesh cooker object.
	 *
	 * @return global skeletal mesh cooker object, or NULL if none exists
	 */
	virtual FConsoleSkeletalMeshCooker* GetGlobalSkeletalMeshCooker( void ) = 0;

	/**
	 * Returns the global static mesh cooker object.
	 *
	 * @return global static mesh cooker object, or NULL if none exists
	 */
	virtual FConsoleStaticMeshCooker* GetGlobalStaticMeshCooker( void ) = 0;

	/**
	 * Returns the global shader precompiler object.
	 * @return global shader precompiler object, or NULL if none exists.
	 */
	virtual FConsoleShaderPrecompiler* GetGlobalShaderPrecompiler( void ) = 0;

	/**
	 * Returns the global shader precompiler object.
	 * @return global shader precompiler object, or NULL if none exists.
	 */
	virtual FConsoleSymbolParser* GetGlobalSymbolParser( void ) = 0;

	// *******************************************************************

	/**
	 * Run the game on the target console (required to implement)
	 *
	 * @param	TargetList				The list of handles of consoles to run the game on.
	 * @param	NumTargets				The number of handles in TargetList.
	 * @param	MapName					The name of the map that is going to be loaded.
	 * @param	URL						The map name and options to run the game with
	 * @param	OutputConsoleCommand	A buffer that the menu item can fill out with a console command to be run by the Editor on return from this function
	 * @param	CommandBufferSize		The size of the buffer pointed to by OutputConsoleCommand
	 *
	 * @return	Returns true if the run was successful
	 */
	virtual bool RunGame(TARGETHANDLE *TargetList, int NumTargets, const wchar_t* MapName, const wchar_t* URL, wchar_t* OutputConsoleCommand, int CommandBufferSize) = 0;

	/**
	 * Gets a list of targets that have been selected via menu's in UnrealEd.
	 *
	 * @param	OutTargetList			The list to be filled with target handles.
	 * @param	InOutTargetListSize		Contains the size of OutTargetList. When the function returns it contains the # of target handles copied over.
	 */
	virtual void GetMenuSelectedTargets(TARGETHANDLE *OutTargetList, int &InOutTargetListSize) = 0;

	/**
	 * Return the number of console-specific menu items this platform wants to add to the main
	 * menu in UnrealEd.
	 *
	 * @return	The number of menu items for this console
	 */
	virtual int GetNumMenuItems() 
	{
		return 0;
	}

	/**
	 * Return the string label for the requested menu item
	 * @param	Index		The requested menu item
	 * @param	bIsChecked	Is this menu item checked (or selected if radio menu item)
	 * @param	bIsRadio	Is this menu item part of a radio group?
	 * @param	OutHandle	Receives the handle of the target associated with the menu item.
	  *
	 * @return	Label for the requested menu item
	  */
	virtual const wchar_t* GetMenuItem(int /*Index*/, bool& /*bIsChecked*/, bool& /*bIsRadio*/, TARGETHANDLE& /*OutHandle*/)
	{
		return NULL; 
	}

	/**
	 * Internally process the given menu item when it is selected in the editor
	 * @param	Index		The selected menu item
	 * @param	OutputConsoleCommand	A buffer that the menu item can fill out with a console command to be run by the Editor on return from this function
	 * @param	CommandBufferSize		The size of the buffer pointed to by OutputConsoleCommand
	 */
	virtual void ProcessMenuItem(int /*Index*/, wchar_t* /*OutputConsoleCommand*/, int /*CommandBufferSize*/)
	{
	}

	/**
	 * Handles receiving a value from the application, when ProcessMenuItem returns that the ConsoleSupport needs to get a value
	 * @param	Value		The actual value received from user
	 */
	virtual void SetValueCallback(const wchar_t* /*Value*/) 
	{
	}

	/**
	 * Forces a stub target to be created to await connection
	 *
	 * @Param TargetAddress IP of target to add
	 *
	 * @returns Handle of new stub target
	 */
	virtual TARGETHANDLE ForceAddTarget( const wchar_t* /*TargetAddress*/ )
	{
		return NULL;
	}

	/**
	 * Asynchronously copies a set of files to a set of targets.
	 *
	 * @param	Handles						An array of targets to receive the files.
	 * @param	HandlesSize					The size of the array pointed to by Handles.
	 * @param	SrcFilesToSync				An array of source files to copy. This must be the same size as DestFilesToSync.
	 * @param	DestFilesToSync				An array of destination files to copy to. This must be the same size as SrcFilesToSync.
	 * @param	FilesToSyncSize				The size of the SrcFilesToSync and DestFilesToSync arrays.
	 * @param	DirectoriesToCreate			An array of directories to be created on the targets.
	 * @param	DirectoriesToCreateSize		The size of the DirectoriesToCreate array.
	 * @param	OutputCallback				TTY callback for receiving colored output.
	 */
	virtual bool SyncFiles(TARGETHANDLE * /*Handles*/, int /*HandlesSize*/, const wchar_t ** /*SrcFilesToSync*/, const wchar_t ** /*DestFilesToSync*/, int /*FilesToSyncSize*/, const wchar_t ** /*DirectoriesToCreate*/, int /*DirectoriesToCreateSize*/, ColoredTTYEventCallbackPtr /*OutputCallback*/)
	{
		return true;
	}
};

// Typedef's for easy DLL binding.
typedef FConsoleSupport* (*FuncGetConsoleSupport) (void*);

extern "C"
{
	/** 
	 * Returns a pointer to a subclass of FConsoleSupport.
	 *
	 * @return The pointer to the console specific FConsoleSupport
	 */
	CONSOLETOOLS_API FConsoleSupport* GetConsoleSupport(void*);
}

#endif
#endif	// SUPPORT_NAMES_ONLY

#pragma pack(pop)
