/*=============================================================================
	UnPostProcess.cpp: 
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
 
IMPLEMENT_CLASS(UPostProcessEffect);
IMPLEMENT_CLASS(UPostProcessChain);
IMPLEMENT_CLASS(APostProcessVolume);

namespace
{
	static const FName NAME_EnableBloom								= FName( TEXT("bEnableBloom") );
	static const FName NAME_Bloom_Scale								= FName( TEXT("Bloom_Scale") );
	static const FName NAME_Bloom_Threshold							= FName( TEXT("Bloom_Threshold") );
	static const FName NAME_Bloom_Tint								= FName( TEXT("Bloom_Tint") );
	static const FName NAME_Bloom_ScreenBlendThreshold				= FName( TEXT("Bloom_ScreenBlendThreshold") );
	static const FName NAME_Bloom_InterpolationDuration				= FName( TEXT("Bloom_InterpolationDuration") );

	static const FName NAME_EnableDOF								= FName( TEXT("bEnableDOF") );
	static const FName NAME_DOF_FalloffExponent						= FName( TEXT("DOF_FalloffExponent") );
	static const FName NAME_DOF_BlurKernelSize						= FName( TEXT("DOF_BlurKernelSize") );
	static const FName NAME_DOF_BlurBloomKernelSize					= FName( TEXT("DOF_BlurBloomKernelSize") );
	static const FName NAME_DOF_MaxNearBlurAmount					= FName( TEXT("DOF_MaxNearBlurAmount") );
	static const FName NAME_DOF_MinBlurAmount						= FName( TEXT("DOF_MinBlurAmount") );
	static const FName NAME_DOF_MaxFarBlurAmount					= FName( TEXT("DOF_MaxFarBlurAmount") );
	static const FName NAME_DOF_FocusType							= FName( TEXT("DOF_FocusType") );
	static const FName NAME_DOF_FocusInnerRadius					= FName( TEXT("DOF_FocusInnerRadius") );
	static const FName NAME_DOF_FocusDistance						= FName( TEXT("DOF_FocusDistance") );
	static const FName NAME_DOF_FocusPosition						= FName( TEXT("DOF_FocusPosition") );
	static const FName NAME_DOF_InterpolationDuration				= FName( TEXT("DOF_InterpolationDuration") );
	static const FName NAME_DOF_BokehTexture						= FName( TEXT("DOF_BokehTexture") );

	static const FName NAME_EnableMotionBlur						= FName( TEXT("bEnableMotionBlur") );
	static const FName NAME_MotionBlur_MaxVelocity					= FName( TEXT("MotionBlur_MaxVelocity") );
	static const FName NAME_MotionBlur_Amount						= FName( TEXT("MotionBlur_Amount") );
	static const FName NAME_MotionBlur_FullMotionBlur				= FName( TEXT("MotionBlur_FullMotionBlur") );
	static const FName NAME_MotionBlur_CameraRotationThreshold		= FName( TEXT("MotionBlur_CameraRotationThreshold") );
	static const FName NAME_MotionBlur_CameraTranslationThreshold	= FName( TEXT("MotionBlur_CameraTranslationThreshold") );
	static const FName NAME_MotionBlur_InterpolationDuration		= FName( TEXT("MotionBlur_InterpolationDuration") );

	static const FName NAME_EnableSceneEffect						= FName( TEXT("bEnableSceneEffect") );
	static const FName NAME_Scene_Desaturation						= FName( TEXT("Scene_Desaturation") );
	static const FName NAME_Scene_Colorize							= FName( TEXT("Scene_Colorize") );
	static const FName NAME_Scene_TonemapperScale					= FName( TEXT("Scene_TonemapperScale") );
	static const FName NAME_Scene_ImageGrainScale					= FName( TEXT("Scene_ImageGrainScale") );
	static const FName NAME_Scene_HighLights						= FName( TEXT("Scene_HighLights") );
	static const FName NAME_Scene_MidTones							= FName( TEXT("Scene_MidTones") );
	static const FName NAME_Scene_Shadows							= FName( TEXT("Scene_Shadows") );
	static const FName NAME_Scene_InterpolationDuration				= FName( TEXT("Scene_InterpolationDuration") );
	static const FName NAME_Scene_ColorGradingLUT					= FName( TEXT("Scene_ColorGradingLUT") );

	static const FName NAME_AllowAmbientOcclusion					= FName( TEXT("bAllowAmbientOcclusion") );

	static const FName NAME_OverrideRimShaderColor					= FName( TEXT("bOverrideRimShaderColor") );
	static const FName NAME_RimShader_Color							= FName( TEXT("RimShader_Color") );
	static const FName NAME_RimShader_InterpolationDuration			= FName( TEXT("RimShader_InterpolationDuration") );

	static const FName NAME_Mobile_BlurAmount						= FName( TEXT("MobilePostProcess.Mobile_BlurAmount") );
	static const FName NAME_Mobile_TransitionTime 					= FName( TEXT("MobilePostProcess.Mobile_TransitionTime") );
	static const FName NAME_Mobile_Bloom_Scale						= FName( TEXT("MobilePostProcess.Mobile_Bloom_Scale") );
	static const FName NAME_Mobile_Bloom_Threshold					= FName( TEXT("MobilePostProcess.Mobile_Bloom_Threshold") );
	static const FName NAME_Mobile_Bloom_Tint						= FName( TEXT("MobilePostProcess.Mobile_Bloom_Tint") );
	static const FName NAME_Mobile_DOF_Distance						= FName( TEXT("MobilePostProcess.Mobile_DOF_Distance") );
	static const FName NAME_Mobile_DOF_MinRange						= FName( TEXT("MobilePostProcess.Mobile_DOF_MinRange") );
	static const FName NAME_Mobile_DOF_MaxRange						= FName( TEXT("MobilePostProcess.Mobile_DOF_MaxRange") );
	static const FName NAME_Mobile_DOF_FarBlurFactor				= FName( TEXT("MobilePostProcess.Mobile_DOF_FarBlurFactor") );
}

/*-----------------------------------------------------------------------------
UPostProcessEffect
-----------------------------------------------------------------------------*/

/**
* Check if the effect should be shown
* @param View - current view
* @return TRUE if the effect should be rendered
*/
UBOOL UPostProcessEffect::IsShown(const FSceneView* View) const
{
	check(View);
	check(View->Family);

	if(
		( ( View->Family->ShowFlags & SHOW_PostProcess ) == 0 )
		|| ( !View->Family->ShouldPostProcess() )
	)
	{
		return FALSE;
	}
	else if( View->Family->ShowFlags & SHOW_Editor )
	{
		return bShowInEditor;
	}
	else
	{
		return bShowInGame;
	}
}

/*-----------------------------------------------------------------------------
FPostProcessSceneProxy
-----------------------------------------------------------------------------*/

/** 
* Initialization constructor. 
* @param InEffect - post process effect to mirror in this proxy
*/
FPostProcessSceneProxy::FPostProcessSceneProxy(const UPostProcessEffect* InEffect)
:	DepthPriorityGroup(InEffect ? InEffect->SceneDPG : SDPG_PostProcess)
,	FinalEffectInGroup(0)
,	bAffectsLightingOnly(InEffect ? InEffect->bAffectsLightingOnly : FALSE)
{
}

/*-----------------------------------------------------------------------------
APostProcessVolume
-----------------------------------------------------------------------------*/

/**
 * Routes ClearComponents call to Super and removes volume from linked list in world info.
 */
void APostProcessVolume::ClearComponents()
{
	// Route clear to super first.
	Super::ClearComponents();

	// GWorld will be NULL during exit purge.
	if( GWorld )
	{
		APostProcessVolume* CurrentVolume  = GWorld->GetWorldInfo()->HighestPriorityPostProcessVolume;
		APostProcessVolume*	PreviousVolume = NULL;

		// Iterate over linked list, removing this volume if found.
		while( CurrentVolume )
		{
			// Found.
			if( CurrentVolume == this )
			{
				// Remove from linked list.
				if( PreviousVolume )
				{
					PreviousVolume->NextLowerPriorityVolume = NextLowerPriorityVolume;
				}
				// Special case removal from first entry.
				else
				{
					GWorld->GetWorldInfo()->HighestPriorityPostProcessVolume = NextLowerPriorityVolume;
				}

				// BREAK OUT OF LOOP
				break;
			}
			// Further traverse linked list.
			else
			{
				PreviousVolume	= CurrentVolume;
				CurrentVolume	= CurrentVolume->NextLowerPriorityVolume;
			}
		}

		// Reset next pointer to avoid dangling end bits and also for GC.
		NextLowerPriorityVolume = NULL;
	}
}

/**
 * Routes UpdateComponents call to Super and adds volume to linked list in world info.
 */
void APostProcessVolume::UpdateComponentsInternal(UBOOL bCollisionUpdate)
{
	// Route update to super first.
	Super::UpdateComponentsInternal( bCollisionUpdate );

	APostProcessVolume* CurrentVolume  = GWorld->GetWorldInfo()->HighestPriorityPostProcessVolume;
	APostProcessVolume*	PreviousVolume = NULL;

	// First volume in the world info.
	if( CurrentVolume == NULL )
	{
		GWorld->GetWorldInfo()->HighestPriorityPostProcessVolume = this;
		NextLowerPriorityVolume	= NULL;
	}
	// Find where to insert in sorted linked list.
	else
	{
		// Avoid double insertion!
		while( CurrentVolume && CurrentVolume != this )
		{
			// We use > instead of >= to be sure that we are not inserting twice in the case of multiple volumes having
			// the same priority and the current one already having being inserted after one with the same priority.
			if( Priority > CurrentVolume->Priority )
			{
				// Special case for insertion at the beginning.
				if( PreviousVolume == NULL )
				{
					GWorld->GetWorldInfo()->HighestPriorityPostProcessVolume = this;
				}
				// Insert before current node by fixing up previous to point to current.
				else
				{
					PreviousVolume->NextLowerPriorityVolume = this;
				}
				// Point to current volume, finalizing insertion.
				NextLowerPriorityVolume = CurrentVolume;

				// BREAK OUT OF LOOP.
				break;
			}
			// Further traverse linked list.
			else
			{
				PreviousVolume	= CurrentVolume;
				CurrentVolume	= CurrentVolume->NextLowerPriorityVolume;
			}
		}

		// We're the lowest priority volume, insert at the end.
		if( CurrentVolume == NULL )
		{
			check( PreviousVolume );
			PreviousVolume->NextLowerPriorityVolume = this;
			NextLowerPriorityVolume = NULL;
		}
	}
}

/**
* Called when properties change.
*/
void APostProcessVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// clamp desaturation to 0..1
	Settings.Scene_Desaturation = Clamp(Settings.Scene_Desaturation, 0.f, 1.f);

	Super::PostEditChangeProperty(PropertyChangedEvent);

}

/**
* Called after this instance has been serialized.
*/
void APostProcessVolume::PostLoad()
{
	Super::PostLoad();

	// clamp desaturation to 0..1 (fixup for old data)
	Settings.Scene_Desaturation = Clamp(Settings.Scene_Desaturation, 0.f, 1.f);

	ULinkerLoad* LODLinkerLoad = GetLinker();
	if (LODLinkerLoad && (LODLinkerLoad->Ver() < VER_COLORGRADING2))
	{
		// Before the override flag was introduced the override state was derived if the texture was actually defined.
		Settings.bOverride_Scene_ColorGradingLUT = Settings.ColorGrading_LookupTable != 0;
	}
}

// helper macro for OverrideSettingsFor()
#define LERP_POSTPROCESS(Group, Name) \
	if( bOverride_##Group##_##Name ) \
	{ \
		ToOverride.Group##_##Name = Lerp(ToOverride.Group##_##Name, Group##_##Name, Alpha); \
		ToOverride.bOverride_##Group##_##Name= TRUE; \
	}

// helper macro for OverrideSettingsFor()
#define SET_POSTPROCESS(Group, Name) \
	if( bOverride_##Group##_##Name ) \
	{ \
		ToOverride.Group##_##Name = Group##_##Name; \
		ToOverride.bOverride_##Group##_##Name= TRUE; \
	}

// helper macro for OverrideSettingsFor()
#define LERP_MOBILEPOSTPROCESS(Name) \
	if ( MobilePostProcess.bOverride_##Name ) \
	{ \
		ToOverride.MobilePostProcess.Name = Lerp(ToOverride.MobilePostProcess.Name, MobilePostProcess.Name, Alpha); \
		ToOverride.MobilePostProcess.bOverride_##Name = TRUE; \
	}

/**
 * Blends the settings on this structure marked as override setting onto the given settings
 *
 * @param	ToOverride	The settings that get overridden by the overridable settings on this structure. 
 * @param	Alpha		The opacity of these settings. If Alpha is 1, ToOverride will equal this setting structure.
 */
void FPostProcessSettings::OverrideSettingsFor( FPostProcessSettings& ToOverride, FLOAT Alpha ) const
{
	// TOGGLE OVERRIDES
	if (Alpha <= 0.0f)
	{
		return;
	}
		
	// BLOOM OVERRIDES
	if( bOverride_EnableBloom )
	{
		ToOverride.bEnableBloom = bEnableBloom;
	}
	if (ToOverride.bEnableBloom)
	{
		LERP_POSTPROCESS(Bloom, Scale)
		LERP_POSTPROCESS(Bloom, Threshold)
		LERP_POSTPROCESS(Bloom, ScreenBlendThreshold)
		LERP_POSTPROCESS(Bloom, InterpolationDuration)

		if( bOverride_Bloom_Tint )
		{
			ToOverride.Bloom_Tint = Lerp(FLinearColor(ToOverride.Bloom_Tint), FLinearColor(Bloom_Tint), Alpha).ToFColor(TRUE);
			ToOverride.bOverride_Bloom_Tint = TRUE;
		}

		// this one is in the wrong category (DOF, should be bloom)
		LERP_POSTPROCESS(DOF, BlurBloomKernelSize)
	}

	// DEPTH OF FIELD OVERRIDES
	if( bOverride_EnableDOF )
	{
		ToOverride.bEnableDOF = bEnableDOF;
	}

	if (ToOverride.bEnableDOF)
	{		
		LERP_POSTPROCESS(DOF, FalloffExponent)
		LERP_POSTPROCESS(DOF, BlurKernelSize)
		LERP_POSTPROCESS(DOF, MaxNearBlurAmount)
		LERP_POSTPROCESS(DOF, MinBlurAmount)
		LERP_POSTPROCESS(DOF, MaxFarBlurAmount)
		SET_POSTPROCESS(DOF, FocusType)
		LERP_POSTPROCESS(DOF, FocusInnerRadius)
		LERP_POSTPROCESS(DOF, FocusDistance)
		LERP_POSTPROCESS(DOF, FocusPosition)
		LERP_POSTPROCESS(DOF, InterpolationDuration)
		SET_POSTPROCESS(DOF, BokehTexture)
	}

	// MOTION BLUR OVERRIDES
	if( bOverride_EnableMotionBlur )
	{
		ToOverride.bEnableMotionBlur = bEnableMotionBlur;
	}
	if (ToOverride.bEnableMotionBlur)
	{
		LERP_POSTPROCESS(MotionBlur, MaxVelocity)
		LERP_POSTPROCESS(MotionBlur, Amount)
		LERP_POSTPROCESS(MotionBlur, FullMotionBlur)
		LERP_POSTPROCESS(MotionBlur, CameraRotationThreshold)
		LERP_POSTPROCESS(MotionBlur, CameraTranslationThreshold)
		LERP_POSTPROCESS(MotionBlur, InterpolationDuration)
	}

	// SCENE EFFECT OVERRIDES
	if( bOverride_EnableSceneEffect )
	{
		ToOverride.bEnableSceneEffect = bEnableSceneEffect;
	}
	if (ToOverride.bEnableSceneEffect)
	{
		LERP_POSTPROCESS(Scene, HighLights)
		LERP_POSTPROCESS(Scene, MidTones)
		LERP_POSTPROCESS(Scene, Shadows)
		LERP_POSTPROCESS(Scene, Desaturation)
		LERP_POSTPROCESS(Scene, Colorize)
		LERP_POSTPROCESS(Scene, InterpolationDuration)
		LERP_POSTPROCESS(Scene, TonemapperScale)
		LERP_POSTPROCESS(Scene, ImageGrainScale)
	}

	if( bOverride_AllowAmbientOcclusion )
	{
		ToOverride.bAllowAmbientOcclusion = bAllowAmbientOcclusion;
	}

	// RIM SHADER OVERRIDES
	if( bOverride_OverrideRimShaderColor )
	{
		ToOverride.bOverrideRimShaderColor = bOverrideRimShaderColor;
	}
	if (ToOverride.bOverrideRimShaderColor)
	{
		LERP_POSTPROCESS(RimShader, Color)
		LERP_POSTPROCESS(RimShader, InterpolationDuration)
	}

	if(bOverride_Scene_ColorGradingLUT)
	{
		ToOverride.ColorGrading_LookupTable = ColorGrading_LookupTable;
		ToOverride.bOverride_Scene_ColorGradingLUT = TRUE;
	}

	// MOBILE COLOR GRADING OVERRIDES
	if ( bOverride_MobileColorGrading )
	{
		ToOverride.MobileColorGrading.TransitionTime= Lerp( ToOverride.MobileColorGrading.TransitionTime, MobileColorGrading.TransitionTime, Alpha );
		ToOverride.MobileColorGrading.Blend			= Lerp( ToOverride.MobileColorGrading.Blend, MobileColorGrading.Blend, Alpha );
		ToOverride.MobileColorGrading.Desaturation	= Lerp( ToOverride.MobileColorGrading.Desaturation, MobileColorGrading.Desaturation, Alpha );
		ToOverride.MobileColorGrading.HighLights	= Lerp( ToOverride.MobileColorGrading.HighLights, MobileColorGrading.HighLights, Alpha );
		ToOverride.MobileColorGrading.MidTones		= Lerp( ToOverride.MobileColorGrading.MidTones, MobileColorGrading.MidTones, Alpha );
		ToOverride.MobileColorGrading.Shadows		= Lerp( ToOverride.MobileColorGrading.Shadows, MobileColorGrading.Shadows, Alpha );
	}

	// MOBILE POST-PROCESS SETTINGS OVERRIDES
	if ( ToOverride.bEnableDOF || ToOverride.bEnableBloom )
	{
		LERP_MOBILEPOSTPROCESS( Mobile_BlurAmount );
		LERP_MOBILEPOSTPROCESS( Mobile_TransitionTime );
		LERP_MOBILEPOSTPROCESS( Mobile_Bloom_Scale );
		LERP_MOBILEPOSTPROCESS( Mobile_Bloom_Threshold );
		LERP_MOBILEPOSTPROCESS( Mobile_Bloom_Tint );
		LERP_MOBILEPOSTPROCESS( Mobile_DOF_Distance );
		LERP_MOBILEPOSTPROCESS( Mobile_DOF_MinRange );
		LERP_MOBILEPOSTPROCESS( Mobile_DOF_MaxRange );
		LERP_MOBILEPOSTPROCESS( Mobile_DOF_FarBlurFactor );
	}
}

#undef LERP_POSTPROCESS
#undef SET_POSTPROCESS
#undef LERP_MOBILEPOSTPROCESS

/**
 * Enables the override setting for the given post-process setting.
 *
 * @param	PropertyName	The post-process property name to enable.
 */
void FPostProcessSettings::EnableOverrideSetting( const FName& PropertyName )
{
	if( PropertyName == NAME_EnableBloom )
	{
		EnableBloom();
		return;
	}

	if( PropertyName == NAME_Bloom_Scale )
	{
		bOverride_Bloom_Scale = TRUE;
		EnableBloom();
		return;
	}

	if( PropertyName == NAME_Bloom_Threshold )
	{
		bOverride_Bloom_Threshold = TRUE;
		EnableBloom();
		return;
	}

	if( PropertyName == NAME_Bloom_Tint )
	{
		bOverride_Bloom_Tint = TRUE;
		EnableBloom();
		return;
	}

	if( PropertyName == NAME_Bloom_ScreenBlendThreshold )
	{
		bOverride_Bloom_ScreenBlendThreshold = TRUE;
		EnableBloom();
		return;
	}

	if( PropertyName == NAME_Bloom_InterpolationDuration )
	{
		bOverride_Bloom_InterpolationDuration = TRUE;
		EnableBloom();
		return;
	}

	if( PropertyName == NAME_DOF_BlurBloomKernelSize )
	{
		bOverride_DOF_BlurBloomKernelSize = TRUE;
		EnableBloom();
		return;
	}

	if( PropertyName == NAME_EnableDOF )
	{
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_FalloffExponent )
	{
		bOverride_DOF_FalloffExponent = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_BlurKernelSize )
	{
		bOverride_DOF_BlurKernelSize = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_MaxNearBlurAmount )
	{
		bOverride_DOF_MaxNearBlurAmount = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_MinBlurAmount )
	{
		bOverride_DOF_MinBlurAmount = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_MaxFarBlurAmount )
	{
		bOverride_DOF_MaxFarBlurAmount = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_FocusType )
	{
		bOverride_DOF_FocusType = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_FocusInnerRadius )
	{
		bOverride_DOF_FocusInnerRadius = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_FocusDistance )
	{
		bOverride_DOF_FocusDistance = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_FocusPosition )
	{
		bOverride_DOF_FocusPosition = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_InterpolationDuration )
	{
		bOverride_DOF_InterpolationDuration = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_DOF_BokehTexture )
	{
		bOverride_DOF_BokehTexture = TRUE;
		EnableDOF();
		return;
	}

	if( PropertyName == NAME_EnableMotionBlur )
	{
		EnableMotionBlur();
		return;
	}

	if( PropertyName == NAME_MotionBlur_MaxVelocity )
	{
		bOverride_MotionBlur_MaxVelocity = TRUE;
		EnableMotionBlur();
		return;
	}

	if( PropertyName == NAME_MotionBlur_Amount )
	{
		bOverride_MotionBlur_Amount = TRUE;
		EnableMotionBlur();
		return;
	}

	if( PropertyName == NAME_MotionBlur_FullMotionBlur )
	{
		bOverride_MotionBlur_FullMotionBlur = TRUE;
		EnableMotionBlur();
		return;
	}

	if( PropertyName == NAME_MotionBlur_CameraRotationThreshold )
	{
		bOverride_MotionBlur_CameraRotationThreshold = TRUE;
		EnableMotionBlur();
		return;
	}

	if( PropertyName == NAME_MotionBlur_CameraTranslationThreshold )
	{
		bOverride_MotionBlur_CameraTranslationThreshold = TRUE;
		EnableMotionBlur();
		return;
	}

	if( PropertyName == NAME_MotionBlur_InterpolationDuration )
	{
		bOverride_MotionBlur_InterpolationDuration = TRUE;
		EnableMotionBlur();
		return;
	}

	if( PropertyName == NAME_EnableSceneEffect )
	{
		EnableSceneEffect();
		return;
	}

	if( PropertyName == NAME_Scene_Desaturation )
	{
		bOverride_Scene_Desaturation = TRUE;
		EnableSceneEffect();
		return;
	}

	if( PropertyName == NAME_Scene_Colorize )
	{
		bOverride_Scene_Colorize = TRUE;
		EnableSceneEffect();
		return;
	}

	if( PropertyName == NAME_Scene_TonemapperScale )
	{
		bOverride_Scene_TonemapperScale = TRUE;
		EnableSceneEffect();
		return;
	}

	if( PropertyName == NAME_Scene_ImageGrainScale )
	{
		bOverride_Scene_ImageGrainScale = TRUE;
		EnableSceneEffect();
		return;
	}

	if( PropertyName == NAME_Scene_HighLights )
	{
		bOverride_Scene_HighLights = TRUE;
		EnableSceneEffect();
		return;
	}

	if( PropertyName == NAME_Scene_MidTones )
	{
		bOverride_Scene_MidTones = TRUE;
		EnableSceneEffect();
		return;
	}

	if( PropertyName == NAME_Scene_Shadows )
	{
		bOverride_Scene_Shadows = TRUE;
		EnableSceneEffect();
		return;
	}

	if( PropertyName == NAME_Scene_ColorGradingLUT )
	{
		bOverride_Scene_ColorGradingLUT = TRUE;
		EnableSceneEffect();
		return;
	}

	if( PropertyName == NAME_Scene_InterpolationDuration )
	{
		bOverride_Scene_InterpolationDuration = TRUE;
		EnableSceneEffect();
		return;
	}

	if( PropertyName == NAME_AllowAmbientOcclusion )
	{
		bOverride_AllowAmbientOcclusion = TRUE;
		return;
	}

	if( PropertyName == NAME_OverrideRimShaderColor )
	{
		EnableRimShader();
		return;
	}

	if( PropertyName == NAME_RimShader_Color )
	{
		bOverride_RimShader_Color = TRUE;
		EnableRimShader();
		return;
	}

	if( PropertyName == NAME_RimShader_InterpolationDuration )
	{
		bOverride_RimShader_InterpolationDuration = TRUE;
		EnableRimShader();
		return;
	}

	if ( PropertyName == NAME_Mobile_BlurAmount )
	{
		MobilePostProcess.bOverride_Mobile_BlurAmount = TRUE;
		return;
	}
	if ( PropertyName == NAME_Mobile_Bloom_Scale )
	{
		MobilePostProcess.bOverride_Mobile_Bloom_Scale = TRUE;
		EnableBloom();
		return;
	}
	if ( PropertyName == NAME_Mobile_Bloom_Threshold )
	{
		MobilePostProcess.bOverride_Mobile_Bloom_Threshold = TRUE;
		EnableBloom();
		return;
	}
	if ( PropertyName == NAME_Mobile_Bloom_Tint )
	{
		MobilePostProcess.bOverride_Mobile_Bloom_Tint = TRUE;
		EnableBloom();
		return;
	}
	if ( PropertyName == NAME_Mobile_DOF_Distance )
	{
		MobilePostProcess.bOverride_Mobile_DOF_Distance = TRUE;
		EnableDOF();
		return;
	}
	if ( PropertyName == NAME_Mobile_DOF_MinRange )
	{
		MobilePostProcess.bOverride_Mobile_DOF_MinRange = TRUE;
		EnableDOF();
		return;
	}
	if ( PropertyName == NAME_Mobile_DOF_MaxRange )
	{
		MobilePostProcess.bOverride_Mobile_DOF_MaxRange = TRUE;
		EnableDOF();
		return;
	}
	if ( PropertyName == NAME_Mobile_DOF_FarBlurFactor )
	{
		MobilePostProcess.bOverride_Mobile_DOF_FarBlurFactor = TRUE;
		EnableDOF();
		return;
	}
}

/**
 * Checks the override setting for the given post-process setting.
 *
 * @param	PropertyName	The post-process property name to enable.
 */
UBOOL FPostProcessSettings::IsOverrideSetting( const FName& PropertyName )
{
	if(( PropertyName == NAME_EnableBloom )
	|| ( PropertyName == NAME_Bloom_Scale )
	|| ( PropertyName == NAME_Bloom_Threshold )
	|| ( PropertyName == NAME_Bloom_Tint )
	|| ( PropertyName == NAME_Bloom_ScreenBlendThreshold )
	|| ( PropertyName == NAME_Bloom_InterpolationDuration )
	|| ( PropertyName == NAME_DOF_BlurBloomKernelSize )
	|| ( PropertyName == NAME_EnableDOF )
	|| ( PropertyName == NAME_DOF_FalloffExponent )
	|| ( PropertyName == NAME_DOF_BlurKernelSize )
	|| ( PropertyName == NAME_DOF_MaxNearBlurAmount )
	|| ( PropertyName == NAME_DOF_MinBlurAmount )
	|| ( PropertyName == NAME_DOF_MaxFarBlurAmount )
	|| ( PropertyName == NAME_DOF_FocusType )
	|| ( PropertyName == NAME_DOF_FocusInnerRadius )
	|| ( PropertyName == NAME_DOF_FocusDistance )
	|| ( PropertyName == NAME_DOF_FocusPosition )
	|| ( PropertyName == NAME_DOF_InterpolationDuration )
	|| ( PropertyName == NAME_DOF_BokehTexture )
	|| ( PropertyName == NAME_EnableMotionBlur )
	|| ( PropertyName == NAME_MotionBlur_MaxVelocity )
	|| ( PropertyName == NAME_MotionBlur_Amount )
	|| ( PropertyName == NAME_MotionBlur_FullMotionBlur )
	|| ( PropertyName == NAME_MotionBlur_CameraRotationThreshold )
	|| ( PropertyName == NAME_MotionBlur_CameraTranslationThreshold )
	|| ( PropertyName == NAME_MotionBlur_InterpolationDuration )
	|| ( PropertyName == NAME_EnableSceneEffect )
	|| ( PropertyName == NAME_Scene_Desaturation )
	|| ( PropertyName == NAME_Scene_Colorize )
	|| ( PropertyName == NAME_Scene_TonemapperScale )
	|| ( PropertyName == NAME_Scene_ImageGrainScale )
	|| ( PropertyName == NAME_Scene_HighLights )
	|| ( PropertyName == NAME_Scene_MidTones )
	|| ( PropertyName == NAME_Scene_Shadows )
	|| ( PropertyName == NAME_Scene_ColorGradingLUT )
	|| ( PropertyName == NAME_Scene_InterpolationDuration )
	|| ( PropertyName == NAME_AllowAmbientOcclusion )
	|| ( PropertyName == NAME_OverrideRimShaderColor )
	|| ( PropertyName == NAME_RimShader_Color )
	|| ( PropertyName == NAME_RimShader_InterpolationDuration )
	|| ( PropertyName == NAME_Mobile_BlurAmount )
	|| ( PropertyName == NAME_Mobile_Bloom_Scale )
	|| ( PropertyName == NAME_Mobile_Bloom_Threshold )
	|| ( PropertyName == NAME_Mobile_Bloom_Tint )
	|| ( PropertyName == NAME_Mobile_DOF_Distance )
	|| ( PropertyName == NAME_Mobile_DOF_MinRange )
	|| ( PropertyName == NAME_Mobile_DOF_MaxRange )
	|| ( PropertyName == NAME_Mobile_DOF_FarBlurFactor ))
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * Disables the override setting for the given post-process setting.
 *
 * @param	PropertyName	The post-process property name to enable.
 */
void FPostProcessSettings::DisableOverrideSetting( const FName& PropertyName )
{
	if( PropertyName == NAME_EnableBloom )
	{
		DisableBloomOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Bloom_Scale )
	{
		bOverride_Bloom_Scale = FALSE;
		DisableBloomOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Bloom_Threshold )
	{
		bOverride_Bloom_Threshold = FALSE;
		DisableBloomOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Bloom_Tint )
	{
		bOverride_Bloom_Tint = FALSE;
		DisableBloomOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Bloom_ScreenBlendThreshold )
	{
		bOverride_Bloom_ScreenBlendThreshold = FALSE;
		DisableBloomOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Bloom_InterpolationDuration )
	{
		bOverride_Bloom_InterpolationDuration = FALSE;
		DisableBloomOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_BlurBloomKernelSize )
	{
		bOverride_DOF_BlurBloomKernelSize = FALSE;
		DisableBloomOverrideConditional();
		return;
	}

	if( PropertyName == NAME_EnableDOF )
	{
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_FalloffExponent )
	{
		bOverride_DOF_FalloffExponent = FALSE;
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_BlurKernelSize )
	{
		bOverride_DOF_BlurKernelSize = FALSE;
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_MaxNearBlurAmount )
	{
		bOverride_DOF_MaxNearBlurAmount = FALSE;
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_MinBlurAmount )
	{
		bOverride_DOF_MinBlurAmount = FALSE;
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_MaxFarBlurAmount )
	{
		bOverride_DOF_MaxFarBlurAmount = FALSE;
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_FocusType )
	{
		bOverride_DOF_FocusType = FALSE;
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_FocusInnerRadius )
	{
		bOverride_DOF_FocusInnerRadius = FALSE;
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_FocusDistance )
	{
		bOverride_DOF_FocusDistance = FALSE;
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_FocusPosition )
	{
		bOverride_DOF_FocusPosition = FALSE;
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_InterpolationDuration )
	{
		bOverride_DOF_InterpolationDuration = FALSE;
		DisableDOFOverrideConditional();
		return;
	}

	if( PropertyName == NAME_DOF_BokehTexture )
	{
		bOverride_DOF_BokehTexture = FALSE;
		DisableDOFOverrideConditional();
		return;
	}


	if( PropertyName == NAME_EnableMotionBlur )
	{
		DisableMotionBlurOverrideConditional();
		return;
	}

	if( PropertyName == NAME_MotionBlur_MaxVelocity )
	{
		bOverride_MotionBlur_MaxVelocity = FALSE;
		DisableMotionBlurOverrideConditional();
		return;
	}

	if( PropertyName == NAME_MotionBlur_Amount )
	{
		bOverride_MotionBlur_Amount = FALSE;
		DisableMotionBlurOverrideConditional();
		return;
	}

	if( PropertyName == NAME_MotionBlur_FullMotionBlur )
	{
		bOverride_MotionBlur_FullMotionBlur = FALSE;
		DisableMotionBlurOverrideConditional();
		return;
	}

	if( PropertyName == NAME_MotionBlur_CameraRotationThreshold )
	{
		bOverride_MotionBlur_CameraRotationThreshold = FALSE;
		DisableMotionBlurOverrideConditional();
		return;
	}

	if( PropertyName == NAME_MotionBlur_CameraTranslationThreshold )
	{
		bOverride_MotionBlur_CameraTranslationThreshold = FALSE;
		DisableMotionBlurOverrideConditional();
		return;
	}

	if( PropertyName == NAME_MotionBlur_InterpolationDuration )
	{
		bOverride_MotionBlur_InterpolationDuration = FALSE;
		DisableMotionBlurOverrideConditional();
		return;
	}

	if( PropertyName == NAME_EnableSceneEffect )
	{
		DisableSceneEffectOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Scene_Desaturation )
	{
		bOverride_Scene_Desaturation = FALSE;
		DisableSceneEffectOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Scene_Colorize )
	{
		bOverride_Scene_Colorize = FALSE;
		DisableSceneEffectOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Scene_TonemapperScale )
	{
		bOverride_Scene_TonemapperScale = FALSE;
		DisableSceneEffectOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Scene_ImageGrainScale )
	{
		bOverride_Scene_ImageGrainScale = FALSE;
		DisableSceneEffectOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Scene_HighLights )
	{
		bOverride_Scene_HighLights = FALSE;
		DisableSceneEffectOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Scene_MidTones )
	{
		bOverride_Scene_MidTones = FALSE;
		DisableSceneEffectOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Scene_Shadows )
	{
		bOverride_Scene_Shadows = FALSE;
		DisableSceneEffectOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Scene_ColorGradingLUT )
	{
		bOverride_Scene_ColorGradingLUT = FALSE;
		DisableSceneEffectOverrideConditional();
		return;
	}

	if( PropertyName == NAME_Scene_InterpolationDuration )
	{
		bOverride_Scene_InterpolationDuration = FALSE;
		DisableSceneEffectOverrideConditional();
		return;
	}

	if( PropertyName == NAME_AllowAmbientOcclusion )
	{
		bOverride_AllowAmbientOcclusion = FALSE;
		return;
	}

	if( PropertyName == NAME_OverrideRimShaderColor )
	{
		DisableRimShaderOverrideConditional();
		return;
	}

	if( PropertyName == NAME_RimShader_Color )
	{
		bOverride_RimShader_Color = FALSE;
		DisableRimShaderOverrideConditional();
		return;
	}

	if( PropertyName == NAME_RimShader_InterpolationDuration )
	{
		bOverride_RimShader_InterpolationDuration = FALSE;
		DisableRimShaderOverrideConditional();
		return;
	}

	if ( PropertyName == NAME_Mobile_BlurAmount )
	{
		MobilePostProcess.bOverride_Mobile_BlurAmount = FALSE;
		return;
	}
	if ( PropertyName == NAME_Mobile_Bloom_Scale )
	{
		MobilePostProcess.bOverride_Mobile_Bloom_Scale = FALSE;
		DisableMobileBloomOverrideConditional();
		return;
	}
	if ( PropertyName == NAME_Mobile_Bloom_Threshold )
	{
		MobilePostProcess.bOverride_Mobile_Bloom_Threshold = FALSE;
		DisableMobileBloomOverrideConditional();
		return;
	}
	if ( PropertyName == NAME_Mobile_Bloom_Tint )
	{
		MobilePostProcess.bOverride_Mobile_Bloom_Tint = FALSE;
		DisableMobileBloomOverrideConditional();
		return;
	}
	if ( PropertyName == NAME_Mobile_DOF_Distance )
	{
		MobilePostProcess.bOverride_Mobile_DOF_Distance = FALSE;
		DisableMobileDOFOverrideConditional();
		return;
	}
	if ( PropertyName == NAME_Mobile_DOF_MinRange )
	{
		MobilePostProcess.bOverride_Mobile_DOF_MinRange = FALSE;
		DisableMobileDOFOverrideConditional();
		return;
	}
	if ( PropertyName == NAME_Mobile_DOF_MaxRange )
	{
		MobilePostProcess.bOverride_Mobile_DOF_MaxRange = FALSE;
		DisableMobileDOFOverrideConditional();
		return;
	}
	if ( PropertyName == NAME_Mobile_DOF_FarBlurFactor )
	{
		MobilePostProcess.bOverride_Mobile_DOF_FarBlurFactor = FALSE;
		DisableMobileDOFOverrideConditional();
		return;
	}
}

/**
 * Sets all override values to false, which prevents overriding of this struct.
 *
 * @note	Overrides can be enabled again. 
 */
void FPostProcessSettings::DisableAllOverrides()
{
	// Disable bloom overrides
	bOverride_EnableBloom = FALSE;
	bOverride_Bloom_Scale = FALSE;
	bOverride_Bloom_Threshold = FALSE;
	bOverride_Bloom_Tint = FALSE;
	bOverride_Bloom_ScreenBlendThreshold = FALSE;
	bOverride_Bloom_InterpolationDuration = FALSE;

	// Disable depth of field overrides
	bOverride_EnableDOF = FALSE;
	bOverride_DOF_FalloffExponent = FALSE;
	bOverride_DOF_BlurKernelSize = FALSE;
	bOverride_DOF_BlurBloomKernelSize = FALSE;
	bOverride_DOF_MaxNearBlurAmount = FALSE;
	bOverride_DOF_MinBlurAmount = FALSE;
	bOverride_DOF_MaxFarBlurAmount = FALSE;
	bOverride_DOF_FocusType = FALSE;
	bOverride_DOF_FocusInnerRadius = FALSE;
	bOverride_DOF_FocusDistance = FALSE;
	bOverride_DOF_FocusPosition = FALSE;
	bOverride_DOF_InterpolationDuration = FALSE;
	bOverride_DOF_BokehTexture = FALSE;

	// Disable motion blur overrides
	bOverride_EnableMotionBlur = FALSE;
	bOverride_MotionBlur_MaxVelocity = FALSE;
	bOverride_MotionBlur_Amount = FALSE;
	bOverride_MotionBlur_FullMotionBlur = FALSE;
	bOverride_MotionBlur_CameraRotationThreshold = FALSE;
	bOverride_MotionBlur_CameraTranslationThreshold = FALSE;
	bOverride_MotionBlur_InterpolationDuration = FALSE;

	// Disable scene effect overrides
	bOverride_EnableSceneEffect = FALSE;
	bOverride_Scene_Desaturation = FALSE;
	bOverride_Scene_Colorize = FALSE;
	bOverride_Scene_TonemapperScale = FALSE;
	bOverride_Scene_ImageGrainScale = FALSE;
	bOverride_Scene_HighLights = FALSE;
	bOverride_Scene_MidTones = FALSE;
	bOverride_Scene_Shadows = FALSE;
	bOverride_Scene_InterpolationDuration = FALSE;
	bOverride_Scene_ColorGradingLUT = FALSE;

	bOverride_AllowAmbientOcclusion = FALSE;

	// Disable Rim Shader overrides
	bOverride_OverrideRimShaderColor = FALSE;
	bOverride_RimShader_Color = FALSE;
	bOverride_RimShader_InterpolationDuration = FALSE;

	// Disable Mobile post-processing overrides
	MobilePostProcess.bOverride_Mobile_BlurAmount = FALSE;
	MobilePostProcess.bOverride_Mobile_TransitionTime = FALSE;
	MobilePostProcess.bOverride_Mobile_Bloom_Scale = FALSE;
	MobilePostProcess.bOverride_Mobile_Bloom_Threshold = FALSE;
	MobilePostProcess.bOverride_Mobile_Bloom_Tint = FALSE;
	MobilePostProcess.bOverride_Mobile_DOF_Distance = FALSE;
	MobilePostProcess.bOverride_Mobile_DOF_MinRange = FALSE;
	MobilePostProcess.bOverride_Mobile_DOF_MaxRange = FALSE;
	MobilePostProcess.bOverride_Mobile_DOF_FarBlurFactor = FALSE;
}

/**
 * Disables the override to enable bloom if no bloom overrides are set.
 */
void FPostProcessSettings::DisableBloomOverrideConditional()
{
	if( !bOverride_Bloom_Scale && !bOverride_Bloom_Threshold && !bOverride_Bloom_Tint && !bOverride_Bloom_ScreenBlendThreshold && !bOverride_Bloom_InterpolationDuration )
	{
		bOverride_EnableBloom = FALSE;
		bEnableBloom = FALSE;
	}
}

/**
 * Disables the override to enable DOF if no DOF overrides are set.
 */
void FPostProcessSettings::DisableDOFOverrideConditional()
{
	if( !bOverride_DOF_FalloffExponent
	&&	!bOverride_DOF_BlurKernelSize
	&&	!bOverride_DOF_BlurBloomKernelSize
	&&	!bOverride_DOF_MaxNearBlurAmount
	&&	!bOverride_DOF_MinBlurAmount
	&&	!bOverride_DOF_MaxFarBlurAmount
	&&	!bOverride_DOF_FocusType
	&&	!bOverride_DOF_FocusInnerRadius
	&&	!bOverride_DOF_FocusDistance
	&&	!bOverride_DOF_FocusPosition
	&&	!bOverride_DOF_InterpolationDuration 
	&&	!bOverride_DOF_BokehTexture)
	{
		bOverride_EnableDOF = FALSE;
		bEnableDOF = FALSE;
	}
}

/**
 * Disables the override to enable motion blur if no motion blur overrides are set.
 */
void FPostProcessSettings::DisableMotionBlurOverrideConditional()
{
	if( !bOverride_MotionBlur_MaxVelocity
	&&	!bOverride_MotionBlur_Amount
	&&	!bOverride_MotionBlur_FullMotionBlur
	&&	!bOverride_MotionBlur_CameraRotationThreshold
	&&	!bOverride_MotionBlur_CameraTranslationThreshold
	&&	!bOverride_MotionBlur_InterpolationDuration )
	{
		bOverride_EnableMotionBlur = FALSE;
		bEnableMotionBlur = FALSE;
	}
}

/**
 * Disables the override to enable scene effect if no scene effect overrides.
 */
void FPostProcessSettings::DisableSceneEffectOverrideConditional()
{
	if(	!bOverride_Scene_Desaturation
	&&	!bOverride_Scene_HighLights
	&&	!bOverride_Scene_MidTones
	&&	!bOverride_Scene_Shadows
	&&	!bOverride_Scene_Colorize
	&&	!bOverride_Scene_ColorGradingLUT
	&&	!bOverride_Scene_InterpolationDuration)
	{
		bOverride_EnableSceneEffect = FALSE;
		bEnableSceneEffect = FALSE;
	}
}

/**
 * Disables the override to enable rim shader if no overrides are set for rim shader settings.
 */
void FPostProcessSettings::DisableRimShaderOverrideConditional()
{
	if( !bOverride_RimShader_Color && !bOverride_RimShader_InterpolationDuration )
	{
		bOverride_OverrideRimShaderColor = FALSE;
		bOverrideRimShaderColor = FALSE;
	}
}

/**
 * Disables the override to enable mobile bloom if no bloom overrides are set.
 */
void FPostProcessSettings::DisableMobileBloomOverrideConditional()
{
	if( !MobilePostProcess.bOverride_Mobile_Bloom_Scale &&
		!MobilePostProcess.bOverride_Mobile_Bloom_Threshold &&
		!MobilePostProcess.bOverride_Mobile_Bloom_Tint )
	{
		bOverride_EnableBloom = FALSE;
		bEnableBloom = FALSE;
	}
}

/**
 * Disables the override to enable mobile DOF if no DOF overrides are set.
 */
void FPostProcessSettings::DisableMobileDOFOverrideConditional()
{
	if( !MobilePostProcess.bOverride_Mobile_DOF_Distance
	&&	!MobilePostProcess.bOverride_Mobile_DOF_MinRange
	&&	!MobilePostProcess.bOverride_Mobile_DOF_MaxRange
	&&	!MobilePostProcess.bOverride_Mobile_DOF_FarBlurFactor )
	{
		bOverride_EnableDOF = FALSE;
		bEnableDOF = FALSE;
	}
}
