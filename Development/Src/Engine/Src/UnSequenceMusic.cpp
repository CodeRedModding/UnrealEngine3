/*=============================================================================
   UnSequenceMusic.cpp: Gameplay Music Sequence native code
   Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineSequenceClasses.h"
#include "EngineAudioDeviceClasses.h"

IMPLEMENT_CLASS(UMusicTrackDataStructures);
IMPLEMENT_CLASS(USeqAct_PlayMusicTrack);

void USeqAct_PlayMusicTrack::Activated()
{
	AWorldInfo *WorldInfo = GWorld->GetWorldInfo();
	if (WorldInfo != NULL)
	{
		WorldInfo->UpdateMusicTrack(MusicTrack);
	}
}

/**
 * Helper function to check if either the mobile build directory (ex: Game\Build\IPhone or Game\Build\Android)
 * exists, or if specified, the MP3 file within the Resources\Music folder of the mobile build directory
 *
 * @param MobileBuildDir - The mobile build directory to look for/in
 * @param MP3Filename - The name of the mp3 file to check for. If NULL, only check the directory
 *
 * @return - Whether or not the directory or file was found
 *
 * @note - The FString comparison is not case sensitive. If the MP3Filename is found, it will be renamed
 * to match the capitalization of the actual file name
 */
UBOOL MobileMP3Exists(FString MobileBuildDir, FString* MP3Filename=NULL)
{
	// list of FStrings to store the results of the GFileManager's FindFiles function
	TArray<FString> ResultFileList;
	FString BuildDirectory = FString("..\\..\\") + GGameName + FString("Game\\Build\\");

	// check if the mobile build directory exists
	if (MP3Filename == NULL)
	{
		GFileManager->FindFiles(ResultFileList, *(BuildDirectory + MobileBuildDir), FALSE, TRUE);
		return ResultFileList.Num() > 0;
	}
	
	// if the MP3Filename parameter is specified, check if it exists in the mobile build directory
	FString MP3FilePath = FString("\\Resources\\Music\\") + *MP3Filename + FString(".mp3");
	GFileManager->FindFiles(ResultFileList, *(BuildDirectory + MobileBuildDir + MP3FilePath), TRUE, FALSE);

	if (ResultFileList.Num() > 0)
	{
		// rename the MP3Filename parameter to match the capitalization of the actual file name that was found
		*MP3Filename = FFilename(ResultFileList(0)).GetBaseFilename();
		return TRUE;
	}

	return FALSE;
}

/**
 * Helper function to issue a warning about the MP3Filename
 *
 * @param MessageKey - The key in the localization file for the message to show
 * @param MP3Filename - The name of the mp3 file that this message is about
 * @param MobileBuildDir - The name of the build directory/directories where the mp3 file is (or should be) found
 * @param bUsePopup - Whether to show the warning using a pop-up message (TRUE) or through the logs (FALSE)
 */
void IssueMP3FileWarning(FString SeqPathName, FString MessageKey, FString MP3Filename, FString MobileBuildDir, UBOOL bUsePopup=FALSE)
{
	if (bUsePopup)
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd(*MessageKey, TEXT("UnrealEd")), *MP3Filename, *MobileBuildDir));
	}
	else
	{
		GWarn->Logf(NAME_Warning, *(SeqPathName + FString(": ") + FString::Printf(LocalizeSecure(LocalizeUnrealEd(*MessageKey, TEXT("UnrealEd")), *MP3Filename, *MobileBuildDir))));
	}
}

/**
 * Issues a warning if the mp3 file is not found in the Resources\Music folder of any mobile build directories that exist for the game.
 * Also issues a warning if the mp3 file is found in multiple mobile build and the capitalization does not match between the file names in the different directories.
 * If the MP3Filename is found, it is renamed to match the capitalization of the actual file name since the mobile file systems are case sensitive.
 *
 * @param MP3Filename - The name of the mp3 file to look for
 * @param bPopupWarning - Whether to show the warning using a pop-up message (TRUE) or through the logs (FALSE)
 */
void VerifyMP3File(FString SeqPathName, FString* MP3Filename, UBOOL bPopupWarning=FALSE)
{
	// get copies of the MP3Filename for each platform so we can test the capitalizations between the two file names
	FString IPhoneMP3Filename = *MP3Filename;
	FString AndroidMP3Filename = *MP3Filename;
	UBOOL bIPhoneBuildDirExists = MobileMP3Exists(FString("IPhone"));
	UBOOL bAndroidBuildDirExists = MobileMP3Exists(FString("Android"));
	UBOOL bIPhoneFileFound = bIPhoneBuildDirExists ? MobileMP3Exists(FString("IPhone"), &IPhoneMP3Filename) : FALSE;
	UBOOL bAndroidFileFound = bAndroidBuildDirExists ? MobileMP3Exists(FString("Android"), &AndroidMP3Filename) : FALSE;

	// mp3 file was found in both mobile build directories ignoring capitalization
	if (bIPhoneFileFound && bAndroidFileFound)
	{
		// check if capitalization matches between the files in the different directories
		if (appStrcmp(*IPhoneMP3Filename, *AndroidMP3Filename) == 0)
		{
			*MP3Filename = IPhoneMP3Filename;
		}
		else
		{
			IssueMP3FileWarning(SeqPathName, FString("MobileMP3FileCapitalizationDoesNotMatch"), *MP3Filename, FString("IPhone and Android"), bPopupWarning);
		}
	}
	else
	{
		// only issue a warning if the file was not found but the mobile build directory for this platform DOES exist (therefore, the file SHOULD exist)
		if (bIPhoneFileFound)
		{
			*MP3Filename = IPhoneMP3Filename;
		}
		else if (bIPhoneBuildDirExists)
		{
			IssueMP3FileWarning(SeqPathName, FString("MobileMP3FileMissing"), *MP3Filename, FString("IPhone"), bPopupWarning);
		}
		
		if (bAndroidFileFound)
		{
			*MP3Filename = AndroidMP3Filename;
		}
		else if (bAndroidBuildDirExists)
		{
			IssueMP3FileWarning(SeqPathName, FString("MobileMP3FileMissing"), *MP3Filename, FString("Android"), bPopupWarning);
		}
	}
}

void USeqAct_PlayMusicTrack::PreSave()
{
#if WITH_EDITORONLY_DATA
	if (GIsCooking)
	{
		if (GCookingTarget & UE3::PLATFORM_Mobile)
		{
			if (MusicTrack.MP3Filename != FString("none") && !MusicTrack.MP3Filename.IsEmpty())
			{
				// Remove reference so it does not get cooked to save some space.
				MusicTrack.TheSoundCue = NULL;
				VerifyMP3File(GetPathName(), &MusicTrack.MP3Filename);
			}
			else if (MusicTrack.TheSoundCue != NULL)
			{
				GWarn->Logf(NAME_Warning, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("MobileMP3FilenameEmpty", TEXT("UnrealEd")), *GetPathName())));
			}
		}
		else if (MusicTrack.TheSoundCue == NULL && (MusicTrack.MP3Filename != FString("none") || !MusicTrack.MP3Filename.IsEmpty()))
		{
			GWarn->Logf(NAME_Warning, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("MP3SoundCueMissing", TEXT("UnrealEd")), *GetPathName())));
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void USeqAct_PlayMusicTrack::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (MusicTrack.MP3Filename != FString("none") && !MusicTrack.MP3Filename.IsEmpty())
	{
		VerifyMP3File(GetPathName(), &MusicTrack.MP3Filename, TRUE);
	}
}