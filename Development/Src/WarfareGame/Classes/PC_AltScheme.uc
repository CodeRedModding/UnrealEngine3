class PC_AltScheme extends WarPC;

/** cover key pressed */
exec function CoverKeyPressed()
{
	StartCover();
}

/** cover key released */
exec function CoverKeyReleased()
{
	StopCover();
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

	// We can lean at next node, make sure we have the correct posture
	// i.e can't lean if crouching for a stand only covertype.
	if( (bRight && PrimaryCover.bLeanRight) ||
		(!bRight && PrimaryCover.bLeanLeft) )
	{
		if( DesiredNode.CoverType != CT_Standing && PawnCT == CT_Standing )
		{
			return false;
		}
	}

	return true;
}

/**
 * This is an abstract state derived by various cover states to provide
 * a generic interface.  
 * Should not be considered as a valid state for transitioning to.
 */
simulated state Cover
{
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
		if( bFire != 0 )
		{
			if( out_PawnCA == CA_Default )
			{
				// if firing with no lean, set proper blind firing direction
				// (default if 'up')
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
		else 
		{
			if( out_PawnCA == CA_BlindRight || out_PawnCA == CA_BlindLeft )
			{
				// if was blind firing, but not firing anymore, reset!
				out_PawnCA = CA_Default;
			}
		}
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

		// split up vert/horiz joy axis, and see which one is dominant
		if( Abs(WarPlayerInput(PlayerInput).RawJoyUp) > Abs(WarPlayerInput(PlayerInput).RawJoyRight) )
		{
			if( WarPlayerInput(PlayerInput).RawJoyUp > 0.0f )	// up
			{	
				if( NodeCT == CT_MidLevel )	// stand up
				{
					out_PawnCD = CD_Up;
					out_PawnCA = CA_PopUp;
				}
				else if( NodeCT == CT_Standing )
				{
					out_PawnCT = CT_Standing;
				}
			}
			else if( WarPlayerInput(PlayerInput).RawJoyUp < 0.0f ) // down
			{
				if( out_PawnCT == CT_Standing )	// crouch
				{
					out_PawnCT = CT_MidLevel;
					if( (out_PawnCD == CD_Right && PrimaryCover.RightNode != None) || 
						(out_PawnCD == CD_Left && PrimaryCover.LeftNode != None) )
					{
						out_PawnCD	= CD_Default;
						out_PawnCA	= CA_Default;
					}
				}

				// if was looking up, get cam down
				if( out_PawnCD == CD_Up )
				{
					out_PawnCD = CD_Default;
				}
			}
		}
		else
		{
			if( WarPlayerInput(PlayerInput).RawJoyRight > 0.0f ) // right
			{
				if( CanTransitionTo( true, out_PawnCT) )
				{
					if( out_PawnCD == CD_Left )
					{
						CoverTransitionCountDown = CoverTransitionTime;			
						out_PawnCD = CD_Default;
					}

					if( CoverTransitionCountDown == 0.f && 
						(out_PawnCD == CD_Default || out_PawnCD == CD_Up) )
					{
						// Transition to moving
						bAllowMovement = true;
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
					if( CoverTransitionCountDown == 0.f )
					{
						out_PawnCD	= CD_Right;
						out_PawnCA	= CA_LeanRight;
					}
				}
			}
			else if( WarPlayerInput(PlayerInput).RawJoyRight < 0.f ) // left
			{
				if( CanTransitionTo( false, out_PawnCT) )
				{
					if( out_PawnCD == CD_Right )
					{
						CoverTransitionCountDown = CoverTransitionTime;			
						out_PawnCD = CD_Default;
					}

					if( CoverTransitionCountDown == 0.f && 
						(out_PawnCD == CD_Default || out_PawnCD == CD_Up) )
					{
						// Transition to moving
						bAllowMovement = true;
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
					if( CoverTransitionCountDown == 0.f )
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

		NodeCT		= GetNodeCoverType();
		out_PawnCD	= CD_Default;

		// Update Pawn's posture
		if( NodeCT == CT_Crouching && out_PawnCT == CT_MidLevel )
		{
			// transition from midlevel to crouching
			out_PawnCT = CT_Crouching;
		}
		else  if( NodeCT != CT_Crouching && out_PawnCT == CT_Crouching )
		{
			// transition from crouching to midlevel
			out_PawnCT = CT_MidLevel;
		}

		if( WarPlayerInput(PlayerInput).RawJoyUp > Abs(WarPlayerInput(PlayerInput).RawJoyRight) )	// up
		{	
			if( NodeCT == CT_MidLevel )
			{
				out_PawnCD = CD_Up;
				out_PawnCA = CA_PopUp;
			}
			else if( NodeCT == CT_Standing )
			{
				out_PawnCT = CT_Standing;
			}
		}
		else
		{
			out_PawnCA = CA_Default;
		}

		if( WarPlayerInput(PlayerInput).RawJoyUp < -Abs(WarPlayerInput(PlayerInput).RawJoyRight) )	// down
		{
			// if standing, then crouch...
			if( out_PawnCT == CT_Standing )
			{
				out_PawnCT	= CT_MidLevel;
				out_PawnCA	= CA_Default;
			}
			// Make sure cam is down.
			out_PawnCD	= CD_Default;
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

			if( (PawnRelDirDot > 0 && !CanTransitionTo( true, GetPawnCoverType() )) ||
				(PawnRelDirDot < 0 && !CanTransitionTo( false, GetPawnCoverType() )) )
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
			if( out_PawnCD == CD_Right )
			{
				out_PawnCA = CA_LeanRight;
			}
			else if( out_PawnCD == CD_Left )
			{
				out_PawnCA = CA_LeanLeft;
			}
		}
		else 
		{
			out_PawnCA = CA_Default;
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
		local CoverNode.ECoverType		NodeCT;
		local bool						bCanMove;

		NodeCT = GetNodeCoverType();

		// split up vert/horiz joy axis, and see which one is dominant
		if( Abs(WarPlayerInput(PlayerInput).RawJoyUp) > Abs(WarPlayerInput(PlayerInput).RawJoyRight) )
		{
			if( WarPlayerInput(PlayerInput).RawJoyUp > 0.f )	// up
			{	
				if( NodeCT == CT_MidLevel )
				{
					out_PawnCD = CD_Up;
					out_PawnCA = CA_PopUp;
				}
				else if( NodeCT == CT_Standing )
				{
					out_PawnCT = CT_Standing;
				}
			}
			else
			{
				out_PawnCA = CA_Default;
			}

			if( WarPlayerInput(PlayerInput).RawJoyUp < 0.f )	// down
			{
				if( out_PawnCT == CT_Standing )
				{
					out_PawnCT	= CT_MidLevel;
					out_PawnCA	= CA_Default;
				}

				// if was looking up, get cam down
				if( out_PawnCD == CD_Up )
				{
					out_PawnCD = CD_Default;
				}
			}
		}
		else
		{
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
	CoverTransitionTime=0.4
}