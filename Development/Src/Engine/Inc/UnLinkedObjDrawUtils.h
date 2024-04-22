/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNLINKEDOBJDRAWUTILS_H__
#define __UNLINKEDOBJDRAWUTILS_H__

#define LO_CAPTION_HEIGHT		(22)
#define LO_CONNECTOR_WIDTH		(8)
#define LO_CONNECTOR_LENGTH		(10)
#define LO_EXTRA_HIT_X          (4)
#define LO_EXTRA_HIT_Y          (4)
#define LO_EXTRA_HIT_WIDTH      (LO_EXTRA_HIT_X + LO_EXTRA_HIT_X)
#define LO_EXTRA_HIT_LENGTH     (LO_EXTRA_HIT_Y + LO_EXTRA_HIT_Y)
#define LO_DESC_X_PADDING		(8)
#define LO_DESC_Y_PADDING		(8)
#define LO_TEXT_BORDER			(3)
#define LO_MIN_SHAPE_SIZE		(64)

#define LO_SLIDER_HANDLE_WIDTH	(7)
#define LO_SLIDER_HANDLE_HEIGHT	(15)

//
//	HLinkedObjProxy
//
struct HLinkedObjProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HLinkedObjProxy,HHitProxy);

	UObject*	Obj;

	HLinkedObjProxy(UObject* InObj):
		HHitProxy(HPP_UI),
		Obj(InObj)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Obj;
	}
};

//
//	HLinkedObjProxySpecial
//
struct HLinkedObjProxySpecial : public HHitProxy
{
	DECLARE_HIT_PROXY(HLinkedObjProxySpecial,HHitProxy);

	UObject*	Obj;
	INT			SpecialIndex;

	HLinkedObjProxySpecial(UObject* InObj, INT InSpecialIndex):
		HHitProxy(HPP_UI),
		Obj(InObj),
		SpecialIndex(InSpecialIndex)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Obj;
	}
};

/** Determines the type of connector a HLinkedObjConnectorProxy represents */
enum EConnectorHitProxyType
{
	/** an input connector */
	LOC_INPUT,

	/** output connector */
	LOC_OUTPUT,

	/** variable connector */
	LOC_VARIABLE,

	/** event connector */
	LOC_EVENT,
};

/**
 * In a linked object drawing, represents a connector for an object's link
 */
struct FLinkedObjectConnector
{
	/** the object that this proxy's connector belongs to */
	UObject* 				ConnObj;

	/** the type of connector this proxy is attached to */
	EConnectorHitProxyType	ConnType;

	/** the link index for this proxy's connector */
	INT						ConnIndex;

	/** Constructor */
	FLinkedObjectConnector(UObject* InObj, EConnectorHitProxyType InConnType, INT InConnIndex):
		ConnObj(InObj), ConnType(InConnType), ConnIndex(InConnIndex)
	{}

	void Serialize( FArchive& Ar )
	{
		Ar << ConnObj;
	}
};

/**
 * Abstract interface to allow editor to provide callbacks
 */
class FLinkedObjectDrawHelper
{
public:
	/** virtual destructor to allow proper deallocation*/
	virtual ~FLinkedObjectDrawHelper(void) {};
	/** Callback for rendering preview meshes */
	virtual void DrawThumbnail (UObject* PreviewObject,  TArray<UMaterialInterface*>& InMaterialOverrides, FViewport* Viewport, FCanvas* Canvas, const FIntRect& InRect) = 0;
};

/**
 * Hit proxy for link connectors in a linked object drawing.
 */
struct HLinkedObjConnectorProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HLinkedObjConnectorProxy,HHitProxy);

	FLinkedObjectConnector Connector;

	HLinkedObjConnectorProxy(UObject* InObj, EConnectorHitProxyType InConnType, INT InConnIndex):
		HHitProxy(HPP_UI),
		Connector(InObj, InConnType, InConnIndex)
	{}

	virtual void Serialize(FArchive& Ar)
	{
		Connector.Serialize(Ar);
	}
};

/**
 * Hit proxy for a line connection between two objects.
 */
struct HLinkedObjLineProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HLinkedObjLineProxy,HHitProxy);

	FLinkedObjectConnector Src, Dest;

	HLinkedObjLineProxy(UObject *SrcObj, INT SrcIdx, UObject *DestObj, INT DestIdx) :
		HHitProxy(HPP_UI),
		Src(SrcObj,LOC_OUTPUT,SrcIdx),
		Dest(DestObj,LOC_INPUT,DestIdx)
	{}

	virtual void Serialize(FArchive& Ar)
	{
		Src.Serialize(Ar);
		Dest.Serialize(Ar);
	}
};

struct FLinkedObjConnInfo
{
	// The name of the connector
	FString			Name;
	// Tooltip strings for this connector
	TArray<FString>	ToolTips;
	// The color of the connector
	FColor			Color;
	// Whether or not this connector is an output
	UBOOL			bOutput;
	// Whether or not this connector is moving
	UBOOL			bMoving;
	// Whether or not this connector can move left
	UBOOL			bClampedMax;
	// Whether or not this connector can move right
	UBOOL			bClampedMin;
	// Whether or not this connector is new since the last draw
	UBOOL			bNewConnection;
	// The delta that should be applied to this connectors initial position
	INT				OverrideDelta;
	// Enabled will render as the default text color with the connector in the chosen color
	// Disabled will gray the name and connector - overriding the chosen color
	UBOOL			bEnabled;

	FLinkedObjConnInfo( const TCHAR* InName, const FColor& InColor, UBOOL InMoving = FALSE, UBOOL InNewConnection = TRUE, INT InOverrideDelta = 0, const UBOOL InbOutput = 0, UBOOL InEnabled = TRUE )
	{
		Name = InName;
		Color = InColor;
		bOutput = InbOutput;
		bMoving = InMoving;
		OverrideDelta = InOverrideDelta;
		bNewConnection = InNewConnection;
		bClampedMax = FALSE;
		bClampedMin = FALSE;
		bEnabled = InEnabled;
	}
};

struct FLinkedObjDrawInfo
{
	// Lists of different types of connectors
	TArray<FLinkedObjConnInfo>	Inputs;
	TArray<FLinkedObjConnInfo>	Outputs;
	TArray<FLinkedObjConnInfo>	Variables;
	TArray<FLinkedObjConnInfo>	Events;

	// Pointer to the sequence object that used this struct
	UObject*		ObjObject;
	
	// If we have a pending variable connector recalc
	UBOOL			bPendingVarConnectorRecalc;
	// If we have a pending input connector recalc
	UBOOL			bPendingInputConnectorRecalc;
	// if we have a pending output connector recalc
	UBOOL			bPendingOutputConnectorRecalc;

	/** Defaults to not having any visualization unless otherwise set*/
	FIntPoint		VisualizationSize;

	// Outputs - so you can remember where connectors are for drawing links
	TArray<INT>			InputY;
	TArray<INT>			OutputY;
	TArray<INT>			VariableX;
	TArray<INT>			EventX;
	INT				DrawWidth;
	INT				DrawHeight;

	/** Where the resulting visualization should be drawn*/
	FIntPoint		VisualizationPosition;

	/** Tool tip strings for this node. */
	TArray<FString> ToolTips;

	FLinkedObjDrawInfo()
	{
		ObjObject = NULL;
			
		bPendingVarConnectorRecalc = FALSE;
		bPendingInputConnectorRecalc = FALSE;
		bPendingOutputConnectorRecalc = FALSE;

		VisualizationSize = FIntPoint::ZeroValue();
	}
};

/**
 * A collection of static drawing functions commonly used by linked object editors.
 */
class FLinkedObjDrawUtils
{
public:
	/** Set the font used by Canvas-based editors */
	static void InitFonts( UFont* InNormalFont );
	/** Get the font used by Canvas-based editor */
	static UFont* GetFont();
	static void DrawNGon(FCanvas* Canvas, const FVector2D& Center, const FColor& Color, INT NumSides, FLOAT Radius);
	static void DrawSpline(FCanvas* Canvas, const FIntPoint& Start, const FVector2D& StartDir, const FIntPoint& End, const FVector2D& EndDir, const FColor& LineColor, UBOOL bArrowhead, UBOOL bInterpolateArrowPositon=FALSE);
	static void DrawArrowhead(FCanvas* Canvas, const FIntPoint& Pos, const FVector2D& Dir, const FColor& Color);

	static FIntPoint GetTitleBarSize(FCanvas* Canvas, const TCHAR* Name);
	static FIntPoint GetCommentBarSize(FCanvas* Canvas, const TCHAR* Comment);
	static INT DrawTitleBar(FCanvas* Canvas, const FIntPoint& Pos, const FIntPoint& Size, const FColor& FontColor, const FColor& BorderColor, const FColor& BkgColor, const TCHAR* Name, const TArray<FString>& Comments, INT BorderWidth = 0);
	static INT DrawComments(FCanvas* Canvas, const FIntPoint& Pos, const FIntPoint& Size, const TArray<FString>& Comments, UFont* Font);

	static FIntPoint GetLogicConnectorsSize(const FLinkedObjDrawInfo& ObjInfo, INT* InputY=NULL, INT* OutputY=NULL);
	
	/** 
	 * Draws logic connectors on the sequence object with adjustments for moving connectors
	 * 
	 * @param Canvas	The canvas to draw on
	 * @param ObjInfo	Information about the sequence object so we know how to draw the connectors
	 * @param Pos		The positon of the sequence object
	 * @param Size		The size of the sequence object
	 * @param ConnectorTileBackgroundColor		(Optional)Color to apply to a connector tiles bacground
 	 * @param bHaveMovingInput			(Optional)Whether or not we have a moving input connector
	 * @param bHaveMovingOutput			(Optional)Whether or not we have a moving output connector
	 * @param bGhostNonMoving			(Optional)Whether or not we should ghost all non moving connectors while one is moving
	 */
	static void DrawLogicConnectorsWithMoving(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size, const FLinearColor* ConnectorTileBackgroundColor=NULL, const UBOOL bHaveMovingInput = FALSE, const UBOOL bHaveMovingOutput = FALSE, const UBOOL bGhostNonMoving = FALSE );

	static FIntPoint GetVariableConnectorsSize(FCanvas* Canvas, const FLinkedObjDrawInfo& ObjInfo);
	
	/** 
	 * Draws variable connectors on the sequence object with adjustments for moving connectors
	 * 
	 * @param Canvas	The canvas to draw on
	 * @param ObjInfo	Information about the sequence object so we know how to draw the connectors
	 * @param Pos		The positon of the sequence object
	 * @param Size		The size of the sequence object
	 * @param VarWidth	The width of space we have to draw connectors
 	 * @param bHaveMovingVariable			(Optional)Whether or not we have a moving variable connector
	 * @param bGhostNonMoving			(Optional)Whether or not we should ghost all non moving connectors while one is moving
	 */
	static void DrawVariableConnectorsWithMoving(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size, const INT VarWidth, UBOOL bHaveMovingVariable = FALSE, const UBOOL bGhostNonMoving = FALSE );

	static void DrawLogicConnectors(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size, const FLinearColor* ConnectorTileBackgroundColor=NULL);
	static void DrawVariableConnectors(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size, const INT VarWidth);
	
	/** Draws connection and node tooltips. */
	static void DrawToolTips(FCanvas* Canvas, const FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size);

	static void DrawLinkedObj(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const TCHAR* Name, const TCHAR* Comment, const FColor& FontColor, const FColor& BorderColor, const FColor& TitleBkgColor, const FIntPoint& Pos);

	static INT ComputeSliderHeight(INT SliderWidth);
	static INT Compute2DSliderHeight(INT SliderWidth);
	// returns height of drawn slider
	static INT DrawSlider( FCanvas* Canvas, const FIntPoint& SliderPos, INT SliderWidth, const FColor& BorderColor, const FColor& BackGroundColor, FLOAT SliderPosition, const FString& ValText, UObject* Obj, int SliderIndex=0, UBOOL bDrawTextOnSide=FALSE);
	// returns height of drawn slider
	static INT Draw2DSlider(FCanvas* Canvas, const FIntPoint &SliderPos, INT SliderWidth, const FColor& BorderColor, const FColor& BackGroundColor, FLOAT SliderPositionX, FLOAT SliderPositionY, const FString &ValText, UObject *Obj, int SliderIndex, UBOOL bDrawTextOnSide);

	/**
	 * @return		TRUE if the current viewport contains some portion of the specified AABB.
	 */
	static UBOOL AABBLiesWithinViewport(FCanvas* Canvas, FLOAT X, FLOAT Y, FLOAT SizeX, FLOAT SizeY);

	/**
	 * Convenience function for filtering calls to FRenderInterface::DrawTile via AABBLiesWithinViewport.
	 */
	static void DrawTile(FCanvas* Canvas,FLOAT X,FLOAT Y,FLOAT SizeX,FLOAT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,const FLinearColor& Color,FTexture* Texture = NULL,UBOOL AlphaBlend = 1);

	/**
	 * Convenience function for filtering calls to FRenderInterface::DrawTile via AABBLiesWithinViewport. Additional flag to control the freezing of time
	 */
	static void DrawTile(FCanvas* Canvas, FLOAT X,FLOAT Y,FLOAT SizeX,FLOAT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,FMaterialRenderProxy* MaterialRenderProxy,UBOOL bFreezeTime = false);

	/**
	 * Convenience function for filtering calls to FRenderInterface::DrawString through culling heuristics.
	 */
	static INT DrawString(FCanvas* Canvas,FLOAT StartX,FLOAT StartY,const TCHAR* Text,class UFont* Font,const FLinearColor& Color);

	/**
	 * Convenience function for filtering calls to FRenderInterface::DrawShadowedString through culling heuristics.
	 */
	static INT DrawShadowedString(FCanvas* Canvas,FLOAT StartX,FLOAT StartY,const TCHAR* Text,class UFont* Font,const FLinearColor& Color);

	/**
	 * Convenience function for filtering calls to FRenderInterface::DrawStringWrapped through culling heuristics.
	 */
	static INT DisplayComment( FCanvas* Canvas, UBOOL Draw, FLOAT CurX, FLOAT CurY, FLOAT Z, FLOAT& XL, FLOAT& YL, UFont* Font, const TCHAR* Text, FLinearColor DrawColor );

	/**
	 * Takes a transformation matrix and returns a uniform scaling factor based on the 
	 * length of the rotation vectors.
	 *
	 * @param Matrix	A matrix to use to calculate a uniform scaling parameter.
	 *
	 * @return A uniform scaling factor based on the matrix passed in.
	 */
	static FLOAT GetUniformScaleFromMatrix(const FMatrix &Matrix);

private:
	/** Font to use in Canvas-based editors */
	static UFont* NormalFont;
};

#endif // __UNLINKEDOBJDRAWUTILS_H__
