/**
 *	ParticleModuleTypeDataTrail2
 *	Provides the base data for trail emitters.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleTypeDataTrail2 extends ParticleModuleTypeDataBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

//*************************************************************************************************
// General Trail Variables
//*************************************************************************************************
/** The tessellation amount to use for each trail				*/
var(Trail)		int		TessellationFactor;

/** The distance between particles for full TessellationFactor	*/
var				float	TessellationFactorDistance;

/** The strength to apply to the tangents						*/
var(Trail)		float	TessellationStrength;

/** The number of times to tile the texture along each trail	*/
var(Trail)		int		TextureTile;

/** The number of sheets to render								*/
var				int		Sheets;

/** The number of live trails									*/
var(Trail)		int		MaxTrailCount;

/** Max particles per trail										*/
var(Trail)		int		MaxParticleInTrailCount;

/** If true, do not join the trail to the source position 		*/
var(Trail)		bool 	bClipSourceSegement;

/**
 *	If true, clear the tangents (old bug behavior)
 *	This is present to preserve existing content...
 */
var(Trail)		bool	bClearTangents;

//*************************************************************************************************
// Trail Rendering Variables
//*************************************************************************************************
var(Rendering)	bool	RenderGeometry;
var(Rendering)	bool	RenderDirectLine;
var(Rendering)	bool	RenderLines;
var(Rendering)	bool	RenderTessellation;

//*************************************************************************************************
// C++ Text
//*************************************************************************************************
cpptext
{
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);

	// Trail
	virtual void	GetDataPointers(FParticleEmitterInstance* Owner, const BYTE* ParticleBase, 
			INT& CurrentOffset, FTrail2TypeDataPayload*& TrailData, FLOAT*& TaperValues);
	virtual void	GetDataPointerOffsets(FParticleEmitterInstance* Owner, const BYTE* ParticleBase, 
			INT& CurrentOffset, INT& TrailDataOffset, INT& TaperValuesOffset);
}

//*************************************************************************************************
// Default properties
//*************************************************************************************************
defaultproperties
{
	TessellationFactor=1
	TessellationFactorDistance=0.0
	TessellationStrength=25.0
	TextureTile=1
	Sheets=1
	MaxTrailCount=1
	MaxParticleInTrailCount=0

	RenderGeometry=TRUE
	RenderDirectLine=FALSE
	RenderLines=FALSE
	RenderTessellation=FALSE

	bClipSourceSegement=FALSE

	bClearTangents=TRUE
}
