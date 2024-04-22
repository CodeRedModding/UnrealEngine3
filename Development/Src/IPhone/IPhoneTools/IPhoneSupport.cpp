/*=============================================================================
	IPhoneTools/IPhoneSupport.cpp: IPhone platform support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "IPhoneSupport.h"

#include "..\..\Engine\Classes\PixelFormatEnum.uci"

#include <vcclr.h>
#include <Mmreg.h>

#undef GetEnvironmentVariable

using namespace std;
using namespace System;
using namespace System::Text;
using namespace System::IO;


#define COMMON_BUFFER_SIZE	(16 * 1024)

#define RGBA_ONLY 0

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

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
bool RunChildProcess(const wchar_t* CommandLine, const wchar_t* WorkingDirectory, char* Errors, int /*ErrorsSize*/, DWORD* ProcessReturnValue)
{
	// run the command (and avoid a window popping up)
// 	SECURITY_ATTRIBUTES SecurityAttr;
// 	SecurityAttr.nLength = sizeof(SecurityAttr);
// 	SecurityAttr.bInheritHandle = TRUE;
// 	SecurityAttr.lpSecurityDescriptor = NULL;
//
// 	HANDLE StdOutRead, StdOutWrite;
// 	CreatePipe(&StdOutRead, &StdOutWrite, &SecurityAttr, 0);
// 	SetHandleInformation(StdOutRead, HANDLE_FLAG_INHERIT, 0);

	// set up process spawn structures
	STARTUPINFO StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
// 	StartupInfo.dwFlags = STARTF_USESTDHANDLES;
// 	StartupInfo.hStdOutput = StdOutWrite;
// 	StartupInfo.hStdError = StdOutWrite;
	PROCESS_INFORMATION ProcInfo;

	wchar_t FinalCommandLine[COMMON_BUFFER_SIZE];
	swprintf(FinalCommandLine, ARRAY_COUNT(FinalCommandLine), L"cmd /C %s", CommandLine);

	// kick off the child process
	if (!CreateProcessW(NULL, (LPWSTR)FinalCommandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, (LPWSTR)WorkingDirectory, &StartupInfo, &ProcInfo))
	{
		sprintf(Errors, "\nFailed to start process '%S'\n", CommandLine);
		return false;
	}

	bool bProcessComplete = false;
	// read up to 64k of output at once (that would be crazy amount of error, but whatever)
// 	char Buffer[1024 * 64];
// 	DWORD BufferSize = sizeof(Buffer);
	Errors[0] = 0;

	// wait until the process is finished
	while (!bProcessComplete)
	{
		DWORD Reason = WaitForSingleObject(ProcInfo.hProcess, 1000);

// 		// See if we have some data to read
// 		DWORD SizeToRead;
// 		PeekNamedPipe(StdOutRead, NULL, 0, NULL, &SizeToRead, NULL);
// 		while (SizeToRead > 0)
// 		{
// 			// read some output
// 			DWORD SizeRead;
// 			ReadFile(StdOutRead, &Buffer, min(SizeToRead, BufferSize - 1), &SizeRead, NULL);
// 			Buffer[SizeRead] = 0;
//
// 			// decrease how much we need to read
// 			SizeToRead -= SizeRead;
//
// 			// append the output to the log
// 			strncpy(Errors, Buffer, ErrorsSize - 1);
// 		}

		// when the process signals, its done
		if (Reason != WAIT_TIMEOUT)
		{
			// break out of the loop
			bProcessComplete = true;
		}
	}

	// Get the return value
	DWORD ErrorCode;
	GetExitCodeProcess(ProcInfo.hProcess, &ErrorCode);

	// fill in the error code is needed
	if (ErrorCode != 0)
	{
		sprintf(Errors, "\nFailed while running '%S'\n", CommandLine);
	}

	// pass back the return code if desired
	if (ProcessReturnValue)
	{
		*ProcessReturnValue = ErrorCode;
	}

	// Close process and thread handles.
	CloseHandle(ProcInfo.hProcess);
	CloseHandle(ProcInfo.hThread);
// 	CloseHandle(StdOutRead);
// 	CloseHandle(StdOutWrite);

	return true;
}

/**
 * Runs a child process and spawns a progress bar wrapper (with a details button to see the actual output spew if desired)
 *
 * @param CommandLine The commandline of the child process to run
 * @param WorkingDirectory The working directory of the child process to run
 * @param RelativeBinariesDir The binaries directory of Unreal
 * @param Errors Output buffer for any errors
 * @param ErrorsSize Size of the Errors buffer
 * @param ProcessReturnValue Optional out value that the process returned
 *
 * @return TRUE if the process was run (even if the process failed)
 */
bool RunChildProcessWithProgressBar(const wchar_t* CommandLine, const wchar_t* WorkingDirectory, const wchar_t* RelativeBinariesDir, char* Errors, int ErrorsSize, DWORD* ProcessReturnValue)
{
	wchar_t NewCommandLine[COMMON_BUFFER_SIZE];
	wchar_t CurrentWorkingDirectory[COMMON_BUFFER_SIZE];

	// Make sure the working directory is a real string
	GetCurrentDirectoryW(COMMON_BUFFER_SIZE, CurrentWorkingDirectory);
	if (WorkingDirectory == NULL)
	{
		WorkingDirectory = CurrentWorkingDirectory;
	}

	// Construct a new command line to run the UnrealConsole progress bar wrapper
	swprintf(
		NewCommandLine,
		ARRAY_COUNT(NewCommandLine),
		L"UnrealConsole.exe -wrapexe -pwd %s -cwd %s %s",
		CurrentWorkingDirectory,
		WorkingDirectory,
		CommandLine
		);

	// Run UnrealConsole with it's working directory in the binaries directory
	// (it will use the value from -cwd as the working directory for the true target process)
	return RunChildProcess(NewCommandLine, RelativeBinariesDir, Errors, ErrorsSize, ProcessReturnValue);
}
struct RiffDataChunk
{
	long  ID;
	long  DataSize;
	const byte* Data;
};


#define ADPCM_100_QUALITY_COMPRESSION_BLOCK_SIZE 10			//8 Samples stored in buffer (compression of 1.6 to 1)
#define ADPCM_COMPRESSION_BLOCK_SIZE_RANGE 246				//(Max block calculated 256)500 samples stored in buffer (Compression of 3.9 to 1)
#define NUMBER_SAMPLES_TO_CALCULATE_STARTING_ADPCM_VALUES 6 //We need to ensure that the smallest block size has at least this number of samples or the delta calculation isn't correct
#define MAX_DELTA_GENERATION_SCANS 10
#define TOTAL_ERROR_THRESHOLD_FOR_DELTA_CALCULATION 50

// Magic values as specified by ADPCM standard
static int AdaptationTable[] = {
	230, 230, 230, 230,
	307, 409, 512, 614,
	768, 614, 512, 409,
	307, 230, 230, 230
} ;

#define NUM_COEFFICIENT_VALUES 7
static int AdaptationCoefficient1[] = {
	256,  512, 0, 192, 240,  460,  392
};

static int AdaptationCoefficient2[] = {
	0, -256, 0,  64,   0, -208, -232
};

struct FIPhoneSoundCooker : public FConsoleSoundCooker
{
	/**
	 * Constructor
	 */
	FIPhoneSoundCooker( void )
	{
	}

	/**
	 * Virtual destructor
	 */
	virtual ~FIPhoneSoundCooker( void )
	{
	}

private:


	void GenerateWaveFile(RiffDataChunk* RiffDataChunks, int RiffDataChunkCount)
	{
		const int RiffChunkHeaderSize = sizeof(int) * 2;

		//Determine the size of the wave file to be generated
		unsigned int RiffDataSize = 0;
		for (int Scan = 0; Scan < RiffDataChunkCount; ++Scan)
		{
			RiffDataSize += RiffDataChunks[Scan].DataSize;
		}
		RiffDataSize += RiffDataChunkCount * RiffChunkHeaderSize;

		RiffDataSize += sizeof(long);/*storage for WAVE tag*/

		//Malloc data for the wave file
		ConvertedDataSize = RiffDataSize + sizeof(long)/*RIFF tag*/ + sizeof(int)/*storage for chunk length*/ ;
		ConvertedData = (BYTE*)malloc(ConvertedDataSize);

		//Write out riff chunk (i.e. entire file)
		//ID
		int Offset = 0;
		long id = 'FFIR';
		memcpy(ConvertedData + Offset, &id, sizeof(id));
		Offset += sizeof(id);
		//Size
		memcpy(ConvertedData + Offset, &RiffDataSize, sizeof(int));
		Offset += sizeof(int);

		//----------------------------------------------------------------------
		//Start of riff Payload
		//----------------------------------------------------------------------
		id = 'EVAW';
		memcpy(ConvertedData + Offset, &id, sizeof(id));
		Offset += sizeof(id);

		//Write out each riff data chunk
		for (int Scan = 0; Scan < RiffDataChunkCount; ++Scan)
		{
			RiffDataChunk const& Chunk = RiffDataChunks[Scan];
			memcpy(ConvertedData + Offset, &Chunk.ID, sizeof(Chunk.ID));
			Offset += sizeof(Chunk.ID);
			//Payload size
			memcpy(ConvertedData + Offset, &Chunk.DataSize, sizeof(&Chunk.DataSize));
			Offset += sizeof(Chunk.DataSize);
			//Chunk payload
			memcpy(ConvertedData + Offset, Chunk.Data, Chunk.DataSize);
			Offset += Chunk.DataSize;
		}
	}
	/**
	 * Writes out a wav file from source data and format with no validation or error checking
	 *
	 * @param	Path			Path to write to
	 * @param	SrcBuffer		Pointer to source buffer
	 * @param	SrcBufferSize	Size in bytes of source buffer
	 * @param	WaveFormat		Pointer to platform specific wave format description
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
    */
	void GenerateConvertedADPCM(const BYTE* SrcBuffer, DWORD SrcBufferSize, WORD BlockSize, FSoundQualityInfo* QualityInfo)
	{
		// Set up the waveformat
		WAVEFORMATEXTENSIBLE FormatEx;
		ADPCMWAVEFORMAT* Format = (ADPCMWAVEFORMAT*)&FormatEx.Format;
		Format->wfx.nChannels = ( WORD )QualityInfo->NumChannels;
		Format->wfx.nSamplesPerSec = QualityInfo->SampleRate;
		Format->wfx.nBlockAlign = BlockSize;
		Format->wfx.wBitsPerSample = 4;
		Format->wfx.wFormatTag = WAVE_FORMAT_ADPCM;
		Format->wSamplesPerBlock = (((Format->wfx.nBlockAlign - 7/*preamble encoding data*/) * 8) / (Format->wfx.wBitsPerSample)) + 2/*Two preamble samples*/;
		Format->wfx.nAvgBytesPerSec = ((Format->wfx.nSamplesPerSec / Format->wSamplesPerBlock) * Format->wfx.nBlockAlign);
		Format->wNumCoef = 7;
		Format->wfx.cbSize = sizeof(ADPCMCOEFSET) * 7 + 4;
		short Coefs[] = { 256, 0, 512, -256, 0, 0, 192, 64, 240, 0, 460, -208, 392, -232 };
		memcpy(Format->aCoef, Coefs, sizeof(Coefs));

		RiffDataChunk RiffDataChunks[2];

		int HeaderSize = sizeof(WAVEFORMATEX) + Format->wfx.cbSize;
		RiffDataChunks[0].ID = ' tmf';
		RiffDataChunks[0].DataSize = HeaderSize;
		RiffDataChunks[0].Data = reinterpret_cast<byte*>(Format);

		RiffDataChunks[1].ID = 'atad';
		RiffDataChunks[1].DataSize = SrcBufferSize;
		RiffDataChunks[1].Data = SrcBuffer;

		GenerateWaveFile(RiffDataChunks, 2);
	}

	unsigned char EncodeADPCMSample(const signed short InputSample, const signed short PrevSample1, const signed short PrevSample2,  const int Coefficent1, const int Coefficent2, int& Delta, signed short& CalculatedSample)
	{
		// calculate the nibble for this sample
		int PredictedSample = (PrevSample1 * Coefficent1 + PrevSample2 * Coefficent2) / 256;
		int ErrorDelta = (InputSample - PredictedSample) / Delta;
		ErrorDelta = Math::Max(-8, ErrorDelta);
		ErrorDelta = Math::Min(7, ErrorDelta);

		// we need to store as an "unsigned" value, so keep the sign extended bits around
		signed char SmallDelta = (signed char)ErrorDelta;
		unsigned char UnsignedNibble = ((unsigned char&)SmallDelta) & 0xF;

		//Calculate the estimated sample generated from this encoding to be
		//used to help encode the next sample
		int IntCalculatedSample = PredictedSample + (Delta * ErrorDelta);
		IntCalculatedSample = Math::Max(-32768, IntCalculatedSample);
		IntCalculatedSample = Math::Min(32767, IntCalculatedSample);
		CalculatedSample = (signed short )IntCalculatedSample;

		// adapt the delta
		Delta = (Delta * AdaptationTable[UnsignedNibble]) / 256;
		Delta = Math::Max(16, Delta);

		return UnsignedNibble;
	}

	unsigned char CalculateBestCoefficientAndStartingDelta(int& StartingDelta, const signed short* InputSamples, int InputIndex, int TotalSamples, int SampleStride)
	{
		unsigned int TotalError = UInt32::MaxValue;
		unsigned int LargestError = 0;
		unsigned char CoefficentIndex = 0;

		for(unsigned char CoefficentScan = 0; CoefficentScan < NUM_COEFFICIENT_VALUES; ++CoefficentScan)
		{
			int TestDelta = AdaptationTable[0];
			int PrevTestDelta = TestDelta;
			unsigned int TestTotalError = 0;
			unsigned int TestLargestError = 0;
			for (int DeltaCycleScan = 0; DeltaCycleScan < MAX_DELTA_GENERATION_SCANS; ++DeltaCycleScan)
			{
				int TestInputIndex = InputIndex;
			
				signed short PrevSample2 = InputSamples[TestInputIndex];
				TestInputIndex += SampleStride;
				signed short PrevSample1 = InputSamples[TestInputIndex];
				TestInputIndex += SampleStride;

				TestTotalError = 0;
				TestLargestError = 0;
				signed short CalculatedSample;
			
				PrevTestDelta = TestDelta;
				for(int SampleScan = 0; SampleScan < NUMBER_SAMPLES_TO_CALCULATE_STARTING_ADPCM_VALUES; ++SampleScan)
				{
					signed short InputSample = TestInputIndex < TotalSamples ? InputSamples[TestInputIndex] : 0;
					TestInputIndex += SampleStride;
					EncodeADPCMSample(InputSample, PrevSample1, PrevSample2, AdaptationCoefficient1[CoefficentScan], AdaptationCoefficient2[CoefficentScan], TestDelta, CalculatedSample);

					unsigned int CurrentError = Math::Abs(CalculatedSample - InputSample);
					TestTotalError += CurrentError;
					if (CurrentError > TestLargestError)
					{
						TestLargestError = CurrentError;
					}

					PrevSample2 = PrevSample1;
					PrevSample1 = (signed short)CalculatedSample;
				}

				if(TestTotalError < TOTAL_ERROR_THRESHOLD_FOR_DELTA_CALCULATION)
				{
					break;
				}
			}

			if (TestTotalError < TotalError)
			{
				TotalError = TestTotalError;
				LargestError = TestLargestError;
				CoefficentIndex = CoefficentScan;
				StartingDelta = PrevTestDelta;
			}
		}

		return CoefficentIndex;
	}

	void EncodeADPCMBlock(const signed short* InputSamples, int& InputIndex, int SampleStride, int TotalSamples, int BlockSize, unsigned char CoefficientIndex, unsigned char* Block, int Delta, unsigned int& TotalError, unsigned int& LargestError)
	{

		// get the coefficients
		int Coefficent1 = AdaptationCoefficient1[CoefficientIndex];
		int Coefficent2 = AdaptationCoefficient2[CoefficientIndex];

		// get the first samples to prime the pump
		signed short PrevSample2 = InputSamples[InputIndex];
		InputIndex += SampleStride;
		signed short PrevSample1 = InputSamples[InputIndex];
		InputIndex += SampleStride;

		// fill out the block
		int BlockOffset = 0;
		Block[BlockOffset++] = CoefficientIndex;
		signed short ShortDelta = (signed short)Delta;
		Block[BlockOffset++] = ShortDelta & 0xFF;
		Block[BlockOffset++] = ShortDelta >> 8;
		//Write out the preample samples
		Block[BlockOffset++] = PrevSample1 & 0xFF;
		Block[BlockOffset++] = PrevSample1 >> 8;
		Block[BlockOffset++] = PrevSample2 & 0xFF;
		Block[BlockOffset++] = PrevSample2 >> 8;

		// now we need to calculate nibble pairs (we start at the byte after the preamble, go through
		// each byte in the block after that)
		for (int PairIndex = BlockOffset; PairIndex < BlockSize; PairIndex++)
		{
			unsigned char NibblePair = 0;

			for (int NibbleIndex = 0; NibbleIndex < 2; NibbleIndex++)
			{
				// get the next sample from the input
				signed short InputSample = InputIndex < TotalSamples ? InputSamples[InputIndex] : 0;
				InputIndex += SampleStride;

				signed short CalculatedSample = 0;
				unsigned char UnsignedNibble = EncodeADPCMSample(InputSample, PrevSample1, PrevSample2, Coefficent1, Coefficent2, Delta, CalculatedSample);
				if (NibbleIndex == 0)
				{
					NibblePair |= UnsignedNibble << 4;
				}
				else
				{
					NibblePair |= UnsignedNibble;
				}


				unsigned int CurrentError = Math::Abs(CalculatedSample - InputSample);
				TotalError += CurrentError;
				if (CurrentError > LargestError)
				{
					LargestError = CurrentError;
				}

				PrevSample2 = PrevSample1;
				PrevSample1 = (signed short)CalculatedSample;
			}

			// write out the 2 nibbles
			Block[PairIndex] = NibblePair;
		}
	}

	/**
	*
	* Encodes a single channel of audio into ADPCM
	*
	* @param   InputSamples		The input samples
	* @param   SampleStride		The stride between samples of the focus channel
	* @param   TotalSamples     The total number of samples in the InputSamples buffer
	* @param   NumBlocks		The number of blocks allocated pointed to by OutputData
	* @param   BlockSize		The size of each block
	* @param   OutputData		The allocated output data buffer for the encoded data
	*
	* @returns void
	*/
	void EncodeChannelToADPCM(const signed short* InputSamples, int SampleStride, int TotalSamples, int NumBlocks, int BlockSize, unsigned char* OutputData)
	{
		
		int InputIndex = 0;
		for (int BlockIndex = 0; BlockIndex < NumBlocks; BlockIndex++)
		{
			int Delta;
			unsigned char CoefficentIndex = CalculateBestCoefficientAndStartingDelta(Delta, InputSamples, InputIndex, TotalSamples, SampleStride);
			
			unsigned char* Block = OutputData + (BlockIndex * BlockSize);
			unsigned int TotalError = 0;
			unsigned int LargestError = 0;
			EncodeADPCMBlock(InputSamples, InputIndex, SampleStride, TotalSamples, BlockSize, CoefficentIndex, Block, Delta, TotalError, LargestError);
		}
	}

	bool CookPCMAudio(const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo )
	{

		// Set up the waveformat
		WAVEFORMATEXTENSIBLE FormatEx;
		WAVEFORMATEX* Format = (WAVEFORMATEX*)&FormatEx.Format;
		Format->nChannels = ( WORD )QualityInfo->NumChannels;
		Format->nSamplesPerSec = QualityInfo->SampleRate;
		Format->nBlockAlign = ( WORD )( Format->nChannels * sizeof( short ) );
		Format->nAvgBytesPerSec = Format->nBlockAlign * QualityInfo->SampleRate;
		Format->wBitsPerSample = 16;
		Format->wFormatTag = WAVE_FORMAT_PCM;

		RiffDataChunk RiffDataChunks[2];

		int HeaderSize = sizeof(WAVEFORMATEX);
		RiffDataChunks[0].ID = ' tmf';
		RiffDataChunks[0].DataSize = HeaderSize;
		RiffDataChunks[0].Data = reinterpret_cast<byte*>(Format);

		RiffDataChunks[1].ID = 'atad';
		RiffDataChunks[1].DataSize = QualityInfo->SampleDataSize;
		RiffDataChunks[1].Data = SrcBuffer;

		GenerateWaveFile(RiffDataChunks, 2);

		return true;
	}

	bool CookADPCMAudio(const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo )
	{

		bool bIsStereo = (QualityInfo->NumChannels == 2);

		// input source samples are 2 bytes
		int SourceNumSamplesPerChannel = QualityInfo->SampleDataSize / 2;
		//We are storing the encoded data for each channel separately 
		//so set calculate the number of blocks for a single channel
		if (bIsStereo)
		{
			SourceNumSamplesPerChannel /= 2;
		}

		//each sample is compressed into 4 bits
		int CompressedNumSamplesPerByte = 2;

		// break up the buffer into N samples (per channel)
		const int PreambleSamples = 2;
			//a quality of 100 encodes to pcm, but we want our full block range
			//expressed at the high end quality setting
		const float Quality = Math::Max(0.f, Math::Min(100.f,  static_cast<float>(QualityInfo->Quality + 1.0f)));
		float QualityBlockAdjustment = ADPCM_COMPRESSION_BLOCK_SIZE_RANGE;
		QualityBlockAdjustment *= 1.0f-(Quality/100.f);
		const WORD BlockSize = (ADPCM_100_QUALITY_COMPRESSION_BLOCK_SIZE + static_cast<WORD>(QualityBlockAdjustment)) ;
		const int PreambleSize = 7;
		int CompressedSamplesPerBlock = (BlockSize - PreambleSize) * CompressedNumSamplesPerByte + PreambleSamples;
		int NumBlocksPerChannel = SourceNumSamplesPerChannel / CompressedSamplesPerBlock;
		//If our storage didn't exactly line up with the number of samples we need an extra block
		if (NumBlocksPerChannel * CompressedSamplesPerBlock != SourceNumSamplesPerChannel)
		{
			NumBlocksPerChannel++;
		}

		// initialize the output
		int OutputDataSize = NumBlocksPerChannel * BlockSize;
		if (bIsStereo)
		{
			OutputDataSize *= 2;
		}

		unsigned char* OutputData = (unsigned char*)malloc(OutputDataSize);
		memset(OutputData, 0, OutputDataSize);

		signed short* InputSamples = (signed short*)SrcBuffer;
		if (bIsStereo)
		{
			unsigned char* ChannelOutputData = OutputData;
			//Left and right channel data is interlaced. 
			const int StereoSampleStride = 2;
			//First channel
			EncodeChannelToADPCM(InputSamples, StereoSampleStride, SourceNumSamplesPerChannel * 2/*num channels*/, NumBlocksPerChannel, BlockSize, ChannelOutputData);
			//Next channel
			InputSamples++;
			//Set output pointer past the blocks for the first channel
			ChannelOutputData += NumBlocksPerChannel * BlockSize;
			EncodeChannelToADPCM(InputSamples, StereoSampleStride, SourceNumSamplesPerChannel * 2/*num channels*/, NumBlocksPerChannel, BlockSize, ChannelOutputData);
		}
		else
		{
			const int MonoSampleStride = 1;
			EncodeChannelToADPCM(InputSamples, MonoSampleStride, SourceNumSamplesPerChannel, NumBlocksPerChannel, BlockSize, OutputData);
		}

		GenerateConvertedADPCM(OutputData, OutputDataSize, BlockSize, QualityInfo);

		return true;
	}

public:
	/**
	 * Cooks the source data for the platform and stores the cooked data internally.
	 *
	 * @param	SrcBuffer		Pointer to source buffer
	 * @param	QualityInfo		All the information the compressor needs to compress the audio
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	virtual bool Cook( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo )
	{
		if (QualityInfo->Quality >= 100)
		{
			return CookPCMAudio(SrcBuffer, QualityInfo);
		}
		else
		{
			return CookADPCMAudio(SrcBuffer, QualityInfo);
		}
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
		return ConvertedDataSize;
	}

	/**
	 * Copies the cooked ata into the passed in buffer of at least size GetCookedDataSize()
	 *
	 * @param CookedData		Buffer of at least GetCookedDataSize() bytes to copy data to.
	 */
	virtual void GetCookedData( BYTE* CookedData )
	{
		memcpy(CookedData, ConvertedData, ConvertedDataSize);
		free(ConvertedData);
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
		return ErrorString.c_str();
	}

private:

	

	/** The compressed audio data to return to the cooker */
	unsigned char* ConvertedData;

	/** The size of ConvertedData */
	UINT ConvertedDataSize;

	/** String used to report error */
	wstring ErrorString;
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

struct FIPhoneTextureCooker : public FConsoleTextureCooker
{
	/**
	 * Constructor
	 */
	FIPhoneTextureCooker()
	{
	}

	/**
	 * Destructor
	 */
	virtual ~FIPhoneTextureCooker()
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
				FPixelFormat Format = { 8,	4,	1,	8,	'1TXD',	false };
				return Format;
			}
		case PF_DXT3:
			{
				FPixelFormat Format = { 4,	4,	1,	8,	'3TXD',	false };
				return Format;
			}
		case PF_DXT5:
			{
				FPixelFormat Format = { 4,	4,	1,	8,	'5TXD',	false };
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

		// cache if it's PVR or not
		bIsPVRFormat = PixelFormat.DDSFourCC != 0;
	}

	/**
	 * Gets the pitch in bytes for a row of the texture at the given mip index
	 * @param	Level		Miplevel to query size for
	 */
	UINT GetNonPVRPitch( DWORD Level )
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
	UINT GetNonPVRNumRows( DWORD Level )
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
	UINT GetNonPVRMipSize( UINT Level )
	{
		// calculate the size in bytes
		return GetNonPVRPitch(Level) * GetNonPVRNumRows(Level);
	}

	/**
	 * Returns the DXT size
	 *
	 * @param	Level		Miplevel to query size for
	 * @return	Returns	the size in bytes of Miplevel 'Level'
	 */
	UINT GetPVRMipSize( UINT Level )
	{
		UINT MipSizeX = max(SizeX >> Level, 1);
		UINT MipSizeY = max(SizeY >> Level, 1);

		UINT BlocksX = max(MipSizeX / PixelFormat.BlockSizeX, 2);
		UINT BlocksY = max(MipSizeY / PixelFormat.BlockSizeY, 2);

		UINT DataSize = BlocksX * BlocksY * PixelFormat.BlockBytes;

		return DataSize;
	}

	/**
	 * Returns the DXT size
	 *
	 * @param	Level		Miplevel to query size for
	 * @return	Returns	the size in bytes of Miplevel 'Level'
	 */
	virtual UINT GetMipSize( UINT Level )
	{
		// calculate the size in bytes
		return bIsPVRFormat ? GetPVRMipSize(Level) : GetNonPVRMipSize(Level);
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
		UINT NumBytes = GetNonPVRMipSize(Level);
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
	UINT Format;
	UINT SizeX;
	UINT SizeY;
	BOOL bIsPVRFormat;
	FPixelFormat PixelFormat;
};


struct FIPhoneSkeletalMeshCooker : public FConsoleSkeletalMeshCooker
{
	void Init(void)
	{

	}

	virtual void CookMeshElement(const FSkeletalMeshFragmentCookInfo& ElementInfo, FSkeletalMeshFragmentCookOutputInfo& OutInfo)
	{
		// no optimization needed, just copy the data over
		memcpy(OutInfo.NewIndices, ElementInfo.Indices, ElementInfo.NumTriangles * 3 * sizeof(WORD) );
	}
};

/**
* IPhone version of static mesh cooker
*/
struct FIPhoneStaticMeshCooker : public FConsoleStaticMeshCooker
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

/** IPhone platform shader precompiler */
struct FIPhoneShaderPrecompiler : public FConsoleShaderPrecompiler
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
		bool /*bPreprocessed*/
		)
	{
		return false;
	}
};

struct FRunGameThreadData
{
	class FIPhoneSupport* SupportObject;
	wstring URL;
	wstring MapName;
	wstring Configuration;
	wstring GameName;
};

FIPhoneSupport::FIPhoneSupport(void* InModule)
{
	Module = InModule;
}

/** Initialize the DLL with some information about the game/editor
 *
 * @param	GameName		The name of the current game ("ExampleGame", "UTGame", etc)
 * @param	Configuration	The name of the configuration to run ("Debug", "Release", etc)
 */
void FIPhoneSupport::Initialize(const wchar_t* InGameName, const wchar_t* InConfiguration)
{
	// cache the parameters
	GameName = InGameName;
	Configuration = InConfiguration;

	RelativeBinariesDir = L"";

	wchar_t CurDir[COMMON_BUFFER_SIZE];
	GetCurrentDirectoryW(COMMON_BUFFER_SIZE, CurDir);
	if (_wcsicmp(CurDir + wcslen(CurDir) - 8, L"Binaries") != 0)
	{
		RelativeBinariesDir = L"..\\";
	}
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
unsigned int FIPhoneSupport::GetIPAddress(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	if (!Target)
	{
		return 0;
	}

	return Target->GetRemoteAddress().sin_addr.s_addr;
}

/**
 * Get the name of the specified target
 *
 * @param	Handle The handle of the console to retrieve the information from.
 * @return Name of the target, or NULL if the Index is out of bounds
 */
const wchar_t* FIPhoneSupport::GetTargetName(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	
	if(!Target)
	{
		return NULL;
	}

	return Target->Name.c_str();
}

const wchar_t* FIPhoneSupport::GetTargetGameName(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	
	if(!Target)
	{
		return NULL;
	}

	return Target->GameName.c_str();
}

const wchar_t* FIPhoneSupport::GetTargetGameType(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	
	if(!Target)
	{
		return NULL;
	}

	return Target->GameTypeName.c_str();
}

/**
 * Returns the type of the specified target.
 */
FConsoleSupport::ETargetType FIPhoneSupport::GetTargetType(TARGETHANDLE Handle)
{
	CIPhoneTarget* Target = NetworkManager.ConvertTarget(Handle);

	if( Target)
	{
		if( !_wcsicmp( Target->OSVersion.c_str(), L"v3.2" ) )
		{
			return TART_IOS32;
		}
		else if( !_wcsnicmp( Target->OSVersion.c_str(), L"v3.", 3 ) )
		{
			return TART_IOS3x;
		}
		else if( !_wcsicmp( Target->OSVersion.c_str(), L"v4.0" ) )
		{
			return TART_IOS40;
		}
		else if( !_wcsicmp( Target->OSVersion.c_str(), L"v4.1" ) )
		{
			return TART_IOS41;
		}
		else if( !_wcsicmp( Target->OSVersion.c_str(), L"<remote>" ) )
		{
			return TART_Remote;
		}
	}

	return TART_Unknown ;
}

/**
 * Open an internal connection to a target. This is used so that each operation is "atomic" in
 * that connection failures can quickly be 'remembered' for one "sync", etc operation, so
 * that a connection is attempted for each file in the sync operation...
 * For the next operation, the connection can try to be opened again.
 *
 * @param Handle The handle of the console to connect to.
 *
 * @return false for failure.
 */
bool FIPhoneSupport::ConnectToTarget(TARGETHANDLE Handle)
{
	return NetworkManager.ConnectToTarget(Handle);
}

/**
 * Called after an atomic operation to close any open connections
 *
 * @param Handle The handle of the console to disconnect.
 */
void FIPhoneSupport::DisconnectFromTarget(TARGETHANDLE Handle)
{
	NetworkManager.DisconnectTarget(Handle);
}

/**
 * Exits the current instance of the game and reboots the target if appropriate. Must be implemented
 *
 * @param Handle The handle of the console to reset
 *
 * @return true if successful
 */
bool FIPhoneSupport::ResetTargetToShell(TARGETHANDLE, bool)
{
	return false;
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
bool FIPhoneSupport::RunGameOnTarget(TARGETHANDLE, const wchar_t* GameName, const wchar_t* Configuration, const wchar_t*, const wchar_t*)
{
	wchar_t CommandLine[COMMON_BUFFER_SIZE];
	wchar_t WorkingDir[COMMON_BUFFER_SIZE];
	char ErrorsRaw[COMMON_BUFFER_SIZE];
	DWORD ReturnValue;

	swprintf(
		CommandLine,
		ARRAY_COUNT(CommandLine),
		L"%s\\IPhone\\iPhonePackager.exe deploy %s %s -interactive",
		RelativeBinariesDir.c_str(),
		GameName,
		Configuration
	);
	swprintf(
		WorkingDir,
		ARRAY_COUNT(WorkingDir),
		L"%s\\IPhone",
		RelativeBinariesDir.c_str()
	);

	// IPP expects the working directory to be it's own directory
	ErrorsRaw[0] = 0;
	RunChildProcessWithProgressBar(CommandLine, WorkingDir, RelativeBinariesDir.c_str(), ErrorsRaw, sizeof(ErrorsRaw) - 1, &ReturnValue);
	if (ReturnValue == 0)
	{
		MessageBox(NULL, L"Deployed successfully, you can now start the application on your iOS device", L"Finished installing on iOS device", MB_ICONINFORMATION | MB_SETFOREGROUND);
	}

	return (ReturnValue == 0);
}

/**
 * This function is run on a separate thread for cooking, copying, and running an autosaved level.
 *
 * @param	Data	A pointer to data passed into the thread providing it with extra data needed to do its job.
 */
void __cdecl FIPhoneSupport::RunGameThread(void* Data)
{
	FRunGameThreadData *ThreadData = (FRunGameThreadData*)Data;

	wchar_t* URLCopy = _wcsdup(ThreadData->URL.c_str());

	bool bUseDebugExecutable = false;

	wchar_t CommandLine[COMMON_BUFFER_SIZE];
	wchar_t WorkingDir[COMMON_BUFFER_SIZE];
	wchar_t Errors[COMMON_BUFFER_SIZE];
	char ErrorsRaw[COMMON_BUFFER_SIZE];
	DWORD ReturnValue;

	// Cook the map
	{
		// A special workaround for UDKM (similar to the one in WindowsSupport for UDK)
		// Use the module name (the running exe of the Editor) as the binary to use for cooking
		WCHAR ModuleNameString[MAX_PATH];

		// Just get the executable name, for UDK purposes
		GetModuleFileNameW( NULL, ModuleNameString, MAX_PATH );
		_wcslwr( ModuleNameString );
		wstring ModuleName = ModuleNameString;

		// Make sure we launch the command line friendlier shell
		size_t Position = ModuleName.find( L".exe" );
		if( Position != wstring::npos )
		{
			ModuleName.replace( Position, 4, L".com" );
		}

		swprintf(
			CommandLine,
			ARRAY_COUNT(CommandLine),
			L"%s cookpackages %s -full -platform=iphone",
			ModuleName.c_str(),
			ThreadData->MapName.c_str()
		);

		// Use the working directory of the current process for this task
		ErrorsRaw[0] = 0;
		RunChildProcessWithProgressBar(CommandLine, NULL, ThreadData->SupportObject->RelativeBinariesDir.c_str(), ErrorsRaw, sizeof(ErrorsRaw) - 1, &ReturnValue);
		if (ReturnValue != 0)
		{
			swprintf(Errors, ARRAY_COUNT(Errors), L"%S", ErrorsRaw);
			MessageBox(NULL, Errors, L"Cooking failed", MB_ICONSTOP | MB_SETFOREGROUND);
			return;
		}

	}

	// Update the TOC
	{
		swprintf(
			CommandLine,
			ARRAY_COUNT(CommandLine),
			L"%sCookerSync %s -p IPhone -nd",
			ThreadData->SupportObject->RelativeBinariesDir.c_str(),
			ThreadData->GameName.c_str()
		);

		// Use the working directory of the current process for this task
		ErrorsRaw[0] = 0;
		RunChildProcessWithProgressBar(CommandLine, NULL, ThreadData->SupportObject->RelativeBinariesDir.c_str(), ErrorsRaw, sizeof(ErrorsRaw) - 1, &ReturnValue);
		if (ReturnValue != 0)
		{
			swprintf(Errors, ARRAY_COUNT(Errors), L"%S", ErrorsRaw);
			MessageBox(NULL, Errors, L"CookerSync failed", MB_ICONSTOP | MB_SETFOREGROUND);
			return;
		}
	}

	// Save out the command line
	{
		wchar_t CommandLineFilePath[COMMON_BUFFER_SIZE];
		swprintf(
			CommandLineFilePath,
			ARRAY_COUNT(CommandLineFilePath),
			L"%s..\\%s\\CookedIPhone\\UE3CommandLine.txt",
			ThreadData->SupportObject->RelativeBinariesDir.c_str(),
			ThreadData->GameName.c_str()
		);

		// Open the command line file
		FILE* CommandLineFile = _wfopen(CommandLineFilePath, L"w");

		// Write out the URL to the command line
		fwprintf(CommandLineFile, URLCopy);
		fclose(CommandLineFile);
	}

	// Package up the application
	{
		swprintf(
			CommandLine,
			ARRAY_COUNT(CommandLine),
			L"%s\\IPhone\\iPhonePackager.exe RepackageIPA %s %s -interactive -sign -compress=none",
			ThreadData->SupportObject->RelativeBinariesDir.c_str(),
			ThreadData->GameName.c_str(),
			ThreadData->Configuration.c_str()
		);
		swprintf(
			WorkingDir,
			ARRAY_COUNT(WorkingDir),
			L"%s\\IPhone",
			ThreadData->SupportObject->RelativeBinariesDir.c_str()
		);

		// ITP expects the working directory to be it's own directory
		ErrorsRaw[0] = 0;
		RunChildProcessWithProgressBar(CommandLine, WorkingDir, ThreadData->SupportObject->RelativeBinariesDir.c_str(), ErrorsRaw, sizeof(ErrorsRaw) - 1, &ReturnValue);
		if (ReturnValue != 0)
		{
			swprintf(Errors, ARRAY_COUNT(Errors), L"%S", ErrorsRaw);
			MessageBox(NULL, Errors, L"Packaging failed", MB_ICONSTOP | MB_SETFOREGROUND);
			return;
		}
	}

	{
		ThreadData->SupportObject->RunGameOnTarget(0, ThreadData->GameName.c_str(), bUseDebugExecutable ? L"Debug" : L"Release", URLCopy, L"");
	}

	free(URLCopy);
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
bool FIPhoneSupport::RunGame(TARGETHANDLE* /*TargetList*/, int /*NumTargets*/, const wchar_t* MapName, const wchar_t* URL, wchar_t* /*OutputConsoleCommand*/, int /*CommandBufferSize*/)
{
	FRunGameThreadData *Data = new FRunGameThreadData();

	Data->SupportObject = this;
	Data->MapName = MapName;
	Data->GameName = GameName;
	Data->Configuration = Configuration;
	Data->URL = URL;

	// Do all cooking, copying, and running on a separate thread so the UI doesn't hang.
	_beginthread(&FIPhoneSupport::RunGameThread, 0, Data);

	return true;
}

/**
 * Return the number of console-specific menu items this platform wants to add to the main
 * menu in UnrealEd.
 *
 * @return	The number of menu items for this console
 */
int FIPhoneSupport::GetNumMenuItems()
{
	return 1;
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
const wchar_t* FIPhoneSupport::GetMenuItem(int /*Index*/, bool& /*bIsChecked*/, bool& /*bIsRadio*/, TARGETHANDLE& /*OutHandle*/)
{
// 		// Just have a default target for now, always checked
// 		assert(Index == 0);
// 		bIsChecked = true;
// 		bIsRadio = false;
//
// 		// Get the first target
// 		NetworkManager.GetTargets(&OutHandle);
//
// 		TargetPtr Target = NetworkManager.GetTarget(OutHandle);
// 		return Target->Name.c_str();

	// Until we have control over real targets, simply say we have one
	return L"iOS Device";
}
/**
 * Gets a list of targets that have been selected via menus in UnrealEd.
 *
 * @param	OutTargetList			The list to be filled with target handles.
 * @param	InOutTargetListSize		Contains the size of OutTargetList. When the function returns it contains the # of target handles copied over.
 */
void FIPhoneSupport::GetMenuSelectedTargets(TARGETHANDLE* OutTargetList, int &InOutTargetListSize)
{
// 		TARGETHANDLE Handles[1];
// 		INT NumHandles = 1;
//
// 		// Get the first target
// 		NetworkManager.GetTargets(Handles);
//
// 		InOutTargetListSize = 1;
// 		OutTargetList[0] = Handles[0];

	// Until we have control over real targets, simply say we have one
	InOutTargetListSize = 1;
	OutTargetList[0] = NULL;
}


/**
 * Retrieve the state of the console (running, not running, crashed, etc)
 *
 * @param Handle The handle of the console to retrieve the information from.
 *
 * @return the current state
 */
FConsoleSupport::ETargetState FIPhoneSupport::GetTargetState(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);

	if(!Target)
	{
		return TS_Unconnected;
	}

	return TS_Running;
}

/**
 * Allow for target to perform periodic operations
 */
void FIPhoneSupport::Heartbeat(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	if (Target)
	{
		Target->Tick();
	}
}

/**
 * Turn an address into a symbol for callstack dumping
 *
 * @param Address Code address to be looked up
 * @param OutString Function name/symbol of the given address
 * @param OutLength Size of the OutString buffer
 */
void FIPhoneSupport::ResolveAddressToString(unsigned int /*Address*/, wchar_t* OutString, int OutLength)
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
void FIPhoneSupport::SendConsoleCommand(TARGETHANDLE Handle, const wchar_t* Command)
{
	if(!Command || wcslen(Command) == 0)
	{
		return;
	}

	NetworkManager.SendToConsole(Handle, Command);
}

/**
 * Returns the global sound cooker object.
 *
 * @return global sound cooker object, or NULL if none exists
 */
FConsoleSoundCooker* FIPhoneSupport::GetGlobalSoundCooker()
{
	static FIPhoneSoundCooker* GlobalSoundCooker = NULL;
	if( !GlobalSoundCooker )
	{
		GlobalSoundCooker = new FIPhoneSoundCooker();
	}
	return GlobalSoundCooker;
}

/**
 * Returns the global texture cooker object.
 *
 * @return global sound cooker object, or NULL if none exists
 */
FConsoleTextureCooker* FIPhoneSupport::GetGlobalTextureCooker()
{
	static FIPhoneTextureCooker* GlobalTextureCooker = NULL;
	if( !GlobalTextureCooker )
	{
		GlobalTextureCooker = new FIPhoneTextureCooker();
	}
	return GlobalTextureCooker;
}

/**
 * Returns the global skeletal mesh cooker object.
 *
 * @return global skeletal mesh cooker object, or NULL if none exists
 */
FConsoleSkeletalMeshCooker* FIPhoneSupport::GetGlobalSkeletalMeshCooker()
{
	static FIPhoneSkeletalMeshCooker* GlobalSkeletalMeshCooker = NULL;
	if( !GlobalSkeletalMeshCooker )
	{
		GlobalSkeletalMeshCooker = new FIPhoneSkeletalMeshCooker();
	}
	return GlobalSkeletalMeshCooker;
}

/**
 * Returns the global static mesh cooker object.
 *
 * @return global static mesh cooker object, or NULL if none exists
 */
FConsoleStaticMeshCooker* FIPhoneSupport::GetGlobalStaticMeshCooker()
{
	static FIPhoneStaticMeshCooker* GlobalStaticMeshCooker = NULL;
	if( !GlobalStaticMeshCooker )
	{
		GlobalStaticMeshCooker = new FIPhoneStaticMeshCooker();
	}
	return GlobalStaticMeshCooker;
}

FConsoleShaderPrecompiler* FIPhoneSupport::GetGlobalShaderPrecompiler()
{
	static FIPhoneShaderPrecompiler* GlobalShaderPrecompiler = NULL;
	if(!GlobalShaderPrecompiler)
	{
		GlobalShaderPrecompiler = new FIPhoneShaderPrecompiler();
	}
	return GlobalShaderPrecompiler;
}


/**
 * Retrieves the handle of the default console.
 */
TARGETHANDLE FIPhoneSupport::GetDefaultTarget()
{
	TargetPtr Target = NetworkManager.GetDefaultTarget();

	if(Target)
	{
		return Target.GetHandle();
	}

	return INVALID_TARGETHANDLE;
}

/**
 * Retrieves a handle to each available target.
 *
 * @param	OutTargetList			An array to copy all of the target handles into.
 */
int FIPhoneSupport::GetTargets(TARGETHANDLE* OutTargetList)
{
	return NetworkManager.GetTargets(OutTargetList);
}

/**
 * Sets the callback function for TTY output.
 *
 * @param	Callback	Pointer to a callback function or NULL if TTY output is to be disabled.
 * @param	Handle		The handle to the target that will register the callback.
 */
void FIPhoneSupport::SetTTYCallback(TARGETHANDLE Handle, TTYEventCallbackPtr Callback)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);

	if(Target)
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
void FIPhoneSupport::SetCrashCallback(TARGETHANDLE Handle, CrashCallbackPtr Callback)
{
	CIPhoneTarget* Target = NetworkManager.ConvertTarget(Handle);
	if (Target)
	{
		Target->CrashCallback = Callback;
	}
}

/**
 * Starts the (potentially async) process of enumerating all available targets
 */
void FIPhoneSupport::EnumerateAvailableTargets()
{
	// Search for available targets
	NetworkManager.Initialize();
	NetworkManager.DetermineTargets();
}

/**
 * Forces a stub target to be created to await connection
 *
 * @Param TargetAddress IP of target to add
 *
 * @returns Handle of new stub target
 */
TARGETHANDLE FIPhoneSupport::ForceAddTarget( const wchar_t* TargetAddress )
{
	return( NetworkManager.ForceAddTarget( TargetAddress ) );
}

CONSOLETOOLS_API FConsoleSupport* GetConsoleSupport(void* Module)
{
	static FIPhoneSupport* IPhoneSupport = NULL;
	if( IPhoneSupport == NULL )
	{
		try
		{
			IPhoneSupport = new FIPhoneSupport(Module);
		}
		catch( ... )
		{
			IPhoneSupport = NULL;
		}
	}

	return IPhoneSupport;
}
