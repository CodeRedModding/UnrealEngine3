/*=============================================================================
	UnDistributions.h: C++ declaration of UDistributionFloat and UDistributionVector classes.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

enum EDistributionVectorLockFlags
{
    EDVLF_None,
    EDVLF_XY,
    EDVLF_XZ,
    EDVLF_YZ,
    EDVLF_XYZ,
	EDVLF_MAX
};

enum EDistributionVectorMirrorFlags
{
	EDVMF_Same,
	EDVMF_Different,
	EDVMF_Mirror,
	EDVMF_MAX
};

class UDistributionFloat : public UComponent, public FCurveEdInterface
{
public:
    DECLARE_CLASS(UDistributionFloat,UComponent,0,Engine)
	virtual FLOAT GetValue( FLOAT F = 0.f );
};


class UDistributionVector : public UComponent, public FCurveEdInterface
{
public:
    DECLARE_CLASS(UDistributionVector,UComponent,0,Engine)
	virtual FVector GetValue( FLOAT F = 0.f );
};