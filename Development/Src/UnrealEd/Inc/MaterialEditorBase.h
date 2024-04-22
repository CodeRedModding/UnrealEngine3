/*=============================================================================
	MaterialEditorBase.h:	Base class for the material editor and material instance editor.  
							Contains info needed for previewing materials.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MATERIALINSTANCEBASE_H__
#define __MATERIALINSTANCEBASE_H__

#include "TrackableWindow.h"

// Forward declarations.
class FMaterialEditorPreviewVC;
class WxMaterialEditorPreview;

/**
 * Base material editor class
 */
class WxMaterialEditorBase : public WxTrackableFrame, public FCallbackEventDevice
{
public:

	/** Viewport client for the 3D material preview region. */
	FMaterialEditorPreviewVC*				PreviewVC;

	/** Preview window for the material instance. */
	WxMaterialEditorPreview*				PreviewWin;

	/** The material instance applied to the preview mesh. */
	UMaterialInterface*						MaterialInterface;

	/** Component for the preview static mesh. */
	UMaterialEditorMeshComponent*			PreviewMeshComponent;

	/** Component for the preview skeletal mesh. */
	UMaterialEditorSkeletalMeshComponent*	PreviewSkeletalMeshComponent;

	/** The shape to use for the preview windows. */
	EThumbnailPrimType						PreviewPrimType;

	/** If TRUE, render background object in the preview scene. */
	UBOOL									bShowBackground;

	/** If TRUE, render grid the preview scene. */
	UBOOL									bShowGrid;

	/** If TRUE, use PreviewSkeletalMeshComponent as the preview mesh; if FALSe, use PreviewMeshComponent. */
	UBOOL									bUseSkeletalMeshAsPreview;

	WxMaterialEditorBase(wxWindow* InParent, wxWindowID InID, UMaterialInterface* InMaterialInterface);
	virtual ~WxMaterialEditorBase();

	/**
	 * This function is called when the window has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();

	/**
	 * Refreshes the viewport containing the preview mesh.
	 */
	void RefreshPreviewViewport();

	/**
	 * Draws messages on the canvas.
	 */
	virtual void DrawMessages(FViewport* Viewport,FCanvas* Canvas) {}

	/**
	 * Draws material info strings such as instruction count and current errors onto the canvas.
	 */
	void DrawMaterialInfoStrings(
		FCanvas* Canvas, 
		const UMaterial* Material, 
		const FMaterialResource* MaterialResource, 
		const TArray<FString>& CompileErrors, 
		INT &DrawPositionY, 
		UBOOL bDrawInstructions);

	// FSerializableObject interface
	virtual void Serialize(FArchive& Ar);

protected:
	/**
	 * Sets the mesh on which to preview the material.  One of either InStaticMesh or InSkeletalMesh must be non-NULL!
	 * Does nothing if a skeletal mesh was specified but the material has bUsedWithSkeletalMesh=FALSE.
	 *
	 * @return	TRUE if a mesh was set successfully, FALSE otherwise.
	 */
	UBOOL SetPreviewMesh(UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh);

	/**
	 * Sets the preview mesh to the named mesh, if it can be found.  Checks static meshes first, then skeletal meshes.
	 * Does nothing if the named mesh is not found or if the named mesh is a skeletal mesh but the material has
	 * bUsedWithSkeletalMesh=FALSE.
	 *
	 * @return	TRUE if the named mesh was found and set successfully, FALSE otherwise.
	 */
	UBOOL SetPreviewMesh(const TCHAR* InMeshName);

	/**
	 * Sets the preview mesh for the preview window to the selected primitive type.
	 */
	void SetPrimitivePreview();

	/**
	 * Called by SetPreviewMesh, allows derived types to veto the setting of a preview mesh.
	 *
	 * @return	TRUE if the specified mesh can be set as the preview mesh, FALSE otherwise.
	 */
	virtual UBOOL ApproveSetPreviewMesh(UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh);

	/**
	 *
	 */
	void SetPreviewMaterial(UMaterialInterface* InMaterialRenderProxy);

	/** Toggles showing of the background in the preview window. */
	void ToggleShowBackground();

	/** FCallbackEventDevice interface */
	void Send(ECallbackEventType InType, UObject* InObject);

	/** Wx Event Handlers. */
	void OnPrimTypeCylinder(wxCommandEvent& In);
	void OnPrimTypeCube(wxCommandEvent& In);
	void OnPrimTypeSphere(wxCommandEvent& In);
	void OnPrimTypePlane(wxCommandEvent& In);
	void OnRealTimePreview(wxCommandEvent& In);
	void OnShowBackground(wxCommandEvent& In);
	void OnToggleGrid(wxCommandEvent& In);
	virtual void OnSetPreviewMeshFromSelection(wxCommandEvent& In);
	virtual void OnSyncGenericBrowser(wxCommandEvent& In);
	virtual UObject* GetSyncObject() { return NULL; };


	void UI_PrimTypeCylinder(wxUpdateUIEvent& In);
	void UI_PrimTypeCube(wxUpdateUIEvent& In);
	void UI_PrimTypeSphere(wxUpdateUIEvent& In);
	void UI_PrimTypePlane(wxUpdateUIEvent& In);
	void UI_RealTimePreview(wxUpdateUIEvent& In);
	void UI_ShowGrid(wxUpdateUIEvent& In);

	DECLARE_EVENT_TABLE()
};

#endif	// __MATERIALINSTANCEBASE_H__
