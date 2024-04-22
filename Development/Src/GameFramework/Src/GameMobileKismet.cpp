/**
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

#include "GameFramework.h"
#include "UnIpDrv.h"

IMPLEMENT_CLASS(USeqEvent_MobileRawInput);

IMPLEMENT_CLASS(USeqEvent_MobileBase);
IMPLEMENT_CLASS(USeqEvent_MobileMotion);
IMPLEMENT_CLASS(USeqEvent_MobileZoneBase);
IMPLEMENT_CLASS(USeqEvent_MobileInput);
IMPLEMENT_CLASS(USeqEvent_MobileButton);
IMPLEMENT_CLASS(USeqEvent_MobileLook);

IMPLEMENT_CLASS(USeqEvent_HudRender);

IMPLEMENT_CLASS(USeqAct_Deproject);

IMPLEMENT_CLASS(USeqEvent_MobileSwipe);
IMPLEMENT_CLASS(USeqEvent_MobileObjectPicker);

IMPLEMENT_CLASS(USeqAct_MobileClearInputZones);
IMPLEMENT_CLASS(USeqAct_MobileRemoveInputZone);
IMPLEMENT_CLASS(USeqAct_MobileAddInputZones);
IMPLEMENT_CLASS(USeqAct_MobileSaveLoadValue);

void USeqEvent_MobileBase::Update(APlayerController* Originator, UMobilePlayerInput* OriginatorInput) {}
void USeqEvent_MobileZoneBase::UpdateZone(APlayerController* Originator, UMobilePlayerInput* OriginatorInput, UMobileInputZone* OriginatorZone) {}


/**
 * Tracks whether the zone is in an active state and what it's current values are
 *
 * @param bIsActive will be true if the zone is active or activating
 */

void USeqEvent_MobileInput::UpdateZone(APlayerController* Originator, UMobilePlayerInput* OriginatorInput, UMobileInputZone* OriginatorZone)
{
	// Copy over the important data

	CurrentX = OriginatorZone->CurrentLocation.X;
	CurrentY = OriginatorZone->CurrentLocation.Y;
	CenterX = OriginatorZone->CurrentCenter.X;
	CenterY = OriginatorZone->CurrentCenter.Y;
	
	XAxisValue = OriginatorZone->LastAxisValues.X;
	YAxisValue = OriginatorZone->LastAxisValues.Y;

	// Setup who which Output should be active.
	INT ActiveIndex = (OriginatorZone->State == ZoneState_Activating || OriginatorZone->State == ZoneState_Active)?0:1;

	TArray<INT> ActivateIndices;
	ActivateIndices.AddItem(ActiveIndex);

	// Check for activation - NOTE:  THis might not be the best way.  Look to see if this is too heavy of a performance
	// cost to call each frame.

	CheckActivate(Originator,Originator,0,&ActivateIndices);
}

void USeqEvent_MobileButton::UpdateZone(APlayerController* Originator, UMobilePlayerInput* OriginatorInput, UMobileInputZone* OriginatorZone)
{
	// is zone now active?
	UBOOL bIsActive = (OriginatorZone->State == ZoneState_Activating || OriginatorZone->State == ZoneState_Active);
	
	// default to not touched
	INT ActiveIndex = 1;
	
	if (bSendPressedOnlyOnTouchDown || bSendPressedOnlyOnTouchUp)
	{
		// send down 0th index only if it's an edge case and we want the edge
		if ((bSendPressedOnlyOnTouchDown && bIsActive && !bWasActiveLastFrame) ||
			(bSendPressedOnlyOnTouchUp && !bIsActive && bWasActiveLastFrame))
		{
			ActiveIndex = 0;
		}
	}
	else if (bIsActive)
	{
		// if down, go out the 0th index
		ActiveIndex = 0;
	}
		
	// remember for next frame
	bWasActiveLastFrame = bIsActive;

	TArray<INT> ActivateIndices;
	ActivateIndices.AddItem(ActiveIndex);

	// Check for activation - NOTE:  THis might not be the best way.  Look to see if this is too heavy of a performance
	// cost to call each frame.

	CheckActivate(Originator,Originator,0,&ActivateIndices);
}


void USeqEvent_MobileMotion::Update(APlayerController* Originator, UMobilePlayerInput* OriginatorInput)
{
	// these are converted to Unreal units in UnIn.cpp
	Pitch = OriginatorInput->aTilt.X;
	Yaw = OriginatorInput->aTilt.Y;
	Roll = OriginatorInput->aTilt.Z;

	DeltaPitch = OriginatorInput->aRotationRate.X;
	DeltaYaw = OriginatorInput->aRotationRate.Y;
	DeltaRoll = OriginatorInput->aRotationRate.Z;

	TArray<INT> ActivateIndices;
	INT ActiveIndex = 0;
	ActivateIndices.AddItem(ActiveIndex);

	// Check for activation - NOTE:  THis might not be the best way.  Look to see if this is too heavy of a performance
	// cost to call each frame.
	CheckActivate(Originator,Originator,0,&ActivateIndices);
}


/**
 * Converts a joystick zone in to a yaw and strength
 *
 * @param bIsActive will be true if the zone is active or activating
 */

void USeqEvent_MobileLook::UpdateZone(APlayerController* Originator, UMobilePlayerInput* OriginatorInput, UMobileInputZone* OriginatorZone)
{
	// Setup who which Output should be active.
	INT ActiveIndex = (OriginatorZone->State == ZoneState_Activating || OriginatorZone->State == ZoneState_Active)?0:1;

	if (ActiveIndex == 0)
	{
		FLOAT AdjustedX = OriginatorZone->CurrentLocation.X - OriginatorZone->CurrentCenter.X;
		FLOAT AdjustedY = OriginatorZone->CurrentLocation.Y - OriginatorZone->CurrentCenter.Y;

		Yaw = Abs((appAtan2(AdjustedX, AdjustedY) * 10430.2192) - 32767);
		StickStrength = Abs((OriginatorZone->CurrentLocation - OriginatorZone->CurrentCenter).Size());
	}

	TArray<INT> ActivateIndices;
	ActivateIndices.AddItem(ActiveIndex);

	RotationVector = FRotator(0,static_cast<INT>(Yaw),0).Vector();



	// Check for activation - NOTE:  THis might not be the best way.  Look to see if this is too heavy of a performance
	// cost to call each frame.

	CheckActivate(Originator,Originator,0,&ActivateIndices);
}

/**
 * Handle a touch event coming from the device. 
 *
 * @param Originator		is a reference to the PC that caused the input
 * @param Handle			the id of the touch
 * @param Type				What type of event is this
 * @param TouchLocation		Where the touch occurred
 * @param DeviceTimestamp	Input event timestamp from the device
 */

void USeqEvent_MobileRawInput::InputTouch(APlayerController* Originator, UINT Handle, UINT InTouchpadIndex, BYTE Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp)
{
	// only respond to touches from the proper touchpad
	if (InTouchpadIndex != TouchpadIndex)
	{
		return;
	}

	INT ActiveIndex = TT_MAX;

	// Setup the Active Index

	switch (Type)
	{
		case Touch_Began : ActiveIndex = 0; break;
		case Touch_Moved : ActiveIndex = 1; break;
		case Touch_Ended : ActiveIndex = 2; break;
		case Touch_Cancelled : ActiveIndex = 3; break;
	}

	TouchLocationX = TouchLocation.X;
	TouchLocationY = TouchLocation.Y;
	TimeStamp = FLOAT(DeviceTimestamp);

	TArray<INT> ActivateIndices;
	ActivateIndices.AddItem(ActiveIndex);
	CheckActivate(Originator,Originator,0,&ActivateIndices);
}

/**
 *  Take the current values and deproject it                                                                     
 */
void USeqAct_Deproject::Activated()
{
	// We need the viewport size, so start there
	FVector2D ViewportSize;
	UGameViewportClient* GameViewportClient = GEngine->GameViewport;
	if (GameViewportClient != NULL)
	{
		GameViewportClient->GetViewportSize(ViewportSize);

		// Take the X and Y and create values relative to the current viewport.

		FVector2D RelLocation;
		RelLocation.X = ScreenX / ViewportSize.X;
		RelLocation.Y = ScreenY / ViewportSize.Y;

		// Now, Grab a pointer to the local player and Deproject

		FVector StartLocation;
		FVector Direction;

		ULocalPlayer* Player = GEngine->GamePlayers( 0 );
		Player->DeProject(RelLocation, StartLocation, Direction);

		FVector EndLocation = StartLocation + Direction * TraceDistance;

		// Start with a quick trace to find the closest actor
		FCheckResult Hit(1.f);
		GWorld->SingleLineCheck( Hit, NULL, EndLocation, StartLocation, TRACE_World|TRACE_Actors, FVector(0.f) );
		if( Hit.Actor )
		{
			HitObject = Hit.Actor;
			HitLocation = Hit.Location;
			HitNormal = Hit.Normal;
		}
		else
		{
			HitObject = NULL;
			HitLocation = FVector(0,0,0);
			HitNormal = FVector(0,0,0);
		}
	}
}

/**
 * Handle a touch event coming from the device. 
 *
 * @param Originator		is a reference to the PC that caused the input
 * @param Handle			the id of the touch
 * @param Type				What type of event is this
 * @param TouchLocation		Where the touch occurred
 * @param DeviceTimestamp	Input event timestamp from the device
 */

void USeqEvent_MobileSwipe::InputTouch(APlayerController* Originator, UINT Handle, UINT InTouchpadIndex, BYTE Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp)
{
	// only respond to touches from the proper touchpad
	if (InTouchpadIndex != TouchpadIndex)
	{
		return;
	}

	// If this is a touch event, store the initial location, but don't activate
	if (Type == Touch_Began)
	{
		InitialTouch = TouchLocation;
		TouchedActors.Empty();
		return;
	}
	else if (Type == Touch_Ended)
	{

		INT ActiveIndex = 0;
		
		FLOAT DistX = TouchLocation.X - InitialTouch.X; 
		FLOAT aDistX = Abs(DistX);
		FLOAT DistY = TouchLocation.Y - InitialTouch.Y;
		FLOAT aDistY = Abs(DistY);

		if (aDistX >= aDistY)
		{
			if (aDistX >= MinDistance && aDistY < Tolerance)	// If we moved far enough and were within the tolerance then we have a valid swipe
			{
				// Sign denotes left or right
				ActiveIndex = DistX > 0 ? 1 : 0;
			}
			else
			{	
				return;	// Quick out without activating anything
			}
		}
		else
		{
			if (aDistY >= MinDistance && aDistX < Tolerance)	// If we moved far enough and were within the tolerance then we have a valid swipe
			{
				// Sign denotes up or down
				ActiveIndex = DistY > 0 ? 3 : 2;
			}
			else
			{	
				return;	// Quick out without activating anything
			}
		}

		TArray<INT> ActivateIndices;
		ActivateIndices.AddItem(ActiveIndex);
		CheckActivate(Originator,Originator,0,&ActivateIndices);
	}


	// In all events (except Cancel) make sure we track any actors we swipe over

	if (Type != Touch_Cancelled)
	{

		UGameViewportClient* GameViewportClient = GEngine->GameViewport;

		// We need the viewport size, so start there
		FVector2D ViewportSize;
		GameViewportClient->GetViewportSize(ViewportSize);

		// Take the X and Y and create values relative to the current viewport.

		FVector2D RelLocation;
		RelLocation.X = TouchLocation.X / ViewportSize.X;
		RelLocation.Y = TouchLocation.Y / ViewportSize.Y;

		// Now, Grab a pointer to the local player and Deproject

		FVector StartLocation;
		FVector Direction;

		ULocalPlayer* Player = GEngine->GamePlayers( 0 );
		Player->DeProject(RelLocation, StartLocation, Direction);

		FVector EndLocation = StartLocation + Direction * TraceDistance;

		// Start with a quick trace to find the closest actor
		FCheckResult Hit(1.f);
		GWorld->SingleLineCheck( Hit, NULL, EndLocation, StartLocation, TRACE_World|TRACE_Actors, FVector(0.f) );
		if( Hit.Actor != NULL)
		{
			if (TouchedActors.Num()==0 || TouchedActors.FindItemIndex(Hit.Actor) == INDEX_NONE)
			{
				TouchedActors.AddItem(Hit.Actor);
			}
		}
	}

	if (Type == Touch_Ended && TouchedActors.Num()>0)
	{
		// look at all of the variable Links and look for 
		for (INT Idx = 0; Idx < VariableLinks.Num(); Idx++)
		{
			if (VariableLinks(Idx).SupportsVariableType(USeqVar_ObjectList::StaticClass()))
			{
				// for each List that we are Linked to
				for (INT LinkIdx = 0; LinkIdx < VariableLinks(Idx).LinkedVariables.Num(); LinkIdx++)
				{
					if (VariableLinks(Idx).LinkedVariables(LinkIdx) != NULL)
					{
						// we know the Object should be an ObjectList.
						USeqVar_ObjectList* AList = Cast<USeqVar_ObjectList>((VariableLinks(Idx).LinkedVariables(LinkIdx)));
						if( AList != NULL )
						{
							AList->ObjList.Empty();
							for (INT TouchActorIdx=0; TouchActorIdx < TouchedActors.Num(); TouchActorIdx++ )
							{
								AList->ObjList.AddItem(TouchedActors(TouchActorIdx));
							}
						}
					}
				}
			}
		}
	}
}

/**
 * Handle a touch event coming from the device. 
 *
 * @param Originator		is a reference to the PC that caused the input
 * @param Handle			the id of the touch
 * @param Type				What type of event is this
 * @param TouchLocation		Where the touch occurred
 * @param DeviceTimestamp	Input event timestamp from the device
 */

void USeqEvent_MobileObjectPicker::InputTouch(APlayerController* Originator, UINT Handle, UINT InTouchpadIndex, BYTE Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp)
{
	// only respond to touches from the proper touchpad
	if (InTouchpadIndex != TouchpadIndex)
	{
		return;
	}

	UGameViewportClient* GameViewportClient = GEngine->GameViewport;
	if ((bCheckonTouch || Type == Touch_Ended) && GameViewportClient != NULL)
	{
		// Default to Fail
		INT ActiveIndex = 1;	 

		FinalTouchObject = NULL;
		FinalTouchLocation = FVector(0,0,0);
		FinalTouchNormal = FVector(0,0,0);

		TArray<UObject**> ObjVars;
		GetObjectVars(ObjVars,TEXT("Target"));
		if (ObjVars.Num() > 0)	
		{
			// We need the viewport size, so start there
			FVector2D ViewportSize;
			GameViewportClient->GetViewportSize(ViewportSize);

			// Take the X and Y and create values relative to the current viewport.

			FVector2D RelLocation;
			RelLocation.X = TouchLocation.X / ViewportSize.X;
			RelLocation.Y = TouchLocation.Y / ViewportSize.Y;

			// Now, Grab a pointer to the local player and Deproject

			FVector StartLocation;
			FVector Direction;

			ULocalPlayer* Player = GEngine->GamePlayers( 0 );
			Player->DeProject(RelLocation, StartLocation, Direction);

			FVector EndLocation = StartLocation + Direction * TraceDistance;

			// Start with a quick trace to find the closest actor
			FCheckResult Hit(1.f);
			GWorld->SingleLineCheck( Hit, NULL, EndLocation, StartLocation, TRACE_World|TRACE_Actors, FVector(0.f) );
			if( Hit.Actor != NULL)
			{
				AActor *Actor = NULL;
				for (INT VarIdx = 0; VarIdx < ObjVars.Num() && Actor == NULL; VarIdx++)
				{
					Actor = Cast<AActor>(*ObjVars(VarIdx));
					if (Actor != NULL && Actor == Hit.Actor)
					{
						FinalTouchObject = Actor;
						FinalTouchLocation = Hit.Location;
						FinalTouchNormal = Hit.Normal;
						ActiveIndex = 0;
						break;
					}
				}
			}
		}
		TArray<INT> ActivateIndices;
		ActivateIndices.AddItem(ActiveIndex);
		CheckActivate(Originator,Originator,0,&ActivateIndices);
	}
}

void USeqAct_MobileAddInputZones::Activated()
{
	// nothing to do if zone isn't set up
	if (NewZone == NULL)
	{
		return;
	}

	for (FLocalPlayerIterator It(GEngine); It; ++It)
	{
		// see if it has a mobile input
		UMobilePlayerInput* MobileInput = Cast<UMobilePlayerInput>(It->Actor->PlayerInput);
		if (MobileInput)
		{
			// make sure there's enough room in the groups array
			if (MobileInput->MobileInputGroups.Num() <= MobileInput->CurrentMobileGroup)
			{
				MobileInput->MobileInputGroups.AddZeroed((MobileInput->CurrentMobileGroup - MobileInput->MobileInputGroups.Num()) + 1);
			}
			
			// copy the zone to a new one
			UMobileInputZone* ZoneCopy = DuplicateObject<UMobileInputZone>(NewZone, UObject::GetTransientPackage(), *ZoneName.ToString());
			ZoneCopy->InputOwner = MobileInput;
			MobileInput->MobileInputZones.AddItem(ZoneCopy);
			// add the zone to the front so that later added zones are on top of earlier zones
			MobileInput->MobileInputGroups(MobileInput->CurrentMobileGroup).AssociatedZones.InsertItem(ZoneCopy, 0);

			// initialize it with a NULL viewport so it'll be updated dynamically
			MobileInput->NativeInitializeZone(ZoneCopy, FVector2D(0, 0), TRUE);

			// hook up any kismet events
			MobileInput->eventRefreshKismetLinks();
		}
	}
}

void USeqAct_MobileRemoveInputZone::Activated()
{

	for (FLocalPlayerIterator It(GEngine); It; ++It)
	{
		// see if it has a mobile input
		UMobilePlayerInput* MobileInput = Cast<UMobilePlayerInput>(It->Actor->PlayerInput);
		if (MobileInput)
		{
			// remove any zone with the given name from the input groups arrays
			for (INT GroupIndex = 0; GroupIndex < MobileInput->MobileInputGroups.Num(); GroupIndex++)
			{
				for (INT ZoneIndex = 0; ZoneIndex < MobileInput->MobileInputGroups(GroupIndex).AssociatedZones.Num(); ZoneIndex++)
				{
					if (MobileInput->MobileInputGroups(GroupIndex).AssociatedZones(ZoneIndex)->GetName() == ZoneName)
					{
						MobileInput->MobileInputGroups(GroupIndex).AssociatedZones.Remove(ZoneIndex);
						ZoneIndex--;
					}
				}
			}

			// remove any zone with the given name from the master zone array
			for (INT ZoneIndex = 0; ZoneIndex < MobileInput->MobileInputZones.Num(); ZoneIndex++)
			{
				if (MobileInput->MobileInputZones(ZoneIndex)->GetName() == ZoneName)
				{
					MobileInput->MobileInputZones.Remove(ZoneIndex);
					ZoneIndex--;
				}
			}

			// update kismet
			MobileInput->eventRefreshKismetLinks();
		}
	}
}

void USeqAct_MobileClearInputZones::Activated()
{
	for (FLocalPlayerIterator It(GEngine); It; ++It)
	{
		// see if it has a mobile input
		UMobilePlayerInput* MobileInput = Cast<UMobilePlayerInput>(It->Actor->PlayerInput);
		if (MobileInput)
		{
			// remove all zones from all arrays
			for (INT GroupIndex = 0; GroupIndex < MobileInput->MobileInputGroups.Num(); GroupIndex++)
			{
				MobileInput->MobileInputGroups(GroupIndex).AssociatedZones.Empty();
			}
			MobileInput->MobileInputZones.Empty();
			MobileInput->eventRefreshKismetLinks();
		}
	}
}

/**
 * Format the given variable to a string representation
 */
static FString VariableToString(USequenceVariable* Variable)
{
	if (Variable->IsA(USeqVar_Int::StaticClass()))
	{
		return FString::Printf(TEXT("%d"), ((USeqVar_Int*)Variable)->IntValue);
	}
	else if (Variable->IsA(USeqVar_Float::StaticClass()))
	{
		return FString::Printf(TEXT("%f"), ((USeqVar_Float*)Variable)->FloatValue);
	}
	else if (Variable->IsA(USeqVar_Bool::StaticClass()))
	{
		return FString::Printf(TEXT("%d"), ((USeqVar_Bool*)Variable)->bValue);
	}
	else if (Variable->IsA(USeqVar_Vector::StaticClass()))
	{
		FVector& VectValue = ((USeqVar_Vector*)Variable)->VectValue;
		return FString::Printf(TEXT("%f,%f,%f"), VectValue.X, VectValue.Y, VectValue.Z);
	}

	return TEXT("");
}

/**
 * Set variable's value from a string (usually from VariableToString)
 */
static void StringToVariable(USequenceVariable* Variable, const TCHAR* Value)
{
	if (Variable->IsA(USeqVar_Int::StaticClass()))
	{
		((USeqVar_Int*)Variable)->IntValue = appAtoi(Value);
	}
	else if (Variable->IsA(USeqVar_Float::StaticClass()))
	{
		((USeqVar_Float*)Variable)->FloatValue = appAtof(Value);
	}
	else if (Variable->IsA(USeqVar_Bool::StaticClass()))
	{
		((USeqVar_Bool*)Variable)->bValue = appAtoi(Value);
	}
	else if (Variable->IsA(USeqVar_Vector::StaticClass()))
	{
		// split up the input
		FString ValueStr(Value);
		TArray<FString> Values;
		ValueStr.ParseIntoArray(&Values, TEXT(","), FALSE);
		if (Values.Num() == 3)
		{
			FVector& VectValue = ((USeqVar_Vector*)Variable)->VectValue;
			VectValue.X = appAtof(*Values(0));
			VectValue.Y = appAtof(*Values(1));
			VectValue.Z = appAtof(*Values(2));
		}
	}
}

void USeqAct_MobileSaveLoadValue::Activated()
{
	// check for Save input
	if (InputLinks(0).bHasImpulse)
	{
		// go over all linked variables, and convert them to the string representation
		for (INT LinkIndex = 0; LinkIndex < VariableLinks.Num(); LinkIndex++)
		{
			for (INT VarIndex = 0; VarIndex < VariableLinks(LinkIndex).LinkedVariables.Num(); VarIndex++)
			{
				USequenceVariable *Var = VariableLinks(LinkIndex).LinkedVariables(VarIndex);
				if (Var != NULL)
				{
					// send the save command via the string
					FString Command = FString::Printf(TEXT("mobile SaveSetting %s %s"), *Var->VarName.ToString(), *VariableToString(Var));
					GEngine->Exec(*Command);
				}
			}
		}
	}
	// check for Load input
	else if (InputLinks(1).bHasImpulse)
	{
		// go over all linked variables, and convert them to the string representation
		for (INT LinkIndex = 0; LinkIndex < VariableLinks.Num(); LinkIndex++)
		{
			for (INT VarIndex = 0; VarIndex < VariableLinks(LinkIndex).LinkedVariables.Num(); VarIndex++)
			{
				USequenceVariable *Var = VariableLinks(LinkIndex).LinkedVariables(VarIndex);
				if (Var != NULL)
				{
					// load the setting, but we pass in the current value as the default, in case it was never saved (we don't want to assume 0)
					FString Command = FString::Printf(TEXT("mobile LoadSetting %s %s"), *Var->VarName.ToString(), *VariableToString(Var));
					FStringOutputDevice Output;
					GEngine->Exec(*Command, Output);

					// Output now has the loaded value, so we need to jam it into the variable
					StringToVariable(Var, *Output);
				}
			}
		}
	}
}

void USeqAct_MobileSaveLoadValue::DeActivated()
{
	// check for Save input
	if (InputLinks(0).bHasImpulse)
	{
		OutputLinks(0).ActivateOutputLink();
	}
	else if (InputLinks(1).bHasImpulse)
	{
		OutputLinks(1).ActivateOutputLink();
	}
}

