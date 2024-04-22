/*=============================================================================
	SpeedTreeEditor.h: SpeedTree editor definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if WITH_SPEEDTREE

class WxSpeedTreeEditor;

struct FSpeedTreeEditorViewportClient : public FEditorLevelViewportClient, public FPreviewScene
{
public:
	FSpeedTreeEditorViewportClient(class WxSpeedTreeEditor* pwxEditor);

	virtual FSceneInterface* GetScene()
	{ 
		return FPreviewScene::GetScene( ); 
	}
	
	virtual FLinearColor GetBackgroundColor()
	{ 
		return FColor(64, 64, 64); 
	}
	
	virtual UBOOL InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual UBOOL InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad=FALSE);
	
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Input << (FPreviewScene&)*this; 
	}

	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);

protected:
	WxSpeedTreeEditor*			SpeedTreeEditor;
	class USpeedTreeComponent*	SpeedTreeComponent;
};

class WxSpeedTreeEditor : public wxFrame, public FSerializableObject
{
	DECLARE_DYNAMIC_CLASS(WxSpeedTreeEditor)
public:
	
	WxSpeedTreeEditor() { }
	WxSpeedTreeEditor(wxWindow* Parent, wxWindowID wxID, class USpeedTree* SpeedTree);

	virtual	~WxSpeedTreeEditor();

	virtual void Serialize(FArchive& Ar);

	void OnSize(wxSizeEvent& wxIn);
	void OnPaint(wxPaintEvent& wxIn);
	
	DECLARE_EVENT_TABLE( )

public:
	class USpeedTree*				SpeedTree;
	WxViewportHolder* 				ViewportHolder;
	wxSplitterWindow* 				SplitterWnd;
	WxPropertyWindowHost* 			PropertyWindow;
	FSpeedTreeEditorViewportClient*	ViewportClient;
};

#endif

