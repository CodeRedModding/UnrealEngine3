//------------------------------------------------------------------------------
// The render widget for Unreal Engine 3.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2005 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#ifndef FxRenderWidgetUE3_H__
#define FxRenderWidgetUE3_H__

#ifdef __UNREAL__

#include "FxRenderWidget.h"
#include "UnrealEd.h"

namespace OC3Ent
{

namespace Face
{

//------------------------------------------------------------------------------
// FFaceFXStudioViewportClient.
//------------------------------------------------------------------------------

struct FFaceFXStudioViewportClient : public FEditorLevelViewportClient
{
	class FxRenderWidgetUE3* RenderWidgetUE3;

	FPreviewScene			 PreviewScene;

	/** Helper class that draws common scene elements. */
	FEditorCommonDrawHelper		DrawHelper;

	UBOOL					 bDrawingInfoWidget;
	
	FFaceFXStudioViewportClient( FxRenderWidgetUE3* InRWUE3 );
	~FFaceFXStudioViewportClient();

	// FEditorLevelViewportClient interface.
	virtual FSceneInterface* GetScene( void ) { return PreviewScene.GetScene(); }
	virtual FLinearColor GetBackgroundColor( void );
	virtual void Draw( FViewport* Viewport, FCanvas* Canvas );
	void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI);
	virtual void Tick( FLOAT DeltaSeconds );

	virtual UBOOL InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual UBOOL InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad=FALSE);
	
	virtual void Serialize( FArchive& Ar );
};

//------------------------------------------------------------------------------
// WxFaceFXStudioPreview.
//------------------------------------------------------------------------------

class WxFaceFXStudioPreview : public wxWindow
{
public:
	FFaceFXStudioViewportClient* PreviewVC;

	WxFaceFXStudioPreview( wxWindow* InParent, wxWindowID InID, class FxRenderWidgetUE3* InRWUE3 );
	~WxFaceFXStudioPreview();

	void CreateViewport( class FxRenderWidgetUE3* InRWUE3 );
	void DestroyViewport( void );

	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};

//------------------------------------------------------------------------------
// FxRenderWidgetUE3.
//------------------------------------------------------------------------------

class FxRenderWidgetUE3 : public FxRenderWidget, public FNotifyHook, public FSerializableObject
{
	typedef FxRenderWidget Super;
	WX_DECLARE_DYNAMIC_CLASS(FxRenderWidgetUE3)
	DECLARE_EVENT_TABLE()

public:
	WxFaceFXStudioPreview*       PreviewWindow;
	USkeletalMesh*			     PreviewSkelMesh;
	UFaceFXStudioSkelComponent*  PreviewSkelComp;

	UAnimNodeSequence*           PreviewAnimNodeSeq;
	
	// Constructor.
	FxRenderWidgetUE3( wxWindow* parent = NULL, FxWidgetMediator* mediator = NULL );
	// Destructor.
	virtual ~FxRenderWidgetUE3();

	// Return the PreviewVC.
	FFaceFXStudioViewportClient* GetPreviewVC( void );

	// Required FxWidget message handlers from the FxRenderWidget interface.
	virtual void OnAppStartup( FxWidget* sender );
	virtual void OnAppShutdown( FxWidget* sender );
	virtual void OnCreateActor( FxWidget* sender );
	virtual void OnLoadActor( FxWidget* sender, const FxString& actorPath );
	virtual void OnCloseActor( FxWidget* sender );
	virtual void OnSaveActor( FxWidget* sender, const FxString& actorPath );
	virtual void OnTimeChanged( FxWidget* sender, FxReal newTime );
	virtual void OnRefresh( FxWidget* sender );
	virtual void OnQueryRenderWidgetCaps( FxWidget* sender, FxRenderWidgetCaps& renderWidgetCaps );

	// FSerializableObject interface.
	virtual void Serialize( FArchive& Ar );

	// FNotifyHook interface.
	virtual void NotifyDestroy( void* Src );
	virtual void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	virtual void NotifyExec( void* Src, const TCHAR* Cmd );

	// Sets the skeletal mesh being previewed.
	void SetSkeletalMesh( USkeletalMesh* pSkeletalMesh );

	// Ticks the render widget.
	void Tick( FLOAT DeltaSeconds );

	// Updates the skeletal component being previewed.
	void UpdateSkeletalComponent( void );

protected:
	// True if the render widget is fully initialized and ready to begin drawing.
	UBOOL bReadyToDraw;

	// Handle size events.
	void OnSize( wxSizeEvent& event );
	void OnHelp( wxHelpEvent& event );
};

} // namespace Face

} // namespace OC3Ent

#endif

#endif
