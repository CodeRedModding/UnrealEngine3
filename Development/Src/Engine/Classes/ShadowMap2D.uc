/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ShadowMap2D extends Object
	native
	noexport;

/** The texture which contains the shadow-map data. */
var private const ShadowMapTexture2D Texture;

/** The scale which is applied to the shadow-map coordinates before sampling the shadow-map textures. */
var private const Vector2D CoordinateScale;

/** The bias which is applied to the shadow-map coordinates before sampling the shadow-map textures. */
var private const Vector2D CoordinateBias;

/** The GUID of the light which this shadow-map is for. */
var private const Guid LightGuid;

/** Indicates whether the texture contains shadow factors (0 for shadowed, 1 for unshadowed) or signed distance field values. */
var private const bool bIsShadowFactorTexture;

/** Optional instanced mesh component this shadowmap is used with */
var private transient InstancedStaticMeshComponent Component;

/** Optional instance index this shadowmap is used with. If this is non-zero, this shadowmap object is temporary */
var private transient int InstanceIndex;

defaultproperties
{
    bIsShadowFactorTexture=True
}