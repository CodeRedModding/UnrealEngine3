class AnimTree_COGGear extends AnimTree;

defaultproperties
{
	//============================================================
	// Normal Branch (walking/crouching/etc)
		// IMPORTANT: Follow the tree from the bottom
	
		Begin Object Class=AnimNodeSequence Name=anRunFwd
			AnimSeqName=Run_Fwd_Rdy_01
			bLooping=true
			bPlaying=true
		End Object
	
		Begin Object Class=AnimNodeSequence Name=anRunBwd
			AnimSeqName=Run_Bwd_Rdy_01
			bLooping=true
			bPlaying=true
		End Object
	
		Begin Object Class=AnimNodeSequence Name=anRunLft
			AnimSeqName=Run_Lft_Rdy_01
			bLooping=true
			bPlaying=true
		End Object
	
		Begin Object Class=AnimNodeSequence Name=anRunRt
			AnimSeqName=Run_Rt_Rdy_01
			bLooping=true
			bPlaying=true
		End Object
	
		Begin Object Class=AnimNodeBlendDirectional Name=anRunDir
			Children(0)=(Anim=anRunFwd)
			Children(1)=(Anim=anRunBwd)
			Children(2)=(Anim=anRunLft)
			Children(3)=(Anim=anRunRt)
		End Object
	
		// Running
	
	
		Begin Object Class=AnimNodeCrossfader Name=anStandIdle
			NodeName=anStandIdle
			DefaultAnimSeqName=Idle_Rdy_01
		End Object
	
		// Standing Idle
	
	
		Begin Object Class=AnimNodeBlendBySpeed Name=anMoveType
			Children(0)=(Anim=anStandIdle)
			Children(1)=(Anim=anRunDir)
			BlendUpTime=0.15
			BlendDownTime=0.25
			BlendDownPerc=0.1
			Constraints=(0,350)
		End Object
	
		// Speed Blend for Standing
	
		// Standing ^^^^^^^^^^^^^^^^^
	
		Begin Object Class=AnimNodeSequence Name=anCrouchWalkFwd
			AnimSeqName=COG_Crouch_Forward_00
			bLooping=true
			bPlaying=true
		End Object
	
		Begin Object Class=AnimNodeSequence Name=anCrouchWalkBwd
			AnimSeqName=CrouchBackward_00
			bLooping=true
			bPlaying=true
		End Object
	
		Begin Object Class=AnimNodeSequence Name=anCrouchWalkLft
			AnimSeqName=CrouchLeft_00
			bLooping=true
			bPlaying=true
		End Object
	
		Begin Object Class=AnimNodeSequence Name=anCrouchWalkRt
			AnimSeqName=CrouchRight_00
			bLooping=true
			bPlaying=true
		End Object
	
		Begin Object Class=AnimNodeBlendDirectional Name=anCrouchWalkDir
			Children(0)=(Anim=anCrouchWalkFwd)
			Children(1)=(Anim=anCrouchWalkBwd)
			Children(2)=(Anim=anCrouchWalkLft)
			Children(3)=(Anim=anCrouchWalkRt)
		End Object
	
		Begin Object Class=AnimNodeSequence Name=anCrouchIdle
			AnimSeqName=CrouchIdle_00
			bLooping=true
			bPlaying=true
		End Object
	
		Begin Object Class=AnimNodeBlendBySpeed Name=anCrouchMove
			Children(0)=(Anim=anCrouchIdle)
			Children(1)=(Anim=anCrouchWalkDir)
			BlendUpTime=0.15
			BlendDownTime=0.25
			BlendDownPerc=0.1
			Constraints=(0,180)
		End Object
		// Crouching ^^^^^^^^^^^^^^^
		
		Begin Object Class=AnimNodeBlendByPosture Name=anPosture
			Children(0)=(Anim=anMoveType)
			Children(1)=(Anim=anCrouchMove)
		End Object
	
		// Blend for the Physics level

		Begin Object Class=AnimNodeCrossfader Name=anFire
			NodeName=anFire
			DefaultAnimSeqName=Fire_Auto_01
		End object
	
		Begin Object Class=WarAnim_CoverFireBlendNode Name=anFireBlend
			Children(0)=(Anim=anPosture)
			Children(1)=(Anim=anFire)
			InitChild2StartBone=spine
			InitPerBoneIncrease=0.2
			Child2Weight=(Weight=0.0)
		End Object
		// Blend in Firing channels
	// End Normal Branch
	//============================================================

	//============================================================
	// Cover Branch

			//====================================================
			// BTW Standing

			// BTW Standing, idle
			Begin Object Class=AnimNodeSequence Name=anBTW_Idle
				AnimSeqName=BTWId01
				bLooping=true
				bPlaying=true
			End Object
		
			// BTW Standing, walking left
			Begin Object Class=AnimNodeSequence Name=anBTW_MoveLeft
				AnimSeqName=BTWStLt01
				bLooping=true
				bPlaying=true
			End Object
			
			// BTW Standing, walking right
			Begin Object Class=AnimNodeSequence Name=anBTW_MoveRight
				AnimSeqName=BTWStRt01
				bLooping=true
				bPlaying=true
			End Object
			
			// BTW Standing, step in/out left
			Begin Object Class=WarAnim_CoverSequenceNode Name=anBTW_StepLeft
				IntroAnimSeqName=BTWId01ToTuLt01
				IdleAnimSeqName=BTWTuLt01
				OutroAnimSeqName=BTWTuLt01ToId01
				AnimSeqName=BTWTuLt01
				bLooping=true
				bPlaying=true
				bCauseActorAnimPlay=true
			End Object
			
			// BTW Standing, step in/out right
			Begin Object Class=WarAnim_CoverSequenceNode Name=anBTW_StepRight
				IntroAnimSeqName=BTWId01ToTuRt01
				IdleAnimSeqName=BTWTuRt01
				OutroAnimSeqName=BTWTuRt01ToId01
				AnimSeqName=BTWTuRt01
				bLooping=true
				bPlaying=true
				bCauseActorAnimPlay=true
			End Object
			
				// BTW Standing, leaning left idle
				Begin Object Class=AnimNodeSequence Name=anBTW_LeanLeft
					AnimSeqName=BTWLnLtOt01
					bLooping=true
					bPlaying=true
				End Object
	
				// BTW Standing, leaning left fire
				Begin Object Class=AnimNodeSequence Name=anBTW_FireLeft
					AnimSeqName=BTWLnLtOtFr01
					bLooping=true
					bPlaying=true
				End Object

			// BTW Standing, leaning left idle/fire
			Begin Object Class=WarAnim_CoverFireBlendNode Name=anBTW_LeftBlend
				Children(0)=(Anim=anBTW_LeanLeft)
				Children(1)=(Anim=anBTW_FireLeft)
			End Object
			
				// BTW Standing, leaning right idle
				Begin Object Class=AnimNodeSequence Name=anBTW_LeanRight
					AnimSeqName=BTWLnRtOt01
					bLooping=true
					bPlaying=true
				End Object
		
				// BTW Standing, leaning right fire
				Begin Object Class=AnimNodeSequence Name=anBTW_FireRight
					AnimSeqName=BTWLnRtOtFr01
					bLooping=true
					bPlaying=true
				End Object
			
			// BTW Standing, leaning right idle/fire
			Begin Object Class=WarAnim_CoverFireBlendNode Name=anBTW_RightBlend
				Children(0)=(Anim=anBTW_LeanRight)
				Children(1)=(Anim=anBTW_FireRight)
			End Object

			// BTW Standing, blind fire left
			Begin Object Class=AnimNodeSequence Name=anBTW_BlindFireLeft
				AnimSeqName=BTWBlLtFr01
				bLooping=true
				bPlaying=true
			End Object

			// BTW Standing, blind fire left
			Begin Object Class=AnimNodeSequence Name=anBTW_BlindFireRight
				AnimSeqName=BTWBlRtFr01
				bLooping=true
				bPlaying=true
			End Object

			Begin Object Class=AnimNodeSequence Name=anBTW_Reload
				AnimSeqName=BTWIdRd01
			End Object

			// BTW Standing, cover blend
			Begin Object Class=WarAnim_CoverMoveBlendNode Name=anBTW_Cover
				Children(0)=(Anim=anBTW_Idle)
				Children(1)=(Anim=anBTW_MoveLeft)
				Children(2)=(Anim=anBTW_MoveRight)
				Children(3)=(Anim=anBTW_LeftBlend)
				Children(4)=(Anim=anBTW_RightBlend)
				Children(5)=(Anim=anBTW_StepLeft)
				Children(6)=(Anim=anBTW_StepRight)
				Children(7)=(Anim=anBTW_BlindFireLeft)
				Children(8)=(Anim=anBTW_BlindFireRight)
				Children(9)=()
				Children(10)=(Anim=anBTW_Reload)
			End Object

			// BTW Standing
			//====================================================

			//====================================================
			// BTW Midlevel

				Begin Object Class=AnimNodeSequence Name=anBTWCr_Idle
					AnimSeqName=BTWCrId01
					bLooping=true
					bPlaying=true
				End Object
	
				Begin Object Class=AnimNodeSequence Name=anBTWCr_BlindFire
					AnimSeqName=BTWCrBlFr01
					bLooping=true
					bPlaying=true
				End Object

			Begin Object Class=WarAnim_CoverFireBlendNode Name=anBTWCr_IdleBlend
				Children(0)=(Anim=anBTWCr_Idle)
				Children(1)=(Anim=anBTWCr_BlindFire)
			End Object
	
			Begin Object Class=AnimNodeSequence Name=anBTWCr_MoveLeft
				AnimSeqName=BTWCrStLt01
				bLooping=true
				bPlaying=true
			End Object
			
			Begin Object Class=AnimNodeSequence Name=anBTWCr_MoveRight
				AnimSeqName=BTWCrStRt01
				bLooping=true
				bPlaying=true
			End Object
			
			Begin Object Class=WarAnim_CoverSequenceNode Name=anBTWCr_StepLeft
				IntroAnimSeqName=BTWCrId01ToCrTuLt01
				IdleAnimSeqName=BTWCrTuLt01
				OutroAnimSeqName=BTWCrTuLt01ToCrId01
				AnimSeqName=BTWTuLt01
				bLooping=true
				bPlaying=true
				bCauseActorAnimPlay=true
			End Object
			
			Begin Object Class=WarAnim_CoverSequenceNode Name=anBTWCr_StepRight
				IntroAnimSeqName=BTWCrId01ToCrTuRt01
				IdleAnimSeqName=BTWCrTuRt01
				OutroAnimSeqName=BTWCrTuRt01ToCrId01
				AnimSeqName=BTWTuRt01
				bLooping=true
				bPlaying=true
				bCauseActorAnimPlay=true
			End Object
		
				Begin Object Class=AnimNodeSequence Name=anBTWCr_LeanLeft
					AnimSeqName=BTWCrLnLtOt01
					bLooping=true
					bPlaying=true
				End Object
				
				Begin Object Class=AnimNodeSequence Name=anBTWCr_FireLeft
					AnimSeqName=BTWCrLnLtOtFr01
					bLooping=true
					bPlaying=true
				End Object
				
			Begin Object Class=WarAnim_CoverFireBlendNode Name=anBTWCr_LeftBlend
				Children(0)=(Anim=anBTWCr_LeanLeft)
				Children(1)=(Anim=anBTWCr_FireLeft)
			End Object
			
				Begin Object Class=AnimNodeSequence Name=anBTWCr_LeanRight
					AnimSeqName=BTWCrLnRtOt01
					bLooping=true
					bPlaying=true
				End Object

				Begin Object Class=AnimNodeSequence Name=anBTWCr_FireRight
					AnimSeqName=BTWCrLnRtOtFr01
					bLooping=true
					bPlaying=true
				End Object
				
			Begin Object Class=WarAnim_CoverFireBlendNode Name=anBTWCr_RightBlend
				Children(0)=(Anim=anBTWCr_LeanRight)
				Children(1)=(Anim=anBTWCr_FireRight)
			End Object

			// BTW Crouch, blind fire left
			Begin Object Class=AnimNodeSequence Name=anBTWCr_BlindFireLeft
				AnimSeqName=BTWCrBlLtFr01
				bLooping=true
				bPlaying=true
			End Object

			// BTW Crouch, blind fire right
			Begin Object Class=AnimNodeSequence Name=anBTWCr_BlindFireRight
				AnimSeqName=BTWCrBlRtFr01
				bLooping=true
				bPlaying=true
			End Object
		
			Begin Object Class=AnimNodeSequence Name=anBTWCr_StandUpIdle
				AnimSeqName=Idle_Rdy_01
				bLooping=true
				bPlaying=true
			End Object

			Begin Object Class=AnimNodeSequence Name=anBTWCr_StandUpFire
				AnimSeqName=Fire_Auto_01
				bLooping=true
				bPlaying=true
			End Object

			Begin Object Class=AnimNodeSequence Name=anBTWCr_Reload
				AnimSeqName=BTWCrIdRd01
			End Object

			Begin Object Class=WarAnim_CoverFireBlendNode Name=anBTWCr_StandUpBlend
				Children(0)=(Anim=anBTWCr_StandUpIdle)
				Children(1)=(Anim=anBTWCr_StandUpFire)
			End Object

			Begin Object Class=WarAnim_CoverMoveBlendNode Name=anBTWCr_Cover
				Children(0)=(Anim=anBTWCr_IdleBlend)
				Children(1)=(Anim=anBTWCr_MoveLeft)
				Children(2)=(Anim=anBTWCr_MoveRight)
				Children(3)=(Anim=anBTWCr_LeftBlend)
				Children(4)=(Anim=anBTWCr_RightBlend)
				Children(5)=(Anim=anBTWCr_StepLeft)
				Children(6)=(Anim=anBTWCr_StepRight)
				Children(7)=(Anim=anBTWCr_BlindFireLeft)
				Children(8)=(Anim=anBTWCr_BlindFireRight)
				Children(9)=(Anim=anBTWCr_StandUpBlend)
				Children(10)=(Anim=anBTWCr_Reload)
			End Object
			
			// BTW Midlevel
			//====================================================
		
			//====================================================
			// Crouching

			Begin Object Class=AnimNodeSequence Name=anCr_Idle
				AnimSeqName=CrCvId01
				bLooping=true
				bPlaying=true
			End Object
	
			Begin Object Class=AnimNodeSequence Name=anCr_MoveLeft
				AnimSeqName=CrCvWalkRt01
				bLooping=true
				bPlaying=true
			End Object
			
			Begin Object Class=AnimNodeSequence Name=anCr_MoveRight
				AnimSeqName=CrCvWalkLt01
				bLooping=true
				bPlaying=true
			End Object

			Begin Object Class=AnimNodeSequence Name=anCr_Reload
				AnimSeqName=CrCvIdRd01
			End Object

			Begin Object Class=WarAnim_CoverMoveBlendNode Name=anCr_Cover
				Children(0)=(Anim=anCr_Idle)
				Children(1)=(Anim=anCr_MoveLeft)
				Children(2)=(Anim=anCr_MoveRight)
				Children(10)=(Anim=anCr_Reload)
			End Object

			// Crouching
			//====================================================

		Begin Object Class=WarAnim_CoverBlendNode Name=anCoverBlend
			Children(0)=(Anim=anBTW_Cover)
			Children(1)=(Anim=anBTWCr_Cover)
			Children(2)=(Anim=anCr_Cover)
		End Object

	// End Cover Branch
	//============================================================

	//============================================================
	// Evade Branch
			
			Begin Object Class=AnimNodeSequence Name=anEvadeForward
				bCauseActorAnimEnd=true
				AnimSeqName=DvFd01
				Rate=1.5
				bZeroRootTranslationX=true
				bZeroRootTranslationY=true
				NodeName=EvadeForward
			End Object

			Begin Object Class=AnimNodeSequence Name=anEvadeBackward
				bCauseActorAnimEnd=true
				AnimSeqName=DvBd01
				Rate=1.5
				bZeroRootTranslationX=true
				bZeroRootTranslationY=true
				NodeName=EvadeBackward
			End Object

			Begin Object Class=AnimNodeSequence Name=anEvadeLeft
				bCauseActorAnimEnd=true
				AnimSeqName=DvRt01
				Rate=1.5
				bZeroRootTranslationX=true
				bZeroRootTranslationY=true
				NodeName=EvadeLeft
			End Object

			Begin Object Class=AnimNodeSequence Name=anEvadeRight
				bCauseActorAnimEnd=true
				AnimSeqName=DvLt01
				Rate=1.5
				bZeroRootTranslationX=true
				bZeroRootTranslationY=true
				NodeName=EvadeRight
			End Object
		
		Begin Object Class=WarAnim_EvadeBlendNode Name=anEvadeBlend
			Children(0)=(Anim=anEvadeForward)
			Children(1)=(Anim=anEvadeBackward)
			Children(2)=(Anim=anEvadeLeft)
			Children(3)=(Anim=anEvadeRight)
		End Object
	// End Evade Branch
	//============================================================
	
			// Death anims
		Begin Object Class=AnimNodeSequence Name=aDeathAnimNode
			AnimSeqName=DtBd01
			Rate=0.5
			NodeName=DeathNode
		End Object

	//============================================================
	// Root of the tree
	Begin Object Class=WarAnim_BaseBlendNode Name=anBaseBlend
		Children(0)=(Anim=anFireBlend)
		Children(1)=(Anim=anCoverBlend)
		Children(2)=(Anim=anEvadeBlend)
		Children(3)=(Anim=aDeathAnimNode)
	End Object

	// Top of the node
	Children(0)=(Anim=anBaseBlend)
	// End Root
	//============================================================
}