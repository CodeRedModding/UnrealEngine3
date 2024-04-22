/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __NEWMATERIALEDITOR_H__
#define __NEWMATERIALEDITOR_H__

#include "UnLinkedObjEditor.h"
#include "MaterialEditorBase.h"

// Forward declarations.
class FMaterialEditorPreviewVC;
class FScopedTransaction;
class UMaterial;
class UMaterialEditorMeshComponent;
class UMaterialEditorOptions;
class UMaterialExpression;
class WxMaterialEditorToolBar;
class WxMaterialExpressionList;
class WxMaterialFunctionLibraryList;
class WxMaterialEditorSourceWindow;
class WxPropertyWindow;
struct FLinkedObjDrawInfo;
class FExpressionPreview;

/** Static array of categorized material expression classes. */
struct FCategorizedMaterialExpressionNode
{
	FString	CategoryName;
	TArray<UClass*> MaterialExpressionClasses;
};

/** Used in the material function library to store function information. */
struct FFunctionEntryInfo
{
	FString Path;
	FString ToolTip;

	friend UBOOL operator==(const FFunctionEntryInfo& A,const FFunctionEntryInfo& B)
	{
		return A.Path == B.Path;
	}
};

/** Used in the material function library to store category information. */
struct FCategorizedMaterialFunctions
{
	FString	CategoryName;
	TArray<FFunctionEntryInfo> FunctionInfos;
};

/**
 * The material editor.
 */
class WxMaterialEditor : public WxMaterialEditorBase, public FNotifyHook, public FLinkedObjEdNotifyInterface, public FSerializableObject, public FDockingParent
{
public:
	/** The set of selected material expressions. */
	TArray<UMaterialExpression*> SelectedExpressions;

	/** The set of selected comments. */
	TArray<UMaterialExpressionComment*> SelectedComments;

	/** Object containing selected connector. */
	UObject* ConnObj;

	/** Type of selected connector. */
	INT ConnType;

	/** Index of selected connector. */
	INT ConnIndex;

	/** 
	 * TRUE if the list of UMaterialExpression-derived classes shared between material
	 * editors has already been created.
	 */
	static UBOOL bMaterialExpressionClassesInitialized;

	/** Whether FunctionsFromAssetDatabase has been initialized this run. */
	static UBOOL bMaterialFunctionLibraryInitialized;

	/** If FALSE, sort material expressions using categorized menus. */
	UBOOL	bUseUnsortedMenus;

	/** Static array of UMaterialExpression-derived classes, shared between all material editor instances. */
	static TArray<UClass*>			MaterialExpressionClasses;

	static TArray<FCategorizedMaterialExpressionNode> CategorizedMaterialExpressionClasses;
	static TArray<UClass*> FavoriteExpressionClasses;
	static TArray<UClass*> UnassignedExpressionClasses;

	/** Global list of functions retrieved from the asset database. */
	static TArray<FCategorizedMaterialFunctions> FunctionsFromAssetDatabase;

	/** List of functions from the asset database, and other functions in memory, sorted by category. */
	TArray<FCategorizedMaterialFunctions> CategorizedFunctionLibrary;

	/** Map of active material editors, used to prevent editing the same material or material function with multiple editors simultaneously. */
	static TMap<FString, WxMaterialEditor*> ActiveMaterialEditors;

	/** The material applied to the preview mesh. */
	UMaterial*						Material;

	/** The source material being edited by this material editor. Only will be updated when Material's settings are copied over this material */
	UMaterial*						OriginalMaterial;

	/** 
	 * Material function being edited.  
	 * If this is non-NULL, a function is being edited and Material is being used to preview it.
	 */
	UMaterialFunction*				EditMaterialFunction;

	/** The material applied to the preview mesh when previewing an expression. */
	UMaterial*						ExpressionPreviewMaterial;

	/** The expression currently being previewed.  This is NULL when not in expression preview mode. */
	UMaterialExpression*			PreviewExpression;

	/** The editor's property window, displaying a property window for the material or any selected material expression node(s). */
	WxPropertyWindowHost*			PropertyWindow;

	/** The viewport client for the material editor's linked object editor. */
	FLinkedObjViewportClient*		LinkedObjVC;

	/** The background texture displayed in the linked object editor region. */
	UTexture2D*						BackgroundTexture;

	/** The material editor's toolbar. */
	WxMaterialEditorToolBar*		ToolBar;

	/** The material editor's expression list */
	WxMaterialExpressionList*		MaterialExpressionList;

	/** The material function library window. */
	WxMaterialFunctionLibraryList*	MaterialFunctionLibraryList;

	/** Component for the preview static mesh. */
	UMaterialEditorMeshComponent*			PreviewMeshComponent;

	/** Component for the preview skeletal mesh. */
	UMaterialEditorSkeletalMeshComponent*	PreviewSkeletalMeshComponent;

	/** View Source window */
	WxMaterialEditorSourceWindow*		SourceWindow;


	WxMaterialEditor(wxWindow* InParent, wxWindowID InID, UMaterial* InMaterial);
	WxMaterialEditor(wxWindow* InParent, wxWindowID InID, UMaterialFunction* InMaterialFunction);

	/** Initializes material editor state, regardless of whether a material or material function is being edited. */
	void Initialize(UMaterial* Material);

	virtual ~WxMaterialEditor();

	/** Updates CategorizedFunctionLibrary from functions in memory and then rebuilds the function library window. */
	void RebuildMaterialFunctionLibrary();

	/**
	 * Load editor settings from disk (docking state, window pos/size, option state, etc).
	 */
	void LoadEditorSettings();

	/**
	 * Saves editor settings to disk (docking state, window pos/size, option state, etc).
	 */
	void SaveEditorSettings();

	/**
	 * Refreshes the viewport containing the material expression graph.
	 */
	void RefreshExpressionViewport();

	/**
	 * Refreshes material expression previews.  Refreshes all previews if bAlwaysRefreshAllPreviews is TRUE.
	 * Otherwise, refreshes only those previews that have a bRealtimePreview of TRUE.
	 */
	void RefreshExpressionPreviews();

	/**
	 * Refreshes all material expression previews, regardless of whether or not realtime previews are enabled.
	 */
	void ForceRefreshExpressionPreviews();

	/**
	 * Refreshes the preview for the specified material expression.  Does nothing if the specified expression
	 * has a bRealtimePreview of FALSE.
	 *
	 * @param	MaterialExpression		The material expression to update.
	 */
	void RefreshExpressionPreview(UMaterialExpression* MaterialExpression, UBOOL bRecompile);

	/**
	 * Run the HLSL material translator and refreshes the View Source window.
	 */
	void RefreshSourceWindowMaterial();

	/**
	 * Recompiles the material used in the preview window.
	 */
	void UpdatePreviewMaterial( );

	/** Sets the expression to be previewed. */
	void SetPreviewExpression(UMaterialExpression* PreviewExpression);

	/**
	 * Draws the root node -- that is, the node corresponding to the final material.
	 */
	void DrawMaterialRoot(UBOOL bIsSelected, FCanvas* Canvas);

	/**
	 * Render connections between the material's inputs and the material expression outputs connected to them.
	 */
	void DrawMaterialRootConnections(FCanvas* Canvas);

	/**
	 * Draws the specified material expression node.
	 */
	void DrawMaterialExpression(UMaterialExpression* MaterialExpression, UBOOL bExpressionSelected, FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo);

	/**
	 * Render connectors between this material expression's inputs and the material expression outputs connected to them.
	 */
	void DrawMaterialExpressionConnections(UMaterialExpression* MaterialExpression, FCanvas* Canvas);

	/**
	 * Draws comments for the specified material expression node.
	 */
	void DrawMaterialExpressionComments(UMaterialExpression* MaterialExpression, FCanvas* Canvas);

	/** Draws tooltips for material expressions. */
	void DrawMaterialExpressionToolTips(const TArray<FLinkedObjDrawInfo>& LinkedObjDrawInfos, FCanvas* Canvas);

	/**
	 * Draws UMaterialExpressionComment nodes.
	 */
	void DrawCommentExpressions(FCanvas* Canvas);

	/**
	 * Highlights any active logic connectors in the specified linked object.
	 */
	void HighlightActiveConnectors(FLinkedObjDrawInfo &ObjInfo);

	/** @return Returns whether the specified connector is currently highlighted or not. */
	UBOOL IsConnectorHighlighted(UObject* Object, BYTE ConnType, INT ConnIndex);

	/** @return Returns whether the specified connector is currently marked or not. */
	UBOOL IsConnectorMarked(UObject* Object, BYTE ConnType, INT ConnIndex);

	/**
	 * Creates a new material expression of the specified class.  Will automatically connect output 0
	 * of the new expression to an input tab, if one was clicked on.
	 *
	 * @param	NewExpressionClass		The type of material expression to add.  Must be a child of UMaterialExpression.
	 * @param	bAutoConnect			If TRUE, connect the new material expression to the most recently selected connector, if possible.
	 * @param	bAutoSelect				If TRUE, deselect all expressions and select the newly created one.
	 * @param	NodePos					Position of the new node.
	 */
	UMaterialExpression* CreateNewMaterialExpression(UClass* NewExpressionClass, UBOOL bAutoConnect, UBOOL bAutoSelect, UBOOL bAutoAssignResource, const FIntPoint& NodePos);

	// FLinkedObjViewportClient interface

	virtual void DrawObjects(FViewport* Viewport, FCanvas* Canvas);

	/**
	 * Called when the user right-clicks on an empty region of the expression viewport.
	 */
	virtual void OpenNewObjectMenu();

	/**
	 * Called when the user right-clicks on an object in the viewport.
	 */
	virtual void OpenObjectOptionsMenu();

	/**
	 * Called when the user right-clicks on an object connector in the viewport.
	 */
	virtual void OpenConnectorOptionsMenu();

	/**
	 * Updates the editor's property window to contain material expression properties if any are selected.
	 * Otherwise, properties for the material are displayed.
	 */
	virtual void UpdatePropertyWindow();

	/**
	 * Deselects all selected material expressions and comments.
	 */
	virtual void EmptySelection();


	/**
	 * Add the specified object to the list of selected objects
	 */
	virtual void AddToSelection(UObject* Obj);

	/**
	 * Remove the specified object from the list of selected objects.
	 */
	virtual void RemoveFromSelection(UObject* Obj);

	/**
	 * Checks whether the specified object is currently selected.
	 *
	 * @return	TRUE if the specified object is currently selected
	 */
	virtual UBOOL IsInSelection(UObject* Obj) const;

	/**
	 * Returns the number of selected objects
	 */
	virtual INT GetNumSelected() const;

	/**
	 * Called when the user clicks on an unselected link connector.
	 *
	 * @param	Connector	the link connector that was clicked on
	 */
	virtual void SetSelectedConnector(FLinkedObjectConnector& Connector);

	/**
	 * Gets the position of the selected connector.
	 *
	 * @return	an FIntPoint that represents the position of the selected connector, or (0,0)
	 *			if no connectors are currently selected
	 */
	virtual FIntPoint GetSelectedConnLocation(FCanvas* Canvas);

	/**
	 * Gets the EConnectorHitProxyType for the currently selected connector
	 *
	 * @return	the type for the currently selected connector, or 0 if no connector is selected
	 */
	virtual INT GetSelectedConnectorType();

	/**
	 * Makes a connection between connectors.
	 */
	virtual void MakeConnectionToConnector(FLinkedObjectConnector& Connector);

	/** Makes a connection between the currently selected connector and the passed in end point. */
	void MakeConnectionToConnector(UObject* EndObject, INT EndConnType, INT EndConnIndex);

	/**
	 * Called when the user releases the mouse over a link connector and is holding the ALT key.
	 * Commonly used as a shortcut to breaking connections.
	 *
	 * @param	Connector	The connector that was ALT+clicked upon.
	 */
	virtual void AltClickConnector(FLinkedObjectConnector& Connector);

	/**
	 * Called when the user double-clicks an object in the viewport
	 *
	 * @param	Obj		the object that was double-clicked on
	 */
	virtual void DoubleClickedObject(UObject* Obj);

	/**
	 * Called when double-clicking a connector.
	 * Used to pan the connection's link into view
	 *
	 * @param	Connector	The connector that was double-clicked
	 */
	virtual void DoubleClickedConnector(FLinkedObjectConnector& Connector);

	/**
	 * Called when the user performs a draw operation while objects are selected.
	 *
	 * @param	DeltaX	the X value to move the objects
	 * @param	DeltaY	the Y value to move the objects
	 */
	virtual void MoveSelectedObjects(INT DeltaX, INT DeltaY);

	virtual void EdHandleKeyInput(FViewport* Viewport, FName Key, EInputEvent Event);

	/**
	/**
	 *	Handling for dragging on 'special' hit proxies. Should only have 1 object selected when this is being called. 
	 *	In this case used for handling resizing handles on Comment objects. 
	 *
	 *	@param	DeltaX			Horizontal drag distance this frame (in pixels)
	 *	@param	DeltaY			Vertical drag distance this frame (in pixels)
	 *	@param	SpecialIndex	Used to indicate different classes of action. @see HLinkedObjProxySpecial.
	 */
	virtual void SpecialDrag( INT DeltaX, INT DeltaY, INT NewX, INT NewY, INT SpecialIndex );

	/**
	 * Called once the user begins a drag operation.  Transactions expression, comment position.
	 */
	virtual void BeginTransactionOnSelected();

	/**
	 * Called when the user releases the mouse button while a drag operation is active.
	 */
	virtual void EndTransactionOnSelected();

	// FSerializableObject interface
	virtual void Serialize(FArchive& Ar);

	// FNotifyHook interface
	virtual void NotifyPreChange(void* Src, UProperty* PropertyAboutToChange);
	virtual void NotifyPostChange(void* Src, UProperty* PropertyThatChanged);
	virtual void NotifyExec(void* Src, const TCHAR* Cmd);

	/**
	 * Draws messages on the specified viewport and canvas.
	 */
	virtual void DrawMessages( FViewport* Viewport, FCanvas* Canvas );

	/** 
	 *	Grab the expression array for the given category.
	 *	
	 *	@param	InCategoryName	The category to retrieve
	 *	@param	bCreate			If TRUE, create the entry if not found.
	 *
	 *	@return	The category node.
	 */
	static FCategorizedMaterialExpressionNode* GetCategoryNode(const FString& InCategoryName, UBOOL bCreate);

	/**
	 * Initializes the list of UMaterialExpression-derived classes shared between all material editor instances.
	 */
	static void InitMaterialExpressionClasses();

	/** Initializes the material function library list. */
	void InitializeMaterialFunctionLibrary();

	/**
	 * Remove the expression from the favorites menu list.
	 */
	static void RemoveMaterialExpressionFromFavorites(UClass* InClass);

	/**
	 * Add the expression to the favorites menu list.
	 */
	static void AddMaterialExpressionToFavorites(UClass* InClass);

	/**
	 * Retrieves all visible parameters within the material.
	 *
	 * @param	Material			The material to retrieve the parameters from.
	 * @param	MaterialInstance	The material instance that contains all parameter overrides.
	 * @param	VisisbleExpressions	The array that will contain the id's of the visible parameter expressions.
	 */
	static void GetVisibleMaterialParameters(const UMaterial *Material, UMaterialInstance *MaterialInstance, TArray<FGuid> &VisibleExpressions);

	/**
	 *	Checks if the given expression is in the favorites list...
	 *
	 *	@param	InExpression	The expression to check for.
	 *
	 *	@return	UBOOL			TRUE if it's in the list, FALSE if not.
	 */
	UBOOL IsMaterialExpressionInFavorites(UMaterialExpression* InExpression);

protected:
	/**
	 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
	 *
	 *  @return		A string representing a name to use for this docking parent.
	 */
	virtual const TCHAR* GetDockingParentName() const;

	/**
	 * @return		The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
	 */
	virtual const INT GetDockingParentVersion() const;

	/**
	 * Called by SetPreviewMesh, allows derived types to veto the setting of a preview mesh.
	 *
	 * @return	TRUE if the specified mesh can be set as the preview mesh, FALSE otherwise.
	 */
	virtual UBOOL ApproveSetPreviewMesh(UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh);

	/** Configuration class used to store editor settings across sessions. */
	UMaterialEditorOptions*		EditorOptions;

	/** Set to TRUE when modifications have been made to the material */
	UBOOL						bMaterialDirty;

private:
	/**
	 * Recursively walks the expression tree and parses the visible expression branches.
	 *
	 * @param	MaterialExpression	The expression to parse.
	 * @param	MaterialInstance	The material instance that contains all parameter overrides.
	 * @param	VisisbleExpressions	The array that will contain the id's of the visible parameter expressions.
	 */
	static void GetVisibleMaterialParametersFromExpression(UMaterialExpression *MaterialExpression, UMaterialInstance *MaterialInstance, TArray<FGuid> &VisibleExpressions, TArray<UMaterialExpression*> &ProcessedExpressions);

	class FMaterialNodeDrawInfo
	{
	public:
		INT DrawWidth;
		TArray<INT> LeftYs;
		TArray<INT> RightYs;

		FMaterialNodeDrawInfo()
			:	DrawWidth( 0 )
		{}
		FMaterialNodeDrawInfo(INT InOutputY)
			:	DrawWidth( 0 )
		{
			// Initialize the "output" connectors array with five entries, for material expression.
			LeftYs.AddItem( InOutputY );
			LeftYs.AddItem( InOutputY );
			LeftYs.AddItem( InOutputY );
			LeftYs.AddItem( InOutputY );
			LeftYs.AddItem( InOutputY );
		}
	};

	/** Draw information for the material itself. */
	FMaterialNodeDrawInfo							MaterialDrawInfo;

	/** Draw information for material expressions. */
	TMap<UObject*,FMaterialNodeDrawInfo>		MaterialNodeDrawInfo;

	/** If TRUE, always refresh all expression previews.  This overrides UMaterialExpression::bRealtimePreview. */
	UBOOL											bAlwaysRefreshAllPreviews;

	/** Material expression previews. */
	TIndirectArray<FExpressionPreview>				ExpressionPreviews;

	/** If TRUE, don't render connectors that are not connected to anything. */
	UBOOL											bHideUnusedConnectors;

	/** If TRUE, show material stats like number of shader instructions. */
	UBOOL											bShowStats;

	/** Current search query */
	FString SearchQuery;

	/** Array of expressions matching the current search terms. */
	TArray<UMaterialExpression*> SearchResults;

	/** Index in the above array of the currently selected search result */
	INT SelectedSearchResult;

	/** For double-clicking connectors */
	UObject* DblClickObject;
	INT DblClickConnType;
	INT DblClickConnIndex;

	/** For shift-clicking connectors */
	UObject* MarkedObject;
	INT MarkedConnType;
	INT MarkedConnIndex;

	/**
	 * A human-readable name - material expression input pair.
	 */
	class FMaterialInputInfo
	{
	public:
		FMaterialInputInfo()
		{}
		FMaterialInputInfo(const TCHAR* InName, FExpressionInput* InInput, EMaterialProperty InProperty)
			:	Name( InName )
			,	Input( InInput )
			,	Property( InProperty )
		{}
		const TCHAR* Name;
		FExpressionInput* Input;
		EMaterialProperty Property;
	};

	TArray<FMaterialInputInfo> MaterialInputs;

	/** The current transaction. */
	FScopedTransaction*								ScopedTransaction;

	/** Expressions being moved as a result of lying withing a comment expression. */
	TArray<UMaterialExpression*> ExpressionsLinkedToComments;

	/**
	 * @param		InMaterial	The material to query.
	 * @param		ConnType	Type of the connection (LOC_OUTPUT).
	 * @param		ConnIndex	Index of the connection to query
	 * @return					A viewport location for the connection.
	 */
	FIntPoint GetMaterialConnectionLocation(UMaterial* InMaterial, INT ConnType, INT ConnIndex);

	/**
	 * @param		InMaterialExpression	The material expression to query.
	 * @param		ConnType				Type of the connection (LOC_INPUT or LOC_OUTPUT).
	 * @param		ConnIndex				Index of the connection to query
	 * @return								A viewport location for the connection.
	 */
	FIntPoint GetExpressionConnectionLocation(UMaterialExpression* InMaterialExpression, INT ConnType, INT ConnIndex);

	/**
	 * Returns the drawinfo object for the specified expression, creating it if one does not currently exist.
	 */
	FMaterialNodeDrawInfo& GetDrawInfo(UMaterialExpression* MaterialExpression);

	/**
	 * Returns the expression preview for the specified material expression.
	 */
	FExpressionPreview* GetExpressionPreview(UMaterialExpression* MaterialExpression, UBOOL& bNewlyCreated, UBOOL bDeferCompilation);

	/**
	 * Disconnects and removes the given expressions and comments.
	 */
	void DeleteObjects(const TArray<UMaterialExpression*>& Expressions, const TArray<UMaterialExpressionComment*>& Comments);

	/**
	 * Disconnects and removes the selected material expressions.
	 */
	void DeleteSelectedObjects();

	/**
	 * Duplicates the selected material expressions.  Preserves internal references.
	 */
	void DuplicateSelectedObjects();

	/**
	 * Pastes into this material from the editor's material copy buffer.
	 *
	 * @param	PasteLocation	If non-NULL locate the upper-left corner of the new nodes' bound at this location.
	 */
	void PasteExpressionsIntoMaterial(const FIntPoint* PasteLocation);

	/**
	 * Deletes any disconnected material expressions.
	 */
	void CleanUnusedExpressions();

	/** 
	 * Computes a bounding box for the selected material expressions.  Output is not sensible if no expressions are selected.
	 */
	FIntRect GetBoundingBoxOfSelectedExpressions();

	/** 
	 * Computes a bounding box for the specified set of material expressions.  Output is not sensible if the set is empty.
	 */
	FIntRect GetBoundingBoxOfExpressions(const TArray<UMaterialExpression*>& Expressions);

	/**
	 * Creates a new material expression comment frame.
	 */
	void CreateNewCommentExpression();

	/**
	* Draws specified material expression node:
	* 
	* @param	Canvas				The canvas the expression is being rendered too.
	* @param	ObjInfo				The linking information for this expression.
	* @param	Name				Title Text shown at the top of the expression.
	* @param	Comment				Comment Text associated with this expression.
	* @param	BorderColor			The boarder color of the expression.
	* @param	TitleBkgColor		The background color of the title bar.
	* @param	MaterialExpression	Material expression to be drawn.
	* @param	bRenderPreview		Allows for disabling the render preview pane.
	* @param	bFreezeTime			Allows time to be paused for this expression.
	*/
	void DrawMaterialExpressionLinkedObj(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const TCHAR* Name, const TCHAR* Comment, const FColor& FontColor, const FColor& BorderColor, const FColor& TitleBkgColor, UMaterialExpression* MaterialExpression, UBOOL bRenderPreview, UBOOL bFreezeTime);

	/**
	 * Breaks the link currently indicated by ConnObj/ConnType/ConnIndex.
	 */
	void BreakConnLink(UBOOL bOnlyIfMouseOver);

	/** Updates the marked connector based on what's currently under the mouse, and potentially makes a new connection. */
	void UpdateMarkedConnector();

	/**
	 * Breaks all connections to/from selected material expressions.
	 */
	void BreakAllConnectionsToSelectedExpressions();

	/** Removes the selected expression from the favorites list. */
	void RemoveSelectedExpressionFromFavorites();
	/** Adds the selected expression to the favorites list. */
	void AddSelectedExpressionToFavorites();

	/**
	 * Connects an expression output to the specified material input slot.
	 *
	 * @param	MaterialInputIndex		Index to the material input (Diffuse=0, Emissive=1, etc).
	 */
	void OnConnectToMaterial(INT MaterialInputIndex);

	/**
	 * Updates the original material with the changes made in the editor
	 */
	void UpdateOriginalMaterial();

	/**
	* Rebuilds dependant Material Instance Editors
	* @param		MatInst	Material Instance to search dependent editors and force refresh of them.
	*/
	void RebuildMaterialInstanceEditors(UMaterialInstance * MatInst);

	/**
	 * Uses the global Undo transactor to reverse changes, update viewports etc.
	 */
	INT PreEditUndo();
	void Undo();
	void PostEditUndo( const INT iPreNumExpressions );

	/**
	 * Uses the global Undo transactor to redo changes, update viewports etc.
	 */
	INT PreEditRedo();
	void Redo();
	void PostEditRedo( const INT iPreNumExpressions );

	/**
	  * Updates the SearchResults array based on the search query
	  *
	  * @param	bQueryChanged		Indicates whether the update is because of a query change or a potential material content change.
	  */
	void UpdateSearch( UBOOL bQueryChanged );

	/**
	  * Moves the shows the selected result in the viewport
	  */
	void ShowSearchResult();

	/**
	* PanLocationOnscreen: pan the viewport if necessary to make the particular location visible
	*
	* @param	X, Y		Coordinates of the location we want onscreen
	* @param	Border		Minimum distance we want the location from the edge of the screen.
	*/
	void PanLocationOnscreen( INT X, INT Y, INT Border );

	/**
	* IsActiveMaterialInput: get whether a particular input should be considered "active"
	* based on whether it is relevant to the current material.
	* e.g. SubsurfaceXXX inputs are only active when EnableSubsurfaceScattering is TRUE
	*
	* @param	InputInfo		The input to check.
	* @return	TRUE if the input is active and should be shown normally, FALSE if the input should be drawn to indicate that it is inactive.
	*/
	UBOOL IsActiveMaterialInput(const FMaterialInputInfo& InputInfo);

	////////////////////////
	// Wx event handlers.

	DECLARE_EVENT_TABLE()

	void OnClose(wxCloseEvent& In);

	void OnNewMaterialExpression(wxCommandEvent& In);
	void OnNewComment(wxCommandEvent& In);
	void OnUseCurrentTexture(wxCommandEvent& In);
	void OnDuplicateObjects(wxCommandEvent& In);
	void OnPasteHere(wxCommandEvent& In);
	void OnConvertObjects(wxCommandEvent& In);
	void OnSelectDownsteamNodes(wxCommandEvent& In);
	void OnSelectUpsteamNodes(wxCommandEvent& In);
	void OnDeleteObjects(wxCommandEvent& In);
	void OnBreakLink(wxCommandEvent& In);
	void OnBreakAllLinks(wxCommandEvent& In);
	void OnRemoveFromFavorites(wxCommandEvent& In);
	void OnAddToFavorites(wxCommandEvent& In);
	void OnPreviewNode(wxCommandEvent &In);

	void OnConnectToMaterial_DiffuseColor(wxCommandEvent& In);
	void OnConnectToMaterial_DiffusePower(wxCommandEvent& In);
	void OnConnectToMaterial_EmissiveColor(wxCommandEvent& In);
	void OnConnectToMaterial_SpecularColor(wxCommandEvent& In);
	void OnConnectToMaterial_SpecularPower(wxCommandEvent& In);
	void OnConnectToMaterial_Opacity(wxCommandEvent& In);
	void OnConnectToMaterial_OpacityMask(wxCommandEvent& In);
	void OnConnectToMaterial_Distortion(wxCommandEvent& In);
	void OnConnectToMaterial_TransmissionMask(wxCommandEvent& In);
	void OnConnectToMaterial_TransmissionColor(wxCommandEvent& In);
	void OnConnectToMaterial_Normal(wxCommandEvent& In);
	void OnConnectToMaterial_CustomLighting(wxCommandEvent& In);
	void OnConnectToMaterial_CustomLightingDiffuse(wxCommandEvent& In);
	void OnConnectToMaterial_AnisotropicDirection(wxCommandEvent& In);
	void OnConnectToMaterial_WorldPositionOffset(wxCommandEvent& In);
	void OnConnectToMaterial_WorldDisplacement(wxCommandEvent& In);
	void OnConnectToMaterial_TessellationMultiplier(wxCommandEvent& In);
	void OnConnectToMaterial_SubsurfaceInscatteringColor(wxCommandEvent& In);
	void OnConnectToMaterial_SubsurfaceAbsorptionColor(wxCommandEvent& In);
	void OnConnectToMaterial_SubsurfaceScatteringRadius(wxCommandEvent& In);
	void OnConnectToFunctionOutput(wxCommandEvent& In);

	void OnShowHideConnectors(wxCommandEvent& In);
	
	void OnRealTimeExpressions(wxCommandEvent& In);
	void OnAlwaysRefreshAllPreviews(wxCommandEvent& In);
	
	void OnApply(wxCommandEvent& In);
	void OnFlatten(wxCommandEvent& In);

	void OnToggleStats(wxCommandEvent& In);
	void UI_ToggleStats(wxUpdateUIEvent& In);

	void OnViewSource(wxCommandEvent& In);
	void UI_ViewSource(wxUpdateUIEvent& In);

	void UI_RealTimeExpressions(wxUpdateUIEvent& In);
	void UI_AlwaysRefreshAllPreviews(wxUpdateUIEvent& In);
	void UI_HideUnusedConnectors(wxUpdateUIEvent& In);
	void UI_DrawCurves(wxUpdateUIEvent& In);

	void UI_Apply(wxUpdateUIEvent& In);

	void OnCameraHome(wxCommandEvent& In);
	void OnCleanUnusedExpressions(wxCommandEvent& In);
	void OnSetPreviewMeshFromSelection(wxCommandEvent& In);
	void OnMaterialExpressionTreeDrag(wxTreeEvent& In);
	void OnMaterialExpressionListDrag(wxListEvent& In);
	void OnMaterialFunctionTreeDrag(wxTreeEvent& In);
	void OnMaterialFunctionListDrag(wxListEvent& In);

	void OnSearchChanged(wxCommandEvent& In);
	void OnSearchNext(wxCommandEvent& In);
	void OnSearchPrev(wxCommandEvent& In);

	/**
	 * Routes the event to the appropriate handlers
	 *
	 * @param InType the event that was fired
	 */
	void Send(ECallbackEventType InType, UObject* InObject);

	virtual UObject* GetSyncObject()
	{
		if (EditMaterialFunction)
		{
			return EditMaterialFunction->ParentFunction;
		}
		return OriginalMaterial;
	}
};

#endif // __NEWMATERIALEDITOR_H__
