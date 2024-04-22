//! @file SubstanceAirResource.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirOutput.h"
#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirResource.h"

#if WITH_SUBSTANCE_AIR

FSubstanceAirTexture2DResource::FSubstanceAirTexture2DResource(
	UTexture2D* InOwner,
	INT InitialMipCount, 
	output_inst_t* output):
		FTexture2DResource(
			InOwner, 
			InitialMipCount, 
			TEXT("nofile"))
{

}


FSubstanceAirTexture2DResource::~FSubstanceAirTexture2DResource()
{

}

#endif // WITH_SUBSTANCE_AIR
