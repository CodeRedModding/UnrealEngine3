/*=============================================================================
	LightmassRender.h: lightmass rendering-related definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LIGHTMASSRENDER_H__
#define __LIGHTMASSRENDER_H__

/** Forward declarations of Lightmass types */
namespace Lightmass
{
	struct FMaterialData;
}

/**
 * Class for rendering sample 'textures' of various material properties.
 */
class FLightmassMaterialRenderer
{
public:
	FLightmassMaterialRenderer() : 
		  RenderTarget(NULL)
		, Canvas(NULL)
	{
	}

	virtual ~FLightmassMaterialRenderer();

	/**
	 *	Generate the required material data for the given material.
	 *
	 *	@param	Material				The material of interest.
	 *	@param	bInWantNormals			True if normals should be generated as well
	 *	@param	MaterialEmissive		The emissive samples for the material.
	 *	@param	MaterialDiffuse			The diffuse samples for the material.
	 *	@param	MaterialSpecular		The specular samples for the material.
	 *	@param	MaterialSpecularPower	The specular power samples for the material.
	 *	@param	MaterialTransmission	The transmission samples for the material.
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if not.
	 */
	virtual UBOOL GenerateMaterialData(UMaterialInterface& InMaterial, UBOOL bInWantNormals, Lightmass::FMaterialData& OutMaterialData, 
		TArray<FFloat16Color>& OutMaterialEmissive, TArray<FFloat16Color>& OutMaterialDiffuse, 
		TArray<FFloat16Color>& OutMaterialSpecular, TArray<FFloat16Color>& OutMaterialTransmission,
		TArray<FFloat16Color>& OutMaterialNormal);

protected:
	/**
	 *	Create the required render target.
	 *
	 *	@param	InFormat			The format of the render target
	 *	@param	InSizeX				The X resolution of the render target
	 *	@param	InSizeY				The Y resolution of the render target
	 *
	 *	@return	UBOOL		TRUE if it was successful, FALSE if not
	 */
	virtual UBOOL CreateRenderTarget(EPixelFormat InFormat, INT InSizeX, INT InSizeY);

	/**
	 *	Generate the material data for the given material and it's given property.
	 *
	 *	@param	InMaterial				The material of interest.
	 *	@param	InMaterialProperty		The property to generate the samples for
	 *	@param	InOutSizeX				The desired X size of the sample to capture (in), the resulting size (out)
	 *	@param	InOutSizeY				The desired Y size of the sample to capture (in), the resulting size (out)
	 *	@param	OutMaterialSamples		The samples for the material.
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if not.
	 */
	virtual UBOOL GenerateMaterialPropertyData(UMaterialInterface& InMaterial, 
		EMaterialProperty InMaterialProperty, INT& InOutSizeX, INT& InOutSizeY, 
		TArray<FFloat16Color>& OutMaterialSamples);

	//
	UTextureRenderTarget2D* RenderTarget;
	FCanvas* Canvas;
};

/**
 * Class for rendering sample 'textures' of terrain material properties.
 */
class FLightmassTerrainMaterialRenderer : public FLightmassMaterialRenderer
{
public:
	FLightmassTerrainMaterialRenderer() : 
		FLightmassMaterialRenderer()
	{
	}

	virtual ~FLightmassTerrainMaterialRenderer();

	/**
	 *	Generate the required material data for the given material.
	 *
	 *	@param	Material				The material of interest.
	 *	@param	bInWantNormals			True if normal data should be generated as well
	 *	@param	MaterialEmissive		The emissive samples for the material.
	 *	@param	MaterialDiffuse			The diffuse samples for the material.
	 *	@param	MaterialSpecular		The specular samples for the material.
	 *	@param	MaterialSpecularPower	The specular power samples for the material.
	 *	@param	MaterialTransmission	The transmission samples for the material.
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if not.
	 */
	virtual UBOOL GenerateTerrainMaterialData(FTerrainMaterialResource& InMaterial, UBOOL bInWantNormals, Lightmass::FMaterialData& OutMaterialData, 
		TArray<FFloat16Color>& OutMaterialEmissive, TArray<FFloat16Color>& OutMaterialDiffuse, 
		TArray<FFloat16Color>& OutMaterialSpecular, TArray<FFloat16Color>& OutMaterialTransmission,
		TArray<FFloat16Color>& OutMaterialNormal);

protected:
	/**
	 *	Generate the material data for the given material and it's given property.
	 *
	 *	@param	InMaterial				The material of interest.
	 *	@param	InMaterialProperty		The property to generate the samples for
	 *	@param	InOutSizeX				The desired X size of the sample to capture (in), the resulting size (out)
	 *	@param	InOutSizeY				The desired Y size of the sample to capture (in), the resulting size (out)
	 *	@param	OutMaterialSamples		The samples for the material.
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if not.
	 */
	virtual UBOOL GenerateTerrainMaterialPropertyData(FTerrainMaterialResource& InMaterial, 
		EMaterialProperty InMaterialProperty, INT& InOutSizeX, INT& InOutSizeY, 
		TArray<FFloat16Color>& OutMaterialSamples);

	//
};

#endif	//__LIGHTMASSRENDER
