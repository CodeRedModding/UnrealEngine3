/*=============================================================================
	MaterialEditorBase.cpp:	Base class for the material editor and material instance editor.  
							Contains info needed for previewing materials.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "MaterialEditorPreviewScene.h"
#include "MaterialEditorBase.h"
#include "UnLinkedObjDrawUtils.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxMaterialEditorPrevew
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * wxWindows container for FMaterialEditorPreviewVC.
 */
class WxMaterialEditorPreview : public wxWindow
{
public:
	FMaterialEditorPreviewVC*	MaterialEditorPrevewVC;

	WxMaterialEditorPreview(wxWindow* InParent, wxWindowID InID, WxMaterialEditorBase* InMaterialEditor, const FVector& InViewLocation, const FRotator& InViewRotation)
		:	wxWindow( InParent, InID )
	{
		MaterialEditorPrevewVC = new FMaterialEditorPreviewVC( InMaterialEditor, GUnrealEd->GetThumbnailManager()->TexPropSphere, NULL );
		MaterialEditorPrevewVC->Viewport = GEngine->Client->CreateWindowChildViewport( MaterialEditorPrevewVC, (HWND)GetHandle() );
		MaterialEditorPrevewVC->Viewport->CaptureJoystickInput( FALSE );

		MaterialEditorPrevewVC->ViewLocation = InViewLocation;
		MaterialEditorPrevewVC->ViewRotation = InViewRotation;
		MaterialEditorPrevewVC->SetViewLocationForOrbiting( FVector(0.f,0.f,0.f) );
	}

	virtual ~WxMaterialEditorPreview()
	{
		GEngine->Client->CloseViewport( MaterialEditorPrevewVC->Viewport );
		MaterialEditorPrevewVC->Viewport = NULL;
		delete MaterialEditorPrevewVC;
	}

private:
	void OnSize(wxSizeEvent& In)
	{
		MaterialEditorPrevewVC->Viewport->Invalidate();
		const wxRect rc = GetClientRect();
		::MoveWindow( (HWND)MaterialEditorPrevewVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
		In.Skip();
	}

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE( WxMaterialEditorPreview, wxWindow )
	EVT_SIZE( WxMaterialEditorPreview::OnSize )
END_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxMaterialEditorBase
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(WxMaterialEditorBase, WxTrackableFrame)
	EVT_TOOL( IDM_SHOW_BACKGROUND, WxMaterialEditorBase::OnShowBackground )
	EVT_TOOL( ID_MATERIALEDITOR_TOGGLEGRID, WxMaterialEditorBase::OnToggleGrid )

	EVT_TOOL( ID_PRIMTYPE_CYLINDER, WxMaterialEditorBase::OnPrimTypeCylinder )
	EVT_TOOL( ID_PRIMTYPE_CUBE, WxMaterialEditorBase::OnPrimTypeCube )
	EVT_TOOL( ID_PRIMTYPE_SPHERE, WxMaterialEditorBase::OnPrimTypeSphere )
	EVT_TOOL( ID_PRIMTYPE_PLANE, WxMaterialEditorBase::OnPrimTypePlane )
	EVT_TOOL( ID_MATERIALEDITOR_REALTIME_PREVIEW, WxMaterialEditorBase::OnRealTimePreview )
	EVT_TOOL( ID_MATERIALEDITOR_SET_PREVIEW_MESH_FROM_SELECTION, WxMaterialEditorBase::OnSetPreviewMeshFromSelection )
	EVT_TOOL( ID_MATERIALEDITOR_SYNCGENERICBROWSER, WxMaterialEditorBase::OnSyncGenericBrowser )


	EVT_UPDATE_UI( ID_PRIMTYPE_CYLINDER, WxMaterialEditorBase::UI_PrimTypeCylinder )
	EVT_UPDATE_UI( ID_PRIMTYPE_CUBE, WxMaterialEditorBase::UI_PrimTypeCube )
	EVT_UPDATE_UI( ID_PRIMTYPE_SPHERE, WxMaterialEditorBase::UI_PrimTypeSphere )
	EVT_UPDATE_UI( ID_PRIMTYPE_PLANE, WxMaterialEditorBase::UI_PrimTypePlane )
	EVT_UPDATE_UI( ID_MATERIALEDITOR_REALTIME_PREVIEW, WxMaterialEditorBase::UI_RealTimePreview )
	EVT_UPDATE_UI( ID_MATERIALEDITOR_TOGGLEGRID, WxMaterialEditorBase::UI_ShowGrid )
END_EVENT_TABLE()

WxMaterialEditorBase::WxMaterialEditorBase(wxWindow* InParent, wxWindowID InID, UMaterialInterface* InMaterialInterface)
	:	WxTrackableFrame( InParent, InID, TEXT(""), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
	,	PreviewPrimType( TPT_None )
	,	bUseSkeletalMeshAsPreview( FALSE )
	,	bShowBackground( FALSE )
	,	bShowGrid( TRUE )
{
	// Create 3D preview window.
	const FRotator ViewAngle( 0, 0, 0 );
	PreviewWin = new WxMaterialEditorPreview( this, -1, this, FVector(0.f,0.f,0.f), ViewAngle );
	PreviewVC = PreviewWin->MaterialEditorPrevewVC;
	if (InMaterialInterface)
	{
		SetPreviewMaterial( InMaterialInterface );
	}

	// Default to the sphere.
	SetPreviewMesh( GUnrealEd->GetThumbnailManager()->TexPropSphere, NULL );

	if(GCallbackEvent)
	{
		// Register our callbacks
		GCallbackEvent->Register(CALLBACK_MaterialTextureSettingsChanged, this);
	}
}

WxMaterialEditorBase::~WxMaterialEditorBase()
{
	if(GCallbackEvent)
	{
		// Unregister all of our events
		GCallbackEvent->UnregisterAll(this);
	}
}

void WxMaterialEditorBase::Serialize(FArchive& Ar)
{
	PreviewVC->Serialize(Ar);

	Ar << MaterialInterface;
	Ar << PreviewMeshComponent;
	Ar << PreviewSkeletalMeshComponent;
}

/**
 * Sets the mesh on which to preview the material.  One of either InStaticMesh or InSkeletalMesh must be non-NULL!
 * Does nothing if a skeletal mesh was specified but the material has bUsedWithSkeletalMesh=FALSE.
 *
 * @return	TRUE if a mesh was set successfully, FALSE otherwise.
 */
UBOOL WxMaterialEditorBase::SetPreviewMesh(UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh)
{
	// Allow derived types to veto setting the mesh.
	const UBOOL bApproved = ApproveSetPreviewMesh( InStaticMesh, InSkeletalMesh );
	if ( !bApproved )
	{
		return FALSE;
	}

	bUseSkeletalMeshAsPreview = InSkeletalMesh != NULL;
	if ( bUseSkeletalMeshAsPreview )
	{
		// Remove the static mesh from the preview scene.
		PreviewVC->PreviewScene->AddComponent( PreviewSkeletalMeshComponent, FMatrix::Identity );
		PreviewVC->PreviewScene->RemoveComponent( PreviewMeshComponent );

		// Update the toolbar state implicitly through PreviewPrimType.
		PreviewPrimType = TPT_None;

		// Set the new preview skeletal mesh.
		PreviewSkeletalMeshComponent->SetSkeletalMesh( InSkeletalMesh );
	}
	else
	{
		FMatrix Matrix = FMatrix::Identity;
		// Update the toolbar state implicitly through PreviewPrimType.
		if ( InStaticMesh == GUnrealEd->GetThumbnailManager()->TexPropCylinder )
		{
			PreviewPrimType = TPT_Cylinder;
		}
		else if ( InStaticMesh == GUnrealEd->GetThumbnailManager()->TexPropCube )
		{
			PreviewPrimType = TPT_Cube;
		}
		else if ( InStaticMesh == GUnrealEd->GetThumbnailManager()->TexPropSphere )
		{
			PreviewPrimType = TPT_Sphere;
		}
		else if ( InStaticMesh == GUnrealEd->GetThumbnailManager()->TexPropPlane )
		{
			PreviewPrimType = TPT_Plane;
			Matrix = Matrix * FRotationMatrix( FRotator(0, 32767, 0) );
		}
		else
		{
			PreviewPrimType = TPT_None;
		}

		// Remove the skeletal mesh from the preview scene.
		PreviewVC->PreviewScene->RemoveComponent( PreviewSkeletalMeshComponent );
		PreviewVC->PreviewScene->AddComponent( PreviewMeshComponent, Matrix );

		// Set the new preview static mesh.
		FComponentReattachContext ReattachContext( PreviewMeshComponent );
		PreviewMeshComponent->StaticMesh = InStaticMesh;
	}

	return TRUE;
}

/**
 * Sets the preview mesh to the named mesh, if it can be found.  Checks static meshes first, then skeletal meshes.
 * Does nothing if the named mesh is not found or if the named mesh is a skeletal mesh but the material has
 * bUsedWithSkeletalMesh=FALSE.
 *
 * @return	TRUE if the named mesh was found and set successfully, FALSE otherwise.
 */
UBOOL WxMaterialEditorBase::SetPreviewMesh(const TCHAR* InMeshName)
{
	UBOOL bSuccess = FALSE;
	if ( InMeshName )
	{
		UStaticMesh* StaticMesh = FindObject<UStaticMesh>( ANY_PACKAGE, InMeshName );
		if ( StaticMesh )
		{
			bSuccess = SetPreviewMesh( StaticMesh, NULL );
		}
		else
		{
			USkeletalMesh* SkeletalMesh = FindObject<USkeletalMesh>( ANY_PACKAGE, InMeshName );
			if ( SkeletalMesh )
			{
				bSuccess = SetPreviewMesh( NULL, SkeletalMesh );
			}
		}
	}
	return bSuccess;
}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxMaterialEditorBase::OnSelected()
{
	Raise();
}

/**
 * Called by SetPreviewMesh, allows derived types to veto the setting of a preview mesh.
 *
 * @return	TRUE if the specified mesh can be set as the preview mesh, FALSE otherwise.
 */
UBOOL WxMaterialEditorBase::ApproveSetPreviewMesh(UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh)
{
	// Default impl is to always accept.
	return TRUE;
}

/**
 *
 */
void WxMaterialEditorBase::SetPreviewMaterial(UMaterialInterface* InMaterialInterface)
{
	check( PreviewMeshComponent );
	check( PreviewSkeletalMeshComponent );

	MaterialInterface = InMaterialInterface;

	PreviewMeshComponent->Materials.Empty();
	PreviewMeshComponent->Materials.AddItem( InMaterialInterface );
	PreviewSkeletalMeshComponent->Materials.Empty();
	PreviewSkeletalMeshComponent->Materials.AddItem( InMaterialInterface );
}

/**
* Called when there is a Callback issued.
* @param InType	The type of callback that was issued.
* @param InObject	Object that was modified.
*/
void WxMaterialEditorBase::Send(ECallbackEventType InType, UObject* InObject)
{
	switch(InType)
	{
	case CALLBACK_MaterialTextureSettingsChanged:
		if(InObject == MaterialInterface)
		{
			// If a texture used by our preview mesh has changed format, we need to reattach as it forced a material compile.
			RefreshPreviewViewport();
		}
		break;
	}
}

/** Refreshes the viewport containing the preview mesh. */
void WxMaterialEditorBase::RefreshPreviewViewport()
{
	//reattach the preview components, so if the preview material changed it will be propagated to the render thread
	PreviewMeshComponent->BeginDeferredReattach();
	PreviewSkeletalMeshComponent->BeginDeferredReattach();
	PreviewVC->Viewport->Invalidate();
}

/**
 * Draws material info strings such as instruction count and current errors onto the canvas.
 */
void WxMaterialEditorBase::DrawMaterialInfoStrings(
	FCanvas* Canvas, 
	const UMaterial* Material, 
	const FMaterialResource* MaterialResource, 
	const TArray<FString>& CompileErrors, 
	INT &DrawPositionY,
	UBOOL bDrawInstructions)
{
	check(Material && MaterialResource);

	// The font to use when displaying info strings
	UFont* FontToUse = GEngine->TinyFont;
	const INT SpacingBetweenLines = 13;

	if (bDrawInstructions)
	{
		// Display any errors and messages in the upper left corner of the viewport.
		TArray<FString> Descriptions;
		TArray<INT> InstructionCounts;
		MaterialResource->GetRepresentativeInstructionCounts(Descriptions, InstructionCounts);

		for (INT InstructionIndex = 0; InstructionIndex < Descriptions.Num(); InstructionIndex++)
		{
			FString InstructionCountString = FString::Printf(TEXT("%s: %u instructions"),*Descriptions(InstructionIndex),InstructionCounts(InstructionIndex));
			FLinkedObjDrawUtils::DrawShadowedString( Canvas, 5, DrawPositionY, *InstructionCountString, FontToUse, FLinearColor(1,1,0) );
			DrawPositionY += SpacingBetweenLines;
		}

		// Display the number of samplers used by the material.
		const INT SamplersUsed = MaterialResource->GetSamplerUsage();
		INT MaxSamplers = Material->LightingModel == MLM_Unlit ? 16 : 
			(Material->bUsedWithStaticLighting ? MAX_ME_STATICLIGHTING_PIXELSHADER_SAMPLERS : MAX_ME_DYNAMICLIGHTING_PIXELSHADER_SAMPLERS);
		if( Material->bUsedWithScreenDoorFade )
		{
			// Screen door fade uses up a texture sampler
			--MaxSamplers;
		}
		FLinkedObjDrawUtils::DrawShadowedString(
			Canvas,
			5,
			DrawPositionY,
			*FString::Printf(TEXT("Texture samplers: %u/%u"), SamplersUsed, MaxSamplers),
			FontToUse,
			SamplersUsed > MaxSamplers ? FLinearColor(1,0,0) : FLinearColor(1,1,0)
			);
		DrawPositionY += SpacingBetweenLines;
	}

	for(INT ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
	{
		FLinkedObjDrawUtils::DrawShadowedString( Canvas, 5, DrawPositionY, *CompileErrors(ErrorIndex), FontToUse, FLinearColor(1,0,0) );
		DrawPositionY += SpacingBetweenLines;
	}
}

void WxMaterialEditorBase::OnShowBackground(wxCommandEvent& In)
{
	ToggleShowBackground();
}

void WxMaterialEditorBase::OnToggleGrid(wxCommandEvent& In)
{
	bShowGrid = In.IsChecked() ? TRUE : FALSE;
	PreviewVC->SetShowGrid( bShowGrid );
	RefreshPreviewViewport();
}

/**
 * Sets the preview mesh for the preview window to the selected primitive type.
 */
void WxMaterialEditorBase::SetPrimitivePreview()
{
	switch(PreviewPrimType)
	{
	case TPT_Cube:
		SetPreviewMesh( GUnrealEd->GetThumbnailManager()->TexPropCube, NULL );
		break;
	case TPT_Plane:
		SetPreviewMesh( GUnrealEd->GetThumbnailManager()->TexPropPlane, NULL );
		break;
	case TPT_Cylinder:
		SetPreviewMesh( GUnrealEd->GetThumbnailManager()->TexPropCylinder, NULL );
		break;
	default:
		SetPreviewMesh( GUnrealEd->GetThumbnailManager()->TexPropSphere, NULL );
	}

	RefreshPreviewViewport();
}

void WxMaterialEditorBase::OnPrimTypeCylinder(wxCommandEvent& In)
{
	PreviewPrimType = TPT_Cylinder;
	SetPrimitivePreview();
}

void WxMaterialEditorBase::OnPrimTypeCube(wxCommandEvent& In)
{
	PreviewPrimType = TPT_Cube;
	SetPrimitivePreview();
}

void WxMaterialEditorBase::OnPrimTypeSphere(wxCommandEvent& In)
{
	PreviewPrimType = TPT_Sphere;
	SetPrimitivePreview();
}

void WxMaterialEditorBase::OnPrimTypePlane(wxCommandEvent& In)
{
	PreviewPrimType = TPT_Plane;
	SetPrimitivePreview();
}

void WxMaterialEditorBase::OnRealTimePreview(wxCommandEvent& In)
{
	PreviewVC->ToggleRealtime();
}

void WxMaterialEditorBase::ToggleShowBackground()
{
	bShowBackground = !bShowBackground;
	// @todo DB: Set the background mesh for the preview viewport.
	RefreshPreviewViewport();
}


void WxMaterialEditorBase::OnSetPreviewMeshFromSelection(wxCommandEvent& In)
{
	UBOOL bFoundPreviewMesh = FALSE;
	GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

	// Look for a selected static mesh.
	UStaticMesh* SelectedStaticMesh = GEditor->GetSelectedObjects()->GetTop<UStaticMesh>();
	if ( SelectedStaticMesh )
	{
		SetPreviewMesh( SelectedStaticMesh, NULL );
		MaterialInterface->PreviewMesh = SelectedStaticMesh->GetPathName();
		bFoundPreviewMesh = TRUE;
	}
	else
	{
		// No static meshes were selected; look for a selected skeletal mesh.
		USkeletalMesh* SelectedSkeletalMesh = GEditor->GetSelectedObjects()->GetTop<USkeletalMesh>();
		if ( SelectedSkeletalMesh )
		{
			SetPreviewMesh( NULL, SelectedSkeletalMesh );
			MaterialInterface->PreviewMesh = SelectedSkeletalMesh->GetPathName();
			bFoundPreviewMesh = TRUE;
		}
	}

	if ( bFoundPreviewMesh )
	{
		MaterialInterface->MarkPackageDirty();
		RefreshPreviewViewport();
	}
}

void WxMaterialEditorBase::OnSyncGenericBrowser(wxCommandEvent& In)
{
	UObject* SyncMI = GetSyncObject();

	TArray<UObject*> Objects;
	if (SyncMI)
	{
		Objects.AddItem(SyncMI);
		GApp->EditorFrame->SyncBrowserToObjects(Objects);
	}
}


void WxMaterialEditorBase::UI_PrimTypeCylinder(wxUpdateUIEvent& In)
{
	In.Check( PreviewPrimType == TPT_Cylinder );
}

void WxMaterialEditorBase::UI_PrimTypeCube(wxUpdateUIEvent& In)
{
	In.Check( PreviewPrimType == TPT_Cube );
}

void WxMaterialEditorBase::UI_PrimTypeSphere(wxUpdateUIEvent& In)
{
	In.Check( PreviewPrimType == TPT_Sphere );
}

void WxMaterialEditorBase::UI_PrimTypePlane(wxUpdateUIEvent& In)
{
	In.Check( PreviewPrimType == TPT_Plane );
}

void WxMaterialEditorBase::UI_RealTimePreview(wxUpdateUIEvent& In)
{
	In.Check( PreviewVC->IsRealtime() == TRUE );
}

void WxMaterialEditorBase::UI_ShowGrid(wxUpdateUIEvent& In)
{
	In.Check( bShowGrid == TRUE );
}

