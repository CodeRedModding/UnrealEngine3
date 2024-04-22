/*=============================================================================
	AnimationCompressionAlgorithm_RemoveLinearKeys.cpp: Keyframe reduction algorithm that simply removes every second key.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "AnimationUtils.h"
#include "AnimationEncodingFormat.h"
#include "AnimationCompression.h"

IMPLEMENT_CLASS(UAnimationCompressionAlgorithm_RemoveLinearKeys);

// Define to 1 to enable timing of the meat of linear key removal done in DoReduction
// The times are non-trivial, but the extra log spam isn't useful if one isn't optimizing DoReduction runtime
#define TIME_LINEAR_KEY_REMOVAL 0

/**
 * Helper function to enforce that the delta between two Quaternions represents
 * the shortest possible rotation angle
 */
static FQuat EnforceShortestArc(const FQuat& A, const FQuat& B)
{
	const FLOAT DotResult = (A | B);
	const FLOAT Bias = appFloatSelect(DotResult, 1.0f, -1.0f);
	return B*Bias;
}

/**
 * Helper template function to interpolate between two data types.
 * Used in the FilterLinearKeysTemplate function below
 */
template <typename T>
T Interpolate(const T& A, const T& B, FLOAT Alpha)
{
	// only the custom instantiations below are valid
	check(0);
	return 0;
}

/** custom instantiation of Interpolate for FVectors */
template <> FVector Interpolate<FVector>(const FVector& A, const FVector& B, FLOAT Alpha)
{
	return Lerp(A,B,Alpha);
}

/** custom instantiation of Interpolate for FQuats */
template <> FQuat Interpolate<FQuat>(const FQuat& A, const FQuat& B, FLOAT Alpha)
{
	FQuat result = LerpQuat(A,B,Alpha);
	result.Normalize();

	return result;
}

/**
 * Helper template function to calculate the delta between two data types.
 * Used in the FilterLinearKeysTemplate function below
 */
template <typename T>
FLOAT CalcDelta(const T& A, const T& B)
{
	// only the custom instantiations below are valid
	check(0);
	return 0;
}

/** custom instantiation of CalcDelta for FVectors */
template <> FLOAT CalcDelta<FVector>(const FVector& A, const FVector& B)
{
	return (A - B).Size();
}

/** custom instantiation of CalcDelta for FQuat */
template <> FLOAT CalcDelta<FQuat>(const FQuat& A, const FQuat& B)
{
	return FQuatError(A, B);
}

/**
 * Helper function to replace a specific component in the FBoneAtom structure
 */
template <typename T>
FBoneAtom UpdateBoneAtom(INT BoneIndex, const FBoneAtom& Atom, const T& Component)
{
	// only the custom instantiations below are valid
	check(0);
	return FBoneAtom();
}

/** custom instantiation of UpdateBoneAtom for FVectors */
template <> FBoneAtom UpdateBoneAtom<FVector>(INT BoneIndex, const FBoneAtom& Atom, const FVector& Component)
{
	return FBoneAtom(Atom.GetRotation(), Component, 1.0f);
}

/** custom instantiation of UpdateBoneAtom for FQuat */
template <> FBoneAtom UpdateBoneAtom<FQuat>(INT BoneIndex, const FBoneAtom& Atom, const FQuat& Component)
{
	FQuat IncommingQuat = Component;
	if (BoneIndex > 0)
	{
		IncommingQuat.W *= -1.0f;
	}
	return FBoneAtom(IncommingQuat, Atom.GetTranslation(), 1.0f);
}

/**
 * Template function to reduce the keys of a given data type.
 * Used to reduce both Translation and Rotation keys using the corresponding
 * data types FVector and FQuat
 */
template <typename T>
void FilterLinearKeysTemplate(
	TArray<T>& Keys,
    TArray<FLOAT>& Times,
	TArray<FBoneAtom>& BoneAtoms,
    const TArray<FLOAT>* ParentTimes,
	const TArray<FMatrix>& RawWorldBones,
	const TArray<FMatrix>& NewWorldBones,
    const TArray<INT>& TargetBoneIndices,
	INT NumFrames,
	INT BoneIndex,
	INT ParentBoneIndex,
	FLOAT ParentScale,
	FLOAT MaxDelta,
	FLOAT MaxTargetDelta,
	FLOAT EffectorDiffSocket,
	const TArray<FBoneData>& BoneData
	)
{
	const INT KeyCount = Keys.Num();
	check( Keys.Num() == Times.Num() );
	check( KeyCount >= 1 );
	
	// generate new arrays we will fill with the final keys
	TArray<T> NewKeys;
	TArray<FLOAT> NewTimes;
	NewKeys.Empty(KeyCount);
	NewTimes.Empty(KeyCount);

	// Only bother doing anything if we have some keys!
	if(KeyCount > 0)
	{
		INT LowKey = 0;
		INT HighKey = KeyCount-1;
		INT PrevKey = 0;
		
		// copy the low key (this one is a given)
		NewTimes.AddItem(Times(0));
		NewKeys.AddItem(Keys(0));

		FBoneAtom DummyBone(FQuat::Identity, FVector(END_EFFECTOR_SOCKET_DUMMY_BONE_SIZE, END_EFFECTOR_SOCKET_DUMMY_BONE_SIZE, END_EFFECTOR_SOCKET_DUMMY_BONE_SIZE));
		FMatrix DummyMatrix = DummyBone.ToMatrix();

		FLOAT const DeltaThreshold = (BoneData(BoneIndex).IsEndEffector() && (BoneData(BoneIndex).bHasSocket || BoneData(BoneIndex).bKeyEndEffector)) ? EffectorDiffSocket : MaxTargetDelta;

		// We will test within a sliding window between LowKey and HighKey.
		// Therefore, we are done when the LowKey exceeds the range
		while (LowKey < KeyCount-1)
		{
			// high key always starts at the top of the range
			HighKey = KeyCount-1;

			// keep testing until the window is closed
			while (HighKey > LowKey+1)
			{
				// get the parameters of the window we are testing
				const FLOAT LowTime = Times(LowKey);
				const FLOAT HighTime = Times(HighKey);
				const T LowValue = Keys(LowKey);
				const T HighValue = Keys(HighKey);
				const FLOAT Range = HighTime - LowTime;
				const FLOAT InvRange = 1.0f/Range;

				// iterate through all interpolated members of the window to
				// compute the error when compared to the original raw values
				FLOAT MaxLerpError = 0.0f;
				FLOAT MaxTargetError = 0.0f;
				for (INT TestKey = LowKey+1; TestKey< HighKey; ++TestKey)
				{
					// get the parameters of the member being tested
					FLOAT TestTime = Times(TestKey);
					T TestValue = Keys(TestKey);

					// compute the proposed, interpolated value for the key
					const FLOAT Alpha = (TestTime - LowTime) * InvRange;
					const T LerpValue = Interpolate(LowValue, HighValue, Alpha);

					// compute the error between our interpolated value and the desired value
					FLOAT LerpError = CalcDelta(TestValue, LerpValue);

					// if the local-space lerp error is within our tolerances, we will also check the
					// effect this interpolated key will have on our target end effectors
					FLOAT TargetError = -1.0f;
					if (LerpError <= MaxDelta)
					{
						// get the raw world transform for this bone (the original world-space position)
						const INT FrameIndex = TestKey;
						const FMatrix& RawBase = RawWorldBones((BoneIndex*NumFrames) + FrameIndex);
						const FMatrix InvRawBase = RawBase.Inverse();
						
						// generate the proposed local bone atom and transform (local space)
						FBoneAtom ProposedAtom = UpdateBoneAtom(BoneIndex, BoneAtoms(FrameIndex), LerpValue);
						FMatrix ProposedTM = ProposedAtom.ToMatrix();

						// convert the proposed local transform to world space using this bone's parent transform
						const FMatrix& CurrentParent = ParentBoneIndex != INDEX_NONE ? NewWorldBones((ParentBoneIndex*NumFrames) + FrameIndex) : FMatrix::Identity;
						FMatrix ProposedBase = ProposedTM * CurrentParent;
						
						// for each target end effector, compute the error we would introduce with our proposed key
						for (INT TargetIndex=0; TargetIndex<TargetBoneIndices.Num(); ++TargetIndex)
						{
							// find the offset transform from the raw base to the end effector
							const INT TargetBoneIndex = TargetBoneIndices(TargetIndex);
							FMatrix RawTarget = RawWorldBones((TargetBoneIndex*NumFrames) + FrameIndex);
							const FMatrix& RelTM = RawTarget * InvRawBase; 

							// forecast where the new end effector would be using our proposed key
							FMatrix ProposedTarget = RelTM * ProposedBase;

							// If this is an EndEffector with a Socket attached to it, add an extra bone, to measure error introduced by effector rotation compression.
							if( BoneData(TargetIndex).bHasSocket || BoneData(TargetIndex).bKeyEndEffector )
							{
								ProposedTarget = DummyMatrix * ProposedTarget;
								RawTarget = DummyMatrix * RawTarget;
							}

							// determine the extend of error at the target end effector
							FLOAT ThisError = (ProposedTarget.GetOrigin() - RawTarget.GetOrigin()).Size();
							TargetError = Max(TargetError, ThisError); 

							// exit early when we encounter a large delta
							FLOAT const TargetDeltaThreshold = BoneData(TargetIndex).bHasSocket ? EffectorDiffSocket : DeltaThreshold;
							if( TargetError > TargetDeltaThreshold )
							{ 
								break;
							}
						}
					}

					// If the parent has a key at this time, we'll scale our error values as requested.
					// This increases the odds that we will choose keys on the same frames as our parent bone,
					// making the skeleton more uniform in key distribution.
					if (ParentTimes)
					{
						if (ParentTimes->FindItemIndex(TestTime) != INDEX_NONE)
						{
							// our parent has a key at this time, 
							// inflate our perceived error to increase our sensitivity
							// for also retaining a key at this time
							LerpError *= ParentScale;
							TargetError *= ParentScale;
						}
					}
					
					// keep track of the worst errors encountered for both 
					// the local-space 'lerp' error and the end effector drift we will cause
					MaxLerpError = Max(MaxLerpError, LerpError);
					MaxTargetError = Max(MaxTargetError, TargetError);

					// exit early if we have failed in this span
					if (MaxLerpError > MaxDelta ||
						MaxTargetError > DeltaThreshold)
					{
						break;
					}
				}

				// determine if the span succeeded. That is, the worst errors found are within tolerances
				if (MaxLerpError <= MaxDelta &&
					MaxTargetError <= DeltaThreshold)
				{
					// save the high end of the test span as our next key
					NewTimes.AddItem(Times(HighKey));
					NewKeys.AddItem(Keys(HighKey));

					// start testing a new span
					LowKey = HighKey;
					HighKey =  KeyCount-1;
				}
				else
				{
					// we failed, shrink the test span window and repeat
					--HighKey;
				}
			}

			// if the test window is still valid, accept the high key
			if (HighKey > LowKey)
			{
				NewTimes.AddItem(Times(HighKey));
				NewKeys.AddItem(Keys(HighKey));
			}
			LowKey= HighKey;
		}

		// The process has ended, but we must make sure the last key is accounted for
		if (NewTimes.Last() != Times.Last() &&
			CalcDelta(Keys.Last(), NewKeys.Last()) >= MaxDelta )
		{
			NewTimes.AddItem(Times.Last());
			NewKeys.AddItem(Keys.Last());
		}

		// return the new key set to the caller
		Times= NewTimes;
		Keys= NewKeys;
	}
}

/**
 * To guide the key removal process, we need to maintain a table of world transforms
 * for the bones we are investigating. This helper function fills a row of the 
 * table for a specified bone.
 */
void UAnimationCompressionAlgorithm_RemoveLinearKeys::UpdateWorldBoneTransformTable(
	UAnimSequence* AnimSeq, 
	USkeletalMesh* SkelMesh, 
	const TArray<FBoneData>& BoneData, 
	const struct FAnimSetMeshLinkup& AnimLinkup,
	const TArray<FMeshBone>& RefSkel,
	INT BoneIndex,
	UBOOL UseRaw,
	TArray<FMatrix>& OutputWorldBones)
{
	const FBoneData& Bone		= BoneData(BoneIndex);
	const INT NumFrames			= AnimSeq->NumFrames;
	const FLOAT SequenceLength	= AnimSeq->SequenceLength;
	const INT FrameStart		= (BoneIndex*NumFrames);
	const INT TrackIndex		= AnimLinkup.BoneToTrackTable(BoneIndex);
	
	check(OutputWorldBones.Num() >= (FrameStart+NumFrames));

	const FLOAT TimePerFrame = SequenceLength / (FLOAT)(NumFrames-1);

	UAnimSet* AnimSet = AnimSeq->GetAnimSet();

	if( TrackIndex != INDEX_NONE )
	{
		// get the local-space bone transforms using the animation solver
		for ( INT FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex )
		{
			FLOAT Time = (FLOAT)FrameIndex * TimePerFrame;
			FBoneAtom LocalAtom;

			AnimSeq->GetBoneAtom(LocalAtom, TrackIndex, Time, FALSE, UseRaw);

			FQuat Rot = LocalAtom.GetRotation();
			if (BoneIndex > 0)
			{
				Rot.W *= -1.0f;
			}

			LocalAtom.SetRotation(EnforceShortestArc(FQuat::Identity, Rot));
			// Saw some crashes happening with it, so normalize here. 
			LocalAtom.NormalizeRotation();

			OutputWorldBones((BoneIndex*NumFrames) + FrameIndex) = LocalAtom.ToMatrix();
		}
	}
	else
	{
		// get the default rotation and translation from the reference skeleton
		FMatrix DefaultTransform;
		FBoneAtom LocalAtom( RefSkel(BoneIndex).BonePos.Orientation, RefSkel(BoneIndex).BonePos.Position, 1.f );
		LocalAtom.SetRotation(EnforceShortestArc(FQuat::Identity, LocalAtom.GetRotation()));
		DefaultTransform = LocalAtom.ToMatrix();

		// copy the default transformation into the world bone table
		for ( INT FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex )
		{
			OutputWorldBones((BoneIndex*NumFrames) + FrameIndex) = DefaultTransform;
		}
	}

	// apply parent transforms to bake into world space. We assume the parent transforms were previously set using this function
	const INT ParentIndex = Bone.GetParent();
	if (ParentIndex != INDEX_NONE)
	{
		check (ParentIndex < BoneIndex);
		for ( INT FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex )
		{
			OutputWorldBones((BoneIndex*NumFrames) + FrameIndex) = OutputWorldBones((BoneIndex*NumFrames) + FrameIndex) * OutputWorldBones((ParentIndex*NumFrames) + FrameIndex);
		}
	}
}

/**
 * Pre-filters the tracks before running the main key removal algorithm
 */
void UAnimationCompressionAlgorithm_RemoveLinearKeys::FilterBeforeMainKeyRemoval(
	UAnimSequence* AnimSeq, 
	USkeletalMesh* SkelMesh, 
	const struct FAnimSetMeshLinkup& AnimLinkup,
	const TArray<FBoneData>& BoneData, 
	TArray<FTranslationTrack>& TranslationData,
	TArray<FRotationTrack>& RotationData)
{
	// remove obviously redundant keys from the source data
	FilterTrivialKeys(TranslationData, RotationData, TRANSLATION_ZEROING_THRESHOLD, QUATERNION_ZEROING_THRESHOLD);
}


/**
  * Updates the world bone transforms for a range of bone indices
  */
void UAnimationCompressionAlgorithm_RemoveLinearKeys::UpdateWorldBoneTransformRange(
	UAnimSequence* AnimSeq, 
	USkeletalMesh* SkelMesh, 
	const TArray<FBoneData>& BoneData, 
	const struct FAnimSetMeshLinkup& AnimLinkup,
	const TArray<FMeshBone>& RefSkel,
	const TArray<FTranslationTrack>& PositionTracks,
	const TArray<FRotationTrack>& RotationTracks,
	INT StartingBoneIndex,
	INT EndingBoneIndex,
	UBOOL UseRaw,
	TArray<FMatrix>& OutputWorldBones)
{
	// bitwise compress the tracks into the anim sequence buffers
	// to make sure the data we've compressed so far is ready for solving
	CompressUsingUnderlyingCompressor(
		AnimSeq,
		SkelMesh,
		AnimLinkup,
		BoneData, 
		PositionTracks,
		RotationTracks,
		FALSE);

	// build all world-space transforms from this bone to the target end effector we are monitoring
	// all parent transforms have been built already
	for ( INT Index = StartingBoneIndex; Index <= EndingBoneIndex; ++Index )
	{
		UpdateWorldBoneTransformTable(
			AnimSeq, 
			SkelMesh, 
			BoneData, 
			AnimLinkup,
			RefSkel,
			Index,
			UseRaw,
			OutputWorldBones);
	}
}

/**
 * Creates a list of the bone atom result for every frame of a given track
 */
void UAnimationCompressionAlgorithm_RemoveLinearKeys::UpdateBoneAtomList(
	UAnimSequence* AnimSeq, 
	INT BoneIndex,
	INT TrackIndex,
	INT NumFrames,
	FLOAT TimePerFrame,
	TArray<FBoneAtom>& BoneAtoms,
	const struct FAnimSetMeshLinkup& AnimLinkup,
	const TArray<FMeshBone>& RefSkel)
{
	UAnimSet* AnimSet = AnimSeq->GetAnimSet();

	BoneAtoms.Reset(NumFrames);
	for ( INT FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex )
	{
		FLOAT Time = (FLOAT)FrameIndex * TimePerFrame;
		FBoneAtom LocalAtom;
		AnimSeq->GetBoneAtom(LocalAtom, TrackIndex, Time, FALSE, FALSE);

		FQuat Rot = LocalAtom.GetRotation();
		if (BoneIndex > 0)
		{
			Rot.W *= -1.0f;
		}

		LocalAtom.SetRotation( EnforceShortestArc(FQuat::Identity, Rot) );
		BoneAtoms.AddItem(LocalAtom);
	}
}

/**
 * If the passed in animation sequence is additive, converts it to absolute (using the frame 0 pose) and returns TRUE
 * (indicating it should be converted back to relative later with ConvertToRelativeSpace)
 *
 * @param AnimSeq			The animation sequence being compressed
 * @param TranslationData	Translation Tracks to compress and bit-pack into the Animation Sequence.
 * @param RotationData		Rotation Tracks to compress and bit-pack into the Animation Sequence.
 *
 * @return TRUE if the animation was additive and has been converted to absolute space.
 */
UBOOL UAnimationCompressionAlgorithm_RemoveLinearKeys::ConvertFromRelativeSpace(UAnimSequence* AnimSeq, const struct FAnimSetMeshLinkup& AnimLinkup)
{
	// if this is an additive animation, temporarily convert it out of relative-space
	const UBOOL bAdditiveAnimation = AnimSeq->bIsAdditive;
	if (bAdditiveAnimation)
	{
		// let the sequence believe it is no longer additive
		AnimSeq->bIsAdditive = FALSE;

		// convert the raw tracks out of additive-space
		const INT NumTracks = AnimSeq->RawAnimationData.Num();
		for (INT TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
		{
			INT const BoneIndex = AnimLinkup.BoneToTrackTable.FindItemIndex(TrackIndex);
			UBOOL const bIsRootBone = (BoneIndex == 0);

			FRawAnimSequenceTrack& BasePoseTrack = AnimSeq->AdditiveBasePose(TrackIndex);
			FRawAnimSequenceTrack& RawTrack	= AnimSeq->RawAnimationData(TrackIndex);

			if( !bIsRootBone )
			{
				BasePoseTrack.RotKeys(0).W *= -1.f;
			}

			// @note: we only extract the first frame, as we don't want to induce motion from the base pose
			// only the motion from the additive data should matter.
			const FVector& RefBonePos = BasePoseTrack.PosKeys(0);
			const FQuat& RefBoneRotation = BasePoseTrack.RotKeys(0);

			// Transform position keys.
			for (INT PosIndex = 0; PosIndex < RawTrack.PosKeys.Num(); ++PosIndex)
			{
				RawTrack.PosKeys(PosIndex) += RefBonePos;
			}

			// Transform rotation keys.
			for (INT RotIndex = 0; RotIndex < RawTrack.RotKeys.Num(); ++RotIndex)
			{
				if( !bIsRootBone )
				{
					RawTrack.RotKeys(RotIndex).W *= -1.f;
				}

				RawTrack.RotKeys(RotIndex) = RawTrack.RotKeys(RotIndex) * RefBoneRotation;
				RawTrack.RotKeys(RotIndex).Normalize();

				if( !bIsRootBone )
				{
					RawTrack.RotKeys(RotIndex).W *= -1.f;
				}
			}

			if( !bIsRootBone )
			{
				BasePoseTrack.RotKeys(0).W *= -1.f;
			}
		}
	}

	return bAdditiveAnimation;
}

/**
 * Converts an absolute animation sequence to a relative (additive) one.
 *
 * @param AnimSeq			The animation sequence being compressed
 * @param TranslationData	Translation Tracks to convert to relative space
 * @param RotationData		Rotation Tracks  to convert to relative space
 *
 */
void UAnimationCompressionAlgorithm_RemoveLinearKeys::ConvertToRelativeSpace(
	UAnimSequence* AnimSeq,
	TArray<FTranslationTrack>& TranslationData,
	TArray<FRotationTrack>& RotationData,
	const struct FAnimSetMeshLinkup& AnimLinkup)
{
	// restore the additive flag in the sequence
	AnimSeq->bIsAdditive = TRUE;

	// convert the raw tracks back to additive-space
	const INT NumTracks = AnimSeq->RawAnimationData.Num();
	for ( INT TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		INT const BoneIndex = AnimLinkup.BoneToTrackTable.FindItemIndex(TrackIndex);
		UBOOL const bIsRootBone = (BoneIndex == 0);

		FRawAnimSequenceTrack& BasePoseTrack = AnimSeq->AdditiveBasePose(TrackIndex);
		FRawAnimSequenceTrack& RawTrack	= AnimSeq->RawAnimationData(TrackIndex);

		if( !bIsRootBone )
		{
			BasePoseTrack.RotKeys(0).W *= -1.f;
		}

		// @note: we only extract the first frame, as we don't want to induce motion from the base pose
		// only the motion from the additive data should matter.
		const FQuat& InvRefBoneRotation = -BasePoseTrack.RotKeys(0);
		const FVector& InvRefBoneTranslation = -BasePoseTrack.PosKeys(0);

		// transform position keys.
		for ( INT PosIndex = 0; PosIndex < RawTrack.PosKeys.Num(); ++PosIndex )
		{
			RawTrack.PosKeys(PosIndex) += InvRefBoneTranslation;
		}

		// transform rotation keys.
		for ( INT RotIndex = 0; RotIndex < RawTrack.RotKeys.Num(); ++RotIndex )
		{
			if( !bIsRootBone )
			{
				RawTrack.RotKeys(RotIndex).W *= -1.f;
			}

			RawTrack.RotKeys(RotIndex) = RawTrack.RotKeys(RotIndex) * InvRefBoneRotation;
			RawTrack.RotKeys(RotIndex).Normalize();

			if( !bIsRootBone )
			{
				RawTrack.RotKeys(RotIndex).W *= -1.f;
			}
		}

		// convert the new translation tracks to additive space
		FTranslationTrack& TranslationTrack = TranslationData(TrackIndex);
		for (INT KeyIndex = 0; KeyIndex < TranslationTrack.PosKeys.Num(); ++KeyIndex)
		{
			TranslationTrack.PosKeys(KeyIndex) += InvRefBoneTranslation;
		}

		// convert the new rotation tracks to additive space
		FRotationTrack& RotationTrack = RotationData(TrackIndex);
		for (INT KeyIndex = 0; KeyIndex < RotationTrack.RotKeys.Num(); ++KeyIndex)
		{
			if( !bIsRootBone )
			{
				RotationTrack.RotKeys(KeyIndex).W *= -1.f;
			}

			RotationTrack.RotKeys(KeyIndex) = RotationTrack.RotKeys(KeyIndex) * InvRefBoneRotation;
			RotationTrack.RotKeys(KeyIndex).Normalize();

			if( !bIsRootBone )
			{
				RotationTrack.RotKeys(KeyIndex).W *= -1.f;
			}
		}

		if( !bIsRootBone )
		{
			BasePoseTrack.RotKeys(0).W *= -1.f;
		}
	}
}

/**
 * Locates spans of keys within the position and rotation tracks provided which can be estimated
 * through linear interpolation of the surrounding keys. The remaining key values are bit packed into
 * the animation sequence provided
 *
 * @param	AnimSeq		The animation sequence being compressed
 * @param	SkelMesh	The skeletal mesh to use to guide the compressor
 * @param	BoneData	BoneData array describing the hierarchy of the animated skeleton
 * @param	TranslationCompressionFormat	The format to use when encoding translation keys.
 * @param	RotationCompressionFormat		The format to use when encoding rotation keys.
 * @param	TranslationData		Translation Tracks to compress and bit-pack into the Animation Sequence.
 * @param	RotationData		Rotation Tracks to compress and bit-pack into the Animation Sequence.
 * @return				None.
 */
void UAnimationCompressionAlgorithm_RemoveLinearKeys::ProcessAnimationTracks(
	UAnimSequence* AnimSeq, 
	USkeletalMesh* SkelMesh, 
	const struct FAnimSetMeshLinkup& AnimLinkup,
	const TArray<FBoneData>& BoneData, 
	TArray<FTranslationTrack>& PositionTracks,
	TArray<FRotationTrack>& RotationTracks)
{
	// extract all the data we'll need about the skeleton and animation sequence
	const INT NumBones			= BoneData.Num();
	const INT NumFrames			= AnimSeq->NumFrames;
	const FLOAT SequenceLength	= AnimSeq->SequenceLength;
	const INT LastFrame = NumFrames-1;
	const FLOAT FrameRate = (FLOAT)(LastFrame) / SequenceLength;
	const FLOAT TimePerFrame = SequenceLength / (FLOAT)(LastFrame);

	const TArray<FMeshBone>& RefSkel		= SkelMesh->RefSkeleton;
	check( AnimLinkup.BoneToTrackTable.Num() == RefSkel.Num() );

	// make sure the parent key scale is properly bound to 1.0 or more
	ParentKeyScale = Max(ParentKeyScale, 1.0f);

	// generate the raw and compressed skeleton in world-space
	TArray<FMatrix> RawWorldBones;
	TArray<FMatrix> NewWorldBones;
	RawWorldBones.Empty(NumBones * NumFrames);
	NewWorldBones.Empty(NumBones * NumFrames);
	RawWorldBones.AddZeroed(NumBones * NumFrames);
	NewWorldBones.AddZeroed(NumBones * NumFrames);

	// generate an array to hold the indices of our end effectors
	TArray<INT> EndEffectors;
	EndEffectors.Empty(NumBones);

	// Create an array of FBoneAtom to use as a workspace
	TArray<FBoneAtom> BoneAtoms;

	// setup the raw bone transformation and find all end effectors
	for ( INT BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex )
	{
		// get the raw world-atoms for this bone
		UpdateWorldBoneTransformTable(
			AnimSeq, 
			SkelMesh, 
			BoneData, 
			AnimLinkup,
			RefSkel,
			BoneIndex,
			TRUE,
			RawWorldBones);

		// also record all end-effectors we find
		const FBoneData& Bone = BoneData(BoneIndex);
		if (Bone.IsEndEffector())
		{
			EndEffectors.AddItem(BoneIndex);
		}
	}

	TArray<INT> TargetBoneIndices;
	// for each bone...
	for ( INT BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex )
	{
		const FBoneData& Bone = BoneData(BoneIndex);
		const INT ParentBoneIndex = Bone.GetParent();

		const INT TrackIndex = AnimLinkup.BoneToTrackTable(BoneIndex);
		if (TrackIndex != INDEX_NONE)
		{
			// get the tracks we will be editing for this bone
			FRotationTrack& RotTrack = RotationTracks(TrackIndex);
			FTranslationTrack& TransTrack = PositionTracks(TrackIndex);
			const INT NumRotKeys = RotTrack.RotKeys.Num();
			const INT NumPosKeys = TransTrack.PosKeys.Num();

			check( (NumPosKeys == 1) || (NumRotKeys == 1) || (NumPosKeys == NumRotKeys) );

			// build an array of end effectors we need to monitor
			TargetBoneIndices.Reset(NumBones);

			INT HighestTargetBoneIndex = BoneIndex;
			INT FurthestTargetBoneIndex = BoneIndex;
			INT ShortestChain = 0;
			FLOAT OffsetLength= -1.0f;
			for (INT EffectorIndex=0; EffectorIndex < EndEffectors.Num(); ++EffectorIndex)
			{
				const INT EffectorBoneIndex = EndEffectors(EffectorIndex);
				const FBoneData& EffectorBoneData = BoneData(EffectorBoneIndex);

				INT RootIndex = EffectorBoneData.BonesToRoot.FindItemIndex(BoneIndex);
				if (RootIndex != INDEX_NONE)
				{
					if (ShortestChain == 0 || (RootIndex+1) < ShortestChain)
					{
						ShortestChain = (RootIndex+1);
					}
					TargetBoneIndices.AddItem(EffectorBoneIndex);
					HighestTargetBoneIndex = Max(HighestTargetBoneIndex, EffectorBoneIndex);
					FLOAT ChainLength= 0.0f;
					for (long FamilyIndex=0; FamilyIndex < RootIndex; ++FamilyIndex)
					{
						const INT NextParentBoneIndex= EffectorBoneData.BonesToRoot(FamilyIndex);
						ChainLength += RefSkel(NextParentBoneIndex).BonePos.Position.Size();
					}

					if (ChainLength > OffsetLength)
					{
						FurthestTargetBoneIndex = EffectorBoneIndex;
						OffsetLength = ChainLength;
					}

				}
			}

			// if requested, retarget the FBoneAtoms towards the target end effectors
			if (bRetarget)
			{
				if (NumRotKeys > 0 && ParentBoneIndex != INDEX_NONE)
				{
					if (HighestTargetBoneIndex == BoneIndex)
					{
						for ( INT KeyIndex = 0; KeyIndex < NumRotKeys; ++KeyIndex )
						{
							FQuat& Key = RotTrack.RotKeys(KeyIndex);

							check(ParentBoneIndex != INDEX_NONE);
							const INT FrameIndex = Clamp(KeyIndex, 0, LastFrame);
							const FMatrix& NewWorldParent = NewWorldBones((ParentBoneIndex*NumFrames) + FrameIndex);
							const FMatrix& RawWorldChild = RawWorldBones((BoneIndex*NumFrames) + FrameIndex);
							const FMatrix& InvNewWorldParent = NewWorldParent.Inverse();
							const FMatrix& RelTM = (RawWorldChild * InvNewWorldParent).GetMatrixWithoutScale(); 
							FQuat Rot = FBoneAtom(RelTM).GetRotation();

							// rotations we apply need to be in the inverted key space
							if (BoneIndex > 0)
							{
								Rot.W *= -1.0f;
							}

							const FQuat& AlignedKey = EnforceShortestArc(Key, Rot);
							Key = AlignedKey;
						}
					}
					else
					{
						// update our bone table from the current bone through the last end effector we need to test
						UpdateWorldBoneTransformRange(
							AnimSeq, 
							SkelMesh, 
							BoneData, 
							AnimLinkup,
							RefSkel,
							PositionTracks,
							RotationTracks,
							BoneIndex,
							HighestTargetBoneIndex,
							FALSE,
							NewWorldBones);
						
						// adjust all rotation keys towards the end effector target
						for ( INT KeyIndex = 0; KeyIndex < NumRotKeys; ++KeyIndex )
						{
							FQuat& Key = RotTrack.RotKeys(KeyIndex);

							const INT FrameIndex = Clamp(KeyIndex, 0, LastFrame);

							const FMatrix& NewWorldTransform = NewWorldBones((BoneIndex*NumFrames) + FrameIndex);

							const FMatrix& InvSpace = NewWorldTransform.Inverse();

							const FMatrix& DesiredChildTransform = RawWorldBones((FurthestTargetBoneIndex*NumFrames) + FrameIndex) * InvSpace;
							const FMatrix& CurrentChildTransform = NewWorldBones((FurthestTargetBoneIndex*NumFrames) + FrameIndex) * InvSpace;

							// find the two vectors which represent the angular error we are trying to correct
							const FVector& CurrentHeading = CurrentChildTransform.GetOrigin();
							const FVector& DesiredHeading = DesiredChildTransform.GetOrigin();

							// if these are valid, we can continue
							if (!CurrentHeading.IsNearlyZero() && !DesiredHeading.IsNearlyZero())
							{
								const FLOAT DotResult = CurrentHeading.SafeNormal() | DesiredHeading.SafeNormal();

								// limit the range we will retarget to something reasonable (~60 degrees)
								if (DotResult < 1.0f && DotResult > 0.5f)
								{
									FQuat Adjustment= FQuatFindBetween(CurrentHeading, DesiredHeading); 
									Adjustment.Normalize();
									Adjustment= EnforceShortestArc(FQuat::Identity, Adjustment);

									const FVector Test = Adjustment.RotateVector(CurrentHeading);
									const FLOAT Delta = (Test - DesiredHeading).Size();
									if (Delta < 0.001f)
									{
										// rotations we apply need to be in the inverted key space
										if (BoneIndex > 0)
										{
											Adjustment.W *= -1.0f;
										}

										FQuat NewKey = Adjustment * Key;
										NewKey.Normalize();

										const FQuat& AlignedKey = EnforceShortestArc(Key, NewKey);
										Key = AlignedKey;
									}
								}
							}
						}
					}
				}

				if (NumPosKeys > 0 && ParentBoneIndex != INDEX_NONE)
				{
					// update our bone table from the current bone through the last end effector we need to test
					UpdateWorldBoneTransformRange(
						AnimSeq, 
						SkelMesh, 
						BoneData, 
						AnimLinkup,
						RefSkel,
						PositionTracks,
						RotationTracks,
						BoneIndex,
						HighestTargetBoneIndex,
						FALSE,
						NewWorldBones);

					// adjust all translation keys to align better with the destination
					for ( INT KeyIndex = 0; KeyIndex < NumPosKeys; ++KeyIndex )
					{
						FVector& Key= TransTrack.PosKeys(KeyIndex);

						const INT FrameIndex= Clamp(KeyIndex, 0, LastFrame);
						const FMatrix& NewWorldParent = NewWorldBones((ParentBoneIndex*NumFrames) + FrameIndex);
						const FMatrix& RawWorldChild = RawWorldBones((BoneIndex*NumFrames) + FrameIndex);
						const FMatrix& InvNewWorldParent = NewWorldParent.Inverse();
						const FMatrix& RelTM = (RawWorldChild * InvNewWorldParent).GetMatrixWithoutScale();
						const FBoneAtom Delta = FBoneAtom(RelTM);

						Key = Delta.GetTranslation();
					}
				}

			}

			// look for a parent track to reference as a guide
			INT GuideTrackIndex = INDEX_NONE;
			if (ParentKeyScale > 1.0f)
			{
				for (long FamilyIndex=0; (FamilyIndex < Bone.BonesToRoot.Num()) && (GuideTrackIndex == INDEX_NONE); ++FamilyIndex)
				{
					const INT NextParentBoneIndex= Bone.BonesToRoot(FamilyIndex);
					GuideTrackIndex = AnimLinkup.BoneToTrackTable(NextParentBoneIndex);
				}
			}

			// update our bone table from the current bone through the last end effector we need to test
			UpdateWorldBoneTransformRange(
				AnimSeq, 
				SkelMesh, 
				BoneData, 
				AnimLinkup,
				RefSkel,
				PositionTracks,
				RotationTracks,
				BoneIndex,
				HighestTargetBoneIndex,
				FALSE,
				NewWorldBones);

			// rebuild the BoneAtoms table using the current set of keys
			UpdateBoneAtomList(AnimSeq, BoneIndex, TrackIndex, NumFrames, TimePerFrame, BoneAtoms, AnimLinkup, RefSkel); 

			// determine the EndEffectorTolerance. 
			// We use the Maximum value by default, and the Minimum value
			// as we approach the end effectors
			FLOAT EndEffectorTolerance = MaxEffectorDiff;
			if (ShortestChain <= 1)
			{
				EndEffectorTolerance = MinEffectorDiff;
			}

			// Determine if a guidance track should be used to aid in choosing keys to retain
			TArray<FLOAT>* GuidanceTrack = NULL;
			FLOAT GuidanceScale = 1.0f;
			if (GuideTrackIndex != INDEX_NONE)
			{
				FRotationTrack& GuideRotTrack = RotationTracks(GuideTrackIndex);
				FTranslationTrack& GuideTransTrack = PositionTracks(GuideTrackIndex);
				GuidanceTrack = &GuideTransTrack.Times;
				GuidanceScale = ParentKeyScale;
			}
			
			// if the TargetBoneIndices array is empty, then this bone is an end effector.
			// so we add it to the list to maintain our tolerance checks
			if (TargetBoneIndices.Num() == 0)
			{
				TargetBoneIndices.AddItem(BoneIndex);
			}

			if (bActuallyFilterLinearKeys)
			{
				// filter out translations we can approximate through interpolation
				FilterLinearKeysTemplate<FVector>(
					TransTrack.PosKeys, 
					TransTrack.Times, 
					BoneAtoms,
					GuidanceTrack, 
					RawWorldBones,
					NewWorldBones,
					TargetBoneIndices,
					NumFrames,
					BoneIndex,
					ParentBoneIndex,
					GuidanceScale, 
					MaxPosDiff, 
					EndEffectorTolerance,
					EffectorDiffSocket,
					BoneData);

				// update our bone table from the current bone through the last end effector we need to test
				UpdateWorldBoneTransformRange(
					AnimSeq, 
					SkelMesh, 
					BoneData, 
					AnimLinkup,
					RefSkel,
					PositionTracks,
					RotationTracks,
					BoneIndex,
					HighestTargetBoneIndex,
					FALSE,
					NewWorldBones);

				// rebuild the BoneAtoms table using the current set of keys
				UpdateBoneAtomList(AnimSeq, BoneIndex, TrackIndex, NumFrames, TimePerFrame, BoneAtoms, AnimLinkup, RefSkel); 

				// filter out rotations we can approximate through interpolation
				FilterLinearKeysTemplate<FQuat>(
					RotTrack.RotKeys, 
					RotTrack.Times, 
					BoneAtoms,
					GuidanceTrack, 
					RawWorldBones,
					NewWorldBones,
					TargetBoneIndices,
					NumFrames,
					BoneIndex,
					ParentBoneIndex,
					GuidanceScale, 
					MaxAngleDiff, 
					EndEffectorTolerance,
					EffectorDiffSocket,
					BoneData);
			}
		}

		// make sure the final compressed keys are repesented in our NewWorldBones table
		UpdateWorldBoneTransformRange(
			AnimSeq, 
			SkelMesh, 
			BoneData, 
			AnimLinkup,
			RefSkel,
			PositionTracks,
			RotationTracks,
			BoneIndex,
			BoneIndex,
			FALSE,
			NewWorldBones);
	}
};

/**
 * Compresses the tracks passed in using the underlying compressor for this key removal codec
 */
void UAnimationCompressionAlgorithm_RemoveLinearKeys::CompressUsingUnderlyingCompressor(
	UAnimSequence* AnimSeq,
	USkeletalMesh* SkelMesh,
	const struct FAnimSetMeshLinkup& AnimLinkup,
	const TArray<FBoneData>& BoneData,
	const TArray<FTranslationTrack>& TranslationData,
	const TArray<FRotationTrack>& RotationData,
	const UBOOL bFinalPass)
{
	BitwiseCompressAnimationTracks(
		AnimSeq,
		static_cast<AnimationCompressionFormat>(TranslationCompressionFormat),
		static_cast<AnimationCompressionFormat>(RotationCompressionFormat),
		TranslationData,
		RotationData,
		TRUE);

	// record the proper runtime decompressor to use
	AnimSeq->KeyEncodingFormat = AKF_VariableKeyLerp;
	AnimationFormat_SetInterfaceLinks(*AnimSeq);
}

/**
 * Keyframe reduction algorithm that removes any keys which can be linearly approximated by neighboring keys.
 */
void UAnimationCompressionAlgorithm_RemoveLinearKeys::DoReduction(UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, const TArray<FBoneData>& BoneData)
{
#if WITH_EDITORONLY_DATA
	// Only need to do the heavy lifting if it will have some impact
	// One of these will always be true for the base class, but derived classes may choose to turn both off (e.g., in PerTrackCompression)
	const UBOOL bRunningProcessor = bRetarget || bActuallyFilterLinearKeys;

	if (GIsEditor && bRunningProcessor)
	{
		GWarn->BeginSlowTask( TEXT("Compressing animation with a RemoveLinearKeys scheme."), FALSE);
	}

	// Grab the animation linkup
	UAnimSet* AnimSet = AnimSeq->GetAnimSet();
	const INT AnimLinkupIndex = AnimSet->GetMeshLinkupIndex( SkelMesh );
	check( AnimLinkupIndex != INDEX_NONE );
	check( AnimLinkupIndex < AnimSet->LinkupCache.Num() );

	const FAnimSetMeshLinkup& AnimLinkup = AnimSet->LinkupCache( AnimLinkupIndex );

	// If the processor is to be run, then additive animations need to be converted from relative to absolute
	const UBOOL bNeedToConvertBackToAdditive = bRunningProcessor ? ConvertFromRelativeSpace(AnimSeq, AnimLinkup) : false;

	// Separate the raw data into tracks and remove trivial tracks (all the same value)
	TArray<FTranslationTrack> TranslationData;
	TArray<FRotationTrack> RotationData;
	SeparateRawDataIntoTracks(AnimSeq->RawAnimationData, AnimSeq->SequenceLength, TranslationData, RotationData);
	FilterBeforeMainKeyRemoval(AnimSeq, SkelMesh, AnimLinkup, BoneData, TranslationData, RotationData);

	if (bRunningProcessor)
	{
#if TIME_LINEAR_KEY_REMOVAL
		DOUBLE TimeStart = appSeconds();
#endif
		// compress this animation without any key-reduction to prime the codec
		CompressUsingUnderlyingCompressor(
			AnimSeq,
			SkelMesh,
			AnimLinkup,
			BoneData, 
			TranslationData,
			RotationData,
			FALSE);

		// now remove the keys which can be approximated with linear interpolation
		ProcessAnimationTracks(
			AnimSeq,
			SkelMesh,
			AnimLinkup,
			BoneData,
			TranslationData,
			RotationData);
#if TIME_LINEAR_KEY_REMOVAL
		DOUBLE ElapsedTime = appSeconds() - TimeStart;
		debugf(TEXT("ProcessAnimationTracks time is (%f) seconds"),ElapsedTime);
#endif

		// if previously additive, convert back to relative-space
		if( bNeedToConvertBackToAdditive )
		{
			ConvertToRelativeSpace(AnimSeq, TranslationData, RotationData, AnimLinkup);
		}
	}

	// Remove Translation Keys from tracks marked bAnimRotationOnly
	FilterAnimRotationOnlyKeys(TranslationData, AnimSeq, SkelMesh);

	// compress the final (possibly key-reduced) tracks into the anim sequence buffers
	CompressUsingUnderlyingCompressor(
		AnimSeq,
		SkelMesh,
		AnimLinkup,
		BoneData,
		TranslationData,
		RotationData,
		TRUE);

	if (GIsEditor && bRunningProcessor)
	{
		GWarn->EndSlowTask();
	}
	AnimSeq->CompressionScheme = static_cast<UAnimationCompressionAlgorithm*>( StaticDuplicateObject( this, this, AnimSeq, TEXT("None"), ~RF_RootSet ) );
#endif // WITH_EDITORONLY_DATA
}

