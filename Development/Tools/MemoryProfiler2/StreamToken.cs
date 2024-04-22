/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Collections.Generic;
using System.Text;
using System.Reflection;
using System.Diagnostics;

namespace MemoryProfiler2
{
	/// <summary> The lower 2 bits of a pointer are piggy-bagged to store what kind of data follows it. This enum lists the possible types. </summary>
    public enum EProfilingPayloadType
    {
        TYPE_Malloc		= 0,
        TYPE_Free		= 1,
        TYPE_Realloc	= 2,
        TYPE_Other		= 3,
        // Don't add more than 4 values - we only have 2 bits to store this.
    }

	/// <summary> 
	///	The the case of TYPE_Other, this enum determines the subtype of the token.
	/// Mirrored in FMallocProfiler.h
	/// </summary>
	public enum EProfilingPayloadSubType
	{
		// Core marker types

		/// <summary> Marker used to determine when malloc profiler stream has ended. </summary> 
		SUBTYPE_EndOfStreamMarker = 0,

		/// <summary> Marker used to determine when we need to read data from the next file. </summary> 
		SUBTYPE_EndOfFileMarker = 1,

		/// <summary> Marker used to determine when a snapshot has been added. </summary> 
		SUBTYPE_SnapshotMarker = 2,

		/// <summary> Marker used to determine when a new frame has started. </summary> 
		SUBTYPE_FrameTimeMarker = 3,

		/// <summary> Not used. Only for backward compatibility. Use a new snapshot marker instead. </summary> 
		SUBTYPE_TextMarker = 4,

		// Marker types for periodic non-GMalloc memory status updates. Only for backward compatibility, replaced with SUBTYPE_MemoryAllocationStats

		/// <summary> Marker used to store the total amount of memory used by the game. </summary> 
		SUBTYPE_TotalUsed = 5,

		/// </summary> Marker used to store the total amount of memory allocated from the OS. </summary> 
		SUBTYPE_TotalAllocated = 6,

		/// <summary> Marker used to store the allocated in use by the application virtual memory. </summary> 
		SUBTYPE_CPUUsed = 7,

		/// <summary> Marker used to store the allocated from the OS/allocator, but not used by the application. </summary> 
		SUBTYPE_CPUSlack = 8,

		/// <summary> Marker used to store the alignment waste from a pooled allocator plus book keeping overhead. </summary> 
		SUBTYPE_CPUWaste = 9,

		/// <summary> Marker used to store the allocated in use by the application physical memory. </summary> 
		SUBTYPE_GPUUsed = 10,

		/// <summary> Marker used to store the allocated from the OS, but not used by the application. </summary> 
		SUBTYPE_GPUSlack = 11,

		/// <summary> Marker used to store the alignment waste from a pooled allocator plus book keeping overhead. </summary> 
		SUBTYPE_GPUWaste = 12,

		/// <summary> Marker used to store the overhead of the operating system. </summary> 
		SUBTYPE_OSOverhead = 13,

		/// <summary> Marker used to store the size of loaded executable, stack, static, and global object size. </summary> 
		SUBTYPE_ImageSize = 14,

		/// Version 3
		// Marker types for automatic snapshots.

		/// <summary> Marker used to determine when engine has started the cleaning process before loading a new level. </summary> 
		SUBTYPE_SnapshotMarker_LoadMap_Start = 21,

		/// <summary> Marker used to determine when a new level has started loading. </summary> 
		SUBTYPE_SnapshotMarker_LoadMap_Mid = 22,

		/// <summary> Marker used to determine when a new level has been loaded. </summary> 
		SUBTYPE_SnapshotMarker_LoadMap_End = 23,

		/// <summary> Marker used to determine when garbage collection has started. </summary> 
		SUBTYPE_SnapshotMarker_GC_Start = 24,

		/// <summary> Marker used to determine when garbage collection has finished. </summary> 
		SUBTYPE_SnapshotMarker_GC_End = 25,

		/// <summary> Marker used to determine when a new streaming level has been requested to load. </summary> 
		SUBTYPE_SnapshotMarker_LevelStream_Start = 26,

		/// <summary> Marker used to determine when a previously streamed level has been made visible. </summary> 
		SUBTYPE_SnapshotMarker_LevelStream_End = 27,

		/// <summary> Marker used to store a generic malloc statistics. @see FMemoryAllocationStats. </summary> 
		SUBTYPE_MemoryAllocationStats = 31,

		/// <summary> Start licensee-specific subtypes from here. </summary> 
		SUBTYPE_LicenseeBase = 50,

		/// <summary> Unknown the subtype of the token. </summary> 
		SUBTYPE_Unknown,
	};

	/// <summary> Enumeration of all memory allocation statistics types. </summary>
	public enum FMemoryAllocationTypes
	{
		/// <summary> The total amount of memory used by the game. </summary>
		TotalUsed,
		/// <summary> The total amount of memory allocated from the OS. </summary>
		TotalAllocated,

		// Virtual memory for Xbox and PC / Main memory for PS3 (tracked in the allocators) Host is not included ??

		/// <summary> The allocated in use by the application virtual memory. </summary>
		CPUUsed,
		/// <summary> The allocated from the OS/allocator, but not used by the application. </summary>
		CPUSlack,
		/// <summary> Alignment waste from a pooled allocator plus book keeping overhead. </summary>
		CPUWaste,
		/// <summary> The amount of free memory before the first malloc has been done. (PS3 only). </summary>
		CPUAvailable,

		// Physical memory for Xbox and PC / Local memory for PS3 (tracked in the allocators)

		/// <summary> The allocated in use by the application physical memory. </summary>
		GPUUsed,
		/// <summary> The allocated from the OS, but not used by the application. </summary>
		GPUSlack,
		/// <summary> Alignment waste from a pooled allocator plus book keeping overhead. </summary>
		GPUWaste,
		/// <summary> The total amount of memory available for the allocator. (PS3 only) </summary>
		GPUAvailable,

		/// <summary> Used memory as reported by the operating system. (Xbox360, PS3 only) </summary>
		OSReportedUsed,
		/// <summary> Free memory as reported by the operating system. (Xbox360, PS3 only) </summary>
		OSReportedFree,
		/// <summary> The overhead of the operating system. (Xbox360 only) </summary>
		OSOverhead,
		/// <summary> Size of loaded executable, stack, static, and global object size. (Xbox360, PS3 only) </summary>
		ImageSize,

		/// <summary> Host memory in use by the application. (PS3 only) </summary>
		HostUsed,
		/// <summary> Host memory allocated, but not used by the application. (PS3 only) </summary>
		HostSlack,
		/// <summary> Host memory wasted due to allocations' alignment. (PS3 only) </summary>
		HostWaste,
		/// <summary> The total amount of host memory that has been allocated. (PS3 only) </summary>
		HostAvailable,

		/// <summary> Size of allocated memory in the texture pool. </summary>
		AllocatedTextureMemorySize,
		/// <summary> Size of available memory in the texture pool. </summary>
		AvailableTextureMemorySize,

		/// <summary> Last item used as a count. </summary>
		Count,
	}

	/// <summary> Struct used to hold memory allocation statistics. Mirrored in MemoryBase.h. This need to be a class because of how ReadMemoryAllocationsStats method works. </summary>
	public class FMemoryAllocationStats
	{
		/// <summary> The total amount of memory used by the game. </summary>
		public Int64 TotalUsed;
		/// <summary> The total amount of memory allocated from the OS. </summary>
		public Int64 TotalAllocated;

		// Virtual memory for Xbox and PC / Main memory for PS3 (tracked in the allocators) Host is not included ??

		/// <summary> The allocated in use by the application virtual memory. </summary>
		public Int64 CPUUsed;
		/// <summary> The allocated from the OS/allocator, but not used by the application. </summary>
		public Int64 CPUSlack;
		/// <summary> Alignment waste from a pooled allocator plus book keeping overhead. </summary>
		public Int64 CPUWaste;
		/// <summary> The amount of free memory before the first malloc has been done. (PS3 only). </summary>
		public Int64 CPUAvailable;

		// Physical memory for Xbox and PC / Local memory for PS3 (tracked in the allocators)

		/// <summary> The allocated in use by the application physical memory. </summary>
		public Int64 GPUUsed;
		/// <summary> The allocated from the OS, but not used by the application. </summary>
		public Int64 GPUSlack;
		/// <summary> Alignment waste from a pooled allocator plus book keeping overhead. </summary>
		public Int64 GPUWaste;
		/// <summary> The total amount of memory available for the allocator. (PS3 only) </summary>
		public Int64 GPUAvailable;

		/// <summary> Used memory as reported by the operating system. (Xbox360, PS3 only) </summary>
		public Int64 OSReportedUsed;
		/// <summary> Free memory as reported by the operating system. (Xbox360, PS3 only) </summary>
		public Int64 OSReportedFree;
		/// <summary> The overhead of the operating system. (Xbox360 only) </summary>
		public Int64 OSOverhead;
		/// <summary> Size of loaded executable, stack, static, and global object size. (Xbox360, PS3 only) </summary>
		public Int64 ImageSize;

		/// <summary> Host memory in use by the application. (PS3 only) </summary>
		public Int64 HostUsed;
		/// <summary> Host memory allocated, but not used by the application. (PS3 only) </summary>
		public Int64 HostSlack;
		/// <summary> Host memory wasted due to allocations' alignment. (PS3 only) </summary>
		public Int64 HostWaste;
		/// <summary> The total amount of host memory that has been allocated. (PS3 only) </summary>
		public Int64 HostAvailable;

		/// <summary> Size of allocated memory in the texture pool. </summary>
		public Int64 AllocatedTextureMemorySize;
		/// <summary> Size of available memory in the texture pool. </summary>
		public Int64 AvailableTextureMemorySize;

		public void Zero()
		{
			TotalUsed = 0;
			TotalAllocated = 0;

			CPUUsed = 0;
			CPUSlack = 0;
			CPUWaste = 0;
			CPUAvailable = 0;

			GPUUsed = 0;
			GPUSlack = 0;
			GPUWaste = 0;
			GPUAvailable = 0;

			HostUsed = 0;
			HostSlack = 0;
			HostWaste = 0;
			HostAvailable = 0;

			OSReportedUsed = 0;
			OSReportedFree = 0;
			OSOverhead = 0;
			ImageSize = 0;

			AllocatedTextureMemorySize = 0;
			AvailableTextureMemorySize = 0;
		}

		/// <summary> Returns a difference between old and new memory allocation stats. </summary>
		public static FMemoryAllocationStats Diff( FMemoryAllocationStats Old, FMemoryAllocationStats New )
		{
			FMemoryAllocationStats Diff = new FMemoryAllocationStats();

			FieldInfo[] FieldInfos = typeof( FMemoryAllocationStats ).GetFields();
			int PropertiesNum = FieldInfos.Length;

			for( int FieldIndex = 0; FieldIndex < PropertiesNum; FieldIndex++ )
			{
				Int64 OldValue = (Int64)FieldInfos[ FieldIndex ].GetValue( Old );
				Int64 NewValue = (Int64)FieldInfos[ FieldIndex ].GetValue( New );

				FieldInfos[ FieldIndex ].SetValue( Diff, NewValue-OldValue );
			}

			return Diff;
		}

		/// <summary> Creates a new copy of this class. </summary>
		public FMemoryAllocationStats DeepCopy()
		{
			FMemoryAllocationStats Copy = ( FMemoryAllocationStats )MemberwiseClone();
			return Copy;
		}

		/// <summary> Converts this memory allocation statistics to its equivalent string representation. </summary>
		public override string ToString()
		{
			StringBuilder StrBuilder = new StringBuilder(1024);

			/// <summary> The total amount of memory used by the game. </summary>
			StrBuilder.AppendLine( "	TotalUsed: " + MainWindow.FormatSizeString2( TotalUsed ) );

			/// <summary> The total amount of memory allocated from the OS. </summary>
			StrBuilder.AppendLine( "	TotalAllocated: " + MainWindow.FormatSizeString2( TotalAllocated ) );

			// Virtual memory for Xbox and PC / Main memory for PS3 (tracked in the allocators) Host is not included ??

			/// <summary> The allocated in use by the application virtual memory. </summary>
			StrBuilder.AppendLine( "	CPUUsed: " + MainWindow.FormatSizeString2( CPUUsed ) );
			/// <summary> The allocated from the OS/allocator, but not used by the application. </summary>
			StrBuilder.AppendLine( "	CPUSlack: " + MainWindow.FormatSizeString2( CPUSlack ) );
			/// <summary> Alignment waste from a pooled allocator plus book keeping overhead. </summary>
			StrBuilder.AppendLine( "	CPUWaste: " + MainWindow.FormatSizeString2( CPUWaste ) );

			/// <summary> The amount of free memory before the first malloc has been done. (PS3 only). </summary>
			if( FStreamInfo.GlobalInstance.Platform == EPlatformType.PS3 )
			{
				StrBuilder.AppendLine( "	CPUAvailable: " + MainWindow.FormatSizeString2( CPUAvailable ) );
			}

			// Physical memory for Xbox and PC / Local memory for PS3 (tracked in the allocators)

			/// <summary> The allocated in use by the application physical memory. </summary>
			StrBuilder.AppendLine( "	GPUUsed: " + MainWindow.FormatSizeString2( GPUUsed ) );
			/// <summary> The allocated from the OS, but not used by the application. </summary>
			StrBuilder.AppendLine( "	GPUSlack: " + MainWindow.FormatSizeString2( GPUSlack ) );
			/// <summary> Alignment waste from a pooled allocator plus book keeping overhead. </summary>
			StrBuilder.AppendLine( "	GPUWaste: " + MainWindow.FormatSizeString2( GPUWaste ) );
			/// <summary> The total amount of memory available for the allocator. (PS3 only) </summary>
			StrBuilder.AppendLine( "	GPUAvailable: " + MainWindow.FormatSizeString2( GPUAvailable ) );

			/// <summary> Used memory as reported by the operating system. (Xbox360, PS3 only) </summary>
			if( FStreamInfo.GlobalInstance.Platform == EPlatformType.PS3 || FStreamInfo.GlobalInstance.Platform == EPlatformType.Xbox360 )
			{
				StrBuilder.AppendLine( "	OSReportedUsed: " + MainWindow.FormatSizeString2( OSReportedUsed ) );
			}
			/// <summary> Free memory as reported by the operating system. (Xbox360, PS3 only) </summary>
			if( FStreamInfo.GlobalInstance.Platform == EPlatformType.PS3 || FStreamInfo.GlobalInstance.Platform == EPlatformType.Xbox360 )
			{
				StrBuilder.AppendLine( "	OSReportedFree: " + MainWindow.FormatSizeString2( OSReportedFree ) );
			}
			/// <summary> The overhead of the operating system. (Xbox360 only) </summary>
			if( FStreamInfo.GlobalInstance.Platform == EPlatformType.Xbox360 )
			{
				StrBuilder.AppendLine( "	OSOverhead: " + MainWindow.FormatSizeString2( OSOverhead ) );
			}
			/// <summary> Size of loaded executable, stack, static, and global object size. (Xbox360, PS3 only) </summary>
			if( FStreamInfo.GlobalInstance.Platform == EPlatformType.PS3 || FStreamInfo.GlobalInstance.Platform == EPlatformType.Xbox360 )
			{
				StrBuilder.AppendLine( "	ImageSize: " + MainWindow.FormatSizeString2( ImageSize ) );
			}

			if( FStreamInfo.GlobalInstance.Platform == EPlatformType.PS3 )
			{
				/// <summary> Host memory in use by the application. (PS3 only) </summary>
				StrBuilder.AppendLine( "	HostUsed: " + MainWindow.FormatSizeString2( HostUsed ) );
				/// <summary> Host memory allocated, but not used by the application. (PS3 only) </summary>
				StrBuilder.AppendLine( "	HostSlack: " + MainWindow.FormatSizeString2( HostSlack ) );
				/// <summary> Host memory wasted due to allocations' alignment. (PS3 only) </summary>
				StrBuilder.AppendLine( "	HostWaste: " + MainWindow.FormatSizeString2( HostWaste ) );
				/// <summary> The total amount of host memory that has been allocated. (PS3 only) </summary>
				StrBuilder.AppendLine( "	HostAvailable: " + MainWindow.FormatSizeString2( HostAvailable ) );
			}

			/// <summary> Size of allocated memory in the texture pool. </summary>
			StrBuilder.AppendLine( "	AllocatedTextureMemorySize: " + MainWindow.FormatSizeString2( AllocatedTextureMemorySize ) );
			/// <summary> Size of available memory in the texture pool. </summary>
			StrBuilder.AppendLine( "	AvailableTextureMemorySize: " + MainWindow.FormatSizeString2( AvailableTextureMemorySize ) );

			return StrBuilder.ToString();
		}
	};

	/// <summary>
    /// Variable sized token emitted by capture code. The parsing code ReadNextToken deals with this and updates
    /// internal data. The calling code is responsible for only accessing member variables associated with the type.
    /// </summary>
    public class FStreamToken//@TODO: Can I turn this back into a struct?
    {
        // Parsing configuration

		/// <summary> Mask of pointer field to get a real pointer (the two LSB are type fields, and the top bits may be a pool index. </summary>
        public static UInt64 PointerMask = 0xFFFFFFFFFFFFFFFCUL;

        public const UInt64 TypeMask = 0x3UL;
        public static int PoolShift = 64;

		/// <summary> Whether to decote the script callstacks. </summary>
        public static bool bDecodeScriptCallstacks;

		/// <summary> Version of the stream. </summary>
        public static uint Version;

		/// <summary> Position in the stream. </summary>
		public ulong StreamIndex = 0;

		/// <summary> Type of token. </summary>
        public EProfilingPayloadType Type;

		/// <summary> Subtype of token if it's of TYPE_Other. </summary>
        public EProfilingPayloadSubType SubType;

		/// <summary> Pointer in the case of alloc / free. </summary>
        public UInt64 Pointer;

		/// <summary> Old pointer in the case of realloc. </summary>
        public UInt64 OldPointer;

		/// <summary> New pointer in the case of realloc. </summary>
        public UInt64 NewPointer;

		/// <summary> Index into callstack array. </summary>
        public Int32 CallStackIndex;

		/// <summary> Size of allocation in alloc / realloc case. </summary>
        public Int32 Size;

		/// <summary> Payload if type is TYPE_Other. </summary>
        public UInt32 Payload;

		/// <summary> Payload data if type is TYPE_Other and subtype is SUBTYPE_SnapshotMarker or SUBTYPE_TextMarker. Index into array of unique names. </summary>
        public int TextIndex;

		/// <summary> Payload data if type is TYPE_Other and subtype is SUBTYPE_FrameTimeMarker. Current delta time in seconds. </summary>
        public float DeltaTime;

		/// <summary> Total time, increased every time DeltaTime has been read. </summary>
		public float TotalTime = 0.0f;

		/// <summary> Time between two consecutive snapshot markers. </summary>
		public float ElapsedTime = 0.0f;

		/// <summary> Memory pool. </summary>
        public EMemoryPool Pool;

		/// <summary> Platform dependent memory metrics. </summary>
        public List<long> Metrics = new List<long>();

		/// <summary> A list of indices into the name table, one for each loaded level including persistent level. </summary>
        public List<int> LoadedLevels = new List<int>();

		/// <summary> Generic memory allocation stats. </summary>
		public FMemoryAllocationStats MemoryAllocationStats = new FMemoryAllocationStats();

		/// <summary> Index into script callstack array. </summary>
        public int ScriptCallstackIndex;

		/// <summary> Index into script-object type array. </summary>
        public int ScriptObjectTypeIndex;

		/// <summary> Reads a script callstack. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
		void ReadScriptCallstack( BinaryReader BinaryStream )
		{
			if( bDecodeScriptCallstacks )
			{
				ScriptCallstackIndex = BinaryStream.ReadUInt16();
				bool bAllocatingScriptObject = ( ScriptCallstackIndex & 0x8000 ) != 0;

				ScriptCallstackIndex = ScriptCallstackIndex & 0x7FFF;
				if( ScriptCallstackIndex == 0x7FFF )
				{
					ScriptCallstackIndex = -1;
				}

				if( bAllocatingScriptObject )
				{
					int ScriptObjectTypeCompactedName = BinaryStream.ReadInt32();
					ScriptObjectTypeIndex = ScriptObjectTypeCompactedName & 0x00FFFFFF;
					// Number is always 0.
					int ScriptObjectTypeNumber = ( ScriptObjectTypeCompactedName & 0x7FFFFFFF ) >> 24;
				}
			}
		}

		/// <summary> Reads additional data required for GCM callstacks. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
		bool ReadGCMData(BinaryReader BinaryStream, ref UInt32 UnsignedSize )
		{
			// @see FMallocGcmProfiler.h
			const UInt32 GCM_MEMORY_PROFILER_ID_BIT = 0x80000000;
			bool bHasGCMData = false;

			if( ( UnsignedSize & GCM_MEMORY_PROFILER_ID_BIT ) == GCM_MEMORY_PROFILER_ID_BIT )
			{
				// Lower five bits are EAllocationType, upper three bits are EMemoryPool
				byte AllocationType = BinaryStream.ReadByte();
				Pool = FMemoryPoolInfo.ConvertToMemoryPoolFlag( ( EMemoryPoolSerialized )( AllocationType & 0xe0 ) );

				UnsignedSize &= ~GCM_MEMORY_PROFILER_ID_BIT;

				bHasGCMData = true;
			}

			return bHasGCMData;
		}

		/// <summary> Reads platform dependent memory metrics. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
        void ReadMetrics(BinaryReader BinaryStream)
        {
            // Read the metrics 
            int NumMetrics = BinaryStream.ReadByte();
            for (int i = 0; i < NumMetrics; i++)
            {
                Metrics.Add(BinaryStream.ReadInt64());
            }
        }

		/// <summary> Reads names of all loaded levels. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
		void ReadLoadedLevels( BinaryReader BinaryStream )
		{
			// Read the currently loaded levels
			int NumLevels = BinaryStream.ReadInt16();
			for( int LevelIndex = 0; LevelIndex < NumLevels; LevelIndex++ )
			{
				int LevelNameIndex = BinaryStream.ReadInt32();

				LoadedLevels.Add( LevelNameIndex );
			}
		}

		/// <summary> Reads generic memory allocations stats. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
		private void ReadMemoryAllocationsStats( BinaryReader BinaryStream )
		{
			int StatsNum = BinaryStream.ReadByte();

			FieldInfo[] FieldInfos = MemoryAllocationStats.GetType().GetFields();
			int PropertiesNum = FieldInfos.Length;
			Debug.Assert( StatsNum == PropertiesNum );

			for( int StatIndex = 0; StatIndex < StatsNum; StatIndex++ )
			{
				Int64 Value = BinaryStream.ReadInt64();
				FieldInfos[ StatIndex ].SetValue( MemoryAllocationStats, Value );
			}
		}

		/// <summary> Updates the token with data read from passed in stream and returns whether we've reached the end. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
		public bool ReadNextToken( BinaryReader BinaryStream )
        {
            bool bReachedEndOfStream = false;

			// Initialize to defaults.
			SubType = EProfilingPayloadSubType.SUBTYPE_Unknown;
			TextIndex = -1;

            // Read the pointer and convert to token type by looking at lowest 2 bits. Pointers are always
            // 4 byte aligned so need to clear them again after the conversion.
            UInt64 RawPointerData = BinaryStream.ReadUInt64();

			Pool = EMemoryPool.MEMPOOL_Main;
            Type = (EProfilingPayloadType)(RawPointerData & TypeMask);
            Pointer = RawPointerData & PointerMask;

            Metrics.Clear();
            LoadedLevels.Clear();
			MemoryAllocationStats.Zero();
			CallStackIndex = -1;
            ScriptCallstackIndex = -1;
            ScriptObjectTypeIndex = -1;

			NewPointer = 0;
			OldPointer = 0;
			Size = -1;
			Payload = 0;
			DeltaTime = -1.0f;

            // Serialize based on token type.
			switch( Type )
            {
                // Malloc
                case EProfilingPayloadType.TYPE_Malloc:
				{
					// Get the call stack index.
					CallStackIndex = BinaryStream.ReadInt32();

					// Get the size of an allocation.
					UInt32 UnsignedSize = BinaryStream.ReadUInt32();

					// Read GCM data if any.
					bool bHasGCMData = ReadGCMData( BinaryStream, ref UnsignedSize );

					// If GCM doesn't exist read script callstack.
					if( bHasGCMData == false )
					{
						ReadScriptCallstack( BinaryStream );
					}
					Size = ( int )UnsignedSize;
					break;
				}
				

                // Free
                case EProfilingPayloadType.TYPE_Free:
                {
					break;
				}

				// Realloc
				case EProfilingPayloadType.TYPE_Realloc:
				{
					OldPointer = Pointer;
					NewPointer = BinaryStream.ReadUInt64();
					CallStackIndex = BinaryStream.ReadInt32();

					UInt32 UnsignedSize = BinaryStream.ReadUInt32();
					bool bHasGCMData = ReadGCMData( BinaryStream, ref UnsignedSize );
					if( bHasGCMData == false )
					{
						ReadScriptCallstack( BinaryStream );
					}
					Size = ( int )UnsignedSize;
					break;
				}
				

                // Other
				case EProfilingPayloadType.TYPE_Other:
				{
					SubType = ( EProfilingPayloadSubType )BinaryStream.ReadInt32();
					Payload = BinaryStream.ReadUInt32();

					// Read subtype.
					switch( SubType )
					{
						// End of stream!
						case EProfilingPayloadSubType.SUBTYPE_EndOfStreamMarker:
						{
							if( Version > 2 )
							{
								ReadMemoryAllocationsStats( BinaryStream );
								ReadLoadedLevels( BinaryStream );
								ReadMetrics( BinaryStream );

							}
							bReachedEndOfStream = true;
							break;
						}
						

						case EProfilingPayloadSubType.SUBTYPE_EndOfFileMarker:
						{
							break;
						}

						case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Start:
						case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Mid:
						case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_End:
						case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_Start:
						case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_End:
						case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LevelStream_Start:
						case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LevelStream_End:
						case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker:
						{
							TextIndex = ( int )Payload;

							if( Version > 2 )
							{
								ReadMemoryAllocationsStats( BinaryStream );
								ReadLoadedLevels( BinaryStream );
								ReadMetrics( BinaryStream );
							}
							break;
						}
						

						case EProfilingPayloadSubType.SUBTYPE_FrameTimeMarker:
						{
							DeltaTime = BitConverter.ToSingle( System.BitConverter.GetBytes( Payload ), 0 );
							TotalTime += DeltaTime;
							ElapsedTime += DeltaTime;
							break;
						}

						case EProfilingPayloadSubType.SUBTYPE_TextMarker:
						{
							TextIndex = ( int )Payload;
							break;
						}

						case EProfilingPayloadSubType.SUBTYPE_MemoryAllocationStats:
						{
							ReadMemoryAllocationsStats( BinaryStream );
							break;
						}

						case EProfilingPayloadSubType.SUBTYPE_TotalUsed:
						case EProfilingPayloadSubType.SUBTYPE_TotalAllocated:
						case EProfilingPayloadSubType.SUBTYPE_CPUUsed:
						case EProfilingPayloadSubType.SUBTYPE_CPUSlack:
						case EProfilingPayloadSubType.SUBTYPE_CPUWaste:
						case EProfilingPayloadSubType.SUBTYPE_GPUUsed:
						case EProfilingPayloadSubType.SUBTYPE_GPUSlack:
						case EProfilingPayloadSubType.SUBTYPE_GPUWaste:
						case EProfilingPayloadSubType.SUBTYPE_ImageSize:
						case EProfilingPayloadSubType.SUBTYPE_OSOverhead:
						{
							break;
						}

						default:
						{
							throw new InvalidDataException();
						}
					}
					break;
				}
			}

            return !bReachedEndOfStream;
        }
    }
}
