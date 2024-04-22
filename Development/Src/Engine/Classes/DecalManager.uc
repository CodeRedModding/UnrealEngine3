/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DecalManager extends Actor
	native(Decal)
	config(Game);

/** template to base pool components off of - should not be used for decals or attached to anything */
var protected DecalComponent DecalTemplate;
/** components currently in the pool */
var array<DecalComponent> PoolDecals;
/** maximum allowed active components - if this is greater than 0 and is exceeded, the oldest active decal is taken */
var int MaxActiveDecals;
/** default lifetime for decals */
var globalconfig float DecalLifeSpan;
/** default depth bias offset */
var float DecalDepthBias;
/** default decal blend range */
var vector2D DecalBlendRange;

/** components currently active in the world and how much longer they will be */
struct native ActiveDecalInfo
{
	var DecalComponent Decal;
	var float LifetimeRemaining;
};
var array<ActiveDecalInfo> ActiveDecals;

cpptext
{
	virtual void TickSpecial(FLOAT DeltaTime);
}

/** @return whether dynamic decals are enabled */
native static final function bool AreDynamicDecalsEnabled();

/**
 * Called when the given decal's lifetime has run out
 * @note: caller responsible for removing from ActiveDecals array (this is to prevent code iterating the array from having dependencies on this function)
 */
event DecalFinished(DecalComponent Decal)
{
	if (Decal != None)
	{
		// clear it and return it to the pool
		Decal.ResetToDefaults();
		PoolDecals[PoolDecals.length] = Decal;
	}
}

/** @return whether spawning decals is allowed right now */
function bool CanSpawnDecals()
{
	return AreDynamicDecalsEnabled();
}


/**
 * This will set all of the various decal parameters.  This is the function that should be updated when there are new
 * Decal Paramaters that exist that should be updated by game code
 *
 * @param InExistingDecal If you have an existing decal that you want to set all the params on.
 * @param DecalMaterial the material to use for the decal
 * @param Width decal width
 * @param Height decal height
 * @param Thickness decal thickness (used to calculate the nearplane/farplane values)
 * @param bNoClip if true, use the bNoClip code path for decal generation (requires DecalMaterial to have clamped texture coordinates)
 * @param DecalRotation rotation of the decal in degrees
 * @param HitComponent if specified, will only project on this component (optimization)
 * @param bProjectOnTerrain whether decal can project on terrain (default true)
 * @param bProjectOnTerrain whether decal can project on skeletal meshes (default false)
 * @param HitBone if HitComponent is a skeletal mesh, the bone that was hit
 * @param HitNodeIndex if HitComponent is BSP, the node that was hit
 * @param HitLevelIndex if HitComponent is BSP, the index of the level whose BSP was hit
 * @param InFracturedStaticMeshComponentIndex The index of the FracturedMesh component.  -1/INDEX_NONE if the decal should project onto both the shell and core of the FracturedMeshActor
 * @param DepthBias depth bias offset to control z-fighting
 * @param BlendRange Start/End blend range specified as an angle in degrees. Controls where to start blending out the decal on a surface
 *
 **/
static final function SetDecalParameters( DecalComponent TheDecal,
						MaterialInterface DecalMaterial,
						vector DecalLocation,
						rotator DecalOrientation,
						float Width,
						float Height,
						float Thickness,
						bool bNoClip,
						float DecalRotation,
						PrimitiveComponent HitComponent,
						bool bProjectOnTerrain,
						bool bProjectOnSkeletalMeshes,
						name HitBone,
						int HitNodeIndex,
						int HitLevelIndex,
						int InFracturedStaticMeshComponentIndex,
						float DepthBias,
						vector2D BlendRange
						)
{
	// set the decal's data
	TheDecal.Location = DecalLocation;
	TheDecal.Orientation = DecalOrientation;
	TheDecal.DecalRotation = DecalRotation;
	TheDecal.Width = Width;
	TheDecal.Height = Height;
	// the thickness is just divided in half to create the far/near plane
	TheDecal.FarPlane = Thickness * 0.5;
	TheDecal.NearPlane = -TheDecal.FarPlane;
	TheDecal.bNoClip = bNoClip;
	TheDecal.HitComponent = HitComponent;
	TheDecal.HitBone = HitBone;
	TheDecal.HitNodeIndex = HitNodeIndex;
	TheDecal.HitLevelIndex = HitLevelIndex;
	TheDecal.SetDecalMaterial(DecalMaterial);
	TheDecal.bProjectOnTerrain = bProjectOnTerrain;
	TheDecal.bProjectOnSkeletalMeshes = bProjectOnSkeletalMeshes;
	TheDecal.FracturedStaticMeshComponentIndex = InFracturedStaticMeshComponentIndex;
 	TheDecal.DepthBias = DepthBias;
	TheDecal.BlendRange = BlendRange;
}

/** @return a decal from the pool, or a new one if the pool was empty */
protected function DecalComponent GetPooledComponent()
{
	local int i;
	local DecalComponent Result;

	// try to grab one from the pool
	while (PoolDecals.length > 0)
	{
		i = PoolDecals.length - 1;
		Result = PoolDecals[i];
		PoolDecals.Remove(i, 1);
		if (Result != None && !Result.IsPendingKill() && !Result.IsWaitingForResetToDefaultsToComplete())
		{
			break;
		}
		else
		{
			Result = None;
		}
	}

	if (Result == None)
	{
		// if pool is empty, get started expiring something
		if (MaxActiveDecals > 0 && ActiveDecals.length >= MaxActiveDecals)
		{
			// expire oldest decal
			Result = ActiveDecals[0].Decal;
			Result.ResetToDefaults();
			ActiveDecals.Remove(0, 1);
		}

		Result = new(self) DecalTemplate.Class(DecalTemplate);
	}

	return Result;
}

/**
 * Spawns a decal with the given parameters, taking a component from the pool or creating as necessary.
 *
 * @note: the component is returned so the caller can perform any additional modifications (parameters, etc),
 * 	but it shouldn't keep the reference around as the component will be returned to the pool as soon as the lifetime runs out
 *
 * @param DecalMaterial the material to use for the decal
 * @param Width decal width
 * @param Height decal height
 * @param Thickness decal thickness (used to calculate the nearplane/farplane values)
 * @param bNoClip if true, use the bNoClip code path for decal generation (requires DecalMaterial to have clamped texture coordinates)
 * @param DecalRotation (opt) rotation of the decal in degrees
 * @param HitComponent (opt) if specified, will only project on this component (optimization)
 * @param bProjectOnTerrain (opt) whether decal can project on terrain (default true)
 * @param bProjectOnTerrain (opt) whether decal can project on skeletal meshes (default false)
 * @param HitBone (opt) if HitComponent is a skeletal mesh, the bone that was hit
 * @param HitNodeIndex (opt) if HitComponent is BSP, the node that was hit
 * @param HitLevelIndex (opt) if HitComponent is BSP, the index of the level whose BSP was hit
 * @param InDecalLifeSpan lifetime for the decal
 * @param InFracturedStaticMeshComponentIndex The index of the FracturedMesh component.  -1/INDEX_NONE if the decal should project onto both the shell and core of the FracturedMeshActor
 * @param DepthBias depth bias offset to control z-fighting
 * @param BlendRange Start/End blend range specified as an angle in degrees. Controls where to start blending out the decal on a surface
  * @return the DecalComponent that will be used (may be None if dynamic decals are disabled)
 */
function DecalComponent SpawnDecal( MaterialInterface DecalMaterial,
						vector DecalLocation,
						rotator DecalOrientation,
						float Width,
						float Height,
						float Thickness,
						bool bNoClip,
						optional float DecalRotation = (FRand() * 360.0),
						optional PrimitiveComponent HitComponent,
						optional bool bProjectOnTerrain = TRUE,
						optional bool bProjectOnSkeletalMeshes,
						optional name HitBone,
						optional int HitNodeIndex = INDEX_NONE,
						optional int HitLevelIndex = INDEX_NONE,
						optional float InDecalLifeSpan = DecalLifeSpan,
						optional int InFracturedStaticMeshComponentIndex = INDEX_NONE,
						optional float InDepthBias = DecalDepthBias,
						optional vector2D InBlendRange = DecalBlendRange

						)
{
	local DecalComponent Result;
	local ActiveDecalInfo DecalInfo;

	// do nothing if decals are disabled
	if (!CanSpawnDecals())
	{
		return None;
	}

	Result = GetPooledComponent();

	// set the decal's data
	SetDecalParameters(
		Result,
		DecalMaterial,
		DecalLocation,
		DecalOrientation,
		Width,
		Height,
		Thickness,
		bNoClip,
		DecalRotation,
		HitComponent,
		bProjectOnTerrain,
		bProjectOnSkeletalMeshes,
		HitBone,
		HitNodeIndex,
		HitLevelIndex,
		INDEX_NONE,
		InDepthBias,
		InBlendRange
		);

	AttachComponent(Result);

	// add to list to tick lifetime
	DecalInfo.Decal = Result;
	DecalInfo.LifetimeRemaining = InDecalLifeSpan;
	ActiveDecals.AddItem(DecalInfo);

	return Result;
}

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	Begin Object Class=DecalComponent Name=BaseDecal
		bIgnoreOwnerHidden=TRUE // this is needed a the owner of this decal is "hidden" as it is a global entity @see UDecalComponent::IsEnabled()
	End Object
	DecalTemplate=BaseDecal

	DecalDepthBias=-0.00006
	DecalBlendRange=(X=89.5,Y=180)
}
