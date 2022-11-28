/*=============================================================================
	UnBits.h: Unreal bitstream manipulation classes.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "CorePrivate.h" 

// Table.
static BYTE GShift[8]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
static BYTE GMask [8]={0x00,0x01,0x03,0x07,0x0f,0x1f,0x3f,0x7f};

// Optimized arbitrary bit range memory copy routine.
void appBitsCpy( BYTE* Dest, INT DestBit, BYTE* Src, INT SrcBit, INT BitCount )
{
	if( BitCount==0 ) return;
	
	// Special case - always at least one bit to copy,
	// a maximum of 2 bytes to read, 2 to write - only touch bytes that are actually used.
	if( BitCount <= 8 ) 
	{
		DWORD DestIndex	   = DestBit/8;
		DWORD SrcIndex	   = SrcBit /8;
		DWORD LastDest	   =( DestBit+BitCount-1 )/8;  
		DWORD LastSrc	   =( SrcBit +BitCount-1 )/8;  
		DWORD ShiftSrc     = SrcBit & 7; 
		DWORD ShiftDest    = DestBit & 7;
		DWORD FirstMask    = 0xFF << ShiftDest;  
		DWORD LastMask     = 0xFE << ((DestBit + BitCount-1) & 7) ; // Pre-shifted left by 1.	
		DWORD Accu;		

		if( SrcIndex == LastSrc )
			Accu = (Src[SrcIndex] >> ShiftSrc); 
		else
			Accu =( (Src[SrcIndex] >> ShiftSrc) | (Src[LastSrc ] << (8-ShiftSrc)) );			

		if( DestIndex == LastDest )
		{
			DWORD MultiMask = FirstMask & ~LastMask;
			Dest[DestIndex] = ( ( Dest[DestIndex] & ~MultiMask ) | ((Accu << ShiftDest) & MultiMask) );		
		}
		else
		{		
			Dest[DestIndex] = (BYTE)( ( Dest[DestIndex] & ~FirstMask ) | (( Accu << ShiftDest) & FirstMask) ) ;
			Dest[LastDest ] = (BYTE)( ( Dest[LastDest ] & LastMask  )  | (( Accu >> (8-ShiftDest)) & ~LastMask) ) ;
		}

		return;
	}

	// Main copier, uses byte sized shifting. Minimum size is 9 bits, so at least 2 reads and 2 writes.
	DWORD DestIndex		= DestBit/8;
	DWORD FirstSrcMask  = 0xFF << ( DestBit & 7);  
	DWORD LastDest		= ( DestBit+BitCount )/8; 
	DWORD LastSrcMask   = 0xFF << ((DestBit + BitCount) & 7); 
	DWORD SrcIndex		= SrcBit/8;
	DWORD LastSrc		= ( SrcBit+BitCount )/8;  
	INT   ShiftCount    = (DestBit & 7) - (SrcBit & 7); 
	INT   DestLoop      = LastDest-DestIndex; 
	INT   SrcLoop       = LastSrc -SrcIndex;  
	DWORD FullLoop;
	DWORD BitAccu;

	// Lead-in needs to read 1 or 2 source bytes depending on alignment.
	if( ShiftCount>=0 )
	{
		FullLoop  = Max(DestLoop, SrcLoop);  
		BitAccu   = Src[SrcIndex] << ShiftCount; 
		ShiftCount += 8; //prepare for the inner loop.
	}
	else
	{
		ShiftCount +=8; // turn shifts -7..-1 into +1..+7
		FullLoop  = Max(DestLoop, SrcLoop-1);  
		BitAccu   = Src[SrcIndex] << ShiftCount; 
		SrcIndex++;		
		ShiftCount += 8; // Prepare for inner loop.  
		BitAccu = ( ( (DWORD)Src[SrcIndex] << ShiftCount ) + (BitAccu)) >> 8; 
	}

	// Lead-in - first copy.
	Dest[DestIndex] = (BYTE) (( BitAccu & FirstSrcMask) | ( Dest[DestIndex] &  ~FirstSrcMask ) );
	SrcIndex++;
	DestIndex++;

	// Fast inner loop. 
	for(; FullLoop>1; FullLoop--) 
	{   // ShiftCount ranges from 8 to 15 - all reads are relevant.
		BitAccu = (( (DWORD)Src[SrcIndex] << ShiftCount ) + (BitAccu)) >> 8; // Copy in the new, discard the old.
		SrcIndex++;
		Dest[DestIndex] = (BYTE) BitAccu;  // Copy low 8 bits.
		DestIndex++;		
	}

	// Lead-out. 
	if( LastSrcMask != 0xFF) 
	{
		if ((DWORD)(SrcBit+BitCount-1)/8 == SrcIndex ) // Last legal byte ?
		{
			BitAccu = ( ( (DWORD)Src[SrcIndex] << ShiftCount ) + (BitAccu)) >> 8; 
		}
		else
		{
			BitAccu = BitAccu >> 8; 
		}		

		Dest[DestIndex] = (BYTE)( ( Dest[DestIndex] & LastSrcMask ) | (BitAccu & ~LastSrcMask) );  		
	}	
}

/*-----------------------------------------------------------------------------
	FBitWriter.
-----------------------------------------------------------------------------*/

FBitWriter::FBitWriter( INT InMaxBits )
:	Num			( 0 )
,	Max			( InMaxBits )
,	Buffer		( (InMaxBits+7)>>3 )
{
	appMemzero( &Buffer(0), Buffer.Num() );
	ArIsPersistent = ArIsSaving = 1;
	ArNetVer |= 0x80000000;
}
void FBitWriter::SerializeBits( void* Src, INT LengthBits )
{
	if( Num+LengthBits<=Max )
	{
		//for( INT i=0; i<LengthBits; i++,Num++ )
		//	if( ((BYTE*)Src)[i>>3] & GShift[i&7] )
		//		Buffer(Num>>3) |= GShift[Num&7];
		if( LengthBits == 1 )
		{
			if( ((BYTE*)Src)[0] & 0x01 )
				Buffer(Num>>3) |= GShift[Num&7];
			Num++;
		}
		else
		{
			appBitsCpy( &Buffer(0), Num, (BYTE*)Src, 0, LengthBits);
			Num += LengthBits;
		}
	}
	else ArIsError = 1;
}
void FBitWriter::Serialize( void* Src, INT LengthBytes )
{
	//warning: Copied and pasted from FBitWriter::SerializeBits
	INT LengthBits = LengthBytes*8;
	if( Num+LengthBits<=Max )
	{
		appBitsCpy( &Buffer(0), Num, (BYTE*)Src, 0, LengthBits);
		Num += LengthBits;
	}
	else ArIsError = 1;
}
void FBitWriter::SerializeInt( DWORD& Value, DWORD ValueMax )
{
	if( Num+appCeilLogTwo(ValueMax)<=Max )
	{
		DWORD NewValue=0;
		for( DWORD Mask=1; NewValue+Mask<ValueMax && Mask; Mask*=2,Num++ )
		{
			if( Value&Mask )
			{
				Buffer(Num>>3) += GShift[Num&7];
				NewValue += Mask;
			}
		}
	} else ArIsError = 1;
}
void FBitWriter::WriteInt( DWORD Value, DWORD ValueMax )
{
	//warning: Copied and pasted from FBitWriter::SerializeInt
	if( Num+appCeilLogTwo(ValueMax)<=Max )
	{
		DWORD NewValue=0;
		for( DWORD Mask=1; NewValue+Mask<ValueMax && Mask; Mask*=2,Num++ )
		{
			if( Value&Mask )
			{
				Buffer(Num>>3) += GShift[Num&7];
				NewValue += Mask;
			}
		}
	} else ArIsError = 1;
}
void FBitWriter::WriteBit( BYTE In )
{
	if( Num+1<=Max )
	{
		if( In )
			Buffer(Num>>3) |= GShift[Num&7];
		Num++;
	}
	else ArIsError = 1;
}
BYTE* FBitWriter::GetData()
{
	return &Buffer(0);
}
INT FBitWriter::GetNumBytes()
{
	return (Num+7)>>3;
}
INT FBitWriter::GetNumBits()
{
	return Num;
}
void FBitWriter::SetOverflowed()
{
	ArIsError = 1;
}

/*-----------------------------------------------------------------------------
	FBitWriterMark.
-----------------------------------------------------------------------------*/

void FBitWriterMark::Pop( FBitWriter& Writer )
{
	checkSlow(Num<=Writer.Num);
	checkSlow(Num<=Writer.Max);

	if( Num&7 )
		Writer.Buffer(Num>>3) &= GMask[Num&7];
	INT Start = (Num       +7)>>3;
	INT End   = (Writer.Num+7)>>3;
	appMemzero( &Writer.Buffer(Start), End-Start );

	Writer.ArIsError = Overflowed;
	Writer.Num       = Num;
}

/*-----------------------------------------------------------------------------
	FBitReader.
-----------------------------------------------------------------------------*/

//
// Reads bitstreams.
//
FBitReader::FBitReader( BYTE* Src, INT CountBits )
:	Num			( CountBits )
,	Buffer		( (CountBits+7)>>3 )
,	Pos			( 0 )
{
	ArIsPersistent = ArIsLoading = 1;
	ArNetVer |= 0x80000000;
	if( Src )
		appMemcpy( &Buffer(0), Src, (CountBits+7)>>3 );
}
void FBitReader::SetData( FBitReader& Src, INT CountBits )
{
	Num        = CountBits;
	Pos        = 0;
	ArIsError  = 0;
	Buffer.Empty();
	Buffer.Add( (CountBits+7)>>3 );
	Src.SerializeBits( &Buffer(0), CountBits );
}
void FBitReader::SerializeBits( void* Dest, INT LengthBits )
{
	appMemzero( Dest, (LengthBits+7)>>3 );
	if( Pos+LengthBits<=Num )
	{
		//for( INT i=0; i<LengthBits; i++,Pos++ )
		//	if( Buffer(Pos>>3) & GShift[Pos&7] )
		//		((BYTE*)Dest)[i>>3] |= GShift[i&7];
		if( LengthBits == 1 )
		{
			if( Buffer(Pos>>3) & GShift[Pos&7] )
				((BYTE*)Dest)[0] |= 0x01;
			Pos++;
		}
		else
		{
			appBitsCpy( (BYTE*)Dest, 0, &Buffer(0), Pos, LengthBits );
			Pos += LengthBits;
		}
	}
	else SetOverflowed();
}
void FBitReader::SerializeInt( DWORD& Value, DWORD ValueMax )
{
	Value=0;
	for( DWORD Mask=1; Value+Mask<ValueMax && Mask; Mask*=2,Pos++ )
	{
		if( Pos>=Num )
		{
			ArIsError = 1;
			break;
		}
		if( Buffer(Pos>>3) & GShift[Pos&7] )
		{
			Value |= Mask;
		}
	}
}
DWORD FBitReader::ReadInt( DWORD Max )
{
	DWORD Value=0;
	SerializeInt( Value, Max );
	return Value;
}
BYTE FBitReader::ReadBit()
{
	BYTE Bit=0;
	SerializeBits( &Bit, 1 );
	return Bit;
}
void FBitReader::Serialize( void* Dest, INT LengthBytes )
{
	SerializeBits( Dest, LengthBytes*8 );
}
BYTE* FBitReader::GetData()
{
	return &Buffer(0);
}
UBOOL FBitReader::AtEnd()
{
	return ArIsError || Pos==Num;
}
void FBitReader::SetOverflowed()
{
	ArIsError = 1;
}
INT FBitReader::GetNumBytes()
{
	return (Num+7)>>3;
}
INT FBitReader::GetNumBits()
{
	return Num;
}
INT FBitReader::GetPosBits()
{
	return Pos;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

