
#include "UnrealEd.h"
#include "..\..\Launch\Resources\resource.h"
#include "EngineSequenceClasses.h"

/*-----------------------------------------------------------------------------
	WxMainMenu.
-----------------------------------------------------------------------------*/

WxMainMenu::WxMainMenu()
{
	// Allocations

	FileMenu = new wxMenu();
		MRUMenu = new wxMenu();
		ImportMenu = new wxMenu();
		ExportMenu = new wxMenu();
	EditMenu = new wxMenu();
	ViewMenu = new wxMenu();
		BrowserMenu = new wxMenu();
		ViewportConfigMenu = new wxMenu();
	BrushMenu = new wxMenu();
	BuildMenu = new wxMenu();
	ToolsMenu = new wxMenu();
	HelpMenu = new wxMenu();
	VolumeMenu = new wxMenu();

	// File menu
	{
		// Popup Menus
		{
			ImportMenu->Append( IDM_IMPORT_NEW, TEXT("Into &New Map..."), TEXT("Import file into an empty map") );
			ImportMenu->Append( IDM_IMPORT_MERGE, TEXT("Into &Existing Map..."), TEXT("Merge the imported file with the currently loaded map") );

			ExportMenu->Append( IDM_EXPORT_ALL, TEXT("&All..."), TEXT("Export everything in the map") );
			ExportMenu->Append( IDM_EXPORT_SELECTED, TEXT("&Selected Only..."), TEXT("Export only the selected items") );
		}

		FileMenu->Append( IDM_NEW, TEXT("&New"), TEXT("Create a new map") );
		FileMenu->Append( IDM_OPEN, TEXT("&Open..."), TEXT("Open an existing map") );
		FileMenu->AppendSeparator();
		FileMenu->Append( IDM_SAVE, TEXT("&Save"), TEXT("Save the map") );
		FileMenu->Append( IDM_SAVE_AS, TEXT("Save &As..."), TEXT("Save the map with a new name") );
		FileMenu->AppendSeparator();
		FileMenu->Append( IDM_IMPORT, TEXT("&Import"), ImportMenu );
		FileMenu->Append( IDM_EXPORT, TEXT("&Export"), ExportMenu );
		FileMenu->AppendSeparator();
		FileMenu->Append( IDM_MRU_LIST, TEXT("Recent"), MRUMenu );
		FileMenu->AppendSeparator();
		FileMenu->Append( IDM_EXIT, TEXT("E&xit"), TEXT("Exits the editor") );

		Append( FileMenu, TEXT("&File") );
	}

	// Edit menu
	{
		EditMenu->Append( IDM_UNDO, TEXT("&Undo"), TEXT("Undo the last action") );
		EditMenu->Append( IDM_REDO, TEXT("&Redo"), TEXT("Redo the previously undone action") );
		EditMenu->AppendSeparator();
		EditMenu->AppendCheckItem( ID_EDIT_SHOW_WIDGET, TEXT("&Show Widget"), TEXT("Toggle display/use of the widget.") );
		EditMenu->AppendCheckItem( ID_EDIT_TRANSLATE, TEXT("&Translate"), TEXT("Use the translation widget.") );
		EditMenu->AppendCheckItem( ID_EDIT_ROTATE, TEXT("&Rotate"), TEXT("Use the rotation widget.") );
		EditMenu->AppendCheckItem( ID_EDIT_SCALE, TEXT("&Scale"), TEXT("Use the scaling widget.") );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDM_SEARCH, TEXT("&Search"), TEXT("Search for actors") );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDM_CUT, TEXT("Cu&t"), TEXT("Cut out the selection and put it into the clipboard") );
		EditMenu->Append( IDM_COPY, TEXT("&Copy"), TEXT("Copy the selection and put it into the clipboard") );
		EditMenu->Append( IDM_PASTE, TEXT("&Paste"), TEXT("Paste data in the clipboard into the map") );
		EditMenu->Append( IDM_DUPLICATE, TEXT("D&uplicate"), TEXT("Duplicate the selection") );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDM_DELETE, TEXT("&Delete"), TEXT("Delete the selection") );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDM_SELECT_NONE, TEXT("Select &None"), TEXT("Deselect everything") );
		EditMenu->Append( IDM_SELECT_ALL, TEXT("Select &All"), TEXT("Select everything") );
		EditMenu->Append( IDM_SELECT_INVERT, TEXT("&Invert Selection"), TEXT("Invert the current selection") );

		Append( EditMenu, TEXT("&Edit") );
	}

	// View menu
	{
		// Popup Menus
		{
			BrowserMenu->Append( IDM_BROWSER_ACTOR, TEXT("&Actor Classes"), TEXT("Show the actor classes browser") );
			BrowserMenu->Append( IDM_BROWSER_GROUP, TEXT("&Group"), TEXT("Show the group browser") );
			BrowserMenu->Append( IDM_BROWSER_LAYER, TEXT("&Layer"), TEXT("Show the layer browser") );
			BrowserMenu->AppendSeparator();
			BrowserMenu->Append( IDM_BROWSER_GENERIC, TEXT("&Generic"), TEXT("Show the generic browser") );
			BrowserMenu->AppendSeparator();
			BrowserMenu->Append( IDM_OPEN_KISMET, TEXT("&Kismet"), TEXT("Show Kismet") );

			for( INT x = 0 ; x < GApp->EditorFrame->ViewportConfigTemplates.Num() ; ++x )
			{
				FViewportConfig_Template* vct = GApp->EditorFrame->ViewportConfigTemplates(x);
				ViewportConfigMenu->AppendCheckItem( IDM_VIEWPORT_CONFIG_START+x, *(vct->Desc), *FString::Printf( TEXT("Switch to the '%s' viewport configuration"), *(vct->Desc) ) );
			}
		}

		ViewMenu->Append( IDM_LOG, TEXT("&Log Window"), TEXT("Show the log window") );
		ViewMenu->AppendSeparator();
		ViewMenu->Append( IDM_BROWSER, TEXT("&Browser Windows"), BrowserMenu );
		ViewMenu->AppendSeparator();
		ViewMenu->Append( IDM_ACTOR_PROPERTIES, TEXT("&Actor Properties"), TEXT("Shows the actor properties window") );
		ViewMenu->Append( IDM_SURFACE_PROPERTIES, TEXT("&Surface Properties"), TEXT("Shows the surface properties window") );
		ViewMenu->Append( IDM_LEVEL_PROPERTIES, TEXT("&Level Properties"), TEXT("Shows the level properties window") );
		ViewMenu->AppendSeparator();
		ViewMenu->Append( IDM_DRAG_GRID, TEXT("&Drag Grid"), GApp->EditorFrame->DragGridMenu );
		ViewMenu->Append( IDM_ROTATION_GRID, TEXT("&Rotation Grid"), GApp->EditorFrame->RotationGridMenu );
		ViewMenu->Append( IDM_VIEWPORT_CONFIG, TEXT("&Viewport Configuration"), ViewportConfigMenu );
		ViewMenu->AppendSeparator();
		ViewMenu->AppendCheckItem( IDM_BRUSHPOLYS, TEXT("&Brush Marker Polys"), TEXT("Toggle brush marker polys") );
		ViewMenu->AppendSeparator();
		ViewMenu->AppendCheckItem( IDM_FULLSCREEN, TEXT("&Fullscreen"), TEXT("Toggle fullscreen mode") );

		Append( ViewMenu, TEXT("&View") );
	}

	// Brush menu
	{
		BrushMenu->Append( IDM_BRUSH_ADD, TEXT("&Add"), TEXT("Add to BSP") );
		BrushMenu->Append( IDM_BRUSH_SUBTRACT, TEXT("&Subtract"), TEXT("Subtract from BSP") );
		BrushMenu->Append( IDM_BRUSH_INTERSECT, TEXT("&Intersect"), TEXT("Intersect with BSP") );
		BrushMenu->Append( IDM_BRUSH_DEINTERSECT, TEXT("&Deintersect"), TEXT("Deintersect from BSP") );
		BrushMenu->AppendSeparator();
		BrushMenu->Append( IDM_BRUSH_ADD_MOVER, TEXT("Add &Mover"), TEXT("Add a mover") );
		BrushMenu->Append( IDM_BRUSH_ADD_ANTIPORTAL, TEXT("Add A&ntiportal"), TEXT("Add an antiportal") );
		BrushMenu->Append( IDM_BRUSH_ADD_SPECIAL, TEXT("Add S&pecial"), TEXT("Add a special brush") );
		BrushMenu->AppendSeparator();
		BrushMenu->Append( ID_BRUSH_IMPORT, TEXT("&Import..."), TEXT("Import a brush") );
		BrushMenu->Append( ID_BRUSH_EXPORT, TEXT("&Export..."), TEXT("Export builder brush") );

		Append( BrushMenu, TEXT("&Brush") );

		// Volume menu
		{
			BrushMenu->AppendSeparator();

			INT ID = IDM_VolumeClasses_START;

			for( TObjectIterator<UClass> It ; It ; ++It )
			{
				if( It->IsChildOf(AVolume::StaticClass()) )
				{
					VolumeMenu->Insert( 0, ID, It->GetName(), TEXT(""), 0 );
					ID++;
				}
			}

			BrushMenu->Append( 900, TEXT("Add &Volume"), VolumeMenu );
		}
	}

	// Build menu
	{
		BuildMenu->Append( IDM_BUILD_PLAY, TEXT("&Play Level"), TEXT("Start the game and plays the current level") );
		BuildMenu->Append( IDM_BUILD_GEOMETRY, TEXT("&Geometry"), TEXT("Rebuild geometry") );
		BuildMenu->Append( IDM_BUILD_LIGHTING, TEXT("&Lighting"), TEXT("Rebuild lighting") );
		BuildMenu->Append( IDM_BUILD_AI_PATHS, TEXT("&AI Paths"), TEXT("Rebuild AI pathing") );
		BuildMenu->Append( IDM_BUILD_COVER, TEXT("Build &Cover"), TEXT("Rebuild Cover") );
		BuildMenu->Append( IDM_BUILD_ALL, TEXT("&Build All"), TEXT("Rebuild everything") );
		BuildMenu->AppendSeparator();
		BuildMenu->Append( IDM_BUILD_OPTIONS, TEXT("&Options"), TEXT("Show rebuild options dialog") );

		Append( BuildMenu, TEXT("&Build") );
	}

	// Tools menu
	{
		ToolsMenu->Append( ID_Tools2DEditor, TEXT("&2D Shape Editor"), TEXT("2D Shape Editing Tool") );
		ToolsMenu->Append( IDM_WIZARD_NEW_TERRAIN, TEXT("&New Terrain..."), TEXT("Add A New Terrain") );
		ToolsMenu->Append( IDMN_TOOL_CHECK_ERRORS, TEXT("&Check map for errors..."), TEXT("Check map for errors") );
		ToolsMenu->AppendSeparator();
		ToolsMenu->Append( IDM_REPLACESKELMESHACTORS, TEXT("Replace SkeletalMeshActors"), TEXT("") );

		Append( ToolsMenu, TEXT("&Tools") );
	}

	// Help Menu
	{
		HelpMenu->Append( IDMENU_ABOUTBOX, TEXT("&About UnrealEd..."), TEXT("Shows Information About UnrealEd") );

		Append( HelpMenu, TEXT("&Help") );
	}

	// MRU list

	MRUFiles = new FMRUList( TEXT("MRU"), MRUMenu );
}

WxMainMenu::~WxMainMenu()
{
	MRUFiles->WriteINI();
	delete MRUFiles;
}

/*-----------------------------------------------------------------------------
	WxMainContextMenuBase.
-----------------------------------------------------------------------------*/

WxMainContextMenuBase::WxMainContextMenuBase()
{
	ActorFactoryMenu = NULL;
	ActorFactoryMenuAdv = NULL;
	RecentClassesMenu = NULL;
}

WxMainContextMenuBase::~WxMainContextMenuBase()
{

}

void WxMainContextMenuBase::AppendAddActorMenu()
{
	// Add 'add actor of selected class' option.
	UClass* SelClass = GSelectionTools.GetTop<UClass>();
	if( SelClass )
	{
		FString wk = FString::Printf( TEXT("Add %s Here"), SelClass->GetName() );
		Append( ID_BackdropPopupAddClassHere, *wk, TEXT("") );
	}

	// Add an "add <class>" option here for the most recent actor classes that were selected in the level.

	RecentClassesMenu = new wxMenu();

	for( INT x = 0 ; x < GSelectionTools.SelectedClasses.Num() ; ++x )
	{
		FString wk = FString::Printf( TEXT("Add %s"), GSelectionTools.SelectedClasses(x)->GetName() );
		RecentClassesMenu->Append( ID_BackdropPopupAddLastSelectedClassHere_START+x, *wk, TEXT("") );
	}

	Append( IDMENU_SurfPopupAddRecentMenu, TEXT("Add Recent"), RecentClassesMenu );

	// Create actor factory entries.
	ActorFactoryMenu = new wxMenu();
	ActorFactoryMenuAdv = new wxMenu();

	for(INT i=0; i<GEditor->ActorFactories.Num(); i++)
	{
		UActorFactory* Factory = GEditor->ActorFactories(i);

		// The basic menu only shows factories that can be run without any intervention
		Factory->AutoFillFields();
		if( Factory->CanCreateActor() )
		{
			ActorFactoryMenu->Append( IDMENU_ActorFactory_Start+i, *(Factory->GetMenuName()), TEXT("") );
		}

		// The advanced menu shows all of them.
		ActorFactoryMenuAdv->Append( IDMENU_ActorFactoryAdv_Start+i, *(Factory->GetMenuName()), TEXT("") );
	}

	ActorFactoryMenu->AppendSeparator();
	ActorFactoryMenu->Append( IDMENU_SurfPopupAddActorAdvMenu, TEXT("All Templates"), ActorFactoryMenuAdv );

	Append( IDMENU_SurfPopupAddActorMenu, TEXT("Add Actor"), ActorFactoryMenu );

	AppendSeparator();
}

/*-----------------------------------------------------------------------------
	WxMainContextMenu.
-----------------------------------------------------------------------------*/

WxMainContextMenu::WxMainContextMenu()
	: WxMainContextMenuBase()
{
	// Look at what is selected and record information about it for later.

	UBOOL bHaveBrush = 0, bHaveStaticMesh = 0, bHaveSelectedActors = 0;
	AActor* FirstActor = NULL;
	INT NumActors = 0;
	INT NumNavPoints = 0;
	for( TSelectedActorIterator It( GEditor->Level ) ; It ; ++It )
	{
		bHaveSelectedActors = 1;

		FirstActor = *It;
		NumActors++;

		if( !bHaveBrush && It->IsA(ABrush::StaticClass()) )
		{
			bHaveBrush = 1;
		}
		else
		if( !bHaveStaticMesh && It->IsA(AStaticMeshActor::StaticClass()) )
		{
			bHaveStaticMesh = 1;
		}
		else
		if( It->IsA(ANavigationPoint::StaticClass()) )
		{
			NumNavPoints++;
		}
	}

	FGetInfoRet gir = GApp->EditorFrame->GetInfo( GUnrealEd->Level, GI_NUM_SELECTED | GI_CLASSNAME_SELECTED | GI_CLASS_SELECTED );

	if( bHaveSelectedActors > 0 )
	{
		Append( IDMENU_ActorPopupProperties, *FString::Printf( TEXT("%s &Properties (%i Selected)"), *(gir.String), gir.iValue ), TEXT("") );
		Append( ID_SyncGenericBrowser, TEXT("Sync Generic Browser"), TEXT(""), 0 );

		// If only 1 Actor is selected, and its referenced by Kismet, offer option to find it.
		if(NumActors == 1)
		{
			check(FirstActor);
			USequence* RootSeq = GEditor->Level->GameSequence;
			if(RootSeq && RootSeq->ReferencesObject(FirstActor))
			{
				Append( IDMENU_FindActorInKismet, *FString::Printf(TEXT("Find %s In Kismet"), FirstActor->GetName()), TEXT("") );
			}
		}

		AppendSeparator();
	}
	
	// add some path building options if nav points are selected
	if (NumNavPoints > 0)
	{
		PathMenu = new wxMenu();
		PathMenu->Append( IDMENU_ActorPopupPathPosition, TEXT("Auto Position"), TEXT("") );
		PathMenu->Append( IDMENU_ActorPopupPathRebuildSelected, TEXT("Rebuild Selected Path(s)"), TEXT("") );
		PathMenu->AppendSeparator();
		// if multiple path nodes are selected, add options to force/proscribe
		if (NumNavPoints > 1)
		{
			PathMenu->Append( IDMENU_ActorPopupPathProscribe, TEXT("Proscribe Paths"), TEXT("") );
			PathMenu->Append( IDMENU_ActorPopupPathForce, TEXT("Force Paths"), TEXT("") );
			PathMenu->AppendSeparator();
		}
		PathMenu->Append( IDMENU_ActorPopupPathClearProscribed, TEXT("Clear Proscribed Paths"), TEXT("") );
		PathMenu->Append( IDMENU_ActorPopupPathClearForced, TEXT("Clear Forced Paths"), TEXT("") );
		Append( IDMENU_ActorPopupPathMenu, TEXT("Path Options"), PathMenu );
		AppendSeparator();
	}
	else
	{
		PathMenu = NULL;
	}

	if(bHaveBrush)
	{
		OrderMenu = new wxMenu();
		OrderMenu->Append( IDMENU_ActorPopupToFirst, TEXT("To First"), TEXT("") );
		OrderMenu->Append( IDMENU_ActorPopupToLast, TEXT("To Last"), TEXT("") );
		Append( IDMENU_ActorPopupOrderMenu, TEXT("Order"), OrderMenu );

		PolygonsMenu = new wxMenu();
		PolygonsMenu->Append( IDMENU_ActorPopupToBrush, TEXT("To Brush"), TEXT("") );
		PolygonsMenu->Append( IDMENU_ActorPopupFromBrush, TEXT("From Brush"), TEXT("") );
		PolygonsMenu->AppendSeparator();
		PolygonsMenu->Append( IDMENU_ActorPopupMerge, TEXT("Merge"), TEXT("") );
		PolygonsMenu->Append( IDMENU_ActorPopupSeparate, TEXT("Separate"), TEXT("") );
		Append( IDMENU_ActorPopupPolysMenu, TEXT("Polygons"), PolygonsMenu );

		CSGMenu = new wxMenu();
		CSGMenu->Append( IDMENU_ActorPopupMakeAdd, TEXT("Additive"), TEXT("") );
		CSGMenu->Append( IDMENU_ActorPopupMakeSubtract, TEXT("Subtractive"), TEXT("") );
		Append( IDMENU_ActorPopupCSGMenu, TEXT("CSG"), CSGMenu );

		SolidityMenu = new wxMenu();
		SolidityMenu->Append( IDMENU_ActorPopupMakeSolid, TEXT("Solid"), TEXT("") );
		SolidityMenu->Append( IDMENU_ActorPopupMakeSemiSolid, TEXT("Semi Solid"), TEXT("") );
		SolidityMenu->Append( IDMENU_ActorPopupMakeNonSolid, TEXT("Non Solid"), TEXT("") );
		Append( IDMENU_ActorPopupSolidityMenu, TEXT("Solidity"), SolidityMenu );

		SelectMenu = new wxMenu();
		SelectMenu->Append( IDMENU_ActorPopupSelectBrushesAdd, TEXT("Adds/Solids"), TEXT("") );
		SelectMenu->Append( IDMENU_ActorPopupSelectBrushesSubtract, TEXT("Subtracts"), TEXT("") );
		SelectMenu->Append( IDMENU_ActorPopupSelectBrushesSemisolid, TEXT("Semi Solids"), TEXT("") );
		SelectMenu->Append( IDMENU_ActorPopupSelectBrushesNonsolid, TEXT("Non Solids"), TEXT("") );
		Append( IDMENU_ActorPopupSelectBrushMenu, TEXT("Select"), SelectMenu );

		Append( IDMENU_QuantizeVertices, TEXT("Snap To Grid"), TEXT("") );
		Append( IDMENU_ConvertToStaticMesh, TEXT("Convert to Static Mesh"), TEXT("") );

		// Volume menu
		{
			VolumeMenu = new wxMenu();

			INT ID = IDM_VolumeClasses_START;

			for( TObjectIterator<UClass> It ; It ; ++It )
			{
				if( It->IsChildOf(AVolume::StaticClass()) )
				{
					VolumeMenu->Insert( 0, ID, It->GetName(), TEXT(""), 0 );
					ID++;
				}
			}

			Append( 900, TEXT("&Add Volume"), VolumeMenu );
		}

		AppendSeparator();
	}
	else
	{	
		OrderMenu = NULL;
		PolygonsMenu = NULL;
		CSGMenu = NULL;
	}

	if( bHaveSelectedActors > 0 )
	{
		AlignMenu = new wxMenu();
		AlignMenu->Append( IDMENU_SnapToFloor, TEXT("Snap To Floor"), TEXT("") );
		AlignMenu->Append( IDMENU_AlignToFloor, TEXT("Align To Floor"), TEXT("") );
		Append( IDMENU_ActorPopupAlignMenu, TEXT("Align"), AlignMenu );

		Append( IDMENU_ActorPopupAlignCameras, TEXT("Align Cameras"), TEXT("") );

		AppendSeparator();
	}

	Append( IDMENU_ActorPopupSelectAllClass, *FString::Printf( TEXT("Select all %s"), *(gir.String) ), TEXT("") );

	if( bHaveStaticMesh )
	{
		Append( IDMENU_ActorPopupSelectMatchingStaticMeshes, TEXT("Select Matching Static Meshes"), TEXT("") );
	}
	Append( IDMENU_SaveBrushAsCollision, TEXT("Save Brush As Collision"), TEXT("") );

	AppendSeparator();

	AppendAddActorMenu();

	// Pivot menu

	PivotMenu = new wxMenu();

	PivotMenu->Append( IDMENU_ActorPopupResetPivot, TEXT("Reset"), TEXT("") );
	PivotMenu->Append( ID_BackdropPopupPivot, TEXT("Move Here"), TEXT("") );
	PivotMenu->Append( ID_BackdropPopupPivotSnapped, TEXT("Move Here (snapped)"), TEXT("") );
	Append( ID_BackdropPopupPivotMenu, TEXT("Pivot"), PivotMenu );

	AppendSeparator();

	// Transform menu

	TransformMenu = new wxMenu();

	TransformMenu->Append( IDMENU_ActorPopupMirrorX, TEXT("Mirror X Axis"), TEXT("") );
	TransformMenu->Append( IDMENU_ActorPopupMirrorY, TEXT("Mirror Y AXis"), TEXT("") );
	TransformMenu->Append( IDMENU_ActorPopupMirrorZ, TEXT("Mirror Z Axis"), TEXT("") );
	Append( ID_BackdropPopupTransformMenu, TEXT("Transform"), TransformMenu );

	AppendSeparator();

	Append( ID_BackDropPopupPlayFromHere, TEXT("Play From Here"), TEXT("") );
}

WxMainContextMenu::~WxMainContextMenu()
{

}

/*-----------------------------------------------------------------------------
	WxMainContextSurfaceMenu.
-----------------------------------------------------------------------------*/

WxMainContextSurfaceMenu::WxMainContextSurfaceMenu()
	: WxMainContextMenuBase()
{
	FGetInfoRet gir = GApp->EditorFrame->GetInfo( GUnrealEd->Level, GI_NUM_SURF_SELECTED );

	FString TmpString;

	TmpString = FString::Printf( TEXT("Surface Properties (%i Selected)"), gir.iValue );
	Append( IDM_SURFACE_PROPERTIES, *TmpString, TEXT("") );
	Append( ID_SyncGenericBrowser, TEXT("Sync Generic Browser"), TEXT(""), 0 );

	AppendSeparator();

	AppendAddActorMenu();

	SelectSurfMenu = new wxMenu();
	SelectSurfMenu->Append( ID_SurfPopupSelectMatchingGroups, TEXT("Matching Groups"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupSelectMatchingItems, TEXT("Matching Items"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupSelectMatchingBrush, TEXT("Matching Brush"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupSelectMatchingTexture, TEXT("Matching Texture"), TEXT("") );
	SelectSurfMenu->AppendSeparator();
	SelectSurfMenu->Append( ID_SurfPopupSelectAllAdjacents, TEXT("All Adjacents"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupSelectAdjacentCoplanars, TEXT("Adjacent Coplanars"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupSelectAdjacentWalls, TEXT("Adjacent Walls"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupSelectAdjacentFloors, TEXT("Adjacent Floors"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupSelectAdjacentSlants, TEXT("Adjacent Slants"), TEXT("") );
	SelectSurfMenu->AppendSeparator();
	SelectSurfMenu->Append( ID_SurfPopupSelectReverse, TEXT("Reverse"), TEXT("") );
	SelectSurfMenu->AppendSeparator();
	SelectSurfMenu->Append( ID_SurfPopupMemorize, TEXT("Memorize Set"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupRecall, TEXT("Recall Memory"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupOr, TEXT("Or With Memory"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupAnd, TEXT("And With Memory"), TEXT("") );
	SelectSurfMenu->Append( ID_SurfPopupXor, TEXT("Xor With Memory"), TEXT("") );
	Append( IDMNEU_SurfPopupSelectSurfMenu, TEXT("Select Surfaces"), SelectSurfMenu );

	Append( ID_EditSelectAllSurfs, TEXT("Select All Surface"), TEXT("") );
	Append( ID_EditSelectNone, TEXT("Select None"), TEXT("") );

	AppendSeparator();


	ExtrudeMenu = new wxMenu();
	ExtrudeMenu->Append( ID_SurfPopupExtrude16, TEXT("16"), TEXT("") );
	ExtrudeMenu->Append( ID_SurfPopupExtrude32, TEXT("32"), TEXT("") );
	ExtrudeMenu->Append( ID_SurfPopupExtrude64, TEXT("64"), TEXT("") );
	ExtrudeMenu->Append( ID_SurfPopupExtrude128, TEXT("128"), TEXT("") );
	ExtrudeMenu->Append( ID_SurfPopupExtrude256, TEXT("256"), TEXT("") );
	ExtrudeMenu->Append( ID_SurfPopupExtrudeCustom, TEXT("Custom..."), TEXT("") );
	Append( IDMENU_SurfPopupExtrudeMenu, TEXT("Extrude"), ExtrudeMenu );

	Append( ID_SurfPopupBevel, TEXT("Bevel..."), TEXT("") );

	AppendSeparator();

	UMaterialInstance* mi = GSelectionTools.GetTop<UMaterialInstance>();
	FString Wk = TEXT("Apply Material");
	if( mi )
	{
		Wk = FString::Printf( TEXT("Apply Material : %s"), mi->GetName() );
	}
	Append( ID_SurfPopupApplyMaterial, *Wk, TEXT("") );
	Append( ID_SurfPopupReset, TEXT("Reset"), TEXT("") );

	AlignmentMenu = new wxMenu();
	AlignmentMenu->Append( ID_SurfPopupUnalign, TEXT("Default"), TEXT("") );
	AlignmentMenu->Append( ID_SurfPopupAlignPlanarAuto, TEXT("Planar"), TEXT("") );
	AlignmentMenu->Append( ID_SurfPopupAlignPlanarWall, TEXT("PlanarWall"), TEXT("") );
	AlignmentMenu->Append( ID_SurfPopupAlignPlanarFloor, TEXT("Planar Floor"), TEXT("") );
	AlignmentMenu->Append( ID_SurfPopupAlignBox, TEXT("Box"), TEXT("") );
	AlignmentMenu->Append( ID_SurfPopupAlignFace, TEXT("Face"), TEXT("") );
	Append( IDMENU_SurfPopupAlignmentMenu, TEXT("Alignment"), AlignmentMenu );

	AppendSeparator();

	Append( ID_BackDropPopupPlayFromHere, TEXT("Play From Here"), TEXT("") );
}

WxMainContextSurfaceMenu::~WxMainContextSurfaceMenu()
{

}

/*-----------------------------------------------------------------------------
	WxDragGridMenu.
-----------------------------------------------------------------------------*/

WxDragGridMenu::WxDragGridMenu()
{
	// Drag Grid menu
	{
		AppendCheckItem( IDM_DRAG_GRID_TOGGLE, TEXT("&Use Drag Grid"), TEXT("Toggle the use of the drag grid") );
		AppendSeparator();
		AppendCheckItem( IDM_DRAG_GRID_1, TEXT("1"), TEXT("Set the drag grid to 1") );
		AppendCheckItem( IDM_DRAG_GRID_2, TEXT("2"), TEXT("Set the drag grid to 2") );
		AppendCheckItem( IDM_DRAG_GRID_4, TEXT("4"), TEXT("Set the drag grid to 4") );
		AppendCheckItem( IDM_DRAG_GRID_8, TEXT("8"), TEXT("Set the drag grid to 8") );
		AppendCheckItem( IDM_DRAG_GRID_16, TEXT("16"), TEXT("Set the drag grid to 16") );
		AppendCheckItem( IDM_DRAG_GRID_32, TEXT("32"), TEXT("Set the drag grid to 32") );
		AppendCheckItem( IDM_DRAG_GRID_64, TEXT("64"), TEXT("Set the drag grid to 64") );
		AppendCheckItem( IDM_DRAG_GRID_128, TEXT("128"), TEXT("Set the drag grid to 128") );
		AppendCheckItem( IDM_DRAG_GRID_256, TEXT("256"), TEXT("Set the drag grid to 256") );
		AppendCheckItem( IDM_DRAG_GRID_512, TEXT("512"), TEXT("Set the drag grid to 512") );
		AppendCheckItem( IDM_DRAG_GRID_1024, TEXT("1024"), TEXT("Set the drag grid to 1024") );
	}
}

WxDragGridMenu::~WxDragGridMenu()
{
}

/*-----------------------------------------------------------------------------
	WxRotationGridMenu.
-----------------------------------------------------------------------------*/

WxRotationGridMenu::WxRotationGridMenu()
{
	// Rotation Grid menu
	{
		AppendCheckItem( IDM_ROTATION_GRID_TOGGLE, TEXT("&Use Rotation Grid"), TEXT("Toggle the use of the rotation grid") );
		AppendSeparator();
		AppendCheckItem( IDM_ROTATION_GRID_512, TEXT("2 Degrees"), TEXT("Set the rotation grid to 2 degrees") );
		AppendCheckItem( IDM_ROTATION_GRID_1024, TEXT("5 Degrees"), TEXT("Set the rotation grid to 5 degrees") );
		AppendCheckItem( IDM_ROTATION_GRID_4096, TEXT("22 Degrees"), TEXT("Set the rotation grid to 22 degrees") );
		AppendCheckItem( IDM_ROTATION_GRID_8192, TEXT("45 Degrees"), TEXT("Set the rotation grid to 45 degrees") );
		AppendCheckItem( IDM_ROTATION_GRID_16384, TEXT("90 Degrees"), TEXT("Set the rotation grid to 90 degrees") );
	}
}

WxRotationGridMenu::~WxRotationGridMenu()
{
}

/*-----------------------------------------------------------------------------
	WxMBGroupBrowser.
-----------------------------------------------------------------------------*/

WxMBGroupBrowser::WxMBGroupBrowser()
{
	wxMenu* EditMenu = new wxMenu();
	wxMenu* ViewMenu = new wxMenu();

	// Edit menu
	{
		EditMenu->Append( IDMN_GB_NEW_GROUP, TEXT("&New..."), TEXT("Create a new group") );
		EditMenu->Append( IDMN_GB_RENAME_GROUP, TEXT("&Rename..."), TEXT("Rename the selected group") );
		EditMenu->Append( IDMN_GB_DELETE_GROUP, TEXT("&Delete"), TEXT("Delete the selected groups") );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDMN_GB_ADD_TO_GROUP, TEXT("&Add Selected Actors to Group"), TEXT("Add all selected actors to selected groups") );
		EditMenu->Append( IDMN_GB_DELETE_FROM_GROUP, TEXT("De&lete Selected Actors from Group"), TEXT("Delete selected actors from selected groups") );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDMN_GB_SELECT, TEXT("&Select Actors"), TEXT("Select all the actors belonging to this group") );
		EditMenu->Append( IDMN_GB_DESELECT, TEXT("&Deselect Actors"), TEXT("Deselect all the actors belonging to this group") );
	}

	// View menu
	{
		ViewMenu->Append( IDMN_GB_REFRESH, TEXT("&Refresh"), TEXT("Refresh the window") );
	}

	Append( EditMenu, TEXT("&Edit") );
	Append( ViewMenu, TEXT("&View") );
}

WxMBGroupBrowser::~WxMBGroupBrowser()
{
}

/*-----------------------------------------------------------------------------
	WxMBLayerBrowser.
-----------------------------------------------------------------------------*/

WxMBLayerBrowser::WxMBLayerBrowser()
{
	wxMenu* EditMenu = new wxMenu();

	// Edit menu
	{
		EditMenu->Append( IDM_LB_NEW, TEXT("&New..."), TEXT("Create a new layer") );
		EditMenu->Append( IDM_LB_DELETE, TEXT("&Delete"), TEXT("Delete current layer") );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDM_LB_ADDTOLAYER, TEXT("&Add Selected Actors"), TEXT("Add selected actors to this layer") );
		EditMenu->Append( IDM_LB_REMOVEFROMLAYER, TEXT("&Remove Selected Actors"), TEXT("Remove selected actors from this layer") );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDM_LB_SELECT, TEXT("&Select Actors"), TEXT("Select the actors inside of this layer") );
	}

	Append( EditMenu, TEXT("&Edit") );
}

WxMBLayerBrowser::~WxMBLayerBrowser()
{
}

/*-----------------------------------------------------------------------------
	WxMBActorBrowser.
-----------------------------------------------------------------------------*/

WxMBActorBrowser::WxMBActorBrowser()
{
	// File menu
	wxMenu* FileMenu = new wxMenu();

	FileMenu->Append( IDMN_AB_FileOpen, TEXT("&Open..."), TEXT("") );
	FileMenu->Append( IDMN_AB_EXPORT_ALL, TEXT("&Export All Scripts"), TEXT("") );

	Append( FileMenu, TEXT("&File") );
}

WxMBActorBrowser::~WxMBActorBrowser()
{
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowser.
-----------------------------------------------------------------------------*/

WxMBGenericBrowser::WxMBGenericBrowser()
{
	wxMenu* FileMenu = new wxMenu();
	wxMenu* ViewMenu = new wxMenu();

	// File menu
	{
		FileMenu->Append( IDM_NEW, TEXT("&New..."), TEXT("") );
		FileMenu->AppendSeparator();
		FileMenu->Append( IDMN_FileOpen, TEXT("&Open..."), TEXT("") );
		FileMenu->Append( IDMN_FileSave, TEXT("&Save..."), TEXT("") );
		FileMenu->AppendSeparator();
		FileMenu->Append( IDM_IMPORT, TEXT("&Import..."), TEXT("") );
		FileMenu->Append( IDM_EXPORT, TEXT("&Export..."), TEXT("") );
	}


	// View menu
	{
		ViewMenu->Append( IDMN_REFRESH, TEXT("&Refresh"), TEXT("") );
	}

	Append( FileMenu, TEXT("&File") );
	Append( ViewMenu, TEXT("&View") );
}

WxMBGenericBrowser::~WxMBGenericBrowser()
{
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext.
-----------------------------------------------------------------------------*/

void WxMBGenericBrowserContext::AppendObjectMenu()
{
	AppendSeparator();
	Append(IDMN_ObjectContext_CopyReference,TEXT("&Copy Reference"),TEXT(""));
	AppendSeparator();
	Append(IDMN_ObjectContext_Duplicate,TEXT("&Duplicate"),TEXT(""));
	Append(IDMN_ObjectContext_Rename,TEXT("&Rename"),TEXT(""));
	Append(IDMN_ObjectContext_Delete,TEXT("De&lete"),TEXT(""));
	Append(IDMN_ObjectContext_Export,TEXT("&Export to File"),TEXT(""));
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_Object.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_Object::WxMBGenericBrowserContext_Object()
{
	Append(IDMN_ObjectContext_Properties,TEXT("&Properties..."),TEXT(""));
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_Material.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_Material::WxMBGenericBrowserContext_Material()
{
	Append(IDMN_ObjectContext_Editor,TEXT("&Material Editor"),TEXT(""));
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_Texture.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_Texture::WxMBGenericBrowserContext_Texture()
{
	Append(IDMN_ObjectContext_Editor,TEXT("&Texture Viewer"),TEXT(""));
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_StaticMesh.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_StaticMesh::WxMBGenericBrowserContext_StaticMesh()
{
	Append(IDMN_ObjectContext_Editor,TEXT("&Static Mesh Viewer"),TEXT(""));
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_Sound.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_Sound::WxMBGenericBrowserContext_Sound()
{
	Append(IDMN_ObjectContext_Properties,TEXT("&Properties..."),TEXT(""));
	AppendSeparator();
	Append(IDMN_ObjectContext_Custom_START,TEXT("Pl&ay"),TEXT(""));
	Append(IDMN_ObjectContext_Custom_START+1,TEXT("&Stop"),TEXT(""));
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_SoundCue.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_SoundCue::WxMBGenericBrowserContext_SoundCue()
{
	Append(IDMN_ObjectContext_Editor,TEXT("&Sound Cue Editor"),TEXT(""));
	AppendSeparator();
	Append(IDMN_ObjectContext_Custom_START,TEXT("Pl&ay"),TEXT(""));
	Append(IDMN_ObjectContext_Custom_START+1,TEXT("&Stop"),TEXT(""));
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_PhysicsAsset.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_PhysicsAsset::WxMBGenericBrowserContext_PhysicsAsset()
{
	Append(IDMN_ObjectContext_Editor,TEXT("&Physics Asset Editor (PhAT)"),TEXT(""));
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
WxMBGenericBrowserContext_Skeletal.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_Skeletal::WxMBGenericBrowserContext_Skeletal()
{
	Append(IDMN_ObjectContext_Editor,TEXT("&AnimSet Viewer"),TEXT("")); 
	Append(IDMN_ObjectContext_Custom_START,TEXT("&Create New Physics Asset"),TEXT("")); 
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_ParticleSystem.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_ParticleSystem::WxMBGenericBrowserContext_ParticleSystem()
{
	Append(IDMN_ObjectContext_Editor,TEXT("&Editor (Cascade)"),TEXT("")); 
	Append(IDMN_ObjectContext_Properties,TEXT("&Properties..."),TEXT(""));
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
	WxMBGenericBrowserContext_AnimSet.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_AnimSet::WxMBGenericBrowserContext_AnimSet()
{
	Append(IDMN_ObjectContext_Editor,TEXT("&AnimSet Viewer"),TEXT(""));
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
	XMBGenericBrowserContext_AnimTree.
-----------------------------------------------------------------------------*/

WxMBGenericBrowserContext_AnimTree::WxMBGenericBrowserContext_AnimTree()
{
	Append(IDMN_ObjectContext_Editor,TEXT("&AnimTree Editor"),TEXT(""));
	AppendObjectMenu();
}

/*-----------------------------------------------------------------------------
	WxMBMaterialEditorContext_OutputHandle.
-----------------------------------------------------------------------------*/

WxMBMaterialEditorContext_OutputHandle::WxMBMaterialEditorContext_OutputHandle()
{
	Append( IDM_DELETE, TEXT("Delete"), TEXT("Delete expression") );
}

WxMBMaterialEditorContext_OutputHandle::~WxMBMaterialEditorContext_OutputHandle()
{
}

/*-----------------------------------------------------------------------------
	WxMBMaterialEditorContext_Base.
-----------------------------------------------------------------------------*/

int CDECL ClassNameCompare(const void *A, const void *B)
{
	return appStricmp((*(UClass**)A)->GetName(),(*(UClass**)B)->GetName());
}

WxMBMaterialEditorContext_Base::WxMBMaterialEditorContext_Base()
{
	TArray<UClass*>	MaterialExpressionClasses;

	// Get a list of all expression types

	for( TMap<INT,UClass*>::TIterator It(GApp->ShaderExpressionMap) ; It ; ++It )
		MaterialExpressionClasses.AddItem(It.Value());

	// Sort the list

	appQsort( &MaterialExpressionClasses(0), MaterialExpressionClasses.Num(), sizeof(UClass*), ClassNameCompare );

	// Create a menu based on the sorted list

	for( INT x = 0 ; x < MaterialExpressionClasses.Num() ; ++x )
		Append(
			*GApp->ShaderExpressionMap.FindKey(MaterialExpressionClasses(x)),
			*FString::Printf(TEXT("New %s"),*FString(MaterialExpressionClasses(x)->GetName()).Mid(appStrlen(TEXT("MaterialExpression")))),
			TEXT("")
			);
}

WxMBMaterialEditorContext_Base::~WxMBMaterialEditorContext_Base()
{
}

/*-----------------------------------------------------------------------------
	WxMBMaterialEditorContext_TextureSample.
-----------------------------------------------------------------------------*/

WxMBMaterialEditorContext_TextureSample::WxMBMaterialEditorContext_TextureSample()
{
	Append( IDM_USE_CURRENT_TEXTURE, TEXT("Use Current Texture"), TEXT("Use Current Texture") );
}

WxMBMaterialEditorContext_TextureSample::~WxMBMaterialEditorContext_TextureSample()
{
}

/*-----------------------------------------------------------------------------
	WxStaticMeshEditMenu.
-----------------------------------------------------------------------------*/

WxStaticMeshEditMenu::WxStaticMeshEditMenu()
{
	wxMenu* ViewMenu = new wxMenu();
	wxMenu* ToolMenu = new wxMenu();
	wxMenu* CollisionMenu = new wxMenu();

	// View menu
	{
		ViewMenu->AppendCheckItem( ID_SHOW_OPEN_EDGES, TEXT("&Open Edges"), TEXT("") );
		ViewMenu->AppendCheckItem( ID_SHOW_WIREFRAME, TEXT("&Wireframe"), TEXT("") );
		ViewMenu->AppendCheckItem( ID_SHOW_BOUNDS, TEXT("&Bounds"), TEXT("") );
		ViewMenu->AppendCheckItem( IDMN_COLLISION, TEXT("&Collision"), TEXT("") );
		ViewMenu->AppendSeparator();
		ViewMenu->AppendCheckItem( ID_LOCK_CAMERA, TEXT("&Lock Camera"), TEXT("") );
	}

	// Tool menu
	{
		ToolMenu->Append( ID_SAVE_THUMBNAIL, TEXT("&Save Thumbnail Angle"), TEXT("") );
	}

	// Collision menu
	{
		CollisionMenu->Append( IDMN_SME_COLLISION_6DOP, TEXT("&6DOP simplified collision"), TEXT("") );
		CollisionMenu->Append( IDMN_SME_COLLISION_10DOPX, TEXT("10DOP-&X simplified collision"), TEXT("") );
		CollisionMenu->Append( IDMN_SME_COLLISION_10DOPY, TEXT("10DOP-&Y simplified collision"), TEXT("") );
		CollisionMenu->Append( IDMN_SME_COLLISION_10DOPZ, TEXT("10DOP-&Z simplified collision"), TEXT("") );
		CollisionMenu->Append( IDMN_SME_COLLISION_18DOP, TEXT("1&8DOP simplified collision"), TEXT("") );
		CollisionMenu->Append( IDMN_SME_COLLISION_26DOP, TEXT("&26DOP simplified collision"), TEXT("") );
		CollisionMenu->Append( IDMN_SME_COLLISION_SPHERE, TEXT("Sphere simplified collision"), TEXT("") );
	}

	Append( ViewMenu, TEXT("&View") );
	Append( ToolMenu, TEXT("&Tool") );
	Append( CollisionMenu, TEXT("&Collision") );
}

WxStaticMeshEditMenu::~WxStaticMeshEditMenu()
{
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
