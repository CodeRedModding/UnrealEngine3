/*================================================================================
	FoliageEdMode.h: Foliage editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/


#ifndef __FoliageEdMode_h__
#define __FoliageEdMode_h__

#ifdef _MSC_VER
	#pragma once
#endif

// Current user settings in Foliage UI
struct FFoliageUISettings
{
	void Load();
	void Save();

	// Window
	void SetWindowSizePos(INT NewX, INT NewY, INT NewWidth, INT NewHeight) { WindowX = NewX; WindowY = NewY; WindowWidth = NewWidth; WindowHeight = NewHeight; }
	void GetWindowSizePos(INT& OutX, INT& OutY, INT& OutWidth, INT& OutHeight) { OutX = WindowX; OutY = WindowY; OutWidth = WindowWidth; OutHeight = WindowHeight; }

	// tool
	bool GetPaintToolSelected() const { return bPaintToolSelected ? TRUE : FALSE; }
	void SetPaintToolSelected(bool InbPaintToolSelected) { bPaintToolSelected = InbPaintToolSelected; }
	bool GetReapplyToolSelected() const { return bReapplyToolSelected ? TRUE : FALSE; }
	void SetReapplyToolSelected(bool InbReapplyToolSelected) { bReapplyToolSelected = InbReapplyToolSelected; }
	bool GetSelectToolSelected() const { return bSelectToolSelected ? TRUE : FALSE; }
	void SetSelectToolSelected(bool InbSelectToolSelected) { bSelectToolSelected = InbSelectToolSelected; }
	bool GetLassoSelectToolSelected() const { return bLassoSelectToolSelected ? TRUE : FALSE; }
	void SetLassoSelectToolSelected(bool InbLassoSelectToolSelected) { bLassoSelectToolSelected = InbLassoSelectToolSelected; }
	bool GetPaintBucketToolSelected() const { return bPaintBucketToolSelected ? TRUE : FALSE; }
	void SetPaintBucketToolSelected(bool InbPaintBucketToolSelected) { bPaintBucketToolSelected = InbPaintBucketToolSelected; }
	bool GetReapplyPaintBucketToolSelected() const { return bReapplyPaintBucketToolSelected ? TRUE : FALSE; }
	void SetReapplyPaintBucketToolSelected(bool InbReapplyPaintBucketToolSelected) { bReapplyPaintBucketToolSelected = InbReapplyPaintBucketToolSelected; }
	FLOAT GetRadius() const { return Radius; }
	void SetRadius(FLOAT InRadius) { Radius = InRadius; }
	FLOAT GetPaintDensity() const { return PaintDensity; }
	void SetPaintDensity(FLOAT InPaintDensity) { PaintDensity = InPaintDensity; }
	FLOAT GetUnpaintDensity() const { return UnpaintDensity; }
	void SetUnpaintDensity(FLOAT InUnpaintDensity) { UnpaintDensity = InUnpaintDensity; }
	bool GetFilterLandscape() const { return bFilterLandscape ? TRUE : FALSE; }
	void SetFilterLandscape(bool InbFilterLandscape ) { bFilterLandscape = InbFilterLandscape; }
	bool GetFilterStaticMesh() const { return bFilterStaticMesh ? TRUE : FALSE; }
	void SetFilterStaticMesh(bool InbFilterStaticMesh ) { bFilterStaticMesh = InbFilterStaticMesh; }
	bool GetFilterBSP() const { return bFilterBSP ? TRUE : FALSE; }
	void SetFilterBSP(bool InbFilterBSP ) { bFilterBSP = InbFilterBSP; }
	bool GetFilterTerrain() const { return bFilterTerrain ? TRUE : FALSE; }
	void SetFilterTerrain(bool InbFilterTerrain ) { bFilterTerrain = InbFilterTerrain; }

	FFoliageUISettings()
	:	WindowX(-1)
	,	WindowY(-1)
	,	WindowWidth(284)
	,	WindowHeight(400)
	,	bPaintToolSelected(TRUE)
	,	bReapplyToolSelected(FALSE)
	,	bSelectToolSelected(FALSE)
	,	bLassoSelectToolSelected(FALSE)
	,	bPaintBucketToolSelected(FALSE)
	,	bReapplyPaintBucketToolSelected(FALSE)
	,	Radius(512.f)
	,	PaintDensity(0.5f)
	,	UnpaintDensity(0.f)
	,	bFilterLandscape(TRUE)
	,	bFilterStaticMesh(TRUE)
	,	bFilterBSP(TRUE)
	,	bFilterTerrain(TRUE)
	{
	}

	~FFoliageUISettings()
	{
	}

private:
	INT WindowX;
	INT WindowY;
	INT WindowWidth;
	INT WindowHeight;

	UBOOL bPaintToolSelected;
	UBOOL bReapplyToolSelected;
	UBOOL bSelectToolSelected;
	UBOOL bLassoSelectToolSelected;
	UBOOL bPaintBucketToolSelected;
	UBOOL bReapplyPaintBucketToolSelected;

	FLOAT Radius;
	FLOAT PaintDensity;
	FLOAT UnpaintDensity;

public:
	UBOOL bFilterLandscape;
	UBOOL bFilterStaticMesh;
	UBOOL bFilterBSP;
	UBOOL bFilterTerrain;
};

//
// Wrapper to expose FFoliageMeshInfo to WPF code. 
//
struct FFoliageMeshUIInfo
{
	UStaticMesh* StaticMesh;
	FFoliageMeshInfo* MeshInfo;
	
	FFoliageMeshUIInfo(UStaticMesh* InStaticMesh, FFoliageMeshInfo* InMeshInfo)
	:	StaticMesh(InStaticMesh)
	,	MeshInfo(InMeshInfo)
	{}
};

// Snapshot of current MeshInfo state. Created at start of a brush stroke to store the existing instance info.
class FMeshInfoSnapshot
{
	FFoliageInstanceHash Hash;
	TArray<FVector> Locations;
public:
	FMeshInfoSnapshot(FFoliageMeshInfo* MeshInfo)
	:	Hash(*MeshInfo->InstanceHash)
	,	Locations(MeshInfo->Instances.Num())
	{
		for( INT Idx=0;Idx<MeshInfo->Instances.Num();Idx++ )
		{
			Locations(Idx) = MeshInfo->Instances(Idx).Location;
		}
	}

	INT CountInstancesInsideSphere( const FSphere& Sphere )
	{
		TSet<INT> TempInstances;
		Hash.GetInstancesOverlappingBox(FBox::BuildAABB(Sphere.Center, FVector(Sphere.W,Sphere.W,Sphere.W)), TempInstances );

		INT Count=0;
		for( TSet<INT>::TConstIterator It(TempInstances); It; ++It )
		{
			if( FSphere(Locations(*It),0.f).IsInside(Sphere) )
			{
				Count++;
			}
		}	
		return Count;
	}
};


/**
 * Foliage editor mode
 */
class FEdModeFoliage : public FEdMode
{
public:
	FFoliageUISettings UISettings;

	/** Constructor */
	FEdModeFoliage();

	/** Destructor */
	virtual ~FEdModeFoliage();

	/** FSerializableObject: Serializer */
	virtual void Serialize( FArchive &Ar );

	/** FEdMode: Called when the mode is entered */
	virtual void Enter();

	/** FEdMode: Called when the mode is exited */
	virtual void Exit();

	/** Called when the current level changes */
	void NotifyNewCurrentLevel();

	/** Called when the user changes the current tool in the UI */
	void NotifyToolChanged();

	/**
	 * Called when the mouse is moved over the viewport
	 *
	 * @param	InViewportClient	Level editor viewport client that captured the mouse input
	 * @param	InViewport			Viewport that captured the mouse input
	 * @param	InMouseX			New mouse cursor X coordinate
	 * @param	InMouseY			New mouse cursor Y coordinate
	 *
	 * @return	TRUE if input was handled
	 */
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

	/** FEdMode: Render elements for the Foliage tool */
	virtual void Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI );

	/** FEdMode: Render HUD elements for this tool */
	virtual void DrawHUD( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas );

	// Handling SelectActor
	virtual UBOOL Select( AActor* InActor, UBOOL bInSelected );
	/** FEdMode: Called when the currently selected actor has changed */
	virtual void ActorSelectionChangeNotify();

	/** Notifies all active modes of mouse click messages. */
	UBOOL HandleClick( HHitProxy *HitProxy, const FViewportClick &Click );

	/** FEdMode: widget handling */
	virtual FVector GetWidgetLocation() const;
	virtual UBOOL AllowWidgetMove();
	virtual UBOOL ShouldDrawWidget() const;
	virtual UBOOL UsesTransformWidget() const;
	virtual INT GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const;

	/** Forces real-time perspective viewports */
	void ForceRealTimeViewports( const UBOOL bEnable, const UBOOL bStoreCurrentState );

	/** Trace under the mouse cursor and update brush position */
	void FoliageBrushTrace( FEditorLevelViewportClient* ViewportClient, INT MouseX, INT MouseY );

	/** Generate start/end points for a random trace inside the sphere brush. 
	    returns a line segment inside the sphere parallel to the view direction */
	void GetRandomVectorInBrush(FVector& OutStart, FVector& OutEnd );

	/** Apply brush */
	void ApplyBrush( FEditorLevelViewportClient* ViewportClient );

	/** Update existing mesh info for current level */
	void UpdateFoliageMeshList();

	TArray<struct FFoliageMeshUIInfo>& GetFoliageMeshList();
	
	/** Add a new mesh */
	void AddFoliageMesh(UStaticMesh* StaticMesh);

	/** Remove a mesh */
	void RemoveFoliageMesh(UStaticMesh* StaticMesh);

	/** Replace a mesh with another one */
	void ReplaceStaticMesh(UStaticMesh* OldStaticMesh, UStaticMesh* NewStaticMesh);

	/** Bake meshes to StaticMeshActors */
	void BakeFoliage(UStaticMesh* StaticMesh, UBOOL bSelectedOnly);

	/** Copy the settings object for this static mesh */
	void CopySettingsObject(UStaticMesh* StaticMesh);

	/** Replace the settings object for this static mesh with the one specified */
	void ReplaceSettingsObject(UStaticMesh* StaticMesh, UInstancedFoliageSettings* NewSettings);

	/** Save the settings object */
	void SaveSettingsObject(UStaticMesh* StaticMesh);

	/** Apply paint bucket to actor */
	void ApplyPaintBucket(AActor* Actor, UBOOL bRemove);
private:

	/** Add instances inside the brush to match DesiredInstanceCount */
	void AddInstancesForBrush( AInstancedFoliageActor* IFA, UStaticMesh* StaticMesh, FFoliageMeshInfo& MeshInfo, INT DesiredInstanceCount, TArray<INT>& ExistingInstances, FLOAT Pressure );

	/** Remove instances inside the brush to match DesiredInstanceCount */
	void RemoveInstancesForBrush( AInstancedFoliageActor* IFA, FFoliageMeshInfo& MeshInfo, INT DesiredInstanceCount, TArray<INT>& ExistingInstances, FLOAT Pressure );

	/** Reapply instance settings to exiting instances */
	void ReapplyInstancesForBrush( AInstancedFoliageActor* IFA, FFoliageMeshInfo& MeshInfo, TArray<INT>& ExistingInstances );

#if WITH_MANAGED_CODE
	/** Foliage palette window */
	TScopedPointer< class FFoliageEditWindow > FoliageEditWindow;
#endif

	UBOOL bBrushTraceValid;
	FVector BrushLocation;
	FVector BrushTraceDirection;
	UStaticMeshComponent* SphereBrushComponent;

	// Landscape layer cache data
	TMap<FName, TMap<class ULandscapeComponent*, TArray<BYTE> > > LandscapeLayerCaches;

	// Placed level data
	TArray<struct FFoliageMeshUIInfo> FoliageMeshList;

	// Cache of instance positions at the start of the transaction
	TMap<UStaticMesh*, class FMeshInfoSnapshot> InstanceSnapshot;

	UBOOL bToolActive;
	UBOOL bCanAltDrag;

	// The actor that owns any currently visible selections
	AInstancedFoliageActor* SelectionIFA;
};


#endif	// __FoliageEdMode_h__
