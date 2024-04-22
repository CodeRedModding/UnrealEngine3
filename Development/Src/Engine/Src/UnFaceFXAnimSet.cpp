/*=============================================================================
	UnFaceFXAnimSet.cpp: Code for managing FaceFX AnimSets
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnFaceFXSupport.h"

#if WITH_FACEFX
using namespace OC3Ent;
using namespace Face;
#endif

IMPLEMENT_CLASS(UFaceFXAnimSet);

#if WITH_FACEFX
/** Map of all the FaceFXAssets to which a FaceFXAnimSet is mounted. */
TMultiMap<UFaceFXAnimSet*, UFaceFXAsset*> GFaceFXAnimSetToAssetMap;

/** Map of all the FaceFXAnimSets which are mounted to a particular FaceFXAsset. */
TMultiMap<UFaceFXAsset*, UFaceFXAnimSet*> GFaceFXAssetToAnimSetMap;
#endif

void UFaceFXAnimSet::CreateFxAnimSet( class UFaceFXAsset* FaceFXAsset )
{
#if WITH_EDITORONLY_DATA
	DefaultFaceFXAsset = FaceFXAsset;

#if WITH_FACEFX
	if( DefaultFaceFXAsset && !InternalFaceFXAnimSet )
	{
		FxName AnimSetName(TCHAR_TO_ANSI(*GetName()));
		FxAnimSet* AnimSet = new FxAnimSet();
		AnimSet->SetName(AnimSetName);
		FxAnimGroup ContainedAnimGroup;
		ContainedAnimGroup.SetName(AnimSetName);
		AnimSet->SetAnimGroup(ContainedAnimGroup);
		FxActor* OwningActor = DefaultFaceFXAsset->GetFxActor();
		if( OwningActor )
		{
			AnimSet->SetOwningActorName(OwningActor->GetName());
		}
		InternalFaceFXAnimSet = AnimSet;
	}
#endif
#endif // WITH_EDITORONLY_DATA
}

#if WITH_FACEFX
class FxAnimSet* UFaceFXAnimSet::GetFxAnimSet( void )
{
	if( !InternalFaceFXAnimSet )
		return NULL;

	FxAnimSet* AnimSet = (FxAnimSet*)InternalFaceFXAnimSet;
	return AnimSet;
}
#endif

void UFaceFXAnimSet::FixupReferencedSoundCues()
{
#if WITH_EDITORONLY_DATA
#if WITH_FACEFX
	NumLoadErrors = 0;
	FxAnimSet* AnimSet = GetFxAnimSet();
	if( AnimSet )
	{
		const FxAnimGroup& AnimGroup = AnimSet->GetAnimGroup();
		FxSize NumAnims = AnimGroup.GetNumAnims();

		// If ReferencedSoundCues is empty and there are Anims to add we need to build it first.
		if( 0 == ReferencedSoundCues.Num() && NumAnims > 0)
		{
#if CONSOLE
			debugf(NAME_Warning,TEXT("%s has %i anims but no referenced sound cues."), *GetFullName(), NumAnims);
#else
			// Build the ReferencedSoundCues array and set the indices in each
			// animation.
			ReferencedSoundCues.Reserve(NumAnims);
			for( FxSize i = 0; i < NumAnims; ++i )
			{
				FxAnim* Anim = AnimSet->GetAnimPtr(i);
				if( Anim )
				{
					Anim->SetSoundCueIndex(FxInvalidIndex);
					FxString SoundCuePath = Anim->GetSoundCuePath();
					if( SoundCuePath.Length() > 0 )
					{
						USoundCue* SoundCue = LoadObject<USoundCue>(NULL, ANSI_TO_TCHAR(SoundCuePath.GetData()), NULL, LOAD_NoWarn, NULL);
						if( SoundCue )
						{
							ReferencedSoundCues.AddItem(SoundCue);
							Anim->SetSoundCueIndex(ReferencedSoundCues.Num()-1);
							Anim->SetSoundCuePointer(SoundCue);
						}
						else
						{
							NumLoadErrors++;
							FString ExpectedPath(ANSI_TO_TCHAR(SoundCuePath.GetData()));
							debugf(NAME_Warning, TEXT("FaceFX: Found lost sound cue in FaceFXAnimSet %s for animation %s.%s.%s (expected path %s)"),
								*GetFullName(),
								ANSI_TO_TCHAR(AnimSet->GetNameAsCstr()), 
								ANSI_TO_TCHAR(AnimGroup.GetNameAsCstr()), 
								ANSI_TO_TCHAR(Anim->GetNameAsCstr()), 
								*ExpectedPath);
						}
					}
				}
			}

			if( ReferencedSoundCues.Num() > 0 )
			{
				// Update the RawFaceFXAnimSetBytes (checking for cooking and byte ordering here is
				// probably a little overkill).
				FxArchive::FxArchiveByteOrder ByteOrder = FxArchive::ABO_LittleEndian;
				// If cooking, save the data in Big Endian format.
				if( GCookingTarget & UE3::PLATFORM_Console )
				{
#if __INTEL_BYTE_ORDER__
					ByteOrder = FxArchive::ABO_BigEndian;
#else
					ByteOrder = FxArchive::ABO_LittleEndian;
#endif
				}
				FxByte* AnimSetMemory = NULL;
				FxSize  NumAnimSetMemoryBytes = 0;
				if( !FxSaveAnimSetToMemory(*AnimSet, AnimSetMemory, NumAnimSetMemoryBytes, ByteOrder) )
				{
					warnf(TEXT("FaceFX: Failed to save animset for %s"), *GetPathName());
				}
				RawFaceFXAnimSetBytes.Empty();
				RawFaceFXAnimSetBytes.Add(NumAnimSetMemoryBytes);
				appMemcpy(RawFaceFXAnimSetBytes.GetData(), AnimSetMemory, NumAnimSetMemoryBytes);
				FxFree(AnimSetMemory, NumAnimSetMemoryBytes);

				// Mark the package as dirty.
				MarkPackageDirty();
			}
#endif
		}
		else
		{
			// ReferencedSoundCues was valid so link up each animation.
			UBOOL bMadeCorrections = FALSE;
			for( FxSize i = 0; i < NumAnims; ++i )
			{
				FxAnim* Anim = AnimSet->GetAnimPtr(i);
				if( Anim )
				{
					FxSize SoundCueIndex = Anim->GetSoundCueIndex();
					const FxString& SoundCuePath = Anim->GetSoundCuePath();
					if( FxInvalidIndex != SoundCueIndex)
					{
						if( SoundCueIndex < static_cast<FxSize>(ReferencedSoundCues.Num()) )  
						{  
							USoundCue* SoundCue = ReferencedSoundCues(SoundCueIndex);
							Anim->SetSoundCuePointer(SoundCue);
#if !CONSOLE
							if( SoundCue && SoundCuePath.Length() > 0 )
							{
								FString ExpectedPath(ANSI_TO_TCHAR(SoundCuePath.GetData()));
								FString ActualPath = SoundCue->GetPathName();
								if( ExpectedPath != ActualPath )
								{
									bMadeCorrections = TRUE;
									Anim->SetSoundCuePath(FxString(TCHAR_TO_ANSI(*ActualPath)));
									debugf(NAME_Warning, TEXT("FaceFX: Corrected inconsistent sound cue linkage in FaceFXAnimSet %s for animation %s.%s.%s (expected path %s -> actual path %s)"),
										*GetFullName(),
										ANSI_TO_TCHAR(AnimSet->GetNameAsCstr()), 
										ANSI_TO_TCHAR(AnimGroup.GetNameAsCstr()), 
										ANSI_TO_TCHAR(Anim->GetNameAsCstr()), 
										*ExpectedPath, 
										*ActualPath);
								}
							}
							else if( !SoundCue && SoundCuePath.Length() > 0 )
							{
								NumLoadErrors++;
								FString ExpectedPath(ANSI_TO_TCHAR(SoundCuePath.GetData()));
								debugf(NAME_Warning, TEXT("FaceFX: Found lost sound cue in FaceFXAnimSet %s for animation %s.%s.%s (expected path %s)"),
									*GetFullName(),
									ANSI_TO_TCHAR(AnimSet->GetNameAsCstr()), 
									ANSI_TO_TCHAR(AnimGroup.GetNameAsCstr()), 
									ANSI_TO_TCHAR(Anim->GetNameAsCstr()), 
									*ExpectedPath);
							}
#endif
						}  
						else  
						{  
							Anim->SetSoundCueIndex(FxInvalidIndex);  
							Anim->SetSoundCuePointer(NULL);  
							GWarn->Logf( TEXT("Error finding SoundCue for FaceFX Anim: %s.%s.%s. Open and resave FaceFXAnimSet (%s)."), 
								ANSI_TO_TCHAR(AnimSet->GetNameAsCstr()), 
								ANSI_TO_TCHAR(AnimGroup.GetNameAsCstr()), 
								ANSI_TO_TCHAR(Anim->GetNameAsCstr()),
								*GetFullName());
						}  
					}
					else if(SoundCuePath.Length() > 0)
					{
						NumLoadErrors++;
						FString ExpectedPath(ANSI_TO_TCHAR(SoundCuePath.GetData()));
						debugf(NAME_Warning, TEXT("FaceFX: Found lost sound cue in FaceFXAnimSet %s for animation %s.%s.%s (expected path %s)"),
							*GetFullName(),
							ANSI_TO_TCHAR(AnimSet->GetNameAsCstr()), 
							ANSI_TO_TCHAR(AnimGroup.GetNameAsCstr()), 
							ANSI_TO_TCHAR(Anim->GetNameAsCstr()), 
							*ExpectedPath);
					}
				}
			}
#if !CONSOLE
			if( bMadeCorrections )
			{
				// Update the RawFaceFXAnimSetBytes (checking for cooking and byte ordering here is
				// probably a little overkill).
				FxArchive::FxArchiveByteOrder ByteOrder = FxArchive::ABO_LittleEndian;
				// If cooking, save the data in Big Endian format.
				if( GCookingTarget & UE3::PLATFORM_Console )
				{
#if __INTEL_BYTE_ORDER__
					ByteOrder = FxArchive::ABO_BigEndian;
#else
					ByteOrder = FxArchive::ABO_LittleEndian;
#endif
				}
				FxByte* AnimSetMemory = NULL;
				FxSize  NumAnimSetMemoryBytes = 0;
				if( !FxSaveAnimSetToMemory(*AnimSet, AnimSetMemory, NumAnimSetMemoryBytes, ByteOrder) )
				{
					warnf(TEXT("FaceFX: Failed to save animset for %s"), *GetPathName());
				}
				RawFaceFXAnimSetBytes.Empty();
				RawFaceFXAnimSetBytes.Add(NumAnimSetMemoryBytes);
				appMemcpy(RawFaceFXAnimSetBytes.GetData(), AnimSetMemory, NumAnimSetMemoryBytes);
				FxFree(AnimSetMemory, NumAnimSetMemoryBytes);

				// Mark the package as dirty.
				MarkPackageDirty();
			}
#endif
		}
	}
#endif
#endif // WITH_EDITORONLY_DATA
}

FString UFaceFXAnimSet::GetDesc()
{
	INT NumAnims = 0;
	FString OriginalActorName;
#if WITH_FACEFX
	FxAnimSet* AnimSet = GetFxAnimSet();
	if( AnimSet )
	{
		NumAnims = AnimSet->GetAnimGroup().GetNumAnims();
		OriginalActorName = ANSI_TO_TCHAR(AnimSet->GetOwningActorName().GetAsCstr());
	}
#endif
	return FString::Printf(TEXT("%d Anims, Created From: %s"), NumAnims, *OriginalActorName);
}

void UFaceFXAnimSet::PostLoad()
{
	Super::PostLoad();

	if( ( GIsEditor == TRUE ) && ( GIsCooking == FALSE ) && ( GIsUCC == FALSE ) )
	{
		FixupReferencedSoundCues();
	}
}

void UFaceFXAnimSet::FinishDestroy()
{
	// Clean up FaceFX stuff when the AnimSet gets garbage collected.
#if WITH_FACEFX
	TArray<UFaceFXAsset*> OutAssets;
	GFaceFXAnimSetToAssetMap.MultiFind(this, OutAssets);

	// Unmount this FaceFXAnimSet from all FaceFXAssets it is actively mounted in.
	for(INT i=0; i<OutAssets.Num(); i++)
	{
		UFaceFXAsset* Asset = OutAssets(i);
		check(Asset);
		// This will update the maps to remove the pairs between this set and this asset
		Asset->UnmountFaceFXAnimSet(this);
	}

	FxAnimSet* AnimSet = GetFxAnimSet();
	if( AnimSet )
	{
		delete AnimSet;
		AnimSet = NULL;
		InternalFaceFXAnimSet = NULL;
	}
#endif

	Super::FinishDestroy();
}

void UFaceFXAnimSet::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_FACEFX
	FxAnimSet* AnimSet = GetFxAnimSet();
#endif

	// FaceFX data is embedded as raw bytes straight into packages.
	if( Ar.IsSaving() )
	{
		RawFaceFXAnimSetBytes.Empty();
		
#if WITH_FACEFX
		if( AnimSet )
		{
			FxArchive::FxArchiveByteOrder ByteOrder = FxArchive::ABO_LittleEndian;
			// If cooking, save the data in Big Endian format.
			if( GCookingTarget & UE3::PLATFORM_Console )
			{
#if __INTEL_BYTE_ORDER__
				ByteOrder = FxArchive::ABO_BigEndian;
#else
				ByteOrder = FxArchive::ABO_LittleEndian;
#endif
			}
			FxByte* AnimSetMemory = NULL;
			FxSize  NumAnimSetMemoryBytes = 0;
			if( !FxSaveAnimSetToMemory(*AnimSet, AnimSetMemory, NumAnimSetMemoryBytes, ByteOrder) )
			{
				warnf(TEXT("FaceFX: Failed to save animset for %s"), *GetPathName());
			}
			RawFaceFXAnimSetBytes.Add(NumAnimSetMemoryBytes);
			appMemcpy(RawFaceFXAnimSetBytes.GetData(), AnimSetMemory, NumAnimSetMemoryBytes);
			FxFree(AnimSetMemory, NumAnimSetMemoryBytes);
		}
#endif

		Ar << RawFaceFXAnimSetBytes;
		Ar << RawFaceFXMiniSessionBytes;
	}
	else if( Ar.IsLoading() )
	{
		RawFaceFXAnimSetBytes.Empty();
		RawFaceFXMiniSessionBytes.Empty();		

		Ar << RawFaceFXAnimSetBytes;
		Ar << RawFaceFXMiniSessionBytes;

#if WITH_FACEFX
		if( AnimSet )
		{
			delete AnimSet;
		}
		AnimSet = new FxAnimSet();
		InternalFaceFXAnimSet = AnimSet;

#if LOG_FACEFX_PERF
		DWORD StartSerialization = appCycles();
#endif
		if( !FxLoadAnimSetFromMemory(*AnimSet, static_cast<FxByte*>(RawFaceFXAnimSetBytes.GetData()), RawFaceFXAnimSetBytes.Num()) )
		{
			warnf(TEXT("FaceFX: Failed to load animset for %s"), *GetPathName());
		}
#if LOG_FACEFX_PERF
		DWORD EndSerialization = appCycles();
#endif
#endif
#if LOG_FACEFX_PERF
		FxSize NumBytes = RawFaceFXAnimSetBytes.Num();
#endif

		// Flush raw bytes that are only needed in the editor and ucc.
		if( !GIsEditor && !GIsUCC )
		{
			RawFaceFXAnimSetBytes.Empty();
			RawFaceFXMiniSessionBytes.Empty();
		}

#if LOG_FACEFX_PERF
		debugf(TEXT("DevFaceFX_Perf: Loading animset %s : (Total Bytes: %d) Serialization: %f ms"), 
			*GetPathName(),
			NumBytes,
			(EndSerialization-StartSerialization)*GSecondsPerCycle*1000.0f);
#endif
	}
	else if( Ar.IsCountingMemory() )
	{
		//TODO: Implement these without going through the save process
		//Ar << InternalFaceFXAnimSet;
		//Ar << RawFaceFXAnimSetBytes;
		//Ar << RawFaceFXMiniSessionBytes;
	}
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UFaceFXAnimSet::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

#if WITH_EDITORONLY_DATA
	// if we aren't keeping any non-stripped platforms, we can toss the data
	if (!(PlatformsToKeep & ~UE3::PLATFORM_Stripped))
	{
//		RawFaceFXAnimSetBytes.Empty();
		RawFaceFXMiniSessionBytes.Empty();
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UFaceFXAnimSet::GetResourceSize()
{
#if WITH_FACEFX
	INT Size = 0;
	if (!GExclusiveResourceSizeMode)
	{	
		FArchiveCountMem CountBytesSize( this );
		CountBytesSize << RawFaceFXAnimSetBytes;
#if WITH_EDITORONLY_DATA
		CountBytesSize << ReferencedSoundCues;	
#endif // WITH_EDITORONLY_DATA
		Size += CountBytesSize.GetNum();
	}

	if (!GIsEditor)
	{	
		// Add FxAnimSet size - recommended by OC3ENT
		// Originally used serialize, and that could cause memory fragmentation if we do this a lot
		// (i.e. during memory/perf captures )
		// This is an approximation of RawFaceFXAnimSetBytes, but works outside of the editor
		FxAnimSet* AnimSet = GetFxAnimSet();
		if(AnimSet)
		{
			const FxAnimGroup& Group = AnimSet->GetAnimGroup();
			for( UINT I=0; I<Group.GetNumAnims(); I++ )
			{
				const FxAnim Anim = Group.GetAnim(I);
				Size += sizeof(Anim);
				for ( UINT J=0; J<Anim.GetNumAnimCurves(); ++J )
				{
					const FxAnimCurve AnimCurve = Anim.GetAnimCurve(J);
					Size += sizeof(AnimCurve);
					if ( AnimCurve.GetNumKeys() > 0 )
					{
						Size += AnimCurve.GetNumKeys()*sizeof(FxAnimKey);
					}
				}
			}
		}
	}

	return Size;
#else
	return 0;
#endif
}

/** Get list of FaceFX animations in this AnimSet. Names are in the form GroupName.AnimName.*/
void UFaceFXAnimSet::GetSequenceNames(TArray<FString>& OutNames)
{
#if WITH_FACEFX
	FxAnimSet* AnimSet = GetFxAnimSet();
	if(AnimSet)
	{
		const FxAnimGroup& fGroup = AnimSet->GetAnimGroup();
		const FxChar* fGroupName = fGroup.GetNameAsCstr();

		INT NumAnims = static_cast<INT>(fGroup.GetNumAnims());
		for(INT j=0; j<NumAnims; j++)
		{
			const FxAnim& fAnim = fGroup.GetAnim(j);
			const FxChar* fAnimName = fAnim.GetNameAsCstr();

			FString SeqFullName = FString::Printf( TEXT("%s.%s"), ANSI_TO_TCHAR(fGroupName), ANSI_TO_TCHAR(fAnimName) );
			OutNames.AddItem(SeqFullName);
		}
	}
#endif
}
