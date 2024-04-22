/*=============================================================================
	HModel.h: HModel definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_HMODEL
#define _INC_HMODEL

class UModelComponent;


/**
 * A hit proxy representing a UModel.
 */
class HModel : public HHitProxy
{
	DECLARE_HIT_PROXY(HModel,HHitProxy);
public:

	/** Initialization constructor. */
	HModel(UModelComponent* InComponent, UModel* InModel):
		Component(InComponent),
		Model(InModel)
	{}

	/**
	 * Finds the surface at the given screen coordinates of a view family.
	 * @return True if a surface was found.
	 */
	UBOOL ResolveSurface(const FSceneView* View,INT X,INT Y,UINT& OutSurfaceIndex) const;

	// HHitProxy interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Component;
		Ar << Model;
	}
	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}

	// Accessors.
	UModelComponent* GetModelComponent() const { return Component; }
	UModel* GetModel() const { return Model; }

private:
	
	UModelComponent* Component;
	UModel* Model;
};

#endif
