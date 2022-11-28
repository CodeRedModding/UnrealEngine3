/**
 * Cover node encapsulates all information for a pathnode that can be
 * used as cover by a player.  All linking and detection of what kind
 * cover provided by this node is calculated at path build time, and 
 * stored in the various properties (all const variable basically).
 */
class CoverNode extends PathNode
	native;

cpptext
{
	void InitialPass();
	void LinkPass();
	void FinalPass();
	void UpdateSprite();
	virtual void PostEditChange(UProperty *PropertyThatChanged)
	{
		UpdateSprite();
		Super::PostEditChange(PropertyThatChanged);
	}
	void FindBase();
}

/** If true, the node will automatically adjust and link up with other cover nodes */
var() const bool bAutoSetup;

/** Additional weighting to apply when AI is evaluation cover */
var() const byte AIRating;

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
	/** Full crouching cover */
	CT_Crouching,
};
var() const ECoverType CoverType;

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
	/** Stepping out to the left */
	CA_StepLeft,
	/** Stepping out to the right */
	CA_StepRight,
	/** Pop up, out of cover */
	CA_PopUp,
};

/** Distance used when aligning to nearby surfaces */
var const float AlignDist;

/** Distance used when aligning node to edges. Should match up player collision cylinder */
var const float EdgeAlignDist;

/** Distance used when aligning against corners */
var const float CornerAlignDist;

/** Distance threshold to treat two nodes as transitions */
var const float TransitionDist;

/** Min height for nearby geometry to categorize as standing cover */
var const float StandHeight;

/** Min height for nearby geometry to categorize as mid-level cover */
var const float MidHeight;

/** Min height for nearby geometry to categorize as crouch cover */
var const float CrouchHeight;

/** Linked node to the left of this node */
var() const CoverNode LeftNode;

/** Linked node to the right of this node */
var() const CoverNode RightNode;

/** Linked node that can be transitioned to from this node */
var() const CoverNode TransitionNode;

/** Can a player lean to the left at this node */
var() const bool bLeanLeft;

/** Can a player lean to the right at this node */
var() const bool bLeanRight;

/** Is this node current enabled for use? */
var() bool bEnabled;

/** Is this cover node a circular link? */
var() bool bCircular;

/** Current controller claiming this cover node */
var transient Controller CoverClaim;

/**
 * Returns true if this node is claimed by a valid player.
 */
final function bool IsClaimed()
{
	return (CoverClaim != None &&
			CoverClaim.Pawn != None &&
			CoverClaim.Pawn.Health > 0);
}

/**
 * Asserts a claim on this covernode by the specified player,
 * notifying the previous claim if applicable.
 */
final function Claim(Controller newClaim)
{
	if (newClaim != CoverClaim &&
		newClaim != None)
	{
		if (CoverClaim != None)
		{
			CoverClaim.NotifyCoverClaimRevoked(self);
		}
		if (newClaim.Pawn != None &&
			newClaim.Pawn.Health > 0)
		{
			CoverClaim = newClaim;
		}
	}
}

/**
 * Negates the previous claim on this cover node.
 */
final function UnClaim(Controller oldClaim)
{
	if (oldClaim == CoverClaim)
	{
		CoverClaim = None;
	}
}

/**
 * Scripted support for toggling a covernode, checks
 * which operation to perform by looking at the 
 * active input.
 * 
 * Input 1: turn on
 * Input 2: turn off
 * Input 3: toggle
 */
simulated function OnToggle(SeqAct_Toggle action)
{
	if (action.InputLinks[0].bHasImpulse)
	{
		// turn on
		bEnabled = true;
	}
	else
	if (action.InputLinks[1].bHasImpulse)
	{
		// turn off
		bEnabled = false;
	}
	else
	if (action.InputLinks[2].bHasImpulse)
	{
		// toggle
		bEnabled = !bEnabled;
	}
	// if no longer enabled and currently in use, notify the user
	if (!bEnabled &&
		CoverClaim != None)
	{
		CoverClaim.NotifyCoverDisabled(self);
		// and reject the claim
		UnClaim(CoverClaim);
	}
}

/**
 * Overridden to prevent claims other than the current one.
 */
function bool IsAvailableTo(Actor chkActor)
{
	return (CoverClaim == None ||
			chkActor == CoverClaim ||
			chkActor == CoverClaim.Pawn);
}

/**
 * Returns true if the specified cover node is linked in some way to this
 * node.
 */
function bool IsLinkedTo(CoverNode chkCover)
{
	local CoverNode testNode;
	local bool bLinked;
	bLinked = chkCover == self;
	if (!bLinked)
	{
		// run to the right
		testNode = RightNode;
		while (testNode != None &&
			   testNode != self &&
			   !bLinked)
		{
			if (testNode == chkCover)
			{
				bLinked = true;
			}
			else
			{
				testNode = testNode.RightNode;
			}
		}
		// run to the left
		testNode = LeftNode;
		while (testNode != None &&
			   testNode != self &&
			   !bLinked)
		{
			if (testNode == chkCover)
			{
				bLinked = true;
			}
			else
			{
				testNode = testNode.LeftNode;
			}
		}
	}
	return bLinked;
}

defaultproperties
{
	Begin Object Name=CollisionCylinder
		CollisionRadius=48.f
		CollisionHeight=58.f
	End Object

	Begin Object NAME=Sprite LegacyClassName=CoverNode_CoverNodeSprite_Class
		Sprite=Texture2D'EngineResources.CoverNodeNone'
	End Object

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		HiddenGame=true
	End Object
	Components.Add(ArrowComponent0)

	AlignDist=58.f
	EdgeAlignDist=48.f
	CornerAlignDist=58.f
	TransitionDist=32.f

	StandHeight=150.f
	MidHeight=90.f
	CrouchHeight=58.f

	bAutoSetup=true
	bEnabled=true
}
