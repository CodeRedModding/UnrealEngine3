/*=============================================================================
	DebugRenderSceneProxy.h: Useful scene proxy for rendering non performance-critical information.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

=============================================================================*/

#ifndef _INC_DEBUGRENDERSCENEPROXY
#define _INC_DEBUGRENDERSCENEPROXY

class FDebugRenderSceneProxy : public FPrimitiveSceneProxy
{
public:

	FDebugRenderSceneProxy(const UPrimitiveComponent* InComponent):
	  FPrimitiveSceneProxy(InComponent)
	  {}

	  // FPrimitiveSceneProxy interface.
	  
	  /** 
	  * Draw the scene proxy as a dynamic element
	  *
	  * @param	PDI - draw interface to render to
	  * @param	View - current view
	  * @param	DPGIndex - current depth priority 
	  * @param	Flags - optional set of flags from EDrawDynamicElementFlags
	  */
	  virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	  /**
	  * Draws a line with an arrow at the end.
	  *
	  * @param Start		Starting point of the line.
	  * @param End		Ending point of the line.
	  * @param Color		Color of the line.
	  * @param Mag		Size of the arrow.
	  */
	  void DrawLineArrow(FPrimitiveDrawInterface* PDI,const FVector &Start,const FVector &End,const FColor &Color,FLOAT Mag);

	  virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	  DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + Cylinders.GetAllocatedSize() + ArrowLines.GetAllocatedSize() + Stars.GetAllocatedSize() + DashedLines.GetAllocatedSize() + Lines.GetAllocatedSize() + WireBoxes.GetAllocatedSize() ); }

	  /** Struct to hold info about lines to render. */
	struct FDebugLine
	{
		FDebugLine(const FVector &InStart, const FVector &InEnd, const FColor &InColor) : 
		Start(InStart),
		End(InEnd),
		Color(InColor) {}

		FVector Start;
		FVector End;
		FColor Color;
	};

	/** Struct to hold info about boxes to render. */
	struct FDebugBox
	{
		FDebugBox( const FBox& InBox, const FColor& InColor )
			: Box( InBox ), Color( InColor )
		{
		}

		FBox Box;
		FColor Color;
	};

	/** Struct to hold info about cylinders to render. */
	struct FWireCylinder
	{
		FWireCylinder(const FVector &InBase, const FLOAT InRadius, const FLOAT InHalfHeight, const FColor &InColor) :
		Base(InBase),
		Radius(InRadius),
		HalfHeight(InHalfHeight),
		Color(InColor) {}

		FVector Base;
		FLOAT Radius;
		FLOAT HalfHeight;
		FColor Color;
	};

	/** Struct to hold info about lined stars to render. */
	struct FWireStar
	{
		FWireStar(const FVector &InPosition, const FColor &InColor, const FLOAT &InSize) : 
		Position(InPosition),
		Color(InColor),
		Size(InSize) {}

		FVector Position;
		FColor Color;
		FLOAT Size;
	};

	/** Struct to hold info about arrowed lines to render. */
	struct FArrowLine
	{
		FArrowLine(const FVector &InStart, const FVector &InEnd, const FColor &InColor) : 
		Start(InStart),
		End(InEnd),
		Color(InColor) {}

		FVector Start;
		FVector End;
		FColor Color;
	};

	/** Struct to gold info about dashed lines to render. */
	struct FDashedLine
	{
		FDashedLine(const FVector &InStart, const FVector &InEnd, const FColor &InColor, const FLOAT InDashSize) :
		Start(InStart),
		End(InEnd),
		Color(InColor),
		DashSize(InDashSize) {}

		FVector Start;
		FVector End;
		FColor Color;
		FLOAT DashSize;
	};

	TArray<FWireCylinder>	Cylinders;
	TArray<FArrowLine>		ArrowLines;
	TArray<FWireStar>		Stars;
	TArray<FDashedLine>		DashedLines;
	TArray<FDebugLine>		Lines;
	TArray<FDebugBox>		WireBoxes;
};

#endif
