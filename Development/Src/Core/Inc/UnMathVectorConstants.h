/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

namespace GlobalVectorConstants
{
	static const VectorRegister Float0001 = MakeVectorRegister( 0.0f, 0.0f, 0.0f, 1.0f );
	static const VectorRegister SmallLengthThreshold = MakeVectorRegister(1.e-8f, 1.e-8f, 1.e-8f, 1.e-8f);
	static const VectorRegister FloatOneHundredth = MakeVectorRegister(0.01f, 0.01f, 0.01f, 0.01f);
	static const VectorRegister Float111_Minus1 = MakeVectorRegister( 1.f, 1.f, 1.f, -1.f );
	static const VectorRegister FloatOneHalf = MakeVectorRegister( 0.5f, 0.5f, 0.5f, 0.5f );

	/** This is to speed up Quaternion Inverse. Static variable to keep sign of inverse **/
	static const VectorRegister QINV_SIGN_MASK = MakeVectorRegister( -1.f, -1.f, -1.f, 1.f );

	static const VectorRegister QMULTI_SIGN_MASK0 = MakeVectorRegister( 1.f, -1.f, 1.f, -1.f );
	static const VectorRegister QMULTI_SIGN_MASK1 = MakeVectorRegister( 1.f, 1.f, -1.f, -1.f );
	static const VectorRegister QMULTI_SIGN_MASK2 = MakeVectorRegister( -1.f, 1.f, 1.f, -1.f );
}
