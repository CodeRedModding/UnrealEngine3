/*=============================================================================
	AndroidTools/AndroidSupport.cpp: Android platform support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "AndroidSupport.h"

#include "..\..\Engine\Classes\PixelFormatEnum.uci"

#include <ddraw.h>

using namespace System;
using namespace System::Text;

/*
#define NON_UE3_APPLICATION
#include "..\..\IpDrv\Inc\GameType.h"
#include "..\..\IpDrv\Inc\DebugServerDefs.h"
*/

#include "CommandLineWrapper.h"
#include "AndroidSymbolParser.h"

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

/**
 * Constructor that takes an ANSI name
 */
FAndroidTarget::FAndroidTarget(const char* InName)
	: bIsConnected(false)
	, TxtCallback(NULL)
	, CrashCallback(NULL)
	, TxtHandleRead(INVALID_HANDLE_VALUE)
	, bHasSetupSocket(false)
{
	TargetName = ToWString(string(InName));
}

/**
 * Terminates any child processes that the target has started
 */
void FAndroidTarget::KillChildProcess()
{
	if (TxtHandleRead != INVALID_HANDLE_VALUE)
	{
		// kill logcat
		TerminateProcess(TxtProcess.hProcess, 0);
		CloseHandle(TxtHandleRead);
		CloseHandle(TxtHandleWrite);
		CloseHandle(TxtProcess.hThread);
		CloseHandle(TxtProcess.hProcess);
	}
}

/**
 * Handle sending a string to TTY listeners (ie UnrealConsole)
 */
void FAndroidTarget::OnTTY(const wchar_t *Txt)
{
	if(TxtCallback)
	{
		TxtCallback(Txt);
	}
}

/**
 * Handle sending a string to TTY listeners (ie UnrealConsole)
 */
void FAndroidTarget::OnTTY(const string &Txt)
{
	wstring Final = ToWString(Txt);
	OnTTY(Final.c_str());
}

/**
 * Process a line of text, looking for crashes
 */
void FAndroidTarget::ParseLine(const string &Line)
{

// look for a crash like this:
// I/DEBUG   ( 2187): *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
// I/DEBUG   ( 2187): Build fingerprint: 'Samsung/SGH-I897/SGH-I897/SGH-I897:2.1-update1/ECLAIR/UCJF6:user/release-keys'
// I/DEBUG   ( 2187): pid: 2841, tid: 2853  >>> com.epicgames.UnrealEngine3 <<<
// I/DEBUG   ( 2187): signal 11 (SIGSEGV), fault addr 00000018
// I/DEBUG   ( 2187):  r0 00000000  r1 00020110  r2 0000f5fc  r3 0002015c
// I/DEBUG   ( 2187):  r4 831d2ad0  r5 000153f0  r6 00011954  r7 00000000
// I/DEBUG   ( 2187):  r8 0000f5fc  r9 000c793c  10 00011954  fp 000003f4
// I/DEBUG   ( 2187):  ip 00000000  sp 4c758e20  lr afe0f088  pc 82fb7512  cpsr 60000030
// ----> I/DEBUG   ( 2187):          #00  pc 00fb7512  /data/data/com.epicgames.UnrealEngine3/lib/libUnrealEngine3.so
// ----> I/DEBUG   ( 2187):          #01  pc 00f9bc1c  /data/data/com.epicgames.UnrealEngine3/lib/libUnrealEngine3.so
// ----> I/DEBUG   ( 2187):          #02  pc 0000fdd4  /system/lib/libc.so
// ----> I/DEBUG   ( 2187):          #03  pc 0000f8a0  /system/lib/libc.so
// I/DEBUG   ( 2187):
// I/DEBUG   ( 2187): code around pc:



	// cache the length of the I/DEBUG prefix
	const string::size_type IDEBUG_LEN = strlen("I/DEBUG");

#if _DEBUG
	OutputDebugStringA(Line.c_str());
#endif

	OnTTY(Line);

	// look for the first I/DEBUG line
	string::size_type TokIndex = Line.find("I/DEBUG");

	while(TokIndex != string::npos)
	{
		// look for a # at the column that will have it for #00\tpc
		if (TokIndex + 28 < Line.length() && Line[TokIndex + 28] == '#')
		{
			// read in the PC for the stack line
			DWORD Address = 0;
			if(sscanf_s(&Line[TokIndex + 36], "%8x", &Address) == 1)
			{
				Callstack.push_back(Address);
			}
		}
		else
		{
			// now look for the bit of text after the callstack that says we are done
			if (Callstack.size() != 0 && Line.find("code around pc", TokIndex) != string::npos)
			{
				if(!GenerateCallstack(Callstack))
				{
					OnTTY(L"A problem occured while generating the callstack!");
				}

				// done with the callstack, toss it!
				Callstack.clear();
			}
		}

		TokIndex = Line.find("I/DEBUG", TokIndex + IDEBUG_LEN);
	}
}

/**
 * Parse a callstack of address to text
 */
bool FAndroidTarget::GenerateCallstack(std::vector<DWORD> &CallstackAddresses)
{
	string ExePath;
	bool bSucceeded = false;

	if(CrashCallback)
	{
		try
		{
			FAndroidManagedSymbolParser^ SymbolParser = gcnew FAndroidManagedSymbolParser();

			// @todo: Is there anyway to get the .apk this was run with?? 
			if(SymbolParser->LoadSymbols(gcnew String(""), gcnew String(""), true, nullptr))
			{
				StringBuilder ^Bldr = gcnew StringBuilder("\r\nStack Trace:\r\n");
				String ^FileName = String::Empty;
				String ^Function = String::Empty;
				int LineNumber = 0;

				try
				{
					for(size_t CallStackIndex = 0; CallStackIndex < CallstackAddresses.size(); ++CallStackIndex)
					{
						SymbolParser->ResolveAddressToSymboInfo(CallstackAddresses[CallStackIndex], FileName, Function, LineNumber);
						Bldr->Append( String::Format( L"{0} [{1}:{2}]\r\n", Function->EndsWith( L")" ) ? Function : Function + L"()", FileName->Length > 0 ? FileName : L"???", LineNumber.ToString() ) );
					}

					bSucceeded = true;
				}
				finally
				{
					SymbolParser->UnloadSymbols();
				}

				Bldr->Append("\r\n");

				// @todo: If we can get the .apk above, we can get the GameName 
// 					String ^GameName = System::IO::Path::GetFileNameWithoutExtension(gcnew String(ExePath.c_str()));
// 					int GameNameIndex = GameName->LastIndexOf("Game-PS3", StringComparison::OrdinalIgnoreCase);
// 
// 					if(GameNameIndex != -1)
// 					{
// 						GameName = GameName->Substring(0, GameNameIndex);
// 					}
// 
// 					pin_ptr<const wchar_t> GameNamePtr = PtrToStringChars(GameName);
				const wchar_t* GameNamePtr = L"UnknownGame";
				pin_ptr<const wchar_t> FinalCallstack = PtrToStringChars(Bldr->ToString());

				CrashCallback(GameNamePtr, FinalCallstack, L"");
			}
		}
		catch(Exception ^ex)
		{
			pin_ptr<const wchar_t> Txt = PtrToStringChars(ex->ToString());
			OnTTY(Txt);
		}
	}
	else if(CrashCallback)
	{
		OnTTY(L"Couldn't get the path to the executable of the currently running process!\r\n");
	}

	return bSucceeded;
}
/**
 * Look for any incoming stdout from adb logcat, and process it
 */
void FAndroidTarget::ProcessTextFromChildProcess()
{
	if (TxtHandleRead != INVALID_HANDLE_VALUE)
	{
		// poll status of the child process
		DWORD Reason = WaitForSingleObject(TxtProcess.hProcess, 0);

		// See if we have some data to read
		DWORD SizeToRead;
		PeekNamedPipe(TxtHandleRead, NULL, 0, NULL, &SizeToRead, NULL);

		// buffer for to read in text
		char Buffer[1024 * 16];
		DWORD BufferSize = sizeof(Buffer);

		// pull it off
		while (SizeToRead > 0)
		{
			// read some output
			DWORD SizeRead;
			ReadFile(TxtHandleRead, &Buffer, min(SizeToRead, BufferSize - 1), &SizeRead, NULL);
			Buffer[SizeRead] = 0;

			// decrease how much we need to read
			SizeToRead -= SizeRead;

			// convert it to a string
			string TxtBuf(Buffer);
			string::size_type LastLineIndex = TxtBuf.find_last_of('\n');

			// look for a EOL
			if(LastLineIndex == string::npos)
			{
				// if we didn't get one, save it for later
				TTYBuffer += TxtBuf;
			}
			else
			{
				// get all complete lines
				string LineSegment = TxtBuf.substr(0, LastLineIndex + 1);

				// prepend any cached partial lines
				if(TTYBuffer.size() > 0)
				{
					TTYBuffer += LineSegment;
					ParseLine(TTYBuffer);
				}
				else
				{
					ParseLine(LineSegment);
				}

				// remember the text after the last EOL to deal with next time
				TTYBuffer = TxtBuf.substr(LastLineIndex + 1);
			}
		}

		// when the process signals, its done, so close down operations
		// @todo: Actually, if adb logcat goes down, we should keep trying to restart it (maybe once a second or something)
		if (Reason == WAIT_OBJECT_0)
		{
			CloseHandle(TxtHandleRead);
			CloseHandle(TxtHandleWrite);
			CloseHandle(TxtProcess.hThread);
			CloseHandle(TxtProcess.hProcess);
			TxtHandleRead = INVALID_HANDLE_VALUE;
		}
	}
}


/**
 * Runs a child process without spawning a command prompt window for each one
 *
 * @param CommandLine The commandline of the child process to run
 * @param Errors Output buffer for any errors
 * @param ErrorsSize Size of the Errors buffer
 * @param ProcessReturnValue Optional out value that the process returned
 *
 * @return TRUE if the process was run (even if the process failed)
 */
bool RunChildProcess(const char* CommandLine, char* Errors, int ErrorsSize, DWORD* ProcessReturnValue, int Timeout)
{
	// run the command (and avoid a window popping up)
	SECURITY_ATTRIBUTES SecurityAttr;
	SecurityAttr.nLength = sizeof(SecurityAttr);
	SecurityAttr.bInheritHandle = TRUE;
	SecurityAttr.lpSecurityDescriptor = NULL;

	HANDLE StdOutRead, StdOutWrite;
	CreatePipe(&StdOutRead, &StdOutWrite, &SecurityAttr, 0);
	SetHandleInformation(StdOutRead, HANDLE_FLAG_INHERIT, 0);

	// set up process spawn structures
	STARTUPINFOA StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	StartupInfo.hStdOutput = StdOutWrite;
	StartupInfo.hStdError = StdOutWrite;
	PROCESS_INFORMATION ProcInfo;

	// kick off the child process
	if (!CreateProcessA(NULL, (LPSTR)CommandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &StartupInfo, &ProcInfo))
	{
		sprintf(Errors, "\nFailed to start process '%s'\n", CommandLine);
		return false;
	}

	bool bProcessComplete = false;
	char Buffer[1024 * 64];
	DWORD BufferSize = sizeof(Buffer);
	Errors[0] = 0;

	// wait until the precompiler is finished
	int TimeoutCount = 0;
	while (!bProcessComplete)
	{
		//Don't return until the job is complete (-1 is an infinite timeout)
		DWORD Reason = WaitForSingleObject(ProcInfo.hProcess, 1000);

		// read up to 64k of error (that would be crazy amount of error, but whatever)
		DWORD SizeToRead;
		// See if we have some data to read
		PeekNamedPipe(StdOutRead, NULL, 0, NULL, &SizeToRead, NULL);
		while (SizeToRead > 0)
		{
			// read some output
			DWORD SizeRead;
			ReadFile(StdOutRead, &Buffer, min(SizeToRead, BufferSize - 1), &SizeRead, NULL);
			Buffer[SizeRead] = 0;

			// decrease how much we need to read
			SizeToRead -= SizeRead;

			// append the output to the
			strncpy(Errors, Buffer, ErrorsSize - 1);
		}

		// when the process signals, its done
		if (Reason == WAIT_OBJECT_0)
		{
			// break out of the loop
			bProcessComplete = true;
		}

		TimeoutCount++;
		if( Timeout != 0 && TimeoutCount > Timeout )
		{
			sprintf(Errors, "\nProcess '%s' timed out after '%d' seconds; kicking\n", CommandLine, Timeout);
			break;
		}
	}

	// Get the return value
	DWORD ErrorCode;
	GetExitCodeProcess(ProcInfo.hProcess, &ErrorCode);

	// pass back the return code if desired
	if (ProcessReturnValue)
	{
		*ProcessReturnValue = ErrorCode;
	}

	// Close process and thread handles.
	CloseHandle(ProcInfo.hProcess);
	CloseHandle(ProcInfo.hThread);
	CloseHandle(StdOutRead);
	CloseHandle(StdOutWrite);

	return bProcessComplete;
}

struct FAndroidSoundCooker : public FConsoleSoundCooker
{
	/**
	* Constructor
	*/
	FAndroidSoundCooker( void )
	{
	}

	/**
	* Virtual destructor
	*/
	virtual ~FAndroidSoundCooker( void )
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
	virtual bool Cook( const BYTE* /*SrcBuffer*/, FSoundQualityInfo* /*QualityInfo*/ )
	{
		return false;
	}

	/**
	* Cooks upto 8 mono files into a multichannel file (eg. 5.1). The front left channel is required, the rest are optional.
	*
	* @param	SrcBuffers		Pointers to source buffers
	* @param	QualityInfo		All the information the compressor needs to compress the audio
	*
	* @return	TRUE if succeeded, FALSE otherwise
	*/
	virtual bool CookSurround( const BYTE*[8] /*SrcBuffers[8]*/, FSoundQualityInfo* /*QualityInfo*/ )
	{
		return false;
	}

	/**
	* Returns the size of the cooked data in bytes.
	*
	* @return The size in bytes of the cooked data including any potential header information.
	*/
	virtual UINT GetCookedDataSize( void )
	{
		return 4;
	}

	/**
	* Copies the cooked ata into the passed in buffer of at least size GetCookedDataSize()
	*
	* @param CookedData		Buffer of at least GetCookedDataSize() bytes to copy data to.
	*/
	virtual void GetCookedData( BYTE* CookedData )
	{
		DWORD Zero = 0;
		memcpy(CookedData, &Zero, 4);
	}

	/**
	* Recompresses raw PCM to the the platform dependent format, and then back to PCM. Used for quality previewing.
	*
	* @param	SrcData			Uncompressed PCM data
	* @param	DstData			Uncompressed PCM data after being compressed
	* @param	QualityInfo		All the information the compressor needs to compress the audio
	*/
	virtual INT Recompress( const BYTE* /*SrcBuffer*/, BYTE* /*DestBuffer*/, FSoundQualityInfo* /*QualityInfo*/ )
	{
		return 0;
	}

	/**
	* Queries for any warning or error messages resulting from the cooking phase
	*
	* @return Warning or error message string, or NULL if nothing to report
	*/
	virtual const wchar_t* GetCookErrorMessages( void ) const
	{
		return NULL;
	}

private:

};

/**
* Maps Unreal formats to various settings about a pixel
*/
struct FPixelFormat
{
	UINT BlockSizeX, BlockSizeY, BlockSizeZ, BlockBytes;
	DWORD DDSFourCC;
	bool NeedToSwizzle;
};

struct FAndroidTextureCooker : public FConsoleTextureCooker
{
	/**
	* Constructor
	*/
	FAndroidTextureCooker()
	{
	}

	/**
	 * Destructor
	 */
	virtual ~FAndroidTextureCooker()
	{
	}

	/**
	 * Converts an unreal pixel format enumeration value to a FPixelFormat structure
	 *
	 * @param UnrealFormat	Unreal pixel format
	 *
	 * @return The FPixelFormat that matches the specified Unreal format
	 */
	FPixelFormat GetPixelFormat(DWORD UnrealFormat)
	{
		switch (UnrealFormat)
		{
		case PF_A8R8G8B8:
			{
				FPixelFormat Format = { 1,	1,	1,	4,	0,		true };
				return Format;
			}
		case PF_G8:
			{
				FPixelFormat Format = { 1,	1,	1,	1,	0,		true };
				return Format;
			}
		case PF_DXT1:
			{
				FPixelFormat Format = { 4,	4,	1,	8,	'1TXD',	false };
				return Format;
			}
		case PF_DXT3:
			{
				FPixelFormat Format = { 4,	4,	1,	16,	'3TXD',	false };
				return Format;
			}
		case PF_DXT5:
			{
				FPixelFormat Format = { 4,	4,	1,	16,	'5TXD',	false };
				return Format;
			}
		default:
			// Unsupported format
			{
				FPixelFormat Format = { 1,	1,	1,	0,	0,		false };
				return Format;
			}
		}
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
	virtual void Init( DWORD UnrealFormat, UINT Width, UINT Height, UINT /*NumMips*/, DWORD /*CreateFlags*/ )
	{
		// cache information
		Format = UnrealFormat;
		SizeX = Width;
		SizeY = Height;

		PixelFormat = GetPixelFormat(UnrealFormat);
	}

	/**
	* Gets the pitch in bytes for a row of the texture at the given mip index
	* @param	Level		Miplevel to query size for
	*/
	UINT GetPitch( DWORD Level )
	{
		// NOTE: We don't add any padding in cooked data

		// calculate by blocks, for things like DXT
		UINT MipSizeX = max(SizeX >> Level, PixelFormat.BlockSizeX);
		UINT Pitch = (MipSizeX / PixelFormat.BlockSizeX) * PixelFormat.BlockBytes;
		return Pitch;
	}

	/**
	* Gets the number of rows of the mip at the given mip index
	* @param	Level		Miplevel to query size for
	*/
	UINT GetNumRows( DWORD Level )
	{
		UINT MipSizeY = max(SizeY >> Level, PixelFormat.BlockSizeY) / PixelFormat.BlockSizeY;
		return MipSizeY;
	}

	/**
	* Returns the platform specific size of a miplevel.
	*
	* @param	Level		Miplevel to query size for
	* @return	Returns	the size in bytes of Miplevel 'Level'
	*/
	virtual UINT GetMipSize( UINT Level )
	{
		// calculate the size in bytes
		return GetPitch(Level) * GetNumRows(Level);
	}

	/**
	* Cooks the specified miplevel, and puts the result in Dst which is assumed to
	* be of at least GetMipSize size.
	*
	* @param Level			Miplevel to cook
	* @param Src			Src pointer
	* @param Dst			Dst pointer, needs to be of at least GetMipSize size
	* @param SrcRowPitch	Row pitch of source data
	*/
	virtual void CookMip( UINT Level, void* Src, void* Dst, UINT /*SrcRowPitch*/ )
	{
		// just write the data back out
		UINT NumBytes = GetMipSize(Level);
		memcpy(Dst, Src, NumBytes);
	}

	/**
	* Returns the index of the first mip level that resides in the packed miptail
	*
	* @return index of first level of miptail
	*/
	virtual INT GetMipTailBase()
	{
		return 0xFF;
	}

	/**
	* Cooks all mip levels that reside in the packed miptail. Dst is assumed to
	* be of size GetMipSize (size of the miptail level).
	*
	* @param Src - ptrs to mip data for each source mip level
	* @param SrcRowPitch - array of row pitch entries for each source mip level
	* @param Dst - ptr to miptail destination
	*/
	virtual void CookMipTail( void** /*Src*/, UINT* /*SrcRowPitch*/, void* /*Dst*/ )
	{

	}

private:

	/** Cache some texture information */
	FPixelFormat PixelFormat;
	UINT Format;
	UINT SizeX;
	UINT SizeY;
};

struct FAndroidSkeletalMeshCooker : public FConsoleSkeletalMeshCooker
{
	void Init(void)
	{
	}

	virtual void CookMeshElement(const FSkeletalMeshFragmentCookInfo& ElementInfo, FSkeletalMeshFragmentCookOutputInfo& OutInfo)
	{
		// no optimization needed, just copy the data over
		memcpy(OutInfo.NewIndices, ElementInfo.Indices, ElementInfo.NumTriangles * 3 * sizeof(DWORD) );
	}
};

/**
* IPhone version of static mesh cooker
*/
struct FAndroidStaticMeshCooker : public FConsoleStaticMeshCooker
{
	void Init(void)
	{

	}

	/**
	* Cooks a mesh element.
	* @param ElementInfo - Information about the element being cooked
	* @param OutIndices - Upon return, contains the optimized index buffer.
	* @param OutPartitionSizes - Upon return, points to a list of partition sizes in number of triangles.
	* @param OutNumPartitions - Upon return, contains the number of partitions.
	* @param OutVertexIndexRemap - Upon return, points to a list of vertex indices which maps from output vertex index to input vertex index.
	* @param OutNumVertices - Upon return, contains the number of vertices indexed by OutVertexIndexRemap.
	*/
	virtual void CookMeshElement(FMeshFragmentCookInfo& ElementInfo, FMeshFragmentCookOutputInfo& OutInfo)
	{
		// no optimization needed, just copy the data over
		memcpy(OutInfo.NewIndices, ElementInfo.Indices, ElementInfo.NumTriangles * 3 * sizeof(WORD) );
	}
};

/** Android platform shader precompiler */
struct FAndroidShaderPrecompiler : public FConsoleShaderPrecompiler
{
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
		const char* /*ShaderPath*/,
		const char* /*EntryFunction*/,
		bool /*bIsVertexShader*/,
		DWORD /*CompileFlags*/,
		const char* /*Definitions*/,
		const char* /*IncludeDirectory*/,
		char* const* /*IncludeFileNames*/,
		char* const* /*IncludeFileContents*/,
		int /*NumIncludes*/,
		bool /*bDumpingShaderPDBs*/,
		const char* /*ShaderPDBPath*/,
		unsigned char* /*BytecodeBufer*/,
		int& /*BytecodeSize*/,
		char* /*ConstantBuffer*/,
		char* /*Errors*/
		)
	{
		return false;
	}

private:
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
		const char* /*ShaderPath*/,
		const char* /*Definitions*/,
		const char* /*IncludeDirectory*/,
		char* const* /*IncludeFileNames*/,
		char* const* /*IncludeFileContents*/,
		int /*NumIncludes*/,
		unsigned char* /*ShaderText*/,
		int& /*ShaderTextSize*/,
		char* /*Errors*/
		)
	{
		return false;
	}

	/**
	* Disassemble the shader with the given byte code. Must be implemented
	*
	* @param ShaderByteCode	The null terminated shader byte code to be disassembled
	* @param ShaderText		Block of memory to fill in with the preprocessed shader output
	* @param ShaderTextSize	Size of the returned text
	*
	* @return true if successful
	*/
	virtual bool DisassembleShader(
		const DWORD* /*ShaderByteCode*/,
		unsigned char* /*ShaderText*/,
		int& /*ShaderTextSize*/)
	{
		return false;
	}

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
	virtual bool CreateShaderCompileCommandLine(
		const char* /*ShaderPath*/,
		const char* /*IncludePath*/,
		const char* /*EntryFunction*/,
		bool /*bIsVertexShader*/,
		DWORD /*CompileFlags*/,
		const char* /*Definitions*/,
		char* /*CommandStr*/,
		bool /*bPreprocessed*/)
	{
		return false;
	}
};

#include "AndroidSupport.h"

FAndroidSupport::FAndroidSupport(void* InModule)
{
	// Remember the module handle for freeing the dll
	Module = InModule;

	// hunt down the android install directory
	char EnvVar[MAX_PATH];
	if (GetEnvironmentVariableA("ANDROID_ROOT", EnvVar, MAX_PATH - 1))
	{
		AndroidRoot = EnvVar;
	}
	else
	{
		// assume C:\Android if no env var is set
		AndroidRoot = "C:\\Android";
		// if it doesn't exist, assume D:\Android - after this we give up and things fail
		if (GetFileAttributesA("C:\\Android") == INVALID_FILE_ATTRIBUTES &&
			GetFileAttributesA("D:\\Android") != INVALID_FILE_ATTRIBUTES)
		{
			AndroidRoot = "D:\\Android";
		}
	}
}

FAndroidSupport::~FAndroidSupport()
{
	for (size_t TargetIndex = 0; TargetIndex < AndroidTargets.size(); TargetIndex++)
	{
		FAndroidTarget* Target = AndroidTargets[TargetIndex];
		Target->KillChildProcess();
	}
}

/** Initialize the DLL with some information about the game/editor
 *
 * @param	GameName		The name of the current game ("ExampleGame", "UTGame", etc)
 * @param	Configuration	The name of the configuration to run ("Debug", "Release", etc)
 */
void FAndroidSupport::Initialize(const wchar_t* InGameName, const wchar_t* InConfiguration)
{
	// cache the parameters
	GameName = InGameName;
	Configuration = InConfiguration;

	RelativeBinariesDir = L".";

	wchar_t CurDir[1024];
	GetCurrentDirectoryW(1024, CurDir);
	if (_wcsicmp(CurDir + wcslen(CurDir) - 8, L"Binaries") != 0)
	{
		RelativeBinariesDir = L"..\\";
	}

	SelectedTargetIndex = GetIniInt(L"TargetIndex", 0);

	// set it to 0 if it's out of range
	if (SelectedTargetIndex >= (int)AndroidTargets.size())
	{
		SelectedTargetIndex = 0;
	}
}

/**
 * Returns the type of the specified target.
 */
FConsoleSupport::ETargetType FAndroidSupport::GetTargetType(TARGETHANDLE Handle)
{
	ETargetType RetVal = TART_Unknown;
	TargetPtr Target = NetworkManager.GetTarget(Handle);

	if(Target)
	{
		RetVal = TART_Remote;
	}

	return RetVal;
}

/**
 * Retrieves a target with the specified handle.
 *
 * @param	Handle	The handle of the target to retrieve.
 */
FAndroidTarget* FAndroidSupport::GetTarget(TARGETHANDLE Handle)
{
	size_t Index = (size_t)Handle;

	if(Index >= 0 && Index < AndroidTargets.size())
	{
		return AndroidTargets[Index];
	}

	return NULL;
}

/** Returns a string with the relative path to the binaries folder prepended */
wstring FAndroidSupport::GetRelativePathString( wchar_t* InWideString )
{
	wchar_t TempWide[1024];
	wcscpy_s( TempWide, ARRAY_COUNT( TempWide ), RelativeBinariesDir.c_str() );
	wcscat_s( TempWide, ARRAY_COUNT( TempWide ), InWideString );

	return TempWide;
}

int FAndroidSupport::GetIniInt(const wchar_t* Key, int Default)
{
	// this integer should never be the default for a value
	const int DUMMY_VALUE = -1234567;

	// look in the user settings
	int Value = GetPrivateProfileInt(L"PlayOnAndroid", Key, DUMMY_VALUE, GetRelativePathString( L"Android\\PlayOnAndroid.ini" ).c_str() );
	// if not found, look in the default ini, which really should have it (but can still use the default if not)
	if (Value == DUMMY_VALUE)
	{
		Value = GetPrivateProfileInt(L"PlayOnAndroid", Key, Default, GetRelativePathString( L"Android\\DefaultPlayOnAndroid.ini" ).c_str() );
	}
	return Value;
}

void FAndroidSupport::GetIniString(const wchar_t* Key, const wchar_t* Default, wchar_t* Value, int ValueSize)
{
	const wchar_t* DUMMY_VALUE = L"DUMMY";
	assert((unsigned int)ValueSize > wcslen(DUMMY_VALUE) + 1);

	GetPrivateProfileString(L"PlayOnAndroid", Key, DUMMY_VALUE, Value, ValueSize, GetRelativePathString( L"Android\\PlayOnAndroid.ini" ).c_str() );
	if (wcscmp(Value, DUMMY_VALUE) == 0)
	{
		GetPrivateProfileString(L"PlayOnAndroid", Key, Default, Value, ValueSize, GetRelativePathString( L"Android\\DefaultPlayOnAndroid.ini" ).c_str() );
	}
}

bool FAndroidSupport::GetIniBool(const wchar_t* Key, bool Default)
{
	wchar_t Value[16];
	GetIniString(Key, Default ? L"true" : L"false", Value, sizeof(Value));
	return wcscmp(Value, L"true") == 0;
}

void FAndroidSupport::SetIniInt(const wchar_t* Key, int Value)
{
	wchar_t ValueStr[65];
	_itow(Value, ValueStr, 10);
	SetIniString(Key, ValueStr);
}

void FAndroidSupport::SetIniString(const wchar_t* Key, const wchar_t* Value)
{
	WritePrivateProfileString(L"PlayOnAndroid", Key, Value, GetRelativePathString( L"Android\\PlayOnAndroid.ini" ).c_str() );
}

void FAndroidSupport::SetIniBool(const wchar_t* Key, bool Value)
{
	SetIniString(Key, Value ? L"true" : L"false");
}

/**
 * Return the default IP address to use when sending data into the game for object propagation
 * Note that this is probably different than the IP address used to debug (reboot, run executable, etc)
 * the console. (required to implement)
 *
 * @param	Handle The handle of the console to retrieve the information from.
 *
 * @return	The in-game IP address of the console, in an Intel byte order 32 bit integer
 */
unsigned int FAndroidSupport::GetIPAddress(TARGETHANDLE Handle)
{
	FAndroidTarget* Target = GetTarget(Handle);

	if (Target == NULL)
	{
		return 0;
	}

	// return local host (127.0.0.1)
	return 0x7F000001;
}

/**
 * Get the name of the specified target
 *
 * @param	Handle The handle of the console to retrieve the information from.
 * @return Name of the target, or NULL if the Index is out of bounds
 */
const wchar_t* FAndroidSupport::GetTargetName(TARGETHANDLE Handle)
{
	FAndroidTarget* Target = GetTarget(Handle);

	if (Target == NULL)
	{
		return NULL;
	}

	return Target->TargetName.c_str();
}

const wchar_t* FAndroidSupport::GetTargetComputerName(TARGETHANDLE Handle)
{
	FAndroidTarget* Target = GetTarget(Handle);

	if (Target == NULL)
	{
		return NULL;
	}

	return Target->TargetName.c_str();
}

const wchar_t* FAndroidSupport::GetTargetGameName(TARGETHANDLE /*Handle*/)
{
	return L"Unknown";
}

const wchar_t* FAndroidSupport::GetTargetGameType(TARGETHANDLE Handle)
{
	FAndroidTarget* Target = GetTarget(Handle);

	if (Target == NULL)
	{
		return NULL;
	}

	return L"Game";
}

/**
 * Low level file copy to android using adb push.
 * Verifies that the file is newer on the device by keeping a staging area of the most recently copied files
 * @param SourceFileName - Name of the file to copy to device
 * @param DestFileName - Name of the target file
 */
void FAndroidSupport::CopyFileWhenNewer (FConsoleFileManifest& Manifest, const wchar_t* SourceFilename, const wchar_t* DestFilename, bool bDisplayMessageBox, ColoredTTYEventCallbackPtr OutputCallback)
{
	char CommandLine[1024];
	char Errors[1024];
	wchar_t Message[1024];
	//Non-zero return value omits the "copied" message

	int Timeout = Manifest.ShouldCopyFile(SourceFilename);
	if (Timeout > 0)
	{
		DWORD RetVal = 0;
		sprintf_s(CommandLine, "%s\\android-sdk-windows\\platform-tools\\adb.exe push %ls /sdcard/UnrealEngine3/%ls",
			AndroidRoot.c_str(), SourceFilename, DestFilename);

		wsprintf( Message, L"Copying '%ls' with timeout %d\r\n", SourceFilename, Timeout );
		OutputDebugStringW( Message );
		while( !RunChildProcess(CommandLine, Errors, 1023, &RetVal, Timeout ) )
		{
			// infinite retry!
			OutputDebugStringW( L" ... copy timed out; retrying\r\n" );
		}

		if (bDisplayMessageBox)
		{
			::MessageBoxA(NULL, CommandLine, "Copying", MB_OK);
		}

		if (OutputCallback)
		{
			if (RetVal == 0)
			{
				sprintf_s(Errors, "Copied %ls to /sdcard/UnrealEngine3/%ls...", SourceFilename, DestFilename);
			}
			wstring WideErrors = ToWString(Errors);
			OutputCallback(0x000000ff, WideErrors.c_str());
		}

		Manifest.UpdateFileStatusInLog(SourceFilename);
	}
}

/**
 * Copies a single file from PC to target
 *
 * @param Handle		The handle of the console to retrieve the information from.
 * @param SourceFilename Path of the source file on PC
 * @param DestFilename Platform-independent destination filename (ie, no xe:\\ for Xbox, etc)
 *
 * @return true if successful
 */
bool FAndroidSupport::CopyFileToTarget(TARGETHANDLE /*Handle*/, const wchar_t* SourceFilename, const wchar_t* DestFilename)
{
	bool bDisplayMessageBox = true;

	FConsoleFileManifest Manifest;
	Manifest.PullFromDevice(AndroidRoot.c_str());

	// copy a single file to the device
	CopyFileWhenNewer(Manifest, SourceFilename, DestFilename, bDisplayMessageBox, NULL);

	Manifest.Push(AndroidRoot.c_str());

	return false;
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
bool FAndroidSupport::SyncFiles(TARGETHANDLE * /*Handles*/, int /*HandlesSize*/, const wchar_t **SrcFilesToSync, const wchar_t **DestFilesToSync, int FilesToSyncSize, const wchar_t ** /*DirectoriesToCreate*/, int /*DirectoriesToCreateSize*/, ColoredTTYEventCallbackPtr OutputCallback)
{
	wchar_t LocalFilename[1024];

	FConsoleFileManifest Manifest;
	Manifest.PullFromDevice(AndroidRoot.c_str());

	for (int File = 0; File < FilesToSyncSize; File++)
	{
		// skip over the base directory
		const wchar_t* RootLoc = wcschr(DestFilesToSync[File], '\\');
		if (RootLoc == NULL)
		{
			RootLoc = DestFilesToSync[File];
		}
		else
		{
			// skip over hte slash
			RootLoc += 1;
		}

		// copy to our local buffer
		wcscpy_s(LocalFilename, RootLoc);

		// fix slashes
		for (wchar_t* Travel = LocalFilename; *Travel; Travel++)
		{
			if (*Travel == '\\')
			{
				*Travel = '/';
			}
		}

		bool bDisplayMessageBox = false;

		// copy the file to the device (sync would be nice here!)
		CopyFileWhenNewer(Manifest, SrcFilesToSync[File], LocalFilename, bDisplayMessageBox, OutputCallback);
	}

	Manifest.Push(AndroidRoot.c_str());

	return true;
}

bool FAndroidSupport::ConnectToTarget(TARGETHANDLE Handle)
{
	// @todo android: Maybe start an adb logcat process or something here?
	FAndroidTarget* Target = GetTarget(Handle);
	if (Target == NULL)
	{
		return false;
	}

	// do nothing if already connected
	if (Target->bIsConnected)
	{
		return true;
	}

	Target->bIsConnected = true;
	return true;
}

/**
 * Called after an atomic operation to close any open connections
 *
 * @param Handle The handle of the console to disconnect.
 */
void FAndroidSupport::DisconnectFromTarget(TARGETHANDLE Handle)
{
	FAndroidTarget* Target = GetTarget(Handle);
	if (Target == NULL)
	{
		return;
	}

	Target->bIsConnected = false;
}

/**
 * Reboots the target console. Must be implemented
 *
 * @param Handle The handle of the console to retrieve the information from.
 *
 * @return true if successful
 */
bool FAndroidSupport::ResetTargetToShell(TARGETHANDLE /*Handle*/, bool )
{
	DWORD RetVal;
	char Output[1024];
	char CommandLine[1024];
	sprintf_s(CommandLine, "%s\\android-sdk-windows\\platform-tools\\adb.exe reboot", AndroidRoot.c_str());
	RunChildProcess(CommandLine, Output, 1023, &RetVal, 0);

	return RetVal == 0;
}

/**
 * Reboots the target console. Must be implemented
 *
 * @param Handle			The handle of the console to retrieve the information from.
 * @param Configuration		Build type to run (Debug, Release, RelaseLTCG, etc)
 * @param BaseDirectory		Location of the build on the console (can be empty for platforms that don't copy to the console)
 * @param GameName			Name of the game to run (Example, UT, etc)
 * @param URL				Optional URL to pass to the executable
 * @param bForceGameName	Forces the name of the executable to be only what's in GameName instead of being auto-generated
 *
 * @return true if successful
 */
bool FAndroidSupport::RunGameOnTarget(TARGETHANDLE, const wchar_t* GameName, const wchar_t* Configuration, const wchar_t* URL, const wchar_t* BaseDirectory)
{
	char CommandLine[1024];
	char Errors[1024];
	DWORD RetVal;

	// use aapt dump badging to get the package name from the apk
	// kill a running instance (uninstalling it works, but it removes shortcuts on the desktop, which is annoying)
	sprintf_s(CommandLine, "%s\\android-sdk-windows\\platform-tools\\aapt.exe dump badging %lsAndroid\\%ls-%ls.apk",
		AndroidRoot.c_str(), RelativeBinariesDir.c_str(), GameName, Configuration);
	RunChildProcess(CommandLine, Errors, 1023, &RetVal, 0);

	// aapt should be able to parse the apk, or we have problems
	if (RetVal != 0)
	{
		return false;
	}

	// parse the output (in the Errors block) for the package name, it will look like:
	//		package: name='com.epicgames.UnrealEngine3' versionCode='1' versionName='1.0'
	char* FirstTick = strchr(Errors, '\'');
	if (FirstTick == NULL)
	{
		return false;
	}
	char* SecondTick = strchr(FirstTick + 1, '\'');

	// extract the PackageName
	char PackageName[256];
	strncpy_s(PackageName, FirstTick + 1, SecondTick - FirstTick - 1);

	// uninstall the existing apk
	sprintf_s(CommandLine, "%s\\android-sdk-windows\\platform-tools\\adb.exe -d uninstall %s",
		AndroidRoot.c_str(), PackageName);
	RunChildProcess(CommandLine, Errors, 1023, &RetVal, 0);

	if (RetVal != 0)
	{
		return false;
	}

	// install the apk
	sprintf_s(CommandLine, "%s\\android-sdk-windows\\platform-tools\\adb.exe -d install %lsAndroid\\%ls-%ls.apk",
		AndroidRoot.c_str(), RelativeBinariesDir.c_str(), GameName, Configuration);
	RunChildProcess(CommandLine, Errors, 1023, &RetVal, 0);

	if (RetVal != 0)
	{
		return false;
	}

	// look for Failure word
	if (strstr(Errors, "Failure") != NULL)
	{
		return false;
	}

	wchar_t* Formats[] = { L"DXT", L"PVRTC", L"ATITC" };
	for (INT Index = 0; Index < ARRAY_COUNT(Formats); Index++)
	{
		// open the command line file
		wchar_t CommandLineFilename[1024];
		swprintf(CommandLineFilename, ARRAY_COUNT(CommandLineFilename), L"%ls..\\%ls\\CookedAndroid_%ls\\UE3CommandLine.txt",
			RelativeBinariesDir.c_str(), GameName, Formats[Index] );

		FILE* CommandLineFile = _wfopen(CommandLineFilename, L"w");
		// only write it if the directory existed
		if (CommandLineFile)
		{
			// write out the URL to the commandline
			fwprintf(CommandLineFile, L"%ls -BaseDir=%ls", URL, BaseDirectory);
			fclose(CommandLineFile);
		}
	}

	// run the installed apk
	sprintf_s(CommandLine, "%s\\android-sdk-windows\\platform-tools\\adb.exe shell am start -n com.epicgames.UnrealEngine3/com.epicgames.UnrealEngine3.UE3JavaApp",
		AndroidRoot.c_str());
	RunChildProcess(CommandLine, Errors, 1023, &RetVal, 0);

	return RetVal == 0;
/*
	// run the command (and avoid a window popping up)
	SECURITY_ATTRIBUTES SecurityAttr;
	SecurityAttr.nLength = sizeof(SecurityAttr);
	SecurityAttr.bInheritHandle = TRUE;
	SecurityAttr.lpSecurityDescriptor = NULL;

	// set up process spawn structures
	STARTUPINFOW StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	PROCESS_INFORMATION ProcInfo;

	// kick off the child process
	if (!CreateProcessW(NULL, (LPWSTR)CommandLine, NULL, NULL, TRUE, 0, NULL, NULL, &StartupInfo, &ProcInfo))
	{
		return false;
	}

	// closing the handles won't kill the process, but allows OS to completely close the child app
	CloseHandle(ProcInfo.hProcess);
	CloseHandle(ProcInfo.hThread);
*/
}

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
bool FAndroidSupport::RunGame(TARGETHANDLE* TargetList, int NumTargets, const wchar_t* MapName, const wchar_t* URL, wchar_t* /*OutputConsoleCommand*/, int /*CommandBufferSize*/)
{
	wchar_t* URLCopy = _wcsdup(URL);

	bool bUseDebugExecutable = false;

	//		char Errors[1024];
	DWORD ReturnValue;
	wchar_t Command[1024];

	// cook the map
	swprintf(Command,
		ARRAY_COUNT(Command),
		L"%ls%ls cookpackages %ls -platform=Android",
		_wcsicmp(Configuration.c_str(), L"Debug") == 0 ? L"Debug-" : L"",
		GameName.c_str(),
		MapName);
	string CommandString = ToString(Command);

	// spawn the child process with no DOS window popping up
	//		RunChildProcess(CommandString.c_str(), Errors, sizeof(Errors) - 1, &ReturnValue);
	ReturnValue = system(CommandString.c_str());

	if (ReturnValue != 0)
	{
		MessageBox(NULL, L"Cooking failed. See Logs\\Launch.log for more information", L"Cooking failed", MB_ICONSTOP);
		return false;
	}

	// update the TOC
	swprintf(Command,
		ARRAY_COUNT(Command),
		L"%lsCookerSync %ls -p Android -nd",
		RelativeBinariesDir.c_str(),
		GameName.c_str());
	CommandString = ToString(Command);

	// spawn the child process with no DOS window popping up
	//		RunChildProcess(CommandString.c_str(), Errors, sizeof(Errors) - 1, &ReturnValue);
	ReturnValue = system(CommandString.c_str());

	if (ReturnValue != 0)
	{
		MessageBox(NULL, L"CookerSync failed. See Logs\\Launch.log for more information", L"Synching failed", MB_ICONSTOP);
		return false;
	}

	// save out the commandline
	wchar_t CommandLineFilePath[1024];
	swprintf( CommandLineFilePath, ARRAY_COUNT( CommandLineFilePath ), L"%ls..\\%ls\\CookedAndroid\\UE3CommandLine.txt",
		RelativeBinariesDir.c_str(), GameName.c_str());

	// open the command line file
	FILE* CommandLineFile = _wfopen(CommandLineFilePath, L"w");

	// write out the URL to the commandline
	fwprintf(CommandLineFile, URL);
	fclose(CommandLineFile);

	for(int i = 0; i < NumTargets; ++i)
	{
		if (ConnectToTarget(TargetList[i]) == false)
		{
			MessageBox(NULL, L"Failed to connect to a target. Check your settings and the Target Manager", L"Connect failed", MB_ICONSTOP);
			return false;
		}

		if (RunGameOnTarget(TargetList[i], GameName.c_str(), bUseDebugExecutable ? L"Debug" : L"Release", URL, L"") == false)
		{
			MessageBox(NULL, L"Running failed.", L"Run failed", MB_ICONSTOP);
			return false;
		}
	}

	free(URLCopy);

	return true;
}

/**
 * Return the number of console-specific menu items this platform wants to add to the main
 * menu in UnrealEd.
 *
 * @return	The number of menu items for this console
 */
int FAndroidSupport::GetNumMenuItems()
{
	return (int)AndroidTargets.size();
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
const wchar_t* FAndroidSupport::GetMenuItem(int Index, bool& bIsChecked, bool& bIsRadio, TARGETHANDLE& OutHandle)
{
	bIsRadio = true;
	bIsChecked = false;

	FAndroidTarget* Target = AndroidTargets[Index];
	if (Target == NULL)
	{
		return L"<None>";
	}

	bIsChecked = SelectedTargetIndex == Index;
	OutHandle = (TARGETHANDLE)Index;
	return Target->TargetName.c_str();
}

/**
 * Gets a list of targets that have been selected via menu's in UnrealEd.
 *
 * @param	OutTargetList			The list to be filled with target handles.
 * @param	InOutTargetListSize		Contains the size of OutTargetList. When the function returns it contains the # of target handles copied over.
 */
void FAndroidSupport::GetMenuSelectedTargets(TARGETHANDLE* OutTargetList, int &InOutTargetListSize)
{
	int HandlesCopied = 0;

	for(int i = 0; HandlesCopied < InOutTargetListSize && i < (int)AndroidTargets.size(); ++i)
	{
		bool bIsChecked;
		bool bIsRadio;
		TARGETHANDLE Handle;

//		GetMenuItem(i + MI_FirstTarget, bIsChecked, bIsRadio, Handle);
		GetMenuItem(i, bIsChecked, bIsRadio, Handle);

		if(bIsChecked)
		{
			OutTargetList[HandlesCopied] = (TARGETHANDLE)i;
			++HandlesCopied;
		}
	}

	InOutTargetListSize = HandlesCopied;
}

/**
 * Retrieve the state of the console (running, not running, crashed, etc)
 *
 * @param Handle The handle of the console to retrieve the information from.
 *
 * @return the current state
 */
FConsoleSupport::ETargetState FAndroidSupport::GetTargetState(TARGETHANDLE Handle)
{
	FAndroidTarget* Target = GetTarget(Handle);

	if(!Target)
	{
		return TS_Unconnected;
	}

	return TS_Running;
}

/**
 * Allow for target to perform periodic operations
 */
void FAndroidSupport::Heartbeat(TARGETHANDLE Handle)
{
	FAndroidTarget* Target = GetTarget(Handle);
	if(!Target)
	{
		return;
	}

	Target->ProcessTextFromChildProcess();
}

/**
 * Turn an address into a symbol for callstack dumping
 *
 * @param Address Code address to be looked up
 * @param OutString Function name/symbol of the given address
 * @param OutLength Size of the OutString buffer
 */
void FAndroidSupport::ResolveAddressToString(unsigned int /*Address*/, wchar_t* OutString, int OutLength)
{
	OutString[0] = 0;
	OutLength = 0;
}

/**
 * Send a text command to the target
 *
 * @param Handle The handle of the console to retrieve the information from.
 * @param Command Command to send to the target
 */
void FAndroidSupport::SendConsoleCommand(TARGETHANDLE Handle, const wchar_t* Command)
{
	FAndroidTarget* Target = GetTarget(Handle);
	if (Target == NULL)
	{
		return;
	}

	if(!Command || wcslen(Command) == 0)
	{
		return;
	}

	// only setup the forwarding socket if we need it
	if (!Target->bHasSetupSocket)
	{
		// forward a port to the device (each device gets a different local port)
		unsigned short BoundPort = 13650 + (unsigned short)Handle;
		char CommandLine[1024];
		sprintf_s(CommandLine,"%s\\android-sdk-windows\\platform-tools\\adb.exe forward tcp:%d tcp:13650",
			AndroidRoot.c_str(), BoundPort);

		char Output[1024];
		DWORD RetVal;
		RunChildProcess(CommandLine, Output, 1023, &RetVal, 0);

		Target->CommandSocket.SetAttributes(FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Stream, FConsoleSocket::SP_TCP);
		Target->CommandSocket.CreateSocket();
		Target->CommandSocket.SetAddress("127.0.0.1");
		Target->CommandSocket.SetPort(BoundPort);
		if (!Target->CommandSocket.Connect())
		{
			return;
		}
		Target->CommandSocket.SetNonBlocking(true);

		Target->bHasSetupSocket = TRUE;
	}

	string CommandStr = ToString(Command);
	Target->CommandSocket.SendTo((char*)CommandStr.c_str(), (int)CommandStr.size());
}

/**
 * Returns the global sound cooker object.
 *
 * @return global sound cooker object, or NULL if none exists
 */
FConsoleSoundCooker* FAndroidSupport::GetGlobalSoundCooker()
{
	static FAndroidSoundCooker* GlobalSoundCooker = NULL;
	if( !GlobalSoundCooker )
	{
		GlobalSoundCooker = new FAndroidSoundCooker();
	}
	return GlobalSoundCooker;
}

/**
 * Returns the global texture cooker object.
 *
 * @return global sound cooker object, or NULL if none exists
 */
FConsoleTextureCooker* FAndroidSupport::GetGlobalTextureCooker()
{
	static FAndroidTextureCooker* GlobalTextureCooker = NULL;
	if( !GlobalTextureCooker )
	{
		GlobalTextureCooker = new FAndroidTextureCooker();
	}
	return GlobalTextureCooker;
}

/**
 * Returns the global skeletal mesh cooker object.
 *
 * @return global skeletal mesh cooker object, or NULL if none exists
 */
FConsoleSkeletalMeshCooker* FAndroidSupport::GetGlobalSkeletalMeshCooker()
{
	static FAndroidSkeletalMeshCooker* GlobalSkeletalMeshCooker = NULL;
	if( !GlobalSkeletalMeshCooker )
	{
		GlobalSkeletalMeshCooker = new FAndroidSkeletalMeshCooker();
	}
	return GlobalSkeletalMeshCooker;
}

/**
 * Returns the global static mesh cooker object.
 *
 * @return global static mesh cooker object, or NULL if none exists
 */
FConsoleStaticMeshCooker* FAndroidSupport::GetGlobalStaticMeshCooker()
{
	static FAndroidStaticMeshCooker* GlobalStaticMeshCooker = NULL;
	if( !GlobalStaticMeshCooker )
	{
		GlobalStaticMeshCooker = new FAndroidStaticMeshCooker();
	}
	return GlobalStaticMeshCooker;
}

FConsoleShaderPrecompiler* FAndroidSupport::GetGlobalShaderPrecompiler()
{
	static FAndroidShaderPrecompiler* GlobalShaderPrecompiler = NULL;
	if(!GlobalShaderPrecompiler)
	{
		GlobalShaderPrecompiler = new FAndroidShaderPrecompiler();
	}
	return GlobalShaderPrecompiler;
}

/**
 * Retrieves the handle of the default console.
 */
TARGETHANDLE FAndroidSupport::GetDefaultTarget()
{
	// no concept of a default, just use the first one if there at least one
	if (AndroidTargets.size() > 0)
	{
		return 0;
	}

	return INVALID_TARGETHANDLE;
}

/**
 * Retrieves a handle to each available target.
 *
 * @param	OutTargetList			An array to copy all of the target handles into.
 * @param	InOutTargetListSize		This variable needs to contain the size of OutTargetList. When the function returns it will contain the number of target handles copied into OutTargetList.
 */
int FAndroidSupport::GetTargets(TARGETHANDLE* OutTargetList)
{
	if( OutTargetList != NULL )
	{
		int Index;
		for (Index = 0; Index < (int)AndroidTargets.size(); ++Index)
		{
			OutTargetList[Index] = (TARGETHANDLE)Index;
		}
	}

	return (int)AndroidTargets.size();
}

/**
 * Sets the callback function for TTY output.
 *
 * @param	Callback	Pointer to a callback function or NULL if TTY output is to be disabled.
 * @param	Handle		The handle to the target that will register the callback.
 */
void FAndroidSupport::SetTTYCallback(TARGETHANDLE Handle, TTYEventCallbackPtr Callback)
{
	FAndroidTarget* Target = GetTarget(Handle);

	if (Target == NULL)
	{
		return;
	}

	// if we've already started logcat, nothing to do here
	if (Target->TxtHandleRead != INVALID_HANDLE_VALUE)
	{
		return;
	}

	// run the command (and avoid a window popping up)
	SECURITY_ATTRIBUTES SecurityAttr;
	SecurityAttr.nLength = sizeof(SecurityAttr);
	SecurityAttr.bInheritHandle = TRUE;
	SecurityAttr.lpSecurityDescriptor = NULL;

	CreatePipe(&Target->TxtHandleRead, &Target->TxtHandleWrite, &SecurityAttr, 0);
	SetHandleInformation(Target->TxtHandleRead, HANDLE_FLAG_INHERIT, 0);

	// set up process spawn structures
	STARTUPINFO StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	StartupInfo.hStdOutput = Target->TxtHandleWrite;
	StartupInfo.hStdError = Target->TxtHandleWrite;

	// clear the old logcat data
	DWORD RetVal;
	char Output[1024];
	char ClearCommandLine[1024];
	sprintf_s(ClearCommandLine, "%s\\android-sdk-windows\\platform-tools\\adb.exe logcat -c", AndroidRoot.c_str());
	RunChildProcess(ClearCommandLine, Output, 1023, &RetVal, 0);

	wchar_t CommandLine[1024];
	swprintf(CommandLine, ARRAY_COUNT(CommandLine),
		L"%S\\android-sdk-windows\\platform-tools\\adb.exe -s %ls logcat UE3:V DEBUG:V *:S",
		AndroidRoot.c_str(), Target->TargetName.c_str());

	// kick off the child process
	if (!CreateProcess(NULL, CommandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &StartupInfo, &Target->TxtProcess))
	{
		CloseHandle(Target->TxtHandleRead);
		CloseHandle(Target->TxtHandleWrite);
		Target->TxtHandleRead = INVALID_HANDLE_VALUE;
	}
	else
	{
		Target->TxtCallback = Callback;
	}
}

/**
 * Sets the callback function for handling crashes.
 *
 * @param	Callback	Pointer to a callback function or NULL if handling crashes is to be disabled.
 * @param	Handle		The handle to the target that will register the callback.
 */
void FAndroidSupport::SetCrashCallback(TARGETHANDLE Handle, CrashCallbackPtr Callback)
{
	FAndroidTarget* Target = GetTarget(Handle);

	if (Target == NULL)
	{
		return;
	}

	if(Target)
	{
		Target->CrashCallback = Callback;
	}
}

/**
 * Starts the (potentially async) process of enumerating all available targets
 */
void FAndroidSupport::EnumerateAvailableTargets()
{
	char CommandLine[1024];
	sprintf_s(CommandLine,"%s\\android-sdk-windows\\platform-tools\\adb.exe devices", AndroidRoot.c_str());
	char Output[1024];
	DWORD RetVal;
	RunChildProcess(CommandLine, Output, 1023, &RetVal, 0);

	if (RetVal != 0)
	{
		return;
	}

	// clear current list before regenerating
	AndroidTargets.clear();

	char* MainContext = NULL;
	char* Token = strtok_s(Output, "\r\n", &MainContext);
	while (Token)
	{
		if ((_strnicmp(Token, "* ", 2) == 0) ||
			(_strnicmp(Token, "List ", 5) == 0))
		{
			 // do nothing
		}
		else
		{
			// break apart the inner string
			char* InnerContext = NULL;
			char* ID = strtok_s(Token, "\t ", &InnerContext);

			// ID now points to the identifier of a target
			FAndroidTarget* Target = new FAndroidTarget(ID);
			AndroidTargets.push_back(Target);
		}

		// next line
		Token = strtok_s(NULL, "\r\n", &MainContext);
	}
}

/// <summary>
/// Assembly resolve method to pick correct StandaloneSymbolParser DLL
/// </summary>
System::Reflection::Assembly^ CurrentDomain_AssemblyResolve(System::Object^, System::ResolveEventArgs^ args)
{
	// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
	array<String^> ^AssemblyInfo = args->Name->Split(gcnew array<Char> { ',' });
	String^ AssemblyName = AssemblyInfo[0];

	if (AssemblyName->Equals("standalonesymbolparser", StringComparison::InvariantCultureIgnoreCase))
	{
		Char Buffer[MAX_PATH];
		GetModuleFileNameW(NULL, Buffer, MAX_PATH);
		String^ StartupPath = IO::Path::GetDirectoryName(gcnew String(Buffer));

		if (IntPtr::Size == 8)
		{
			AssemblyName = StartupPath + "\\Win64\\" + AssemblyName + ".dll";
		}
		else
		{
			AssemblyName = StartupPath + "\\Win32\\" + AssemblyName + ".dll";
		}

		//Debug.WriteLineIf( System.Diagnostics.Debugger.IsAttached, "Loading assembly: " + AssemblyName );

		return System::Reflection::Assembly::LoadFile(AssemblyName);
	}

	return nullptr;
}

CONSOLETOOLS_API FConsoleSupport* GetConsoleSupport(void* Module)
{
	static FAndroidSupport* AndroidSupport = NULL;
	if (AndroidSupport == NULL)
	{
		// add support for finding SSP DLL in win32/win64 directory
		AppDomain::CurrentDomain->AssemblyResolve += gcnew ResolveEventHandler(CurrentDomain_AssemblyResolve);

		try
		{
			AndroidSupport = new FAndroidSupport(Module);
		}
		catch( ... )
		{
			AndroidSupport = NULL;
		}
	}

	return AndroidSupport;
}
