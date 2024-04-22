/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CoverLink extends NavigationPoint
	native
	DependsOn(Pylon)
	placeable
	ClassGroup(Cover)
	config(Game);

/**
 *	Global flag: Whether coverlinks should create slot markers for navigation
 *	Should be FALSE if using navigation mesh, where cover navigation info will be built into the mesh
 */
var globalconfig bool GLOBAL_bUseSlotMarkers;

// Initial flanking dot prod value
const COVERLINK_ExposureDot		= 0.4f;
// Considered vulnerable at edge slot if past this dot prod
const COVERLINK_EdgeCheckDot	= 0.25f;
const COVERLINK_EdgeExposureDot	= 0.85f;
// Navigation points within this range are considered dangerous to travel through
const COVERLINK_DangerDist		= 1536.f;

struct immutablewhencooked native CoverReference extends ActorReference
{
	/** Slot referenced in the link */
	var() int SlotIdx;
	structcpptext
	{
		friend FArchive& operator<<( FArchive& Ar, FCoverReference& T );
	}
};

cpptext
{
	struct FFireLinkInfo
	{
		class ACoverLink*	Link;
		INT					SlotIdx;
		FCoverSlot*			Slot;
		FVector				SlotLocation;
		FRotator			SlotRotation;
		FVector				X, Y, Z;
		TArray<BYTE>		Types;
		TArray<BYTE>		Actions;

		INT*				out_FireLinkIdx;

		FFireLinkInfo( ACoverLink* InLink, INT InSlotIdx, INT* InIdx = NULL )
		{
			Link			= InLink;
			SlotIdx			= InSlotIdx;
			Slot			= &Link->Slots(SlotIdx);
			out_FireLinkIdx = InIdx;

			if( Slot->bLeanLeft )
			{
				Actions.AddItem( CA_LeanLeft );
			}
			if( Slot->bLeanRight )
			{
				Actions.AddItem( CA_LeanRight );
			}
			if( Slot->bCanPopUp && Slot->CoverType == CT_MidLevel )
			{
				Actions.AddItem( CA_PopUp );
			}

			Types.AddItem( Slot->CoverType );
			if( Slot->CoverType == CT_Standing )
			{
				Types.AddItem( CT_MidLevel );
			}

			SlotLocation = Link->GetSlotLocation(SlotIdx);
			SlotRotation = Link->GetSlotRotation(SlotIdx);
			FRotationMatrix(SlotRotation).GetAxes(X,Y,Z);
		}
	};

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void BuildSlotInfo( INT SlotIdx, UBOOL bSeedPylon = FALSE, AScout* Scout = NULL);
	virtual void BuildSlotInfoInternal( AScout* Scout, INT SlotIdx, UBOOL bSeedPylon = FALSE );

	/**Sorts the CoverSlots
		* @param LastSelectedSlot - the last coverslot the user has selected, the sort will update this value if passed in*/
	void SortSlots(FCoverSlot** LastSelectedSlot = NULL);
	void BuildFireLinks( AScout* Scout );
	void BuildOtherLinks( AScout* Scout );
	UBOOL GetFireActions( FFireLinkInfo& SrcInfo, ACoverLink* TestLink, INT TestSlotIdx, UBOOL bFill = TRUE );
	UBOOL CanFireLinkHit( const FVector &ViewPt, const FVector &TargetLoc, UBOOL bDebugLines = FALSE );

	UBOOL GetExposedInfo( ACoverLink* SrcLink, INT SrcSlotIdx, ACoverLink* DestLink, INT DestSlotIdx, FLOAT& out_ExposedScale );

	virtual UBOOL GetFireLinkTo( INT SlotIdx, const FCoverInfo& ChkCover, BYTE ChkActin, BYTE ChkType, INT& out_FireLinkIdx, TArray<INT>& Items );
	virtual UBOOL HasFireLinkTo( INT SlotIdx, const FCoverInfo& ChkCover, UBOOL bAllowFallbackLinks = FALSE );
	FLOAT GetSlotHeight(INT SlotIdx);
#if WITH_EDITOR
	/** Properly handles the mirroring of cover slots associated with this link */
	virtual void EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation);

	virtual void CheckForErrors();
	virtual INT AddMyMarker(AActor *S);
#endif
	virtual UBOOL IsFireLinkValid( INT SlotIdx, INT FireLinkIdx, BYTE ArrayID = 0 );
	virtual void GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel);

	UBOOL IsOverlapSlotClaimed( APawn *ChkClaim, INT SlotIdx, UBOOL bSkipTeamCheck );

	static FCoverSlot* CoverInfoToSlotPtr( FCoverInfo& InSlot );
	static FCoverSlot* CoverRefToSlotPtr( FCoverReference& InRef );

	UBOOL FindCoverEdges(const FVector& StartLoc, FVector AxisX, FVector AxisY, FVector AxisZ);
	INT AddCoverSlot(FVector& SlotLocation, FRotator& SlotRotation, FCoverSlot Slot, INT SlotIdx = -1);
	void EditorAutoSetup(FVector Direction,FVector *HitL = NULL, FVector *HitN = NULL);
	void ClearExposedFireLinks();

	// called during navmesh generation to link this coverlink into the mesh
	virtual UBOOL LinkCoverSlotToNavigationMesh(INT SlotIdx, class UNavigationMeshBase* Mesh=NULL);

	virtual INT FindCoverReference( ACoverLink* TestLink, INT TestSlotIdx, UBOOL bAddIfNotFound = TRUE );
	virtual UBOOL GetCachedCoverInfo( INT RefIdx, FCoverInfo& out_Info );
	void FixupLevelCoverReferences();

	static FORCEINLINE void FireLinkInteraction_PackSrcType( BYTE SrcType, BYTE& PackedByte )
	{
		if( SrcType == CT_MidLevel ) { PackedByte |= (1<<0); }
	}
	static FORCEINLINE void FireLinkInteraction_PackSrcAction( BYTE SrcAction, BYTE& PackedByte )
	{
		PackedByte |= (SrcAction == CA_LeanLeft  ? (1<<1) :
					   SrcAction == CA_LeanRight ? (1<<2) :
					   SrcAction == CA_PopUp     ? (1<<3) :
													0);
	}
	static FORCEINLINE void FireLinkInteraction_PackDestType( BYTE DestType, BYTE& PackedByte )
	{
		if( DestType == CT_MidLevel ) { PackedByte |= (1<<4); }
	}
	static FORCEINLINE void FireLinkInteraction_PackDestAction( BYTE DestAction, BYTE& PackedByte )
	{
		PackedByte |= (DestAction == CA_LeanLeft  ? (1<<5) :
					   DestAction == CA_LeanRight ? (1<<6) :
					   DestAction == CA_PopUp     ? (1<<7) :
													0);
	}

	static FORCEINLINE BYTE FireLinkInteraction_UnpackSrcType( const BYTE PackedByte )
	{
		return (PackedByte & (1<<0)) ? CT_MidLevel : CT_Standing;
	}
	static FORCEINLINE BYTE FireLinkInteraction_UnpackSrcAction( const BYTE PackedByte )
	{
		return (PackedByte & (1<<1)) ? CA_LeanLeft  :
			   (PackedByte & (1<<2)) ? CA_LeanRight :
			   (PackedByte & (1<<3)) ? CA_PopUp :
				CA_Default;
	}
	static FORCEINLINE BYTE FireLinkInteraction_UnpackDestType( const BYTE PackedByte )
	{
		return (PackedByte & (1<<4)) ? CT_MidLevel : CT_Standing;
	}
	static FORCEINLINE BYTE FireLinkInteraction_UnpackDestAction( const BYTE PackedByte )
	{
		return (PackedByte & (1<<5)) ? CA_LeanLeft  :
			   (PackedByte & (1<<6)) ? CA_LeanRight :
			   (PackedByte & (1<<7)) ? CA_PopUp :
				CA_Default;
	}
};

/** Utility struct for referencing cover link slots. */
struct immutablewhencooked native CoverInfo
{
	var() editconst CoverLink Link;
	var() editconst int SlotIdx;

	structcpptext
	{
		FCoverInfo()
		{
			Link = NULL;
			SlotIdx = 0;
		}
		FCoverInfo(EEventParm)
		{
			appMemzero(this, sizeof(FCoverInfo));
		}
		FCoverInfo(class ACoverLink* inLink, INT inSlotIdx)
		{
			Link = inLink;
			SlotIdx = inSlotIdx;
		}
		UBOOL operator==(const FCoverInfo &Other) const
		{
			return (this->Link == Other.Link && this->SlotIdx == Other.SlotIdx);
		}
		FString ToString() const;
	}
};

/** Utility struct to reference a position in cover */
struct immutablewhencooked native CovPosInfo
{
	/** CoverLink holding cover position */
	var CoverLink	Link;
	/** Index of left bounding slot */
	var	int			LtSlotIdx;
	/** Index of right bounding slot */
	var int			RtSlotIdx;
	/** Pct of distance Location is, between left and right slots */
	var float		LtToRtPct;
	/** Location in cover */
	var	vector		Location;
	/** Normal vector, used to define direction. Pointing from Location away from Wall. */
	var vector		Normal;
	/** Tangent vector, gives alignement of cover. With multiple slots cover, this gives the direction from Left to Right slots. */
	var vector		Tangent;

	structdefaultproperties
	{
		LtSlotIdx=-1
		RtSlotIdx=-1
		LtToRtPct=+0.f
	}
};


/**
 * Represents the current action this pawn is performing at the
 * current cover node.
 */
enum ECoverAction
{
	/** Default no action */
	CA_Default,
	/** Blindfiring to the left */
	CA_BlindLeft,
	/** Blindfiring to the right */
	CA_BlindRight,
	/** Leaning to the left */
	CA_LeanLeft,
	/** Leaning to the right */
	CA_LeanRight,
	/** Pop up, out of cover */
	CA_PopUp,
	/** Blind fire up */
	CA_BlindUp,

	/** AI Peek from cover options */
	CA_PeekLeft,
	CA_PeekRight,
	CA_PeekUp,
};

/**
 * Represents a direction associated with cover, for use with movement/camera/etc.
 */
enum ECoverDirection
{
	CD_Default,
	CD_Left,
	CD_Right,
	CD_Up,
};

/**
 * Represents what type of cover this node provides.
 */
enum ECoverType
{
	/** Default, no cover */
	CT_None,
	/** Full standing cover */
	CT_Standing,
	/** Mid-level crouch cover, stand to fire */
	CT_MidLevel,
};

/** Descriptive tags for a particular cover location.  Could be used for custom dialogue, for instance. */
enum ECoverLocationDescription
{
	CoverDesc_None,
	CoverDesc_InWindow,
	CoverDesc_InDoorway,
	CoverDesc_BehindCar,
	CoverDesc_BehindTruck,
	CoverDesc_OnTruck,
	CoverDesc_BehindBarrier,
	CoverDesc_BehindColumn,
	CoverDesc_BehindCrate,
	CoverDesc_BehindWall,
	CoverDesc_BehindStatue,
	CoverDesc_BehindSandbags,
	// new entries go here at the end
};

enum EFireLinkID
{
	FLI_FireLink,
	FLI_RejectedFireLink,
	// new entries go here at the end
};

/** Contains specific links between SOURCE actions/postures to DEST actions/postures */
struct immutablewhencooked native FireLinkItem
{
	/** CT_Standing/CT_MidLevel for source */
	var ECoverType		SrcType;
	/** Action for source */
	var ECoverAction	SrcAction;
	/** CT_Standing/CT_MidLevel for source */
	var ECoverType		DestType;
	/** Action for source */
	var ECoverAction	DestAction;
};

/**
 * Contains information about what other cover nodes this node is
 * capable of firing on.
 */
struct immutablewhencooked native FireLink
{
//	var deprecated editconst const CoverReference TargetActor;
//	var deprecated array<FireLinkItem>	Items;

	/** List of fire link interactions */
	var array<byte> Interactions;

	/**
	 *  Packed properties
	 *  CoverRefIdx          (Bits 0  - 15) - Index into Levels CoverIndexPairs array
	 *  DynamicLinkInfoIndex (Bits 16 - 31) - Index into this CoverLinks DynamicLinkInfos array
	 */
	var private const int PackedProperties_CoverPairRefAndDynamicInfo;

	/** Is this link considered a fallback link? (Shouldn't be desired, but is acceptable) */
	var private bool bFallbackLink;
	/** Whether DynamicLinkInfoIndex has been initialized */
	var private bool bDynamicIndexInited;

	structcpptext
	{
		/**
		  *  Updated DynamicLinkInfos array if source or destination is dynamic
		  */
		void UpdateDynamicLinkInfoFor(ACoverLink* MyLink, ACoverLink* TestLink, INT InSlotIdx, const FVector& LastSrcLocation);

		FVector GetLastTargetLocation(ACoverLink *MyLink);
		FVector GetLastSrcLocation(ACoverLink *MyLink);

		FORCEINLINE void SetFallbackLink( UBOOL bSet )
		{
			bFallbackLink = bSet;
		}

		FORCEINLINE UBOOL IsFallbackLink()
		{
			return bFallbackLink;
		}

		FORCEINLINE void SetDynamicIndexInited( UBOOL bSet )
		{
			bDynamicIndexInited = bSet;
		}

		FORCEINLINE UBOOL IsDynamicIndexInited()
		{
			return bDynamicIndexInited;
		}

		FORCEINLINE void SetCoverRefIdx( INT Val )
		{
			Val &= 0x0000FFFF;
			PackedProperties_CoverPairRefAndDynamicInfo &= ~(0x0000FFFF);
			PackedProperties_CoverPairRefAndDynamicInfo |= Val;
		}
		FORCEINLINE DWORD GetCoverRefIdx()
		{
			return (PackedProperties_CoverPairRefAndDynamicInfo & (0x0000FFFF));
		}

		FORCEINLINE void SetDynamicLinkInfoIndex( INT Val )
		{
			Val &= 0xFFFF0000;
			PackedProperties_CoverPairRefAndDynamicInfo &= ~(0xFFFF0000);
			PackedProperties_CoverPairRefAndDynamicInfo |= (Val << 16);
		}
		FORCEINLINE DWORD GetDynamicLinkInfoIndex()
		{
			return ((PackedProperties_CoverPairRefAndDynamicInfo & (0xFFFF0000)) >> 16);
		}
	}
};

struct immutablewhencooked native DynamicLinkInfo
{
	/** Location of the target when this FireLink was created/updated (Used for tracking CoverLink_Dynamic) */
	var Vector LastTargetLocation;

	/** Location of the src when this FireLink was created/updated (Used for tracking CoverLink_Dynamic) */
	var Vector LastSrcLocation;
};

/**
 *	Contains information about other cover nodes this node is exposed to
 *	(ie flanked by)
 */
struct immutablewhencooked native ExposedLink
{
	/** Slot that is dangerous to this link */
	var() editconst const CoverReference	TargetActor;

	/** Scale of how dangerous this exposure is
		(0,255] -- ~0 = not very dangerous, 255 = extremely dangerous */
	var() byte ExposedScale;
};

struct immutablewhencooked native SlotMoveRef
{
	var() PolyReference Poly;
	var() BasedPosition Dest;
	var() int			Direction;

	structcpptext
	{
		void Clear()
		{
			Poly.Clear();
			Dest.Clear();
			Direction = 0;
		}
	}
};

/** Contains information for a cover slot that a player can occupy */
struct immutablewhencooked native CoverSlot
{
	/** Slot marker to allow the slot to exist on the navigation network */
//	var deprecated editconst Actor SlotMarker;
//	var deprecated array<FireLink> ForcedFireLinks;
//	var deprecated array<ExposedLink> ExposedFireLinks;
//	var deprecated editconst array<CoverInfo> OverlapClaims;
//	var deprecated array<CoverReference> TurnTarget;

	/** Current owner of this slot */
	var Pawn SlotOwner;

	/** Slot is invalid until world.timeseconds is >= this value (allows temporary disabling of slots) */
	var transient float SlotValidAfterTime;

	/** Gives LDs ability to force the type - CT_None == auto find*/
	var() ECoverType ForceCoverType;
	/** Type of cover this slot provides */
	var(Auto) editconst ECoverType CoverType;
	/** Per-slot description tag.  If _None, fall back to the description in the CoverLink. */
	var() ECoverLocationDescription	LocationDescription;


	/** Offset from node location for this slot */
	var vector LocationOffset;

	/** Offset from node rotation for this slot */
	var rotator RotationOffset;

	/** List of actions possible from this slot */
	var array<ECoverAction> Actions;

	/** List of all attackable nodes */
	var() editconst array<FireLink> FireLinks;

	/** List of coverlinks/slots that couldn't be shot at - used by COVERLINK_DYNAMIC */
	var() editconst transient array<FireLink>	RejectedFireLinks;

	/**
	 *  ExposedCover Packed Properties
	 *  CoverRefIdx          (Bits 0  - 15) - Index into Levels CoverIndexPairs array
	 *  ExposedScale         (Bits 16 - 23) - Scale of how dangerous this exposure is
	 *                                       (0,255] -- ~0 = not very dangerous, 255 = extremely dangerous
	 */
	var private array<int>  ExposedCoverPackedProperties;

	/**
	 *  Link/slot info about where swat turn evade can move to
	 *  Packs left/right index into Level CoverIndexPair
	 *  left turn target into bits 0-15, right turn target into 16-31
	 */
	var private int TurnTargetPackedProperties;

	/** Info about where cover slip can move to */
	var array<SlotMoveRef> SlipRefs;

	/** List of cover slots that should be claimed when this slot is claimed
		because they are overlapping */
	var(Auto) editconst array<CoverInfo> OverlapClaimsList;
	/** Can we lean left/right to shoot from this slot? */
	var(Auto) bool bLeanLeft, bLeanRight;
	/** Can we popup? */
	var(Auto) bool bForceCanPopUp;
	var(Auto) editconst bool bCanPopUp;
	/** Can we mantle over this cover? */
	var(Auto) editconst bool bCanMantle;
	/** Can we mantle up? */
	var(Auto) editconst bool bCanClimbUp;
	/** Can cover slip at this slot? */
	var(Auto) bool bForceCanCoverSlip_Left, bForceCanCoverSlip_Right;
	var(Auto) editconst bool bCanCoverSlip_Left, bCanCoverSlip_Right;
	/** Can swat turn at this slot? */
	var(Auto) editconst bool bCanSwatTurn_Left, bCanSwatTurn_Right;

	/** Is this slot currently enabled? */
	var() bool bEnabled;

	/** Is popping up allowed for midlevel/crouching cover? */
	var() bool bAllowPopup;
	/** Is mantling allowed here? */
	var() bool bAllowMantle;
	/** Is cover slip allowed? */
	var() bool bAllowCoverSlip;
	/** Is climbing up allowed here? */
	var() bool bAllowClimbUp;
	/** Is swat turn allowed? */
	var() bool bAllowSwatTurn;
	/** if this is on ground adjustments will be skipped */
	var() bool bForceNoGroundAdjust;
	/** Slot can only be used by players, not AI */
	var() bool bPlayerOnly;
	/** Override the default behavior of popup on target preffered over lean out */
	var() bool bPreferLeanOverPopup;
	/** runtime only - whether this slot is on destructible cover (so AI can shoot it to get the enemy out) */
	var transient bool bDestructible;

	/** === Editor specific === */
	/** Is this slot currently selected for editing? */
	var transient bool bSelected;

	/** Map Error: Cover slot failed to find surface to align to */
	var() transient editconst bool bFailedToFindSurface;

	structdefaultproperties
	{
		bEnabled=TRUE

		bCanMantle=TRUE
		bCanCoverSlip_Left=TRUE
		bCanCoverSlip_Right=TRUE
		bCanSwatTurn_Left=TRUE
		bCanSwatTurn_Right=TRUE
		bCanClimbUp=FALSE

		bAllowMantle=TRUE
		bAllowCoverSlip=TRUE
		bAllowPopup=TRUE
		bAllowSwatTurn=TRUE
		bAllowClimbUp=FALSE

		TurnTargetPackedProperties=4294967296
	}

	structcpptext
	{
		FORCEINLINE void SetExposedCoverRefIdx( INT Index, INT Val )
		{
			Val &= 0x0000FFFF;
			ExposedCoverPackedProperties(Index) &= ~(0x0000FFFF);
			ExposedCoverPackedProperties(Index) |= Val;
		}
		FORCEINLINE DWORD GetExposedCoverRefIdx( INT Index )
		{
			return (ExposedCoverPackedProperties(Index) & (0x0000FFFF));
		}

		FORCEINLINE void SetExposedScale( INT Index, INT Val )
		{
			Val &= 0x000000FF;
			ExposedCoverPackedProperties(Index) &= ~(0x00FF0000);
			ExposedCoverPackedProperties(Index) |= (Val << 16);
		}
		FORCEINLINE BYTE GetExposedScale( INT Index )
		{
			return ((ExposedCoverPackedProperties(Index) & (0x00FF0000)) >> 16);
		}

		FORCEINLINE void SetLeftTurnTargetCoverRefIdx( INT Val )
		{
			Val &= 0x0000FFFF;
			TurnTargetPackedProperties &= ~(0x0000FFFF);
			TurnTargetPackedProperties |= Val;
		}
		FORCEINLINE DWORD GetLeftTurnTargetCoverRefIdx()
		{
			return (TurnTargetPackedProperties & (0x0000FFFF));
		}
		FORCEINLINE void SetRightTurnTargetCoverRefIdx( INT Val )
		{
			Val &= 0x0000FFFF;
			TurnTargetPackedProperties &= ~(0xFFFF0000);
			TurnTargetPackedProperties |= (Val << 16);
		}
		FORCEINLINE DWORD GetRightTurnTargetCoverRefIdx()
		{
			return ((TurnTargetPackedProperties & (0xFFFF0000)) >> 16);
		}

		FORCEINLINE FFireLink& GetFireLinkRef( INT FireLinkIdx, BYTE ArrayID = 0 )
		{
			if( ArrayID == FLI_RejectedFireLink )
			{
				return RejectedFireLinks(FireLinkIdx);
			}
			else
			{
				return FireLinks(FireLinkIdx);
			}
		}
	}
};

/** How far auto adjust code traces forward from lean fire point
	to determine if the lean has a valid fire line */
var	float LeanTraceDist;


/** All slots linked to this node */
var() editinline array<CoverSlot> Slots;

/** Array of src and target location for dynamic links */
var array<DynamicLinkInfo> DynamicLinkInfos;

/** List of all players using this cover */
var array<Pawn> Claims;

/** Whether cover link is disabled */
var() bool bDisabled;

/** Claim all slots when someone claims one - used for cover that needs more than one slot, but slots overlap */
var() bool bClaimAllSlots;

/** Allow auto-sorting of the Slots array */
var() bool bAutoSort;

/** Allow auto-adjusting of the Slots orientation/position and covertype? */
var() bool bAutoAdjust;

/** Is this circular cover? */
var() bool bCircular;

/** Cover is looped, first slot and last slot should be reachable direclty */
var() bool bLooped;

/** Is this cover restricted to player use? */
var() bool bPlayerOnly;
/** This cover is dynamic */
var	  bool bDynamicCover;
/** This cover fractures when it is interacted with */
var() bool bFractureOnTouch;
/** Distance link must move to invalidate it's info */
var() float	InvalidateDistance;
/** Max trace dist for fire links to check */
var() float MaxFireLinkDist;

/** Origin for circular cover */
var const vector CircularOrigin;

/** Radius for circular cover */
var const float CircularRadius;

/** Distance used when aligning to nearby surfaces */
var const float AlignDist;

/** Minimum distance to place between non-essential cover slots when auto-generating a cover link */
var const float AutoCoverSlotInterval;

/** Min height for nearby geometry to categorize as standing cover */
var const float StandHeight;

/** Min height for nearby geometry to categorize as mid-level cover */
var const float MidHeight;

var const Vector	StandingLeanOffset;
var const Vector	CrouchLeanOffset;
var const Vector	PopupOffset;

/** Forward distance for checking cover slip links */
var const float	SlipDist;
/** Lateral distance for checking swat turn links */
var const float	TurnDist;
/** Scale applied to danger cost during path finding for slots of this link */
var() float DangerScale;

/** Used for the WorldInfo.CoverList linked list */
var const CoverLink NextCoverLink;

var(Debug)	bool bDebug_FireLinks;
var(Debug)	bool bDebug_ExposedLinks;
/** when enabled, extra info will be drawn and printed to the log related to generation of cover information for this link */
var(Debug)  bool bDebug_CoverGen;

/** Description for the entire CoverLink.  Can be overridden per-slot. */
var() const ECoverLocationDescription	LocationDescription;

/** Should we automatically insert slots when there is too big of a gap? */
var() bool bDoAutoSlotDensityFixup;

simulated native function bool GetFireLinkTargetCoverInfo( int SlotIdx, int FireLinkIdx, out CoverInfo out_Info, optional EFireLinkID ArrayID );

/**
 *  Packs fire link item info into a single byte
 *  SrcType/DestType - only allow CT_Standing/CT_MidLevel
 *  SrcAction/DestAction - only allow CA_LeanLeft/CA_LeanRight/CA_PopUp/CA_Default(destonly)
 */
simulated static native function BYTE PackFireLinkInteractionInfo( ECoverType SrcType, ECoverAction SrcAction, ECoverType	DestType, ECoverAction DestAction );
simulated static native function UnPackFireLinkInteractionInfo( const BYTE PackedByte, out ECoverType SrcType, out ECoverAction SrcAction, out ECoverType DestType, out ECoverAction DestAction );

/** Returns the world location of the requested slot. */
simulated native final function vector GetSlotLocation(int SlotIdx, optional bool bForceUseOffset);

/** Returns the world rotation of the requested slot. */
simulated native final function rotator GetSlotRotation(int SlotIdx, optional bool bForceUseOffset);

/** Returns the world location of the default viewpoint for the specified slot. */
simulated native final function vector GetSlotViewPoint( int SlotIdx, optional ECoverType Type, optional ECoverAction Action );

simulated native final function bool IsExposedTo( int SlotIdx, CoverInfo ChkSlot, out float out_ExposedScale );

simulated final event SetInvalidUntil(int SlotIdx, float TimeToBecomeValid)
{
	Slots[SlotIdx].SlotValidAfterTime = TimeToBecomeValid;
	NotifySlotOwnerCoverDisabled( SlotIdx );
}

/** Asserts a claim on this link by the specified controller. */
simulated final event bool Claim( Pawn NewClaim, int SlotIdx )
{
	local int	Idx;
	local bool	bResult, bDoClaim;
	local PlayerController PC;
	local Pawn PreviousOwner;

`if (`notdefined(FINAL_RELEASE))
	local int NumClaims;
	local array<int> SlotList;
	local String Str;

	//debug
	if( bDebug )
	{
		`log( self@"Claim Slot"@SlotIdx@"For"@NewClaim@"(All?)"@bClaimAllSlots );
	}
`endif

	// Make sure SlotIdx is valid
	if( SlotIdx < 0 )
	{
		return FALSE;
	}

	bDoClaim = TRUE;


	// If slot already claimed
	if( Slots[SlotIdx].SlotOwner != None )
	{
		// If we have already claimed it, nothing to do
		// If we don't, fail claim
		bResult = Slots[SlotIdx].SlotOwner == NewClaim;
		bDoClaim = FALSE;

		// If claimer is different
		if( !bResult )
		{
			// If claimer is a player controller
			PC = PlayerController( NewClaim.Controller );
			if( PC != None )
			{
				PreviousOwner = Slots[SlotIdx].SlotOwner;
				// Tell the previous owner that we are taking over
				bDoClaim = TRUE;
			}
		}
	}

	if( bDoClaim )
	{
		// If all slots must be claimed
		if( bClaimAllSlots )
		{
			// Loop through each slot and set new claim as owner of all
			for( Idx = 0; Idx < Slots.Length; Idx++ )
			{
				if( Slots[Idx].SlotOwner == None )
				{
					// Add entry to general claims list (will contain multiple entries if has multiple slots claimed)
					Claims[Claims.Length] = NewClaim;
					// Mark slot claim
					Slots[Idx].SlotOwner = NewClaim;
					bResult = TRUE;
				}
			}
		}
		else
		{
			// Add entry to general claims list (will contain multiple entries if has multiple slots claimed)
			Claims[Claims.Length] = NewClaim;
			// Mark slot claim
			Slots[SlotIdx].SlotOwner = NewClaim;

			bResult = TRUE;
		}
		if (PreviousOwner != None && PreviousOwner.Controller != None)
		{
			PreviousOwner.Controller.NotifyCoverClaimViolation( NewClaim.Controller, self, SlotIdx );
		}
	}

	//debug
`if (`notdefined(FINAL_RELEASE))
	if( bDebug )
	{
		for( Idx = 0; Idx < Claims.Length; Idx++ )
		{
			if( Claims[Idx] == NewClaim )
			{
				NumClaims++;
			}
		}
		for( Idx = 0; Idx < Slots.Length; Idx++ )
		{
			if( Slots[Idx].SlotOwner == NewClaim )
			{
				SlotList[SlotList.Length] = Idx;
			}
		}
		if( SlotList.Length == 0 )
		{
			Str = "None";
		}
		else
		{
			for( Idx = 0; Idx < SlotList.Length; Idx++ )
			{
				Str = Str@SlotList[Idx];
			}
		}

		`log( self@"Claims from"@NewClaim@NumClaims@"Slots:"@Str );

		ScriptTrace();
	}
`endif

	return bResult;
}

/** Removes any claims the specified controller has on this link. */
simulated final event bool UnClaim( Pawn OldClaim, int SlotIdx, bool bUnclaimAll )
{
	local int Idx, NumReleased;
	local bool bResult;

`if (`notdefined(FINAL_RELEASE))
	//debug
	local int NumClaims;
	local array<int> SlotList;
	local String Str;

	//debug
	if( bDebug )
	{
		`log( self@"UnClaim"@`showvar(OldClaim)@`showvar(SlotIdx)@`showvar(bUnclaimAll)@`showvar(bClaimAllSlots) );
	}
`endif

	if( !bUnclaimAll && SlotIdx < 0)
	{
		return false;
	}

	// If letting go of link completely
	if( bUnclaimAll )
	{
		// Clear the slot owner from all slots
		for( Idx = 0; Idx < Slots.Length; Idx++ )
		{
			if( Slots[Idx].SlotOwner == OldClaim )
			{
				Slots[Idx].SlotOwner = None;
				NumReleased++;
				bResult = TRUE;
			}
		}
	}
	// Otherwise, if we want to let go of only one slot (and shouldn't always hold all of them)
	else if( !bClaimAllSlots && Slots[SlotIdx].SlotOwner == OldClaim )
	{
		// Release this slot
		Slots[SlotIdx].SlotOwner = None;
		NumReleased++;
		bResult = TRUE;
	}

	// For each slot released
	while( NumReleased > 0 )
	{
		// Find a claim in the list
		Idx = Claims.Find(OldClaim);
		if( Idx < 0 )
		{
			break;
		}

		// Clear on claim from the general claims list
		Claims.Remove( Idx, 1 );
		NumReleased--;
	}

	//debug
`if (`notdefined(FINAL_RELEASE))
	if( bDebug )
	{
		for( Idx = 0; Idx < Claims.Length; Idx++ )
		{
			if( Claims[Idx] == OldClaim )
			{
				NumClaims++;
			}
		}
		for( Idx = 0; Idx < Slots.Length; Idx++ )
		{
			if( Slots[Idx].SlotOwner == OldClaim )
			{
				SlotList[SlotList.Length] = Idx;
			}
		}
		if( SlotList.Length == 0 )
		{
			Str = "None";
		}
		else
		{
			for( Idx = 0; Idx < SlotList.Length; Idx++ )
			{
				Str = Str@SlotList[Idx];
			}
		}

		`log( self@"Claims from"@`showvar(OldClaim)@`showvar(NumClaims)@"Slots:"@Str );

		ScriptTrace();
	}
`endif

	return bResult;
}

/** Returns true if the specified controller is able to claim the slot. */
final native function bool IsValidClaim( Pawn ChkClaim, int SlotIdx, optional bool bSkipTeamCheck, optional bool bSkipOverlapCheck );
final native function bool IsValidClaimBetween( Pawn ChkClaim, int StartSlotIdx, int EndSlotIdx, optional bool bSkipTeamCheck, optional bool bSkipOverlapCheck );

/**
 * Checks to see if the specified slot support stationary cover actions.
 */
simulated final function bool IsStationarySlot(int SlotIdx)
{
	return (!bCircular && IsEdgeSlot(SlotIdx,FALSE));
}

/**
 * Finds the current set of slots the specified point is between.  Returns true
 * if a valid slot set was found.
 */
simulated native final function bool FindSlots(vector CheckLocation, float MaxDistance, out int LeftSlotIdx, out int RightSlotIdx);


/**
 * Return true if the specified slot is an edge, signifying "End Of Cover".
 */
simulated native final function bool IsEdgeSlot( int SlotIdx, optional bool bIgnoreLeans );
simulated native final function bool IsLeftEdgeSlot( int SlotIdx, bool bIgnoreLeans );
simulated native final function bool IsRightEdgeSlot( int SlotIdx, bool bIgnoreLeans );

simulated native final function int GetSlotIdxToLeft(  int SlotIdx, optional int Cnt = 1 );
simulated native final function int GetSlotIdxToRight( int SlotIdx, optional int Cnt = 1 );

simulated final function bool AllowRightTransition(int SlotIdx)
{
	local int NextSlotIdx;

	NextSlotIdx = GetSlotIdxToRight( SlotIdx );
	if( NextSlotIdx >= 0 )
	{
		return Slots[NextSlotIdx].bEnabled;
	}
	return FALSE;
}

simulated final function bool AllowLeftTransition(int SlotIdx)
{
	local int NextSlotIdx;

	NextSlotIdx = GetSlotIdxToLeft( SlotIdx );
	if( NextSlotIdx >= 0 )
	{
		return Slots[NextSlotIdx].bEnabled;
	}
	return FALSE;
}

/**
 * Searches for a fire link to the specified cover/slot and returns the cover actions.
 */
native noexport function bool GetFireLinkTo( int SlotIdx, CoverInfo ChkCover, ECoverAction ChkAction, ECoverType ChkType, out int out_FireLinkIdx, out array<int> out_Items );

/**
 * Searches for a valid fire link to the specified cover/slot.
 * NOTE: marked noexport until 'optional out int' is fixed in the exporter
 */
native noexport function bool HasFireLinkTo( int SlotIdx, CoverInfo ChkCover, optional bool bAllowFallbackLinks );

/**
 * Returns a list of AI actions possible from this slot
 */
native final function GetSlotActions( int SlotIdx, out array<ECoverAction> Actions );

/**
 * Enable/disable the entire CoverLink.
 */
simulated event SetDisabled(bool bNewDisabled)
{
	local int SlotIdx;
	local CoverReplicator CoverReplicator;

	bDisabled = bNewDisabled;

	if( bDisabled )
	{
		for( SlotIdx = 0; SlotIdx < Slots.Length; SlotIdx++ )
		{
			NotifySlotOwnerCoverDisabled( SlotIdx );
		}
	}

	// if on server, notify clients slot was disabled
	if( Role == ROLE_Authority )
	{
		CoverReplicator = WorldInfo.Game.GetCoverReplicator();
		if (CoverReplicator != None)
		{
			CoverReplicator.NotifyLinkDisabledStateChange(self);
		}
	}
}

/**
 * Enable/disable a particular cover slot.
 */
simulated event SetSlotEnabled(int SlotIdx, bool bEnable)
{
	Slots[SlotIdx].bEnabled = bEnable;

	if( !bEnable )
	{
		NotifySlotOwnerCoverDisabled( SlotIdx );
	}
}

simulated function NotifySlotOwnerCoverDisabled( int SlotIdx, optional bool bAIOnly )
{
	local int LeftIdx, RightIdx;

	if( Slots[SlotIdx].SlotOwner != None &&
		Slots[SlotIdx].SlotOwner.Controller != None &&
		(!bAIOnly || PlayerController(Slots[SlotIdx].SlotOwner.Controller) == None) )
	{
		// notify any owner that the slot is disabled
		Slots[SlotIdx].SlotOwner.Controller.NotifyCoverDisabled( self, SlotIdx, FALSE );
	}

	// Notify any adjacent owners
	LeftIdx = GetSlotIdxToLeft( SlotIdx );
	if( LeftIdx >= 0 &&
		Slots[LeftIdx].SlotOwner != None &&
		Slots[LeftIdx].SlotOwner.Controller != None &&
		(!bAIOnly || PlayerController(Slots[LeftIdx].SlotOwner.Controller) == None) )
	{
		Slots[LeftIdx].SlotOwner.Controller.NotifyCoverDisabled( self, SlotIdx, TRUE );
	}

	RightIdx = GetSlotIdxToRight( SlotIdx );
	if( RightIdx >= 0 &&
		Slots[RightIdx].SlotOwner != None &&
		Slots[RightIdx].SlotOwner.Controller != None &&
		(!bAIOnly || PlayerController(Slots[RightIdx].SlotOwner.Controller) == None) )
	{
		Slots[RightIdx].SlotOwner.Controller.NotifyCoverDisabled( self, SlotIdx, TRUE );
	}
}

/**
 * Enable/disable playersonly on a particular cover slot.
 */
simulated event SetSlotPlayerOnly(int SlotIdx, bool bInPlayerOnly )
{
	Slots[SlotIdx].bPlayerOnly = bInPlayerOnly;

	if( Slots[SlotIdx].bPlayerOnly )
	{
		NotifySlotOwnerCoverDisabled( SlotIdx, TRUE );
	}
}


/**
 * Handle modify action by enabling/disabling the list of slots, or auto adjusting.
 */
function OnModifyCover(SeqAct_ModifyCover Action)
{
	local array<int> SlotIndices;
	local int Idx, SlotIdx;
	local CoverReplicator CoverReplicator;

	// if the action has slots specified
	if (Action.Slots.Length > 0)
	{
		// use only those indicies
		SlotIndices = Action.Slots;
	}
	else
	{
		// otherwise use all the slots
		for (Idx = 0; Idx < Slots.Length; Idx++)
		{
			SlotIndices[SlotIndices.Length] = Idx;
		}
	}
	for (Idx = 0; Idx < SlotIndices.Length; Idx++)
	{
		SlotIdx = SlotIndices[Idx];
		if (SlotIdx >= 0 && SlotIdx < Slots.Length)
		{
			if (Action.InputLinks[0].bHasImpulse)
			{
				SetSlotEnabled(SlotIdx, TRUE);
			}
			else
			if (Action.InputLinks[1].bHasImpulse)
			{
				SetSlotEnabled(SlotIdx, FALSE);
			}
			else
			if (Action.InputLinks[2].bHasImpulse)
			{
				// update the slot
				if (AutoAdjustSlot(SlotIdx,FALSE) &&
					Slots[SlotIdx].SlotOwner != None && Slots[SlotIdx].SlotOwner.Controller != None)
				{
					// and notify if it changed
					Slots[SlotIdx].SlotOwner.Controller.NotifyCoverAdjusted();
				}
			}
			else
			if (Action.InputLinks[3].bHasImpulse)
			{
				if( Action.ManualCoverType != CT_None )
				{
					Slots[SlotIdx].CoverType = Action.ManualCoverType;
					if (Slots[SlotIdx].SlotOwner != None && Slots[SlotIdx].SlotOwner.Controller != None)
					{
						// notify the owner of the change
						Slots[SlotIdx].SlotOwner.Controller.NotifyCoverAdjusted();
					}
				}
				Slots[SlotIdx].bPlayerOnly = Action.bManualAdjustPlayersOnly;
			}
		}
	}

	CoverReplicator = WorldInfo.Game.GetCoverReplicator();
	if (CoverReplicator != None)
	{
		if (Action.InputLinks[0].bHasImpulse)
		{
			CoverReplicator.NotifyEnabledSlots(self, SlotIndices);
		}
		else if (Action.InputLinks[1].bHasImpulse)
		{
			CoverReplicator.NotifyDisabledSlots(self, SlotIndices);
		}
		else if (Action.InputLinks[2].bHasImpulse)
		{
			CoverReplicator.NotifyAutoAdjustSlots(self, SlotIndices);
		}
		else if (Action.InputLinks[3].bHasImpulse)
		{
			CoverReplicator.NotifySetManualCoverTypeForSlots(self, SlotIndices, Action.ManualCoverType);
		}
	}
}

/**
 * Auto-adjusts the slot orientation/location to the nearest geometry, as well
 * as determine leans and cover type.  Returns TRUE if the cover type changed.
 */
native final function bool AutoAdjustSlot(int SlotIdx, bool bOnlyCheckLeans);
native final function bool IsEnabled();


/**
 * Overridden to disable all slots when toggled off.
 */
function OnToggle(SeqAct_Toggle inAction)
{
	local CoverReplicator CoverReplicator;
	local int SlotIdx;

	Super.OnToggle( inAction );

	if (inAction.InputLinks[0].bHasImpulse)
	{
		bDisabled = FALSE;
	}
	else if (inAction.InputLinks[1].bHasImpulse)
	{
		bDisabled = TRUE;
	}
	else
	{
		bDisabled = !bDisabled;
	}

	// Call SetSlotEnabled() which notifies any Pawns using this cover
	for (SlotIdx = 0; SlotIdx < Slots.Length; ++SlotIdx)
	{
		SetSlotEnabled(SlotIdx, !bDisabled);
	}

	CoverReplicator = WorldInfo.Game.GetCoverReplicator();
	if (CoverReplicator != None)
	{
		CoverReplicator.NotifyLinkDisabledStateChange(self);
	}
}

function CreateCheckpointRecord(out CheckpointRecord Record)
{
	Super.CreateCheckpointRecord(Record);
	Record.bDisabled = bDisabled;
}

function ApplyCheckpointRecord(const out CheckpointRecord Record)
{
	local CoverReplicator CoverReplicator;

	Super.ApplyCheckpointRecord(Record);

	bDisabled = Record.bDisabled;

	CoverReplicator = WorldInfo.Game.GetCoverReplicator();
	if (CoverReplicator != None)
	{
		CoverReplicator.NotifyLinkDisabledStateChange(self);
	}
}

simulated event ShutDown()
{
	Super.ShutDown();

	bDisabled = TRUE;
}

simulated native function bool GetSwatTurnTarget( int SlotIdx, int Direction, out CoverInfo out_Info );

/** Applies an impulse to all nearby fractureable objects, if this coverlink is set to fracture on touch
*
* @param   Origin - Origin of fracture pulse
* @param   Radius - Radius around origin to apply the fracturable pulse. All parts in radius will fracture
* @param   RBStrength - strength to apply to fractureable parts
* @param   DamageType - DamageType to use as the fracturable pulse, potentially ignored by certain fractureable objects
*/
simulated function BreakFracturedMeshes(vector Origin, float Radius, float RBStrength, class<DamageType> DamageType)
{
	local FracturedStaticMeshActor FracActor;
	local byte bWantPhysChunksAndParticles;

	if (!bFractureOnTouch)
	{
		return;
	}

	foreach CollidingActors(class'FracturedStaticMeshActor', FracActor, Radius, Origin, TRUE)
	{
		if((FracActor.Physics == PHYS_None) && FracActor.IsFracturedByDamageType(DamageType))
		{
			// Make sure the impacted fractured mesh is visually relevant
			if( FracActor.FractureEffectIsRelevant( FALSE, Instigator, bWantPhysChunksAndParticles ) )
			{
				FracActor.BreakOffPartsInRadius(Origin, Radius, RBStrength, bWantPhysChunksAndParticles == 1 ? TRUE : FALSE);
			}
		}
	}
}

//debug
`if (`notdefined(FINAL_RELEASE))
simulated event Tick( float DeltaTime )
{
	local int SlotIdx;
	local CoverSlot Slot;
	local Vector OwnerLoc;
	local byte R, G, B;

	// no super tick implemented
	//super.Tick( DeltaTime );

	if( bDebug )
	{
		for( SlotIdx = 0; SlotIdx < Slots.Length; SlotIdx++ )
		{
			Slot = Slots[SlotIdx];
			if( Slot.SlotOwner != None )
			{
				if( Slot.SlotOwner != None )
				{
					OwnerLoc = Slot.SlotOwner.Location;
					R = 166;
					G = 236;
					B = 17;
				}
				else
				{
					OwnerLoc = vect(0,0,0);
					R = 170;
					G = 0;
					B = 0;
				}

				DrawDebugLine( GetSlotLocation(SlotIdx), OwnerLoc, R, G, B );
			}
		}
	}
}
`endif

native final function int AddCoverSlot(vector SlotLocation, rotator SlotRotation, optional int SlotIdx = -1, optional bool bForceSlotUpdate, optional Scout Scout);

simulated final event string GetDebugString(int SlotIdx)
{
	return "L:"$GetRightMost(self)@"S:"$SlotIdx;
}

simulated native final function ECoverLocationDescription GetLocationDescription(int SlotIdx);

simulated event string GetDebugAbbrev()
{
	return "CL";
}

defaultproperties
{
	Components.Remove(PathRenderer)

	Begin Object Name=CollisionCylinder
		CollisionRadius=48.f
		CollisionHeight=58.f
	End Object

	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorMaterials.CoverIcons.CoverNodeNoneLocked'
		SpriteCategoryName="Cover"
	End Object

	Begin Object Class=CoverMeshComponent Name=CoverMesh
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		bUsePrecomputedShadows=False
	End Object
	Components.Add(CoverMesh)
	// Don't show the navigation point arrow on cover links.
	Components.Remove(Arrow)
	Slots(0)=(LocationOffset=(X=64.f))

	AlignDist=36.f
	StandHeight=160.f
	MidHeight=70.f
	AutoCoverSlotInterval=175.f

	StandingLeanOffset=(X=0,Y=78,Z=69)
	CrouchLeanOffset=(X=0,Y=70,Z=19)
	PopupOffset=(X=0,Y=0,Z=70)

	SlipDist=60.f
	TurnDist=512.f

	bAutoSort=TRUE
	bAutoAdjust=TRUE
	bSpecialMove=TRUE
	bBuildLongPaths=FALSE

	MaxFireLinkDist=2048.f
	InvalidateDistance=64.f
	DangerScale=2.f

//debug
//	bDebug=TRUE
//	bStatic=FALSE

	bDebug_FireLinks=FALSE
	bDebug_ExposedLinks=FALSE

	bDestinationOnly=TRUE

	LeanTraceDist=64.f

	bDoAutoSlotDensityFixup=FALSE
}
