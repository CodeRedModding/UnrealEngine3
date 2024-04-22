/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MATERIALEDITORPREVIEWSCENE_H__
#define __MATERIALEDITORPREVIEWSCENE_H__

// Forward declarations.
class WxMaterialEditorBase;

class FMaterialEditorPreviewVC : public FEditorLevelViewportClient
{
public:
	FMaterialEditorPreviewVC(WxMaterialEditorBase* InMaterialEditor, UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh);
	virtual ~FMaterialEditorPreviewVC();

	/**
	 * Sets whether or not the grid and world box should be drawn.
	 */
	void SetShowGrid(UBOOL bShowGrid);

	/////////////////////////////////////////
	// FEditorLevelViewportClient interface

	virtual FSceneInterface* GetScene();
	virtual FLinearColor GetBackgroundColor();

	virtual UBOOL InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual UBOOL InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad=FALSE);
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI);
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);
	virtual void MouseMove(FViewport* Viewport, INT X, INT Y);
	virtual void CapturedMouseMove(FViewport* InViewport, INT InMouseX, INT InMouseY);

	virtual void Serialize(FArchive& Ar);

	FPreviewScene*							PreviewScene;

protected:
	WxMaterialEditorBase*					MaterialEditor;

	/** Draw helper to draw common scene elements. */
	FEditorCommonDrawHelper					DrawHelper;
};

#endif // __MATERIALEDITORPREVIEWSCENE_H__
