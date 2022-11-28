/*=============================================================================
	UnrealEdApp.h: The main application class

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

class UUnrealEdEngine;

class WxUnrealEdApp : public WxLaunchApp
{
public:
	WxEditorFrame* EditorFrame;							// The overall editor frame (houses everything)
	WEditorFrame* OldEditorFrame;

	FString LastDir[LD_MAX];
	FString MapExt;
	W2DShapeEditor* TwoDeeShapeEditor;
	WxDlgSurfaceProperties* DlgSurfProp;
	WBuildPropSheet* BuildSheet;
	WBrowserGroup* BrowserGroup;

	WDlgMapCheck* DlgMapCheck;
	WDlgLoadErrors* DlgLoadErrors;
	WDlgSearchActors* DlgSearchActors;

	UTexture2D *MaterialEditor_RightHandleOn, *MaterialEditor_RightHandle,
		*MaterialEditor_RGBA, *MaterialEditor_R, *MaterialEditor_G, *MaterialEditor_B, *MaterialEditor_A,
		*MaterialEditor_LeftHandle, *MaterialEditor_ControlPanelFill, *MaterialEditor_ControlPanelCap,
		*UpArrow, *DownArrow, *MaterialEditor_LabelBackground, *MaterialEditor_Delete;
	TMap<INT,UClass*> ShaderExpressionMap;		// For relating menu ID's to shader expression classes

	WxPropertyWindowFrame*	ObjectPropertyWindow;

	TArray<class WxKismet*>	KismetWindows;

	virtual bool OnInit();
	virtual int OnExit();
	void SetCurrentMode( EEditorMode InMode );

	// Callback handlers

	void CB_Browse();
	void CB_UseCurrent();
	void CB_SelectionChanged();
	void CB_ViewportUpdateWindowFrame();
	void CB_SurfProps();
	void CB_ActorProps();
	void CB_SaveMap();
	void CB_LoadMap();
	void CB_PlayMap();
	void CB_CameraModeChanged();
	void CB_ActorPropertiesChanged();
	void CB_SaveMapAs();
	void CB_DisplayLoadErrors();
	void CB_RefreshEditor( DWORD InFlags );
	void CB_MapChange( UBOOL InRebuildCollisionHash );
	void CB_AnimationSeqRightClicked();
	void CB_RedrawAllViewports();
	void CB_Undo();
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
