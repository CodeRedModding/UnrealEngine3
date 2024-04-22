///////////////////////////////////////////////////////////////////////  
//  Utility Shaders
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with 
//  the terms of that agreement.
//
//      Copyright (c) 2003-2008 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      Web: http://www.idvinc.com


///////////////////////////////////////////////////////////////////////  
//  Modulate_Float
//
//  Returns x % y (some compilers generate way too many instructions when
//  using the native '%' operator)

float Modulate_Float(float x, float y)
{
    return x - (int(x / y) * y);
}


///////////////////////////////////////////////////////////////////////  
//  WindMatrixLerp
//
//	In order to achieve a bending branch effect, each vertex is multiplied
//	by a wind matrix.  This function interpolates between a static point
//	and a fully wind-effected point by the wind weight passed in.

void WindMatrixLerp(inout float3 vCoord, int nIndex, float fWeight)
{
#ifdef SPEEDTREE_OPENGL
	vCoord = lerp(vCoord, mul(vCoord, float3x3(g_amWindMatrices[nIndex])), fWeight);
#else
	vCoord = lerp(vCoord, mul(vCoord, g_amWindMatrices[nIndex]), fWeight);
#endif
}


///////////////////////////////////////////////////////////////////////  
//  WindEffect
//
//  SpeedTree uses a two-weight wind system that allows the tree model
//  to bend at more than one branch level.
//
//  In order to keep the vertex size small, the four wind parameters are
//	passed using two float values.  It assumes that wind weights are
//  in the range [0,1].  The 0.98 scalar (c_fWeightRange in SpeedTreeWrapper.cpp)
//  maps this range to just less than 1.0, ensuring that the weight will always 
//	be fractional (with the matrix index in the integer part)
//
//      vWindInfo.x = wind_matrix_index1 + wind_weight1 * 0.98
//      vWindInfo.y = wind_matrix_index2 + wind_weight2 * 0.98
//
//	* Caution: Negative wind weights will not work with this scheme.  We rely on the
//		       fact that the SpeedTreeRT library clamps wind weights to [0.0, 1.0]


// enable a second-level wind effect, enabling the tree to essentially "bend" at more
// than one level; can be fairly expensive, especially with SPEEDTREE_ACCURATE_WIND_LIGHTING
// enabled
#define SPEEDTREE_TWO_WEIGHT_WIND


void WindEffect_Normal_Tangent(inout float3 vPosition, inout float3 vNormal, inout float3 vTangent, float2 vWindInfo)
{
	// extract the wind indices & weights:
	//   vWeights.x = [0.0, 0.98] wind weight for wind matrix 1
	//	 vWeights.y = [0.0, 0.98] wind weight for wind matrix 2
	//	 vIndices.x = integer index for wind matrix 1
	//	 vIndices.y = integer index for wind matrix 2
	float2 vWeights = frac(vWindInfo.xy);
	float2 vIndices = vWindInfo.xy - vWeights;

	// this one instruction helps keep two instances of the same base tree from being in
	// sync in their wind behavior; each instance has a unique matrix offset 
	// (g_fWindMatrixOffset.x) which helps keep them from using the same set of 
	// matrices for wind
	vIndices = fmod(vIndices + g_fWindMatrixOffset.xx, SPEEDTREE_NUM_WIND_MATRICES);
    
	// first-level wind effect - interpolate between static position and fully-blown
	// wind position by the wind weight value
	WindMatrixLerp(vPosition, int(vIndices.x), vWeights.x);
	WindMatrixLerp(vNormal, int(vIndices.x), vWeights.x);
	WindMatrixLerp(vTangent, int(vIndices.x), vWeights.x);
    
	// second-level wind effect - interpolate between first-level wind position and 
	// the fully-blown wind position by the second wind weight value
#ifdef SPEEDTREE_TWO_WEIGHT_WIND
	WindMatrixLerp(vPosition, int(vIndices.y), vWeights.y);
	WindMatrixLerp(vNormal, int(vIndices.y), vWeights.y);
	WindMatrixLerp(vTangent, int(vIndices.y), vWeights.y);
#endif
}


void WindEffect_Normal(inout float3 vPosition, inout float3 vNormal, float2 vWindInfo)
{
	// extract the wind indices & weights:
	//   vWeights.x = [0.0, 0.98] wind weight for wind matrix 1
	//	 vWeights.y = [0.0, 0.98] wind weight for wind matrix 2
	//	 vIndices.x = integer index for wind matrix 1
	//	 vIndices.y = integer index for wind matrix 2
	float2 vWeights = frac(vWindInfo.xy);
	float2 vIndices = vWindInfo.xy - vWeights;

	// this one instruction helps keep two instances of the same base tree from being in
	// sync in their wind behavior; each instance has a unique matrix offset 
	// (g_fWindMatrixOffset.x) which helps keep them from using the same set of 
	// matrices for wind
	vIndices = fmod(vIndices + g_fWindMatrixOffset.xx, SPEEDTREE_NUM_WIND_MATRICES);
    
	// first-level wind effect - interpolate between static position and fully-blown
	// wind position by the wind weight value
	WindMatrixLerp(vPosition, int(vIndices.x), vWeights.x);
	WindMatrixLerp(vNormal, int(vIndices.x), vWeights.x);
    
	// second-level wind effect - interpolate between first-level wind position and 
	// the fully-blown wind position by the second wind weight value
#ifdef SPEEDTREE_TWO_WEIGHT_WIND
	WindMatrixLerp(vPosition, int(vIndices.y), vWeights.y);
	WindMatrixLerp(vNormal, int(vIndices.y), vWeights.y);
#endif
}


void WindEffect(inout float3 vPosition, float2 vWindInfo)
{
	// extract the wind indices & weights:
	//   vWeights.x = [0.0, 0.98] wind weight for wind matrix 1
	//	 vWeights.y = [0.0, 0.98] wind weight for wind matrix 2
	//	 vIndices.x = integer index for wind matrix 1
	//	 vIndices.y = integer index for wind matrix 2
	float2 vWeights = frac(vWindInfo.xy);
	float2 vIndices = vWindInfo.xy - vWeights;

	// this one instruction helps keep two instances of the same base tree from being in
	// sync in their wind behavior; each instance has a unique matrix offset 
	// (g_fWindMatrixOffset.x) which helps keep them from using the same set of 
	// matrices for wind
	vIndices = fmod(vIndices + g_fWindMatrixOffset.xx, SPEEDTREE_NUM_WIND_MATRICES);
    
	// first-level wind effect - interpolate between static position and fully-blown
	// wind position by the wind weight value
	WindMatrixLerp(vPosition, int(vIndices.x), vWeights.x);
    
	// second-level wind effect - interpolate between first-level wind position and 
	// the fully-blown wind position by the second wind weight value
#ifdef SPEEDTREE_TWO_WEIGHT_WIND
	WindMatrixLerp(vPosition, int(vIndices.y), vWeights.y);
#endif
}


///////////////////////////////////////////////////////////////////////  
//  Self-Shadow motion
//
//	MoveSelfShadow() uses the first wind matrix to govern the motion of 
//	the self-shadow texture by multiplying it against the texcoords; it 
//	must be dramatically scaled down (X 0.01) to keep the effect realistic.
//
//	This is an effective technique, especially with local wind effects, to
//	keep each instance's texcoords potentially unique in their motion.

#ifdef SPEEDTREE_SELF_SHADOW_LAYER
float2 MoveSelfShadow(float2 vSelfShadowCoords)
{
#ifdef SPEEDTREE_OPENGL
	#ifdef SPEEDTREE_UPVECTOR_Y
		return lerp(vSelfShadowCoords, mul(float3(vSelfShadowCoords, 0.0f), float3x3(g_amWindMatrices[0])).xz, 0.05f);
	#else
		return lerp(vSelfShadowCoords, mul(float3(vSelfShadowCoords, 0.0f), float3x3(g_amWindMatrices[0])).xy, 0.05f);
	#endif
#else
	#ifdef SPEEDTREE_UPVECTOR_Y
		return lerp(vSelfShadowCoords, mul(float3(vSelfShadowCoords, 0.0f), g_amWindMatrices[0]).xz, 0.05f);
	#else
		return lerp(vSelfShadowCoords, mul(float3(vSelfShadowCoords, 0.0f), g_amWindMatrices[0]).xy, 0.05f);
	#endif
#endif
}
#endif


///////////////////////////////////////////////////////////////////////  
//  LightDiffuse
//
//  Very simple lighting equation, used by the fronds and leaves (branches
//  and billboards are normal mapped).

float3 LightDiffuse(float3 vVertex,
                    float3 vNormal,
                    float3 vLightDir,
                    float3 vLightColor,
                    float3 vDiffuseMaterial)
{
    return vDiffuseMaterial * vLightColor * max(dot(vNormal, vLightDir), 0.0f);
}


///////////////////////////////////////////////////////////////////////  
//  LightDiffuse_Capped
//
//  Slightly modified version of LightDiffuse, used by the leaf shader in
//  order to cap the dot contribution

float3 LightDiffuse_Capped(float3 vVertex,
                           float3 vNormal,
                           float3 vLightDir,
                           float3 vLightColor,
                           float3 vDiffuseMaterial)
{
    float fDotProduct = max(dot(vNormal, vLightDir), 0.0f);
    fDotProduct = lerp(fDotProduct, 1.0f, g_fLeafLightingAdjust);
    
    return vDiffuseMaterial * vLightColor * fDotProduct;
}


///////////////////////////////////////////////////////////////////////  
//  FogValue
//
//  Simple LINEAR fog computation.  If an exponential equation is desired,
//  it can be placed here - all of the shaders call this one function.

#ifdef SPEEDTREE_USE_FOG
float FogValue(float fPoint)
{
    float fFogEnd = g_vFogParams.y;
    float fFogDist = g_vFogParams.z;
    
    return saturate((fFogEnd - fPoint) / fFogDist);
}
#endif


///////////////////////////////////////////////////////////////////////  
//  RotationMatrix_zAxis
//
//  Constructs a Z-axis rotation matrix

float3x3 RotationMatrix_zAxis(float fAngle)
{
    // compute sin/cos of fAngle
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float3x3(vSinCos.y, -vSinCos.x, 0.0f, 
                    vSinCos.x, vSinCos.y, 0.0f, 
                    0.0f, 0.0f, 1.0f);
}


///////////////////////////////////////////////////////////////////////  
//  Rotate_zAxis
//
//  Returns an updated .xy value

float2 Rotate_zAxis(float fAngle, float3 vPoint)
{
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float2(dot(vSinCos.yx, vPoint.xy), dot(float2(-vSinCos.x, vSinCos.y), vPoint.xy));
}


///////////////////////////////////////////////////////////////////////  
//  RotationMatrix_yAxis
//
//  Constructs a Y-axis rotation matrix

float3x3 RotationMatrix_yAxis(float fAngle)
{
    // compute sin/cos of fAngle
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float3x3(vSinCos.y, 0.0f, vSinCos.x,
                    0.0f, 1.0f, 0.0f,
                    -vSinCos.x, 0.0f, vSinCos.y);
}


///////////////////////////////////////////////////////////////////////  
//  Rotate_yAxis
//
//  Returns an updated .xz value

float2 Rotate_yAxis(float fAngle, float3 vPoint)
{
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float2(dot(float2(vSinCos.y, -vSinCos.x), vPoint.xz), dot(vSinCos.xy, vPoint.xz));
}


///////////////////////////////////////////////////////////////////////  
//  RotationMatrix_xAxis
//
//  Constructs a X-axis rotation matrix

float3x3 RotationMatrix_xAxis(float fAngle)
{
    // compute sin/cos of fAngle
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float3x3(1.0f, 0.0f, 0.0f,
                    0.0f, vSinCos.y, -vSinCos.x,
                    0.0f, vSinCos.x, vSinCos.y);
}


///////////////////////////////////////////////////////////////////////  
//  Rotate_xAxis
//
//  Returns an updated .yz value

float2 Rotate_xAxis(float fAngle, float3 vPoint)
{
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float2(dot(vSinCos.yx, vPoint.yz), dot(float2(-vSinCos.x, vSinCos.y), vPoint.yz));
}


#ifdef SPEEDTREE_BRANCH_FADING
///////////////////////////////////////////////////////////////////////  
//  ComputeFadeValue

float ComputeFadeValue(float fHint)
{
    float fCurrentLod = g_vBranchLodValues.x;
    float fLodRadius = g_vBranchLodValues.y;
    
    float fValue = (fCurrentLod - fHint) / fLodRadius;
    
    // we don't scale the fade from 0.0 to 1.0 because the range typically used by
    // SpeedTree for its alpha fizzle/fade effect is c_fOpaqueAlpha (84) to 
    // c_fClearAlpha (255), making the low end 84 / 255 instead of 0.
    //
    // 0.33f = c_fOpaqueAlpha / 255.0f
    return lerp(0.33f, 1.0f, clamp(fValue, 0.0f, 1.0f));
}
#endif


#define DIRECTIONAL

///////////////////////////////////////////////////////////////////////  
//  Wind
//
//  This function positions any tree geometry based on their untransformed position and 4 wind floats.

float4 Wind(float4 vPos, float4 vWindData)
{
	// get the oscillation times (they changed independently to allow smooth frequency changes in multiple components)
	float fPrimaryTime = g_vWindTimes.x;
	float fSecondaryTime = g_vWindTimes.y;

	// compute how much the height contributes
	float fAdjust = max(vPos.z, 0.0f) * g_vWindDistances.z;
	if (fAdjust != 0.0f)
		fAdjust = pow(fAdjust, g_vWindDistances.w);

	// move a bare minimum due to gusting/strength
	float fMoveAmount = g_vWindGust.y;

	// primary oscillation
	fMoveAmount += g_vWindDistances.x * sin(fPrimaryTime * 0.3f) * cos(fPrimaryTime * 0.95f);
	fMoveAmount *= fAdjust;

	// xy component
	vPos.xy += g_vWindDir.xy * fMoveAmount;

	// move down a little to hide the sliding effect
	vPos.z -= fMoveAmount * g_vWindGustHints.x;

	// secondary oscillation
	float fOsc = sin((fSecondaryTime + vWindData.w) * 0.3f) * cos((fSecondaryTime + vWindData.w) * 0.95f);

	// reported wind direction (this vector is not normalized and shouldn't be!)
	float3 vDir = vWindData.xyz;

#ifdef DIRECTIONAL
	float3 vNewWindDir = g_vWindDir;

	// adjust based on artist's tuning
	vNewWindDir.z += g_vWindGustHints.y;
	vNewWindDir = normalize(vNewWindDir);

	// how long is it?  this length controls how much it should oscillate
	float fLength = length(vDir);

	// make it oscillate as much as it would have if it wasn't going with the wind
	vNewWindDir *= fLength;

	// add the normal term and the 'with the wind term'
	float fDirectionality = g_vWindGust.x * g_vWindGustHints.z;
	vPos.xyz += (1.0f - fDirectionality) * vDir.xyz * fOsc * g_vWindDistances.y;
	vPos.xyz += fDirectionality * vNewWindDir.xyz * lerp(fOsc, 1.0f, g_vWindGustHints.z) * g_vWindDistances.y * g_vWindGust.z;
#else
	vPos.xyz += vDir.xyz * fOsc * g_vWindDistances.y;
#endif

	return vPos;
}


///////////////////////////////////////////////////////////////////////  
//  LeafWind

float4 LeafWind(float4 vPos, inout float3 vDirection, float fScale)
{
	float2 vDir = -normalize(vPos.xy);
	float fDot = saturate(dot(vDir.xy, g_vWindDir.xy));
	//fDot *= fDot;
	float fDirContribution = 0.5f + fDot * 2.0f;
	fDirContribution *= g_vWindLeaves.z;

	float fLeavesTime = g_vWindTimes.w;

	float fMoveAmount = (g_vWindLeaves.x + fDirContribution * g_vWindLeaves.x) * sin(fLeavesTime + vDirection.y * g_vWindLeaves.w);
	vPos.xyz += vDirection.xyz * fMoveAmount * fScale;

	vDirection += float3(0.0f, 0.0f, fMoveAmount * g_vWindLeaves.y);
	vDirection = normalize(vDirection);

	return vPos;
}