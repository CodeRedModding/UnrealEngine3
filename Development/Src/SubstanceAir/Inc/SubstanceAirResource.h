//! @file SubstanceAirResource.h
//! @brief Declaration of the Substance Air Texture resource class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_RESOURCE_H
#define _SUBSTANCE_AIR_RESOURCE_H

#if WITH_SUBSTANCE_AIR

/**
 * FTextureResource for Substance Air textures
 */
class FSubstanceAirTexture2DResource : public FTexture2DResource
{
public:
	/**
	 * Minimal initialization constructor.
	 *
	 * @param InOwner			UTexture2D which this FTexture2DResource represents.
	 * @param InitialMipCount	Initial number of miplevels to upload to card
	 * @param InFilename		Filename to read data from
 	 */
	FSubstanceAirTexture2DResource( UTexture2D* InOwner, INT InitialMipCount, output_inst_t* );

	/**
	 * Destructor, freeing MipData in the case of resource being destroyed without ever 
	 * having been initialized by the rendering thread via InitRHI.
	 */
	virtual ~FSubstanceAirTexture2DResource();

};

#endif // WITH_SUBSTANCE_AIR

#endif //_SUBSTANCE_AIR_RESOURCE_H
