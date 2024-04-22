/*=============================================================================
	NvApexGenericAsset.h : Header file for NvApexGenericAsset.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef NV_APEX_GENERIC_H

#define NV_APEX_GENERIC_H

#include "NvApexManager.h"


#if WITH_APEX
class UMaterialInterface;

/**
 * Initializes a material for Apex
 * @param [in,out]	MaterialInterface -	The material interface.
**/
void InitMaterialForApex(UMaterialInterface *&MaterialInterface);

/**
 * Gets the Apex Asset Name
 * @param [in,out]	Asset - The Asset.
 * @param [in,out]	DestinationBuffer - The buffer for the string
 * @param	StringLength - The string length.
**/
void GetApexAssetName(UApexAsset *Asset,char *DestinationBuffer,physx::PxU32 StringLength,const char *extension);

void AnsiToString(const char *str,FString &fstring);

#endif

#endif
