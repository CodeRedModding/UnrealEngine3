class PC_MSScheme extends WarPC;

/** Time player has to hold "up" to enter into cover */
var(Cover)	config		float	CoverEnterHoldTime;
/** counter for the above */
var			transient	float	CoverEnterTimeCount;

/** threshold to break from cover. */
var(Cover)	config		float	CoverBreakThreshold;
/** bool when desiring to break from cover, to delay until full input/movement has been processed */
var			transient	bool	bBreakFromCover;

/** Timer to disable PopUp from cover when first enterring cover */
var(Cover)	config		float	DisablePopUpTime;
var			transient	float	DisablePopUpCount;

/** dot (Angle) at which we consider the player to be in the opposite direction of cover (to mess with input). */
var(Cover)	config		float	OppositeDirCoverThreshold;

/** always scan for cover */
event PlayerTick( float DeltaTime )
{
	super.PlayerTick( DeltaTime );

	// if player presses up, check for available cover
	if( Pawn != None && !IsInCoverState() && 
		Abs(WarPlayerInput(PlayerInput).RawJoyUp) > Abs(WarPlayerInput(PlayerInput).RawJoyRight) &&
		WarPlayerInput(PlayerInput).RawJoyUp > 0 &&
		FindCoverSpot() )
	{
		CoverEnterTimeCount += DeltaTime;
		if( CoverEnterTimeCount > CoverEnterHoldTime )
		{
			// if valid cover and "Up" held for more than CoverEnterHoldTime, then enter cover
			CoverEnterTimeCount = 0.f;
			StartCover( true );
		}
	}
	else
	{
		CoverEnterTimeCount = 0.f;
	}
}

/** cover key pressed */
exec function CoverKeyPressed()
{
	if( Pawn != None )
	{
		Pawn.ShouldCrouch( true );
	}
}

/** cover key released */
exec function CoverKeyReleased()
{
	if( Pawn != None )
	{
		Pawn.UnCrouch();
	}
}

/** @see WarPC::FailedStartCover */
function FailedStartCover( bool bOneShot );

/** @see WarPC::SuccessfulStartCover */
function SuccessfulStartCover()
{
	super.SuccessfulStartCover();

	// Disable 'PopUp' from cover for a little while
	DisablePopUpCount = DisablePopUpTime;
}

/** @see WarPC.NotifyEvadeFinished() */
simulated function NotifyEvadeFinished()
{
	super.NotifyEvadeFinished();
	// try to find cover when evade is finished
	StartCover( true );
}

/**
 * Checks if can transition to Left or Right node.
 * Also used to check if can Lean, as no transition means a potential lean.
 *
 * @param	bRight	Check a transition to the right.
 * @param	PawnCT	Pawn Cover Type
 */
function bool CanTransitionTo( bool bRight, CoverNode.ECoverType PawnCT )
{
	local CoverNode	DesiredNode;

	// Find node we can protentially transition to
	if( bRight )
	{
		DesiredNode = PrimaryCover.RightNode;
	}
	else
	{
		DesiredNode = PrimaryCover.LeftNode;
	}

	// If no node.. then cannot transition
	if( DesiredNode == None )
	{
		return false;
	}

	return true;
}

/** returns true if player can fire his weapon */
function bool CanFireWeapon()
{
	local bool		bIsInFront;
	local vector2d	AngularDist;

	bIsInFront = GetViewToCoverAngularDistance( AngularDist );

	// make sure we can fire
	if( GetPawnCoverType() == CT_Standing && 
		GetCoverDirection() == CD_Default &&
		bIsInFront )
	{
		return false;
	}

	return super.CanFireWeapon();
}

/** Returns dot between view direction and cover direction */
final function bool GetViewToCoverAngularDistance( out vector2d AngularDist )
{
	local vector	CamLoc, CX, CY, CZ;
	local rotator	CamRot;
	local bool		bIsInFront;

	if( !IsInCoverState() )
	{
		return true;
	}

	GetPlayerViewPoint( CamLoc, CamRot );
	GetCoverAxes(CX, CY, CZ);

	bIsInFront = GetAngularDistance(AngularDist, Vector(CamRot), CX, CY, CZ );
	GetAngularDegreesFromRadians( AngularDist );

	return bIsInFront;
}

/**
 * This is an abstract state derived by various cover states to provide
 * a generic interface.  
 * Should not be considered as a valid state for transitioning to.
 */
simulated state Cover
{
	/** overriden to delay cover breaking after the whole player input update */
	event PlayerTick( float DeltaTime )
	{
		super.PlayerTick( DeltaTime );

		// if in opposite direction of cover, then up means break from cover...
		if( IsInOppositeCoverDir() &&
			Abs(WarPlayerInput(PlayerInput).RawJoyUp) > Abs(WarPlayerInput(PlayerInput).RawJoyRight) &&
			WarPlayerInput(PlayerInput).RawJoyUp > CoverBreakThreshold )
		{
			bBreakFromCover = true;
		}

		// if we should break from cover, let's do it now
		if( bBreakFromCover )
		{
			bBreakFromCover = false;
			StopCover();
		}

		// timer to disable PopUp when first enterring cover
		if( DisablePopUpCount > 0 )
		{
			DisablePopUpCount -= DeltaTime;
			if( WarPlayerInput(PlayerInput).RawJoyUp <= 0 )
			{
				DisablePopUpCount = 0.f;
			}
		}
	}

	/** Returns true if player is considered in the opposite direction of cover */
	final function bool IsInOppositeCoverDir()
	{
		local vector	CamLoc, CX, CY, CZ;
		local rotator	CamRot;

		GetPlayerViewPoint( CamLoc, CamRot );
		GetCoverAxes(CX, CY, CZ);

		if( CX dot Vector(CamRot) < -OppositeDirCoverThreshold )
		{
			return true;
		}

		return false;
	}

	/** 
	 * Update player's firing posture
	 */
	function UpdateFiringPosture
	(
		out	CoverNode.ECoverType	out_PawnCT,
		out CoverNode.ECoverAction	out_PawnCA,
		out	ECoverDirection			out_PawnCD
	)
	{
		if( bFire != 0 )
		{
			// if firing with no lean, set proper blind firing direction
			// (default if 'up')
			if( out_PawnCA == CA_Default )
			{
				if( out_PawnCD == CD_Right )
				{
					out_PawnCA = CA_BlindRight;
				}
				else if( out_PawnCD == CD_Left )
				{
					out_PawnCA = CA_BlindLeft;
				}
			}
		}

		// Targeting mode forces lean
		if( bTargetingMode )
		{
			if( out_PawnCD == CD_Up || 
				(out_PawnCD == CD_Default && out_PawnCT == CT_MidLevel) )
			{
				out_PawnCD = CD_Up;
				out_PawnCA = CA_PopUp;
			}
			else if( out_PawnCD == CD_Right )
			{
				out_PawnCA = CA_LeanRight;
			}
			else if( out_PawnCD == CD_Left )
			{
				out_PawnCA = CA_LeanLeft;
			}
		}
	}

	/** Pick right or left cover direction based on where camera is looking */
	function SetBestCoverDirection()
	{
		local vector			CamLoc, X, Y, Z;
		local Rotator			CamRot;

		// Non standing cover types don't mind if we start with CD_Default.
		if( PrimaryCover.CoverType != CT_Standing )
		{
			return;
		}

		GetCoverAxes( X, Y, Z );
		GetPlayerViewPoint( CamLoc, CamRot );

		// looking to the right of cover
		if( Vector(CamRot) Dot Y > 0 )
		{
			if( PrimaryCover.RightNode == None || 
				PrimaryCover.bCircular )
			{
				SetCoverDirection( CD_Right );
			}
		}
		else	// looking to the left
		{
			if( PrimaryCover.LeftNode == None || 
				PrimaryCover.bCircular )
			{
				SetCoverDirection( CD_Left );
			}
		}
	}

	function BeginState()
	{
		super.BeginState();
		// Making sure we have a valid cover direction (ie cannot be default when standing, because cannot blind fire)
		SetBestCoverDirection();
	}
}

/**
 * This state handles the player at a single node, modifying the
 * camera/anims/etc to support all the actions available at the current
 * node (PrimaryCover).  Also handles transitions to new cover when
 * the player attempts to move away from cover, and there is a valid
 * link available, at which point the state becomes Cover_Moving.
 */
simulated state Cover_Stationary extends Cover
{
	/** 
	 * Update Player posture. Return true if allowed to move
	 *
	 * @param out	out_PawnCT	Pawn's Cover type
	 * @param out	out_PawnCA	Pawn's Cover Action
	 * @param out	out_PawnCD	Cover Direction
	 * 
	 * @return true if allowed to move
	 */
	function bool UpdatePlayerPosture
	(
			float					DeltaTime,
		out	CoverNode.ECoverType	out_PawnCT,
		out CoverNode.ECoverAction	out_PawnCA,
		out	ECoverDirection			out_PawnCD
	)
	{
		local CoverNode.ECoverType		NodeCT;
		local bool						bAllowMovement;

		NodeCT = GetNodeCoverType();

		// if not holding a direction, clear cover action
		if( WarPlayerInput(PlayerInput).RawJoyUp == 0 &&
			WarPlayerInput(PlayerInput).RawJoyRight == 0 )
		{
			out_PawnCA = CA_Default;
			
			// Update firing posture with our changes
			UpdateFiringPosture( out_PawnCT, out_PawnCA, out_PawnCD );
			return false;
		}

		// if moving away then break cover
		if( Abs(WarPlayerInput(PlayerInput).RawJoyUp) > Abs(WarPlayerInput(PlayerInput).RawJoyRight) && 
			WarPlayerInput(PlayerInput).RawJoyUp < -CoverBreakThreshold )	// down
		{
			bBreakFromCover = true;
			return false;
		}

		// split up vert/horiz joy axis, and see which one is dominant
		if( Abs(WarPlayerInput(PlayerInput).RawJoyUp) > Abs(WarPlayerInput(PlayerInput).RawJoyRight) )
		{
			if( WarPlayerInput(PlayerInput).RawJoyUp > 0.0f &&
				DisablePopUpCount <= 0 )	// up
			{	
				if( NodeCT == CT_MidLevel )	// stand up
				{
					out_PawnCD = CD_Up;
					out_PawnCA = CA_PopUp;
				}
			}
		}
		else
		{
			if( WarPlayerInput(PlayerInput).RawJoyRight > 0.0f ) // right
			{
				if( !bTargetingMode && CanTransitionTo( true, out_PawnCT) )
				{
					// Transition to right node
					if( PrimaryCover.bLeanRight )
					{
						// if primary cover offers a right lean, add a delay before allowing transition. So we can choose to lean/shoot.
						CoverTransitionCountHold += DeltaTime;
						out_PawnCD = CD_Right;
					}
					else
					{
						CoverTransitionCountHold = 0.f;
						if( out_PawnCD == CD_Left )
						{
							CoverTransitionCountDown = CoverTransitionTime;			
							out_PawnCD = CD_Default;
						}
					}

					// if we can transition, do so
					if( (CoverTransitionCountHold == 0.f && CoverTransitionCountDown == 0.f && (out_PawnCD == CD_Default || out_PawnCD == CD_Up)) ||
						(CoverTransitionCountHold > CoverTransitionTime) )
					{
						// Transition to moving
						bAllowMovement	= true;
						out_PawnCD		= CD_Default;
					}
					else
					{
						out_PawnCA	= CA_Default;
					}
				}
				else if( NodeCT != CT_Crouching )
				{
					// if looking from left, just go to default.
					if( out_PawnCD == CD_Left )
					{
						out_PawnCD = CD_Default;
						out_PawnCA = CA_Default;

						// if we were looking left, and middle cover, 
						// then transition to CD_Default first. to give a chance to use CA_PopUp.
						if( NodeCT == CT_MidLevel )
						{
							CoverTransitionCountDown = CoverTransitionTime;
						}
					}

					// If primary cover offers a right lean
					if( CoverTransitionCountDown == 0.f && PrimaryCover.bLeanRight )
					{
						out_PawnCD	= CD_Right;
						out_PawnCA	= CA_LeanRight;
					}
				}
			}
			else if( WarPlayerInput(PlayerInput).RawJoyRight < 0.f ) // left
			{
				if( !bTargetingMode && CanTransitionTo( false, out_PawnCT) )
				{
					// Transition to left node
					if( PrimaryCover.bLeanLeft )
					{
						// if primary cover offers a left lean, add a delay before allowing transition. So we can choose to lean/shoot.
						CoverTransitionCountHold += DeltaTime;
						out_PawnCD = CD_Left;
					}
					else
					{
						CoverTransitionCountHold = 0.f;
						if( out_PawnCD == CD_Right )
						{
							CoverTransitionCountDown = CoverTransitionTime;			
							out_PawnCD = CD_Default;
						}
					}

					// if we can transition, do so
					if( (CoverTransitionCountHold == 0.f && CoverTransitionCountDown == 0.f && (out_PawnCD == CD_Default || out_PawnCD == CD_Up)) ||
						(CoverTransitionCountHold > CoverTransitionTime) )
					{
						// Transition to moving
						bAllowMovement	= true;
						out_PawnCD		= CD_Default;
					}
					else
					{
						out_PawnCA	= CA_Default;
					}
				}
				else if( NodeCT != CT_Crouching )
				{
					// if looking from right, just go to default.
					if( out_PawnCD == CD_Right )
					{
						out_PawnCD = CD_Default;
						out_PawnCA = CA_Default;

						// if we were looking left, and middle cover, 
						// then transition to CD_Default first. to give a chance to use CA_PopUp.
						if( NodeCT == CT_MidLevel )
						{
							CoverTransitionCountDown = CoverTransitionTime;
						}
					}

					// If primary cover offers a right left
					if( CoverTransitionCountDown == 0.f && PrimaryCover.bLeanLeft )
					{
						out_PawnCD	= CD_Left;
						out_PawnCA	= CA_LeanLeft;
					}
				}
			}
		}

		// Update firing posture with our changes
		UpdateFiringPosture( out_PawnCT, out_PawnCA, out_PawnCD );

		return bAllowMovement;
	}

	/** @see PlayerController::ProcessMove */
	function ProcessMove ( float DeltaTime, vector newAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		local float		PawnRelDirDot;
		local CoverNode	NewSecondary;

		if( Pawn == None )
		{
			return;
		}

		Pawn.Acceleration = newAccel;

		// Only update if we move...
		if( IsZero(newAccel) )
		{
			return;
		}

		// exit if non locally controlled
		if( LocalPlayer(Player) == None )
		{
			return;
		}
		
		// bug: we cannot guarantee the acceleration that comes here was initiated by
		// a desire to transition out of stationary cover.. it may come from PlayerWalking...

		// Figure out where we're trying to go
		PawnRelDirDot = newAccel dot GetCoverTangent();
		if( PawnRelDirDot > 0 )
		{
			NewSecondary = PrimaryCover.RightNode;
		}
		else if( PawnRelDirDot < 0 )
		{
			NewSecondary = PrimaryCover.LeftNode;
		}

		if( NewSecondary != None )
		{
			TransitionToCover( PrimaryCover, NewSecondary, 'Cover_Moving', true );
		}
	}

	function BeginState()
	{
		super.BeginState();
		Pawn.SetLocation( PrimaryCover.Location );
		Pawn.Acceleration	= vect(0,0,0);
		Pawn.Velocity		= vect(0,0,0);
	}
}

/**
 * This state represents any time the player is moving between two
 * linked nodes, remapping input to move the player perfectly between
 * the nodes.  The state will transition to Cover_Stationary once a 
 * player moves within touching distance of an end node.
 * Note that PrimaryCover and SecondaryCover are updated.
 * In this state, the player is always in between 2 cover nodes, 
 * and PrimaryCover is the closest one.
 */
simulated state Cover_Moving extends Cover
{
	/** Return current covernode's cover type */
	function CoverNode.ECoverType GetNodeCoverType()
	{
		// if no secondary cover
		if( SecondaryCover == None )
		{
			return PrimaryCover.CoverType;
		}
		else
		{
			// check if we're in a mid->crouch link
			if( PrimaryCover.CoverType == CT_MidLevel ||
				SecondaryCover.CoverType == CT_MidLevel )
			{
				if( PrimaryCover.CoverType == CT_Crouching || 
					SecondaryCover.CoverType == CT_Crouching )
				{
					// return crouch type
					// as that has precedence
					return CT_Crouching;
				}
				else
				{
					return CT_MidLevel;
				}
			}
			else
			// check for either crouching
			if( PrimaryCover.CoverType == CT_Crouching ||
				SecondaryCover.CoverType == CT_Crouching )
			{
				return CT_Crouching;
			}
		}

		return CT_Standing;
	}

	/** 
	 * Get cover axes (Z up and Y pointing right) 
	 * 
	 * @param	out_X - normalized vector along x axis
	 * @param	out_Y - normalized vector along y axis
	 * @param	out_Z - normalized vector along z axis
	 */
	function GetCoverAxes(out vector out_X, out vector out_Y, out vector out_Z)
	{
		if( SecondaryCover == PrimaryCover.LeftNode )
		{
			out_Y = Normal(PrimaryCover.Location - SecondaryCover.Location);
		}
		else
		{
			out_Y = Normal(SecondaryCover.Location - PrimaryCover.Location);
		}
		out_Z = vect(0,0,1);
		out_X = out_Y cross out_Z;
	}

	/** 
	 * Update Player posture. Return true if allowed to move
	 *
	 * @param out	out_PawnCT	Pawn's Cover type
	 * @param out	out_PawnCA	Pawn's Cover Action
	 * @param out	out_PawnCD	Cover Direction
	 * 
	 * @return true if allowed to move
	 */
	function bool UpdatePlayerPosture
	(
			float					DeltaTime,
		out	CoverNode.ECoverType	out_PawnCT,
		out CoverNode.ECoverAction	out_PawnCA,
		out	ECoverDirection			out_PawnCD
	)
	{
		local CoverNode.ECoverType		NodeCT;
		local bool						bAllowMovement;

		// if moving away then break cover
		if( Abs(WarPlayerInput(PlayerInput).RawJoyUp) > Abs(WarPlayerInput(PlayerInput).RawJoyRight) && 
			WarPlayerInput(PlayerInput).RawJoyUp < -CoverBreakThreshold )	// down
		{
			bBreakFromCover = true;
			return false;
		}

		NodeCT		= GetNodeCoverType();
		out_PawnCD	= CD_Default;
		out_PawnCT	= GetNodeCoverType();

		if( WarPlayerInput(PlayerInput).RawJoyUp > Abs(WarPlayerInput(PlayerInput).RawJoyRight) )	// up
		{	
			if( NodeCT == CT_MidLevel &&
				DisablePopUpCount <= 0 )
			{
				out_PawnCD = CD_Up;
				out_PawnCA = CA_PopUp;
			}
		}
		else
		{
			out_PawnCA = CA_Default;
		}

		// Update firing posture with our changes
		UpdateFiringPosture( out_PawnCT, out_PawnCA, out_PawnCD );

		// Do not move if pointing stick more vertically then horizontally
		bAllowMovement = (Abs(WarPlayerInput(PlayerInput).RawJoyUp) < Abs(WarPlayerInput(PlayerInput).RawJoyRight));
	
		return bAllowMovement;
	}

	/** @see PlayerController::ProcessMove */
	function ProcessMove ( float DeltaTime, vector newAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		local CoverNode		NewPrimary, NewSecondary;
		local float			DistToPrimary, DistToSecondary, PawnRelDirDot;
		local name			newState;
		local vector		TestLocation, EstimatedVelocity;

		if( Pawn == None )
		{
			return;
		}

		Pawn.Acceleration = newAccel;

		// Only update if we move...
		if( IsZero(newAccel) && IsZero(Pawn.Velocity) )
		{
			return;
		}

		// exit if non locally controlled
		if( LocalPlayer(Player) == None )
		{
			return;
		}

		EstimatedVelocity	= Pawn.Velocity + newAccel*DeltaTime;
		TestLocation		= Pawn.Location + EstimatedVelocity*DeltaTime;
		NewPrimary			= PrimaryCover;
		NewSecondary		= SecondaryCover;
		DistToPrimary		= VSize2D( PrimaryCover.Location - TestLocation );

		// switched sides!
		if( !IsZero(PrimaryCover.Location - TestLocation) && 
			(PrimaryCover.Location - TestLocation) dot (PrimaryCover.Location - SecondaryCover.Location) < 0.f )
		{
			coverlog("passed by primary cover", "ProcessMove");
			PawnRelDirDot = EstimatedVelocity dot GetCoverTangent();
			// set new secondary node
			// player is always in between 2 nodes. Primary being the closest.
			if( PawnRelDirDot > 0 )
			{
				NewSecondary = PrimaryCover.RightNode;
			}
			else if( PawnRelDirDot < 0 )
			{
				NewSecondary = PrimaryCover.LeftNode;
			}

						// If no new secondary or a lean at current primary, then transition to stationary
			if( NewSecondary == None ||
				PrimaryCover.bLeanRight ||
				PrimaryCover.bLeanLeft )
			//if( (PawnRelDirDot > 0 && !CanTransitionTo( true, GetPawnCoverType() )) ||
			//	(PawnRelDirDot < 0 && !CanTransitionTo( false, GetPawnCoverType() )) )
			{
				// Transition to stationary state, can't move further
				// FIXME -- stand state transition? or just have a single cover state with no transitions?
				newState = 'Cover_Stationary';
			}
		}
		else
		{
			// in between 2 nodes, update which one is the closest
			DistToSecondary = VSize2D( SecondaryCover.Location - TestLocation );
			if( DistToSecondary < DistToPrimary )
			{	
				NewPrimary		= SecondaryCover;
				NewSecondary	= PrimaryCover;
			}	
		}

		if( newState != '' ||
			PrimaryCover != NewPrimary ||
			SecondaryCover != NewSecondary )
		{
			TransitionToCover( NewPrimary, NewSecondary, newState, true );
		}
	}
}

simulated state Cover_Circular extends Cover
{
	function vector GetCoverOrigin()
	{
		return (PrimaryCover.Location + SecondaryCover.Location)/2.f;
	}

	function float GetCoverRadius()
	{
		return VSize2D(PrimaryCover.Location - SecondaryCover.Location)/2.f;
	}

	/** 
	 * Get cover axes (Z up and Y pointing right) 
	 * 
	 * @param	out_X - normalized vector along x axis
	 * @param	out_Y - normalized vector along y axis
	 * @param	out_Z - normalized vector along z axis
	 */
	function GetCoverAxes(out vector out_X, out vector out_Y, out vector out_Z)
	{
		out_X = Normal(GetCoverOrigin() - Pawn.Location);
		out_Z = vect(0,0,1);

		out_Y = out_Z cross out_X;		
	}

	/** 
	 * Update Player posture. Return true if allowed to move
	 *
	 * @param out	out_PawnCT	Pawn's Cover type
	 * @param out	out_PawnCA	Pawn's Cover Action
	 * @param out	out_PawnCD	Cover Direction
	 * 
	 * @return true if allowed to move
	 */
	function bool UpdatePlayerPosture
	(
			float					DeltaTime,
		out	CoverNode.ECoverType	out_PawnCT,
		out CoverNode.ECoverAction	out_PawnCA,
		out	ECoverDirection			out_PawnCD
	)
	{
		local CoverNode.ECoverType		NodeCT;
		local bool						bCanMove;

		// if not holding a direction, clear cover action
		if( WarPlayerInput(PlayerInput).RawJoyUp == 0 &&
			WarPlayerInput(PlayerInput).RawJoyRight == 0 )
		{
			out_PawnCA = CA_Default;
			
			// Update firing posture with our changes
			UpdateFiringPosture( out_PawnCT, out_PawnCA, out_PawnCD );
			return false;
		}

		// if moving away then break cover
		if( Abs(WarPlayerInput(PlayerInput).RawJoyUp) > Abs(WarPlayerInput(PlayerInput).RawJoyRight) && 
			WarPlayerInput(PlayerInput).RawJoyUp < -CoverBreakThreshold )	// down
		{
			bBreakFromCover = true;
			return false;
		}

		NodeCT = GetNodeCoverType();

		// split up vert/horiz joy axis, and see which one is dominant
		if( Abs(WarPlayerInput(PlayerInput).RawJoyUp) > Abs(WarPlayerInput(PlayerInput).RawJoyRight) )
		{
			if( WarPlayerInput(PlayerInput).RawJoyUp > 0.f &&
				DisablePopUpCount <= 0  )	// up
			{	
				if( NodeCT == CT_MidLevel )
				{
					out_PawnCD = CD_Up;
					out_PawnCA = CA_PopUp;
				}
			}
		}
		else
		{
			if( WarPlayerInput(PlayerInput).RawJoyRight > 0.f ) // right
			{
				if( out_PawnCD == CD_Left )
				{
					CoverTransitionCountDown = CoverTransitionTime;
					if( NodeCT == CT_MidLevel )
					{
						// if looking from left, transition to middle first
						out_PawnCD = CD_Default;
					}
					else
					{
						out_PawnCD = CD_Right;
					}
				}

				// if no delay, then transition to right and allow movement
				if( CoverTransitionCountDown == 0.f )
				{
					out_PawnCD = CD_Right;
					if( !bTargetingMode )
					{
						bCanMove = true;
					}
					else
					{
						out_PawnCA	= CA_LeanRight;
					}
				}
			}
			else if( WarPlayerInput(PlayerInput).RawJoyRight < 0.f ) // left
			{
				if( out_PawnCD == CD_Right )
				{
					CoverTransitionCountDown = CoverTransitionTime;
					if( NodeCT == CT_MidLevel )
					{
						// if looking from right, transition to middle first
						out_PawnCD = CD_Default;
					}
					else
					{
						out_PawnCD = CD_Left;
					}
				}

				// if no delay, then transition to left and allow movement
				if( CoverTransitionCountDown == 0.f )
				{
					out_PawnCD = CD_Left;
					if( !bTargetingMode )
					{
						bCanMove = true;
					}
					else
					{
						out_PawnCA	= CA_LeanLeft;
					}
				}
			}
		}

		// Update firing posture with our changes
		UpdateFiringPosture( out_PawnCT, out_PawnCA, out_PawnCD );

		return bCanMove;
	}

	function ProcessMove ( float DeltaTime, vector newAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		super.ProcessMove ( DeltaTime, newAccel, DoubleClickMove, DeltaRot);
		FixPlayerLocation();
	}

	function FixPlayerLocation()
	{
		local vector	Origin;
		local float		Radius;

		Origin		= GetCoverOrigin();
		Origin.Z	= Pawn.Location.Z;
		Radius		= GetCoverRadius();

		if( VSize(Pawn.Location - Origin) > Radius )
		{
			Pawn.SetLocation(Origin + (Normal(Pawn.Location - Origin)*GetCoverRadius()) );
		}
	}
}

defaultproperties
{
	DisablePopUpTime=0.5
	CoverEnterHoldTime=0.28
	CoverTransitionTime=0.4
	CoverBreakThreshold=0.5
	OppositeDirCoverThreshold=0.5
}