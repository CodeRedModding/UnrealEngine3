/*================================================================================
	LandscapeEdMode.h: Landscape editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/


#ifndef __LandscapeEdMode_h__
#define __LandscapeEdMode_h__

#ifdef _MSC_VER
	#pragma once
#endif
#include "UnEdViewport.h"

class FLandscapeBrush
{
public:
	enum EBrushType
	{
		BT_Normal = 0,
		BT_Alpha,
		BT_Component,
		BT_Gizmo
	};
	virtual FLOAT GetBrushExtent() = 0;
	virtual void MouseMove( FLOAT LandscapeX, FLOAT LandscapeY ) = 0;
	virtual UBOOL ApplyBrush( TMap<QWORD, FLOAT>& OutBrush, INT& OutX1, INT& OutY1, INT& OutX2, INT& OutY2 ) =0;
	virtual UBOOL InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FName InKey, EInputEvent InEvent ) { return FALSE; }
	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime) {};
	virtual void BeginStroke(FLOAT LandscapeX, FLOAT LandscapeY, class FLandscapeTool* CurrentTool);
	virtual void EndStroke();
	virtual void EnterBrush() {}
	virtual void LeaveBrush() {}
	virtual ~FLandscapeBrush() {}
	virtual UMaterialInterface* GetBrushMaterial() { return NULL; }
	virtual const TCHAR* GetIconString() = 0;
	virtual FString GetTooltipString() = 0;
	virtual EBrushType GetBrushType() { return BT_Normal; }
};

struct FLandscapeBrushSet
{
	FLandscapeBrushSet(const TCHAR* InBrushSetName, const TCHAR* InToolTip)
	:	BrushSetName(InBrushSetName)
	,	ToolTip(InToolTip)
	{}

	TArray<FLandscapeBrush*> Brushes;
	FString BrushSetName;
	FString ToolTip;
};

enum ELandscapeToolTargetType
{
	LET_Heightmap	= 0,
	LET_Weightmap	= 1,
	LET_Visibility	= 2,
};

namespace ELandscapeConvertMode
{
	enum Type
	{
		Invalid = -1,
		Expand = 0,
		Clip,
	};
}

namespace ELandscapeToolNoiseMode
{
	enum Type
	{
		Invalid = -1,
		Both = 0,
		Add = 1,
		Sub = 2,
	};

	inline FLOAT Conversion(Type Mode, FLOAT NoiseAmount, FLOAT OriginalValue)
	{
		switch( Mode )
		{
		case Add: // always +
			OriginalValue += NoiseAmount;
			break;
		case Sub: // always -
			OriginalValue -= NoiseAmount;
			break;
		case Both:
			break;
		}
		return OriginalValue;
	}
}

struct FLandscapeToolTarget
{
	ULandscapeInfo* LandscapeInfo;
	ELandscapeToolTargetType TargetType;
	FName LayerName;
	
	FLandscapeToolTarget()
	:	LandscapeInfo(NULL)
	,	TargetType(LET_Heightmap)
	,	LayerName(NAME_None)
	{}
};

/**
 * FLandscapeTool
 */
class FLandscapeTool
{
public:
	enum EToolType
	{
		TT_Normal = 0,
		TT_Mask,
	};
	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target) = 0;
	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& Target, FLOAT InHitX, FLOAT InHitY ) = 0;
	virtual void EndTool() = 0;
	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime) {};
	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y ) = 0;
	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY ) = 0;
	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient ) = 0;
	virtual UBOOL InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FName InKey, EInputEvent InEvent ) { return FALSE; }
	virtual ~FLandscapeTool() {}
	virtual const TCHAR* GetIconString() = 0;
	virtual FString GetTooltipString() = 0;
	virtual void SetEditRenderType();
	virtual UBOOL GetMaskEnable() { return TRUE; }
	// Functions which doesn't need Viewport data...
	virtual void Process(INT Index, INT Arg) {}
	virtual EToolType GetToolType() { return TT_Normal; }
};

class FLandscapeToolSet
{
	TArray<FLandscapeTool*> Tools;
	FLandscapeTool*			CurrentTool;
	FString					ToolSetName;
public:
	INT						PreviousBrushIndex;

	FLandscapeToolSet(const TCHAR* InToolSetName)
	:	ToolSetName(InToolSetName),
		CurrentTool(NULL),
		PreviousBrushIndex(0)
	{		
	}

	virtual ~FLandscapeToolSet()
	{
		for( INT ToolIdx=0;ToolIdx<Tools.Num();ToolIdx++ )
		{
			delete Tools(ToolIdx);
		}
	}

	virtual const TCHAR* GetIconString() { return Tools(0)->GetIconString(); }
	virtual FString GetTooltipString() { return Tools(0)->GetTooltipString(); }

	void AddTool(FLandscapeTool* InTool)
	{
		Tools.AddItem(InTool);
	}

	UBOOL SetToolForTarget( const FLandscapeToolTarget& Target )
	{
		for( INT ToolIdx=0;ToolIdx<Tools.Num();ToolIdx++ )
		{
			if( Tools(ToolIdx)->IsValidForTarget(Target) )
			{
				CurrentTool = Tools(ToolIdx);
				return TRUE;
			}
		}

		return FALSE;
	}

	FLandscapeTool* GetTool()
	{
		return CurrentTool;
	}

	const TCHAR* GetToolSetName()
	{
		return *ToolSetName;
	}

	const FString& GetToolSetNameString()
	{
		return ToolSetName;
	}
};

struct FLandscapeTargetListInfo
{
	FString TargetName;
	ELandscapeToolTargetType TargetType;
	FLandscapeLayerStruct* LayerInfo;		// null for heightmap
	UBOOL bSelected;
	ULandscapeInfo* LandscapeInfo;			
	
	FLandscapeTargetListInfo(const TCHAR* InTargetName, ELandscapeToolTargetType InTargetType, FLandscapeLayerStruct* InLayerInfo = NULL, UBOOL InbSelected = FALSE, ULandscapeInfo* InLandscapeInfo = NULL)
	:	TargetName(InTargetName)
	,	TargetType(InTargetType)
	,	LayerInfo(InLayerInfo)
	,	bSelected(InbSelected)
	,	LandscapeInfo(InLandscapeInfo)
	{}
};

struct FLandscapeListInfo
{
	FString LandscapeName;
	FGuid Guid;
	ULandscapeInfo* Info;		// null for heightmap
	INT ComponentQuads;
	INT NumSubsections;
	INT Width;
	INT Height;

	FLandscapeListInfo(const TCHAR* InName, FGuid InGuid, ULandscapeInfo* InInfo, INT InComponentQuads, INT InNumSubsections, INT InWidth, INT InHeight)
		:	LandscapeName(InName)
		,	Guid(InGuid)
		,	Info(InInfo)
		,	ComponentQuads(InComponentQuads)
		,	NumSubsections(InNumSubsections)
		,	Width(InWidth)
		,	Height(InHeight)
	{}
};

struct FGizmoHistory
{
	ALandscapeGizmoActor* Gizmo;
	FString GizmoName;

	FGizmoHistory(ALandscapeGizmoActor* InGizmo)
		: Gizmo(InGizmo)
	{
		GizmoName = Gizmo->GetPathName();
	}

	FGizmoHistory(ALandscapeGizmoActiveActor* InGizmo)
	{
		// handle for ALandscapeGizmoActiveActor -> ALandscapeGizmoActor
		// ALandscapeGizmoActor is only for history, so it has limited data
		Gizmo = InGizmo->SpawnGizmoActor();
		GizmoName = Gizmo->GetPathName();
	}
};

struct FGizmoImportLayer
{
	FString LayerFilename;
	FString LayerName;
	UBOOL bNoImport;

	FGizmoImportLayer()
		: bNoImport(FALSE)
	{
	}

	FGizmoImportLayer(const TCHAR* InLayerFilename, const TCHAR* InLayerName)
		: bNoImport(FALSE),
		LayerFilename(InLayerFilename),
		LayerName(InLayerName)
	{
	}

	FGizmoImportLayer(const TCHAR* InLayerFilename, const TCHAR* InLayerName, UBOOL InNoImport)
		: bNoImport(InNoImport),
		LayerFilename(InLayerFilename),
		LayerName(InLayerName)
	{
	}

};

struct FGizmoData
{
	BYTE DataType;
	TMap< QWORD, FGizmoSelectData > SelectedData;

	FGizmoData(ALandscapeGizmoActor* InGizmo)
	{
		DataType = LGT_None;
	}

	FGizmoData(ALandscapeGizmoActiveActor* InGizmo)
	{
		if (InGizmo)
		{
			DataType = InGizmo->DataType;
			SelectedData = InGizmo->SelectedData;
		}
	}
};

// Current user settings in Landscape UI
struct FLandscapeUISettings
{
	void Load();
	void Save();

	// Window
	void SetWindowSizePos(INT NewX, INT NewY, INT NewWidth, INT NewHeight) { WindowX = NewX; WindowY = NewY; WindowWidth = NewWidth; WindowHeight = NewHeight; }
	void GetWindowSizePos(INT& OutX, INT& OutY, INT& OutWidth, INT& OutHeight) { OutX = WindowX; OutY = WindowY; OutWidth = WindowWidth; OutHeight = WindowHeight; }

	// tool
	FLOAT GetToolStrength() { return Max<FLOAT>(ToolStrength, 0.f); }
	void SetToolStrength(FLOAT InToolStrength) { ToolStrength = InToolStrength; }

	FLOAT GetWeightTargetValue() { return Max<FLOAT>(WeightTargetValue, 0.f); }
	void SetWeightTargetValue(FLOAT InWeightTargetValue) { WeightTargetValue = InWeightTargetValue; }

	UBOOL GetbUseWeightTargetValue() { return bUseWeightTargetValue; }
	void SetbUseWeightTargetValue(UBOOL InbUseWeightTargetValue) { bUseWeightTargetValue = InbUseWeightTargetValue; }

	// Flatten
	ELandscapeToolNoiseMode::Type GetFlattenMode() { return FlattenMode; }
	void SetFlattenMode(ELandscapeToolNoiseMode::Type InFlattenMode) { FlattenMode = InFlattenMode; }
	UBOOL GetbUseSlopeFlatten() { return bUseSlopeFlatten; }
	void SetbUseSlopeFlatten(UBOOL InbUseSlopeFlatten) { bUseSlopeFlatten = InbUseSlopeFlatten; }
	UBOOL GetbPickValuePerApply() { return bPickValuePerApply; }
	void SetbPickValuePerApply(UBOOL InbPickValuePerApply) { bPickValuePerApply = InbPickValuePerApply; }

	// for Erosion Tool
	INT GetErodeThresh() { return Max<INT>(ErodeThresh,0); }
	void SetErodeThresh(FLOAT InErodeThresh) { ErodeThresh = InErodeThresh; }
	INT GetErodeIterationNum() { return Max<INT>(ErodeIterationNum,0); }
	void SetErodeIterationNum(FLOAT InErodeIterationNum) { ErodeIterationNum = InErodeIterationNum; }
	INT GetErodeSurfaceThickness() { return Max<INT>(ErodeSurfaceThickness,0); }
	void SetErodeSurfaceThickness(FLOAT InErodeSurfaceThickness) { ErodeThresh = InErodeSurfaceThickness; }
	ELandscapeToolNoiseMode::Type GetErosionNoiseMode() { return ErosionNoiseMode; }
	void SetErosionNoiseMode(ELandscapeToolNoiseMode::Type InErosionNoiseMode) { ErosionNoiseMode = InErosionNoiseMode; }
	FLOAT GetErosionNoiseScale() { return Max<FLOAT>(ErosionNoiseScale,0.f); }
	void SetErosionNoiseScale(FLOAT InErosionNoiseScale) { ErosionNoiseScale = InErosionNoiseScale; }

	// Hydra Erosion
	INT GetRainAmount() { return Max<INT>(RainAmount,0); }
	void SetRainAmount(INT InRainAmount) { RainAmount = InRainAmount; }
	FLOAT GetSedimentCapacity() { return Max<FLOAT>(SedimentCapacity,0.f); }
	void SetSedimentCapacity(FLOAT InSedimentCapacity) { SedimentCapacity = InSedimentCapacity; }
	INT GetHErodeIterationNum() { return Max<INT>(HErodeIterationNum,0); }
	void SetHErodeIterationNum(FLOAT InHErodeIterationNum) { HErodeIterationNum = InHErodeIterationNum; }
	ELandscapeToolNoiseMode::Type GetRainDistMode() { return RainDistMode; }
	void SetRainDistMode(ELandscapeToolNoiseMode::Type InRainDistMode) { RainDistMode = InRainDistMode; }
	FLOAT GetRainDistScale() { return Max<FLOAT>(RainDistScale,0.f); }
	void SetRainDistScale(FLOAT InRainDistScale) { RainDistScale = InRainDistScale; }
	FLOAT GetHErosionDetailScale() { return Max<FLOAT>(HErosionDetailScale,0.f); }
	void SetHErosionDetailScale(FLOAT InHErosionDetailScale) { HErosionDetailScale = InHErosionDetailScale; }
	UBOOL GetbHErosionDetailSmooth() { return bHErosionDetailSmooth; }
	void SetbHErosionDetailSmooth(FLOAT InbHErosionDetailSmooth) { bHErosionDetailSmooth = InbHErosionDetailSmooth; }

	// Noise Tool
	ELandscapeToolNoiseMode::Type GetNoiseMode() { return NoiseMode; }
	void SetNoiseMode(ELandscapeToolNoiseMode::Type InNoiseMode) { NoiseMode = InNoiseMode; }
	FLOAT GetNoiseScale() { return Max<FLOAT>(NoiseScale,0.f); }
	void SetNoiseScale(FLOAT InNoiseScale) { NoiseScale = InNoiseScale; }

	// Detail Preserving Smooth
	FLOAT GetDetailScale() { return Max<FLOAT>(DetailScale,0.f); }
	void SetDetailScale(FLOAT InDetailScale) { DetailScale = InDetailScale; }
	UBOOL GetbDetailSmooth() { return bDetailSmooth; }
	void SetbDetailSmooth(FLOAT InbDetailSmooth) { bDetailSmooth = InbDetailSmooth; }

	// Maximum Radius
	FLOAT GetMaximumValueRadius() { return Max<FLOAT>(MaximumValueRadius,0.f); }
	void SetMaximumValueRadius(FLOAT InMaximumValueRadius) { MaximumValueRadius = InMaximumValueRadius; }

	// Region
	UBOOL GetbUseSelectedRegion();
	void SetbUseSelectedRegion(UBOOL InbUseSelectedRegion);
	UBOOL GetbUseNegativeMask();
	void SetbUseNegativeMask(UBOOL InbUseNegativeMask);
	UBOOL GetbMaskEnabled() { return bMaskEnabled; }
	void SetbMaskEnabled(UBOOL InbMaskEnabled)	{ bMaskEnabled = InbMaskEnabled; }
	
	// Copy/Paste
	ELandscapeToolNoiseMode::Type GetPasteMode() { return PasteMode; }
	void SetPasteMode(ELandscapeToolNoiseMode::Type InPasteMode);

	// Convert Mode
	ELandscapeConvertMode::Type GetConvertMode() { return ConvertMode; }
	void SetConvertMode(ELandscapeConvertMode::Type InConvertMode) { ConvertMode = InConvertMode; }

	// brush
	FLOAT GetBrushRadius() { return Max<FLOAT>(BrushRadius,0.f); }
	void SetBrushRadius(FLOAT InBrushRadius) { BrushRadius = InBrushRadius; }
	INT GetBrushComponentSize() { return Max<INT>(BrushComponentSize,0); }
	void SetBrushComponentSize(INT InBrushComponentSize) { BrushComponentSize = InBrushComponentSize; }
	FLOAT GetBrushFalloff() { return Max<FLOAT>(BrushFalloff,0.f); }
	void SetBrushFalloff(FLOAT InBrushFalloff) { BrushFalloff = InBrushFalloff; }
	UBOOL GetbUseClayBrush() { return bUseClayBrush; }
	void SetbUseClayBrush(UBOOL InbUseClayBrush) { bUseClayBrush = InbUseClayBrush; }


	FLOAT GetAlphaBrushScale() { return Max<FLOAT>(AlphaBrushScale,0.f); }
	void SetAlphaBrushScale(FLOAT InAlphaBrushScale) { AlphaBrushScale = InAlphaBrushScale; }
	FLOAT GetAlphaBrushRotation() { return AlphaBrushRotation; }
	void SetAlphaBrushRotation(FLOAT InAlphaBrushRotation) { AlphaBrushRotation = InAlphaBrushRotation; }
	FLOAT GetAlphaBrushPanU() { return AlphaBrushPanU; }
	void SetAlphaBrushPanU(FLOAT InAlphaBrushPanU) { AlphaBrushPanU = InAlphaBrushPanU; }
	FLOAT GetAlphaBrushPanV() { return AlphaBrushPanV; }
	void SetAlphaBrushPanV(FLOAT InAlphaBrushPanV) { AlphaBrushPanV = InAlphaBrushPanV; }

	UBOOL SetAlphaTexture(const TCHAR* InTextureName, INT InTextureChannel);
	UTexture2D* GetAlphaTexture() { return AlphaTexture; }
	FString GetAlphaTextureName() { return AlphaTextureName; }
	INT GetAlphaTextureChannel() { return AlphaTextureChannel; }
	INT GetAlphaTextureSizeX() { return AlphaTextureSizeX; }
	INT GetAlphaTextureSizeY() { return AlphaTextureSizeY; }
	const BYTE* GetAlphaTextureData() { return &AlphaTextureData(0); }

	FLOAT GetbSmoothGizmoBrush() { return bSmoothGizmoBrush; }
	void SetbSmoothGizmoBrush(FLOAT InbSmoothGizmoBrush) { bSmoothGizmoBrush = InbSmoothGizmoBrush; }

	TArray<FGizmoHistory>& GetGizmoHistories() { return GizmoHistories; }
	TArray<FGizmoImportLayer>& GetGizmoImportLayers() { return GizmoImportLayers; }
	TArray<FGizmoData>& GetGizmoData() { return GizmoData; }

	UBOOL GetbApplyToAllTargets() { return bApplyToAllTargets; }
	void SetbApplyToAllTargets(FLOAT InbApplyToAllTargets) { bApplyToAllTargets = InbApplyToAllTargets; }

	FLOAT GetLODBiasThreshold() { return Max<FLOAT>(LODBiasThreshold,0.f); }
	void SetLODBiasThreshold(FLOAT InLODBiasThreshold) { LODBiasThreshold = InLODBiasThreshold; }

	FLandscapeUISettings()
	:	WindowX(-1)
	,	WindowY(-1)
	,	WindowWidth(284)
	,	WindowHeight(600)
	,	ToolStrength(0.3f)
	,	WeightTargetValue(1.f)
	,	bUseWeightTargetValue(FALSE)
	,	BrushRadius(2048.f)
	,	BrushComponentSize(1)
	,	BrushFalloff(0.5f)
	,	bUseClayBrush(FALSE)
	,	AlphaBrushScale(0.5f)
	,	AlphaBrushRotation(0.f)
	,	AlphaBrushPanU(0.5f)
	,	AlphaBrushPanV(0.5f)
	,	AlphaTexture(NULL)
	,	AlphaTextureName(TEXT("EditorLandscapeResources.DefaultAlphaTexture"))
	,	AlphaTextureChannel(0)
	,	AlphaTextureSizeX(1)
	,	AlphaTextureSizeY(1)
	,	bSmoothGizmoBrush(TRUE)
	,	FlattenMode(ELandscapeToolNoiseMode::Both)
	,	bUseSlopeFlatten(FALSE)
	,	bPickValuePerApply(FALSE)
	,	ErodeThresh(64)
	,	ErodeIterationNum(28)
	,	ErodeSurfaceThickness(256)
	,	ErosionNoiseMode(ELandscapeToolNoiseMode::Sub)
	,	ErosionNoiseScale(60.f)
	,	RainAmount(128)
	,	SedimentCapacity(0.3f)
	,	HErodeIterationNum(75)
	,	RainDistMode(ELandscapeToolNoiseMode::Both)
	,	RainDistScale(60.f)
	,	bHErosionDetailSmooth(TRUE)
	,	HErosionDetailScale(0.01f)
	,	NoiseMode(ELandscapeToolNoiseMode::Both)
	,	NoiseScale(128.f)
	,	DetailScale(0.3f)
	,	bDetailSmooth(TRUE)
	,	PasteMode(ELandscapeToolNoiseMode::Both)	
	,	MaximumValueRadius(10000.f)
	,	bUseSelectedRegion(TRUE)
	,	bUseNegativeMask(TRUE)
	,	bMaskEnabled(FALSE)
	,	bApplyToAllTargets(TRUE)
	,	ConvertMode(ELandscapeConvertMode::Expand)
	,	LODBiasThreshold(0.5f)
	{
		AlphaTextureData.AddItem(255);
	}

	~FLandscapeUISettings()
	{
		if( AlphaTexture )
		{
			AlphaTexture->RemoveFromRoot();
		}
	}

private:
	INT WindowX;
	INT WindowY;
	INT WindowWidth;
	INT WindowHeight;

	FLOAT ToolStrength;
	FLOAT WeightTargetValue;
	UBOOL bUseWeightTargetValue;

	FLOAT BrushRadius;
	INT BrushComponentSize;
	FLOAT BrushFalloff;
	UBOOL bUseClayBrush;
	FLOAT AlphaBrushScale;
	FLOAT AlphaBrushRotation;
	FLOAT AlphaBrushPanU;
	FLOAT AlphaBrushPanV;

	UTexture2D* AlphaTexture;
	FString AlphaTextureName; 
	INT AlphaTextureChannel;
	INT AlphaTextureSizeX;
	INT AlphaTextureSizeY;
	TArray<BYTE> AlphaTextureData;

	UBOOL bSmoothGizmoBrush;

	// Flatten
	ELandscapeToolNoiseMode::Type FlattenMode;
	UBOOL bUseSlopeFlatten;
	UBOOL bPickValuePerApply;

	// for Erosion Tool
	INT ErodeThresh;
	INT ErodeIterationNum;
	INT ErodeSurfaceThickness;
	ELandscapeToolNoiseMode::Type ErosionNoiseMode;
	FLOAT ErosionNoiseScale;

	// Hydra Erosion
	INT RainAmount;
	FLOAT SedimentCapacity;
	INT HErodeIterationNum;
	ELandscapeToolNoiseMode::Type RainDistMode;
	FLOAT RainDistScale;
	UBOOL bHErosionDetailSmooth;
	FLOAT HErosionDetailScale;

	// Noise Tool
	ELandscapeToolNoiseMode::Type NoiseMode;
	FLOAT NoiseScale;

	// Frequency Smooth
	UBOOL bDetailSmooth;
	FLOAT DetailScale;

	// Copy/Paste
	ELandscapeToolNoiseMode::Type PasteMode;

	// Maximum Radius for Radius proportional value
	FLOAT MaximumValueRadius;

	// Region
	UBOOL bUseSelectedRegion;
	UBOOL bUseNegativeMask;
	UBOOL bMaskEnabled;
	UBOOL bApplyToAllTargets;

	// Convert Mode
	ELandscapeConvertMode::Type ConvertMode;

	// LOD Bias
	FLOAT LODBiasThreshold;

	TArray<FGizmoHistory> GizmoHistories;
	TArray<FGizmoImportLayer> GizmoImportLayers;
	TArray<FGizmoData> GizmoData;
};

/**
 * Landscape editor mode
 */
class FEdModeLandscape : public FEdMode
{
	// for Window Messages
	struct FDeferredWindowMessage
	{
		FDeferredWindowMessage(UINT Msg, WPARAM w, LPARAM l)
			: Message(Msg), wParam(w), lParam(l)
		{
		}
		UINT Message;
		WPARAM wParam;
		LPARAM lParam;
	};
	TArray<FDeferredWindowMessage> WindowMessages;
public:
	void AddWindowMessage(UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		WindowMessages.AddItem(FDeferredWindowMessage(Msg, wParam, lParam));
	}

	FLandscapeUISettings UISettings;

	FLandscapeToolSet* CurrentToolSet;
	FLandscapeBrush* CurrentBrush;
	FLandscapeToolTarget CurrentToolTarget;

	// GizmoBrush for Tick
	FLandscapeBrush* GizmoBrush;
	// UI setting for additional UI Tools
	INT CurrentToolIndex;
	// UI setting for additional UI Tools
	INT CurrentBrushIndex;
	// UI index for adding Landscape component...
	INT AddComponentToolIndex;
	// UI index for Move to Streaming Level for Landscape component...
	INT MoveToLevelToolIndex;

	ALandscapeGizmoActiveActor* CurrentGizmoActor;
	//
	FLandscapeToolSet* CopyPasteToolSet;
	void CopyDataToGizmo();
	void PasteDataFromGizmo();

	/** Constructor */
	FEdModeLandscape();

	/** Initialization */
	void InitializeBrushes();
	void InitializeTools();

	/** Destructor */
	virtual ~FEdModeLandscape();

	/** FSerializableObject: Serializer */
	virtual void Serialize( FArchive &Ar );

	/** FEdMode: Called when the mode is entered */
	virtual void Enter();

	/** FEdMode: Called when the mode is exited */
	virtual void Exit();

	virtual FVector GetWidgetLocation() const;
	/** FEdMode: Called when the mouse is moved over the viewport */
	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y );

	/**
	 * FEdMode: Called when the mouse is moved while a window input capture is in effect
	 *
	 * @param	InViewportClient	Level editor viewport client that captured the mouse input
	 * @param	InViewport			Viewport that captured the mouse input
	 * @param	InMouseX			New mouse cursor X coordinate
	 * @param	InMouseY			New mouse cursor Y coordinate
	 *
	 * @return	TRUE if input was handled
	 */
	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY );

	/** FEdMode: Called when a mouse button is pressed */
	virtual UBOOL StartTracking();

	/** FEdMode: Called when a mouse button is released */
	virtual UBOOL EndTracking();

	/** FEdMode: Called once per frame */
	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime);

	/** FEdMode: Called when a key is pressed */
	virtual UBOOL InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FName InKey, EInputEvent InEvent );

	/** FEdMode: Called when mouse drag input it applied */
	virtual UBOOL InputDelta( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale );

	/** FEdMode: Render elements for the landscape tool */
	virtual void Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI );

	/** FEdMode: Render HUD elements for this tool */
	virtual void DrawHUD( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas );

	// Handling SelectActor
	virtual UBOOL Select( AActor* InActor, UBOOL bInSelected );
	/** FEdMode: Called when the currently selected actor has changed */
	virtual void ActorSelectionChangeNotify();

	virtual void ActorMoveNotify();

	virtual UBOOL ProcessEditCut();
	virtual UBOOL ProcessEditCopy();
	virtual UBOOL ProcessEditPaste();

	/** FEdMode: If the EdMode is handling InputDelta (ie returning true from it), this allows a mode to indicated whether or not the Widget should also move. */
	virtual UBOOL AllowWidgetMove() { return FALSE; }

	/** FEdMode: Draw the transform widget while in this mode? */
	virtual UBOOL ShouldDrawWidget() const;

	/** FEdMode: Returns true if this mode uses the transform widget */
	virtual UBOOL UsesTransformWidget() const;

	virtual INT GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const;

	/** Forces real-time perspective viewports */
	void ForceRealTimeViewports( const UBOOL bEnable, const UBOOL bStoreCurrentState );

	/** Trace under the mouse cursor and return the landscape hit and the hit location (in landscape quad space) */
	UBOOL LandscapeMouseTrace( FEditorLevelViewportClient* ViewportClient, FLOAT& OutHitX, FLOAT& OutHitY );

	/** Trace under the specified coordinates and return the landscape hit and the hit location (in landscape quad space) */
	UBOOL LandscapeMouseTrace( FEditorLevelViewportClient* ViewportClient, INT MouseX, INT MouseY, FLOAT& OutHitX, FLOAT& OutHitY );

	//
	// Interaction  with WPF User Interface
	//

	/** Change current tool */
	void SetCurrentTool( FName ToolSetName );
	void SetCurrentTool( INT ToolIdx );
	void SetMaskEnable( UBOOL bMaskEnabled );

	TArray<FLandscapeTargetListInfo>* GetTargetList();
	TArray<FLandscapeListInfo>* GetLandscapeList();

	void AddLayerInfo(ULandscapeLayerInfoObject* LayerInfo);

	INT UpdateLandscapeList();
	void UpdateTargetList();

	TArray<FLandscapeToolSet*> LandscapeToolSets;
	TArray<FLandscapeBrushSet> LandscapeBrushSets;

	// For collision add visualization
	FLandscapeAddCollision* LandscapeRenderAddCollision;

private:

#if WITH_MANAGED_CODE
	/** Landscape palette window */
	TScopedPointer< class FLandscapeEditWindow > LandscapeEditWindow;
#endif

	TArray<FLandscapeTargetListInfo> LandscapeTargetList;
	TArray<FLandscapeListInfo> LandscapeList;

	UBOOL bToolActive;
	UMaterial* GizmoMaterial;
};

#endif	// __LandscapeEdMode_h__
