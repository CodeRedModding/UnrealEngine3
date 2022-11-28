/*=============================================================================
	Properties.h: Classes for displaying object properties windows

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#define OFFSET_None				-1
#define PROP_CategoryHeight		20
#define PROP_CategorySpacer		8
#define	PROP_ItemHeight			16
#define PROP_Indent				16
#define ARROW_Width				16

/*-----------------------------------------------------------------------------
	Class hierarchy
-----------------------------------------------------------------------------*/

class WxPropertyWindowFrame;
class WxPropertyWindow;
class WxPropertyWindow_Base;
	class WxPropertyWindow_Objects;
	class WxPropertyWindow_Category;
	class WxPropertyWindow_Item;

void InitPropertySubSystem();

/*-----------------------------------------------------------------------------
	WxPropertyWindowToolBar.
-----------------------------------------------------------------------------*/

class WxPropertyWindowToolBar : public wxToolBar
{
public:
	WxPropertyWindowToolBar( wxWindow* InParent, wxWindowID InID );
	~WxPropertyWindowToolBar();

	WxMaskedBitmap CopyB, LockB;
};

/*-----------------------------------------------------------------------------
	UPropertyWindowManager

	Manages property windows and coordinates things like object serialization.
-----------------------------------------------------------------------------*/

class UPropertyWindowManager : public USubsystem
{
	DECLARE_CLASS(UPropertyWindowManager,UObject,CLASS_Transient,UnrealEd);

	UPropertyWindowManager();

	// FExec interface.
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

	// UObject interface.
	void Serialize( FArchive& Ar );

	void Add( WxPropertyWindow* InPropertyWindow );
	void Remove( WxPropertyWindow* InPropertyWindow );

	TArray<WxPropertyWindow*> PropertyWindows;			// The list of active property windows

	// Bitmaps that all property windows need

	WxMaskedBitmap ArrowDownB, ArrowRightB, CheckBoxOnB, CheckBoxOffB, CheckBoxUnknownB, Prop_AddB, Prop_EmptyB, Prop_InsertB,
		Prop_DeleteB, Prop_BrowseB, Prop_PickB, Prop_ClearB, Prop_FindB, Prop_UseB, Prop_NewObjectB;
};

/*-----------------------------------------------------------------------------
	WxPropertyWindowFrame

	If a property window needs to float, it sits inside of one of these
	frames.
-----------------------------------------------------------------------------*/

class WxPropertyWindowFrame : public wxFrame
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyWindowFrame)

	WxPropertyWindowFrame()
	{};
	WxPropertyWindowFrame( wxWindow* parent, wxWindowID id, UBOOL bShowToolBar, FNotifyHook* InNotifyHook = NULL );
	virtual ~WxPropertyWindowFrame();

	/** If TRUE, this property frame will destroy itself if the user clicks the "X" button.  Otherwise, it just hides instead. */
	UBOOL bAllowClose;

	/** If TRUE, this property window is locked and won't react to selection changes. */
	UBOOL bLocked;

	void OnClose( wxCloseEvent& In );
	void OnSize( wxSizeEvent& In );
	void OnCopy( wxCommandEvent& In );
	void OnLock( wxCommandEvent& In );
	void UI_Lock( wxUpdateUIEvent& In );
	UBOOL IsLocked() { return bLocked; }

	void UpdateTitle();

	class WxPropertyWindow* PropertyWindow;			// The property window embedded inside this floating frame
	WxPropertyWindowToolBar* ToolBar;				// The optional toolbar at the top of this frame

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxPropertyWindow

	Top level window for any property window (other than the optional frame)
-----------------------------------------------------------------------------*/

class WxPropertyWindow : public wxWindow
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyWindow)

	WxPropertyWindow()
	{}
	WxPropertyWindow( wxWindow* InParent, FNotifyHook* InNotifyHook );
	virtual ~WxPropertyWindow();

	TArray<FString> ExpandedItems;			// The path names to every expanded item in the tree

	INT SplitterPos;						// The position of the break between the variable names and their values/editing areas
	WxPropertyWindow_Base* LastFocused;		// The item window that last had focus.
	UBOOL bDraggingSplitter;				// If 1, the user is dragging the splitter bar
	UBOOL bShowCategories;					// Show category headers?

	wxScrollBar* ScrollBar;
	INT ThumbPos, MaxH;

	FNotifyHook* NotifyHook;

	void SetObject( UObject* InObject, UBOOL InExpandCategories, UBOOL InSorted, UBOOL InShowCategories );
	void SetObjectArray( TArray<UObject*>* InObjects, UBOOL InExpandCategories, UBOOL InSorted, UBOOL InShowCategories );
	void FinalizeValues();
	void GetObjectArray( TArray<UObject*>& Objects );

	void MoveToNextItem( WxPropertyWindow_Base* InItem );
	void MoveToPrevItem( WxPropertyWindow_Base* InItem );

	void RestoreExpandedItems();
	void RememberExpandedItems();

	void NotifyPreChange(UProperty* PropertyAboutToChange);
	void NotifyPostChange(UProperty* PropertyThatChanged);
	void PositionChildren();
	INT PositionChild( WxPropertyWindow_Base* InItem, INT InX, INT InY );
	UBOOL IsItemShown( WxPropertyWindow_Base* InItem );
	void UpdateScrollBar();
	void OnScroll( wxScrollEvent& In );
	void OnMouseWheel( wxMouseEvent& In );
	void OnSize( wxSizeEvent& In );
	class WxPropertyWindow_Objects* GetRoot() const { return Root; }

	void Serialize( FArchive& Ar );

	DECLARE_EVENT_TABLE()

private:

	void Finalize( UBOOL InExpandCategories, UBOOL InSorted, UBOOL InShowCategories );
	void RemoveAllObjects();
	void LinkChildren();
	WxPropertyWindow_Base* LinkChildrenForItem( WxPropertyWindow_Base* InItem );

	class WxPropertyWindow_Objects* Root;	// The first item window of this property window

	friend WxPropertyWindowFrame;
	friend WxPropertyWindow_Objects;
	friend WxPropertyWindow_Category;
};

/*-----------------------------------------------------------------------------
	WxPropertyWindow_Base

	Base class for all window types that appear in the property window
-----------------------------------------------------------------------------*/

class UPropertyDrawProxy;
class UPropertyInputProxy;

class WxPropertyWindow_Base : public wxWindow
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyWindow_Base)

	WxPropertyWindow_Base()
	{}
	WxPropertyWindow_Base( wxWindow* InParent, WxPropertyWindow_Base* InParentItem, UProperty* InProperty, INT InPropertyOffset, INT InArrayIdx );
	virtual ~WxPropertyWindow_Base();

	INT IndentX;										// How far the text label for this item is indented when drawn

	TArray<WxPropertyWindow_Base*> ChildItems;			// The items parented to this one

	UBOOL					bExpanded;					// Is this item expanded?
	UBOOL					bCanBeExpanded;				// Does this item allow expansion?
	WxPropertyWindow_Base*	ParentItem;					// Parent item/window
	WxPropertyWindow_Base*	Next;
	WxPropertyWindow_Base*	Prev;
	UProperty*				Property;					// The property being displayed/edited
	UPropertyDrawProxy*		DrawProxy;					// The proxy used to draw this windows property
	UPropertyInputProxy*	InputProxy;					// The proxy used to handle user input for this property
	INT						PropertyOffset;				// Offset to the properties data
	INT						ArrayIndex;
	INT						ChildHeight, ChildSpacer;	// Used when the property window is positioning/sizing child items
	INT						LastX;						// The last x position the splitter was at during a drag operation
	UBOOL					bSorted;					// Should the child items of this window be sorted?
	TMap<INT,UClass*>		ClassMap;					// Map of menu identifiers to classes - for editinline popup menus
	UClass*					PropertyClass;				// The base class for creating a list of classes applicable to editinline items

	/** Whether to respect CPF_Edit or display all properties */
	UBOOL					RespectEditFlag;

	UBOOL bEdFindable;
	UBOOL bEditInline;
	UBOOL bEditInlineUse;
	UBOOL bEditInlineNotify;
	UBOOL bSingleSelectOnly;

	void NewObject( UClass* InClass, UObject* InParent );
	FString GetPropertyText();
	WxPropertyWindow* FindPropertyWindow();
	WxPropertyWindow_Objects* FindObjectItemParent();
	virtual BYTE* GetReadAddress( WxPropertyWindow_Base* InItem, UBOOL InRequiresSingleSelection );
	virtual BYTE* GetBase( BYTE* Base );
	virtual BYTE* GetContents( BYTE* Base );
	virtual void Expand();
	virtual void Collapse();
	virtual UObject* GetParentObject();
	UBOOL IsOnSplitter( INT InX );
	void ExpandCategories();
	void DeleteChildItems();
	virtual void CreateChildItems();
	virtual FString GetPath();
	void RememberExpandedItems( TArray<FString>* InExpandedItems );
	void RestoreExpandedItems( TArray<FString>* InExpandedItems );
	void FinishSetUp( UProperty* InProperty );

	void OnLeftClick( wxMouseEvent& In );
	void OnLeftUnclick( wxMouseEvent& In );
	void OnLeftDoubleClick( wxMouseEvent& In );
	void OnSize( wxSizeEvent& In );
	void OnSetFocus( wxFocusEvent& In );
	void OnPropItemButton( wxCommandEvent& In );
	void OnEditInlineNewClass( wxCommandEvent& In );
	void OnMouseMove( wxMouseEvent& In );
	void OnChar( wxKeyEvent& In );
	void SortChildren();

	void Serialize( FArchive& Ar );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxPropertyWindow_Objects

	This holds all the child controls and anything related to
	editing the properties of a collection of UObjects.
-----------------------------------------------------------------------------*/

class WxPropertyWindow_Objects : public WxPropertyWindow_Base
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyWindow_Objects)

	WxPropertyWindow_Objects()
	{}
	WxPropertyWindow_Objects( wxWindow* InParent, WxPropertyWindow_Base* InParentItem, UProperty* InProperty, INT InPropertyOffset, INT InArrayIdx );
	virtual ~WxPropertyWindow_Objects();

	TArray<UObject*>		Objects;					// The list of objects we are editing properties for
	UClass*					BaseClass;					// The lowest level base class for this items list of objects

	void AddObject( UObject* InObject );
	void RemoveObject( UObject* InObject );
	void RemoveAllObjects();
	void Finalize();
	virtual FString GetPath();

	virtual void CreateChildItems();
	virtual UClass* GetBestBaseClass();
	virtual BYTE* GetReadAddress( WxPropertyWindow_Base* InItem, UBOOL InRequiresSingleSelection );
	virtual BYTE* GetBase( BYTE* Base );
	virtual UObject* GetParentObject();
	virtual void Expand();
	void OnPaint( wxPaintEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxPropertyWindow_Category

	The header item for a category of items.
-----------------------------------------------------------------------------*/

class WxPropertyWindow_Category : public WxPropertyWindow_Base
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyWindow_Category)

	WxPropertyWindow_Category()
	{}
	WxPropertyWindow_Category( FName InCategoryName, UClass* InBaseClass, wxWindow* InParent, WxPropertyWindow_Base* InParentItem, UProperty* InProperty, INT InPropertyOffset, INT InArrayIdx );
	virtual ~WxPropertyWindow_Category();

	FName					CategoryName;				// The category name

	virtual void CreateChildItems();

	void OnPaint( wxPaintEvent& In );
	void OnLeftClick( wxMouseEvent& In );
	void OnMouseMove( wxMouseEvent& In );
	virtual FString GetPath();

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxPropertyWindow_Item
-----------------------------------------------------------------------------*/

class WxPropertyWindow_Item : public WxPropertyWindow_Base
{
public:
	WxPropertyWindow_Item( wxWindow* InParent, WxPropertyWindow_Base* InParentItem, UProperty* InProperty, INT InPropertyOffset, INT InArrayIdx );
	virtual ~WxPropertyWindow_Item();

	virtual void CreateChildItems();
	virtual BYTE* GetBase( BYTE* Base );
	virtual BYTE* GetContents( BYTE* Base );

	void OnPaint( wxPaintEvent& In );
	void OnInputTextEnter( wxCommandEvent& In );
	void OnInputTextChanged( wxCommandEvent& In );
	void OnInputComboBox( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
