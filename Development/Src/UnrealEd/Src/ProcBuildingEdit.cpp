/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "EngineProcBuildingClasses.h"
#include "EngineMeshClasses.h"
#include "UnTextureLayout.h"
#include "BSPOps.h"
#include "ScopedTransaction.h"
#include "ImageUtils.h"
#include "GeomTools.h"
#include "LevelUtils.h"
#include "EditorLevelUtils.h"

#if WITH_MANAGED_CODE
#include "GameAssetDatabaseShared.h"
#endif

#define DETAILED_PBUPDATE_TIMES 0

// Time for occlusion tests, in ms
extern FLOAT OcclusionTestTime;
extern FLOAT MeshFindTime;
extern FLOAT MeshTime;


/** Util that gets all MICs applied to meshes on this building */
static TArray<UMaterialInstanceConstant*> GetAllAppliedMICs(AProcBuilding* Building)
{
	TArray<UMaterialInstanceConstant*> OutputMICs;

	// Go over all building mesh components
	for (INT CompIndex = 0; CompIndex < Building->BuildingMeshCompInfos.Num(); CompIndex++)
	{
		UStaticMeshComponent* Component = Building->BuildingMeshCompInfos(CompIndex).MeshComp;
		for (INT MaterialIndex = 0; MaterialIndex < Component->GetNumElements(); MaterialIndex++)
		{
			UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Component->GetMaterial(MaterialIndex));
			if (MIC)
			{
				OutputMICs.AddUniqueItem(MIC);
			}
		}
	}

	// Go over fracturable components as well
	for (INT CompIndex = 0; CompIndex < Building->BuildingFracMeshCompInfos.Num(); CompIndex++)
	{
		UFracturedStaticMeshComponent* FracComp = Building->BuildingFracMeshCompInfos(CompIndex).FracMeshComp;
		for (INT MaterialIndex = 0; MaterialIndex < FracComp->GetNumElements(); MaterialIndex++)
		{
			UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(FracComp->GetMaterial(MaterialIndex));
			if (MIC)
			{
				OutputMICs.AddUniqueItem(MIC);
			}
		}
	}

	return OutputMICs;
}

/** Create MIC for low LOD building, and child MICs for each LODQuad */
static UMaterialInstanceConstant* CreateMICsForBuilding(AProcBuilding* HighLODActor, UStaticMesh* LowLODMesh)
{
	// create the material instance constant and apply it to the low detail mesh
	UMaterialInstanceConstant* SimpleMIC = CastChecked<UMaterialInstanceConstant>(UObject::StaticConstructObject(
		UMaterialInstanceConstant::StaticClass(), 
		LowLODMesh->GetOutermost(), 
		FName(*FString::Printf(TEXT("%s_MIC"), *LowLODMesh->GetName())),
		0
		));

	SimpleMIC->SetParent(GEngine->ProcBuildingSimpleMaterial);

	// Also apply any building-wide material params (so cubemap on windows tints the same etc)
	HighLODActor->SetBuildingMaterialParamsOnMIC(SimpleMIC);

	// assign the material to the simple mesh 
	LowLODMesh->LODModels(0).Elements(0).Material = SimpleMIC;

	// Need to assign this newly created material to any LODQuads		
	check(HighLODActor->LODMeshComps.Num() == HighLODActor->LODMeshUVInfos.Num());
	for(INT QuadIdx=0; QuadIdx<HighLODActor->LODMeshComps.Num(); QuadIdx++)
	{
		UStaticMeshComponent* LODComp = HighLODActor->LODMeshComps(QuadIdx);
		if(LODComp)
		{
			// reattach the lod quad component if it's already attached, because the rendering thread
			// may be using the existing MIC, and the code below will detroy the MIC in place without
			// detaching the component first
			FComponentReattachContext Reattach(LODComp);

			UMaterialInstanceConstant* QuadMIC = CastChecked<UMaterialInstanceConstant>(UObject::StaticConstructObject(
				UMaterialInstanceConstant::StaticClass(), 
				HighLODActor->GetOutermost(), 
				FName(*FString::Printf(TEXT("%s_LODQUADMIC_%d"), *HighLODActor->GetName(), QuadIdx)),
				0
				));

			QuadMIC->SetParent(SimpleMIC);

			const FPBFaceUVInfo& UVInfo = HighLODActor->LODMeshUVInfos(QuadIdx);

			FName UOffsetParamName(TEXT("U_Offset"));
			FName UScaleParamName(TEXT("U_Scale"));
			FName VOffsetParamName(TEXT("V_Offset"));
			FName VScaleParamName(TEXT("V_Scale"));

			QuadMIC->SetScalarParameterValue(UOffsetParamName, UVInfo.Offset.X);
			QuadMIC->SetScalarParameterValue(UScaleParamName, UVInfo.Size.X);
			QuadMIC->SetScalarParameterValue(VOffsetParamName, UVInfo.Offset.Y);
			QuadMIC->SetScalarParameterValue(VScaleParamName, UVInfo.Size.Y);

			LODComp->SetMaterial(0, QuadMIC);
		}
	}

	return SimpleMIC;
}

class FAutoLODTextureGenerator
{
public:
	/**
	 * Constructor
	 *
	 * @param HighLODActor The actor that contains the high detail components
	 * @param LowLODMesh The super low LOD mesh that will receive the generated textures
	 */
	FAutoLODTextureGenerator(AProcBuilding* InHighLODActor, UStaticMesh* InLowLODMesh)
	: HighLODActor(InHighLODActor)
	, LowLODMesh(InLowLODMesh)
	{
	}

	/**
	 * Renders the building to textures, and generates a material instance constant for the texture
	 *
	 * @param UVInfos Array of face UV info, to know where to put each face in the texture
	 * @param bCreateMaterial TRUE if this function should also create and assign a MIC to the mesh (otherwise will just point the existing MIC to the 
	 */
	void GenerateTextures(const TArray<FPBFaceUVInfo>& UVInfos, INT DiffuseTexWidth, INT DiffuseTexHeight, INT LightTexWidth, INT LightTexHeight, FLOAT BuildingSpec)
	{
		// what we need to do here is go through each face of the low LOD mesh, pull back along the normal
		// and render (ortho projection) the high LOD actor from that location.

		// remember if we were selected
		UBOOL bActorWasSelected = HighLODActor->IsSelected();

		if (bActorWasSelected)
		{
			GEditor->GetSelectedActors()->Deselect(HighLODActor);
			HighLODActor->ForceUpdateComponents(FALSE, FALSE);
		}

		// Buffer to hold unlit diffuse color (alpha will be glass cubemaps mask)
		TArray<FColor> BaseColorBuffer;
		BaseColorBuffer.AddZeroed(DiffuseTexWidth * DiffuseTexHeight);

		// Buffer to hold lighting info
		TArray<FColor> LightColorBuffer;
		LightColorBuffer.AddZeroed(LightTexWidth * LightTexHeight);

		
		// Make sure that the lit coloration material isn't using arbitrary color scale when we're rendering
		// lighting-only passes.  We'll scale by 0.5 so that we can encode a wider range of brightness
		// levels in the light map (0 - 2.0), then we'll modulate by 2x in the pixel shader.
		const FLinearColor OldLightingOnlyBrightness = GEngine->LightingOnlyBrightness;
		GEngine->LightingOnlyBrightness = FLinearColor( 0.5f, 0.5f, 0.5f );


		INT TempRenderTexSize = 1024;
		UTextureRenderTarget2D* TempRenderTex = CastChecked<UTextureRenderTarget2D>(UObject::StaticConstructObject(UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient));
		const UBOOL bForceLinearGamma = FALSE;
		TempRenderTex->Init(TempRenderTexSize, TempRenderTexSize, PF_A8R8G8B8, bForceLinearGamma);
		check(TempRenderTex->GameThread_GetRenderTargetResource());

		// Find all applied MICs
		TArray<UMaterialInstanceConstant*> AllMICs = GetAllAppliedMICs(HighLODActor);
		
		// Find all MICs applied to building that support one of our special params
		TArray<UMaterialInstanceConstant*> MICsToReset;
		FName CubemapMaskParamName(TEXT("RenderWindowCubemapMask"));
		FName RenderLODParamName(TEXT("RenderLOD"));
		for (INT MaterialIndex = 0; MaterialIndex < AllMICs.Num(); MaterialIndex++)
		{
			UMaterialInstanceConstant* MIC = AllMICs(MaterialIndex);
			if (MIC)
			{
				FLOAT ParamValue;
				UBOOL bHasCubemapParam = MIC->GetScalarParameterValue(CubemapMaskParamName, ParamValue);
				UBOOL bHasLODParam = MIC->GetScalarParameterValue(RenderLODParamName, ParamValue);

				// See if MIC has either param we are interested in
				if (bHasCubemapParam || bHasLODParam)
				{
					// if it supports LOD one, set it to 1.0 for all passes
					if(bHasLODParam)
					{
						MIC->SetScalarParameterValue(RenderLODParamName, 1.0f);
					}

					// make sure we reset it to 0 later
					MICsToReset.AddUniqueItem(MIC);
				}
			}
		}

		// Bring rendering passes!
		for (INT Pass = 0; Pass < 3; Pass++)
		{
			for(INT MICIndex=0; MICIndex < MICsToReset.Num(); MICIndex++)
			{
				UMaterialInstanceConstant* MIC = MICsToReset(MICIndex);

				if (Pass == 1)
				{
					// set it to 1.0
					MIC->SetScalarParameterValue(CubemapMaskParamName, 1.0f);
				}
				else
				{
					// set it to 0.0
					MIC->SetScalarParameterValue(CubemapMaskParamName, 0.0f);
				}
			}

			// Size of texture we want as result
			INT CurTexWidth = (Pass == 2) ? LightTexWidth : DiffuseTexWidth;
			INT CurTexHeight = (Pass == 2) ? LightTexHeight : DiffuseTexHeight;
			EShowFlags ShowMode = (Pass == 2) ? SHOW_ViewMode_LightingOnly : SHOW_ViewMode_Unlit;

			// create a view family for the rendering
			FSceneViewFamilyContext ViewFamily(
				TempRenderTex->GameThread_GetRenderTargetResource(),
				GWorld->Scene,
				((SHOW_DefaultGame&~SHOW_ViewMode_Mask) | ShowMode) & ~(SHOW_PostProcess | SHOW_DynamicShadows | SHOW_LOD | SHOW_Fog),
				GCurrentTime - GStartTime,
				GDeltaTime,
				GCurrentTime - GStartTime);
			ViewFamily.bClearScene = TRUE;

			for (INT FaceIndex = 0; FaceIndex < HighLODActor->TopLevelScopes.Num(); FaceIndex++)
			{
				// Dont generate texture region if not desired
				if(!HighLODActor->TopLevelScopeInfos(FaceIndex).bGenerateLODPoly)
				{
					continue;
				}

				UBOOL bNonRectFace = (FaceIndex >= HighLODActor->NumMeshedTopLevelScopes);


				// Set view to render correct ratio output
				SetView(&ViewFamily, FaceIndex, 0, 0, TempRenderTexSize, TempRenderTexSize);

				// draw it
				FCanvas Canvas(TempRenderTex->GameThread_GetRenderTargetResource(), NULL);
				BeginRenderingViewFamily(&Canvas,&ViewFamily);

				ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
					FResolveCommand,
					FTextureRenderTargetResource*, RTResource, TempRenderTex->GameThread_GetRenderTargetResource(),
				{
					// copy the results of the scene rendering from the target surface to its texture
					RHICopyToResolveTarget(RTResource->GetRenderTargetSurface(), FALSE, FResolveParams());
				});

				// make sure we wait for the render to complete
				FlushRenderingCommands();

				// for second pass, grab the color of the rtt for the mask to be alpha of base texture
				FRenderTarget* RenderTarget = TempRenderTex->GameThread_GetRenderTargetResource();

				// read the big 2d surface 
				TArray<FColor> SurfData;
				RenderTarget->ReadPixels(SurfData);

				// get the uv info for this face
				const FPBFaceUVInfo& UVInfo = UVInfos(FaceIndex);

				// Now scale it down to the correct size
				INT ScaledSizeX = appTrunc(UVInfo.Size.X * CurTexWidth);
				INT ScaledSizeY = appTrunc(UVInfo.Size.Y * CurTexHeight);

				TArray<FColor> ScaledSurfData;
				const UBOOL bResizeInLinearSpace = !bForceLinearGamma;
				FImageUtils::ImageResize( TempRenderTexSize, TempRenderTexSize, SurfData, ScaledSizeX, ScaledSizeY, ScaledSurfData, bResizeInLinearSpace );

				// And copy to the right space in the final texture
				INT ScaledOffsetX = appTrunc(UVInfo.Offset.X * CurTexWidth);
				INT ScaledOffsetY = appTrunc(UVInfo.Offset.Y * CurTexHeight);


				for(INT TexX=0; TexX<ScaledSizeX; TexX++)
				{
					for(INT TexY=0; TexY<ScaledSizeY; TexY++)
					{
						INT DestX = ScaledOffsetX + TexX;
						INT DestY = ScaledOffsetY + TexY;

						// Pass 0 - diffuse color
						if(Pass == 0)
						{
							BaseColorBuffer(DestX + (DestY*DiffuseTexWidth)) = ScaledSurfData(TexX + (TexY*ScaledSizeX));
						}
						// Pass 1 - glass cubemap mask
						else if(Pass == 1)
						{
							// For non-rect faces (roof etc), never any shiny, so force alpha to white
							if(bNonRectFace)
							{
								BaseColorBuffer(DestX + (DestY*DiffuseTexWidth)).A = 255;
							}
							else
							{
								FColor SourceColor = ScaledSurfData(TexX + (TexY*ScaledSizeX));
								BaseColorBuffer(DestX + (DestY*DiffuseTexWidth)).A = (SourceColor.R  > 32) ? 0 : 255; // TODO: Shouldn't have to mask - but things like roofs need masking to not be windows
								//BaseColorBuffer(DestX + (DestY*DiffuseTexWidth)).A = 255 - SourceColor.R;
							}
						}
						// Pass 2 - lighting
						else if(Pass == 2)
						{
							LightColorBuffer(DestX + (DestY*LightTexWidth)) = ScaledSurfData(TexX + (TexY*ScaledSizeX));
						}
					}
				}
			}
		}

		// Reset all MICs
		for (INT MICIndex = 0; MICIndex < MICsToReset.Num(); MICIndex++)
		{
			// first reset values to 0, then we will "disable it" (uncheck it), by removing it from the param list
			MICsToReset(MICIndex)->SetScalarParameterValue(CubemapMaskParamName, 0.0f);
			MICsToReset(MICIndex)->SetScalarParameterValue(RenderLODParamName, 0.0f);

			// backup all enabled params
			TArray<FScalarParameterValue> BackupParams = MICsToReset(MICIndex)->ScalarParameterValues;

			// clear them
			MICsToReset(MICIndex)->ScalarParameterValues.Empty();

			// copy all but the new one back in
			for (INT ParamIndex = 0; ParamIndex < BackupParams.Num(); ParamIndex++)
			{
				FName ParamName = BackupParams(ParamIndex).ParameterName;
				if ((ParamName != CubemapMaskParamName) && (ParamName != RenderLODParamName))
				{
					MICsToReset(MICIndex)->SetScalarParameterValue(BackupParams(ParamIndex).ParameterName, BackupParams(ParamIndex).ParameterValue);
				}
			}
		}

		// now we have color buffers containing the sides of our building, convert it to normal textures
		FCreateTexture2DParameters Params;
		Params.bDeferCompression = TRUE;
		Params.bUseAlpha = TRUE;
		Params.bSRGB = !bForceLinearGamma;
		Params.bWantSourceArt = FALSE;
		Params.CompressionSettings = TC_OneBitAlpha;
		UTexture2D* FinalTexture = FImageUtils::CreateTexture2D(DiffuseTexWidth, DiffuseTexHeight, BaseColorBuffer, LowLODMesh->GetOutermost(), FString::Printf(TEXT("%s_Faces"), *LowLODMesh->GetName()), 0, Params);
		FinalTexture->LODGroup = TEXTUREGROUP_ProcBuilding_Face;

		Params.CompressionSettings = TC_Default;
		UTexture2D* LightTexture = FImageUtils::CreateTexture2D(LightTexWidth, LightTexHeight, LightColorBuffer, LowLODMesh->GetOutermost(), FString::Printf(TEXT("%s_Light"), *LowLODMesh->GetName()), 0, Params);
		LightTexture->LODGroup = TEXTUREGROUP_ProcBuilding_LightMap;

		UMaterialInstanceConstant* SimpleMIC = Cast<UMaterialInstanceConstant>(LowLODMesh->LODModels(0).Elements(0).Material);
		check(SimpleMIC);

		SimpleMIC->SetTextureParameterValue(FName(TEXT("DiffuseTexture")), FinalTexture);
		SimpleMIC->SetTextureParameterValue(FName(TEXT("LightTexture")), LightTexture);
		SimpleMIC->SetScalarParameterValue(FName(TEXT("BuildingSpec")), BuildingSpec);

		if(HighLODActor->Ruleset)
		{
			// cubemap
			if(HighLODActor->Ruleset->LODCubemap)
			{
				SimpleMIC->SetTextureParameterValue(FName(TEXT("reflection")), HighLODActor->Ruleset->LODCubemap);
			}

			// 'interior' texture
			SimpleMIC->SetScalarParameterValue(FName(TEXT("UseInteriorTexture")), HighLODActor->Ruleset->bEnableInteriorTexture ? 1.f : 0.f);

			if(HighLODActor->Ruleset->InteriorTexture)
			{
				SimpleMIC->SetTextureParameterValue(FName(TEXT("InteriorTexture")), HighLODActor->Ruleset->InteriorTexture);
			}
		}

		// Restore the lighting-only material color scale value
		GEngine->LightingOnlyBrightness = OldLightingOnlyBrightness;


		// Set the low LOD mesh to use maximum texel ratio for streaming mip calculations, as we know
		// that the largest UV mapping is 100% relevant to mip streaming (it will be the largest
		// building quad in the mesh.)
		LowLODMesh->bUseMaximumStreamingTexelRatio = TRUE;

		// This should mark the low LOD level as dirty
		LowLODMesh->MarkPackageDirty();

		if (bActorWasSelected)
		{
			GEditor->GetSelectedActors()->Select(HighLODActor);
			HighLODActor->ForceUpdateComponents(FALSE, FALSE);
		}
	}


private:

	/**
	* Sets up the view to generate a texture for the given face of the mesh
	*
	* @param ViewFamily The view family to add the view to
	* @param Face Which face to render
	* @param X X location to render to in texture
	* @param Y Y location to render to in texture
	* @param SizeX Width of the area to render to in texture
	* @param SizeY Height of the area to render to in texture
	*/
	void SetView(FSceneViewFamily* ViewFamily, INT Face, INT X, INT Y, INT SizeX, INT SizeY)
	{
		FVector4 OverrideLODViewOrigin(0, 0, 0, 1.0f);

		// move to the center of the face, since the ScopeFrame moves to the bottom left of the face
		FMatrix ViewMatrix = HighLODActor->WorldToLocal() * HighLODActor->TopLevelScopes(Face).ScopeFrame.Inverse();

		// don't render the simple mesh, no matter what
		TSet<UPrimitiveComponent*> HiddenPrimitives;
		HiddenPrimitives.Add(HighLODActor->CurrentSimpleMeshComp);
		
		// Also don't render any intermediate LOD meshes
		for(INT i=0; i<HighLODActor->LODMeshComps.Num(); i++)
		{
			HiddenPrimitives.Add(HighLODActor->LODMeshComps(i));
		}

		ViewMatrix = ViewMatrix * FTranslationMatrix(FVector(
			-HighLODActor->TopLevelScopes(Face).DimX / 2.0f, 
			0, 
			-HighLODActor->TopLevelScopes(Face).DimZ / 2.0f
			));

		// move it into Z is front to back space
		ViewMatrix = ViewMatrix * FMatrix(
			FPlane(1, 0, 0, 0),
			FPlane(0, 0, -1, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));

		FMatrix ProjectionMatrix = FOrthoMatrix(
			HighLODActor->TopLevelScopes(Face).DimX / 2.0f,
			HighLODActor->TopLevelScopes(Face).DimZ / 2.0f,
			0.5f / HighLODActor->RenderToTexturePullBackAmount,
			HighLODActor->RenderToTexturePullBackAmount
			);

		ViewFamily->Views.Empty();
		ViewFamily->Views.AddItem(
			new FSceneView(
			ViewFamily,
			NULL,
			-1,
			NULL,
			NULL,
			NULL,
			GEngine->GetWorldPostProcessChain(),
			NULL,
			NULL,
			(FLOAT)X - 1.0f,
			(FLOAT)Y - 1.0f,
			(FLOAT)SizeX + 1.0f,
			(FLOAT)SizeY + 1.0f,
			ViewMatrix,
			ProjectionMatrix,
			FLinearColor::Black,
			FLinearColor(0,0,0,0),
			FLinearColor::White,
			HiddenPrimitives,
			FRenderingPerformanceOverrides(E_ForceInit),
			1.0f,
			TRUE,					// Treat this as a fresh frame, ignore any potential historical data
			FTemporalAAParameters(),
			1, 
			OverrideLODViewOrigin // we need to override this since we are using an ortho view, the normal checks in SceneRendering will fail
			)
			);
	}

	/** The high LOD actor we are rendering in the scene */
	AProcBuilding* HighLODActor;

	/** The low detail mesh that the textures will be applied to */
	UStaticMesh* LowLODMesh;
};


/**
 * Building scope indexer structure for quick sorting
 */
struct FSortableScopeIndexer
{
	// Size of this scope
	FLOAT Size;

	// Index into original scope array
	INT ScopeIndex;
};

/** Compare function used to sort building scopes by their size.  Larger scopes will come first. */
IMPLEMENT_COMPARE_CONSTREF( FSortableScopeIndexer, ProcBuildingEdit, { return ( B.Size - A.Size ); } )


/**
 * Given the scopes for a building, generate an atlas of texture subareas to fit all the scopes
 * into a single texture
 * 
 * @param Scopes List of scopes (faces) to make a single mesh out of
 * @param ScopeInfos List of associated infos for each face
 * @param bAllowCropping TRUE if we should crop UVs to rectangular textures if needed
 * @param OutUVs Output array (matching Scopes array) that will contain the offset/size of the faces in UV space (only written to if successful)
 * @param OutTextureSizeInWorldUnits How large the texture is in world units (largest dimension)
 * @param OutPreNormalizeUVExtents Maximum UV extents, BEFORE the UVs were renormalized
 * 
 * @return TRUE if a valid layout was found
 */
UBOOL CalculateUVInfos(const TArray<FPBScope2D>& Scopes, TArray<FPBScopeProcessInfo>& ScopeInfos, const UBOOL bAllowCropping, TArray<FPBFaceUVInfo>& OutUVs, FLOAT& OutTextureSizeInWorldUnits, FVector2D& OutPreNormalizeUVExtents )
{
	check(ScopeInfos.Num() == Scopes.Num());

	// temp array to keep the UV data
	TArray<FPBFaceUVInfo> UVInfos;
	UVInfos.AddZeroed(Scopes.Num());

	// walk over all the scopes, calculating the biggest one
	FVector2D MaxSize(0, 0);
	for (INT ScopeIndex = 0; ScopeIndex < Scopes.Num(); ScopeIndex++)
	{
		// Ignore if no LOD face wanted
		if(!ScopeInfos(ScopeIndex).bGenerateLODPoly)
		{
			continue;
		}

		const FPBScope2D& Scope = Scopes(ScopeIndex);
		MaxSize.X = Max(MaxSize.X, Scope.DimX);
		MaxSize.Y = Max(MaxSize.Y, Scope.DimZ);
	}


	// Sort all of the scopes by size to improve packing quality
	TArray< FSortableScopeIndexer > SortedScopeIndices;
	{
		// Fill in initial data
		SortedScopeIndices.Add( Scopes.Num() );
		for( INT CurScopeIndex = 0; CurScopeIndex < Scopes.Num(); ++CurScopeIndex )
		{
			FSortableScopeIndexer& CurIndexer = SortedScopeIndices( CurScopeIndex );
			const FPBScope2D& CurScope = Scopes( CurScopeIndex );

			CurIndexer.Size = CurScope.DimX * CurScope.DimZ;
			CurIndexer.ScopeIndex = CurScopeIndex;
		}

		// Sort it!
		Sort<USE_COMPARE_CONSTREF( FSortableScopeIndexer, ProcBuildingEdit )>( SortedScopeIndices.GetData(), SortedScopeIndices.Num() );
	}


	// for now use one max, to keep aspect ratios
	FLOAT LargestScopeSizeInWorldUnits = Max(MaxSize.X, MaxSize.Y);

	// this will convert the max world size to the texture size (we don't need the actual texture size, because
	// we are just going to emit UVs in 0..1 space)
	UINT FakeTexSize = 1024;
	FLOAT TextureToMaxRatio = (FLOAT)FakeTexSize / LargestScopeSizeInWorldUnits;

	// try different layouts based on different scalers (keeping aspect ratio the same), and keep track of UV info
	UBOOL bLayoutFailed = FALSE;
	FLOAT LargestScopeSizeInTextureSpace = 1.0f;

	FVector2D MaxUVExtent;
	for (FLOAT Scaler = 1.f; Scaler > 0.0f; Scaler -= 0.01f)
	{
		FTextureLayout TestLayout(1, 1, FakeTexSize, FakeTexSize, TRUE);
		MaxUVExtent = FVector2D( 0.0f, 0.0f );

		bLayoutFailed = FALSE;
		for( INT IndirectScopeIndex = 0; IndirectScopeIndex < SortedScopeIndices.Num(); ++IndirectScopeIndex )
		{
			// Grab the actual scope index
			const INT ScopeIndex = SortedScopeIndices( IndirectScopeIndex ).ScopeIndex;

			// Dont render if not used in LOD
			if(!ScopeInfos(ScopeIndex).bGenerateLODPoly)
			{
				continue;
			}

			const FPBScope2D& Scope = Scopes(ScopeIndex);

#if 1 // @compat mode until we force a rebuild of all buildings
			// calculate the size of this face in the texture space, using the world space size of the face and the scaler
			UINT SizeX = appTrunc(TextureToMaxRatio * Scope.DimX * Scaler);
			UINT SizeY = appTrunc(TextureToMaxRatio * Scope.DimZ * Scaler);

			// attempt to put the texture into the layout
			UINT OutX, OutY;
			if (TestLayout.AddElement(OutX, OutY, SizeX, SizeY))
			{
#else
			// calculate the size of this face in the texture space, using the world space size of the face and the scaler
			UINT SizeX = appCeil(TextureToMaxRatio * Scope.DimX * Scaler);
			UINT SizeY = appCeil(TextureToMaxRatio * Scope.DimZ * Scaler);

			// attempt to put the texture into the layout (with some padding on either side)
			UINT OutX, OutY;
			if (TestLayout.AddElement(&OutX, &OutY, SizeX + 2, SizeY + 2))
			{
				// move one texel inside the padded region
				OutX += 1;
				OutY += 1;
#endif
				// if it works, track where this face went
				FPBFaceUVInfo& UVInfo = UVInfos(ScopeIndex);
				UVInfo.Offset.X = (FLOAT)OutX / FakeTexSize;
				UVInfo.Offset.Y = (FLOAT)OutY / FakeTexSize;
				UVInfo.Size.X = (FLOAT)SizeX / FakeTexSize;
				UVInfo.Size.Y = (FLOAT)SizeY / FakeTexSize;

				if( UVInfo.Offset.X + UVInfo.Size.X > MaxUVExtent.X )
				{
					MaxUVExtent.X = UVInfo.Offset.X + UVInfo.Size.X;
				}
				if( UVInfo.Offset.Y + UVInfo.Size.Y > MaxUVExtent.Y )
				{
					MaxUVExtent.Y = UVInfo.Offset.Y + UVInfo.Size.Y;
				}
			}
			else
			{
				bLayoutFailed = TRUE;
				break;
			}

		}

		// if we didn't fail, exit now from the loop, we have the largest scaler that will fit them all
		if (!bLayoutFailed)
		{
			LargestScopeSizeInTextureSpace = Scaler;
			break;
		}
	}

	// return FALSE if we failed to find a valid layout
	if (bLayoutFailed)
	{
		return FALSE;
	}


	if( bAllowCropping )
	{
		// Renormalize the UVs such that they fill the entire 0.0-1.0 space.  This breaks the aspect
		// ratio of the UVs, but that's fine because we'll be cropping the texture accordingly.
		for (INT ScopeIndex = 0; ScopeIndex < Scopes.Num(); ScopeIndex++)
		{
			// Dont render if not used in LOD
			if(!ScopeInfos(ScopeIndex).bGenerateLODPoly)
			{
				continue;
			}

			FPBFaceUVInfo& UVInfo = UVInfos(ScopeIndex);

			// Normalize the UVs
			UVInfo.Offset.X /= MaxUVExtent.X;
			UVInfo.Offset.Y /= MaxUVExtent.Y;
			UVInfo.Size.X /= MaxUVExtent.X;
			UVInfo.Size.Y /= MaxUVExtent.Y;
		}
		
		// Output the UV extents (pre-normalization)
		OutPreNormalizeUVExtents = MaxUVExtent;
	}
	else
	{
		// Not using cropping, so use identity UV extents
		OutPreNormalizeUVExtents = FVector2D( 1.0f, 1.0f );
	}


	// We have the size of the largest scope in world units as well as the size of
	// that scope as packed into the texture (0.0-1.0). Compute how large the entire texture
	// is in world units
	OutTextureSizeInWorldUnits = LargestScopeSizeInWorldUnits / LargestScopeSizeInTextureSpace;


	// if successful, overwrite the output array
	OutUVs = UVInfos;
	return TRUE;
}

/**
 * Utility to make a new StaticMesh based on a list of scopes as well as unwrap the mesh
 * to generate UVs for each face to pack all face textures into a single texture
 *
 * @param Scopes List of scopes (faces) to make a single mesh out of
 * @param MeshOuter Object inside of which to create the mesh
 * @param OutUVs Output array (matching Scopes array) that will contain the offset/size of the 
 */
static UStaticMesh* CreateStaticMeshFromScopes(AProcBuilding* Building, const TArray<FPBScope2D>& Scopes, TArray<FPBScopeProcessInfo>& ScopeInfos, INT NumMeshedScopes, TArray<FClipSMPolygon>& NonRectPolys, UObject* MeshOuter, const TArray<FPBFaceUVInfo>& ScopeUVs)
{
	check(Scopes.Num() == ScopeUVs.Num());
	// The number of 'meshed' rect scopes plus the number of non-rect polys, should equal the total number of scopes we have
	check(NumMeshedScopes + NonRectPolys.Num() == Scopes.Num());	

	INT TotalScopeIndex=0;

	// collect the triangles made from the scopes
	TArray<FStaticMeshTriangle> Triangles;	
	for (INT ScopeIndex = 0; ScopeIndex < NumMeshedScopes; ScopeIndex++)
	{
		// Check we want this face in the final mesh
		if(ScopeInfos(ScopeIndex).bGenerateLODPoly)
		{
			const FPBScope2D& Scope = Scopes(TotalScopeIndex);
			const FPBFaceUVInfo& UVInfo = ScopeUVs(TotalScopeIndex);

			FVector Corners[4];
			FVector2D UVs[4];

			// walk around the verts clockwise, starting at bottom left
			for (INT VertIndex = 0; VertIndex < 4; VertIndex++)
			{
				// figure out how to translate from bottom left to the corner
				FVector Translation(0, 0, 0);
				switch (VertIndex)
				{
				case 1:
					Translation.Z = Scope.DimZ;
					UVs[VertIndex].X = 0.0f;
					UVs[VertIndex].Y = 0.0f;
					break;

				case 2:
					Translation.X = Scope.DimX;
					Translation.Z = Scope.DimZ;
					UVs[VertIndex].X = 1.0f;
					UVs[VertIndex].Y = 0.0f;
					break;

				case 3:
					Translation.X = Scope.DimX;
					UVs[VertIndex].X = 1.0f;
					UVs[VertIndex].Y = 1.0f;
					break;

				default:
					UVs[VertIndex].X = 0.0f;
					UVs[VertIndex].Y = 1.0f;
					break;
				}

				// make a matrix to get the vertex from the scopes frame combined with translation along the plane to the corner
				FMatrix VertTranslator = FTranslationMatrix(Translation) * Scope.ScopeFrame;

				// translate 0,0,0 by the matrix to get final vert location
				Corners[VertIndex] = VertTranslator.TransformFVector(FVector(0, 0, 0));
			}

			// build the triangles
			for (INT TriangleIndex = 0; TriangleIndex < 2; TriangleIndex++)
			{
				FStaticMeshTriangle NewTri;
				appMemzero(&NewTri, sizeof(FStaticMeshTriangle));

				NewTri.Vertices[0] = Corners[0];
				NewTri.UVs[0][0] = (UVs[0] * UVInfo.Size) + UVInfo.Offset;

				NewTri.Vertices[1] = Corners[TriangleIndex + 2];
				NewTri.UVs[1][0] = (UVs[TriangleIndex + 2] * UVInfo.Size) + UVInfo.Offset;

				NewTri.Vertices[2] = Corners[TriangleIndex + 1];		
				NewTri.UVs[2][0] = (UVs[TriangleIndex + 1] * UVInfo.Size) + UVInfo.Offset;

				NewTri.NumUVs = 1;

				Triangles.AddItem(NewTri);
			}
		}

		TotalScopeIndex++;
	}

	// Collect all triangles from non-rect regions
	for(INT PolyIdx = 0; PolyIdx < NonRectPolys.Num(); PolyIdx++)
	{
		FClipSMPolygon& Poly = NonRectPolys(PolyIdx);
		if(Poly.Vertices.Num() >= 3)
		{
			TArray<FClipSMTriangle> PolyTris;
			TriangulatePoly( PolyTris, Poly );

			const FPBFaceUVInfo& UVInfo = ScopeUVs(TotalScopeIndex);

			// build the triangles
			for (INT TriIndex=0; TriIndex<PolyTris.Num(); TriIndex++)
			{
				FClipSMTriangle& Tri = PolyTris(TriIndex);

				FStaticMeshTriangle NewTri;
				appMemzero(&NewTri, sizeof(FStaticMeshTriangle));

				NewTri.Vertices[0] = Tri.Vertices[0].Pos;
				NewTri.UVs[0][0] = (Tri.Vertices[0].UVs[0] * UVInfo.Size) + UVInfo.Offset;

				NewTri.Vertices[1] = Tri.Vertices[2].Pos;
				NewTri.UVs[1][0] = (Tri.Vertices[2].UVs[0] * UVInfo.Size) + UVInfo.Offset;

				NewTri.Vertices[2] = Tri.Vertices[1].Pos;	
				NewTri.UVs[2][0] = (Tri.Vertices[1].UVs[0] * UVInfo.Size) + UVInfo.Offset;

				NewTri.NumUVs = 1;

				Triangles.AddItem(NewTri);
			}
		}

		TotalScopeIndex++;
	}

	const FString BuildingName = Building->GetName();
	const FString StaticMeshName = FString::Printf( TEXT( "StaticMeshLOD_%s_" ), *BuildingName );

	// Give the created mesh a name that makes it easier to find / know what it is
	UStaticMesh* StaticMesh = ConstructObject<UStaticMesh>( UStaticMesh::StaticClass(), MeshOuter, UObject::MakeUniqueObjectName( MeshOuter, UStaticMesh::StaticClass(), *StaticMeshName ) );

	// Add one LOD for the base mesh
	new(StaticMesh->LODModels) FStaticMeshRenderData();
	StaticMesh->LODInfo.AddItem(FStaticMeshLODInfo());

	StaticMesh->LODModels(0).RawTriangles.RemoveBulkData();	
	StaticMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_WRITE);

	void* RawTriangleData = StaticMesh->LODModels(0).RawTriangles.Realloc(Triangles.Num());
	check( StaticMesh->LODModels(0).RawTriangles.GetBulkDataSize() == Triangles.Num() * Triangles.GetTypeSize() );
	appMemcpy( RawTriangleData, Triangles.GetData(), StaticMesh->LODModels(0).RawTriangles.GetBulkDataSize() );

	StaticMesh->LODModels(0).RawTriangles.Unlock();

	FStaticMeshElement* NewStaticMeshElement = new(StaticMesh->LODModels(0).Elements) FStaticMeshElement(NULL, 0);
	
	// remove the kdop triangles from the LOD StaticMesh
	NewStaticMeshElement->OldEnableCollision  = FALSE;
	NewStaticMeshElement->EnableCollision = FALSE;

	StaticMesh->Build(FALSE, TRUE);

	return StaticMesh;
}

/**
 * Create an always loaded sublevel for low LOD actors to be placed in. This is basically code from about
 * 3 different functions copied and merged into one.
 *
 * @param ExistingSubLevelName Name of the existing sublevel that the high LOD building is in
 *
 * @return The new LOD sub level
 */
static ULevel* CreateAlwaysLoadedLODLevel(const TCHAR* ExistingSubLevelName)
{
	// Matinee cannot be opened when any level saving occurs.
	GEditorModeTools().ActivateMode( EM_Default );

	FFilename SubLevelPath;
	if (!GPackageFileCache->FindPackageFile(ExistingSubLevelName, NULL, SubLevelPath))
	{
		return NULL;
	}
	
	// Cache the current GWorld and clear it, then allocate a new one, and finally restore it.
	UWorld* OldGWorld = GWorld;
	GWorld = NULL;
	UWorld::CreateNew();
	UWorld* NewGWorld = GWorld;
	check(NewGWorld);

	// grab the new level we just made
	ULevel* NewSubLevel = GWorld->PersistentLevel;

	// create the path to where the level should be saved (right next to the low LOD level)
	FFilename FinalFilename = SubLevelPath.GetBaseFilename(FALSE) + TEXT("_LOD") + SubLevelPath.GetExtension(TRUE);

	// Save the new world to disk.
	const UBOOL bNewWorldSaved = GWorld->SaveWorld(FinalFilename, TRUE, FALSE, FALSE);

	if (bNewWorldSaved)
	{
		GWorld->GetOutermost()->Rename( *FinalFilename.GetBaseFilename(), NULL, REN_ForceNoResetLoaders );
	}

	// Restore the old GWorld and GC the new one.
	GWorld = OldGWorld;

	NewGWorld->RemoveFromRoot();
	UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	// now add this new level we just saved to disk as a sublevel to the original GWorld
	if (bNewWorldSaved)
	{
		// create a new streaming level helper object
		ULevelStreamingAlwaysLoaded* StreamingLevel = ConstructObject<ULevelStreamingAlwaysLoaded>(ULevelStreamingAlwaysLoaded::StaticClass(), GWorld);

		// Tell the AlwaysLoadedLevel that it is for a Proc building LOD
		StreamingLevel->bIsProceduralBuildingLODLevel = TRUE;

		// Associate a package name.
		StreamingLevel->PackageName = FName(*FinalFilename.GetBaseFilename());

		// Seed the level's draw color.
		StreamingLevel->DrawColor = FColor::MakeRandomColor();

		// Add the new level to worldinfo.
		GWorld->GetWorldInfo()->StreamingLevels.AddItem( StreamingLevel );
		GWorld->GetWorldInfo()->PostEditChange();
		GWorld->MarkPackageDirty();

		// look up the new level in the real GWorld
		NewSubLevel = StreamingLevel->LoadedLevel;
	}
	else
	{
		// ignore the new level if we failed to save it
		NewSubLevel = NULL;
	}

	// return success flag
	return NewSubLevel;
}


static void RemoveUnusedLODQuads(AProcBuilding* Building)
{
	// Array used to track which LODQuads have children
	TArray<UBOOL> LODQuadHasChildren;
	LODQuadHasChildren.AddZeroed(Building->LODMeshComps.Num());
	
	// Array used to map from LOD quad index to info index, init'd to INDEX_NONE
	TArray<INT> LODQuadCompInfoIndex;
	LODQuadCompInfoIndex.Add(Building->LODMeshComps.Num());
	for(INT i=0; i<LODQuadCompInfoIndex.Num(); i++)
	{
		LODQuadCompInfoIndex(i) = INDEX_NONE;
	}

	// Iterate over all components used in building
	for(INT i=0; i<Building->BuildingMeshCompInfos.Num(); i++)
	{
		UStaticMeshComponent* BuildingComp = Building->BuildingMeshCompInfos(i).MeshComp;
		if(BuildingComp)
		{
			// See if its a LOD quad
			INT LODMeshIndex = Building->LODMeshComps.FindItemIndex(BuildingComp);
		
			// If it is - remember what index on the infos array it is
			if(LODMeshIndex != INDEX_NONE)			
			{
				LODQuadCompInfoIndex(LODMeshIndex) = i;
			}
			// Its not, see if its parent is a LOD quad
			else
			{
				// Find which LOD quad it is a child of
				UStaticMeshComponent* ParentSMComp = CastChecked<UStaticMeshComponent>(BuildingComp->ReplacementPrimitive);
				INT LODQuadIndex = Building->LODMeshComps.FindItemIndex(ParentSMComp);
				if(LODQuadIndex != INDEX_NONE)
				{
					// And tell that LOD quad that it has a child
					LODQuadHasChildren(LODQuadIndex) = TRUE;
				}
			}		
		}
	}
	
	// Iterate over quads with no children
	for(INT i=LODQuadHasChildren.Num()-1; i>=0; i--)
	{
		if(!LODQuadHasChildren(i))
		{
			INT InfoIndex = LODQuadCompInfoIndex(i);
			
			check(InfoIndex != INDEX_NONE);
			check(Building->BuildingMeshCompInfos(InfoIndex).MeshComp == Building->LODMeshComps(i));

			// Remove this quad - that mean removing it from the BuildingMeshCompInfos, LODMeshComps and LODMeshUVInfos arrays
			// no need to detach any components, they have not yet been attached			
			
			Building->BuildingMeshCompInfos.Remove(InfoIndex);
			Building->LODMeshComps.Remove(i);
			Building->LODMeshUVInfos.Remove(i);
		}
	}
}

/** Util to find the direction of the longest edge of a polygon */
static FVector FindPolyLongestEdge(const FPoly& Poly)
{
	INT NumVerts = Poly.Vertices.Num();

	FLOAT LongestLen = -BIG_NUMBER;
	FVector LongestDir(0,0,0);

	for(INT EdgeIdx=0; EdgeIdx<Poly.Vertices.Num(); EdgeIdx++)
	{
		INT EndVertIndex = (EdgeIdx+1)%NumVerts;
		FVector Edge = Poly.Vertices(EdgeIdx) - Poly.Vertices(EndVertIndex);
		FLOAT EdgeLen = Edge.Size();

		if(Edge.Size() > LongestLen)
		{
			LongestLen = EdgeLen;
			LongestDir = Edge;
		}
	}

	return LongestDir;
}

static void CreatePolyBoundScopes(TArray<FPBScope2D>& OutScopes, TArray<FClipSMPolygon>& OutTexPolys, const TArray<FPoly>& InPolys)
{
	for(INT PolyIdx=0; PolyIdx<InPolys.Num(); PolyIdx++)
	{
		const FPoly& Poly = InPolys(PolyIdx);
		if(Poly.Vertices.Num() >= 3)
		{
			FVector LongEdgeDir = FindPolyLongestEdge(Poly).SafeNormal();
			FVector NormalDir = Poly.Normal.SafeNormal();
			FVector BaseVert = Poly.Vertices(0);
			
			// First stab at transform, use first vertex as origin, and U direction as X
			FPBScope2D BoundScope;
			BoundScope.ScopeFrame = FMatrix(LongEdgeDir, NormalDir, (LongEdgeDir ^ NormalDir).SafeNormal(), BaseVert);

			// Find extents in that scope space
			FVector BoundsMin(BIG_NUMBER);
			FVector BoundsMax(-BIG_NUMBER);
			for(INT VertIdx=0; VertIdx<Poly.Vertices.Num(); VertIdx++)
			{
				FVector LocalVert = BoundScope.ScopeFrame.InverseTransformFVectorNoScale( Poly.Vertices(VertIdx) );
				BoundsMin.X = Min(BoundsMin.X, LocalVert.X);
				BoundsMin.Z = Min(BoundsMin.Z, LocalVert.Z);
				BoundsMax.X = Max(BoundsMax.X, LocalVert.X);
				BoundsMax.Z = Max(BoundsMax.Z, LocalVert.Z);
			}

			// Offset and scale to cover entire poly
			BoundScope.OffsetLocal(FVector(BoundsMin.X, 0.f, BoundsMin.Z));
			BoundScope.DimX = BoundsMax.X - BoundsMin.X;
			BoundScope.DimZ = BoundsMax.Z - BoundsMin.Z;

			// Add to output set
			OutScopes.AddItem(BoundScope);

			// Now convert FPoly into textured FPBTexPolys
			FClipSMPolygon NewTexPoly(0);			

			NewTexPoly.FaceNormal = Poly.Normal;
			NewTexPoly.Vertices.AddZeroed(Poly.Vertices.Num());

			for(INT VertIdx=0; VertIdx<Poly.Vertices.Num(); VertIdx++)
			{
				NewTexPoly.Vertices(VertIdx).Pos = Poly.Vertices(VertIdx);

				// Use scope as 0..1 range for UVs of each vert
				FVector LocalVert = BoundScope.ScopeFrame.InverseTransformFVectorNoScale( NewTexPoly.Vertices(VertIdx).Pos );
				NewTexPoly.Vertices(VertIdx).UVs[0].X = (LocalVert.X/BoundScope.DimX);
				NewTexPoly.Vertices(VertIdx).UVs[0].Y = 1.f - (LocalVert.Z/BoundScope.DimZ);
			}

			OutTexPolys.AddItem(NewTexPoly);
		}
	}
}

/** Expand a 2D box (given by 2 vectors) to include new 2D point */
static void Expand2DBox(FVector2D& Min2D, FVector2D&Max2D, const FVector2D& InVal)
{
	Min2D.X = Min(Min2D.X, InVal.X);
	Min2D.Y = Min(Min2D.Y, InVal.Y);

	Max2D.X = Max(Max2D.X, InVal.X);
	Max2D.Y = Max(Max2D.Y, InVal.Y);	
}

/** Copies/scales/offset UV channel 0 to make a UV channel 1, for lightmaps */
static void GenerateLightmapUVForTri(FStaticMeshTriangle& Tri, const FVector2D& Offset, const FVector2D& Scale)
{
	for(INT i=0; i<3; i++)
	{
		Tri.UVs[i][1].X = (Tri.UVs[i][0].X - Offset.X) / Scale.X;
		Tri.UVs[i][1].Y = (Tri.UVs[i][0].Y - Offset.Y) / Scale.Y;
	}
}

/** Util to convert FPoly to FClipSMPolygon */
static FClipSMPolygon PolyToClipSMPolygon(const FPoly& InPoly)
{
	FClipSMPolygon ClipPoly(0);
	ClipPoly.FaceNormal = InPoly.Normal;
	ClipPoly.Vertices.AddZeroed(InPoly.Vertices.Num());
	for(INT VertIdx=0; VertIdx<InPoly.Vertices.Num(); VertIdx++)
	{
		ClipPoly.Vertices(VertIdx).Pos = InPoly.Vertices(VertIdx);
	}

	return ClipPoly;
}

/** Simple util to make a new StaticMesh based on an FPoly */
static UStaticMesh* MakeStaticMeshForPoly(const FPoly& Poly, AProcBuilding* Building, UMaterialInterface* DefaultMat)
{
	if(Poly.Vertices.Num() < 3)
	{
		return NULL;
	}

	FClipSMPolygon ClipPoly = PolyToClipSMPolygon(Poly);

	TArray<FClipSMTriangle> PolyTris;
	TriangulatePoly( PolyTris, ClipPoly );

	FVector	TextureBase = Poly.Base;
	FVector TextureX = Poly.TextureU / 128.0f;
	FVector TextureY = Poly.TextureV / 128.0f;

	// We keep track of min/max of UVs generated
	FVector2D MinUV(BIG_NUMBER, BIG_NUMBER);
	FVector2D MaxUV(-BIG_NUMBER, -BIG_NUMBER);

	TArray<FStaticMeshTriangle> Triangles;	
	for(INT TriIdx=0; TriIdx<PolyTris.Num(); TriIdx++)
	{
		FStaticMeshTriangle NewTri;
		appMemzero(&NewTri, sizeof(FStaticMeshTriangle));

		NewTri.Vertices[0] = PolyTris(TriIdx).Vertices[0].Pos;				
		NewTri.UVs[0][0].X = (NewTri.Vertices[0] - TextureBase) | TextureX;
		NewTri.UVs[0][0].Y = (NewTri.Vertices[0] - TextureBase) | TextureY;
		Expand2DBox(MinUV, MaxUV, NewTri.UVs[0][0]);

		NewTri.Vertices[1] = PolyTris(TriIdx).Vertices[2].Pos;
		NewTri.UVs[1][0].X = (NewTri.Vertices[1] - TextureBase) | TextureX;
		NewTri.UVs[1][0].Y = (NewTri.Vertices[1] - TextureBase) | TextureY;
		Expand2DBox(MinUV, MaxUV, NewTri.UVs[1][0]);

		NewTri.Vertices[2] = PolyTris(TriIdx).Vertices[1].Pos;		
		NewTri.UVs[2][0].X = (NewTri.Vertices[2] - TextureBase) | TextureX;
		NewTri.UVs[2][0].Y = (NewTri.Vertices[2] - TextureBase) | TextureY;
		Expand2DBox(MinUV, MaxUV, NewTri.UVs[2][0]);

		NewTri.NumUVs = 2; // Will generate lightmap UVs in a second..

		Triangles.AddItem(NewTri);
	}

	// Use min/max to see how much we need to scale texture UVs to make lightmap (non-tiling) UVs
	FVector2D ScaleUV = MaxUV - MinUV;

	for(INT i=0; i<Triangles.Num(); i++)
	{
		GenerateLightmapUVForTri( Triangles(i), MinUV, ScaleUV );
	}

	const FString BuildingName = Building->GetName();
	const FString StaticMeshName = FString::Printf( TEXT( "StaticMesh_%s_" ), *BuildingName );

	UStaticMesh* StaticMesh = ConstructObject<UStaticMesh>(UStaticMesh::StaticClass(), Building->GetOutermost(), UObject::MakeUniqueObjectName( Building->GetOutermost(), UStaticMesh::StaticClass(), *StaticMeshName ) );

	// Add one LOD for the base mesh
	new(StaticMesh->LODModels) FStaticMeshRenderData();
	StaticMesh->LODInfo.AddItem(FStaticMeshLODInfo());

	StaticMesh->LODModels(0).RawTriangles.RemoveBulkData();	
	StaticMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_WRITE);

	void* RawTriangleData = StaticMesh->LODModels(0).RawTriangles.Realloc(Triangles.Num());
	check( StaticMesh->LODModels(0).RawTriangles.GetBulkDataSize() == Triangles.Num() * Triangles.GetTypeSize() );
	appMemcpy( RawTriangleData, Triangles.GetData(), StaticMesh->LODModels(0).RawTriangles.GetBulkDataSize() );

	StaticMesh->LODModels(0).RawTriangles.Unlock();

	// Use default if no override supplied in the poly
	UMaterialInterface* UseMat = Poly.Material ? Poly.Material : DefaultMat;

	new(StaticMesh->LODModels(0).Elements) FStaticMeshElement(UseMat, 0);

	//Build move outside so the caller can do things before building
	//StaticMesh->Build(FALSE, TRUE);

	return StaticMesh;
}

/** Take a set of polys, and create static meshes for them and add to building */
static void CreatePolyMeshes(AProcBuilding* BaseBuilding, const TArray<FPoly>& NonRectFacePolys)
{
	if(!BaseBuilding->Ruleset)
	{
		return;
	}

	for(INT PolyIdx=0; PolyIdx<NonRectFacePolys.Num(); PolyIdx++)
	{
		const FPoly& Poly = NonRectFacePolys(PolyIdx);
		UBOOL bIsRoof = (Poly.Normal.Z) > UCONST_ROOF_MINZ;
		UBOOL bIsFloor = (Poly.Normal.Z) < -UCONST_ROOF_MINZ;

		// Choose material and lightmap res depending on roof/wall
		UMaterialInterface* DefaultMat = BaseBuilding->Ruleset->DefaultNonRectWallMaterial;
		if(bIsRoof)
		{
			DefaultMat = BaseBuilding->Ruleset->DefaultRoofMaterial;
		}
		else if(bIsFloor)
		{
			DefaultMat = BaseBuilding->Ruleset->DefaultFloorMaterial;
		}

		// If the building wants to apply params, do it to the roof as well
		if( BaseBuilding->HasBuildingParamsForMIC() )
		{
			DefaultMat = BaseBuilding->GetBuildingParamMIC(BaseBuilding, DefaultMat);
		}

		INT LightmapRes = bIsRoof ? BaseBuilding->RoofLightmapRes : BaseBuilding->NonRectWallLightmapRes;

		UStaticMesh* WallMesh = MakeStaticMeshForPoly(Poly, BaseBuilding, DefaultMat);
		if(WallMesh)
		{
			// Set lightmap info
			WallMesh->LightMapCoordinateIndex = 1;
			WallMesh->LightMapResolution = LightmapRes;

			//Building must be done AFTER LightMapCoordinateIndex is set or the Index 1 light map coordinates will never be sent down
			WallMesh->Build(FALSE, TRUE);

			UStaticMeshComponent* ScopeComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), BaseBuilding);
			check(ScopeComp);

			// Set mesh
			ScopeComp->SetStaticMesh(WallMesh);

			// Settings from StaticMeshActor
			ScopeComp->bAllowApproximateOcclusion = TRUE;
			ScopeComp->bCastDynamicShadow = TRUE;
			ScopeComp->bForceDirectLightMap = TRUE;
			ScopeComp->bUsePrecomputedShadows = TRUE;
			ScopeComp->BlockNonZeroExtent = FALSE;
			ScopeComp->BlockZeroExtent = TRUE;

			// This will stop us creating an RB_BodyInstance for each component
			ScopeComp->bDisableAllRigidBody = TRUE;

			// this shouldn't be called without the CurrentSimpleMeshComp having been set
			check(BaseBuilding->CurrentSimpleMeshComp);

			// Set LOD parent as very simple mesh
			ScopeComp->ReplacementPrimitive = BaseBuilding->CurrentSimpleMeshComp;

			// Add to info array
			INT InfoIndex = BaseBuilding->BuildingMeshCompInfos.AddZeroed();
			BaseBuilding->BuildingMeshCompInfos(InfoIndex).MeshComp = ScopeComp;
			BaseBuilding->BuildingMeshCompInfos(InfoIndex).TopLevelScopeIndex = INDEX_NONE;
		}
	}
}	

//////////////////////////////////////////////////////////////////////////
// WxUnrealEdApp

/**
 *	Find the ULevel that the LOD for the supplied building should be in
 *	@param bCreate	If the level cannot be found, create it now
 */
static ULevel* FindLODLevelForBuilding(AProcBuilding* Building, UBOOL bCreate)
{
	// if we're in a sublevel, then the sublevel must be a real level (ie has been saved to disk and has a real name)
	// so we can find a level with the sublevel's name with an _LOD extension

	ULevel* LODLevel = NULL;
	FString SubLevelName = Building->GetOutermost()->GetName();
	FString LODLevelName = SubLevelName + TEXT("_LOD");

	// look for the _LOD level by name
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	for (INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num(); LevelIndex++)
	{
		ULevelStreaming* LevStream = WorldInfo->StreamingLevels(LevelIndex);
		check(LevStream);
		if(LevStream->PackageName.ToString() == LODLevelName)
		{
			LODLevel = LevStream->LoadedLevel;
			break;
		}
	}

	// create the always loaded sublevel with _LOD extension if it's not already loaded
	if ((LODLevel == NULL) && bCreate)
	{
		LODLevel = CreateAlwaysLoadedLODLevel(*SubLevelName);

		if (!LODLevel)
		{
			appMsgf(AMT_OK, TEXT("Failed to create always loaded sub level %s. You should try to create a sublevel with this name, with AlwaysLoaded streaming type, then try again."), *LODLevelName);
		}
		else
		{
			if( GIsUnattended == FALSE )
			{
				debugf( TEXT("By adding a building in sublevel %s, another sublevel %s has been automatically created.\nYou shouldn't need to use this level, however, it must be saved along with %s (which will eventually happen automatically)\nMAKE SURE TO CHECK THIS PACKAGE IN TO SOURCE CONTROL!"), *SubLevelName, *LODLevelName, *SubLevelName );
			}
		}
	}

	return LODLevel;
}

/** Make sure low LOD Actor (and possibly low LOD level) are correctly created and hooked up */
static void VerifyLowLODActor(AProcBuilding* Building)
{
	UBOOL bSimpleMeshCompNeedsCleaning = TRUE;

	// let's verify that all levels we need are loaded
	// if the building is in a sublevel, then the _LOD level needs to be loaded
	if (Building->GetLevel() != GWorld->PersistentLevel)
	{
		// if we already have a pointer to a low LOD building actor, then the _LOD level is around
		if (Building->LowLODPersistentActor == NULL)
		{
			ULevel* LODLevel = FindLODLevelForBuilding(Building, TRUE);
			if(!LODLevel)
			{
				return;
			}

			// spawn a static mesh actor for the low LOD persistent actor
			ULevel* OldCurrentLevel = GWorld->CurrentLevel;
			GWorld->CurrentLevel = LODLevel;
			Building->LowLODPersistentActor = (AStaticMeshActor*)GWorld->SpawnActor(AProcBuilding_SimpleLODActor::StaticClass(), FName(*(Building->GetName() + TEXT("_LowLODActor"))), Building->Location, Building->Rotation, NULL, TRUE);
			// detach the component so we can muck with it, since the actor is static
			Building->LowLODPersistentActor->DetachComponent(Building->LowLODPersistentActor->StaticMeshComponent);
			GWorld->CurrentLevel = OldCurrentLevel;

			// copy the min draw distance out of the building's setting
			Building->LowLODPersistentActor->StaticMeshComponent->MassiveLODDistance = Building->SimpleMeshMassiveLODDistance;

			// no need to clean up a brand new component
			bSimpleMeshCompNeedsCleaning = FALSE;
		}
		// keep the actors in sync
		else
		{
			if (Building->Location != Building->LowLODPersistentActor->Location)
			{
				Building->LowLODPersistentActor->SetLocation(Building->Location);
			}
		}

		// make sure the prepivots match
		Building->LowLODPersistentActor->PrePivot = Building->PrePivot;

		// make sure any old, stale embedded low LOD components are cleaned up
		if(Building->SimpleMeshComp)
		{
			Building->DetachComponent(Building->SimpleMeshComp);
			Building->SimpleMeshComp = NULL;
		}

		// in the cross-level case, we use the SMA's single static mesh component as the simple mesh component for all calculations
		Building->CurrentSimpleMeshComp = Building->LowLODPersistentActor->StaticMeshComponent;
		Building->CurrentSimpleMeshActor = Building->LowLODPersistentActor;
	}
	else
	{
		// Make sure we have an empty component for the simple LOD version of building, before generating meshes, so it can be set as parent
		if(!Building->SimpleMeshComp)
		{
			// Then create/update component
			Building->SimpleMeshComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), Building);
			check(Building->SimpleMeshComp);

			// copy the min draw distance out of the building's setting
			Building->SimpleMeshComp->MassiveLODDistance = Building->SimpleMeshMassiveLODDistance;

			// no need to clean up a brand new component
			bSimpleMeshCompNeedsCleaning = FALSE;
		}


		// in the non-cross-level case, we use the building's embedded simple mesh component
		Building->CurrentSimpleMeshComp = Building->SimpleMeshComp;
		Building->CurrentSimpleMeshActor = Building;

		// if we went from sublevel to P level, make sure that any existing low LOD actor is destroyed
		if (Building->LowLODPersistentActor)
		{
			GWorld->DestroyActor(Building->LowLODPersistentActor);
			Building->LowLODPersistentActor = NULL;
		}
	}

	// if the component already existed, then we need to do some cleanup
	if (bSimpleMeshCompNeedsCleaning)
	{
		// make sure the materials array is empty (could be old version)
		Building->CurrentSimpleMeshComp->Materials.Empty();
		Building->CurrentSimpleMeshComp->SetStaticMesh(NULL);

		// detach the component from the actor, it will be reattached on completion
		Building->CurrentSimpleMeshActor->DetachComponent(Building->CurrentSimpleMeshComp);
	}
}

/** Ensure that the supplied building has no LOD version or mesh  */
static void ClearBuildingLOD(AProcBuilding* Building)
{
	// Ensure child buildings don't have a low LOD mesh`
	if(Building->SimpleMeshComp)
	{
		Building->DetachComponent(Building->SimpleMeshComp);
		Building->SimpleMeshComp = NULL;
	}

	// if the child building had ever had a low LOD actor in _LOD map, destroy it
	if (Building->LowLODPersistentActor)
	{
		GWorld->DestroyActor(Building->LowLODPersistentActor);
		Building->LowLODPersistentActor = NULL;
	}
}

/** Called when a proc building gets update, remeshed etc */
void WxUnrealEdApp::CB_ProcBuildingUpdate(AProcBuilding* Building)
{
	if(!Building)
	{
		return;
	}

	// don't regenerate the building meshes for buildings that are being passed between levels
	// in the intermediate level
	if (Building->GetLevel()->GetName() == TEXT("TransLevelMoveBuffer"))
	{
		return;
	}

#if DETAILED_PBUPDATE_TIMES
	FString TimerName = FString::Printf(TEXT("PB Update: %s"), *Building->GetName());
	FScopedLogTimer ScopeTimer(*TimerName, FALSE);
#endif	

	// clear existing meshes
	Building->ClearBuildingMeshes();
	// clear out any old 2d scopes
	Building->TopLevelScopes.Empty();
	Building->TopLevelScopeUVInfos.Empty();
	Building->NumMeshedTopLevelScopes = 0;
	
#if DETAILED_PBUPDATE_TIMES
	ScopeTimer.LogDeltaTime(TEXT("Rebuild Brush"));
#endif

	// If we are a child building in this, ask the base building to regen instead.
	{
		AProcBuilding* BaseBuilding = Building->GetBaseMostBuilding();
		if(BaseBuilding != Building)
		{	
			// Ensure this child has no LOD building
			ClearBuildingLOD(Building);

			// Ask parent building to regen
			GCallbackEvent->Send(CALLBACK_ProcBuildingUpdate, BaseBuilding);		
			return;
		}
	}

	// Handle 'random swatch' mode
	if(	Building->Ruleset && Building->Ruleset->bPickRandomSwatch )
	{
		// First check that if it has a name, that its valid within the current ruleset
		if(Building->ParamSwatchName != NAME_None)
		{
			if(Building->Ruleset->GetSwatchIndexFromName(Building->ParamSwatchName) == INDEX_NONE)
			{
				Building->ParamSwatchName = NAME_None;
			}
		}

		// If no swatch applied, pick one.
		if(Building->ParamSwatchName == NAME_None)
		{
			Building->ParamSwatchName = Building->Ruleset->GetRandomSwatchName();
		}
	}


	// Make sure low LOD actor is set up correctly
	VerifyLowLODActor(Building);
	if(Building->CurrentSimpleMeshComp == NULL)
	{		
		return;
	}
		
	// Find all buildings that are grouped together
	TArray<AProcBuilding*> GroupBuildings;
	Building->GetAllGroupedProcBuildings(GroupBuildings);


	// Iterate over each child building updating its OverlappingBuildings array
	for(INT GroupBldIdx=0; GroupBldIdx<GroupBuildings.Num(); GroupBldIdx++)
	{
		AProcBuilding* GroupBuilding = GroupBuildings(GroupBldIdx);

		// Make sure collision data is up to date
		FBSPOps::csgPrepMovingBrush(GroupBuilding);	

		GroupBuilding->FindOverlappingBuildings(GroupBuilding->OverlappingBuildings);
		
		// Ensure child buildings have no meshes or LOD version
		if(GroupBuilding != Building)
		{
			ClearBuildingLOD(GroupBuilding);

			GroupBuilding->ClearBuildingMeshes();
		}
	}

	// Update internal list of top-level scopes, find corresponding rulesets/child-building, and also non-rect faces as FPolys.
	TArray<FPoly> HighDetailPolys;
	TArray<FPoly> LowDetailPolys;

	Building->UpdateTopLevelScopes(GroupBuildings, HighDetailPolys, LowDetailPolys);

	check(Building->TopLevelScopes.Num() == Building->TopLevelScopeInfos.Num());

	// Remember how many top level scopes need to actually be meshed
	Building->NumMeshedTopLevelScopes = Building->TopLevelScopes.Num();


	TArray<FPBScope2D> NonRectBoundScopes;
	TArray<FClipSMPolygon> NonRectTexPolys;	
	CreatePolyBoundScopes(NonRectBoundScopes, NonRectTexPolys, LowDetailPolys);
	check(NonRectBoundScopes.Num() == NonRectTexPolys.Num());

	// Copy these new scopes (that bound polys) to end of TopLevelScopes array
	for(INT i=0; i<NonRectBoundScopes.Num(); i++)
	{
		//NonRectBoundScopes(i).DrawScope(FColor(0,128,255), Building->LocalToWorld(), TRUE);

		Building->TopLevelScopes.AddItem(NonRectBoundScopes(i));

		FPBScopeProcessInfo PolyScopeInfo(0);
		Building->TopLevelScopeInfos.AddItem(PolyScopeInfo);
	}

	// Figure out the UVs for all scopes
	FLOAT TextureSizeInWorldUnits = 0.0f;	// Not needed here
	FVector2D PreNormalizeUVExtents( 0.0f, 0.0f );	// Not needed here
	const UBOOL bAllowCropping = GEngine->UseProcBuildingLODTextureCropping;
	if (!CalculateUVInfos(Building->TopLevelScopes, Building->TopLevelScopeInfos, bAllowCropping, Building->TopLevelScopeUVInfos, TextureSizeInWorldUnits, PreNormalizeUVExtents))
	{
		// if we were unable to layout the mesh
		debugf(TEXT("Failed to generate a layout for the static mesh, aborting construction"));
		Building->CurrentSimpleMeshComp = NULL;
		Building->CurrentSimpleMeshActor = NULL;
		return;
	}

#if DETAILED_PBUPDATE_TIMES
	ScopeTimer.LogDeltaTime(TEXT("Begin ProcessScope"));
	OcclusionTestTime = 0.f;
	MeshTime = 0.f;
	MeshFindTime = 0.f;
#endif

	// Make sure that BuildingQuadStaticMesh is loaded and available for any ProcessScope functions
	if( !GEngine->BuildingQuadStaticMesh )
	{
		GEngine->BuildingQuadStaticMesh = LoadObject<UStaticMesh>(NULL, *GEngine->BuildingQuadStaticMeshName, NULL, LOAD_None, NULL);	
	}

	// Now use desired ruleset for each 'meshed' scope to generate lots of mesh instances
	for(INT i=0; i<Building->NumMeshedTopLevelScopes; i++)
	{
		UProcBuildingRuleset* ScopeRuleset = Building->TopLevelScopeInfos(i).Ruleset;
		if( ScopeRuleset && ScopeRuleset->RootRule )
		{
			ScopeRuleset->RootRule->ProcessScope( Building->TopLevelScopes(i), i, Building, Building->TopLevelScopeInfos(i).OwningBuilding, Building->CurrentSimpleMeshComp );
		}
	}

#if DETAILED_PBUPDATE_TIMES
	debugf(TEXT("Occlusion: %f ms \nMesh: %f ms \nMeshFind: %f ms"), OcclusionTestTime, MeshTime, MeshFindTime);
	ScopeTimer.LogDeltaTime(TEXT("End ProcessScope"));
#endif

	// Remove LODQuads with no child components
	RemoveUnusedLODQuads(Building);

#if DETAILED_PBUPDATE_TIMES
	ScopeTimer.LogDeltaTime(TEXT("RemoveUnusedLODQuads"));
#endif


	// Don't maintain this between regens
	Building->OverlappingBuildings.Empty();
	
	// Convert non-rect face FPolys to static mesh components
	CreatePolyMeshes(Building, HighDetailPolys);
	
#if DETAILED_PBUPDATE_TIMES
	ScopeTimer.LogDeltaTime(TEXT("CreatePolyMeshes"));
#endif

	
	// now that all the components are set up, we can attach them to the scene
	for (INT InfoIndex = 0; InfoIndex < Building->BuildingMeshCompInfos.Num(); InfoIndex++)
	{
		FPBMeshCompInfo& Info = Building->BuildingMeshCompInfos(InfoIndex);
		Building->AttachComponent(Info.MeshComp);	
	}
	
	for (INT InfoIndex = 0; InfoIndex < Building->BuildingFracMeshCompInfos.Num(); InfoIndex++)
	{
		FPBFracMeshCompInfo& Info = Building->BuildingFracMeshCompInfos(InfoIndex);
		Building->AttachComponent(Info.FracMeshComp);	
	}

#if DETAILED_PBUPDATE_TIMES
	ScopeTimer.LogDeltaTime(TEXT("Attaching Components"));
#endif


	FLOAT BuildingSpec = 0.f;
	if(Building->Ruleset)
	{
		BuildingSpec = Building->Ruleset->BuildingLODSpecular;
	}

	//update uv infos first
	UBOOL bHasValidLayout = CalculateUVInfos(Building->TopLevelScopes, Building->TopLevelScopeInfos, bAllowCropping, Building->TopLevelScopeUVInfos, TextureSizeInWorldUnits, PreNormalizeUVExtents);
	// this would have already failed when the building was first made, so should never fail again
	check(bHasValidLayout);


	// Create a static mesh for the basic shape of the building'
	UStaticMesh* SimpleMesh = CreateStaticMeshFromScopes(Building, Building->TopLevelScopes, Building->TopLevelScopeInfos, Building->NumMeshedTopLevelScopes, NonRectTexPolys, Building->CurrentSimpleMeshActor->GetOutermost(), Building->TopLevelScopeUVInfos);		
	if(SimpleMesh)
	{
		// Set mesh
		Building->CurrentSimpleMeshComp->SetStaticMesh(SimpleMesh);

		// Settings from StaticMeshActor
		Building->CurrentSimpleMeshComp->bAcceptsLights = TRUE; // We leave this as TRUE so we can still get specular on low LOD buildings
		Building->CurrentSimpleMeshComp->bAllowApproximateOcclusion = TRUE;
		Building->CurrentSimpleMeshComp->bCastDynamicShadow = FALSE;
		Building->CurrentSimpleMeshComp->bForceDirectLightMap = TRUE;
		Building->CurrentSimpleMeshComp->BlockNonZeroExtent = FALSE;
		Building->CurrentSimpleMeshComp->BlockZeroExtent = FALSE;
		Building->CurrentSimpleMeshComp->bAllowCullDistanceVolume = FALSE;

		// We don't need light maps for LOD meshes as they'll always be rendered emissive with
		// special textures that have lighting baked in
		Building->CurrentSimpleMeshComp->bUsePrecomputedShadows = FALSE;

		Building->CurrentSimpleMeshActor->AttachComponent(Building->CurrentSimpleMeshComp);

#if DETAILED_PBUPDATE_TIMES
		ScopeTimer.LogDeltaTime(TEXT("Create LOD Mesh"));
#endif

		// Create MICs for low-lod building and LODQuads - does not assign textures yet, that happens in lighting build
		CreateMICsForBuilding(Building, SimpleMesh);

#if DETAILED_PBUPDATE_TIMES
		ScopeTimer.LogDeltaTime(TEXT("GenerateMICs"));
#endif

		SimpleMesh->Build(FALSE, TRUE);


#if DETAILED_PBUPDATE_TIMES
		ScopeTimer.LogDeltaTime(TEXT("Build"));
#endif

	}	

	// To make sure LOD changes take effect
	Building->MarkComponentsAsDirty();
	//		Building->CurrentSimpleMeshActor->MarkComponentsAsDirty();

	// mark the map to need to be saved
	Building->MarkPackageDirty();
	Building->CurrentSimpleMeshActor->MarkPackageDirty();

	// Assign instance version number to all buildings in group
	for(INT BuildingIdx=0; BuildingIdx<GroupBuildings.Num(); BuildingIdx++)
	{
		GroupBuildings(BuildingIdx)->BuildingInstanceVersion = UCONST_PROCBUILDING_VERSION;
	}

	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI ) );
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );	
	
	// reset the transient pointers
	Building->CurrentSimpleMeshComp = NULL;
	Building->CurrentSimpleMeshActor = NULL;

}

/** Fix any shared or mis-leveled LOD actors */
void WxUnrealEdApp::CB_FixProcBuildingLODs()
{
	TMap<AStaticMeshActor*,AProcBuilding*> LowToHighLODMap;

	debugf(TEXT("-- Checking Building LODs"));

	for (TObjectIterator<AProcBuilding> It; It; ++It)
	{
		AProcBuilding* Building = *It;
		if(Building && Building->LowLODPersistentActor)
		{
			// See if the level the LOD is in is incorrect
			ULevel* DesiredLODLevel = FindLODLevelForBuilding(Building, FALSE);
			ULevel* CurrentLODLevel = Building->LowLODPersistentActor->GetLevel();
			FName ExpectedLODActorName = FName(*(Building->GetName() + TEXT("_LowLODActor")));
			if(DesiredLODLevel && CurrentLODLevel && (DesiredLODLevel != CurrentLODLevel))
			{
				debugf(TEXT("FixProcBuildingLODs: LOD Actor (%s) for building (%s) in wrong sublevel (%s vs %s) - removing."), *Building->LowLODPersistentActor->GetPathName(), *Building->GetPathName(), *CurrentLODLevel->GetPathName(),  *DesiredLODLevel->GetPathName());
				GWorld->EditorDestroyActor(Building->LowLODPersistentActor, TRUE);
				Building->LowLODPersistentActor = NULL;
				// Will need to regen building after this
				GEngine->DeferredCommands.AddUniqueItem(FString::Printf(TEXT("ProcBuildingUpdate %s"), *Building->GetPathName()));
			}
			// See if it has the right name (this also avoid 2 actors using the same LOD actor)
			else if( Building->LowLODPersistentActor->GetFName() != ExpectedLODActorName)
			{
				debugf(TEXT("FixProcBuildingLODs: LOD Actor for building has wrong name (%s vs %s) - clearing."), *Building->LowLODPersistentActor->GetName(), *ExpectedLODActorName.ToString() );
				Building->LowLODPersistentActor = NULL;
				GEngine->DeferredCommands.AddUniqueItem(FString::Printf(TEXT("ProcBuildingUpdate %s"), *Building->GetPathName()));
			}
			// Make sure it's the right class
			else if( !Building->LowLODPersistentActor->IsA(AProcBuilding_SimpleLODActor::StaticClass()) )
			{
				debugf(TEXT("FixProcBuildingLODs: LOD Actor for building (%s) is wrong class - removing."), *Building->GetPathName() );
				GWorld->EditorDestroyActor(Building->LowLODPersistentActor, TRUE);
				Building->LowLODPersistentActor = NULL;
				GEngine->DeferredCommands.AddUniqueItem(FString::Printf(TEXT("ProcBuildingUpdate %s"), *Building->GetPathName()));
			}
			// LOD seems to be ok!
			else
			{
				LowToHighLODMap.Set(Building->LowLODPersistentActor, Building);
			}
		}
	}

	// Now check for LOD actors that are not used by any building
	for (TObjectIterator<AStaticMeshActor> It; It; ++It)
	{
		AStaticMeshActor* SMActor = *It;
		if(SMActor)
		{
			// Does this actor have the name of 
			UBOOL bHasLODActorName = (appStristr(*SMActor->GetName(), TEXT("_LowLODActor")) != NULL);
			// See if this actor is of the correct building LOD class
			UBOOL bIsCorrectClass = SMActor->IsA(AProcBuilding_SimpleLODActor::StaticClass());
			// See if this actor us being used by a high-detail AProcBuilding
			UBOOL bIsUsed = (LowToHighLODMap.Find(SMActor) != NULL);
			// If we are named as a LOD actor, but are either the wrong class, or not being used, destroy
			if(bHasLODActorName && (!bIsCorrectClass || !bIsUsed))
			{
				debugf(TEXT("FixProcBuildingLODs: LOD Actor (%s) not used by any ProcBuilding - removing."), *SMActor->GetPathName() );
				GWorld->EditorDestroyActor(SMActor, TRUE);
			}
		}
	}
}


/** Attempts to set the base of this static mesh actor to any proc building directly below it. */
void WxUnrealEdApp::CB_AutoBaseSMActorToProcBuilding(AStaticMeshActor* SMActor)
{
	const FLOAT MaxDistance = 5.f;

	// Do nothing if no SMActor, or its in the level move buffer
	if ( (SMActor == NULL) || (SMActor->GetLevel()->GetName() == TEXT("TransLevelMoveBuffer")) )
	{
		return;
	}

	// Make sure the user didn't disable this feature
	if( !SMActor->bDeleteMe && !SMActor->bDisableAutoBaseOnProcBuilding )
	{
		// Attempt to attach this static mesh actor to a proc building directly below it

		// Cast the line check straight down.
		const FVector Direction(0,0,-1);
		// Get the extent for the line check from the collision component 
		// Start the line check from the middle of the bottom face of the bounding box
		const FBox BBox = SMActor->GetComponentsBoundingBox();
		const FVector TraceStart = BBox.GetCenter();
		FVector BoxExtent = BBox.GetExtent();
		// We bring in box a bit, to avoid hitting neighboring buildings
		BoxExtent.X *= 0.9f;
		BoxExtent.Y *= 0.9f;

		// End the line check shortly after
		const FVector TraceEnd = TraceStart + Direction * MaxDistance; 

		//debugf(TEXT("Trace for: %s Ex:(%s)"), *GetPathName(), *BoxExtent.ToString());

		FMemMark Mark(GMainThreadMemStack);
		// Find all things hit by this swept-box check
		FCheckResult* FirstHit = GWorld->MultiLineCheck(GMainThreadMemStack, TraceEnd, TraceStart, BoxExtent, TRACE_World, SMActor);

		// Iterate over each result
		for( FCheckResult* Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
		{
			AProcBuilding* Building = Cast<AProcBuilding>(Check->Actor);
			// If this is a building, and is in my level, go ahead and attach
			if( Building )
			{
				// See if they are in the wrong sublevel
				if(Building->GetOuter() != SMActor->GetOuter())
				{
					// if they are, offer to move this SMA into the same sublevel as the building before doing the attach
					if( appMsgf(AMT_YesNo, TEXT("'%s' not in same sublevel as '%s'. Would you like to make that level Current, move this StaticMeshActor and attach?"), *SMActor->GetPathName(), *Check->Actor->GetPathName()) )
					{	
						// Get building's level..	
						ULevel* BuildingLevel = Building->GetLevel();

						ULevelStreaming* BuildingStreamingLevel = FLevelUtils::FindStreamingLevel( BuildingLevel ); 
						check(BuildingStreamingLevel);

						TLookupMap<AActor*> ActorsArray;
						ActorsArray.AddItem(SMActor);

						INT NumMoved = 0;
						EditorLevelUtils::MovesActorsToLevel(ActorsArray, BuildingStreamingLevel, NumMoved);

						// The new actor in the correct level is now the selected actor
						AStaticMeshActor* NewSMActor = GEditor->GetSelectedActors()->GetTop<AStaticMeshActor>();
						if(NewSMActor && (NewSMActor->GetOuter() == Building->GetOuter()))
						{
							SMActor = NewSMActor;
						}
					}
				}

				// If outers are now the same..
				if(Building->GetOuter() == SMActor->GetOuter())
				{
					// Only rebase the static mesh actor if the base changed 
					if( SMActor->Base != Building )
					{
						SMActor->SetBase( Building );
					}
				}
			}
		}

		Mark.Pop();
	}
}

//////////////////////////////////////////////////////////////////////////
// UUnrealEdEngine

/**
 * Rerender proc building render-to-textures, because lighting has changed and needs to be baked into the textures
 *
 * @param Buildings optional list of buildings to rerender. If empty, this will rerender all buildings in the world
 */
void UUnrealEdEngine::RegenerateProcBuildingTextures(UBOOL bRenderTextures, TArray<AProcBuilding*> Buildings)
{
	// Show a progress bar for rerendering proc building textures
	GWarn->PushStatus();
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("RegenerateProcBuildingTextures")), FALSE);

	DOUBLE TexStart = appSeconds();
	INT NumBuildings = 0;

	// if the array is empty, collect all ProcBuildings in the world
	if (Buildings.Num() == 0)
	{
		for (FActorIterator It; It; ++It)
		{
			AProcBuilding* Building = Cast<AProcBuilding>( *It );
			if(Building)
			{
				Buildings.AddUniqueItem(Building->GetBaseMostBuilding());
			}
		}
	}

	debugf(TEXT("Start RegenerateProcBuildingTextures (%d buildings)"), Buildings.Num());
	DOUBLE StartTime = appSeconds();

	// regenerate texture for the buildings
	for (INT BuildingIndex = 0; BuildingIndex < Buildings.Num(); BuildingIndex++)
	{
		AProcBuilding* Building = Buildings(BuildingIndex);
		if (Building->LowLODPersistentActor)
		{
			Building->CurrentSimpleMeshComp = Building->LowLODPersistentActor->StaticMeshComponent;
		}
		else
		{
			Building->CurrentSimpleMeshComp = Building->SimpleMeshComp;
		}
		if (Building->CurrentSimpleMeshComp == NULL)
		{
			debugf(TEXT("'%s' has a NULL SimpleMeshComp, will skip regenerating textures..."), *Building->GetFullName());
			continue;
		}

		GWarn->StatusUpdatef( BuildingIndex, Buildings.Num(), *FString::Printf( TEXT("LOD Tex : %s"), *Building->GetPathName() ) );

		// Grab the spec amout we want
		FLOAT BuildingSpec = 0.f;
		if(Building->Ruleset)
		{
			BuildingSpec = Building->Ruleset->BuildingLODSpecular;
		}

		// If we want to generate textures, do that..
		if(bRenderTextures)
		{
			DOUBLE TexStart = appSeconds();

			// Figure out the UVs for all scopes
			Building->TopLevelScopeUVInfos.Empty();
			FLOAT TextureSizeInWorldUnits = 0.0f;
			FVector2D PreNormalizeUVExtents( 0.0f, 0.0f );
			const UBOOL bAllowCropping = GEngine->UseProcBuildingLODTextureCropping;
			UBOOL bHasValidLayout = CalculateUVInfos(Building->TopLevelScopes, Building->TopLevelScopeInfos, bAllowCropping, Building->TopLevelScopeUVInfos, TextureSizeInWorldUnits, PreNormalizeUVExtents);

			if (Building->CurrentSimpleMeshComp == NULL)
			{
				debugf(TEXT("'%s' has a NULL SimpleMeshComp, will skip regenerating textures..."), *Building->GetFullName());
				continue;
			}

			// this would have already failed when the building was first made, so should never fail again
			check(bHasValidLayout);

			// A scale value sets the desired number of texels per world unit.  We'll multiply that
			// with the texture's world space size to compute the final desired texture size
			const FLOAT DesiredLODColorTextureSize = GEngine->ProcBuildingLODColorTexelsPerWorldUnit * TextureSizeInWorldUnits * Building->LODRenderToTextureScale;
			const FLOAT DesiredLODLightingTextureSize = GEngine->ProcBuildingLODLightingTexelsPerWorldUnit * TextureSizeInWorldUnits * Building->LODRenderToTextureScale;

			// Use the pre-normalized UV extents to 'crop' the texture's size.  The UVs have already been normalized.
			// Also, we'll clamp to the maximum size set in the building's properties
			INT LODColorTextureWidth = Min( appTrunc( DesiredLODColorTextureSize * PreNormalizeUVExtents.X ), GEngine->MaxProcBuildingLODColorTextureSize );
			INT LODColorTextureHeight = Min( appTrunc( DesiredLODColorTextureSize * PreNormalizeUVExtents.Y ), GEngine->MaxProcBuildingLODColorTextureSize );
			INT LODLightingTextureWidth = Min( appTrunc( DesiredLODLightingTextureSize * PreNormalizeUVExtents.X ), GEngine->MaxProcBuildingLODLightingTextureSize );
			INT LODLightingTextureHeight = Min( appTrunc( DesiredLODLightingTextureSize * PreNormalizeUVExtents.Y ), GEngine->MaxProcBuildingLODLightingTextureSize );

			// For the top-most mip level, we always want to make sure the resolution is at least a multiple
			// of 4 (DXT friendly), but since these textures may be streamed in we'll need to make sure that
			// at least mip level 5 has dimensions divisible by 4 too, so we use a large divisor on the top mip
			INT MinTextureDivisor = 4; 
#if 0
			// @todo: Shouldn't need this once we have mip generation supporting non-pow2's properly
			// Since these textures may be streamed in we'll need to make sure that at least mip level 5
			// has dimensions divisible by 4 too, so we use a large divisor on the top mip
			MinTextureDivisor = 64;
#endif
			LODColorTextureWidth = Max( MinTextureDivisor, LODColorTextureWidth - LODColorTextureWidth % MinTextureDivisor );
			LODColorTextureHeight = Max( MinTextureDivisor, LODColorTextureHeight - LODColorTextureHeight % MinTextureDivisor );
			LODLightingTextureWidth = Max( MinTextureDivisor, LODLightingTextureWidth - LODLightingTextureWidth % MinTextureDivisor );
			LODLightingTextureHeight = Max( MinTextureDivisor, LODLightingTextureHeight - LODLightingTextureHeight % MinTextureDivisor );

			// Round up to the next power of two if we need to
			if( GEngine->ForcePowerOfTwoProcBuildingLODTextures )
			{
				struct Local
				{
					/**
					 * Rounds the value up to the next power of two, unless the difference between
					 * the the value and next lower power of two is within the specified threshold
					 *
					 * @param	Dim				The number to round
					 * @param	ThresholdPct	Scalar for percent of pixel difference for the value to be rounded down
					 */
					static INT RoundToPowerOfTwoWithThreshold( const INT Dim, const FLOAT ThresholdPct )
					{
						check( ThresholdPct >= 0.0f && ThresholdPct < 1.0f );

						const INT RoundedUp = appRoundUpToPowerOfTwo( Dim );
						const INT RoundedDown = RoundedUp >> 1;

						const INT Threshold = appRound( (FLOAT)( RoundedUp - RoundedDown ) * ThresholdPct );

						// If the dimension "just missed" a lower power of two, go ahead and
						// round down to the lower power of two instead of up to the larger
						// power of two						
						if( RoundedDown > Threshold && ( Dim - RoundedDown ) <= Threshold )
						{
							return RoundedDown;
						}

						return RoundedUp;
					}
				};

				// Round up to a power of two, but use a threshold such that we'll actually round
				// down instead of up if the value is close enough
				const FLOAT RoundDownThreshold = 0.25f;	// Round up unless we could save 75% more memory by rounding down
				LODColorTextureWidth = Local::RoundToPowerOfTwoWithThreshold( LODColorTextureWidth, RoundDownThreshold );
				LODColorTextureHeight = Local::RoundToPowerOfTwoWithThreshold( LODColorTextureHeight, RoundDownThreshold );
				LODLightingTextureWidth = Local::RoundToPowerOfTwoWithThreshold( LODLightingTextureWidth, RoundDownThreshold );
				LODLightingTextureHeight = Local::RoundToPowerOfTwoWithThreshold( LODLightingTextureHeight, RoundDownThreshold );
			}

			// Now generate the textures!
			FAutoLODTextureGenerator AutoLODGen(Building, Building->CurrentSimpleMeshComp->StaticMesh);
			AutoLODGen.GenerateTextures(Building->TopLevelScopeUVInfos, LODColorTextureWidth, LODColorTextureHeight, LODLightingTextureWidth, LODLightingTextureHeight, BuildingSpec);

			//debugf(TEXT("GenerateTextures (%s): %f ms (%dx%d %dx%d)"), *Building->GetPathName(), 1000.f*(appSeconds() - TexStart), LODColorTextureWidth, LODColorTextureHeight, LODLightingTextureWidth, LODLightingTextureHeight);
			NumBuildings++;
		}
		// Otherwise just reset simple mesh MICs to NULL (default material)
		else
		{
			UStaticMesh* SimpleMesh = Building->CurrentSimpleMeshComp->StaticMesh;
			check(SimpleMesh);
			UMaterialInstanceConstant* SimpleMIC = Cast<UMaterialInstanceConstant>(SimpleMesh->LODModels(0).Elements(0).Material);
			check(SimpleMIC);

			SimpleMIC->SetTextureParameterValue(FName(TEXT("DiffuseTexture")), NULL);
			SimpleMIC->SetTextureParameterValue(FName(TEXT("LightTexture")), NULL);
			SimpleMIC->SetScalarParameterValue(FName(TEXT("BuildingSpec")), BuildingSpec);
		}

		Building->CurrentSimpleMeshComp = NULL;

		// Force a GC after each few buildings so we don't OOM when processing many objects
		// you will OOM if you don't GC when you have lots of buildings.
        // NOTE: we might be able to reuse the RT to get around having to do this
		if( ( BuildingIndex % 16 ) == 0 )
		{
			UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
		}
	}

	debugf(TEXT("Finish RegenerateProcBuildingTextures (Took %f secs)"), appSeconds() - StartTime);

	GWarn->EndSlowTask();
	GWarn->PopStatus();
	//debugf(TEXT("RegenerateProcBuildingTextures took %f secs (%d buildings)"), 1000.f*(appSeconds() - TexStart), NumBuildings);

	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI ) );

}

/** Finds all PrcBuildings that were edited while in quick mode, and regen's them */
void UUnrealEdEngine::RegenQuickModeEditedProcBuildings()
{
	for( FActorIterator It; It; ++It )
	{
		AProcBuilding* Building = Cast<AProcBuilding>(*It);
		if(Building && !Building->IsTemplate() && Building->bQuickEdited)
		{
			GApp->CB_ProcBuildingUpdate(Building);
		}
	}
}

/** Returns the ruleset of the first selected building to have one assigned */
UProcBuildingRuleset* UUnrealEdEngine::GetSelectedBuildingRuleset()
{
	// Iterate over selected buildings
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{		
		AProcBuilding* Building = Cast<AProcBuilding>( *It );
		if(Building && Building->Ruleset)
		{
			return Building->Ruleset;
		}
	}

	return NULL;
}


/** Returns the ruleset of the building that is being geometry edited.  Will return NULL if not in geom mode. */
UProcBuildingRuleset* UUnrealEdEngine::GetGeomEditedBuildingRuleset()
{
	UProcBuildingRuleset* Ruleset = NULL;

	// Check in geom mode
	if( !GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		return Ruleset;
	}

	FEdModeGeometry* GeomMode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	// Iterate over selected geom
	for( FEdModeGeometry::TGeomObjectIterator Itor( GeomMode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* GO = *Itor;
		AProcBuilding* Building = Cast<AProcBuilding>(GO->GetActualBrush());
		if(Building)
		{
			Ruleset = Building->GetRuleset();
			break;
		}
	}

	return Ruleset;
}

/** Apply the supplied parameter swatch name to the currently selected building(s) */
void UUnrealEdEngine::ApplyParamSwatchToSelectedBuildings(FName SwatchName)
{
	// Iterate over selected actors
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{		
		AProcBuilding* Building = Cast<AProcBuilding>( *It );
		// Check brush is valid
		if(Building)
		{
			Building->ParamSwatchName = SwatchName;
		}

		// Rebuild meshing with new assignment
		GCallbackEvent->Send(CALLBACK_ProcBuildingUpdate, Building);
	}
}

/** Apply the supplied ruleset to the currently selected faces (only works in geom mode) */
void UUnrealEdEngine::ApplyRulesetVariationToSelectedFaces(FName VariationName)
{
	// Check in geom mode
	if( !GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		return;
	}

	FEdModeGeometry* GeomMode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	
	// Iterate over selected geom
	for( FEdModeGeometry::TGeomObjectIterator Itor( GeomMode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* GO = *Itor;
		
		// Get the Brush actor that owns this geom, and check its a building, and has a UModel
		AProcBuilding* Building = Cast<AProcBuilding>(GO->GetActualBrush());
		if(Building && Building->Brush)
		{
			// Find all selected polys
			for( INT PolyIdx=0 ; PolyIdx<GO->PolyPool.Num() ; PolyIdx++ )
			{
				FGeomPoly* GP = &GO->PolyPool(PolyIdx);
				if( GP->IsSelected() )
				{
					// Set ruleset on this FPoly
					Building->Brush->Polys->Element( GP->ActualPolyIndex ).RulesetVariation = VariationName;
				}
			}
		}
		
		// Rebuild meshing with new assignment
		GCallbackEvent->Send(CALLBACK_ProcBuildingUpdate, Building);
	}
}

/** Apply supplied material to faces of ProcBuilding, for use on roof and non-rect faces */
void UUnrealEdEngine::ApplyMaterialToPBFaces(UMaterialInterface* Material)
{
	// Check in geom mode
	if( !GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		return;
	}

	FEdModeGeometry* GeomMode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	// Iterate over selected geom
	for( FEdModeGeometry::TGeomObjectIterator Itor( GeomMode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* GO = *Itor;

		// Get the Brush actor that owns this geom, and check its a building, and has a UModel
		AProcBuilding* Building = Cast<AProcBuilding>(GO->GetActualBrush());
		if(Building && Building->Brush)
		{
			// Find all selected polys
			for( INT PolyIdx=0 ; PolyIdx<GO->PolyPool.Num() ; PolyIdx++ )
			{
				FGeomPoly* GP = &GO->PolyPool(PolyIdx);
				if( GP->IsSelected() )
				{
					// Set material on this FPoly
					Building->Brush->Polys->Element( GP->ActualPolyIndex ).Material = Material;
				}
			}
		}

		// Rebuild meshing with new assignment
		GCallbackEvent->Send(CALLBACK_ProcBuildingUpdate, Building);		
	}
}

void UUnrealEdEngine::ClearRulesetVariationFaceAssignments()
{
	// Iterate over selected actors
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{		
		AProcBuilding* Building = Cast<AProcBuilding>( *It );
		// Check brush is valid
		if(Building && Building->Brush && Building->Brush->Polys)
		{		
			// Iterate over each poly and set ruleset pointer to NULL
			for(INT i=0; i<Building->Brush->Polys->Element.Num(); i++)
			{
				Building->Brush->Polys->Element(i).RulesetVariation = NAME_None;
			}
		}
		
		// Rebuild meshing with new assignment
		GCallbackEvent->Send(CALLBACK_ProcBuildingUpdate, Building);
	}
}

void UUnrealEdEngine::ClearPBMaterialFaceAssignments()
{
	// Iterate over selected actors
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{		
		AProcBuilding* Building = Cast<AProcBuilding>( *It );
		// Check brush is valid
		if(Building && Building->Brush && Building->Brush->Polys)
		{
			// Iterate over each poly and set ruleset pointer to NULL
			for(INT i=0; i<Building->Brush->Polys->Element.Num(); i++)
			{
				Building->Brush->Polys->Element(i).Material = NULL;
			}
		}

		// Rebuild meshing with new assignment
		GCallbackEvent->Send(CALLBACK_ProcBuildingUpdate, Building);
	}
}



/** Show info window showing resources used by currently selected ProcBuilding actors */
void UUnrealEdEngine::ShowPBResourceInfo()
{
	FPBMemUsageInfo TotalInfo(0);
	INT NumBuildings = 0;

	// Iterate over selected actors
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{		
		AProcBuilding* Building = Cast<AProcBuilding>( *It );
		// Check brush is valid
		if(Building)
		{	
			FPBMemUsageInfo BuildingInfo = Building->GetBuildingMemUsageInfo();

			TotalInfo.AddInfo(BuildingInfo);

			debugf(TEXT("%s"), *BuildingInfo.GetString());

			NumBuildings++;
		}

	}
	
	FString InfoString;
	
	InfoString += FString::Printf(TEXT("Buildings: %d\n"), NumBuildings);
	InfoString += FString::Printf(TEXT("SM Components: %d\n"), TotalInfo.NumStaticMeshComponent);
	InfoString += FString::Printf(TEXT("ISM Components: %d\n"), TotalInfo.NumInstancedStaticMeshComponents);
	InfoString += FString::Printf(TEXT("Instanced Tris: %d\n"), TotalInfo.NumInstancedTris);
	InfoString += FString::Printf(TEXT("Lightmap Mem: %dKB\n"), appRound((FLOAT)TotalInfo.LightmapMemBytes/1024.f));
	InfoString += FString::Printf(TEXT("Shadowmap Mem: %dKB\n"), appRound((FLOAT)TotalInfo.ShadowmapMemBytes/1024.f));
	InfoString += FString::Printf(TEXT("LOD Diffuse Mem: %dKB\n"), appRound((FLOAT)TotalInfo.LODDiffuseMemBytes/1024.f));
	InfoString += FString::Printf(TEXT("LOD Lighting Mem: %dKB\n"), appRound((FLOAT)TotalInfo.LODLightingMemBytes/1024.f));
	
	appMsgf(AMT_OK, *InfoString);
}

/** Select base-most building from the one that is selected */
void UUnrealEdEngine::SelectBasePB()
{
	TArray<AProcBuilding*> BaseBuildings;
	
	// Iterate over selected actors
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{		
		// If a building, find its 'root' building
		AProcBuilding* Building = Cast<AProcBuilding>( *It );
		if(Building)
		{		
			AProcBuilding* ThisBaseBuilding = Building->GetBaseMostBuilding();
			ensure(ThisBaseBuilding);
			BaseBuildings.AddUniqueItem(ThisBaseBuilding);
		}
	}
	
	// Change selection set to just the base-most (root) buildings
	
	SelectNone( FALSE, TRUE );
	
	for(INT i=0; i<BaseBuildings.Num(); i++)
	{
		SelectActor(BaseBuildings(i), TRUE, NULL, FALSE);
	}	
	
	NoteSelectionChange();
}

/** Group selected procedural buildings */
void UUnrealEdEngine::GroupSelectedPB()
{
	TArray<AProcBuilding*> SelectedBuildings;
	AProcBuilding* RootBuilding = NULL;

	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AProcBuilding* Building = Cast<AProcBuilding>( *It );
		if(Building)
		{
			SelectedBuildings.AddItem(Building);
			
			// If this is first building we found, it will be the new root for this group
			if(!RootBuilding)
			{
				RootBuilding = Building;
			}
		}
	}
	
	TArray<AProcBuilding*> OldBases;

	// We do the base changing in 2 passes, to avoid any cycle-loop problems	
	
	// First clear all base-ness	
	for(INT i=0; i<SelectedBuildings.Num(); i++)
	{
		// Check old base, and if it's not in our selected set, we'll regen it later
		AProcBuilding* OldBase = SelectedBuildings(i)->GetBaseMostBuilding();
		if(OldBase && !SelectedBuildings.ContainsItem(OldBase))
		{
			OldBases.AddUniqueItem(OldBase);
		}
	
		// Clear base
		SelectedBuildings(i)->SetBase(NULL);
	}
	
	// Then set everything but the root to point to the root	
	for(INT i=0; i<SelectedBuildings.Num(); i++)
	{
		if(SelectedBuildings(i) != RootBuilding)
		{
			SelectedBuildings(i)->SetBase(RootBuilding);		
		}
	}	
	
	GCallbackEvent->Send(CALLBACK_ProcBuildingUpdate, RootBuilding);
	
	
	// Finally regen any old bases, to remove meshes 
	for(INT i=0; i<OldBases.Num(); i++)
	{
		GCallbackEvent->Send(CALLBACK_ProcBuildingUpdate, OldBases(i));
	}
}

/** Convert select additive BSP brushes into a ProcBuilding */
void UUnrealEdEngine::ConvertBSPToProcBuilding()
{
	const FScopedTransaction Transaction( TEXT("Convert BSP To ProcBuilding") );

	TArray<ABrush*> AddBrushes;

	// Iterate over selected actors, to find all non-volume, additive, brushes.
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{		
		ABrush* Brush = Cast<ABrush>( *It );
		if(	Brush && 
			(Brush->CsgOper == CSG_Add) && 
			!Brush->IsA(AVolume::StaticClass()) )
		{
			AddBrushes.AddItem(Brush);
		}
	}
	
	SelectNone( FALSE, TRUE );

	// Tell all active editor modes to empty their selections.
	GEditorModeTools().SelectNone();

	FlushRenderingCommands(); // make sure not trying to render anything from the brushes we are about to delete
	
	debugf(TEXT("CONVERT BSP->PROCBUIDLING: %d Additive Brushes Selected"), AddBrushes.Num());

	// See if we want to delete the BSP brushes after building creation
	UBOOL bDeleteBrushes = appMsgf(AMT_YesNo, TEXT("Delete BSP Brushes?"));

	// See if we have a ruleset selected, will be assigned to root building
	UProcBuildingRuleset* Ruleset = GEditor->GetSelectedObjects()->GetTop<UProcBuildingRuleset>();

	AProcBuilding* RootBuilding = NULL;
	for(INT BrushIdx=0; BrushIdx<AddBrushes.Num(); BrushIdx++)		
	{
		ABrush* Brush = AddBrushes(BrushIdx);
		AProcBuilding* Building = (AProcBuilding*)GWorld->SpawnActor(AProcBuilding::StaticClass(), NAME_None, GWorld->GetBrush()->Location);
		if( Building )
		{
			Building->PreEditChange(NULL);

			FBSPOps::csgCopyBrush
				(
				Building,
				Brush,
				0,
				RF_Transactional,
				1,
				TRUE
				);

			// Set the texture on all polys to NULL.  This stops invisible texture
			// dependencies from being formed on volumes.
			if( Building->Brush )
			{
				for(INT PolyIdx=0 ; PolyIdx<Building->Brush->Polys->Element.Num() ; PolyIdx++)
				{
					FPoly* Poly = &(Building->Brush->Polys->Element(PolyIdx));
					Poly->Material = NULL;
				}
			}
			Building->PostEditChange();
			
			// If there is a root building, attach this one to it
			if(RootBuilding)
			{
				Building->SetBase(RootBuilding);
			}
			// If there is not one yet, this is it.
			else
			{				
				RootBuilding = Building;
				// Assign ruleset
				RootBuilding->Ruleset = Ruleset;
			}
			
			// Delete BSP brush if desired
			if(bDeleteBrushes)
			{
				GWorld->EditorDestroyActor(Brush, FALSE);
			}
		}
	}
	
	// Regen building meshingness
	if(RootBuilding)
	{
		GCallbackEvent->Send(CALLBACK_ProcBuildingUpdate, RootBuilding);		
	}
		
	GCallbackEvent->Send( CALLBACK_LevelDirtied );
	GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
	
	RedrawLevelEditingViewports();
}

/**
 * Rebuild all buildings in the level 
 */
void UUnrealEdEngine::CleanupOldBuildings(UBOOL bSelectedOnly, UBOOL bCheckVersion)
{
	// First, check all building LODs are ok
	GCallbackEvent->Send(CALLBACK_ProcBuildingFixLODs);

	TArray<AProcBuilding*> Buildings;
	for (FActorIterator It; It; ++It)
	{
		AProcBuilding* Building = Cast<AProcBuilding>(*It);
		if ( Building && (!bSelectedOnly || Building->IsSelected()) )
		{
			// update only base buildings, or child buildings if they have low LOD pointers
			UBOOL bIsBaseBuiling = (Building == Building->GetBaseMostBuilding());
			UBOOL bHasLowLODPointer = (Building->LowLODPersistentActor || Building->SimpleMeshComp);

			// if we don't want to check the version, or the version is less than the current one, add to set to remesh
			if( (bIsBaseBuiling || bHasLowLODPointer) && (!bCheckVersion || (Building->BuildingInstanceVersion < UCONST_PROCBUILDING_VERSION)) )
			{
				Buildings.AddItem(Building);
			}
		}
	}

	if (Buildings.Num() > 0)
	{
		GWarn->BeginSlowTask(TEXT("Updating Old ProcBuildings"), TRUE);
		for (INT BuildingIndex = 0; BuildingIndex < Buildings.Num(); BuildingIndex++)
		{
			AProcBuilding* Building = Buildings(BuildingIndex);

			debugf(TEXT("ProcBuilding Update: %s"), *Building->GetPathName());
			GCallbackEvent->Send(CALLBACK_ProcBuildingUpdate, Building);

			GWarn->UpdateProgress(BuildingIndex, Buildings.Num());

			// for 64 bit maybe skip this so we build faster?
			// Force a GC after each few buildings so we don't OOM when processing many objects
			if( ( BuildingIndex % 16 ) == 0 )
			{
				UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
			}
		}

		GWarn->EndSlowTask();
	}
}

/**
 * Fix up the textures and materials associated with buildings, that had the wrong flags set on them 
 */
void UUnrealEdEngine::CleanupOldBuildingTextures()
{
	TArray<UPackage*> Packages;
	for (FActorIterator It; It; ++It)
	{
		AProcBuilding* Building = Cast<AProcBuilding>(*It);
		if (Building)
		{
			Packages.AddUniqueItem(Building->GetOutermost());
		}
	}

	// also clean up old textures/materials that were standalone and should not be (based on name and in same package as a building)
	for (TObjectIterator<UTexture2D> It; It; ++It)
	{
		if (Packages.FindItemIndex(It->GetOutermost()) != INDEX_NONE && 
			It->GetName().StartsWith(TEXT("StaticMesh")) && It->GetName().EndsWith(TEXT("_Faces")))
		{
			It->ClearFlags(RF_Standalone);
		}
	}
	for (TObjectIterator<UMaterialInstanceConstant> It; It; ++It)
	{
		if (Packages.FindItemIndex(It->GetOutermost()) != INDEX_NONE && 
			It->GetName().StartsWith(TEXT("StaticMesh")) && It->GetName().EndsWith(TEXT("_MIC")))
		{
			It->ClearFlags(RF_Standalone);
		}
	}
}

/** Iterate over all InstancedStaticMeshComponents and see how many instances each have. Info printed to log. */
void UUnrealEdEngine::InstancedMeshComponentCount()
{
	TArray<INT> InstanceHisto;
	INT TotalComps=0;
	INT TotalInstances=0;

	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		const UInstancedStaticMeshComponent* InstComp = *It;
		if( InstComp->IsAttached() && !InstComp->IsPendingKill() )
		{
			INT NumInstances = InstComp->PerInstanceSMData.Num();

			// Add count to total comps/instances
			TotalComps++;
			TotalInstances += NumInstances;

			// Expand histogram array if needed
			if(InstanceHisto.Num() <= NumInstances)
			{
				InstanceHisto.AddZeroed((NumInstances - InstanceHisto.Num()) + 1);
			}

			// Update element of histogram
			InstanceHisto(NumInstances) += 1;
		}
	}

	debugf(TEXT("------- Comps: %d Intances: %d"), TotalComps, TotalInstances);
	for(INT BucketIdx=0; BucketIdx<InstanceHisto.Num(); BucketIdx++)
	{
		if( InstanceHisto(BucketIdx) > 0 )
		{
			debugf( TEXT("%d: %d"), BucketIdx, InstanceHisto(BucketIdx)) ;
		}
	}
}

/** See if the supplied ProcBuilding Ruleset is 'approved' - that is, appears in the AuthorizedPBRulesetCollections list. */
UBOOL UUnrealEdEngine::CheckPBRulesetIsApproved(const UProcBuildingRuleset* Ruleset)
{
	// All rulesets are approved if none are set in .ini file
	if(!Ruleset || ApprovedPBRulesetCollections.Num() == 0)
	{
		return TRUE;
	}

	UBOOL bInApprovedCollection = FALSE;

#if WITH_MANAGED_CODE
	FString RulesetFullName = Ruleset->GetFullName();

	// Get the asset database
	FGameAssetDatabase& AssetDatabase = FGameAssetDatabase::Get();

	// Go over each collection listed in ini file..
	for(INT ColIdx=0; ColIdx<ApprovedPBRulesetCollections.Num(); ColIdx++)
	{
		FString CollectionName = ApprovedPBRulesetCollections(ColIdx);
		
		// Get all the assets in the collection
		TArray<FString> AssetFullNames;
		AssetDatabase.QueryAssetsInCollection(CollectionName, EGADCollection::Shared, AssetFullNames);

		// See if this ruleset is in this collection
		if( AssetFullNames.ContainsItem(RulesetFullName) )
		{
			// If so, set bool, stop looking
			bInApprovedCollection = TRUE;
			break;
		}
	}
#else
	bInApprovedCollection = TRUE;
#endif

	return bInApprovedCollection;
}
