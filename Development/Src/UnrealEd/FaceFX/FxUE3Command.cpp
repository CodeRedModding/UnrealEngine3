//------------------------------------------------------------------------------
// The UE3 command.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2006 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#include "UnrealEd.h"
#include "stdwx.h"

#ifdef __UNREAL__

#include "FxUE3Command.h"
#include "FxVM.h"
#include "FxSessionProxy.h"
#include "FxAnimUserData.h"

#include "Engine.h"

namespace OC3Ent
{

namespace Face
{

FxBool LinkAnim( const FxString& groupName, const FxString& animName,
				 const FxString& soundCuePath, const FxString& soundNodeWaveName )
{
	FxAnim* pTempAnim = NULL;
	if( FxSessionProxy::GetAnim(groupName, animName, &pTempAnim) )
	{
		USoundCue* SoundCue = LoadObject<USoundCue>(NULL, ANSI_TO_TCHAR(soundCuePath.GetData()), NULL, LOAD_NoWarn, NULL);
		if( SoundCue )
		{
			pTempAnim->SetSoundCuePath(soundCuePath);
			pTempAnim->SetSoundNodeWave(soundNodeWaveName);
			pTempAnim->SetSoundCueIndex(FxInvalidIndex);
			pTempAnim->SetSoundCuePointer(SoundCue);
			FxString selectedAnimGroupName;
			FxString selectedAnimName;
			FxSessionProxy::GetSelectedAnimGroup(selectedAnimGroupName);
			FxSessionProxy::GetSelectedAnim(selectedAnimName);
			// If the animation being linked is the currently selected
			// animation, reselect it so that the new USoundCue and
			// USoundNodeWave objects are used.
			if( selectedAnimGroupName == groupName &&
				selectedAnimName == animName )
			{
				FxSessionProxy::SetSelectedAnimGroup(FxAnimGroup::Default.GetAsString());
				FxAnimUserData* pUserData = reinterpret_cast<FxAnimUserData*>(pTempAnim->GetUserData());
				if( pUserData )
				{
					pUserData->SetAudio(NULL);
				}
				FxSessionProxy::SetSelectedAnimGroup(groupName);
				FxSessionProxy::SetSelectedAnim(animName);
			}
			else
			{
				FxAnimUserData* pUserData = reinterpret_cast<FxAnimUserData*>(pTempAnim->GetUserData());
				if( pUserData )
				{
					pUserData->SetAudio(NULL);
				}
			}
			FxSessionProxy::SetIsForcedDirty(FxTrue);
		}
		return FxTrue;
	}
	FxVM::DisplayError("the specified animation does not exist!");
	return FxFalse;
}

FX_IMPLEMENT_CLASS(FxUE3Command, 0, FxCommand);

FxUE3Command::FxUE3Command()
{
}

FxUE3Command::~FxUE3Command()
{
}

FxCommandSyntax FxUE3Command::CreateSyntax( void )
{
	FxCommandSyntax newSyntax;
	newSyntax.AddArgDesc(FxCommandArgumentDesc("-l", "-link", CAT_Flag));
	newSyntax.AddArgDesc(FxCommandArgumentDesc("-g", "-group", CAT_String));
	newSyntax.AddArgDesc(FxCommandArgumentDesc("-a", "-anim", CAT_String));
	newSyntax.AddArgDesc(FxCommandArgumentDesc("-sc", "-soundcuepath", CAT_String));
	newSyntax.AddArgDesc(FxCommandArgumentDesc("-snw", "-soundnodewavename", CAT_String));
	newSyntax.AddArgDesc(FxCommandArgumentDesc("-p", "-package", CAT_String));
	newSyntax.AddArgDesc(FxCommandArgumentDesc("-usc", "-updatesoundcues", CAT_Flag));
	newSyntax.AddArgDesc(FxCommandArgumentDesc("-lfan", "-linkfromanimname", CAT_Flag));
	return newSyntax;
}

FxCommandError FxUE3Command::Execute( const FxCommandArgumentList& argList )
{
	if( argList.GetArgument("-linkfromanimname") )
	{
		FxString groupName;
		FxString animName;
		FxString soundCuePath;
		FxString soundNodeWaveName;
		FxString packageName;

		// Special Command for Gears of War
		if( argList.GetArgument("-package", packageName) &&
			argList.GetArgument("-group", groupName) )
		{
			FxActor* pActor = NULL;
			if( FxSessionProxy::GetActor(&pActor) )
			{
				// Loop through all animation groups.
				FxSize numGroups = pActor->GetNumAnimGroups();
				FxSize groupIndex = pActor->FindAnimGroup(groupName.GetData());
				if( FxInvalidIndex != groupIndex )
				{
					FxAnimGroup& animGroup = pActor->GetAnimGroup(groupIndex);
					FxSize numAnims = animGroup.GetNumAnims();
					for( FxSize i = 0; i < numAnims; ++i )
					{
						const FxAnim& anim = animGroup.GetAnim(i);
						soundCuePath = packageName;
						soundCuePath += ".";
						soundCuePath += anim.GetNameAsString();
						soundCuePath += "Cue";
						//soundCuePath += anim.GetSoundCuePath().AfterLast('.');
						soundNodeWaveName = anim.GetSoundNodeWave();

						FxString msg("  updating ");
						msg += anim.GetSoundCuePath();
						msg += " to ";
						msg += soundCuePath;
						FxVM::DisplayInfo(msg);

						LinkAnim(groupName, anim.GetNameAsString(), soundCuePath, soundNodeWaveName);
					}
					return CE_Success;
				}
			}
		}
	}

	if( argList.GetArgument("-link") )
	{
		FxString groupName;
		FxString animName;
		FxString soundCuePath;
		FxString soundNodeWaveName;
		FxString packageName;

		// Batch redirect operation.  A condition is that the only thing different
		// about the data is the name of the package; all soundCueNames and 
		// soundNodeWaveNames must match what is currently stored in the animations.
		if( argList.GetArgument("-package", packageName) &&
			argList.GetArgument("-group", groupName) )
		{
			FxActor* pActor = NULL;
			if( FxSessionProxy::GetActor(&pActor) )
			{
				// Loop through all animation groups.
				FxSize numGroups = pActor->GetNumAnimGroups();
				FxSize groupIndex = pActor->FindAnimGroup(groupName.GetData());
				if( FxInvalidIndex != groupIndex )
				{
					FxAnimGroup& animGroup = pActor->GetAnimGroup(groupIndex);
					FxSize numAnims = animGroup.GetNumAnims();
					for( FxSize i = 0; i < numAnims; ++i )
					{
						const FxAnim& anim = animGroup.GetAnim(i);
						soundCuePath = packageName;
						soundCuePath += ".";
						soundCuePath += anim.GetSoundCuePath().AfterLast('.');
						soundNodeWaveName = anim.GetSoundNodeWave();

						FxString msg("  updating ");
						msg += anim.GetSoundCuePath();
						msg += " to ";
						msg += soundCuePath;
						FxVM::DisplayInfo(msg);

						LinkAnim(groupName, anim.GetNameAsString(), soundCuePath, soundNodeWaveName);
					}
					return CE_Success;
				}
			}
		}

		if( argList.GetArgument("-group", groupName)           &&
			argList.GetArgument("-anim", animName)             &&
			argList.GetArgument("-soundcuepath", soundCuePath) &&
			argList.GetArgument("-soundnodewavename", soundNodeWaveName) )
		{
			if( LinkAnim(groupName, animName, soundCuePath, soundNodeWaveName) )
			{
				return CE_Success;
			}
		}
		else
		{
			FxVM::DisplayError("to use -link, -group, -anim, -soundcuepath, and -soundnodewavename must all be specified!");
		}
	}
	else if( argList.GetArgument("-updatesoundcues") )
	{
		UpdateSoundCues();
		FxSessionProxy::SetIsForcedDirty(FxTrue);
		return CE_Success;
	}
	else
	{
		FxVM::DisplayError("invalid or no operation specified!");
	}
	return CE_Failure;
}

//@note This is a quick E3 hack and assumes that there are no external FaceFX animation sets
//      involved in this task and that there is a one-to-one mapping between FaceFX animations
//      and USoundCueObjects.
void FxUE3Command::UpdateSoundCues( void )
{
	FxActor*		pActor			= NULL;
	UFaceFXAsset*	pFaceFXAsset	= NULL;
	UFaceFXAnimSet* MountedAnimSet	= NULL;

	if( FxSessionProxy::GetActor(&pActor) && FxSessionProxy::GetFaceFXAsset(&pFaceFXAsset) )
	{
		// Loop through all animation groups.
		FxSize numGroups = pActor->GetNumAnimGroups();
		for( FxSize i = 0; i < numGroups; ++i )
		{
			FxAnimGroup& animGroup = pActor->GetAnimGroup(i);

			// See if this group is an external animset mounted
			MountedAnimSet = NULL;
			for(INT MountIdx=0; MountIdx < pFaceFXAsset->MountedFaceFXAnimSets.Num(); MountIdx++)
			{
				UFaceFXAnimSet* AnimSet = pFaceFXAsset->MountedFaceFXAnimSets(MountIdx);
				
				//FxVM::DisplayInfo( FxString("MountedFaceFXAnimSets ") + 
				//	FxString(MountedAnimSet != NULL ? TCHAR_TO_ANSI(*AnimSet->GetPathName()) : "NULL") );

				if( AnimSet )
				{
					FxAnimSet* fAnimSet = AnimSet->GetFxAnimSet();
					if( fAnimSet )
					{
						const FxAnimGroup& MountedAnimGroup = fAnimSet->GetAnimGroup();

						//FxVM::DisplayInfo(FxString(" Checking mounted group: ") + MountedAnimGroup.GetNameAsString() + FxString(" for existing group: ") + animGroup.GetNameAsString() );
						
						// This group has been mounted, since there is a one to one mapping between
						// groups and mounted animsets.
						// as FaceFX does not support mounting sets with a group that already exists.
						if( MountedAnimGroup.GetName() == animGroup.GetName() )
						{
							MountedAnimSet = AnimSet; 
							break;
						}
					}
				}
			}

			// Loop through all animations in the group.
			FxSize numAnims = animGroup.GetNumAnims();
			for( FxSize j = 0; j < numAnims; ++j )
			{
				FxAnim* pAnim = animGroup.GetAnimPtr(j);
				if( pAnim )
				{
					// Grab the USoundCue object from the animation.
					USoundCue* pSoundCue = reinterpret_cast<USoundCue*>(pAnim->GetSoundCuePointer());
					if( pSoundCue )
					{
						// Set the string references in the USoundCue object to link to the current animation.
						FString FaceFXGroupName(ANSI_TO_TCHAR(animGroup.GetNameAsCstr()));
						FString FaceFXAnimName(ANSI_TO_TCHAR(pAnim->GetNameAsCstr()));
						
						UBOOL bNeedsToBeSaved = FALSE;

						// External AnimSet reference
						if( pSoundCue->FaceFXAnimSetRef != MountedAnimSet )
						{
							FxVM::DisplayInfo( FxString(" Updating FaceFXAnimSetRef ") + 
								FxString(MountedAnimSet != NULL ? TCHAR_TO_ANSI(*MountedAnimSet->GetPathName()) : "NULL") );

							pSoundCue->FaceFXAnimSetRef = MountedAnimSet;
							bNeedsToBeSaved = TRUE;
						}

						// GroupName
						if( pSoundCue->FaceFXGroupName != FaceFXGroupName )
						{
							FxVM::DisplayInfo( FxString(" Updating FaceFXGroupName ") + animGroup.GetNameAsString() );
							pSoundCue->FaceFXGroupName = FaceFXGroupName;
							bNeedsToBeSaved = TRUE;

						}

						// AnimName
						if( pSoundCue->FaceFXAnimName != FaceFXAnimName )
						{
							FxVM::DisplayInfo( FxString(" Updating FaceFXAnimName ") + pAnim->GetNameAsString() );
							pSoundCue->FaceFXAnimName  = FaceFXAnimName;
							bNeedsToBeSaved = TRUE;
						}

						// Mark the package containing the USoundCue object as dirty.
						if( bNeedsToBeSaved )
						{
							FxString msg("Marking ");
							msg += pAnim->GetSoundCuePath();
							msg += " as dirty.";
							FxVM::DisplayInfo(msg);

							pSoundCue->MarkPackageDirty();
						}
					}
				}
			}
		}
	}
}

} // namespace Face

} // namespace OC3Ent

#endif // __UNREAL__
