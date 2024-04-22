/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __TEXTURESTATSBROWSER_H__
#define __TEXTURESTATSBROWSER_H__

#include "ReferencedAssetsBrowser.h"

// todo check for WITH_UE3_NETWORKING
#include "UnIpDrv.h"

// order here also defines the order in the list control, need to start with 0 and end with TSBC_MAX
enum ETextureStatsBrowserColumns
{
	TSBC_Name,				// Name of the texture
	TSBC_TexType,			// Type of texture (e.g 2D, Cube)
	TSBC_MaxDim,			// Max dimension
	TSBC_CurrentDim,		// Current dimension
	TSBC_Format,			// Format (e.g DXT1, DXT5)
	TSBC_Group,				// LOD group
	TSBC_LODBias,			// LOD bias
	TSBC_CurrentKB,			// Current KB usage
	TSBC_FullyLoadedKB,		// Fully loaded KB usage
	TSBC_NumUses,			// Number of times the texture is used
	TSBC_LowResMipsKB,		// KB for low res mips
	TSBC_HighResMipsKB,		// KB for high res mips
	TSBC_LastTimeRendered,	// Last time the texture was seen
	TSBC_Path,				// The path of the package texture
	TSBC_Packages,			// The package the texture is in
	TSBC_MAX				// Must be last
};
extern FString GetColumnName(ETextureStatsBrowserColumns Key, UBOOL &bOutRightAligned);

enum ETextureStatsBrowserListMode
{
	LM_SelectedActors,
	LM_SelectedMaterials,
	LM_CurrentLevel,
	LM_AllLevels,
	LM_CookerStatistics,
	LM_RemoteCapture,
	LM_Max
};

// following functions are needed to hide some colums depening on the ListMode
extern UBOOL IsColumnShownInThatMode(ETextureStatsBrowserListMode ListMode, ETextureStatsBrowserColumns Key);
extern INT GetColumnIndexFromEnum(ETextureStatsBrowserListMode ListMode, ETextureStatsBrowserColumns Enum);
extern ETextureStatsBrowserColumns GetColumnEnumFromIndex(ETextureStatsBrowserListMode ListMode, INT ColumnIndex);

class FUniqueTextureStats
{
	/** Texture path without the name "package.[group.]" */
	FString						Path;
	/** Texture name without the package, "name" */
	FString						Name;
	/** Max Dimension e.g. 256x256, not including the format */
	FString						MaxDim;
	/** Current Dimension e.g 256x256 */
	FString						CurrentDim;
	/** Type e.g. 2D, 3D, Cube, "" if not known, ... */
	FString						TexType;
	/** The texture format, e.g. PF_DXT1 */
	EPixelFormat				Format;
	/** The package names separated by comma, e.g. "CORE,MP_Level0,MP_Level1" */
	FString						Packages;
	/** The texture group, TEXTUREGROUP_MAX is not used, e.g. TEXTUREGROUP_World */
	TextureGroup				Group;
	/** The memory used currently, FLT_MAX means unknown */
	FLOAT						CurrentKB;
	/** The memory used when the texture is fully loaded.  */
	FLOAT						FullyLoadedKB;
	/** Relative time it was used for rendering the last time, FLT_MAX means unknown */
	FLOAT						LastTimeRendered;
	/** 
	* Size of higher mips stored just once. Does not include storage alignment 
	* waste and compression in e.g. the TFC case. (aka StoredOnceMipSizeKB)
	* FLT_MAX means unknown
	*/
	FLOAT						HighResMipsKB;
	/** 
	* Size of lower mips duplicated into packages. This is before compression and
	* the size is not a total but rather for a single package. Multiply by 
	* PackageNames.Num() for total.	(aka DuplicatedMipSizeKB)											
	* FLT_MAX means unknown
	*/
	FLOAT						LowResMipsKB;

	/** LOD Bias for this texture. (Texture LODBias + Texture group) */
	INT							LODBias;

public:
	/** The number of times the texture is used, 0xffffffff if unknown */
	UINT						NumUses;

	/** Used as starting point for accumulation of the combined stats. */
	FUniqueTextureStats() :
		FullyLoadedKB(0),
		NumUses(0),
		Format(PF_Unknown),
		Group(TEXTUREGROUP_MAX),
		HighResMipsKB(0),
		LowResMipsKB(0),
		CurrentKB(0),
		LODBias(0)
	{}

	/** 
	* @param InPackages		The package names separated by comma, e.g. "CORE,MP_Level0,MP_Level1" 
	* @param InHighResMipsKB FLT_MAX if unknown
	* @param InLowResMipsKB FLT_MAX if unknown
	* @param InFullyLoadedKB if FLT_MAX it is automatically derived (if possible)
	*/
	FUniqueTextureStats(const FString &FullyQualifiedPath, const FString &InMaxDim, const FString& InCurrentDim, const FString &InTexType, const FString &InPackages, EPixelFormat InFormat,
						TextureGroup InGroup, FLOAT InHighResMipsKB, FLOAT InLowResMipsKB, FLOAT InFullyLoadedKB, FLOAT InCurrentKB, FLOAT InLastTimeRendered, UINT InNumUses, INT InLodBias ) : 
		NumUses(InNumUses),
		MaxDim(InMaxDim),
		CurrentDim(InCurrentDim),
		TexType(InTexType),
		Format(InFormat),
		Group(InGroup),
		HighResMipsKB(InHighResMipsKB),
		LowResMipsKB(InLowResMipsKB),
		Packages(InPackages),
		FullyLoadedKB(InFullyLoadedKB),
		CurrentKB(InCurrentKB),
		LastTimeRendered(InLastTimeRendered),
		LODBias(InLodBias)
	{
		SetPathAndName(FullyQualifiedPath);

		// derive FullyLoadedKB if possible
		if(FullyLoadedKB == FLT_MAX
		&& HighResMipsKB != FLT_MAX
		&& LowResMipsKB != FLT_MAX)
		{
			FullyLoadedKB = HighResMipsKB + LowResMipsKB;
		}
	}

	FUniqueTextureStats(UTexture* InTexture, AActor* InActorUsingTexture = NULL ) : 
		NumUses(0),
		Format(PF_Unknown),
		HighResMipsKB(FLT_MAX),
		LowResMipsKB(FLT_MAX),
		LastTimeRendered(FLT_MAX)
	{
		SetPathAndName(InTexture->GetPathName());
		Group = (TextureGroup)InTexture->LODGroup;
		
		CurrentKB = InTexture->CalcTextureMemorySize( TMC_ResidentMips ) / 1024.0f;
		FullyLoadedKB = InTexture->CalcTextureMemorySize( TMC_AllMipsBiased ) / 1024.0f;

		FTexture* Resource = InTexture->Resource; 

		LODBias = InTexture->GetCachedLODBias();

		if(Resource)
		{
			LastTimeRendered = GLastTime - Resource->LastRenderTime;
		}

		// Init current dim to an unknown value in case we were given and invalid texture type for stat tracking
		CurrentDim = TEXT("?");
		UTexture2D* Texture2D = Cast<UTexture2D>(InTexture);
		if( Texture2D )
		{
			Format = (EPixelFormat)Texture2D->Format;
			TexType = TEXT("2D"); 

			// Calculate in game current dimensions 
			const INT DroppedMips = Texture2D->Mips.Num() - Texture2D->ResidentMips;
			CurrentDim = FString::Printf(TEXT("%dx%d"), Texture2D->SizeX >> DroppedMips, Texture2D->SizeY >> DroppedMips);
			
			// Calculate the max dimensions
			MaxDim = FString::Printf(TEXT("%dx%d"), Texture2D->SizeX >> LODBias, Texture2D->SizeY >> LODBias);
		}
		else
		{
			// Check if the texture is a TextureCube
			UTextureCube* TextureCube = Cast<UTextureCube>(InTexture);
			if(TextureCube)
			{
				Format = (EPixelFormat)TextureCube->Format;
				TexType = TEXT("Cube"); 

				// Calculate in game current dimensions 
				// Use one face of the texture cube to calculate in game size
				UTexture2D* Face = TextureCube->GetFace(0);
				const INT DroppedMips = Face->Mips.Num() - Face->ResidentMips;
				CurrentDim = FString::Printf(TEXT("%dx%d"), Face->SizeX >> DroppedMips, Face->SizeY >> DroppedMips);

				// Calculate the max dimensions
				MaxDim = FString::Printf(TEXT("%dx%d"), Face->SizeX >> LODBias, Face->SizeY >> LODBias);
			}
		}
	}

	/** Build the unique asset name e.g. "EngineResources.Cursors.Arrow" */
	FString GetFullyQualifiedName() const
	{
		if(Path.Len())
		{
			return Path + TEXT(".") + Name;
		}
		else
		{
			return Name;
		}
	}

	/** Split the given fully qualified path into "Path" and "Name". */
	void SetPathAndName(const FString &FullyQualifiedPath)
	{
		INT Index = FullyQualifiedPath.InStr(TEXT("."), TRUE);

		if(Index == INDEX_NONE)
		{
			Name = FullyQualifiedPath;
			Path = TEXT("");
		}
		else
		{
			Name = FullyQualifiedPath.Right(FullyQualifiedPath.Len() - Index - 1);
			Path = FullyQualifiedPath.Left(Index);
		}
	}

	void AddInfo(FUniqueTextureStats& Info);

	/** Get the text representation for a given Column. */
	void GetColumnData(ETextureStatsBrowserColumns Column, FString &Out) const;

	INT Compare( const FUniqueTextureStats& Other, ETextureStatsBrowserColumns SortIndex ) const;

	friend UBOOL operator==(const FUniqueTextureStats& A,const FUniqueTextureStats& B)
	{
		return A.Path == B.Path && A.Name == B.Name;
	}

	friend FArchive& operator<<( FArchive& Ar, FUniqueTextureStats& TextureStats );
};

/**
 * Building stats browser class.
 */
class WxTextureStatsBrowser : public WxReferencedAssetsBrowserBase
{
	DECLARE_DYNAMIC_CLASS(WxTextureStatsBrowser);

protected:
	/** List control used for displaying stats. */
	WxListView*							ListControl;
	/** To change current ETextureStatsBrowserListMode */
	WxComboBox*							TextureListModes;
	/** A combo box of search modes.  Matches currently available columns. */
	WxComboBox*							SearchModes;
	/** Text control for displaying the combined stats CurrentKB count. */
	wxTextCtrl*							CombinedStatsControl;
	/** A box to type search terms in.  Will refresh the list as characters are typed. */
	wxTextCtrl*							SearchBox;
	/** Array of column headers, used during CSV export. */
	TArray<FString>						ColumnHeaders;
	/** Array of assets the used wants to hide (fully qualified name) */
	TSet<FString>						HiddenAssetNames;
	/** Data stored before hidden element have been removed */
	TArray<FUniqueTextureStats>			AllStats;
	/** If this is different from TextureListModes->GetCurrentSelection() we need to refresh the columns */
	ETextureStatsBrowserListMode		ColumnsListMode;
	/** Textures that should be ignored when taking stats */
	TArray<UTexture*>					TexturesToIgnore;
	/** Needed to prevent garbage collector to free those as we might need them later. */
	TArray<UObject*>					ReferencingObjects;
	/** To update the amount of hidden elements */
	wxButton*							HideSelectionButton;
	/** To update the amount of hidden elements */
	wxButton*							UnhideAllButton;
	/** A timer that counts down between text changes in the search filter.  When it runs out we refresh the list. */
	wxTimer								SearchTimer;
	/** The index to the type of console to remote capture from */
	INT									RemoteConsoleIndex;
public:
	/** Current sort order (-1 or 1) */
	static INT							CurrentSortOrder[TSBC_MAX];
	/** Primary index/ column to sort by */
	static ETextureStatsBrowserColumns	PrimarySortIndex;
	/** Secondary index/ column to sort by */
	static ETextureStatsBrowserColumns	SecondarySortIndex;
	/** path to the cooker data e.g. "D:/UnrealEngine3/UTGame/CookedXenon/GlobalPersistentCookerData.upk" */
	FString								CookerPathData;

	/**
	 * Constructor
	 */
	WxTextureStatsBrowser();

	/**
	 * Forwards the call to our base class to create the window relationship.
	 * Creates any internally used windows after that
	 *
	 * @param DockID the unique id to associate with this dockable window
	 * @param FriendlyName the friendly name to assign to this window
	 * @param Parent the parent of this window (should be a Notebook)
	 */
	virtual void Create(INT DockID,const TCHAR* FriendlyName,wxWindow* Parent);

	/**
 	 * Adds entries to the browser's accelerator key table.  Derived classes should call up to their parents.
 	 */
	virtual void AddAcceleratorTableEntries(TArray<wxAcceleratorEntry>& Entries);

	/**
	 * Called when the browser is getting activated (becoming the visible
	 * window in it's dockable frame).
	 */
	void Activated(void);

	/**
	 * Tells the browser to update itself
	 */
	void Update(void);

	/**
	 * Refreshes the contents of the window when requested
	 *
	 * @param In the command that was sent
	 */
	void OnRefresh(wxCommandEvent& In);

	/**
	 * Returns the key to use when looking up values
	 */
	virtual const TCHAR* GetLocalizationKey(void) const
	{
		return TEXT("TextureStatsBrowser");
	}
	
	/** 
	 * FCallbackDevice Interface 
	 */
	virtual void Send( ECallbackEventType InType );

	/** 
	 * FSerializableObject Interface 
	 */
	virtual void Serialize( FArchive& Ar );

	void UpdateListItem(const FUniqueTextureStats &Stats);

	/** 
	 * Syncs the texture stats browser to the passed in textures (if they exist)
	 *
	 * @param InTextures	Textures to find and sync to.  Note: we pass in a UObject array for compatibility with GenericBrowserType_Texture::InvokeCustomCommand
	 * @param bFocusWindow	If TRUE the window acquires focus.
	 * @return TRUE if we were able to find any of the passed in textures. FALSE if we could find none
	 */
	UBOOL SyncToTextures( const TArray<UObject*>& InTextures, UBOOL bFocusWindow );

protected:

	/**
	 * @param Index is the line number in the list
	 * @return e.g. "EngineResources.Cursors.Arrow"
	 */
	FString GetFullyQualifiedName(UINT Index) const;

	UBOOL IsTextureValidForStats( UTexture* Texture );

	/**
	 * Inserts the columns into the control.
	 */
	void InsertColumns();

	/**
	 * Build the primitives list with new data
	 */
	void BuildList();

	/**
	* Refresh the primitives list from the build data
	*/
	void RefreshList();

	/* Populate the list control for the recorded texture list that was recorded earlier  */
	void UpdateListFromRemoteCapture();

	/* Populate the list control for the cooker data mode  */
	void UpdateListFromCookerData();
	
	void UpdateListItem(const FString& TextureName, const FCookedTextureUsageInfo &UsageInfo);

	/* Builds "Referencers" so wen can traverse the data. */
	void BuildReferencingData();

	/* Populate the list control for all non cooker data modes */
	void UpdateListFromLoadedAssets();

	/* Find all the materials referencing the given texture. */
	void FindMaterialsUsingIt(UTexture &Texture, TArray<UObject *> &OutMaterials); 

	/* Find all the actor referencing the given texture, even if only indirectly though a material. */
	void FindActorsUsingIt(UTexture &Texture, TArray<AActor *> &OutActors); 

	/**
	 * @param ActorUsingTexture can be 0 if the referencer is no actor
	 */
	void AddStatItem(UTexture &Tex, AActor* ActorUsingTexture);

	/**
	 * Handler for EVT_SIZE events.
	 *
	 * @param In the command that was sent
	 */
	void OnSize( wxSizeEvent& In );

	/**
	 * Handler for column click events
	 *
	 * @param In the command that was sent
	 */
	void OnColumnClick( wxListEvent& In );

	/**
	 * Handler for column right click events
	 *
	 * @param In the command that was sent
	 */
	void OnColumnRightClick( wxListEvent& In );

	void OnItemRightClick( wxListEvent& In );

	/**
	 * Handler for item activation (double click) event
	 *
	 * @param In the command that was sent
	 */
	void OnItemActivated( wxListEvent& In );

	void OnItemSelectionChanged( wxListEvent& In );

	void OnSyncToActors( wxCommandEvent &In );

	void OnSyncToMaterials( wxCommandEvent &In );

	void OnInvertSelection( wxCommandEvent &In );

	void OnHideSelection( wxCommandEvent &In );

	void OnUnhideAll( wxCommandEvent &In );

	/**
	 * Called when the Export... menu item is selected 
	 */
	void OnExport(wxCommandEvent& In);

	/**
	 * Called when Ctrl+C is pressed
	 */
	void OnCopySelected(wxCommandEvent& In);

	/**
	 * Sets column width.
	 */
	void SetAutoColumnWidth();

	/**
	 * Called when the texture list mode combo box selection changes 
	 */
	void OnTextureListModeChanged( wxCommandEvent& In );

	/**
	 * Called when the search mode selection changes.
	 */
	void OnSearchModeChanged( wxCommandEvent& In );
 	
	/** 
	 * Called when the text in the search box changes.
	 */
	void OnSearchTextChanged( wxCommandEvent& In );

	/**
	 * Called each time the timer ticks a specific amout of time
	 */
	void OnTimer( wxTimerEvent& In );

	/**
	 * Called when a console is changed in the remote menu
	 */
	void OnChangeConsole( wxCommandEvent& In );

	/** 
	 * Gets a list of objects that should be searched for texture references
	 *
	 * @param ListMode	The listing mode for texture references.  The objects returned depends on this mode
	 * @param OutObjectsToSearch	The list of objects to search
	 */
	void GetObjectsForListMode( ETextureStatsBrowserListMode ListMode, TArray<UObject*>& OutObjectsToSearch ) const;

	void OnSelectionChanged();

	DECLARE_EVENT_TABLE();

	friend class WxTextureStatsContextMenu;
};

class WxTextureStatsContextMenu : public wxMenu
{	
public:
	WxTextureStatsContextMenu(WxTextureStatsBrowser &Parent);
};

#endif // __TEXTURESTATSBROWSER_H__
