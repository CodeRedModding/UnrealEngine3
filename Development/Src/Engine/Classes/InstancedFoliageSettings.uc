/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InstancedFoliageSettings extends Object
	native(Foliage)
	hidecategories(Object)
	editinlinenew;

var(Painting) float Density;
var(Painting) float Radius;
var(Painting) float ScaleMinX;
var(Painting) float ScaleMinY;
var(Painting) float ScaleMinZ;
var(Painting) float ScaleMaxX;
var(Painting) float ScaleMaxY;
var(Painting) float ScaleMaxZ;
var(Painting) bool LockScaleX;
var(Painting) bool LockScaleY;
var(Painting) bool LockScaleZ;
var(Painting) float AlignMaxAngle;
var(Painting) float RandomPitchAngle;
var(Painting) float GroundSlope;
var(Painting) float HeightMin;
var(Painting) float HeightMax;
var(Painting) Name LandscapeLayer;
var(Painting) bool AlignToNormal;
var(Painting) bool RandomYaw;
var(Painting) bool UniformScale;
var(Painting) float ZOffsetMin;
var(Painting) float ZOffsetMax;

var(Clustering) int MaxInstancesPerCluster;
var(Clustering) float MaxClusterRadius;

var float ReapplyDensityAmount;
var bool ReapplyDensity;
var bool ReapplyRadius;
var bool ReapplyAlignToNormal;
var bool ReapplyRandomYaw;
var bool ReapplyScaleX;
var bool ReapplyScaleY;
var bool ReapplyScaleZ;
var bool ReapplyRandomPitchAngle;
var bool ReapplyGroundSlope;
var bool ReapplyHeight;
var bool ReapplyLandscapeLayer;
var bool ReapplyZOffset;

enum FoliageCullOption
{
	FOLIAGECULL_Cull,
	FOLIAGECULL_ScaleZ,
	FOLIAGECULL_ScaleXYZ,
	FOLIAGECULL_TranslateZ
};

var(Culling) int StartCullDistance;
var(Culling) int EndCullDistance;
var(Culling) FoliageCullOption CullOption;
var(Culling) EDetailMode DetailMode;

var(Lighting)	bool		CastShadow;
var(Lighting)	bool		bCastDynamicShadow;
var(Lighting)	bool		bCastStaticShadow;
var(Lighting)	bool		bSelfShadowOnly;
var(Lighting)	bool		bNoModSelfShadow;
var(Lighting)	bool		bAcceptsDynamicDominantLightShadows;
var(Lighting)	bool		bCastHiddenShadow;
var(Lighting)	bool		bCastShadowAsTwoSided;
var(Lighting)	const bool	bAcceptsLights;
var(Lighting)	const bool	bAcceptsDynamicLights;
var(Lighting)	const bool  bUseOnePassLightingOnTranslucency;
var(Lighting)	const bool	bUsePrecomputedShadows;

var(Collision)	bool		bCollideActors;
var(Collision)	bool		bBlockActors;
var(Collision)	bool		bBlockNonZeroExtent;
var(Collision)	bool		bBlockZeroExtent;

var int DisplayOrder;
var bool IsSelected;
var bool ShowNothing;
var bool ShowPaintSettings;
var bool ShowInstanceSettings;

defaultproperties
{
	Density=100.0
	Radius=0.0
	AlignToNormal=true
	RandomYaw=true
	UniformScale=true
	ScaleMinX=1.0
	ScaleMinY=1.0
	ScaleMinZ=1.0
	ScaleMaxX=1.0
	ScaleMaxY=1.0
	ScaleMaxZ=1.0
	AlignMaxAngle=0.0
	RandomPitchAngle=0.0
	GroundSlope=45.0
	HeightMin=-262144.0
	HeightMax=262144.0
	ZOffsetMin=0.0
	ZOffsetMax=0.0
	LandscapeLayer=None
	MaxInstancesPerCluster=100
	MaxClusterRadius=10000.0
	DisplayOrder=0
	IsSelected=false
	ShowNothing=false
	ShowPaintSettings=true
	ShowInstanceSettings=false
	ReapplyDensityAmount=1.0

	CastShadow=TRUE
	bCastDynamicShadow=TRUE
	bCastStaticShadow=TRUE
	bAcceptsDynamicDominantLightShadows=TRUE
	bAcceptsLights=TRUE
	bAcceptsDynamicLights=TRUE
	bUsePrecomputedShadows=TRUE
}
