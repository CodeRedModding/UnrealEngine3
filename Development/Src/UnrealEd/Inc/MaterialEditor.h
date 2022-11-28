
#include "UnrealEd.h"

#define LINE_LEADER_SZ 16

/*-----------------------------------------------------------------------------
	FExpressionPreview
-----------------------------------------------------------------------------*/

struct FExpressionPreview : public FMaterial
{
	UMaterialExpression*	Expression;

	FExpressionPreview() {}
	FExpressionPreview(UMaterialExpression* InExpression):
		Expression(InExpression)
	{}

	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler)
	{
		if(Property == MP_EmissiveColor)
			return Expression->Compile(Compiler);
		else
			return Compiler->Constant(1.0f);
	}
};

/*-----------------------------------------------------------------------------
	FExpressionViewportClient
-----------------------------------------------------------------------------*/

class FExpressionViewportClient : public FEditorLevelViewportClient
{
public:
	FExpressionViewportClient( UMaterial* InMaterial, class WxMaterialEditor_Upper* InUpperWindow, class WxPropertyWindow* InLowerWindow );
	~FExpressionViewportClient();

	void RefreshExpressionPreviews();

	void DrawWindow(FRenderInterface* RI,UMaterialExpression* InExpression,FString InCaption,INT InX, INT InY, INT InWidth, INT InHeight,UBOOL InPadRight,UBOOL InHasErrors,UBOOL InSelected,UBOOL InShowX);
	void DrawExpression(FRenderInterface* RI,UMaterialExpression* Expression,UBOOL Selected);
	void DrawMaterialPreview(FChildViewport* Viewport,FRenderInterface* RI,UMaterial* InMaterial,UBOOL Selected);
	void DrawConnection(FRenderInterface* RI,INT InputX,INT InputY,FExpressionInput* Input,UBOOL Tab);
	void DrawLabel( FRenderInterface* RI, INT InX, INT InY, INT InWidth, FString InLabel, HHitProxy* HitProxy = NULL );
	void UpdateSelectionInfo();
	UBOOL HaveSelections();

	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);
	virtual void InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual void InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime);
	virtual EMouseCursor GetCursor(FChildViewport* Viewport,INT X,INT Y);

	virtual ULevel* GetLevel() { return NULL; }

	TArray<UObject*> SelectedObjects;	// The collection of expressions/materials th
	UBOOL bRotating, bZooming;			// Is the user rotating/zooming the thumbnail mesh?
	UObject* HitExpression;				// The expression/material that was last clicked
	FExpressionInput* HitInput;
	UMaterialExpression* HitOutputExpression;
	FExpressionOutput HitOutput;
	INT NewX, NewY;
	UMaterial* Material;
	TIndirectArray<FExpressionPreview> ExpressionPreviews;
	WxMaterialEditor_Upper* UpperWindow;
	WxPropertyWindow* LowerWindow;
	FIntPoint Origin2D;			// The scroll position
	FLOAT ZoomPct;				// Zoom level for the preview static mesh
	UBOOL bShowBackground;
};

/*-----------------------------------------------------------------------------
	WxMaterialEditor_Upper
-----------------------------------------------------------------------------*/

class WxMaterialEditor_Upper : public wxWindow
{
public:
	WxMaterialEditor_Upper( wxWindow* InParent, class WxMaterialEditor* InMaterialEditor, wxWindowID InID, UMaterial* InMaterial, WxPropertyWindow* InLowerWindow );
	~WxMaterialEditor_Upper();

	void OnSize( wxSizeEvent& In );
	void OnContextNewShaderExpression( wxCommandEvent& In );
	void OnUseCurrenTexture( wxCommandEvent& In );

	UMaterial* Material;
	EThumbnailPrimType PrimType;
	FExpressionViewportClient* ExpressionViewportClient;
	WxMaterialEditor* MaterialEditor;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMaterialEditor
-----------------------------------------------------------------------------*/

class WxMaterialEditor : public wxFrame, FViewportClient, FResourceClient
{
public:
	WxMaterialEditor( wxWindow* InParent, wxWindowID InID, UMaterial* InMaterial );
	~WxMaterialEditor();

	virtual void FreeResource(FResource* Resource);
	virtual void UpdateResource(FResource* Resource);
	virtual void RequestMips( FTextureBase* Texture, UINT RequestedMips, FTextureMipRequest* TextureMipRequest );
	virtual void FinalizeMipRequest( FTextureMipRequest* TextureMipRequest );


	void OnSize( wxSizeEvent& In );
	void OnShowBackground( wxCommandEvent& In );
	void OnPrimTypePlane( wxCommandEvent& In );
	void OnPrimTypeCylinder( wxCommandEvent& In );
	void OnPrimTypeCube( wxCommandEvent& In );
	void OnPrimTypeSphere( wxCommandEvent& In );
	void OnCameraHome( wxCommandEvent& In );
	void OnRealTime( wxCommandEvent& In );
	void UI_ShowBackground( wxUpdateUIEvent& In );
	void UI_PrimTypePlane( wxUpdateUIEvent& In );
	void UI_PrimTypeCylinder( wxUpdateUIEvent& In );
	void UI_PrimTypeCube( wxUpdateUIEvent& In );
	void UI_PrimTypeSphere( wxUpdateUIEvent& In );
	void UI_RealTime( wxUpdateUIEvent& In );

	UMaterial* Material;
	TArray<FMaterialError> Errors;
	class WxMaterialEditorToolBar* ToolBar;
	wxSplitterWindow* SplitterWnd;
	WxMaterialEditor_Upper* UpperWindow;
	WxPropertyWindow* LowerWindow;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
