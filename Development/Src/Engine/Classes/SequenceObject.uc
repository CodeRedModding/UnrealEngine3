/**
 * Base class for all Kismet related objects.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SequenceObject extends Object
	native(Sequence)
	abstract
	hidecategories(Object)
	forcescriptorder( TRUE );

cpptext
{
public:
#if WITH_EDITOR
	virtual void CheckForErrors() {};
#endif

	/**
	 * Notification that this object has been connected to another sequence object via a link.  Called immediately after
	 * the designer creates a link between two sequence objects.
	 *
	 * @param	connObj		the object that this op was just connected to.
	 * @param	connIdx		the index of the connection that was created.  Depends on the type of sequence op that is being connected.
	 */
	virtual void OnConnect(USequenceObject *connObj,INT connIdx) {}

	// USequenceObject interface
	virtual void DrawSeqObj(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime) {};
	virtual void DrawLogicLinks(FCanvas* Canvas, TArray<USequenceObject*> &SelectedSeqObjs, USequenceObject* MouseOverSeqObj, INT MouseOverConnType, INT MouseOverConnIndex) {};
	virtual void DrawVariableLinks(FCanvas* Canvas, TArray<USequenceObject*> &SelectedSeqObjs, USequenceObject* MouseOverSeqObj, INT MouseOverConnType, INT MouseOverConnIndex) {};
	virtual void OnCreated()
	{
		ObjInstanceVersion = eventGetObjClassVersion();
	};
	virtual void OnDelete() {}
	virtual void OnSelected() {};

	virtual void OnExport();

	FString GetSeqObjFullName();
	FString GetSeqObjFullLevelName();

	/**
	 * Traverses the ParentSequence chain until a non-sequence object is found, starting with this object.
	 *
	 * @erturn	a pointer to the first object (including this one) in the ParentSequence chain that does
	 *			has a NULL ParentSequence.
	 */
	USequence* GetRootSequence( UBOOL bOuterFallback=FALSE );
	/**
	 * Traverses the ParentSequence chain until a non-sequence object is found, starting with this object.
	 *
	 * @erturn	a pointer to the first object (including this one) in the ParentSequence chain that does
	 *			has a NULL ParentSequence.
	 */
	const USequence* GetRootSequence( UBOOL bOuterFallback=FALSE ) const;
	/**
	 * Traverses the ParentSequence chain until a non-sequence object is found, starting with this object's ParentSequence.
	 *
	 * @erturn	a pointer to the first object (not including this one) in the ParentSequence chain that does
	 *			has a NULL ParentSequence.
	 */
	USequence* GetParentSequenceRoot( UBOOL bOuterFallback=FALSE ) const;

	virtual void UpdateObject()
	{
		// set the new instance version to match the class version
		const INT ObjClassVersion = eventGetObjClassVersion();
		const UBOOL bDirty = ObjInstanceVersion != ObjClassVersion;
		ObjInstanceVersion = ObjClassVersion;
		if ( bDirty )
		{
			MarkPackageDirty();
		}
	}

	/** Converts this SequenceObject into another sequence object. Returns TRUE if this SequenceObject needs to be deleted */
	virtual USequenceObject* ConvertObject()
	{
		return NULL;
	}

	virtual void DrawKismetRefs( FViewport* Viewport, const FSceneView* View, FCanvas* Canvas ) {}

	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void UpdateStatus() {}

	USequenceObject* FindKismetObject();

	/**
	 * Get the name of the class to use for handling user interaction events (such as mouse-clicks) with this sequence object
	 * in the kismet editor.
	 *
	 * @return	a string containing the path name of a class in an editor package which can handle user input events for this
	 *			sequence object.
	 */
	virtual const FString GetEdHelperClassName() const
	{
		return FString( TEXT("UnrealEd.SequenceObjectHelper") );
	}

	virtual UBOOL IsPendingKill() const;

	/**
	 * Provides a way for non-deletable SequenceObjects (those with bDeletable=false) to be removed programatically.  The
	 * user will not be able to remove this object from the sequence via the UI, but calls to RemoveObject will succeed.
	 */
	virtual UBOOL IsDeletable() const { return bDeletable; }

	/**
	 * Returns whether this SequenceObject can exist in a sequence without being linked to anything else (i.e. does not require
	 * another sequence object to activate it)
	 */
	virtual UBOOL IsStandalone() const { return FALSE; }

	/** called when the level that contains this sequence object is being removed/unloaded */
	virtual void CleanUp();

	/**
	 * Builds a list of objects which have this object in their archetype chain.
	 *
	 * All archetype propagation for sequence objects would be handled by prefab code, so this version just skips the iteration.
	 *
	 * @param	Instances	receives the list of objects which have this one in their archetype chain
	 */
	virtual void GetArchetypeInstances( TArray<UObject*>& Instances );

	/**
	 * Serializes all objects which have this object as their archetype into GMemoryArchive, then recursively calls this function
	 * on each of those objects until the full list has been processed.
	 * Called when a property value is about to be modified in an archetype object.
	 *
	 * Since archetype propagation for sequence objects is handled by the prefab code, this version simply routes the call
	 * to the owning prefab so that it can handle the propagation at the appropriate time.
	 *
	 * @param	AffectedObjects		unused
	 */
	virtual void SaveInstancesIntoPropagationArchive( TArray<UObject*>& AffectedObjects );

	/**
	 * De-serializes all objects which have this object as their archetype from the GMemoryArchive, then recursively calls this function
	 * on each of those objects until the full list has been processed.
	 *
	 * Since archetype propagation for sequence objects is handled by the prefab code, this version simply routes the call
	 * to the owning prefab so that it can handle the propagation at the appropriate time.
	 *
	 * @param	AffectedObjects		unused
	 */
	virtual void LoadInstancesFromPropagationArchive( TArray<UObject*>& AffectedObjects );

	/**
	 * Determines whether this object is contained within a UPrefab.
	 *
	 * @param	OwnerPrefab		if specified, receives a pointer to the owning prefab.
	 *
	 * @return	TRUE if this object is contained within a UPrefab; FALSE if it IS a UPrefab or isn't contained within one.
	 */
	virtual UBOOL IsAPrefabArchetype( class UObject** OwnerPrefab=NULL ) const;

	/**
	 * @return	TRUE if the object is a UPrefabInstance or part of a prefab instance.
	 */
	virtual UBOOL IsInPrefabInstance() const;

	virtual void Initialize() {}
#if WITH_EDITOR
	virtual void PrePathBuild(  AScout* Scout ) {}
	virtual void PostPathBuild( AScout* Scout ) {}

	FColor GetBorderColor(UBOOL bSelected, UBOOL bMouseOver);
	FIntPoint GetTitleBarSize(FCanvas* Canvas);

	/** Gives op a chance to customize the title bar text.  e.g. to include important data.  Returns string to display in the title bar. */
	virtual FString GetDisplayTitle() const;
	virtual FString GetAutoComment() const;
	
	virtual void DrawTitleBar(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, const FIntPoint& Pos, const FIntPoint& Size);

	virtual FIntRect GetSeqObjBoundingBox();
	
	// Gives op a chance to add realtime debugging information (when enabled)
	virtual void GetRealtimeComments(TArray<FString> &OutComments) {}

	void SnapPosition(INT Gridsize, INT MaxSequenceSize);
#endif

protected:
	virtual void ConvertObjectInternal(USequenceObject* NewSeqObj, INT LinkIdx = -1) {}
}

/** Class vs instance version, for offering updates in the Kismet editor */
var const			int	ObjInstanceVersion;

/** Sequence that contains this object */
var const noimport Sequence ParentSequence;

/** Visual position of this object within a sequence */
var editoronly int ObjPosX, ObjPosY;

/** Text label that describes this object */
var	string ObjName;

/**
 * Editor category for this object.  Determines which kismet submenu this object
 * should be placed in
 */
var editoronly string ObjCategory;

/** List of games that do not want to display this object */
var editoronly array<string> ObjRemoveInProject;

/** Color used to draw the object */
var editoronly color ObjColor;

/** User editable text comment */
var() string ObjComment<MultilineWithMaxRows=5>;

/** Whether or not this object is deletable. */
var		bool					bDeletable;

/** Should this object be drawn in the first pass? */
var		bool					bDrawFirst;

/** Should this object be drawn in the last pass? */
var		bool					bDrawLast;

/** Cached drawing dimensions */
var		int						DrawWidth, DrawHeight;

/** Should this object display ObjComment when activated? */
var()	bool					bOutputObjCommentToScreen;

/** Should we suppress the 'auto' comment text - values of properties flagged with the 'autocomment' metadata string. */
var()	bool					bSuppressAutoComment;

/** Pointer to our PIE runtime sequence object for runtime kismet debugging.*/
var transient nontransactional editoronly SequenceObject PIESequenceObject;

/** Writes out the specified text to a dedicated scripting log file.
 * @param LogText the text to print
 * @param bWarning true if this is a warning message.
 * 	Warning messages are also sent to the normal game log and appear onscreen if Engine's configurable bOnScreenKismetWarnings is true
 */
native final function ScriptLog(string LogText, optional bool bWarning = true);

/** Returns the current world's WorldInfo, useful for spawning actors and such. */
native final function WorldInfo GetWorldInfo();

/**
 * Determines whether this class should be displayed in the list of available ops in the level kismet editor.
 *
 * @return	TRUE if this sequence object should be available for use in the level kismet editor
 */
event bool IsValidLevelSequenceObject()
{
	return true;
}

/**
 * Determines whether objects of this class are allowed to be pasted into level sequences.
 *
 * @return	TRUE if this sequence object can be pasted into level sequences.
 */
event bool IsPastingIntoLevelSequenceAllowed()
{
	return IsValidLevelSequenceObject();
}

/**
 * Determines whether objects of this class should clear their names when pasting into a level sequence.  
 * NOTE: If not done errors with subojects can happen from copy/paste
 *
 * @return	TRUE if this sequence object should have its name cleared when pasted into level sequences.
 */
event bool ShouldClearNameOnPasting()
{
	return true;
}

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return 1;
}

defaultproperties
{
	bDeletable=true
	ObjName="Undefined"
	ObjColor=(R=255,G=255,B=255,A=255)
	bSuppressAutoComment=true
}
