class PC_DefaultScheme extends WarPC;

/** threshold to break from cover. */
var()	config		float	CoverBreakThreshold;
/** bool when desiring to break from cover, to delay until full input/movement has been processed */
var		transient	bool	bBreakFromCover;


/** @see WarPC.NotifyEvadeFinished() */
simulated function NotifyEvadeFinished()
{
	super.NotifyEvadeFinished();
	// try to find cover when evade is finished
	StartCover( true );
}

/** cover key pressed */
exec function CoverKeyPressed()
{
	if( IsInState('Cover') || IsTimerActive('StartCover') )
	{
		StopCover();
	}
	else
	{
		StartCover();
	}
}

function StartBlindFire();

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

		if( bBreakFromCover )
		{
			bBreakFromCover = false;
			StopCover();
		}
	}

	/** 
	 * Update player's firing posture
	 *
	 * @param out	out_PawnCT	Pawn's Cover type
	 * @param out	out_PawnCA	Pawn's Cover Action
	 * @param out	out_PawnCD	Cover Direction
	 */
	function UpdateFiringPosture	
	(
		out	CoverNode.ECoverType	out_PawnCT,
		out CoverNode.ECoverAction	out_PawnCA,
		out	ECoverDirection			out_PawnCD
	)
	{
		if( !bTargetingMode && 
			(bFire != 0) )
		{
			if( out_PawnCD == CD_Left )
			{
				out_PawnCA = CA_BlindLeft;
			}
			else if ( out_PawnCD == CD_Right )
			{
				out_PawnCA = CA_BlindRight;
			}
			else
			{
				out_PawnCA = CA_Default;
			}
		}
		else
		{
			if( bTargetingMode )
			{
				if( out_PawnCD == CD_Left )
				{
					out_PawnCA = CA_LeanLeft;
				}
				else if( out_PawnCD == CD_Right )
				{
					out_PawnCA = CA_LeanRight;
				}
				else if( out_PawnCT == CT_MidLevel )
				{	
					out_PawnCA = CA_PopUp;
				}
			}
			else
			{
				out_PawnCA = CA_Default;
			}
		}
	}

	/**
	 * Overridden to instigate check for blind fires from standing/midlevel
	 * cover.
	 */
	exec function StartFire( optional byte FireModeNum )
	{
		if( VSize2D(Pawn.Velocity) > 5 )
		{
			return;
		}

		// Can always fire when crouched
		if( PrimaryCover.CoverType == CT_Crouching )
		{
			super.StartFire( FireModeNum );
		}
		else
		{
			if( GetPawnCoverType() == CT_Standing &&
				GetCoverDirection() == CD_Default )
			{
				return;
			}

			if( bTargetingMode )
			{
				// start firing immediately
				super.StartFire( FireModeNum );
			}
			else
			{
				SetTimer(0.35f, false, 'StartBlindFire');
			}
		}
	}

	exec function StopFire( optional byte FireModeNum )
	{
		if( IsTimerActive('StartBlindFire') )
		{
			ClearTimer('StartBlindFire');
		}
		else
		{
			Global.StopFire( FireModeNum );
		}
	}

	function StartBlindFire()
	{
		// if player is still willing to fire, then do so
		if( bFire != 0 )
		{
			Global.StartFire( 1.f );
		}
	}

	function EndState()
	{
		super.EndState();
		ClearTimer('StartBlindFire');
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
	 * Overridden to check for a cover transition.
	 * FIXME LAURENT - what is that?
	 */
	function bool PerformedUseAction()
	{
		if( PrimaryCover != None &&
			PrimaryCover.TransitionNode != None )
		{
			//log("Transitioning to cover:"@PrimaryCover.TransitionNode);
			PrimaryCover.UnClaim(self);
			PrimaryCover = PrimaryCover.TransitionNode;
			PrimaryCover.Claim(self);
			// transition to stationary with new cover
			GotoState('Cover_Stationary',,true);
			return true;
		}
		else
		{
			return Super.PerformedUseAction();
		}
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
		local bool	bAllowMovement;

		// if moving away then break cover
		if( WarPlayerInput(PlayerInput).RawJoyUp < -CoverBreakThreshold )	// down
		{
			bBreakFromCover = true;
			return false;
		}

		if( WarPlayerInput(PlayerInput).RawJoyRight > 0 )	// right
		{
			// Transition to cover moving. Hold direction.
			if( PrimaryCover.RightNode != None )
			{
				CoverTransitionCountHold += DeltaTime;
				if( CoverTransitionCountHold > CoverTransitionTime )
				{
					bAllowMovement	= true;
					out_PawnCD		= CD_Default;
				}
			}

			// if looking from left, just go to default.
			if( out_PawnCD == CD_Left )
			{
				out_PawnCD = CD_Default;
				CoverTransitionCountHold = 0.f;

				// if we were looking left, and middle cover, 
				// then transition to CD_Default first. to give a chance to use CA_PopUp.
				if( out_PawnCT == CT_MidLevel )
				{
					CoverTransitionCountDown = CoverTransitionTime;
				}
			}

			// If primary cover offers a right lean
			if( PrimaryCover.bLeanRight &&
				out_PawnCD == CD_Default && 
				CoverTransitionCountDown == 0.f )
			{
				out_PawnCD = CD_Right;
			}
		}
		else
		if( WarPlayerInput(PlayerInput).RawJoyRight < 0 )	// left
		{
			// Transition to cover moving. Hold direction.
			if( PrimaryCover.LeftNode != None )
			{
				CoverTransitionCountHold += DeltaTime;
				if( CoverTransitionCountHold > CoverTransitionTime )
				{
					bAllowMovement	= true;
					out_PawnCD		= CD_Default;
				}
			}

			// if looking from right, and no left lean possible, just go to default.
			if( out_PawnCD == CD_Right )
			{
				out_PawnCD = CD_Default;
				CoverTransitionCountHold = 0.f;

				// if we were looking right, and middle cover, 
				// then transition to CD_Default first. to give a chance to use CA_PopUp.
				if( out_PawnCT == CT_MidLevel )
				{
					CoverTransitionCountDown = CoverTransitionTime;
				}
			}

			// If primary cover offers a left lean
			if( PrimaryCover.bLeanLeft &&
				out_PawnCD == CD_Default && 
				CoverTransitionCountDown == 0.f )
			{
				out_PawnCD = CD_Left;
			}
		}
		else
		{
			CoverTransitionCountHold = 0.f;
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
		// if moving away then break cover
		if( WarPlayerInput(PlayerInput).RawJoyUp < -CoverBreakThreshold )
		{
			bBreakFromCover = true;
			return false;
		}

		out_PawnCD = CD_Default;
		out_PawnCT = GetNodeCoverType();

		// Update firing posture with our changes
		UpdateFiringPosture( out_PawnCT, out_PawnCA, out_PawnCD );

		return true;
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
				coverlog("new secondary is on the right", "ProcessMove");
				NewSecondary = PrimaryCover.RightNode;
			}
			else if( PawnRelDirDot < 0 )
			{
				coverlog("new secondary is on the left", "ProcessMove");
				NewSecondary = PrimaryCover.LeftNode;
			}

			// If no new secondary or a lean at current primary, then transition to stationary
			if( NewSecondary == None ||
				PrimaryCover.bLeanRight ||
				PrimaryCover.bLeanLeft )
			{
				coverlog("transition to stationary cover..", "ProcessMove");
				// Transition to stationary state, can't move further
				// FIXME -- stand state transition? or just have a single cover state with no transitions?
				/*
				Pawn.Velocity = vect(0,0,0);
				Pawn.Acceleration = vect(0,0,0);
				Pawn.SetLocation( PrimaryCover.Location );
				*/
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

		// FIXME -- need to update this server side with proper claim/unclaim
		if( newState != '' ||
			PrimaryCover != NewPrimary ||
			SecondaryCover != NewSecondary )
		{
			TransitionToCover( NewPrimary, NewSecondary, newState, true );
		}
	}

	function BeginState()
	{
		local vector CoverTangent;

		super.BeginState();

		CoverTangent = GetCoverTangent();
		Pawn.Velocity		= (Pawn.Velocity dot CoverTangent) * CoverTangent;
		Pawn.Acceleration	= (Pawn.Acceleration dot CoverTangent) * CoverTangent;
	}
}


/** Circular cover around a pillar */
simulated state Cover_Circular extends Cover
{
	/**
	 * Returns origin of the current circular cover link.
	 */
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

		NodeCT = GetNodeCoverType();

		// if moving away then break cover
		if( WarPlayerInput(PlayerInput).RawJoyUp < -CoverBreakThreshold )
		{
			bBreakFromCover = true;
			return false;
		}

		if( WarPlayerInput(PlayerInput).RawJoyRight > 0.f ) // right
		{
			if( out_PawnCD == CD_Left )
			{
				out_PawnCD = CD_Default;
				// if looking from left, transition to middle first
				if( NodeCT == CT_MidLevel )
				{
					CoverTransitionCountDown = CoverTransitionTime;
				}
			}

			// if no delay, then transition to right and allow movement
			if( CoverTransitionCountDown == 0.f )
			{
				out_PawnCD = CD_Right;
				bCanMove = true;
			}
		}

		if( WarPlayerInput(PlayerInput).RawJoyRight < 0.f ) // left
		{
			if( out_PawnCD == CD_Right && NodeCT == CT_MidLevel )
			{
				out_PawnCD = CD_Default;
				// if looking from right, transition to middle first
				if( NodeCT == CT_MidLevel )
				{
					CoverTransitionCountDown = CoverTransitionTime;			
				}
			}

			// if no delay, then transition to left and allow movement
			if( CoverTransitionCountDown == 0.f )
			{
				out_PawnCD = CD_Left;
				bCanMove = true;
			}
		}

		// Update firing posture with our changes
		UpdateFiringPosture( out_PawnCT, out_PawnCA, out_PawnCD );

		return bCanMove;
	}

	/** @see PlayerController::ProcessMove */
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
	CoverTransitionTime=0.4
	CoverBreakThreshold=0.5
}