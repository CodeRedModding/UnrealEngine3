/*=============================================================================
	UnLinkedObjHitProxy.h
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#define LOC_INPUT		(0)
#define LOC_OUTPUT		(1)
#define LOC_VARIABLE	(2)
#define LOC_EVENT		(3)

#define LO_CAPTION_HEIGHT		(22)
#define LO_CONNECTOR_WIDTH		(8)
#define LO_CONNECTOR_LENGTH		(10)
#define LO_DESC_X_PADDING		(8)
#define LO_DESC_Y_PADDING		(8)
#define LO_TEXT_BORDER			(3)
#define LO_MIN_SHAPE_SIZE		(64)

//
//	HLinkedObjProxy
//
struct HLinkedObjProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HLinkedObjProxy,HHitProxy);

	UObject*	Obj;

	HLinkedObjProxy(UObject* InObj):
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
		Obj(InObj),
		SpecialIndex(InSpecialIndex)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Obj;
	}
};

//
//	HLinkedObjConnectorProxy
//
struct HLinkedObjConnectorProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HLinkedObjConnectorProxy,HHitProxy);

	UObject* 				ConnObj;
	INT						ConnType;
	INT						ConnIndex;

	HLinkedObjConnectorProxy(UObject* InObj, INT InConnType, INT InConnIndex):
		ConnObj(InObj),
		ConnType(InConnType),
		ConnIndex(InConnIndex)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << ConnObj;
	}
};

struct FLinkedObjConnInfo
{
	const TCHAR*	Name;
	FColor			Color;

	FLinkedObjConnInfo(const TCHAR* InName, const FColor& InColor)
	{
		Name = InName;
		Color = InColor;
	}
};

struct FLinkedObjDrawInfo
{
	TArray<FLinkedObjConnInfo>	Inputs;
	TArray<FLinkedObjConnInfo>	Outputs;
	TArray<FLinkedObjConnInfo>	Variables;
	TArray<FLinkedObjConnInfo>	Events;

	UObject*		ObjObject;

	// Outputs - so you can remember where connectors are for drawing links
	TArray<INT>		InputY;
	TArray<INT>		OutputY;
	TArray<INT>		VariableX;
	TArray<INT>		EventX;
	INT				DrawWidth;
	INT				DrawHeight;

	FLinkedObjDrawInfo()
	{
		ObjObject = NULL;
	}
};

class FLinkedObjDrawUtils
{
public:
	static void DrawNGon(FRenderInterface* RI, FIntPoint Center, FColor Color, INT NumSides, INT Radius);
	static void DrawSpline(FRenderInterface* RI, const FIntPoint& Start, const FVector2D& StartDir, const FIntPoint& End, const FVector2D& EndDir, const FColor& LineColor, UBOOL bArrowhead);
	static void DrawArrowhead(FRenderInterface* RI, const FIntPoint& Pos, const FVector2D& Dir, const FColor& Color);

	static FIntPoint GetTitleBarSize(FRenderInterface* RI, const TCHAR* Name);
	static void DrawTitleBar(FRenderInterface* RI, const FIntPoint& Pos, const FIntPoint& Size, const FColor& BorderColor, const FColor& BkgColor, const TCHAR* Name, const TCHAR* Comment=NULL);

	static FIntPoint GetLogicConnectorsSize(FRenderInterface* RI, const FLinkedObjDrawInfo& ObjInfo, INT* InputY=NULL, INT* OutputY=NULL);
	static void DrawLogicConnectors(FRenderInterface* RI, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size);

	static FIntPoint GetVariableConnectorsSize(FRenderInterface* RI, const FLinkedObjDrawInfo& ObjInfo);
	static void DrawVariableConnectors(FRenderInterface* RI, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size);

	static void DrawLinkedObj(FRenderInterface* RI, FLinkedObjDrawInfo& ObjInfo, const TCHAR* Name, const TCHAR* Comment, const FColor& BorderColor, const FColor& TitleBkgColor, const FIntPoint& Pos);
};