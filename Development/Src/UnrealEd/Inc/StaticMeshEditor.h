/*=============================================================================
	StaticMeshEditor.h: StaticMesh editor definitions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	UStaticMeshEditorComponent
//

class UStaticMeshEditorComponent : public UStaticMeshComponent
{
	DECLARE_CLASS(UStaticMeshEditorComponent,UStaticMeshComponent,0,UnrealEd);

	class WxStaticMeshEditor*	StaticMeshEditor;

	// FPrimitiveViewInterface interface.

	virtual DWORD GetLayerMask(const FSceneContext& Context) const;
	virtual void Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual void RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
};

//
//	FStaticMeshEditorViewportClient
//

struct FStaticMeshEditorViewportClient: public FEditorLevelViewportClient, FPreviewScene
{
	class WxStaticMeshEditor*	StaticMeshEditor;
	UStaticMeshEditorComponent*	StaticMeshComponent;
	UEditorComponent*			EditorComponent;

	// Used for when the viewport is rotating around a locked position

	UBOOL bLock;
	FRotator LockRot;
	FVector LockLocation;

	// Constructor.

	FStaticMeshEditorViewportClient( class WxStaticMeshEditor* InStaticMeshEditor );

	// FEditorLevelViewportClient interface.

	virtual FScene* GetScene() { return this; }
	virtual ULevel* GetLevel() { return NULL; }
	virtual FColor GetBackgroundColor();
	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);
	virtual void InputAxis(FChildViewport* Viewport,FName Key,FLOAT Delta,FLOAT DeltaTime);

	virtual void Serialize(FArchive& Ar) { Ar << Input << (FPreviewScene&)*this; }
};

class WxStaticMeshEditor : public wxFrame, public WxSerializableWindow
{
public:
	DECLARE_DYNAMIC_CLASS(WxStaticMeshEditor)

	WxStaticMeshEditor()
	{};
	WxStaticMeshEditor( wxWindow* parent, wxWindowID id, UStaticMesh* InStaticMesh );
	virtual ~WxStaticMeshEditor();

	UStaticMesh* StaticMesh;
	UBOOL DrawOpenEdges;
	UINT NumOpenEdges, NumDoubleSidedShadowTriangles;
	EViewportHolder* ViewportHolder;
	wxSplitterWindow* SplitterWnd;
	WxPropertyWindow* PropertyWindow;
	FStaticMeshEditorViewportClient* ViewportClient;
	WxStaticMeshEditorBar* ToolBar;
	WxStaticMeshEditMenu* MenuBar;

	void GenerateKDop( const FVector* Directions, UINT NumDirections );
	void GenerateSphere();

	virtual void Serialize(FArchive& Ar);

	void OnSize( wxSizeEvent& In );
	void OnPaint( wxPaintEvent& In );
	void UI_ShowEdges( wxUpdateUIEvent& In );
	void UI_ShowWireframe( wxUpdateUIEvent& In );
	void UI_ShowBounds( wxUpdateUIEvent& In );
	void UI_ShowCollision( wxUpdateUIEvent& In );
	void UI_LockCamera( wxUpdateUIEvent& In );
	void OnShowEdges( wxCommandEvent& In );
	void OnShowWireframe( wxCommandEvent& In );
	void OnShowBounds( wxCommandEvent& In );
	void OnShowCollision( wxCommandEvent& In );
	void OnLockCamera( wxCommandEvent& In );
	void OnSaveThumbnailAngle( wxCommandEvent& In );
	void OnCollision6DOP( wxCommandEvent& In );
	void OnCollision10DOPX( wxCommandEvent& In );
	void OnCollision10DOPY( wxCommandEvent& In );
	void OnCollision10DOPZ( wxCommandEvent& In );
	void OnCollision18DOP( wxCommandEvent& In );
	void OnCollision26DOP( wxCommandEvent& In );
	void OnCollisionSphere( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};
