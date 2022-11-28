/*=============================================================================
	UnCanvas.h: Unreal canvas definition.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

/*
	UCanvas
	A high-level rendering interface used to render objects on the HUD.
*/

class UCanvas : public UObject
{
	DECLARE_CLASS(UCanvas,UObject,CLASS_Transient,Engine);
	NO_DEFAULT_CONSTRUCTOR(UCanvas);
public:

	// Variables.
	UFont*			Font;
	FLOAT			SpaceX, SpaceY;
	FLOAT			OrgX, OrgY;
	FLOAT			ClipX, ClipY;
	FLOAT			CurX, CurY;
	FLOAT			CurYL;
	FColor			DrawColor;
	BITFIELD		bCenter:1;
	BITFIELD		bNoSmooth:1;
	INT				SizeX, SizeY;

    FRenderInterface*	RenderInterface;
	FSceneView*			SceneView;

	FPlane			ColorModulate;
	UTexture2D*		DefaultTexture;

	// UCanvas interface.
	void Init();
	void Update();
	void DrawTile( UTexture2D* Tex, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, const FLinearColor& Color );
	void DrawMaterialTile( UMaterialInstance* Tex, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL );
	void DrawIcon( UTexture2D* Tex, FLOAT ScreenX, FLOAT ScreenY, FLOAT XSize, FLOAT YSize, const FLinearColor& Color );
	void DrawPattern( UTexture2D* Tex, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT Scale, FLOAT OrgX, FLOAT OrgY, const FLinearColor& Color );
	void VARARGS WrappedStrLenf( UFont* Font, INT& XL, INT& YL, const TCHAR* Fmt, ... );
	void VARARGS WrappedPrintf( UFont* Font, UBOOL Center, const TCHAR* Fmt, ... );

	void VARARGS WrappedStrLenf( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, INT& XL, INT& YL, const TCHAR* Fmt, ... );
	void VARARGS WrappedPrintf( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Fmt, ... );
	
	void WrappedPrint( UBOOL Draw, INT& XL, INT& YL, UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Text ); 

	void WrapStringToArray( const TCHAR* Text, TArray<FString> *OutArray, float Width, UFont *Font = NULL, const TCHAR EOL='\n' );
	static void ClippedStrLen( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, INT& XL, INT& YL, const TCHAR* Text );
	void ClippedPrint( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Text );

	void DrawTileStretched(UTexture2D* Tex, FLOAT Left, FLOAT Top, FLOAT AWidth, FLOAT AHeight);
	void DrawTileScaled(UTexture2D* Tex, FLOAT Left, FLOAT Top, FLOAT NewXScale, FLOAT NewYScale);
	void DrawTileBound(UTexture2D* Tex, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height);
	void DrawTileJustified(UTexture2D* Tex, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height, BYTE Justification);
	void DrawTileScaleBound(UTexture2D* Tex, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height);
	void VARARGS DrawTextJustified(BYTE Justification, FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, const TCHAR* Fmt, ... );
	
	void SetClip( INT X, INT Y, INT XL, INT YL );

	// Natives.
	DECLARE_FUNCTION(execStrLen)
	DECLARE_FUNCTION(execDrawText)
	DECLARE_FUNCTION(execDrawTile)
	DECLARE_FUNCTION(execDrawMaterialTile)
	DECLARE_FUNCTION(execDrawTileClipped)
	DECLARE_FUNCTION(execDrawTextClipped)
	DECLARE_FUNCTION(execTextSize)
	DECLARE_FUNCTION(execWrapStringToArray) // mc

	// jmw

	DECLARE_FUNCTION(execDrawTileStretched);
	DECLARE_FUNCTION(execDrawTileJustified);
	DECLARE_FUNCTION(execDrawTileScaled);
	DECLARE_FUNCTION(execDrawTextJustified);
	DECLARE_FUNCTION(execProject);
	DECLARE_FUNCTION(execDeProject);

    void eventReset()
    {
        ProcessEvent(FindFunctionChecked(TEXT("Reset")),NULL);
    }
};

