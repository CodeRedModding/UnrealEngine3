/*=============================================================================
	UnMathTest.cpp: Test harness for vector INTrinsics abstraction.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "CorePrivate.h" 

// allow for removal of all this code 
#if ENABLE_VECTORINTRINSICS_TEST

// Create some temporary storage variables
MS_ALIGN( 16 ) static FLOAT GScratch[16] GCC_ALIGN( 16 );
static FLOAT GSum;
static UBOOL GPassing;

/**
 * Tests if two vectors (xyzw) are bitwise equal
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 *
 * @return TRUE if equal
 */
UBOOL TestVectorsEqualBitwise( VectorRegister Vec0, VectorRegister Vec1)
{
	VectorStoreAligned( Vec0, GScratch + 0 );
	VectorStoreAligned( Vec1, GScratch + 4 );

	const UBOOL Passed = (memcmp(GScratch + 0, GScratch + 4, sizeof(FLOAT) * 4) == 0);

	GPassing = GPassing && Passed;
	return Passed;
}

/**
 * Tests if two vectors (xyzw) are equal within an optional tolerance
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 * @param Tolerance Error allowed for the comparison
 *
 * @return TRUE if equal(ish)
 */
UBOOL TestVectorsEqual( VectorRegister Vec0, VectorRegister Vec1, FLOAT Tolerance = 0.0f)
{
	VectorStoreAligned( Vec0, GScratch + 0 );
	VectorStoreAligned( Vec1, GScratch + 4 );
	GSum = 0.f;
	for ( INT Component = 0; Component < 4; Component++ ) 
	{
		FLOAT Diff = GScratch[ Component + 0 ] - GScratch[ Component + 4 ];
		GSum += ( Diff >= 0.0f ) ? Diff : -Diff;
	}
	GPassing = GPassing && GSum <= Tolerance;
	return GSum <= Tolerance;
}

/**
 * Tests if two vectors (xyz) are equal within an optional tolerance
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 * @param Tolerance Error allowed for the comparison
 *
 * @return TRUE if equal(ish)
 */
UBOOL TestVectorsEqual3( VectorRegister Vec0, VectorRegister Vec1, FLOAT Tolerance = 0.0f)
{
	VectorStoreAligned( Vec0, GScratch + 0 );
	VectorStoreAligned( Vec1, GScratch + 4 );
	GSum = 0.f;
	for ( INT Component = 0; Component < 3; Component++ ) 
	{
		GSum += Abs<FLOAT>( GScratch[ Component + 0 ] - GScratch[ Component + 4 ] );
	}
	GPassing = GPassing && GSum <= Tolerance;
	return GSum <= Tolerance;
}

/**
 * Helper debugf function to print out success or failure information for a test
 *
 * @param TestName Name of the current test
 * @param bHasPassed TRUE if the test has passed
 */
void LogTest( const TCHAR *TestName, UBOOL bHasPassed ) 
{
	debugf( TEXT("%s: %s"), bHasPassed ? TEXT("PASSED") : TEXT("FAILED"), TestName );
	if ( bHasPassed == FALSE )
	{
		debugf( TEXT("Bad(%f): (%f %f %f %f) (%f %f %f %f)"), GSum, GScratch[0], GScratch[1], GScratch[2], GScratch[3], GScratch[4], GScratch[5], GScratch[6], GScratch[7] );
		GPassing = FALSE;
	}
}

/** 
 * Set the contents of the scratch memory
 * 
 * @param X,Y,Z,W,U values to push into GScratch
 */
void SetScratch( FLOAT X, FLOAT Y, FLOAT Z, FLOAT W, FLOAT U = 0.0f )
{
	GScratch[0] = X;
	GScratch[1] = Y;
	GScratch[2] = Z;
	GScratch[3] = W;
	GScratch[4] = U;
}


/**
 * Run a suite of vector operations to validate vector instrincics are working on the platform
 */
void RunVectorRegisterAbstractionTest()
{
	FLOAT F1 = 1.f;
	DWORD D1 = *(DWORD *)&F1;
	VectorRegister V0, V1, V2, V3;

	GPassing = TRUE;

	V0 = MakeVectorRegister( D1, D1, D1, D1 );
	V1 = MakeVectorRegister( F1, F1, F1, F1 );
	LogTest( TEXT("MakeVectorRegister"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 0.f, 0.f, 0.f, 0.f );
	V1 = VectorZero();
	LogTest( TEXT("VectorZero"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.f, 1.f, 1.f, 1.f );
	V1 = VectorOne();
	LogTest( TEXT("VectorOne"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, -0.5f );
	V1 = VectorLoad( GScratch );
	LogTest( TEXT("VectorLoad"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, -0.5f );
	V1 = VectorLoad( GScratch );
	LogTest( TEXT("VectorLoad"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, -0.5f );
	V1 = VectorLoadAligned( GScratch );
	LogTest( TEXT("VectorLoadAligned"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = VectorLoad( GScratch + 1 );
	V1 = VectorLoadFloat3( GScratch + 1 );
	LogTest( TEXT("VectorLoadFloat3"), TestVectorsEqual3( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, 0.0f );
	V1 = VectorLoadFloat3_W0( GScratch );
	LogTest( TEXT("VectorLoadFloat3_W0"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, 1.0f );
	V1 = VectorLoadFloat3_W1( GScratch );
	LogTest( TEXT("VectorLoadFloat3_W1"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( -0.5f, -0.5f, -0.5f, -0.5f );
	V1 = VectorLoadFloat1( GScratch + 3 );
	LogTest( TEXT("VectorLoadFloat1"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = VectorSetFloat3( GScratch[1], GScratch[2], GScratch[3] );
	V1 = VectorLoadFloat3( GScratch + 1 );
	LogTest( TEXT("VectorSetFloat3"), TestVectorsEqual3( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = VectorSet( GScratch[1], GScratch[2], GScratch[3], GScratch[4] );
	V1 = VectorLoad( GScratch + 1 );
	LogTest( TEXT("VectorSet"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, 1.0f );
	VectorStoreAligned( V0, GScratch + 8 );
	V1 = VectorLoad( GScratch + 8 );
	LogTest( TEXT("VectorStoreAligned"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.55f, 1.0f );
	VectorStore( V0, GScratch + 7 );
	V1 = VectorLoad( GScratch + 7 );
	LogTest( TEXT("VectorStore"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 5.0f, 3.0f, 1.0f, -1.0f );
	VectorStoreFloat3( V0, GScratch );
	V1 = VectorLoad( GScratch );
	V0 = MakeVectorRegister( 5.0f, 3.0f, 1.0f, -0.5f );
	LogTest( TEXT("VectorStoreFloat3"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 5.0f, 3.0f, 1.0f, -1.0f );
	VectorStoreFloat1( V0, GScratch + 1 );
	V1 = VectorLoad( GScratch );
	V0 = MakeVectorRegister( 1.0f, 5.0f, -0.25f, -0.5f );
	LogTest( TEXT("VectorStoreFloat1"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorReplicate( V0, 1 );
	V0 = MakeVectorRegister( 2.0f, 2.0f, 2.0f, 2.0f );
	LogTest( TEXT("VectorReplicate"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, -2.0f, 3.0f, -4.0f );
	V1 = VectorAbs( V0 );
	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	LogTest( TEXT("VectorAbs"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, -2.0f, 3.0f, -4.0f );
	V1 = VectorNegate( V0 );
	V0 = MakeVectorRegister( -1.0f, 2.0f, -3.0f, 4.0f );
	LogTest( TEXT("VectorNegate"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = VectorAdd( V0, V1 );
	V0 = MakeVectorRegister( 3.0f, 6.0f, 9.0f, 12.0f );
	LogTest( TEXT("VectorAdd"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorSubtract( V0, V1 );
	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	LogTest( TEXT("VectorSubtract"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorMultiply( V0, V1 );
	V0 = MakeVectorRegister( 2.0f, 8.0f, 18.0f, 32.0f );
	LogTest( TEXT("VectorMultiply"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorMultiplyAdd( V0, V1, VectorOne() );
	V0 = MakeVectorRegister( 3.0f, 9.0f, 19.0f, 33.0f );
	LogTest( TEXT("VectorMultiplyAdd"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorDot3( V0, V1 );
	V0 = MakeVectorRegister( 28.0f, 28.0f, 28.0f, 28.0f );
	LogTest( TEXT("VectorDot3"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorDot4( V0, V1 );
	V0 = MakeVectorRegister( 60.0f, 60.0f, 60.0f, 60.0f );
	LogTest( TEXT("VectorDot4"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 0.0f, 0.0f, 8.0f );
	V1 = MakeVectorRegister( 0.0f, 2.0f, 0.0f, 4.0f );
	V1 = VectorCross( V0, V1 );
	V0 = MakeVectorRegister( 0.f, 0.0f, 2.0f, 0.0f );
	LogTest( TEXT("VectorCross"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorPow( V0, V1 );
	V0 = MakeVectorRegister( 16.0f, 64.0f, 36.0f, 8.0f );
	LogTest( TEXT("VectorPow"), TestVectorsEqual( V0, V1, 0.001f ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorReciprocalLen( V0 );
	V0 = MakeVectorRegister( 0.25f, 0.25f, 0.25f, 0.25f );
	LogTest( TEXT("VectorReciprocalLen"), TestVectorsEqual( V0, V1, 0.001f ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorNormalize( V0 );
	V0 = MakeVectorRegister( 0.5f, -0.5f, 0.5f, -0.5f );
	LogTest( TEXT("VectorNormalize"), TestVectorsEqual( V0, V1, 0.001f ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorSet_W0( V0 );
	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, 0.0f );
	LogTest( TEXT("VectorSet_W0"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorSet_W1( V0 );
	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, 1.0f );
	LogTest( TEXT("VectorSet_W1"), TestVectorsEqual( V0, V1 ) );

	// Need to test VectorMatrixMultiply.

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorMin( V0, V1 );
	V0 = MakeVectorRegister( 2.0f, 3.0f, 2.0f, 1.0f );
	LogTest( TEXT("VectorMin"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorMax( V0, V1 );
	V0 = MakeVectorRegister( 4.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorMax"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 1, 0, 3, 2 );
	V0 = MakeVectorRegister( 3.0f, 4.0f, 1.0f, 2.0f );
	LogTest( TEXT("VectorSwizzle1032"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 1, 2, 0, 1 );
	V0 = MakeVectorRegister( 3.0f, 2.0f, 4.0f, 3.0f );
	LogTest( TEXT("VectorSwizzle1201"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 2, 0, 1, 3 );
	V0 = MakeVectorRegister( 2.0f, 4.0f, 3.0f, 1.0f );
	LogTest( TEXT("VectorSwizzle2013"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 2, 3, 0, 1 );
	V0 = MakeVectorRegister( 2.0f, 1.0f, 4.0f, 3.0f );
	LogTest( TEXT("VectorSwizzle2301"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 3, 2, 1, 0 );
	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	LogTest( TEXT("VectorSwizzle3210"), TestVectorsEqual( V0, V1 ) );

	BYTE Bytes[4] = { 25, 75, 125, 200 };
	V0 = VectorLoadByte4( Bytes );
	V1 = MakeVectorRegister( 25.f, 75.f, 125.f, 200.f );
	LogTest( TEXT("VectorLoadByte4"), TestVectorsEqual( V0, V1 ) );

	V0 = VectorLoadByte4Reverse( Bytes );
	V1 = MakeVectorRegister( 25.f, 75.f, 125.f, 200.f );
	V1 = VectorSwizzle( V1, 3, 2, 1, 0 );
	LogTest( TEXT("VectorLoadByte4Reverse"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	VectorStoreByte4( V0, Bytes );
	V1 = VectorLoadByte4( Bytes );
	LogTest( TEXT("VectorStoreByte4"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	UBOOL bIsVAGT_TRUE = VectorAnyGreaterThan( V0, V1 ) != 0;
	LogTest( TEXT("VectorAnyGreaterThan-TRUE"), bIsVAGT_TRUE );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	UBOOL bIsVAGT_FALSE = VectorAnyGreaterThan( V0, V1 ) == 0;
	LogTest( TEXT("VectorAnyGreaterThan-FALSE"), bIsVAGT_FALSE );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAnyLesserThan-TRUE"), VectorAnyLesserThan( V0, V1 ) != 0 );

	V0 = MakeVectorRegister( 3.0f, 5.0f, 7.0f, 9.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAnyLesserThan-FALSE"), VectorAnyLesserThan( V0, V1 ) == 0 );

	V0 = MakeVectorRegister( 3.0f, 5.0f, 7.0f, 9.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllGreaterThan-TRUE"), VectorAllGreaterThan( V0, V1 ) != 0 );

	V0 = MakeVectorRegister( 3.0f, 1.0f, 7.0f, 9.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllGreaterThan-FALSE"), VectorAllGreaterThan( V0, V1 ) == 0 );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllLesserThan-TRUE"), VectorAllLesserThan( V0, V1 ) != 0 );

	V0 = MakeVectorRegister( 3.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllLesserThan-FALSE"), VectorAllLesserThan( V0, V1 ) == 0 );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareGT( V0, V1 );
	V3 = MakeVectorRegister( (DWORD)0, (DWORD)0, (DWORD)0, (DWORD)-1 );
	LogTest( TEXT("VectorCompareGT"), TestVectorsEqualBitwise( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareGE( V0, V1 );
	V3 = MakeVectorRegister( (DWORD)0, (DWORD)0, (DWORD)-1, (DWORD)-1 );
	LogTest( TEXT("VectorCompareGE"), TestVectorsEqualBitwise( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareEQ( V0, V1 );
	V3 = MakeVectorRegister( (DWORD)0, (DWORD)0, (DWORD)-1, (DWORD)0 );
	LogTest( TEXT("VectorCompareEQ"), TestVectorsEqualBitwise( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareNE( V0, V1 );
	V3 = MakeVectorRegister( (DWORD)(0xFFFFFFFFU), (DWORD)(0xFFFFFFFFU), (DWORD)(0), (DWORD)(0xFFFFFFFFU) );
	LogTest( TEXT("VectorCompareNE"), TestVectorsEqualBitwise( V2, V3 ) );
	
	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = MakeVectorRegister( (DWORD)-1, (DWORD)0, (DWORD)0, (DWORD)-1 );
	V2 = VectorSelect( V2, V0, V1 );
	V3 = MakeVectorRegister( 1.0f, 4.0f, 2.0f, 8.0f );
	LogTest( TEXT("VectorSelect"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 0.0f, 0.0f );
	V1 = MakeVectorRegister( 0.0f, 0.0f, 2.0f, 1.0f );
	V2 = VectorBitwiseOr( V0, V1 );
	V3 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	LogTest( TEXT("VectorBitwiseOr-Float1"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 24.0f, 36.0f );
	V1 = MakeVectorRegister( (DWORD)(0x80000000U), (DWORD)(0x80000000U), (DWORD)(0x80000000U), (DWORD)(0x80000000U) );
	V2 = VectorBitwiseOr( V0, V1 );
	V3 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, -36.0f );
	LogTest( TEXT("VectorBitwiseOr-Float2"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( (DWORD)(0xFFFFFFFFU), (DWORD)(0x7FFFFFFFU), (DWORD)(0x7FFFFFFFU), (DWORD)(0xFFFFFFFFU) );
	V2 = VectorBitwiseAnd( V0, V1 );
	V3 = MakeVectorRegister( -1.0f, 3.0f, 24.0f, 36.0f );
	LogTest( TEXT("VectorBitwiseAnd-Float"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( (DWORD)(0x80000000U), (DWORD)(0x00000000U), (DWORD)(0x80000000U), (DWORD)(0x80000000U) );
	V2 = VectorBitwiseXor( V0, V1 );
	V3 = MakeVectorRegister( 1.0f, -3.0f, 24.0f, -36.0f );
	LogTest( TEXT("VectorBitwiseXor-Float"), TestVectorsEqual( V2, V3 ) );


	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( 5.0f, 35.0f, 23.0f, 48.0f );
	V2 = VectorMergeVecXYZ_VecW( V0, V1 );
	V3 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 48.0f );
	LogTest( TEXT("VectorMergeXYZ_VecW-1"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( 5.0f, 35.0f, 23.0f, 48.0f );
	V2 = VectorMergeVecXYZ_VecW( V1, V0 );
	V3 = MakeVectorRegister( 5.0f, 35.0f, 23.0f, 36.0f );
	LogTest( TEXT("VectorMergeXYZ_VecW-2"), TestVectorsEqual( V2, V3 ) );


	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocal( V0 );
	V3 = VectorMultiply(V1, V0);
	LogTest( TEXT("VectorReciprocal"), TestVectorsEqual( VectorOne(), V3, 1e-3f ) );

	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocalAccurate( V0 );
	V3 = VectorMultiply(V1, V0);
	LogTest( TEXT("VectorReciprocalAccurate"), TestVectorsEqual( VectorOne(), V3, 1e-7f ) );


	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocalSqrt( V0 );
	V3 = VectorMultiply(VectorMultiply(V1, V1), V0);
	LogTest( TEXT("VectorReciprocalSqrt"), TestVectorsEqual( VectorOne(), V3, 2e-3f ) );

	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocalSqrtAccurate( V0 );
	V3 = VectorMultiply(VectorMultiply(V1, V1), V0);
	LogTest( TEXT("VectorReciprocalSqrtAccurate"), TestVectorsEqual( VectorOne(), V3, 1e-6f ) );


	debugf( TEXT("VectorINTrinsics: %s"), GPassing ? TEXT("PASSED") : TEXT("FAILED") );	
}

#else

/**
 * Stub version that does nothing
 */
void RunVectorRegisterAbstractionTest()
{
}

#endif