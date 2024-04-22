/*=============================================================================
	PreviewScene.h: Preview scene definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PREVIEWSCENE_H__
#define __PREVIEWSCENE_H__

/**
 * Encapsulates a simple scene setup for preview or thumbnail rendering.
 */
class FPreviewScene
{
public:
	FPreviewScene(const FRotator& LightRotation = FRotator(-8192,8192,0),FLOAT SkyBrightness = 0.25f,FLOAT LightBrightness = 1.0f,UBOOL bAlwaysAllowAudioPlayback = FALSE, UBOOL bForceMipsResident = TRUE);
	virtual ~FPreviewScene();

	/**
	 * Adds a component to the preview scene.  This attaches the component to the scene, and takes ownership of it.
	 */
	void AddComponent(class UActorComponent* Component,const FMatrix& LocalToWorld);

	/**
	 * Removes a component from the preview scene.  This detaches the component from the scene, and returns ownership of it.
	 */
	void RemoveComponent(class UActorComponent* Component);

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar,FPreviewScene& P);

	// Accessors.
	FSceneInterface* GetScene() const { return Scene; }

    /** Access to line drawing */
	class ULineBatchComponent* GetLineBatcher() const { return LineBatcher; }
    /** Clean out the line batcher each frame */
	void ClearLineBatcher();

	FRotator GetLightDirection();
	void SetLightDirection(const FRotator& InLightDir);
	void SetLightBrightness(FLOAT LightBrightness);
	void SetLightColor(const FColor& LightColor);

	void EnableDirectionalBounceLight(UBOOL bInEnableBoundLight, FLOAT BounceLightBrightness = 0.0f, FRotator* BounceLightDir = NULL);
	void SetBounceLightDirection(const FRotator& InLightDir);
	void SetBounceLightBrightness(FLOAT LightBrightness);
	void SetBounceLightColor(const FColor& LightColor);

	void SetSkyBrightness(FLOAT SkyBrightness);
	void SetSkyColor(const FColor& LightColor);

	/** Load/Save settings to the config, specifying the key */
	void LoadSettings(const TCHAR* Section);
	void SaveSettings(const TCHAR* Section);

	class UDirectionalLightComponent* DirectionalLight;
	/** List used to simulate light bouncing */
	class UDirectionalLightComponent* DirectionalBounceLight;
	class USkyLightComponent* SkyLight;

private:

	TArray<class UActorComponent*> Components;

protected:
	FSceneInterface* Scene;
	class ULineBatchComponent* LineBatcher;

	/** This controls whether or not all mip levels of textures used by UMeshComponents added to this preview window should be loaded and remain loaded. */
	UBOOL bForceAllUsedMipsResident;
};

#endif // __PREVIEWSCENE_H__
