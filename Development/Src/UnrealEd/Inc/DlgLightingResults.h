/*=============================================================================
	DlgLightingResults.h: UnrealEd dialog for displaying lighting build 
						  errors and warnings.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef __DLGLIGHTINGRESULTS_H__
#define __DLGLIGHTINGRESULTS_H__

#include "DlgMapCheck.h"

/**
 * Dialog that displays lighting build warnings and errors that allows the 
 * user to focus on and delete any actors associated with those errors.
 */
class WxDlgLightingResults : public WxDlgMapCheck
{
public:
	WxDlgLightingResults(wxWindow* InParent);
	virtual ~WxDlgLightingResults();

	/**
	 *	Initialize the dialog box.
	 */
	virtual void Initialize();

	/**
	 * Shows the dialog only if there are warnings or errors to display.
	 */
	virtual void ShowConditionally();

	/**
	 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
	 *
	 * @param	InType					The message type (error/warning/...).
	 * @param	InGroup					The message group (kismet/mobile/...).
	 * @param	InObject				Object associated with the message; can be NULL.
	 * @param	InMessage				The message to display.
	 * @param	InUDNPage				UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage. 
	 */
	static void AddItem(MapCheckType InType, UObject* InObject, const TCHAR* InMessage, const TCHAR* InUDNPage=TEXT(""), MapCheckGroup InGroup=MCGROUP_DEFAULT);

	/** Event handler for when the refresh button is clicked on. */
	virtual void RefreshWindow()
	{
		wxCommandEvent Temp;
		OnRefresh(Temp);
	}

protected:
	/** Event handler for when the refresh button is clicked on. */
	virtual void OnRefresh(wxCommandEvent& In);

	/** Event handler for when the goto button is clicked on. */
	virtual void OnGoTo(wxCommandEvent& In);

	/** Event handler for when a message is clicked on. */
	virtual void OnItemActivated(wxListEvent& In);

protected:
	DECLARE_EVENT_TABLE()
};

//
//	DlgStaticMeshLighting
//

/** Struct that holds information about a entry in the StaticMeshLightingInfo list. */
struct FStaticMeshLightingInfo
{
	/** The actor that is related to this error/warning. */
	AActor* StaticMeshActor;

	/** The staticmesh component that is related to this error/warning. */
	UStaticMeshComponent* StaticMeshComponent;

	/** The source StaticMesh that is related to this info. */
	UStaticMesh* StaticMesh;

	/** If TRUE, the object is currently using Texture mapping. */
	UBOOL bTextureMapping;

	/** The static lighting resolution the texture mapping was estimated with. */
	INT StaticLightingResolution;
	/** Number of lights generating light maps on the primtive. */
	INT LightMapLightCount;
	/** Estimated memory usage in bytes for light map texel data. */
	INT TextureLightMapMemoryUsage;
	/** Estimated memory usage in bytes for light map vertex data. */
	INT VertexLightMapMemoryUsage;
	/** Number of lights generating shadow maps on the primtive. */
	INT ShadowMapLightCount;
	/** Estimated memory usage in bytes for shadow map texel data. */
	INT TextureShadowMapMemoryUsage;
	/** Estimated memory usage in bytes for shadow map vertex data. */
	INT VertexShadowMapMemoryUsage;
	/** If TRUE if the mesh has the proper UV channels. */
	UBOOL bHasLightmapTexCoords;

	UBOOL operator==(const FStaticMeshLightingInfo& Other) const
	{
		return (
			(StaticMeshActor == Other.StaticMeshActor) && 
			(StaticMeshComponent == Other.StaticMeshComponent) &&
			(StaticMesh == Other.StaticMesh) &&
			(bTextureMapping == Other.bTextureMapping) &&
			(StaticLightingResolution == Other.StaticLightingResolution) &&
			(LightMapLightCount == Other.LightMapLightCount) && 
			(TextureLightMapMemoryUsage == Other.TextureLightMapMemoryUsage) &&
			(VertexLightMapMemoryUsage == Other.VertexLightMapMemoryUsage) &&
			(ShadowMapLightCount == Other.ShadowMapLightCount) &&
			(TextureShadowMapMemoryUsage == Other.TextureShadowMapMemoryUsage) &&
			(VertexShadowMapMemoryUsage == Other.VertexShadowMapMemoryUsage) &&
			(bHasLightmapTexCoords == Other.bHasLightmapTexCoords)
			);
	}
};

/**
 * Dialog that displays the StaticMesh lighting info for the selected level(s).
 */
class WxDlgStaticMeshLightingInfo : public WxTrackableDialog, public FSerializableObject
{
public:
	WxDlgStaticMeshLightingInfo(wxWindow* InParent);
	virtual ~WxDlgStaticMeshLightingInfo();

	/** Called when the window has been selected from within the ctrl + tab dialog. */
	virtual void OnSelected();

	/** Shows the dialog only if there are messages to display. */
	virtual void ShowConditionally();

	/** Clears out the list of messages appearing in the window. */
	virtual void ClearMessageList();

	/** Freezes the message list. */
	virtual void FreezeMessageList();

	/** Thaws the message list. */
	virtual void ThawMessageList();

	/** 
	 *	Adds a message to the map check dialog, to be displayed when the dialog is shown.
	 *
	 *	@param	InActor							StaticMeshActor associated with the message
	 *	@param	InStaticMeshComponent			StaticMeshComponent associated with the message
	 *	@param	InStaticMesh					The source StaticMesh that is related to this info.
	 *	@param	bInTextureMapping				If TRUE, the object is currently using Texture mapping.
	 *	@param	InStaticLightingResolution		The static lighting resolution used to estimate texture mapping.
	 *	@param	InLightMapLightCount			The number of lights generating light maps on the primitive.
	 *	@param	InTextureLightMapMemoryUsage	Estimated memory usage in bytes for light map texel data.
	 *	@param	InVertexLightMapMemoryUsage		Estimated memory usage in bytes for light map vertex data.
	 *	@param	InShadowMapLightCount			The number of lights generating shadow maps on the primtive.
	 *	@param	InTextureShadowMapMemoryUsage	Estimated memory usage in bytes for shadow map texel data.
	 *	@param	InVertexShadowMapMemoryUsage	Estimated memory usage in bytes for shadow map vertex data.
	 *	@param	bInHasLightmapTexCoords			If TRUE if the mesh has the proper UV channels.
	 */
	static void AddItem(AActor* InActor, UStaticMeshComponent* InStaticMeshComponent, UStaticMesh* InStaticMesh, 
		UBOOL bInTextureMapping, INT InStaticLightingResolution, 
		INT InLightMapLightCount, INT InTextureLightMapMemoryUsage, INT InVertexLightMapMemoryUsage, 
		INT InShadowMapLightCount, INT InTextureShadowMapMemoryUsage, INT InVertexShadowMapMemoryUsage, 
		UBOOL bInHasLightmapTexCoords);

	/** 
	 *	Adds the given static mesh lighting info to the list.
	 *
	 *	@param	InSMLightingInfo				The lighting info to add
	 */
	virtual void AddItem(FStaticMeshLightingInfo& InSMLightingInfo);

	/** Level options for scan StaticMeshes */
	enum ELevelOptions
	{
		DLGSMLI_CurrentLevel,
		DLGSMLI_SelectedLevels,
		DLGSMLI_AllLevels
	};
	/** 
	 *	Scans the level(s) for static meshes and fills in the information.
	 *
	 *	@param	InLevelOptions					What level(s) to scan.
	 */
	static void ScanStaticMeshLightingInfo(ELevelOptions InLevelOptions);

	/**
	 *	Serialize the referenced assets to prevent GC.
	 *
	 *	@param	Ar		The archive to serialize to.
	 */
	virtual void Serialize(FArchive& Ar);

	/** Loads the list control with the contents of the GErrorWarningInfoList array. */
	virtual void LoadListCtrl();

	/**
	 *	Show the dialog.
	 *
	 *	@param	show	If TRUE show the dialog, FALSE hide it.
	 *
	 *	@return	bool	
	 */
	virtual bool Show( bool show = true );

protected:
	wxListCtrl*							StaticMeshList;
	TArray<UObject*>					ReferencedObjects;
	TArray<FStaticMeshLightingInfo>*	StaticMeshInfoList;

	wxButton*	CloseButton;
	wxButton*	RescanAllLevelsButton;
	wxButton*	RescanSelectedLevelsButton;
	wxButton*	RescanCurrentLevelButton;
	wxButton*	GotoButton;
	wxButton*	SyncButton;
	wxButton*	SwapButton;
	wxButton*	SwapExButton;
	wxButton*	SetToVertexButton;
	wxButton*	SetToTextureButton;
	wxButton*	SetToTextureExButton;
	wxButton*	SelectAllButton;
	wxButton*	UndoButton;
	wxButton*	CopyToClipboardButton;

	ELevelOptions LastScanSetting;

	/** The lights in the world which the system is scanning. */
	TArray<ULightComponent*> AllLights;

	/** Event handler for when the close button is clicked on. */
	virtual void OnClose(wxCommandEvent& In);

	/** Event handler for when the rescan button is clicked on. */
	virtual void OnRescan(wxCommandEvent& In);

	/** Event handler for when the goto button is clicked on. */
	virtual void OnGoTo(wxCommandEvent& In);

	/** Event handler for when the sync button is clicked on. */
	virtual void OnSync(wxCommandEvent& In);

	/** Event handler for when the swap button is clicked on. */
	virtual void OnSwap(wxCommandEvent& In);

	/** Event handler for when the swap ex button is clicked on. */
	virtual void OnSwapEx(wxCommandEvent& In);

	/** Event handler for when the SetToVertex button is clicked on. */
	virtual void OnSetToVertex(wxCommandEvent& In);

	/** Event handler for when the SetToTexture button is clicked on. */
	virtual void OnSetToTexture(wxCommandEvent& In);
	
	/** Event handler for when the SetToTexture button is clicked on. */
	virtual void OnSetToTextureEx(wxCommandEvent& In);
	
	/** Event handler for when a message is clicked on. */
	virtual void OnItemActivated(wxListEvent& In);

	/** Event handler for when a column header is clicked on. */
	virtual void OnListColumnClick(wxListEvent& In);

	/** Event handler for SelectAll button. */
	virtual void OnSelectAll(wxCommandEvent& In);

	/** Event handler for undo. */
	virtual void OnUndo(wxCommandEvent& In);

	/** Event handler for logging selected entries. */
	virtual void OnLog(wxCommandEvent& In);

	/** Event handler for when wx wants to update UI elements. */
	virtual void OnUpdateUI(wxUpdateUIEvent& In);

public:
	/** Set up the columns for the ErrorWarningList control. */
	virtual void SetupListColumns();

	/** 
	 *	Get the level/package name for the given object.
	 *
	 *	@param	InObject	The object to retrieve the level/package name for.
	 *
	 *	@return	FString		The name of the level/package.
	 */
	virtual FString GetLevelOrPackageName(UObject* InObject);

protected:
	/** 
	 *	Sets the lighting information for the given StaticMeshComponent up.
	 *
	 *	@param	InSMComponent		The static mesh component to setup the lighting info for.
	 *	@param	InSMActor			The static mesh actor that 'owns' the component.
	 *	@param	OutSMLightingINfo	The StaticMeshLightingInfo to fill in.
	 *	
	 *	@return	UBOOL				TRUE if it was successful; FALSE if not
	 */
	virtual UBOOL FillStaticMeshLightingInfo(
		UStaticMeshComponent* InSMComponent,
		AActor* InSMActor,
		FStaticMeshLightingInfo& OutSMLightingInfo
		);

	/** Sets the given entry in the list. */
	virtual void FillListEntry(INT InIndex);

	/** Updates the 'totals' entry of the list. */
	virtual void UpdateTotalsEntry();

public:
	enum ESortMethod
	{
		SORTBY_NONE = -1,
		SORTBY_Level,
		SORTBY_Actor,
		SORTBY_StaticMesh,
		SORTBY_MappingType,
		SORTBY_HasLightmapUVs,
		SORTBY_StaticLightingResolution,
		SORTBY_TextureLightMap,
		SORTBY_VertexLightMap,
		SORTBY_NumLightMapLights,
		SORTBY_TextureShadowMap,
		SORTBY_VertexShadowMap,
		SORTBY_NumShadowMapLights
	};

protected:
	/** Sort the list by the given method. */
	virtual void SortList(ESortMethod InSortMethod);

	/**
	 *	Prompt the user for the static lightmap resolution
	 *
	 *	@return	INT		The desired resolution
	 *					-1 indicates the user cancelled the dialog
	 */
	INT GetUserSetStaticLightmapResolution();

	/**
	 *	Get the selected meshes, as well as an array of the selected indices...
	 *
	 *	@param	OutSelectedObjects		The array of selected objects to fill in.
	 *	@param	OutSelectedIndices		The array of selected indices to fill in.
	 */
	void GetSelectedMeshes(TArray<FStaticMeshLightingInfo*>& OutSelectedObjects, TArray<long>& OutSelectedIndices);

	/** 
	 *	Swap the selected entries, using the given resolution.
	 *
	 *	@param	InStaticLightingResolution		== 0 to use the values already set.
	 *											!= 0 to force all to the given value.
	 */
	void SwapMappingMethodOnSelectedComponents(INT InStaticLightingResolution);

	/** 
	 *	Set all the entries to the given mapping type, using the given resolution (if applicable)
	 *
	 *	@param	bInTextureMapping				TRUE if all selects components should be set to texture mapping.
	 *											FALSE if all selects components should be set to vertex mapping.
	 *	@param	InStaticLightingResolution		== 0 to use the values already set.
	 *											!= 0 to force all to the given value.
	 *											Ignored if setting to vertex mapping
	 */
	void SetMappingMethodOnSelectedComponents(UBOOL bInTextureMapping, INT InStaticLightingResolution);

	/** The last 'scan' that was used to fill the dialog */
	ELevelOptions LastLevelScan;

protected:
	DECLARE_EVENT_TABLE()
};

//
//	DlgLightingBuildInfo
//
/** Struct that holds information about an entry in the LightingBuildInfo list. */
struct FLightingBuildInfoEntry
{
	/** The actor and/or object that is related to this info.  Can be NULL if not relevant. */
	UObject* Object;
	/** The lighting time this object took. */
	DOUBLE LightingTime;
	/** The percentage of unmapped texels for this object. */
	FLOAT UnmappedTexelsPercentage;
	/** The memory consumed by unmapped texels for this object. */
	FLOAT UnmappedTexelsMemory;
	/** The memory consumed by all texels for this object. */
	INT TotalTexelMemory;

	UBOOL operator==(const FLightingBuildInfoEntry& Other) const
	{
		return (
			(Object == Other.Object) && 
			(LightingTime == Other.LightingTime) &&
			(UnmappedTexelsPercentage == Other.UnmappedTexelsPercentage) &&
			(UnmappedTexelsMemory == Other.UnmappedTexelsMemory) &&
			(TotalTexelMemory == Other.TotalTexelMemory)
			);
	}
};

/** Custom dynamic list control for lighting results */
class WxLightingListCtrl : public wxListCtrl
{
public:
	virtual wxString OnGetItemText(long item, long column) const;

	void SetLightingBuildInfoList (const TArray<FLightingBuildInfoEntry>* InLightingBuildInfoList) { LightingBuildInfoList = InLightingBuildInfoList;}
	void SetTotalTime(const DOUBLE InTotalTime) { TotalTime = InTotalTime;}

private:
	const TArray<FLightingBuildInfoEntry>*	LightingBuildInfoList;

	INT TotalTime;
};

/**
 *	Dialog that displays the lighting build info for the selected level(s).
 *	This include percent of time spent lighting each object as well as the
 *	percentage of unmapped texels contained in that object.
 */
class WxDlgLightingBuildInfo : public WxTrackableDialog, public FSerializableObject
{
public:
	WxDlgLightingBuildInfo(wxWindow* InParent);
	virtual ~WxDlgLightingBuildInfo();

	/** Called when the window has been selected from within the ctrl + tab dialog. */
	virtual void OnSelected();

	/** Shows the dialog only if there are messages to display. */
	virtual void ShowConditionally();

	/** Clears out the list of messages appearing in the window. */
	virtual void ClearMessageList();

	/** Freezes the message list. */
	virtual void FreezeMessageList();

	/** Thaws the message list. */
	virtual void ThawMessageList();

	/** 
	 *	Adds a message to the dialog, to be displayed when the dialog is shown.
	 *
	 *	@param	InObject						The object associated with the message
	 *	@param	InLightingTime					The percentage of lighting time the object took.
	 *	@param	InUnmappedTexelsPercentage		The percentage of unmapped texels this object has.
	 *	@param	InUnmappedTexelsMemory			The amount of memory consumed by unmapped texels of this object.
	 *	@param	InTotalTexelMemory				The memory consumed by all texels for this object.
	 */
	static void AddItem(UObject* InObject, DOUBLE InLightingTime, FLOAT InUnmappedTexelsPercentage, 
			INT InUnmappedTexelsMemory, INT InTotalTexelMemory);

	/** 
	 *	Adds the given static mesh lighting info to the list.
	 *
	 *	@param	InLightingBuildInfo		The lighting build info to add
	 */
	virtual void AddItem(FLightingBuildInfoEntry& InLightingBuildInfo);

	/**
	 *	Serialize the referenced assets to prevent GC.
	 *
	 *	@param	Ar		The archive to serialize to.
	 */
	virtual void Serialize(FArchive& Ar);

	/** Loads the list control with the contents of the GErrorWarningInfoList array. */
	virtual void LoadListCtrl();

	/**
	 *	Show the dialog.
	 *
	 *	@param	show	If TRUE show the dialog, FALSE hide it.
	 *
	 *	@return	bool	
	 */
	virtual bool Show( bool show = true );

	/** Event handler for when the refresh button is clicked on. */
	virtual void RefreshWindow()
	{
		wxCommandEvent Temp;
		OnRefresh(Temp);
	}

protected:
	
	WxLightingListCtrl*					LightingBuildListCtrl;
	TArray<UObject*>					ReferencedObjects;
	TArray<FLightingBuildInfoEntry>*	LightingBuildInfoList;

	wxButton*		CloseButton;
	wxButton*		GotoButton;
	wxButton*		SyncButton;
	wxStaticText*	TotalTexelMemoryLabel;
	wxStaticText*	TotalTexelMemoryDisplay;
	wxStaticText*	TotalUnmappedTexelMemoryLabel;
	wxStaticText*	TotalUnmappedTexelMemoryDisplay;
	wxStaticText*	TotalUnmappedTexelMemoryPercentageDisplay;

	INT TrackingTotalMemory;
	INT TrackingTotalUnmappedMemory;

	/** Event handler for when the refresh button is clicked on. */
	virtual void OnRefresh(wxCommandEvent& In);

	/** Event handler for when the close button is clicked on. */
	virtual void OnClose(wxCommandEvent& In);

	/** Event handler for when the goto button is clicked on. */
	virtual void OnGoTo(wxCommandEvent& In);

	/** Event handler for when the sync button is clicked on. */
	virtual void OnSync(wxCommandEvent& In);

	/** Event handler for when a message is clicked on. */
	virtual void OnItemActivated(wxListEvent& In);

	/** Event handler for when a column header is clicked on. */
	virtual void OnListColumnClick(wxListEvent& In);

public:
	/** Set up the columns for the ErrorWarningList control. */
	virtual void SetupListColumns();

	/** 
	 *	Get the level/package name for the given object.
	 *
	 *	@param	InObject	The object to retrieve the level/package name for.
	 *
	 *	@return	FString		The name of the level/package.
	 */
	static FString GetLevelOrPackageName(UObject* InObject);

	/** 
	 *	Get the display name for the given object.
	 *
	 *	@param	InObject	The object to retrieve the name for.
	 *	@param	bFullPath	If TRUE, return the full path name.
	 *
	 *	@return	FString		The display name of the object.
	 */
	static FString GetObjectDisplayName(UObject* InObject, UBOOL bFullPath);

protected:

public:
	enum ESortMethod
	{
		SORTBY_NONE = -1,
		SORTBY_Level,
		SORTBY_Object,
		SORTBY_Timing,
		SORTBY_TotalMemory,
		SORTBY_UnmappedMemory,
		SORTBY_UnmappedTexels
	};

protected:
	/** Sort the list by the given method. */
	virtual void SortList(ESortMethod InSortMethod);

protected:
	DECLARE_EVENT_TABLE()
};

#endif // __DLGLIGHTINGRESULTS_H__
