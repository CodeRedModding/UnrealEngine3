/*=============================================================================
	UnSequence.cpp: Gameplay Sequence native code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

@todo UI integration tasks:
- USequence::ReferencesObject (this checks the Originator, but Originator isn't set for UI sequence ops)
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineSequenceClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "EngineMaterialClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineAnimClasses.h"
#include "EngineSoundClasses.h"
#include "EngineParticleClasses.h"

#if WITH_EDITOR
/**
 * Forward declaration for WxKismet::FindOutputLinkTo() wrapper.
 *
 * Finds all of the ops in the sequence's SequenceObjects list that contains
 * an output link to the specified op's input.
 *
 * Returns TRUE to indicate at least one output was returned.
 */
UBOOL KismetFindOutputLinkTo(const USequence *Sequence, const USequenceOp *TargetOp, const INT InputIdx, TArray<USequenceOp*> &OutputOps, TArray<INT> &OutputIndices);
#endif

// how many operations are we allowed to execute in a single frame?
#define MAX_SEQUENCE_STEPS				1000

// Priority with which to display sounds triggered by Kismet.
#define SUBTITLE_PRIORITY_KISMET		10000

// Base class declarations
IMPLEMENT_CLASS(USequence);
IMPLEMENT_CLASS(USequenceAction);
IMPLEMENT_CLASS(USequenceCondition);
IMPLEMENT_CLASS(USequenceEvent);
IMPLEMENT_CLASS(USequenceFrame);
IMPLEMENT_CLASS(USequenceFrameWrapped);
IMPLEMENT_CLASS(USequenceObject);
IMPLEMENT_CLASS(USequenceOp);
IMPLEMENT_CLASS(USequenceVariable);

// Engine level variables
IMPLEMENT_CLASS(USeqVar_Bool);
IMPLEMENT_CLASS(USeqVar_External);
IMPLEMENT_CLASS(USeqVar_Float);
IMPLEMENT_CLASS(UDEPRECATED_SeqVar_Group);
IMPLEMENT_CLASS(USeqVar_Int);
IMPLEMENT_CLASS(USeqVar_Named);
IMPLEMENT_CLASS(USeqVar_Object);
IMPLEMENT_CLASS(USeqVar_ObjectList);
IMPLEMENT_CLASS(USeqVar_ObjectVolume);
IMPLEMENT_CLASS(USeqVar_Player);
IMPLEMENT_CLASS(USeqVar_RandomFloat);
IMPLEMENT_CLASS(USeqVar_RandomInt);
IMPLEMENT_CLASS(USeqVar_String);
IMPLEMENT_CLASS(USeqVar_Vector);
IMPLEMENT_CLASS(USeqVar_Character)

// Engine level conditions.
IMPLEMENT_CLASS(USeqCond_CompareBool);
IMPLEMENT_CLASS(USeqCond_CompareFloat);
IMPLEMENT_CLASS(USeqCond_CompareInt);
IMPLEMENT_CLASS(USeqCond_CompareObject);
IMPLEMENT_CLASS(USeqCond_GetServerType);
IMPLEMENT_CLASS(USeqCond_Increment);
IMPLEMENT_CLASS(USeqCond_IncrementFloat);
IMPLEMENT_CLASS(USeqCond_IsSameTeam);
IMPLEMENT_CLASS(USeqCond_SwitchClass);
IMPLEMENT_CLASS(USeqCond_IsInCombat);
IMPLEMENT_CLASS(USeqCond_IsLoggedIn);
IMPLEMENT_CLASS(USeqCond_IsAlive);
IMPLEMENT_CLASS(USeqCond_SwitchBase);
IMPLEMENT_CLASS(USeqCond_SwitchObject);
IMPLEMENT_CLASS(USeqCond_IsConsole);
IMPLEMENT_CLASS(USeqCond_SwitchPlatform);
IMPLEMENT_CLASS(USeqCond_IsPIE);
IMPLEMENT_CLASS(USeqCond_IsBenchmarking);
IMPLEMENT_CLASS(USeqCond_MatureLanguage);
IMPLEMENT_CLASS(USeqCond_ShowGore);

// Engine level Events
IMPLEMENT_CLASS(USeqEvent_AISeeEnemy);
IMPLEMENT_CLASS(USeqEvent_AnimNotify);
IMPLEMENT_CLASS(USeqEvent_Console);
IMPLEMENT_CLASS(USeqEvent_ConstraintBroken);
IMPLEMENT_CLASS(USeqEvent_Destroyed);
IMPLEMENT_CLASS(USeqEvent_GetInventory);
IMPLEMENT_CLASS(USeqEvent_LevelLoaded);
IMPLEMENT_CLASS(UDEPRECATED_SeqEvent_LevelStartup);
IMPLEMENT_CLASS(UDEPRECATED_SeqEvent_LevelBeginning);
IMPLEMENT_CLASS(USeqEvent_Mover);
IMPLEMENT_CLASS(USeqEvent_ParticleEvent);
IMPLEMENT_CLASS(USeqEvent_ProjectileLanded);
IMPLEMENT_CLASS(USeqEvent_RemoteEvent);
IMPLEMENT_CLASS(USeqEvent_RigidBodyCollision);
IMPLEMENT_CLASS(USeqEvent_SeeDeath);
IMPLEMENT_CLASS(USeqEvent_SequenceActivated);
IMPLEMENT_CLASS(USeqEvent_TakeDamage)
IMPLEMENT_CLASS(USeqEvent_Touch);
IMPLEMENT_CLASS(USeqEvent_Used);
IMPLEMENT_CLASS(USeqEvent_Input);
IMPLEMENT_CLASS(USeqEvent_AnalogInput);
IMPLEMENT_CLASS(USeqEvent_TouchInput);


// Engine level actions
IMPLEMENT_CLASS(USeqAct_ActivateRemoteEvent);
IMPLEMENT_CLASS(USeqAct_ActorFactory);
IMPLEMENT_CLASS(USeqAct_ActorFactoryEx);
IMPLEMENT_CLASS(USeqAct_ProjectileFactory);
IMPLEMENT_CLASS(USeqAct_AIMoveToActor);
IMPLEMENT_CLASS(USeqAct_AndGate);
IMPLEMENT_CLASS(USeqAct_ApplySoundNode);
IMPLEMENT_CLASS(USeqAct_AttachToEvent);
IMPLEMENT_CLASS(USeqAct_CameraFade);
IMPLEMENT_CLASS(USeqAct_CameraLookAt);
IMPLEMENT_CLASS(USeqAct_CameraShake);
IMPLEMENT_CLASS(USeqAct_ChangeCollision);
IMPLEMENT_CLASS(USeqAct_CommitMapChange);
IMPLEMENT_CLASS(USeqAct_ConvertToString);
IMPLEMENT_CLASS(USeqAct_Delay);
IMPLEMENT_CLASS(UDEPRECATED_SeqAct_DelaySwitch);
IMPLEMENT_CLASS(USeqAct_DrawText);
IMPLEMENT_CLASS(USeqAct_FinishSequence);
IMPLEMENT_CLASS(USeqAct_ForceGarbageCollection);
IMPLEMENT_CLASS(USeqAct_Gate);
IMPLEMENT_CLASS(USeqAct_GetDistance);
IMPLEMENT_CLASS(USeqAct_GetProperty);
IMPLEMENT_CLASS(USeqAct_GetVelocity);
IMPLEMENT_CLASS(USeqAct_GetLocationAndRotation);
IMPLEMENT_CLASS(USeqAct_IsInObjectList);
IMPLEMENT_CLASS(USeqAct_Latent);
IMPLEMENT_CLASS(USeqAct_LevelStreaming);
IMPLEMENT_CLASS(USeqAct_LevelStreamingBase);
IMPLEMENT_CLASS(USeqAct_LevelVisibility);
IMPLEMENT_CLASS(USeqAct_Log);
IMPLEMENT_CLASS(USeqAct_FeatureTest);
IMPLEMENT_CLASS(USeqAct_ModifyCover);
IMPLEMENT_CLASS(USeqAct_ModifyHealth);
IMPLEMENT_CLASS(USeqAct_ModifyObjectList);
IMPLEMENT_CLASS(USeqAct_AccessObjectList);
IMPLEMENT_CLASS(USeqAct_MultiLevelStreaming);
IMPLEMENT_CLASS(USeqAct_PlayCameraAnim)
IMPLEMENT_CLASS(USeqAct_ParticleEventGenerator);
IMPLEMENT_CLASS(USeqAct_PlayFaceFXAnim);
IMPLEMENT_CLASS(USeqAct_PlaySound);
IMPLEMENT_CLASS(USeqAct_Possess);
IMPLEMENT_CLASS(USeqAct_PrepareMapChange);
IMPLEMENT_CLASS(USeqAct_RandomSwitch);
IMPLEMENT_CLASS(UDEPRECATED_SeqAct_RangeSwitch);
IMPLEMENT_CLASS(USeqAct_SetBlockRigidBody);
IMPLEMENT_CLASS(USeqAct_SetCameraTarget);
IMPLEMENT_CLASS(USeqAct_SetSequenceVariable);
IMPLEMENT_CLASS(USeqAct_SetBool);
IMPLEMENT_CLASS(USeqAct_SetDOFParams);
IMPLEMENT_CLASS(USeqAct_SetMotionBlurParams);
IMPLEMENT_CLASS(USeqAct_SetFloat);
IMPLEMENT_CLASS(USeqAct_SetInt);
IMPLEMENT_CLASS(USeqAct_SetLocation);
IMPLEMENT_CLASS(USeqAct_SetMaterial);
IMPLEMENT_CLASS(USeqAct_SetMatInstScalarParam);
IMPLEMENT_CLASS(USeqAct_SetMesh);
IMPLEMENT_CLASS(USeqAct_SetObject);
IMPLEMENT_CLASS(USeqAct_SetWorldAttractorParam);
IMPLEMENT_CLASS(USeqAct_SetPhysics);
IMPLEMENT_CLASS(USeqAct_SetRigidBodyIgnoreVehicles);
IMPLEMENT_CLASS(USeqAct_SetString);
IMPLEMENT_CLASS(USeqAct_StreamInTextures);
IMPLEMENT_CLASS(USeqAct_Switch);
IMPLEMENT_CLASS(USeqAct_Timer);
IMPLEMENT_CLASS(USeqAct_Toggle);
IMPLEMENT_CLASS(USeqAct_Trace);
IMPLEMENT_CLASS(USeqAct_WaitForLevelsVisible);
IMPLEMENT_CLASS(USeqAct_AddInt);
IMPLEMENT_CLASS(USeqAct_SubtractInt);
IMPLEMENT_CLASS(USeqAct_MultiplyInt);
IMPLEMENT_CLASS(USeqAct_DivideInt);
IMPLEMENT_CLASS(USeqAct_CastToInt);
IMPLEMENT_CLASS(USeqAct_AddFloat);
IMPLEMENT_CLASS(USeqAct_SubtractFloat);
IMPLEMENT_CLASS(USeqAct_MultiplyFloat);
IMPLEMENT_CLASS(USeqAct_DivideFloat);
IMPLEMENT_CLASS(USeqAct_CastToFloat);
IMPLEMENT_CLASS(USeqAct_GetVectorComponents);
IMPLEMENT_CLASS(USeqAct_SetVectorComponents);
IMPLEMENT_CLASS(USeqAct_SetApexClothingParam);
IMPLEMENT_CLASS(USeqAct_HeadTrackingControl);
IMPLEMENT_CLASS(USeqAct_SetActiveAnimChild);

#if !CONSOLE && WITH_EDITOR
extern UBOOL GKismetRealtimeDebugging;
#endif

//==========================
// AActor Sequence interface

/** 
 *	Get the names of any float properties of this Actor which are marked as 'interp'.
 *	Will also look in components of this Actor, and makes the name in the form 'componentname.propertyname'.
 */
void AActor::GetInterpFloatPropertyNames(TArray<FName> &OutNames)
{
	// first search for any float properties in this actor
	for (TFieldIterator<UFloatProperty> It(GetClass()); It; ++It)
	{
		if (It->PropertyFlags & CPF_Interp)
		{
			// add the property name
			OutNames.AddItem(*It->GetName());
		}
	}
	// next search for any FMatineeRawDistributionFloat properties in this actor
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		if (It->PropertyFlags & CPF_Interp && It->GetCPPType() == TEXT("struct FMatineeRawDistributionFloat"))
		{
			// add the property name
			OutNames.AddItem(*It->GetName());
		}
	}

	// Then iterate over each component of this actor looking for interp properties.
	for(TMap<FName,UComponent*>::TIterator It(GetClass()->ComponentNameToDefaultObjectMap);It;++It)
	{
		FName ComponentName = It.Key();
		UClass* ComponentClass = It.Value()->GetClass();

		for (TFieldIterator<UFloatProperty> FieldIt(ComponentClass); FieldIt; ++FieldIt)
		{
			if (FieldIt->PropertyFlags & CPF_Interp)
			{
				// add the property name, mangled to note the component
				OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"),*ComponentName.ToString(), *FieldIt->GetName())));
			}
		}
		// next search for any FMatineeRawDistributionFloat properties in this actor
		for (TFieldIterator<UStructProperty> FieldIt(ComponentClass); FieldIt; ++FieldIt)
		{
			if (FieldIt->PropertyFlags & CPF_Interp && FieldIt->GetCPPType() == TEXT("struct FMatineeRawDistributionFloat"))
			{
				// add the property name, mangled to note the component
				OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"),*ComponentName.ToString(), *FieldIt->GetName())));
			}
		}
	}

	// Iterate over structs marked as 'interp' looking for float properties within them.
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		if(It->PropertyFlags & CPF_Interp)
		{
			for(TFieldIterator<UFloatProperty> FIt(It->Struct); FIt; ++FIt)
			{
				if (FIt->PropertyFlags & CPF_Interp)
				{
					// add the property name, plus the struct name
					OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"), *It->GetName(), *FIt->GetName())));
				}
			}
			for (TFieldIterator<UStructProperty> FIt(It->Struct); FIt; ++FIt)
			{
				if (FIt->PropertyFlags & CPF_Interp && FIt->GetCPPType() == TEXT("struct FMatineeRawDistributionFloat"))
				{
					// add the property name, plus the struct name
					OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"), *It->GetName(), *FIt->GetName())));
				}
			}

			// Find 'interp' floats in nested 'interp' structs.
			for (TFieldIterator<UStructProperty> SIt(It->Struct); SIt; ++SIt)
			{
				if ( SIt->PropertyFlags & CPF_Interp )
				{
					for (TFieldIterator<UFloatProperty> FIt(SIt->Struct); FIt; ++FIt)
					{
						if (FIt->PropertyFlags & CPF_Interp)
						{
							// add the property name, plus the struct name, and the parent struct name
							OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s.%s"), *It->GetName(), *SIt->GetName(), *FIt->GetName())));
						}
					}
					for (TFieldIterator<UStructProperty> FIt(It->Struct); FIt; ++FIt)
					{
						if (FIt->PropertyFlags & CPF_Interp && FIt->GetCPPType() == TEXT("struct FMatineeRawDistributionFloat"))
						{
							// add the property name, plus the struct name
							OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s.%s"), *It->GetName(), *SIt->GetName(), *FIt->GetName())));
						}
					}
				}
			}

		}
	}
}

/** 
 *	Get the names of any boolean properties of this Actor which are marked as 'interp'.
 *	Will also look in components of this Actor, and makes the name in the form 'componentname.propertyname'.
 * 
 * @param	OutNames	The names of all the boolean properties marked as 'interp'.
 */
void AActor::GetInterpBoolPropertyNames( TArray<FName>& OutNames )
{
	// first search for any bool properties in this actor
	for( TFieldIterator<UBoolProperty> It(GetClass()); It; ++It )
	{
		if( It->PropertyFlags & CPF_Interp )
		{
			// add the property name
			OutNames.AddItem(*It->GetName());
		}
	}

	// Then iterate over each component of this actor looking for interp properties.
	for( TMap<FName,UComponent*>::TIterator It(GetClass()->ComponentNameToDefaultObjectMap); It; ++It )
	{
		FName ComponentName = It.Key();
		UClass* ComponentClass = It.Value()->GetClass();

		for( TFieldIterator<UBoolProperty> FieldIt(ComponentClass); FieldIt; ++FieldIt )
		{
			if( FieldIt->PropertyFlags & CPF_Interp )
			{
				// add the property name, mangled to note the component
				OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"),*ComponentName.ToString(), *FieldIt->GetName())));
			}
		}
	}

	// Iterate over structs marked as 'interp' looking for bool properties within them.
	for( TFieldIterator<UStructProperty> It(GetClass()); It; ++It )
	{
		if( It->PropertyFlags & CPF_Interp )
		{
			for( TFieldIterator<UBoolProperty> FIt(It->Struct); FIt; ++FIt )
			{
				if( FIt->PropertyFlags & CPF_Interp )
				{
					// add the property name, plus the struct name
					OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"), *It->GetName(), *FIt->GetName())));
				}
			}
		}
	}
}

/** 
 *	Get the names of any vector properties of this Actor which are marked as 'interp'.
 *	Will also look in components of this Actor, and makes the name in the form 'componentname.propertyname'.
 */
void AActor::GetInterpVectorPropertyNames(TArray<FName> &OutNames)
{
	// first search for any vector properties in this actor
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		if ((It->PropertyFlags & CPF_Interp) && It->Struct->GetFName() == NAME_Vector)
		{
			// add the property name
			OutNames.AddItem(*It->GetName());
		}
	}

	// Then iterate over each component of this actor looking for interp properties.
	for(TMap<FName,UComponent*>::TIterator It(GetClass()->ComponentNameToDefaultObjectMap);It;++It)
	{
		FName ComponentName = It.Key();
		UClass* ComponentClass = It.Value()->GetClass();

		for (TFieldIterator<UStructProperty> FieldIt(ComponentClass); FieldIt; ++FieldIt)
		{
			if ((FieldIt->PropertyFlags & CPF_Interp) && FieldIt->Struct->GetFName() == NAME_Vector)
			{
				// add the property name, mangled to note the component
				OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"),*ComponentName.ToString(), *FieldIt->GetName())));
			}
		}
	}

	// Iterate over structs marked as 'interp' looking for vector properties within them.
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		if(It->PropertyFlags & CPF_Interp)
		{
			for (TFieldIterator<UStructProperty> VIt(It->Struct); VIt; ++VIt)
			{
				if ((VIt->PropertyFlags & CPF_Interp) && VIt->Struct->GetFName() == NAME_Vector)
				{
					// add the property name, plus the struct name
					OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"), *It->GetName(), *VIt->GetName())));
				}
			}
		}
	}
}


/** 
 *	Get the names of any color properties of this Actor which are marked as 'interp'.
 *	Will also look in components of this Actor, and makes the name in the form 'componentname.propertyname'.
 */
void AActor::GetInterpColorPropertyNames(TArray<FName> &OutNames)
{
	// first search for any vector properties in this actor
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		if ((It->PropertyFlags & CPF_Interp) && It->Struct->GetFName() == NAME_Color)
		{
			// add the property name
			OutNames.AddItem(*It->GetName());
		}
	}

	// Then iterate over each component of this actor looking for interp properties.
	for(TMap<FName,UComponent*>::TIterator It(GetClass()->ComponentNameToDefaultObjectMap);It;++It)
	{
		FName ComponentName = It.Key();
		UClass* ComponentClass = It.Value()->GetClass();

		for (TFieldIterator<UStructProperty> FieldIt(ComponentClass); FieldIt; ++FieldIt)
		{
			if ((FieldIt->PropertyFlags & CPF_Interp) && FieldIt->Struct->GetFName() == NAME_Color)
			{
				// add the property name, mangled to note the component
				OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"),*ComponentName.ToString(), *FieldIt->GetName())));
			}
		}
	}

	// Iterate over structs marked as 'interp' looking for color properties within them.
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		if(It->PropertyFlags & CPF_Interp)
		{
			for (TFieldIterator<UStructProperty> VIt(It->Struct); VIt; ++VIt)
			{
				if ((VIt->PropertyFlags & CPF_Interp) && VIt->Struct->GetFName() == NAME_Color)
				{
					// add the property name, plus the struct name
					OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"), *It->GetName(), *VIt->GetName())));
				}
			}
		}
	}
}


/** 
*	Get the names of any linear color properties of this Actor which are marked as 'interp'.
*	Will also look in components of this Actor, and makes the name in the form 'componentname.propertyname'.
*/
void AActor::GetInterpLinearColorPropertyNames(TArray<FName> &OutNames)
{
	// first search for any vector properties in this actor
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		if ((It->PropertyFlags & CPF_Interp) && It->Struct->GetFName() == NAME_LinearColor)
		{
			// add the property name
			OutNames.AddItem(*It->GetName());
		}
	}

	// Then iterate over each component of this actor looking for interp properties.
	for(TMap<FName,UComponent*>::TIterator It(GetClass()->ComponentNameToDefaultObjectMap);It;++It)
	{
		FName ComponentName = It.Key();
		UClass* ComponentClass = It.Value()->GetClass();

		for (TFieldIterator<UStructProperty> FieldIt(ComponentClass); FieldIt; ++FieldIt)
		{
			if ((FieldIt->PropertyFlags & CPF_Interp) && FieldIt->Struct->GetFName() == NAME_LinearColor)
			{
				// add the property name, mangled to note the component
				OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"),*ComponentName.ToString(), *FieldIt->GetName())));
			}
		}
	}

	// Iterate over structs marked as 'interp' looking for color properties within them.
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		if(It->PropertyFlags & CPF_Interp)
		{
			for (TFieldIterator<UStructProperty> VIt(It->Struct); VIt; ++VIt)
			{
				if ((VIt->PropertyFlags & CPF_Interp) && VIt->Struct->GetFName() == NAME_LinearColor)
				{
					// add the property name, plus the struct name
					OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"), *It->GetName(), *VIt->GetName())));
				}
			}

			// Find 'interp' floats in nested 'interp' structs.
			for (TFieldIterator<UStructProperty> SIt(It->Struct); SIt; ++SIt)
			{
				if ( SIt->PropertyFlags & CPF_Interp )
				{
					for (TFieldIterator<UStructProperty> VIt(SIt->Struct); VIt; ++VIt)
					{
						if ((VIt->PropertyFlags & CPF_Interp) && VIt->Struct->GetFName() == NAME_LinearColor)
						{
							// add the property name, plus the struct name, and the parent struct name
							OutNames.AddItem(FName(*FString::Printf(TEXT("%s.%s.%s"), *It->GetName(), *SIt->GetName(), *VIt->GetName())));
						}
					}
				}
			}
		}
	}
}




// Check property is either a float, vector, linear color or color.
static UBOOL PropertyIsFloatBoolVectorColorOrLinearColor(UProperty* Prop)
{
	if(Prop->IsA(UFloatProperty::StaticClass()))
	{
		return TRUE;
	}
	else if( Prop->IsA(UBoolProperty::StaticClass()) )
	{
		return TRUE;
	}
	else if(Prop->IsA(UStructProperty::StaticClass()))
	{
		UStructProperty* StructProp = (UStructProperty*)Prop;
		FName StructType = StructProp->Struct->GetFName();
		if(StructType == NAME_Vector || StructType == NAME_Color || StructType == NAME_LinearColor)
		{
			return TRUE;
		}
	}

	return FALSE;
}


/** 
 *	This utility for returning the Object and an offset within it for the given property name.
 *	If the name contains no period, we assume the property is in InObject, so basically just look up the offset.
 *	But if it does contain a period, we first see if the first part is a struct property.
 *	If not we see if it is a component name and return component pointer instead of actor pointer.
 */
static UObject* FindObjectAndPropOffset(INT& OutPropOffset, BITFIELD& OutMask, AActor* InActor, FName InPropName)
{
	OutMask = 0;
	FString CompString, PropString;
	if(InPropName.ToString().Split(TEXT("."), &CompString, &PropString))
	{
		// STRUCT
		// First look for a struct with first part of name.
		UStructProperty* StructProp = FindField<UStructProperty>( InActor->GetClass(), *CompString );
		if(StructProp)
		{
			UProperty* Prop = FindField<UProperty>( StructProp->Struct, *PropString );
			if(Prop && PropertyIsFloatBoolVectorColorOrLinearColor(Prop))
			{
				OutPropOffset = StructProp->Offset + Prop->Offset;
				UBoolProperty* BoolProperty = Cast<UBoolProperty>(Prop);
				if (BoolProperty)
				{
					OutMask = BoolProperty->BitMask;
				}
				return InActor;
			}
			else
			{
				// Look for one more level of nested structs.
				FString CompString2, PropString2;
				if (PropString.Split(TEXT("."), &CompString2, &PropString2))
				{
					UStructProperty* StructProp2 = FindField<UStructProperty>( StructProp->Struct, *CompString2 );
					if(StructProp2)
					{
						UProperty* Prop = FindField<UProperty>( StructProp2->Struct, *PropString2 );
						if(Prop && PropertyIsFloatBoolVectorColorOrLinearColor(Prop))
						{
							OutPropOffset = StructProp->Offset + StructProp2->Offset + Prop->Offset;
							UBoolProperty* BoolProperty = Cast<UBoolProperty>(Prop);
							if (BoolProperty)
							{
								OutMask = BoolProperty->BitMask;
							}
							return InActor;
						}
					}
				}

				return NULL;
			}
		}

		// COMPONENT
		// If no struct property with that name, search for a matching component using name->component mapping table.
		FName CompName(*CompString);
		FName PropName(*PropString);
		UObject* OutObject = NULL;

		TArray<UComponent*> Components;
		InActor->CollectComponents(Components,FALSE);

		for ( INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++ )
		{
			UComponent* Component = Components(ComponentIndex);
			if ( Component->GetInstanceMapName() == CompName )
			{
				OutObject = Component;
				break;
			}
		}

		// If we found a component - look for the named property within it.
		if(OutObject)
		{
			UProperty* Prop = FindField<UProperty>( OutObject->GetClass(), *PropName.ToString() );
			if(Prop && PropertyIsFloatBoolVectorColorOrLinearColor(Prop))
			{
				OutPropOffset = Prop->Offset;
				UBoolProperty* BoolProperty = Cast<UBoolProperty>(Prop);
				if (BoolProperty)
				{
					OutMask = BoolProperty->BitMask;
				}
				return OutObject;
			}
			// No property found- just return NULL;
			else
			{
				return NULL;
			}
		}
		// No component with that name found - return NULL
		return NULL;
	}
	// No dot in name - just look for property in this actor.
	else
	{
		UProperty* Prop = FindField<UProperty>( InActor->GetClass(), *InPropName.ToString() );
		if(Prop && PropertyIsFloatBoolVectorColorOrLinearColor(Prop))
		{
			OutPropOffset = Prop->Offset;
			UBoolProperty* BoolProperty = Cast<UBoolProperty>(Prop);
			if (BoolProperty)
			{
				OutMask = BoolProperty->BitMask;
			}
			return InActor;
		}
		else
		{
			// Check the actor's components, too
			for( INT ComponentIndex = 0; ComponentIndex < InActor->Components.Num(); ++ComponentIndex )
			{
				UObject* OutObject = InActor->Components( ComponentIndex );
				if( OutObject != NULL )
				{
					UProperty* Prop = FindField<UProperty>( OutObject->GetClass(), *InPropName.ToString() );
					if(Prop && PropertyIsFloatBoolVectorColorOrLinearColor(Prop))
					{
						OutPropOffset = Prop->Offset;
						UBoolProperty* BoolProperty = Cast<UBoolProperty>(Prop);
						if (BoolProperty)
						{
							OutMask = BoolProperty->BitMask;
						}
						return OutObject;
					}
				}
			}

			return NULL;
		}
	}
}


/** 
 *	This utility for returning the Object and an offset within it for the given property name when it is inside
 *  a container such as a rawdistribution struct.
 *	If the name contains no period, we assume the property is in InObject, so basically just look up the offset.
 *	But if it does contain a period, we first see if the first part is a struct property.
 *	If not we see if it is a component name and return component pointer instead of actor pointer.
 */
static FPointer FindObjectAndPropOffsetInContainer(INT& OutPropOffset, AActor* InActor, FName InPropName)
{
	UStructProperty* StructProp = FindField<UStructProperty>( InActor->GetClass(), *InPropName.ToString() );
	if(StructProp)
	{
		UProperty* Prop = FindField<UProperty>( StructProp->Struct, TEXT("MatineeValue"));


		if(Prop && PropertyIsFloatBoolVectorColorOrLinearColor(Prop))
		{
			OutPropOffset = Prop->Offset;
			return (FPointer)(((BYTE*)InActor) + StructProp->Offset);
		}
	}

	return NULL;
}

/* epic ===============================================
* ::GetInterpFloatPropertyRef
*
* Looks up the matching float property and returns a
* reference to the actual value.
*
* =====================================================
*/
FLOAT* AActor::GetInterpFloatPropertyRef(FName InPropName, FPointer &outContainer)
{
	// get the property name and the Object its in. handles property being in a component.
	INT PropOffset;
	BITFIELD PropertyMask;
	UObject* PropObject = FindObjectAndPropOffset(PropOffset, PropertyMask, this, InPropName);
	if (PropObject == NULL)
	{
		FPointer Container = FindObjectAndPropOffsetInContainer(PropOffset, this, InPropName);

		if(Container != NULL)
		{
			outContainer = Container;
			return ((FLOAT*)(((BYTE*)Container) + PropOffset));
		}
	}
	else
	{
		return ((FLOAT*)(((BYTE*)PropObject) + PropOffset));
	}
	return NULL;
}

/**
 * Looks up the matching boolean property and returns a reference to the actual value.
 * 
 * @param   InName  The name of boolean property to retrieve a reference.
 * @return  A pointer to the actual value; NULL if the property was not found.
 */
BITFIELD* AActor::GetInterpBoolPropertyRef( FName InPropName, BITFIELD& Mask )
{
	// get the property name and the Object its in. handles property being in a component.
	INT PropOffset;
	UObject* PropObject = FindObjectAndPropOffset( PropOffset, Mask, this, InPropName );

	if( PropObject )
	{
		// Since booleans are handled as bitfields in the engine, 
		// we have to return a bitfield pointer instead. 
		return ((BITFIELD*)(((BYTE*)PropObject) + PropOffset));
	}

	return NULL;
}

/* epic ===============================================
* ::GetInterpVectorPropertyRef
*
* Looks up the matching vector property and returns a
* reference to the actual value.
*
* =====================================================
*/
FVector* AActor::GetInterpVectorPropertyRef(FName InPropName)
{
	// get the property name and the Object its in. handles property being in a component.
	INT PropOffset;
	BITFIELD PropertyMask;
	UObject* PropObject = FindObjectAndPropOffset(PropOffset, PropertyMask, this, InPropName);
	if (PropObject != NULL)
	{
		return ((FVector*)(((BYTE*)PropObject) + PropOffset));
	}
	return NULL;
}

/* epic ===============================================
 * ::GetInterpColorPropertyRef
 *
 * Looks up the matching color property and returns a
 * reference to the actual value.
 *
 * =====================================================
 */
FColor* AActor::GetInterpColorPropertyRef(FName InPropName)
{
	// get the property name and the Object its in. handles property being in a component.
	INT PropOffset;
	BITFIELD PropertyMask;
	UObject* PropObject = FindObjectAndPropOffset(PropOffset, PropertyMask, this, InPropName);
	if (PropObject != NULL)
	{
		return ((FColor*)(((BYTE*)PropObject) + PropOffset));
	}
	return NULL;
}


/* ===============================================
 * ::GetInterpColorPropertyRef
 *
 * Looks up the matching color property and returns a
 * reference to the actual value.
 *
 * =====================================================
 */
FLinearColor* AActor::GetInterpLinearColorPropertyRef(FName InPropName)
{
	// get the property name and the Object its in. handles property being in a component.
	INT PropOffset;
	BITFIELD PropertyMask;
	UObject* PropObject = FindObjectAndPropOffset(PropOffset, PropertyMask, this, InPropName);
	if (PropObject != NULL)
	{
		return ((FLinearColor*)(((BYTE*)PropObject) + PropOffset));
	}
	return NULL;
}


/** Notification that this Actor has been renamed. Used so we can rename any SequenceEvents that ref to us. */
void AActor::PostRename()
{
	// Only do this check outside gameplay
	if(GWorld && !GWorld->HasBegunPlay())
	{
		// If we have a Kismet Sequence for this actor's level..
		if(GWorld->GetGameSequence())
		{
			// Find all SequenceEvents (will recurse into subSequences)
			TArray<USequenceObject*> SeqObjs;
			GWorld->GetGameSequence()->FindSeqObjectsByClass( USequenceEvent::StaticClass(), SeqObjs );

			// Then see if any of them refer to this Actor.
			for(INT i=0; i<SeqObjs.Num(); i++)
			{
				USequenceEvent* Event = CastChecked<USequenceEvent>( SeqObjs(i) );

				if(Event->Originator == this)
				{
					USequenceEvent* DefEvent = Event->GetArchetype<USequenceEvent>();
					Event->ObjName = FString::Printf( TEXT("%s %s"), *GetName(), *DefEvent->ObjName );
				}
			}
		}
	}
}

/* ==========================================================================================================
	FSeqVarLink
========================================================================================================== */
/**
 * Determines whether this variable link can be associated with the specified sequence variable class.
 *
 * @param	SequenceVariableClass	the class to check for compatibility with this variable link; must be a child of SequenceVariable
 * @param	bRequireExactClass		if FALSE, child classes of the specified class return a match as well.
 *
 * @return	TRUE if this variable link can be linked to the a SequenceVariable of the specified type.
 */
UBOOL FSeqVarLink::SupportsVariableType( UClass* SequenceVariableClass, UBOOL bRequireExactClass/*=TRUE*/ ) const
{
	if (bAllowAnyType)
	{
		return TRUE;
	}
	else if (ExpectedType != NULL && ExpectedType->IsChildOf(USequenceVariable::StaticClass()))
	{
		if( bRequireExactClass )
		{
			return (SequenceVariableClass == ExpectedType ||
					(SequenceVariableClass == USeqVar_Object::StaticClass() && ExpectedType == USeqVar_Vector::StaticClass()));
		}
		return (SequenceVariableClass == ExpectedType ||
				SequenceVariableClass->IsChildOf(ExpectedType) ||
				SequenceVariableClass->IsChildOf(USeqVar_Object::StaticClass()) && ExpectedType == USeqVar_Vector::StaticClass() );
	}
	return FALSE;
}

/* ==========================================================================================================
	FSeqOpOutputInputLink
========================================================================================================== */
/** native serialization operator */
FArchive& operator<<( FArchive& Ar, FSeqOpOutputInputLink& OutputInputLink )
{
	return Ar << OutputInputLink.LinkedOp << OutputInputLink.InputLinkIdx;
}

/** Comparison operator */
UBOOL FSeqOpOutputInputLink::operator==( const FSeqOpOutputInputLink& Other ) const
{
	return	LinkedOp == Other.LinkedOp && InputLinkIdx == Other.InputLinkIdx;
}
UBOOL FSeqOpOutputInputLink::operator!=( const FSeqOpOutputInputLink& Other ) const
{
	return	LinkedOp != Other.LinkedOp || InputLinkIdx != Other.InputLinkIdx;
}

//==========================
// USequenceOp interface

UBOOL USequenceObject::IsPendingKill() const
{
	// check to see if the parentsequence is pending kill as well as this object
	return (Super::IsPendingKill() || (ParentSequence != NULL && ParentSequence->IsPendingKill()));
}

void USequenceObject::CleanUp()
{
#if WITH_EDITORONLY_DATA
	// Remove the reference to the editor version of this object (used for Kismet debugging in PIE)
	if ( GIsEditor )
	{
		USequenceObject* KismetObject = FindKismetObject();
		if(KismetObject)
		{
			KismetObject->PIESequenceObject = NULL;
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * Builds a list of objects which have this object in their archetype chain.
 *
 * All archetype propagation for sequence objects would be handled by prefab code, so this version just skips the iteration.
 *
 * @param	Instances	receives the list of objects which have this one in their archetype chain
 */
void USequenceObject::GetArchetypeInstances( TArray<UObject*>& Instances )
{
	//@todo - we might want to wrap this with a check for UsesManagedArchetypePropagation and return default behavior otherwise
	if ( HasAnyFlags(RF_ClassDefaultObject) )
	{
		Super::GetArchetypeInstances(Instances);
	}
}

/**
 * Serializes all objects which have this object as their archetype into GMemoryArchive, then recursively calls this function
 * on each of those objects until the full list has been processed.
 * Called when a property value is about to be modified in an archetype object.
 *
 * Since archetype propagation for sequence objects is handled by the prefab code, this version simply routes the call
 * to the owning prefab so that it can handle the propagation at the appropriate time.
 *
 * @param	AffectedObjects		ignored
 */
void USequenceObject::SaveInstancesIntoPropagationArchive( TArray<UObject*>& AffectedObjects )
{
	UObject* OwnerPrefab = NULL;
	if ( IsAPrefabArchetype(&OwnerPrefab) )
	{
		checkSlow(OwnerPrefab);
		OwnerPrefab->SaveInstancesIntoPropagationArchive(AffectedObjects);
	}

	// otherwise, we just swallow this call - SequenceObjects should never execute the UObject version
}

/**
 * De-serializes all objects which have this object as their archetype from the GMemoryArchive, then recursively calls this function
 * on each of those objects until the full list has been processed.
 *
 * Since archetype propagation for sequence objects is handled by the prefab code, this version simply routes the call
 * to the owning prefab so that it can handle the propagation at the appropriate time.
 *
 * @param	AffectedObjects		the array of objects which have this object in their ObjectArchetype chain and will be affected by the change.
 *								Objects which have this object as their direct ObjectArchetype are removed from the list once they're processed.
 */
void USequenceObject::LoadInstancesFromPropagationArchive( TArray<UObject*>& AffectedObjects )
{
	UObject* OwnerPrefab = NULL;
	if ( IsAPrefabArchetype(&OwnerPrefab) )
	{
		checkSlow(OwnerPrefab);
		OwnerPrefab->LoadInstancesFromPropagationArchive(AffectedObjects);
	}

	// otherwise, we just swallow this call - SequenceObjects should never execute the UObject version
}

/**
 * Determines whether this object is contained within a UPrefab.
 *
 * @param	OwnerPrefab		if specified, receives a pointer to the owning prefab.
 *
 * @return	TRUE if this object is contained within a UPrefab; FALSE if it IS a UPrefab or not contained within one.
 */
UBOOL USequenceObject::IsAPrefabArchetype( UObject** OwnerPrefab/*=NULL*/ ) const
{
	UBOOL bResult = FALSE;

	USequence* SequenceProxy = ParentSequence;
	if ( SequenceProxy == NULL )
	{
		for ( UObject* NextOuter = GetOuter(); NextOuter; NextOuter = NextOuter->GetOuter() )
		{
			SequenceProxy = Cast<USequence>(NextOuter);
			if ( SequenceProxy != NULL )
			{
				break;
			}
		}
	}

	if ( SequenceProxy != NULL )
	{
		bResult = SequenceProxy->IsAPrefabArchetype(OwnerPrefab);
	}
	else
	{
		bResult = Super::IsAPrefabArchetype(OwnerPrefab);
	}

	return bResult;
}

/**
 * @return	TRUE if the object is a UPrefabInstance or part of a prefab instance.
 */
UBOOL USequenceObject::IsInPrefabInstance() const
{
	UBOOL bResult = FALSE;

	USequence* SequenceProxy = ParentSequence;
	if ( SequenceProxy == NULL )
	{
		for ( UObject* NextOuter = GetOuter(); NextOuter; NextOuter = NextOuter->GetOuter() )
		{
			SequenceProxy = Cast<USequence>(NextOuter);
			if ( SequenceProxy != NULL )
			{
				break;
			}
		}
	}

	if ( SequenceProxy != NULL )
	{
		bResult = SequenceProxy->IsInPrefabInstance();
	}
	else
	{
		bResult = Super::IsInPrefabInstance();
	}

	return bResult;
}

/**
 * Looks for the outer Sequence and redirects output to it's log
 * file.
 */
void USequenceObject::ScriptLog(const FString &LogText, UBOOL bWarning)
{
	if (!bWarning)
	{
		KISMET_LOG(*LogText);
	}
	else
	{
		KISMET_WARN(*LogText);
	}
}

/* Script hook to GWorld, since the Sequence may need it e.g. to spawn actors */
AWorldInfo* USequenceObject::GetWorldInfo()
{
	return GWorld ? GWorld->GetWorldInfo() : NULL;
}

/**
 * Called once this Object is exported to a package, used to clean
 * up any actor refs, etc.
 */
void USequenceObject::OnExport()
{
}

/** Construct full path name of this Sequence Object, by traversing up through parent Sequences until we get to the root Sequence. */
FString USequenceObject::GetSeqObjFullName()
{
	FString SeqTitle = GetName();
	USequence *ParentSeq = ParentSequence;
	while (ParentSeq != NULL)
	{
		SeqTitle = FString::Printf(TEXT("%s.%s"), *ParentSeq->GetName(), *SeqTitle);
		ParentSeq = ParentSeq->ParentSequence;
	}

	return SeqTitle;
}

/** Similar to GetSeqObjFullName() except the name of the level is used in place of "Main_Sequence" */
FString USequenceObject::GetSeqObjFullLevelName()
{
	FString SeqTitle = GetName();
	USequence* ParentSeq = ParentSequence;
	while (ParentSeq != NULL)
	{
		FString ParentSeqName = ParentSeq->GetName();
		ParentSeqName = ParentSeqName.Replace(TEXT("Main_Sequence"), *ParentSeq->GetOutermost()->GetName(), TRUE);
		ParentSeqName = ParentSeqName.Replace(PLAYWORLD_PACKAGE_PREFIX, TEXT(""), TRUE);
		SeqTitle = FString::Printf(TEXT("%s.%s"), *ParentSeqName, *SeqTitle);
		ParentSeq = ParentSeq->ParentSequence;
	}

	return SeqTitle;
}

/**
 * Traverses the outer chain until a non-sequence object is found, starting with this object.
 *
 * @erturn	a pointer to the first object (including this one) in the Outer chain that does
 *			not have another sequence object as its Outer.
 */
USequence* USequenceObject::GetRootSequence( UBOOL bOuterFallback/*=FALSE*/ )
{
	USequence* RootSeq = GetParentSequenceRoot();
	if ( RootSeq == NULL )
	{
		if ( bOuterFallback )
		{
			for ( UObject* NextOuter = this; NextOuter; NextOuter = NextOuter->GetOuter() )
			{
				USequence* OuterSequence = Cast<USequence>(NextOuter);
				if ( OuterSequence == NULL )
				{
					break;
				}
				else
				{
					RootSeq = OuterSequence;
				}
			}
		}
		else
		{
			RootSeq = Cast<USequence>(this);
		}
	}

	checkf(RootSeq, TEXT("No root sequence for %s, %s"),*GetFullName(),ParentSequence != NULL ? *ParentSequence->GetFullName() : TEXT("NO PARENT"));
	return RootSeq;
}
/**
 * Traverses the outer chain until a non-sequence object is found, starting with this object.
 *
 * @erturn	a pointer to the first object (including this one) in the Outer chain that does
 *			not have another sequence object as its Outer.
 */
const USequence* USequenceObject::GetRootSequence( UBOOL bOuterFallback/*=FALSE*/ ) const
{
	const USequence* RootSeq = GetParentSequenceRoot();
	if ( RootSeq == NULL )
	{
		if ( bOuterFallback )
		{
			for ( const UObject* NextOuter = this; NextOuter; NextOuter = NextOuter->GetOuter() )
			{
				const USequence* OuterSequence = ConstCast<USequence>(NextOuter);
				if ( OuterSequence == NULL )
				{
					break;
				}
				else
				{
					RootSeq = OuterSequence;
				}
			}
		}
		else
		{
			RootSeq = ConstCast<USequence>(this);
		}
	}

	checkf(RootSeq, TEXT("No root sequence for %s, %s"),*GetFullName(),ParentSequence != NULL ? *ParentSequence->GetFullName() : TEXT("NO PARENT"));
	return RootSeq;
}

/**
 * Traverses the outer chain until a non-sequence object is found, starting with this object's ParentSequence.
 *
 * @erturn	a pointer to the first object (not including this one) in the Outer chain that does
 *			not have another sequence object as its Outer.
 */
USequence* USequenceObject::GetParentSequenceRoot( UBOOL bOuterFallback/*=FALSE*/ ) const
{
	USequence* RootSeq = NULL;
	if ( ParentSequence != NULL )
	{
		RootSeq = ParentSequence->GetParentSequenceRoot(bOuterFallback);
		if ( RootSeq == NULL )
		{
			RootSeq = ParentSequence;
		}
	}
	else
	{
		for ( UObject* NextOuter = GetOuter(); NextOuter; NextOuter = NextOuter->GetOuter() )
		{
			USequence* OuterSequence = Cast<USequence>(NextOuter);
			if ( OuterSequence == NULL )
			{
				break;
			}
			else
			{
				RootSeq = OuterSequence;
			}
		}
	}

	return RootSeq;
}


/**
 * Finds all incoming links to this object and connects them to NewSeqObj
 * Used in ConvertObject()
 */
void USequenceOp::ConvertObjectInternal(USequenceObject* NewSeqObj, INT LinkIdx)
{
	USequenceOp* NewSeqOp = Cast<USequenceOp>(NewSeqObj);
	// iterate through all other objects, looking for output links that point to this op
	if ( ParentSequence && NewSeqOp )
	{
		for ( INT chkIdx = 0; chkIdx < ParentSequence->SequenceObjects.Num(); ++chkIdx )
		{
			// if it is a sequence op,
			USequenceOp *ChkOp = Cast<USequenceOp>(ParentSequence->SequenceObjects(chkIdx));
			if ( ChkOp != NULL && ChkOp != this )
			{
				// iterate through this op's output links
				for ( INT linkIdx = 0; linkIdx < ChkOp->OutputLinks.Num(); ++linkIdx )
				{
					// iterate through all the inputs linked to this output
					for ( INT inputIdx = 0; inputIdx < ChkOp->OutputLinks(linkIdx).Links.Num(); ++inputIdx )
					{
						if ( ChkOp->OutputLinks(linkIdx).Links(inputIdx).LinkedOp == this )
						{
							ChkOp->Modify(TRUE);

							// relink the entry
							ChkOp->OutputLinks(linkIdx).Links(inputIdx).LinkedOp = NewSeqOp;
							if ( LinkIdx >= 0 )
							{
								ChkOp->OutputLinks(linkIdx).Links(inputIdx).InputLinkIdx = LinkIdx;
							}
						}
					}
				}
			}
		}
	}
}

void USequenceOp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UpdateDynamicLinks();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Handles updating this sequence op when the ObjClassVersion doesn't match the ObjInstanceVersion, indicating that the op's
 * default values have been changed.
 */
void USequenceOp::UpdateObject()
{
	Modify();
	// grab the default op, that we'll be updating to
	USequenceOp *DefOp = GetArchetype<USequenceOp>();

	// create a duplicate of this object, for reference when updating
	USequenceOp *DupOp = ConstructObject<USequenceOp>(GetClass(),INVALID_OBJECT,NAME_None,0,this);

	// update all our links
	InputLinks = DefOp->InputLinks;
	OutputLinks = DefOp->OutputLinks;
	VariableLinks = DefOp->VariableLinks;
	EventLinks = DefOp->EventLinks;

	// get a list of ops linked to this one
	TArray<FSeqOpOutputLink*> Links;
	ParentSequence->FindLinksToSeqOp(this,Links,DupOp);

	// build any dynamic links
	UpdateDynamicLinks();
	// now try to re-link everything
	// input links first
	{
		// update the links to point at the new idx, or remove if no match is available
		while (Links.Num() > 0)
		{
			FSeqOpOutputLink *Link = Links.Pop();
			for (INT Idx = 0; Idx < Link->Links.Num(); Idx++)
			{
				FSeqOpOutputInputLink &InputLink = Link->Links(Idx);
				if (InputLink.LinkedOp == this)
				{
					// try to fix up the input link idx
					UBOOL bFoundMatch = FALSE;
					if (InputLink.InputLinkIdx >= 0 && InputLink.InputLinkIdx < DupOp->InputLinks.Num())
					{
						// search for the matching input on the new op
						for (INT InputIdx = 0; InputIdx < InputLinks.Num(); InputIdx++)
						{
							if (InputLinks(InputIdx).LinkDesc == DupOp->InputLinks(InputLink.InputLinkIdx).LinkDesc)
							{
								// adjust the idx to match the one in the new op
								InputLink.InputLinkIdx = InputIdx;
								if( InputLink.InputLinkIdx < DupOp->InputLinks.Num() )
								{
									InputLinks(InputIdx).ActivateDelay =	DupOp->InputLinks(InputLink.InputLinkIdx).ActivateDelay;
									InputLinks(InputIdx).bDisabled =		DupOp->InputLinks(InputLink.InputLinkIdx).bDisabled;
									InputLinks(InputIdx).bDisabledPIE =		DupOp->InputLinks(InputLink.InputLinkIdx).bDisabledPIE;
								}
								bFoundMatch = TRUE;
								break;
							}
						}
					}
					// if no match was found,
					if (!bFoundMatch)
					{
						// remove the link
						Link->Links.Remove(Idx--,1);
					}
				}
			}
		}
	}
	// output links
	{
		for (INT OutputIdx = 0; OutputIdx < DupOp->OutputLinks.Num(); OutputIdx++)
		{
			FSeqOpOutputLink &Link = DupOp->OutputLinks(OutputIdx);
			if (Link.Links.Num() > 0)
			{
				// look for a matching link in the new op
				for (INT Idx = 0; Idx < OutputLinks.Num(); Idx++)
				{
					if (OutputLinks(Idx).LinkDesc == Link.LinkDesc)
					{
						// copy over the links
						OutputLinks(Idx).Links = Link.Links;
						OutputLinks(Idx).bHidden = FALSE;
						OutputLinks(Idx).ActivateDelay = Link.ActivateDelay;
						OutputLinks(Idx).bDisabled = Link.bDisabled;
						OutputLinks(Idx).bDisabledPIE = Link.bDisabledPIE;
						break;
					}
				}
			}
		}
	}
	// variable links
	{
		for (INT VarIdx = 0; VarIdx < DupOp->VariableLinks.Num(); VarIdx++)
		{
			FSeqVarLink &Link = DupOp->VariableLinks(VarIdx);
			if (Link.LinkedVariables.Num() > 0)
			{
				UBOOL bFoundLink = FALSE;
				// look for a matching link
				for (INT Idx = 0; Idx < VariableLinks.Num(); Idx++)
				{
					if( VariableLinks(Idx).LinkDesc == Link.LinkDesc ||
						(Link.PropertyName != NAME_None && VariableLinks(Idx).PropertyName == Link.PropertyName) )
					{
						VariableLinks(Idx).LinkedVariables = Link.LinkedVariables;
						VariableLinks(Idx).bHidden = FALSE;
						bFoundLink = TRUE;
						break;
					}
				}
				// if no link was found
				if (!bFoundLink)
				{
					// check for a property to expose
					UProperty *Property = FindField<UProperty>(GetClass(),*Link.PropertyName.ToString());
					if (Property == NULL ||
						!(Property->PropertyFlags & CPF_Edit))
					{
						// try looking for one based on the link desc
						Property = FindField<UProperty>(GetClass(),*Link.LinkDesc);
					}
					if (Property != NULL &&
						Property->PropertyFlags & CPF_Edit)
					{
						// look for the first variable type to support this property type
						TArray<UClass*> VariableClasses;
						//@fixme - this is retarded!!!!  need a better way to figure out the possible classes
						for (TObjectIterator<UClass> It; It; ++It)
						{
							if (It->IsChildOf(USequenceVariable::StaticClass()))
							{
								VariableClasses.AddItem(*It);
							}
						}
						// find the first variable type that supports this link
						for (INT LinkIdx = 0; LinkIdx < VariableClasses.Num(); LinkIdx++)
						{
							USequenceVariable *DefaultVar = VariableClasses(LinkIdx)->GetDefaultObject<USequenceVariable>();
							if (DefaultVar != NULL && DefaultVar->SupportsProperty(Property))
							{
								INT Idx = VariableLinks.AddZeroed();
								FSeqVarLink &VarLink = VariableLinks(Idx);
								VarLink.LinkDesc = *Property->GetName();
								VarLink.PropertyName = Property->GetFName();
								VarLink.ExpectedType = VariableClasses(LinkIdx);
								VarLink.MinVars = 0;
								VarLink.MaxVars = 255;
								VarLink.LinkedVariables = Link.LinkedVariables;
								break;
							}
						}
					}
				}
			}
		}
	}
	// event links
	{
		for (INT EvtIdx = 0; EvtIdx < DupOp->EventLinks.Num(); EvtIdx++)
		{
			FSeqEventLink &Link = DupOp->EventLinks(EvtIdx);
			if (Link.LinkedEvents.Num() > 0)
			{
				for (INT Idx = 0; Idx < EventLinks.Num(); Idx++)
				{
					if (EventLinks(Idx).LinkDesc == Link.LinkDesc)
					{
						EventLinks(Idx).LinkedEvents = Link.LinkedEvents;
						break;
					}
				}
			}
		}
	}

	if ( GIsGame )
	{
		//@Fixme - remove this
		DupOp->MarkPendingKill();
	}

	// give script a crack at making any changes
	eventVersionUpdated(ObjInstanceVersion, eventGetObjClassVersion());

	// normal update, sets the instance version
	Super::UpdateObject();
}

void USequenceOp::InitializeLinkedVariableValues()
{
	// if this event has sequence variables attached to the "Player Index" variable links, copy the value of PlayerIndex to those variables now
	TArray<INT*> IntVars;
	GetIntVars(IntVars, TEXT("Player Index"));
	for ( INT Idx = 0; Idx < IntVars.Num(); Idx++ )
	{
		*(IntVars(Idx)) = PlayerIndex;
	}

	INT GamepadId = UUIInteraction::GetPlayerControllerId(PlayerIndex);

	// if this event has sequence variables attached to the "Gamepad Id" variable link, copy the value of the ControllerId to those variables now
	IntVars.Empty();
	GetIntVars(IntVars, TEXT("Gamepad Id"));
	for ( INT Idx = 0; Idx < IntVars.Num(); Idx++ )
	{
		*(IntVars(Idx)) = GamepadId;
	}
}

#if WITH_EDITOR
void USequenceOp::CheckForErrors()
{
	// validate variable Links
	USequenceOp *DefaultOp = GetArchetype<USequenceOp>();
	checkSlow(DefaultOp);

	for ( INT Idx = 0; Idx < VariableLinks.Num(); Idx++ )
	{
		FSeqVarLink& VarLink = VariableLinks(Idx);
		for ( INT VarIdx = VarLink.LinkedVariables.Num() - 1; VarIdx >= 0; VarIdx-- )
		{
			if ( VarLink.LinkedVariables(VarIdx) == NULL )
			{
				Modify();
				VarLink.LinkedVariables.Remove(VarIdx);
			}
		}
		
		// check for property links that aren't setup correctly
		if (VarLink.PropertyName == NAME_None
		&&	Idx < DefaultOp->VariableLinks.Num()
		&&	VarLink.LinkDesc == DefaultOp->VariableLinks(Idx).LinkDesc
		&&	DefaultOp->VariableLinks(Idx).PropertyName != NAME_None )
		{
			Modify();
			VarLink.PropertyName = DefaultOp->VariableLinks(Idx).PropertyName;
		}
	}

	// validate output Links
	for (INT Idx = 0; Idx < OutputLinks.Num(); Idx++)
	{
		FSeqOpOutputLink& OutputLink = OutputLinks(Idx);
		for ( INT LinkIdx = OutputLink.Links.Num() - 1; LinkIdx >= 0; LinkIdx-- )
		{
			if ( OutputLink.Links(LinkIdx).LinkedOp == NULL )
			{
				Modify();
				OutputLink.Links.Remove(LinkIdx);
			}
		}
	}

	if (GWarn != NULL && GWarn->MapCheck_IsActive())
	{
		for (INT i = 0; i < VariableLinks.Num(); i++)
		{
			const FSeqVarLink& VarLink = VariableLinks(i);
			UProperty* Prop = NULL;
			if (VarLink.PropertyName != NAME_None)
			{
				Prop = FindField<UProperty>(GetClass(), VarLink.PropertyName);
			}

			for (INT j = 0; j < VarLink.LinkedVariables.Num(); j++)
			{
				USeqVar_Object* ObjectVar = Cast<USeqVar_Object>(VarLink.LinkedVariables(j));
				if (VarLink.bModifiesLinkedObject && ObjectVar != NULL)
				{
					AActor* TestActor = Cast<AActor>(ObjectVar->ObjValue);
					FString Reason;
					if (TestActor != NULL && !TestActor->SupportsKismetModification(this, Reason))
					{
						GWarn->MapCheck_Add( MCTYPE_WARNING, TestActor, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KismetFailToModify" ), *GetPathName(), *Reason ) ), TEXT( "KismetFailToModify" ), MCGROUP_KISMET );
					}
				}

				// Check if a variable is linked to a deprecated property
				USequenceVariable* SeqVar = Cast<USequenceVariable>(VarLink.LinkedVariables(j));
				if (SeqVar != NULL && Prop != NULL && Prop->PropertyFlags & CPF_Deprecated)
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KismetDeprecatedPropLinkedVar" ), *GetPathName(), *VarLink.PropertyName.ToString(), *ObjectVar->GetPathName() ) ), TEXT( "KismetDeprecatedPropLinkedVar" ), MCGROUP_KISMET );
				}
			}
			// make sure the static actor isn't "hardcoded" into the property directly
			if (VarLink.bModifiesLinkedObject && Prop != NULL)
			{
				TArray<UObject*> ObjArray;
				{
					FArchiveObjectReferenceCollector Ar(&ObjArray);
					Prop->SerializeItem(Ar, (BYTE*)this + Prop->Offset);
				}
				for (INT i = 0; i < ObjArray.Num(); i++)
				{
					AActor* TestActor = Cast<AActor>(ObjArray(i));
					FString Reason;
					if (TestActor != NULL && !TestActor->SupportsKismetModification(this, Reason))
					{
						GWarn->MapCheck_Add( MCTYPE_WARNING, TestActor, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KismetFailToModify" ), *GetPathName(), *Reason ) ), TEXT( "KismetFailToModify" ), MCGROUP_KISMET );
					}
				}
			}
		}
	}
}
#endif

/**
 *	Ensure that any Output, Variable or Event connectors only Point to Objects with the same Outer (ie are in the same Sequence).
 *	Also remove NULL Links, or Links to inputs that do not exist.
 */
void USequenceOp::CleanupConnections()
{
	// first check output logic Links
	for(INT Idx = 0; Idx < OutputLinks.Num(); Idx++)
	{
		for(INT LinkIdx = 0; LinkIdx < OutputLinks(Idx).Links.Num(); LinkIdx++)
		{
			USequenceOp* SeqOp = OutputLinks(Idx).Links(LinkIdx).LinkedOp;
			INT InputIndex = OutputLinks(Idx).Links(LinkIdx).InputLinkIdx;

			if(	!SeqOp || SeqOp->GetOuter() != GetOuter() || InputIndex >= SeqOp->InputLinks.Num() )
			{
				Modify();
				OutputLinks(Idx).Links.Remove(LinkIdx--,1);
			}
		}
	}

	// next check variables
	for (INT Idx = 0; Idx < VariableLinks.Num(); Idx++)
	{
		for (INT VarIdx = 0; VarIdx < VariableLinks(Idx).LinkedVariables.Num(); VarIdx++)
		{
			USequenceVariable* SeqVar = VariableLinks(Idx).LinkedVariables(VarIdx);
			if(	!SeqVar || SeqVar->GetOuter() != GetOuter())
			{
				Modify();
				VariableLinks(Idx).LinkedVariables.Remove(VarIdx--,1);
			}
		}
	}

	// Events
	for(INT Idx = 0; Idx < EventLinks.Num(); Idx++)
	{
		for(INT evtIdx = 0; evtIdx < EventLinks(Idx).LinkedEvents.Num(); evtIdx++)
		{
			USequenceEvent* SeqEvent = EventLinks(Idx).LinkedEvents(evtIdx);
			if( !SeqEvent || SeqEvent->GetOuter() != GetOuter())
			{
				Modify();
				EventLinks(Idx).LinkedEvents.Remove(evtIdx--,1);
			}
		}
	}
}

//@todo: templatize these helper funcs
void USequenceOp::GetBoolVars(TArray<UBOOL*> &outBools, const TCHAR *inDesc) const
{
	GetOpVars<UBOOL,USeqVar_Bool>(outBools,inDesc);
}

void USequenceOp::GetIntVars(TArray<INT*> &outInts, const TCHAR *inDesc) const
{
	GetOpVars<INT,USeqVar_Int>(outInts,inDesc);
}

void USequenceOp::GetFloatVars(TArray<FLOAT*> &outFloats, const TCHAR *inDesc) const
{
	GetOpVars<FLOAT,USeqVar_Float>(outFloats,inDesc);
}

void USequenceOp::GetVectorVars(TArray<FVector*> &outVectors, const TCHAR *inDesc) const
{
	GetOpVars<FVector,USeqVar_Vector>(outVectors,inDesc);
	GetOpVars<FVector,USeqVar_Object>(outVectors,inDesc);
}

void USequenceOp::GetStringVars(TArray<FString*> &outStrings, const TCHAR *inDesc) const
{
	GetOpVars<FString,USeqVar_String>(outStrings,inDesc);
}

void USequenceOp::GetObjectVars(TArray<UObject**>& OutObjects, const TCHAR* InDesc) const
{
	// search for all variables of the expected type
	for (INT Idx = 0; Idx < VariableLinks.Num(); Idx++)
	{
		FSeqVarLink const& VarLink = VariableLinks(Idx);

		// if no desc requested, or matches requested desc
		if ( InDesc == NULL || *InDesc == 0 || VarLink.LinkDesc == InDesc )
		{
			// do the case where the Variable is an Object list first.
			// ObjectList is derived from the SeqVar_Object so that the rest of kismet can just Link
			// up to them and not know the difference.  So we must do this case separately
			if ( VarLink.SupportsVariableType(USeqVar_ObjectList::StaticClass()) )
			{
				for (INT LinkIdx = 0; LinkIdx < VarLink.LinkedVariables.Num(); LinkIdx++)
				{
					if (VarLink.LinkedVariables(LinkIdx) != NULL)
					{
						// we know that this should be an Object list
						USeqVar_ObjectList* TheList = Cast<USeqVar_ObjectList>((VarLink.LinkedVariables(LinkIdx)));

						if( TheList != NULL )
						{
							// so we need to get all of the Objects out and put them into the
							// outObjects to be returned to the caller
							for( INT ObjToAddIdx = 0; ObjToAddIdx < TheList->ObjList.Num(); ObjToAddIdx++ )
							{
								UObject** ObjectRef = TheList->GetObjectRef(ObjToAddIdx); // get the indiv Object from the list
								if (ObjectRef != NULL)
								{
									OutObjects.AddItem(ObjectRef);
								}
							}
						}
					}
				}
			}

			// if correct type
			else if ( VarLink.SupportsVariableType(USeqVar_Object::StaticClass(),FALSE) )
			{
				// add the refs to out list
				for (INT LinkIdx = 0; LinkIdx < VarLink.LinkedVariables.Num(); LinkIdx++)
				{
					if (VarLink.LinkedVariables(LinkIdx) != NULL)
					{
						for( INT RefCount = 0;; RefCount++ )
						{
							UObject** ObjectRef = VarLink.LinkedVariables(LinkIdx)->GetObjectRef( RefCount );
							if( ObjectRef != NULL )
							{
								OutObjects.AddItem(ObjectRef);
							}
							else
							{
								break;
							}
						}
					}
				}
			}
		}
	}
}

void USequenceOp::GetInterpDataVars(TArray<UInterpData*> &outIData, const TCHAR *inDesc)
{
	// search for all variables of the expected type
	for (INT Idx = 0; Idx < VariableLinks.Num(); Idx++)
	{
		// if correct type, and
		// no desc requested, or matches requested desc
		if (VariableLinks(Idx).SupportsVariableType(UInterpData::StaticClass()) &&
			(inDesc == NULL ||
			VariableLinks(Idx).LinkDesc == inDesc))
		{
			// add the refs to out list
			for (INT LinkIdx = 0; LinkIdx < VariableLinks(Idx).LinkedVariables.Num(); LinkIdx++)
			{
				if (VariableLinks(Idx).LinkedVariables(LinkIdx) != NULL)
				{
					UInterpData* IData = Cast<UInterpData>(VariableLinks(Idx).LinkedVariables(LinkIdx));
					if (IData != NULL)
					{
						outIData.AddItem(IData);
					}
				}
			}
		}
	}
}

void USequenceOp::execGetObjectVars(FFrame &Stack,RESULT_DECL)
{
	P_GET_TARRAY_REF(UObject*,outObjVars);
	P_GET_STR_OPTX(inDesc,TEXT(""));
	P_FINISH;
	TArray<UObject**> ObjVars;
	GetObjectVars(ObjVars,inDesc != TEXT("") ? *inDesc : NULL);
	if (ObjVars.Num() > 0)
	{
		for (INT Idx = 0; Idx < ObjVars.Num(); Idx++)
		{
			outObjVars.AddItem( *ObjVars(Idx) );
		}
	}
}

void USequenceOp::execGetBoolVars(FFrame &Stack,RESULT_DECL)
{
	P_GET_TARRAY_REF(BYTE,outBoolVars);
	P_GET_STR_OPTX(inDesc,TEXT(""));
	P_FINISH;
	TArray<UBOOL*> BoolVars;
	
	GetBoolVars(BoolVars,inDesc != TEXT("") ? *inDesc : NULL);
	if( BoolVars.Num() > 0 )
	{
		for( INT Idx = 0; Idx < BoolVars.Num(); Idx++ )
		{
			outBoolVars.AddItem( *BoolVars(Idx) ? 1 : 0 );
		}
	}
}

void USequenceOp::execGetInterpDataVars(FFrame &Stack,RESULT_DECL)
{
	P_GET_TARRAY_REF(UInterpData*,outIData);
	P_GET_STR_OPTX(inDesc,TEXT(""));
	P_FINISH;

	TArray<UInterpData*> IDataVars;
	GetInterpDataVars(IDataVars,inDesc != TEXT("") ? *inDesc : NULL);
	outIData = IDataVars;
}

void USequenceOp::execLinkedVariables(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UClass, VarClass);
	P_GET_OBJECT_REF(USequenceVariable, OutVariable);
	P_GET_STR_OPTX(InDesc, TEXT(""));
	P_FINISH;

	if (VarClass == NULL)
	{
		Stack.Logf(NAME_ScriptWarning, TEXT("VarClass of None passed into SequenceOp::LinkedVariables()"));
		SKIP_ITERATOR;
	}
	else
	{
		INT LinkIndex = 0;
		INT VariableIndex = 0;
		
		PRE_ITERATOR;
			// get the next SequenceVariable in the iteration
			OutVariable = NULL;
			while (LinkIndex < VariableLinks.Num() && OutVariable == NULL)
			{
				if (VariableLinks(LinkIndex).LinkDesc == InDesc || InDesc == TEXT(""))
				{
					while (VariableIndex < VariableLinks(LinkIndex).LinkedVariables.Num() && OutVariable == NULL)
					{
						USequenceVariable* TestVar = VariableLinks(LinkIndex).LinkedVariables(VariableIndex);
						if (TestVar != NULL && TestVar->IsA(VarClass))
						{
							OutVariable = TestVar;
						}
						VariableIndex++;
					}
					if (OutVariable == NULL)
					{
						LinkIndex++;
						VariableIndex = 0;
					}
				}
				else
				{
					LinkIndex++;
				}
			}
			if (OutVariable == NULL)
			{
				EXIT_ITERATOR;
				break;
			}
		POST_ITERATOR;
	}
}

/**
 *	Activates an output link by index 
 *	@param OutputIdx output index to set impulse on (if it's not disabled)
 */
UBOOL USequenceOp::ActivateOutputLink( INT OutputIdx )
{
	if( OutputIdx >= 0 && OutputIdx < OutputLinks.Num() )
	{
		return OutputLinks(OutputIdx).ActivateOutputLink();
	}
	return FALSE;
}

/**
 * Activates an output link by searching for the one with a matching LinkDesc.
 *
 * @param	LinkDesc	the string used as the value for LinkDesc of the output link to activate.
 *
 * @return	TRUE if the link was found and activated.
 */
UBOOL USequenceOp::ActivateNamedOutputLink( const FString& LinkDesc )
{
	UBOOL bResult = FALSE;

	for ( INT OutputIdx = 0; OutputIdx < OutputLinks.Num(); OutputIdx++ )
	{
		const FSeqOpOutputLink& Link = OutputLinks(OutputIdx);
		if ( Link.LinkDesc == LinkDesc )
		{
			bResult = ActivateOutputLink(OutputIdx);
			break;
		}
	}

	return bResult;
}

/**
 * Determines whether this sequence op is linked to any other sequence ops through its variable, output, event or (optionally)
 * its input links.
 *
 * @param	bConsiderInputLinks		specify TRUE to check this sequence ops InputLinks array for linked ops as well
 *
 * @return	TRUE if this sequence op is linked to at least one other sequence op.
 */
UBOOL USequenceOp::HasLinkedOps( UBOOL bConsiderInputLinks/*=FALSE*/ ) const
{
	UBOOL bResult = FALSE;

	// check all OutputLinks
	for ( INT LinkIndex = 0; !bResult && LinkIndex < OutputLinks.Num(); LinkIndex++ )
	{
		const FSeqOpOutputLink& OutputLink = OutputLinks(LinkIndex);
		for ( INT OpIndex = 0; OpIndex < OutputLink.Links.Num(); OpIndex++ )
		{
			if ( OutputLink.Links(OpIndex).LinkedOp != NULL )
			{
				bResult = TRUE;
				break;
			}
		}
	}

	// check VariableLinks
	for ( INT LinkIndex = 0; !bResult && LinkIndex < VariableLinks.Num(); LinkIndex++ )
	{
		const FSeqVarLink& VarLink = VariableLinks(LinkIndex);
		for ( INT OpIndex = 0; OpIndex < VarLink.LinkedVariables.Num(); OpIndex++ )
		{
			if ( VarLink.LinkedVariables(OpIndex) != NULL )
			{
				bResult = TRUE;
				break;
			}
		}
	}

	// check the EventLinks
	for ( INT LinkIndex = 0; !bResult && LinkIndex < EventLinks.Num(); LinkIndex++ )
	{
		const FSeqEventLink& EventLink = EventLinks(LinkIndex);
		for ( INT OpIndex = 0; OpIndex < EventLink.LinkedEvents.Num(); OpIndex++ )
		{
			if ( EventLink.LinkedEvents(OpIndex) != NULL )
			{
				bResult = TRUE;
				break;
			}
		}
	}

	// check InputLinks, if desired
	if ( !bResult && bConsiderInputLinks )
	{
		for ( INT LinkIndex = 0; LinkIndex < InputLinks.Num(); LinkIndex++ )
		{
			const FSeqOpInputLink& InputLink = InputLinks(LinkIndex);
			if ( InputLink.LinkedOp != NULL )
			{
				bResult = TRUE;
				break;
			}
		}
	}

	return bResult;
}

INT USequenceOp::CurrentSearchTag = 0;

/**
 * Gets all SequenceObjects that are linked to this SequenceObject.
 *
 * @param	out_Objects		will be filled with all ops that are linked to this op via
 *							the VariableLinks, OutputLinks, or InputLinks arrays. This array is NOT cleared first.
 * @param	ObjectType		if specified, only objects of this class (or derived) will
 *							be added to the output array.
 * @param	bRecurse		if TRUE, recurse into linked ops and add their linked ops to
 *							the output array, recursively.
 */
void USequenceOp::GetLinkedObjects(TArray<USequenceObject*>& out_Objects, UClass* ObjectType, UBOOL bRecurse)
{
	CurrentSearchTag++;
	GetLinkedObjectsInternal(out_Objects, ObjectType, bRecurse);
}
void USequenceOp::GetLinkedObjectsInternal(TArray<USequenceObject*>& out_Objects, UClass* ObjectType, UBOOL bRecurse)
{
	// add all ops referenced via the OutputLinks array
	for ( INT LinkIndex = 0; LinkIndex < OutputLinks.Num(); LinkIndex++ )
	{
		FSeqOpOutputLink& OutputLink = OutputLinks(LinkIndex);
		for ( INT OpIndex = 0; OpIndex < OutputLink.Links.Num(); OpIndex++ )
		{
			FSeqOpOutputInputLink& ConnectedLink = OutputLink.Links(OpIndex);
			if (ConnectedLink.LinkedOp != NULL && ConnectedLink.LinkedOp->SearchTag != CurrentSearchTag)
			{
				ConnectedLink.LinkedOp->SearchTag = CurrentSearchTag;
				if ( ObjectType == NULL || ConnectedLink.LinkedOp->IsA(ObjectType) )
				{
					out_Objects.AddItem(ConnectedLink.LinkedOp);
				}

				if ( bRecurse )
				{
					ConnectedLink.LinkedOp->GetLinkedObjectsInternal(out_Objects, ObjectType, bRecurse);
				}
			}
		}
	}

	// add all ops referenced via the VariableLinks array
	for ( INT LinkIndex = 0; LinkIndex < VariableLinks.Num(); LinkIndex++ )
	{
		FSeqVarLink& VarLink = VariableLinks(LinkIndex);
		for ( INT OpIndex = 0; OpIndex < VarLink.LinkedVariables.Num(); OpIndex++ )
		{
			USequenceVariable* ConnectedVar = VarLink.LinkedVariables(OpIndex);
			if (ConnectedVar != NULL)
			{
				if ((ObjectType == NULL || ConnectedVar->IsA(ObjectType)))
				{
					out_Objects.AddUniqueItem(ConnectedVar);
				}
			}
		}
	}

	// add all ops referenced via the EventLinks array
	for ( INT LinkIndex = 0; LinkIndex < EventLinks.Num(); LinkIndex++ )
	{
		FSeqEventLink& EventLink = EventLinks(LinkIndex);
		for ( INT OpIndex = 0; OpIndex < EventLink.LinkedEvents.Num(); OpIndex++ )
		{
			USequenceEvent* LinkedEvent = EventLink.LinkedEvents(OpIndex);
			if (LinkedEvent != NULL && LinkedEvent->SearchTag != CurrentSearchTag)
			{
				LinkedEvent->SearchTag = CurrentSearchTag;
				if ( ObjectType == NULL || LinkedEvent->IsA(ObjectType) )
				{
					out_Objects.AddItem(LinkedEvent);
				}

				if ( bRecurse )
				{
					LinkedEvent->GetLinkedObjectsInternal(out_Objects, ObjectType, bRecurse);
				}
			}
		}
	}
}

/**
 * Notification that an input link on this sequence op has been given impulse by another op.  Propagates the value of
 * PlayerIndex from the ActivatorOp to this one.
 *
 * @param	ActivatorOp		the sequence op that applied impulse to this op's input link
 * @param	InputLinkIndex	the index [into this op's InputLinks array] for the input link that was given impulse
 */
void USequenceOp::OnReceivedImpulse( USequenceOp* ActivatorOp, INT InputLinkIndex )
{
	if ( ActivatorOp != NULL )
	{
#if !CONSOLE && WITH_EDITOR
		if( GIsPlayInEditorWorld && GKismetRealtimeDebugging )
		{
			LastActivatedInputLink = InputLinkIndex;
			ActivatorSeqOp = ActivatorOp;
		}
#endif
		// propagate the PlayerIndex that generated this sequence execution
		PlayerIndex = ActivatorOp->PlayerIndex;
		GamepadID = ActivatorOp->GamepadID;
	}
}



/** Called after the object is loaded. */
void USequenceOp::PostLoad()
{
	// Call parent implementation
	Super::PostLoad();

	// It's kind of expensive to perform linear searches looking for stacked link errors, so we'll only perform
	// these tests when loading maps into the editor
#if !FINAL_RELEASE
	if( GIsEditor )
	{
		// Check for and remove redundant 'stacked links' between a port and variable
		for( INT CurPortIndex = 0; CurPortIndex < VariableLinks.Num(); ++CurPortIndex )
		{
			FSeqVarLink& CurPort = VariableLinks( CurPortIndex );


			// Keep track of which objects are already linked, so we can ignore duplicates!
			// @todo Performance: Use a hash set here?
			TArray< UObject** > ObjectsAlreadyLinked;

			// Keep track of duplicate (stacked) links between this port and an object
			TArray< UObject** > StackedObjectLinks;


			// Check for an 'object list' variable type first
			if( CurPort.SupportsVariableType( USeqVar_ObjectList::StaticClass() ) )
			{
				for( INT CurLinkIdx = 0; CurLinkIdx < CurPort.LinkedVariables.Num(); ++CurLinkIdx )
				{
					if( CurPort.LinkedVariables( CurLinkIdx ) != NULL )
					{
						USeqVar_ObjectList* TheList = Cast< USeqVar_ObjectList >( CurPort.LinkedVariables( CurLinkIdx ) );
						if( TheList != NULL )
						{
							for( INT CurObjectIndex = 0; CurObjectIndex < TheList->ObjList.Num(); ++CurObjectIndex )
							{
								UObject** ObjectRef = TheList->GetObjectRef( CurObjectIndex );
								if( ObjectRef != NULL )
								{
									if( ObjectsAlreadyLinked.ContainsItem( ObjectRef ) )
									{
										// Add to list of problematic object links
										StackedObjectLinks.AddUniqueItem( ObjectRef );

										// Repair this error by removing the redundant link
										TheList->ObjList.Remove( CurObjectIndex );

										// Mark the package as dirty so that it will be resaved
										MarkPackageDirty();

										--CurObjectIndex;
									}
									else
									{
										ObjectsAlreadyLinked.AddItem( ObjectRef );
									}
								}
							}
						}
					}
				}
			}

			// OK, now check for single object links
			else if( CurPort.SupportsVariableType( USeqVar_Object::StaticClass(), FALSE ) )
			{
				for( INT CurLinkIndex = 0; CurLinkIndex < CurPort.LinkedVariables.Num(); ++CurLinkIndex )
				{
					USequenceVariable* CurLink = CurPort.LinkedVariables( CurLinkIndex );

					if( CurLink != NULL )
					{
						for( INT RefCount = 0; ; ++RefCount )
						{
							UObject** ObjectRef = CurLink->GetObjectRef( RefCount );
							if( ObjectRef != NULL )
							{
								if( ObjectsAlreadyLinked.ContainsItem( ObjectRef ) )
								{
									// Add to list of problematic object links
									StackedObjectLinks.AddUniqueItem( ObjectRef );

									// Currently we only know how to repair links to single objects
									if( RefCount == 0 && CurLink->GetObjectRef( 1 ) == NULL )
									{
										// Repair this error by removing the redundant link
										CurPort.LinkedVariables.Remove( CurLinkIndex );
										--CurLinkIndex;

										// Mark the package as dirty so that it will be resaved
										MarkPackageDirty();

										break;
									}
									else
									{
										// Unable to repair this link (because we just don't know how!)
									}
								}
								else
								{
									ObjectsAlreadyLinked.AddItem( ObjectRef );
								}
							}
							else
							{
								break;
							}
						}
					}
				}
			}

			for( INT CurWarningIndex = 0; CurWarningIndex < StackedObjectLinks.Num(); ++CurWarningIndex )
			{
				UObject* CurObject = *StackedObjectLinks( CurWarningIndex );

				debugf(
					NAME_Warning,
					TEXT( "Detected and REPAIRED identical overlapping links between port [%s] and object [%s] for sequence [%s]." ),
					*CurPort.LinkDesc,
					*CurObject->GetName(),
					*GetSeqObjFullName() );
			}
		}
	}
#endif
}



/**
 * Called from parent sequence via ExecuteActiveOps, return
 * TRUE to indicate this action has completed.
 */
UBOOL USequenceOp::UpdateOp(FLOAT DeltaTime)
{
	// dummy stub, immediately finish
	return 1;
}

/**
 * Called once this op has been activated, override in
 * subclasses to provide custom behavior.
 */
void USequenceOp::Activated()
{
	// Editor-only, Kismet visual debugging
#if !CONSOLE && WITH_EDITOR
	if(GIsPlayInEditorWorld && GKismetRealtimeDebugging)
	{
		if (bIsBreakpointSet || bIsHiddenBreakpointSet)
		{
			// Breakpoint triggered, notify the editor engine
			TCHAR TempCommandStr[512];
			appSprintf(TempCommandStr, TEXT("OPENKISMETDEBUGGER SEQUENCE=%s"), *GetSeqObjFullLevelName());
			GEngine->Exec(TempCommandStr);
			GWorld->GetWorldInfo()->bDebugPauseExecution = TRUE;
		}
		
		// Set the state of this node
		bIsActivated = TRUE;
	}
#endif
}

/**
 * Called once this op has been deactivated, default behavior
 * activates all output links.
 */
void USequenceOp::DeActivated()
{
	if ( bAutoActivateOutputLinks )
	{
		// activate all output impulses
		for (INT LinkIdx = 0; LinkIdx < OutputLinks.Num(); LinkIdx++)
		{
			OutputLinks(LinkIdx).ActivateOutputLink();
		}
	}
}

/**
 * Copies the values from all VariableLinks to the member variable [of this sequence op] associated with that VariableLink.
 */
void USequenceOp::PublishLinkedVariableValues()
{
	for (INT LinkIndex = 0; LinkIndex < VariableLinks.Num(); LinkIndex++)
	{
		FSeqVarLink &VarLink = VariableLinks(LinkIndex);

		if( ( VarLink.PropertyName != NAME_None )
			&& ( VarLink.LinkedVariables.Num() > 0 )
			&& ( VarLink.bSequenceNeverReadsOnlyWritesToThisVar == FALSE ) // if false we do read from this varlink so go ahead an populate member vars
			)
		{
			// find the property on this object
			if (VarLink.CachedProperty == NULL)
			{
				VarLink.CachedProperty = FindField<UProperty>(GetClass(),VarLink.PropertyName);
			}
			UProperty *Property = VarLink.CachedProperty;
			if (Property != NULL)
			{
				// find the first valid variable
				USequenceVariable *Var = NULL;
				for (INT Idx = 0; Idx < VarLink.LinkedVariables.Num(); Idx++)
				{
					if ( VarLink.LinkedVariables(Idx) != NULL )
					{
						Var = VarLink.LinkedVariables(Idx);
						break;
					}
				}

				if (Var != NULL)
				{
					// apply the variables
					Var->PublishValue(this,Property,VarLink);
				}
			}
		}
	}
}

/**
 * Copies the values from member variables contained by this sequence op into any VariableLinks associated with that member variable.
 */
void USequenceOp::PopulateLinkedVariableValues()
{
	for (INT LinkIndex = 0; LinkIndex < VariableLinks.Num(); LinkIndex++)
	{
		FSeqVarLink& VarLink = VariableLinks(LinkIndex);
		if ( VarLink.LinkedVariables.Num() > 0 )
		{
			if ( VarLink.PropertyName != NAME_None )
			{
				// find the property on this object
				if (VarLink.CachedProperty == NULL)
				{
					VarLink.CachedProperty = FindField<UProperty>(GetClass(),VarLink.PropertyName);
				}
				UProperty* Property = VarLink.CachedProperty;
				if (Property != NULL)
				{
					// find the first valid variable
					USequenceVariable* Var = NULL;
					for (INT Idx = 0; Idx < VarLink.LinkedVariables.Num(); Idx++)
					{
						if ( VarLink.LinkedVariables(Idx) != NULL )
						{
							Var = VarLink.LinkedVariables(Idx);
							break;
						}
					}

					if (Var != NULL)
					{
						// apply the variables
						Var->PopulateValue(this,Property,VarLink);
					}
				}
			}

			for ( INT VarIndex = 0; VarIndex < VarLink.LinkedVariables.Num(); VarIndex++ )
			{
				USequenceVariable* LinkedVariable = VarLink.LinkedVariables(VarIndex);
				if ( LinkedVariable != NULL )
				{
					LinkedVariable->PostPopulateValue(this, VarLink);
				}
			}
		}
	}
}

/* epic ===============================================
* ::FindConnectorIndex
*
* Utility for finding if a connector of the given type and with the given name exists.
* Returns its index if so, or INDEX_NONE otherwise.
*
* =====================================================
*/
INT USequenceOp::FindConnectorIndex(const FString& ConnName, INT ConnType)
{
	if(ConnType == LOC_INPUT)
	{
		for(INT i=0; i<InputLinks.Num(); i++)
		{
			if( InputLinks(i).LinkDesc == ConnName)
				return i;
		}
	}
	else if(ConnType == LOC_OUTPUT)
	{
		for(INT i=0; i<OutputLinks.Num(); i++)
		{
			if( OutputLinks(i).LinkDesc == ConnName)
				return i;
		}
	}
	else if(ConnType == LOC_VARIABLE)
	{
		for(INT i=0; i<VariableLinks.Num(); i++)
		{
			if( VariableLinks(i).LinkDesc == ConnName)
				return i;
		}
	}
	else if (ConnType == LOC_EVENT)
	{
		for (INT Idx = 0; Idx < EventLinks.Num(); Idx++)
		{
			if (EventLinks(Idx).LinkDesc == ConnName)
			{
				return Idx;
			}
		}
	}

	return INDEX_NONE;
}


void USequenceOp::ForceActivateInput(INT InputIdx)
{
	if ( (InputIdx >= 0) && (InputIdx < InputLinks.Num()) )
	{
		InputLinks(InputIdx).bHasImpulse = TRUE;
		ParentSequence->QueueSequenceOp(this);
	}
}

void USequenceOp::ForceActivateOutput(INT OutputIdx)
{
	if ( (OutputIdx >= 0) && (OutputIdx < OutputLinks.Num()) )
	{
		FSeqOpOutputLink &Link = OutputLinks(OutputIdx);

		KISMET_LOG(TEXT("--> Link %s (%d) activated"),*Link.LinkDesc,OutputIdx);
		// iterate through all linked inputs looking for linked ops
		for (INT InputIdx = 0; InputIdx < Link.Links.Num(); InputIdx++)
		{
			if (Link.Links(InputIdx).LinkedOp != NULL)
			{
				FLOAT ActivateDelay = Link.ActivateDelay + Link.Links(InputIdx).LinkedOp->InputLinks(Link.Links(InputIdx).InputLinkIdx).ActivateDelay;
				if (ActivateDelay <= 0.f)
				{
					Link.Links(InputIdx).LinkedOp->ForceActivateInput(Link.Links(InputIdx).InputLinkIdx);
				}
				else
				{
					ParentSequence->QueueDelayedSequenceOp(this, &Link.Links(InputIdx), ActivateDelay);
				}
			}
		}
	}
}

#if WITH_EDITOR
void USequenceOp::SetBreakpoint(UBOOL bBreakpointOn)
{
	bIsBreakpointSet = bBreakpointOn;
	if(PIESequenceObject)
	{
		USequenceOp* PIEOp = Cast<USequenceOp>(PIESequenceObject);
		if(PIEOp)
		{
			PIEOp->bIsBreakpointSet = bBreakpointOn;
		}
	}
}
#endif


//==========================
// USequence interface

/**
 * Adds all the variables Linked to the external variable to the given variable
 * Link, recursing as necessary for multiply Linked external variables.
 */
static void AddExternalVariablesToLink(FSeqVarLink &varLink, USeqVar_External *extVar)
{
	if (extVar != NULL)
	{
		USequence *Seq = extVar->ParentSequence;
		if (Seq != NULL)
		{
			for (INT VarIdx = 0; VarIdx < Seq->VariableLinks.Num(); VarIdx++)
			{
				if (Seq->VariableLinks(VarIdx).LinkVar == extVar->GetFName())
				{
					for (INT Idx = 0; Idx < Seq->VariableLinks(VarIdx).LinkedVariables.Num(); Idx++)
					{
						USequenceVariable *var = Seq->VariableLinks(VarIdx).LinkedVariables(Idx);
						if (var != NULL)
						{
							//debugf(TEXT("... %s"),*var->GetName());
							// check for a Linked external variable
							if (var->IsA(USeqVar_External::StaticClass()))
							{
								// and recursively add
								AddExternalVariablesToLink(varLink,(USeqVar_External*)var);
							}
							else
							{
								// otherwise add this normal instance
								varLink.LinkedVariables.AddUniqueItem(var);
							}
						}
					}
				}
			}
		}
	}
}

/** Find the variable referenced by name by the USeqVar_Named and add a reference to it. */
static void AddNamedVariableToLink(FSeqVarLink& VarLink, USeqVar_Named *NamedVar)
{	
	// Do nothing if no variable or no type set.
	if (NamedVar == NULL || NamedVar->ExpectedType == NULL || NamedVar->FindVarName == NAME_None)
	{
		return;
	}
	check(NamedVar->ExpectedType->IsChildOf(USequenceVariable::StaticClass()));
	// start with the current sequence
	USequence *Seq = NamedVar->ParentSequence;
	while (Seq != NULL)
	{
		// look for named variables in this sequence
		TArray<USequenceVariable*> Vars;
		Seq->FindNamedVariables(NamedVar->FindVarName, FALSE, Vars, FALSE);
		// if one was found
		if (Vars.Num() > 0)
		{
			if (Vars(0)->IsA(USeqVar_External::StaticClass()))
			{
				// Recursively add
				AddExternalVariablesToLink(VarLink,(USeqVar_External*)(Vars(0)));
			}
			else
			{
				// otherwise add this normal instance
				VarLink.LinkedVariables.AddUniqueItem(Vars(0));
			}
			return;
		}
		// otherwise move to the next sequence
		Seq = Seq->ParentSequence;
	}
}

void USequenceObject::PostLoad()
{
	if (ParentSequence == NULL)
	{
		ParentSequence = Cast<USequence>(GetOuter());
	}

#if CONSOLE
	// If this is running on console clear out comment string to save memory
	// Cant cook this out as it destroys data for USeqAct_Log which should still be able to log messages to the screen on consoles
	ObjComment.Empty();
#endif

	Super::PostLoad();
}

void USequenceObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	MarkPackageDirty();
}

/** Used for finding the editor sequence object that corresponds to a PIE sequence object */
USequenceObject* USequenceObject::FindKismetObject()
{
	if ( GIsEditor && ( GetOutermost()->PackageFlags & PKG_PlayInEditor ) != 0 )
	{
		FString ObjectName = GetPathName();
		ObjectName = ObjectName.Right(ObjectName.Len() - 6);
		USequenceObject *FoundObject = Cast<USequenceObject>(UObject::StaticFindObject( GetClass() , ANY_PACKAGE, *ObjectName ));
		return FoundObject;
	}

	return NULL;
}

void USequence::PostLoad()
{
	Super::PostLoad();

	// Remove NULL entries.
	SequenceObjects.RemoveItem( NULL );

	if (GetLinkerVersion() < VER_FIXED_KISMET_SEQUENCE_NAMES)
	{
		FString MyName = GetName();
		FString InvalidChars = INVALID_OBJECTNAME_CHARACTERS;
		for (INT i = 0; i < InvalidChars.Len(); i++)
		{
			FString Char = InvalidChars.Mid(i, 1);
			MyName.ReplaceInline(*Char, TEXT("-"));
		}
		if (MyName != GetName())
		{
			debugf(TEXT("Fixing up Kismet sequence name: '%s' to '%s'"), *GetName(), *MyName);
			Rename(*MyName, NULL, REN_ForceNoResetLoaders);
		}
	}
}

/**
 * Conditionally creates the log file for this sequence.
 */
void USequence::CreateKismetLog()
{
#if !NO_LOGGING && !FINAL_RELEASE
	// Only create log file if we're not a nested USequence Object...
	if( ParentSequence == NULL
	// ... and kismet logging is enabled.
	&& GEngine->bEnableKismetLogging )
	{
		// Create the script log archive if necessary.
		if( LogFile == NULL )
		{
			// Create string with system time to create a unique filename.
			INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
			appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
			FString	CurrentTime = FString::Printf(TEXT("%i.%02i.%02i-%02i.%02i.%02i"), Year, Month, Day, Hour, Min, Sec );
			FString Filename = FString::Printf(TEXT("%sKismetLog-%s.log"), *appGameLogDir(), *CurrentTime);

			LogFile = new FOutputDeviceFile( *Filename );
			KISMET_LOG(TEXT("Opened Kismet log..."));
		}
	}
#endif
}

/**
 * Initialize this kismet sequence.
 *  - Creates the script log (if this sequence has no parent sequence)
 *  - Registers all events with the objects that they're associated with.
 *  - Resolves all "named" and "external" variable links contained by this sequence.
 */
void USequence::InitializeSequence()
{
	CreateKismetLog();

	// register any events, clear null entries, and/or handle special variable types
	NestedSequences.Empty();
	TArray<USequenceVariable*> ExtNamedVars;
	for (INT Idx = 0; Idx < SequenceObjects.Num(); Idx++)
	{
		USequenceObject* SequenceObj = SequenceObjects(Idx);
		if (SequenceObj == NULL)
		{
			SequenceObjects.Remove(Idx--,1);
			continue;
		}

		SequenceObj->Initialize();

		if( SequenceObj->IsA(USequenceEvent::StaticClass()) )
		{
			// If event fails to register
			if( !((USequenceEvent*)(SequenceObj))->RegisterEvent() )
			{
				// Put it in unregistered stack
				UnregisteredEvents.AddItem( (USequenceEvent*)(SequenceObj) );
			}
		}
		else
		{
			// replace any external or named variables with their Linked counterparts
			if (SequenceObj->IsA(USeqVar_External::StaticClass()))
			{
				ExtNamedVars.AddItem((USeqVar_External*)SequenceObj);
			}
			else if (SequenceObj->IsA(USeqVar_Named::StaticClass()))
			{
				ExtNamedVars.AddItem((USeqVar_Named*)SequenceObj);
			}
			// save a reference to any nested sequences
			else if (SequenceObj->IsA(USequence::StaticClass()))
			{
				NestedSequences.AddUniqueItem((USequence*)(SequenceObj));
			}
		}
		USequenceOp *Op = Cast<USequenceOp>(SequenceObj);
		if (Op != NULL)
		{
			for (INT ObjIdx = 0; ObjIdx < Op->VariableLinks.Num(); ObjIdx++)
			{
				if (Op->VariableLinks(ObjIdx).LinkedVariables.Num() == 0)
				{
					KISMET_LOG(TEXT("Op %s culling variable link %s"),*Op->GetFullName(),*(Op->VariableLinks(ObjIdx).LinkDesc));
					Op->VariableLinks.Remove(ObjIdx--,1);
				}
				else
				{
					UBOOL bHasLink = FALSE;
					for (INT VarIdx = 0; VarIdx < Op->VariableLinks(ObjIdx).LinkedVariables.Num(); VarIdx++)
					{
						if (Op->VariableLinks(ObjIdx).LinkedVariables(VarIdx) != NULL)
						{
							bHasLink = TRUE;
							break;
						}
					}
					if (!bHasLink)
					{
						KISMET_LOG(TEXT("Op %s culling variable link %s"),*Op->GetFullName(),*(Op->VariableLinks(ObjIdx).LinkDesc));
						Op->VariableLinks.Remove(ObjIdx--,1);
					}
				}
			}
		}
	}
	// hook up external/named variables
	if (ExtNamedVars.Num() > 0)
	{
		for (INT ObjIdx = 0; ObjIdx < SequenceObjects.Num(); ObjIdx++)
		{
			// if it's an op with variable links
			USequenceOp *Op = Cast<USequenceOp>(SequenceObjects(ObjIdx));
			if (Op != NULL &&
				Op->VariableLinks.Num() > 0)
			{
				// for each variable,
				for (INT VarIdx = 0; VarIdx < ExtNamedVars.Num(); VarIdx++)
				{
					// look for a link to the variable
					USequenceVariable *Var = ExtNamedVars(VarIdx);
					INT VarLinkIdx;
					for (INT LinkIdx = 0; LinkIdx < Op->VariableLinks.Num(); LinkIdx++)
					{
						FSeqVarLink &Link = Op->VariableLinks(LinkIdx);
						if (Link.LinkedVariables.FindItem(Var,VarLinkIdx))
						{
							// remove the placeholder var
							Link.LinkedVariables.Remove(VarLinkIdx,1);
							INT VarCount = Link.LinkedVariables.Num();
							// and hookup with the proper concrete var
							if (Var->IsA(USeqVar_External::StaticClass()))
							{
								AddExternalVariablesToLink(Link,(USeqVar_External*)Var);
							}
							else
							{
								AddNamedVariableToLink(Link,(USeqVar_Named*)Var);
							}
#if WITH_EDITORONLY_DATA
							// Update Kismet object reference so we can display the realtime value
							if (VarCount < Link.LinkedVariables.Num())
							{
								USequenceObject* KismetObject = Var->FindKismetObject();
								if( KismetObject )
								{
									KismetObject->PIESequenceObject = Link.LinkedVariables(Link.LinkedVariables.Num() - 1);
								}
							}
#endif // WITH_EDITORONLY_DATA
						}
					}
				}
			}
		}
	}
}

/**
 * Called from level startup.  Initializes the sequence and activates any level-startup
 * events contained within the sequence.
 */
void USequence::BeginPlay()
{
	InitializeSequence();

	// initialize all nested sequences
	for (INT Idx = 0; Idx < NestedSequences.Num(); Idx++)
	{
		KISMET_LOG(TEXT("Initializing nested sequence %s"),*NestedSequences(Idx)->GetName());
		NestedSequences(Idx)->BeginPlay();
	}

	// check for any auto-fire events
	for (INT Idx = 0; Idx < SequenceObjects.Num(); Idx++)
	{
		// if we are capturing a matinee, force start it
		if (GEngine->bStartWithMatineeCapture)
		{
			USeqAct_Interp* InterpAct = Cast<USeqAct_Interp>(SequenceObjects(Idx) );
			if(InterpAct && InterpAct->GetName() == GEngine->MatineeCaptureName)
			{
				FString	PackageName = InterpAct->ParentSequence->GetOutermost()->GetName();
				// Check for Play on PC.  That prefix is a bit special as it's only 5 characters. (All others are 6)
				if( PackageName.StartsWith( FString( PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX ) + TEXT( "PC") ) )
				{
					PackageName = PackageName.Right(PackageName.Len() - 5);
				}
				else if( PackageName.StartsWith( PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX ) )
				{
					// This is a Play on Console map package prefix. (6 characters)
					PackageName = PackageName.Right(PackageName.Len() - 6);
				}

				if (PackageName == GEngine->MatineePackageCaptureName)
				{
					USequenceObject* SequenceObj = SequenceObjects(Idx);
					USequenceOp *Op = Cast<USequenceOp>(SequenceObj);
					Op->ForceActivateInput(0);
				}
			}
		}
		else
		{
			// if outer most sequence, check for SequenceActivated events as well
			if (GetOuter()->IsA(ULevel::StaticClass()))
			{
				USeqEvent_SequenceActivated *Evt = Cast<USeqEvent_SequenceActivated>(SequenceObjects(Idx));
				if (Evt != NULL)
				{
					Evt->CheckActivateSimple();
				}
			}
			// level loaded event - only trigger the event if it has output links for "Loaded and Visible"
			USeqEvent_LevelLoaded* Evt = Cast<USeqEvent_LevelLoaded>(SequenceObjects(Idx));
			if( Evt != NULL && 
				Evt->OutputLinks.Num() > 0 && 
				Evt->OutputLinks(0).Links.Num() > 0)
			{
				TArray<INT> ActivateIndices;
				ActivateIndices.AddItem(0);	
				Evt->CheckActivate( GWorld->GetWorldInfo(), NULL, 0, &ActivateIndices );
			}
#if WITH_EDITORONLY_DATA
			// associate the editor versions of these objects
			USequenceObject* KismetObject = SequenceObjects(Idx)->FindKismetObject();
			if(KismetObject)
			{
				KismetObject->PIESequenceObject = SequenceObjects(Idx);
			}
#endif // WITH_EDITORONLY_DATA
		}
	}
}

/**
 * Activates LevelStartup and/or LevelBeginning events in this sequence
 *
 * @param bShouldActivateLevelStartupEvents If TRUE, will activate all LevelStartup events
 * @param bShouldActivateLevelBeginningEvents If TRUE, will activate all LevelBeginning events
 * @param bShouldActivateLevelLoadedEvents If TRUE, will activate all LevelLoadedAndVisible events
 */
void USequence::NotifyMatchStarted(UBOOL bShouldActivateLevelStartupEvents, UBOOL bShouldActivateLevelBeginningEvents, UBOOL bShouldActivateLevelLoadedEvents)
{
	// route beginplay if we need the LevelLoadedEvents
	if (bShouldActivateLevelLoadedEvents)
	{
		BeginPlay();
	}

	// notify nested sequences first
	for (INT Idx = 0; Idx < NestedSequences.Num(); Idx++)
	{
		NestedSequences(Idx)->NotifyMatchStarted(bShouldActivateLevelStartupEvents, bShouldActivateLevelBeginningEvents);
	}

	if (!GEngine->bStartWithMatineeCapture)
	{
		// and look for any startup events to activate
		for (INT Idx = 0; Idx < SequenceObjects.Num(); Idx++)
		{
			// activate LevelStartup events if desired
			if (bShouldActivateLevelStartupEvents)
			{
				UDEPRECATED_SeqEvent_LevelStartup* StartupEvt = Cast<UDEPRECATED_SeqEvent_LevelStartup>(SequenceObjects(Idx));
				if (StartupEvt != NULL)
				{
					StartupEvt->CheckActivate(GWorld->GetWorldInfo(),NULL,0);
				}

				// level loaded event - only trigger the event if it has output links for "Loaded and Visible"
				USeqEvent_LevelLoaded* LoadedEvt = Cast<USeqEvent_LevelLoaded>(SequenceObjects(Idx));
				if (LoadedEvt != NULL && 
					LoadedEvt->OutputLinks.Num() > 0 && 
					LoadedEvt->OutputLinks(0).Links.Num() > 0)
				{
					TArray<INT> ActivateIndices;
					ActivateIndices.AddItem(0);	
					LoadedEvt->CheckActivate(GWorld->GetWorldInfo(), NULL, 0, &ActivateIndices);
				}
			}

			// activate LevelBeginning events if desired
			if (bShouldActivateLevelBeginningEvents)
			{
				UDEPRECATED_SeqEvent_LevelBeginning* BeginningEvt = Cast<UDEPRECATED_SeqEvent_LevelBeginning>(SequenceObjects(Idx));
				if (BeginningEvt != NULL)
				{
					BeginningEvt->CheckActivate(GWorld->GetWorldInfo(),NULL,0);
				}

				// level loaded event - only trigger the event if it has output links for "Level Beginning"
				USeqEvent_LevelLoaded* LoadedEvt = Cast<USeqEvent_LevelLoaded>(SequenceObjects(Idx));
				if (LoadedEvt != NULL && 
					LoadedEvt->OutputLinks.Num() > 1 && 
					LoadedEvt->OutputLinks(1).Links.Num() > 0)
				{
					TArray<INT> ActivateIndices;
					ActivateIndices.AddItem(1);	
					LoadedEvt->CheckActivate(GWorld->GetWorldInfo(), NULL, 0, &ActivateIndices);
				}
			}
		}
	}
}

void USequence::MarkSequencePendingKill()
{
	MarkPendingKill();

	for (INT i = 0; i < SequenceObjects.Num(); i++)
	{
		if (SequenceObjects(i) != NULL)
		{
			SequenceObjects(i)->MarkPendingKill();
		}
	}
	for (INT i = 0; i < NestedSequences.Num(); i++)
	{
		if (NestedSequences(i) != NULL)
		{
			NestedSequences(i)->MarkSequencePendingKill();
		}
	}
}

/**
 * Overridden to release log file if opened.
 */
void USequence::FinishDestroy()
{
	FOutputDeviceFile* LogFile = (FOutputDeviceFile*) this->LogFile;

	if( LogFile != NULL )
	{
		LogFile->TearDown();
		delete LogFile;
		LogFile = NULL;
	}
	Super::FinishDestroy();
}
/**
 * USequence::ScriptLogf - diverts logging information to the dedicated logfile.
 */
VARARG_BODY(void, USequence::ScriptLogf, const TCHAR*, VARARG_NONE)
{
	if (LogFile != NULL)
	{
		FOutputDeviceFile *OutFile = (FOutputDeviceFile*)LogFile;
		INT		BufferSize	= 1024;
		TCHAR*	Buffer		= NULL;
		INT		Result		= -1;

		while(Result == -1)
		{
			appSystemFree(Buffer);
			Buffer = (TCHAR*) appSystemMalloc( BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
			BufferSize *= 2;
		};
		Buffer[Result] = 0;
		OutFile->Serialize( *FString::Printf(TEXT("[%2.3f] %s"),GWorld ? GWorld->GetWorldInfo()->TimeSeconds : 0.f,Buffer), NAME_Log );
		OutFile->Flush();
		appSystemFree( Buffer );
	}
}

VARARG_BODY(void, USequence::ScriptWarnf, const TCHAR*, VARARG_NONE)
{
	INT		BufferSize	= 1024;
	TCHAR*	Buffer		= NULL;
	INT		Result		= -1;

	while(Result == -1)
	{
		appSystemFree(Buffer);
		Buffer = (TCHAR*) appSystemMalloc( BufferSize * sizeof(TCHAR) );
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
		BufferSize *= 2;
	};
	Buffer[Result] = 0;
	// dump it to the main log as a warning
	debugf(NAME_Warning,Buffer);
	if (LogFile != NULL)
	{
		FOutputDeviceFile *OutFile = (FOutputDeviceFile*)LogFile;
		// dump it to the kismet log as well
		OutFile->Serialize(*FString::Printf(TEXT("[%2.3f] %s"),GWorld ? GWorld->GetWorldInfo()->TimeSeconds : 0.f,Buffer), NAME_Warning);
		OutFile->Flush();
	}
	// if enabled, send to the screen
	if (GEngine->bOnScreenKismetWarnings)
	{
		for (FLocalPlayerIterator It(GEngine); It; ++It)
		{
			if (It->Actor != NULL)
			{
				It->Actor->eventClientMessage(FString::Printf(TEXT("Kismet Warning: %s"), Buffer));
				break;
			}
		}
	}
	appSystemFree( Buffer );
}

/**
 * Looks for obvious errors and attempts to auto-correct them if
 * possible.
 */
#if WITH_EDITOR
void USequence::CheckForErrors()
{
	// check for seqeuences that have been removed, but still are ref'd
	UBOOL bOrphaned = HasAnyFlags(RF_PendingKill);
	if ( bOrphaned
	||	(ParentSequence != NULL && !ParentSequence->SequenceObjects.ContainsItem(this)))
	{
		debugf(TEXT("Orphaned sequence %s"),*GetFullName());
		bOrphaned = TRUE;
	}

	// check the sequence object list for errors
	for (INT Idx = 0; Idx < SequenceObjects.Num(); Idx++)
	{
		USequenceObject *SeqObj = SequenceObjects(Idx);
		
		// remove null objects
		if ( SeqObj == NULL || SeqObj->HasAnyFlags(RF_PendingKill) )
		{
			SequenceObjects.Remove(Idx--);
		}
		else if ( bOrphaned )
		{
			// propagate the error check before we clear the ParentSequence ref.
			SeqObj->CheckForErrors();

			SeqObj->ParentSequence = NULL;

			if ( !GIsGame )
			{
				// propagate the kill flag so that any actor refs are automatically cleared
				SeqObj->MarkPendingKill();
			}

			SequenceObjects.Remove(Idx--);
		}
		else
		{
			// make sure the parent sequence is properly set
			SeqObj->ParentSequence = this;

			// propagate the error check
			SeqObj->CheckForErrors();
		}
	}

	// fixup old sequences that have an invalid outer
	if ( GetOuter() == GWorld )
	{
		debugf(NAME_Warning,TEXT("Outer for '%s' is the world.  Moving into the world's current level: %s"),
			*GetFullName(), GWorld->CurrentLevel ? *GWorld->CurrentLevel->GetFullName() : TEXT("None"));

		Rename(NULL,GWorld->CurrentLevel);
	}
}
#endif

/**
 * Steps through the supplied operation stack, adding any newly activated operations to
 * the top of the stack.  Returns TRUE when the OpStack is empty.
 */
UBOOL USequence::ExecuteActiveOps(FLOAT DeltaTime, INT MaxSteps)
{
	// first check delay activations
	for (INT Idx = 0; Idx < DelayedActivatedOps.Num(); Idx++)
	{
		FActivateOp& DelayedOp = DelayedActivatedOps(Idx);

		DelayedOp.RemainingDelay -= DeltaTime;
		if (DelayedOp.RemainingDelay <= 0.f)
		{
			USequenceOp* OpToActivate = DelayedOp.Op;

			// don't activate if the link is disabled, or if we're in the editor and it's disabled for PIE only
			if ( OpToActivate->InputLinks(DelayedOp.InputIdx).ActivateInputLink() )
			{
				// notify this op that one of it's input links has been given impulse
				OpToActivate->OnReceivedImpulse(DelayedOp.ActivatorOp, DelayedOp.InputIdx);

				// check if we should log the object comment to the screen
				//@fixme - refactor this to avoid the code duplication
				if (GAreScreenMessagesEnabled && (GEngine->bOnScreenKismetWarnings) && (OpToActivate->bOutputObjCommentToScreen))
				{
					// iterate through the controller list
					for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
					{
						// if it's a player
						if (Controller->IsA(APlayerController::StaticClass()))
						{
							((APlayerController*)Controller)->eventClientMessage(OpToActivate->ObjComment,NAME_None);
						}
					}
				}          

				// stick the op on the activated stack
				QueueSequenceOp(OpToActivate, FALSE);
			}
			
			// and remove from the list
			DelayedActivatedOps.Remove(Idx--,1);
		}
	}
	// add all delayed latent ops to the activated stack
	while(DelayedLatentOps.Num() > 0)
	{
		QueueSequenceOp(DelayedLatentOps.Pop(),FALSE);
	}

	TArray<FActivateOp> NewlyActivatedOps;
	TArray<USequenceOp*> ActiveLatentOps;
	
	// while there are still active ops on stack,
	INT Steps = 0;
	while (ActiveSequenceOps.Num() > 0 &&
		   (Steps++ < MaxSteps || MaxSteps == 0))
	{
		// make sure we haven't hit an infinite loop
		if (Steps >= MAX_SEQUENCE_STEPS)
		{
			KISMET_WARN(TEXT("Max Kismet scripting execution steps exceeded, aborting!"));
			break;
		}
		// pop top node
		USequenceOp *NextOp = ActiveSequenceOps.Pop();
		// execute next action
		if (NextOp != NULL)
		{
			// Latent Operations in a loop might cause problems before hitting MAX_SEQUENCE_STEPs, so this is another check to avoid problems
			if(NextOp->IsA(USeqAct_Latent::StaticClass()))
			{
				USeqAct_Latent* LatentOp = CastChecked<USeqAct_Latent>(NextOp);
				if(LatentOp->bActive && appIsNearlyEqual(LatentOp->LatentActivationTime, GWorld->GetTimeSeconds()))
				{
					// Delay this latent operation to the next frame
					DelayedLatentOps.Push(LatentOp);
					continue;
				}
			}
			// copy any linked variable values to the op's matching properties
			NextOp->PublishLinkedVariableValues();
			// if it isn't already active
			if (!NextOp->bActive)
			{
				KISMET_LOG(TEXT("-> %s (%s) has been activated"),*NextOp->ObjName,*NextOp->GetName());
				// activate the op
				NextOp->bActive = TRUE;
				(NextOp->ActivateCount)++;
				NextOp->Activated();
				NextOp->eventActivated();
				// Set activation time on Latent Ops
				if(NextOp->IsA(USeqAct_Latent::StaticClass()))
				{
					USeqAct_Latent* LatentOp = CastChecked<USeqAct_Latent>(NextOp);
					LatentOp->LatentActivationTime = GWorld->GetTimeSeconds();
				}

#if !CONSOLE && WITH_EDITOR
				NextOp->PIEActivationTime = GWorld->GetTimeSeconds();
#endif
			}
			else if(NextOp->IsA(USeqAct_Latent::StaticClass()))
			{
					USeqAct_Latent* LatentOp = CastChecked<USeqAct_Latent>(NextOp);
					LatentOp->LatentActivationTime = GWorld->GetTimeSeconds();
			}
			UBOOL bOpDeActivated = FALSE;
			// update the op
			if (NextOp->bActive)
			{
				NextOp->bActive = !NextOp->UpdateOp(DeltaTime);
				// if it's no longer active, or a latent action
				if (!NextOp->bActive ||
					NextOp->bLatentExecution)
				{
					// if it's no longer active send the deactivated callback
					if(!NextOp->bActive)
					{
						bOpDeActivated = TRUE;
						KISMET_LOG(TEXT("-> %s (%s) has finished execution"),*NextOp->ObjName,*NextOp->GetName());
						NextOp->DeActivated();
						NextOp->eventDeactivated();
						// copy any properties to matching linked variables
						NextOp->PopulateLinkedVariableValues();
					}
					else
					{
						// add to the list of latent actions to put back on the stack for the next frame
						ActiveLatentOps.AddUniqueItem(NextOp);
					}
					// iterate through all outputs looking for new impulses,
					for (INT OutputIdx = 0; OutputIdx < NextOp->OutputLinks.Num(); OutputIdx++)
					{
						FSeqOpOutputLink &Link = NextOp->OutputLinks(OutputIdx);
						if (Link.bHasImpulse)
						{
#if !CONSOLE && WITH_EDITOR
							Link.PIEActivationTime = GWorld->GetTimeSeconds();

							if(Link.Links.Num() > 0)
							{
								NextOp->LastActivatedOutputLink = OutputIdx;
							}

							// Mark the link as being activated for the debugger
							if(GIsPlayInEditorWorld && GKismetRealtimeDebugging)
							{
								if (!Link.bIsActivated)
								{
									Link.bIsActivated = TRUE;
								}
							}
#endif

							KISMET_LOG(TEXT("--> Link %s (%d) activated"),*Link.LinkDesc,OutputIdx);
							// iterate through all linked inputs looking for linked ops
							for (INT InputIdx = 0; InputIdx < Link.Links.Num(); InputIdx++)
							{
								if (Link.Links(InputIdx).LinkedOp != NULL)
								{
									FLOAT ActivateDelay = Link.ActivateDelay + Link.Links(InputIdx).LinkedOp->InputLinks(Link.Links(InputIdx).InputLinkIdx).ActivateDelay;
									if (ActivateDelay <= 0.f)
									{
										// add to the list of pending activation for this frame
										// NOTE: this is to handle cases of self-activation, which would break since we immediately clear inputs/outputs after execution
										INT aIdx = NewlyActivatedOps.AddZeroed();
										NewlyActivatedOps(aIdx).Op = Link.Links(InputIdx).LinkedOp;
										NewlyActivatedOps(aIdx).InputIdx = Link.Links(InputIdx).InputLinkIdx;
									}
									else
									{
										QueueDelayedSequenceOp(NextOp, &Link.Links(InputIdx), ActivateDelay);
									}
								}
							}
						}
					}
				}
				else
				{
					debugf(NAME_Warning,TEXT("Op %s (%s) still active while bLatentExecution == FALSE"),*NextOp->GetFullName(),*NextOp->ObjName);
				}
			}
			// clear inputs on this op
			for (INT InputIdx = 0; InputIdx < NextOp->InputLinks.Num(); InputIdx++)
			{
				// if we have multiple activations for this input
				// ignore queued activations for latent actions since those are handled uniquely
				if (NextOp->InputLinks(InputIdx).QueuedActivations > 0 && !NextOp->bLatentExecution)
				{
					KISMET_LOG(TEXT("Reactivating %s, %d queued activations"),*NextOp->GetName(),NextOp->InputLinks(InputIdx).QueuedActivations);
					// retain the impulse, decrement the queue count, and re-add it to the execution list
					NextOp->InputLinks(InputIdx).QueuedActivations--;
					QueueSequenceOp(NextOp);
				}
				else
				{
					NextOp->InputLinks(InputIdx).bHasImpulse = FALSE;
					NextOp->InputLinks(InputIdx).QueuedActivations = 0;
				}
			}
			// and clear outputs
			for (INT OutputIdx = 0; OutputIdx < NextOp->OutputLinks.Num(); OutputIdx++)
			{
				NextOp->OutputLinks(OutputIdx).bHasImpulse = FALSE;
			}
			// add all pending ops to be activated
			while (NewlyActivatedOps.Num() > 0)
			{
				INT Idx = NewlyActivatedOps.Num() - 1;
				USequenceOp *Op = NewlyActivatedOps(Idx).Op;
				INT LinkIdx = NewlyActivatedOps(Idx).InputIdx;
				check(LinkIdx >= 0 && LinkIdx < Op->InputLinks.Num());

				// don't activate if the link is disabled, or if we're in the editor and it's disabled for PIE only
				if ( Op->InputLinks(LinkIdx).ActivateInputLink() )
				{
					// notify this op that one of it's input links has been given impulse
					Op->OnReceivedImpulse(NextOp, LinkIdx);
					QueueSequenceOp(Op,TRUE);

					// check if we should log the object comment to the screen
					if (GAreScreenMessagesEnabled && (GEngine->bOnScreenKismetWarnings) && (Op->bOutputObjCommentToScreen))
					{
						// iterate through the controller list
						for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
						{
							// if it's a player
							if (Controller->IsA(APlayerController::StaticClass()))
							{
								((APlayerController*)Controller)->eventClientMessage(Op->ObjComment,NAME_None);
							}
						}
					}          
				}
				// and remove from list
				NewlyActivatedOps.Pop();
				KISMET_LOG(TEXT("-> %s activating output to: %s"),*NextOp->GetName(),*Op->GetName());
			}
			// and a final notify for the deactivated op
			if (bOpDeActivated)
			{
				NextOp->PostDeActivated();
			}
		}

		// Pop QueuedActivations when all ActiveSequenceOps have been executed
		if( ActiveSequenceOps.Num() == 0 && QueuedActivations.Num() > 0 )
		{
			KISMET_LOG(TEXT("- unqueuing activation"));
			// if indices were specified then point to the array
			if (QueuedActivations(0).ActivateIndices.Num() > 0)
			{
				QueuedActivations(0).ActivatedEvent->ActivateEvent(QueuedActivations(0).InOriginator,QueuedActivations(0).InInstigator,&QueuedActivations(0).ActivateIndices,QueuedActivations(0).bPushTop,TRUE);
			}
			else
			{
				// otherwise pass NULL since ::ActivateEvent would fail with 0 length case
				QueuedActivations(0).ActivatedEvent->ActivateEvent(QueuedActivations(0).InOriginator,QueuedActivations(0).InInstigator,NULL,QueuedActivations(0).bPushTop,TRUE);
			}
			// remove from the queue
			QueuedActivations.Remove(0,1);
		}
	}

	// add back all the active latent actions for execution on the next frame
	while (ActiveLatentOps.Num() > 0)
	{
		USequenceOp *LatentOp = ActiveLatentOps.Pop();
		// make sure this op is still active
		if (LatentOp != NULL &&
			LatentOp->bActive)
		{
			QueueSequenceOp(LatentOp,TRUE);
		}
	}
	return (ActiveSequenceOps.Num() == 0);
}

/**
 * Is this sequence (and parent sequence) currently enabled?
 */
UBOOL USequence::IsEnabled() const
{
	return (bEnabled && (ParentSequence == NULL || ParentSequence->IsEnabled()));
}

/**
 * Set the sequence enabled flag.
 */
void USequence::SetEnabled(UBOOL bInEnabled)
{
	bEnabled = bInEnabled;
}

/**
 * Builds the list of currently active ops and executes them, recursing into
 * any nested sequences as well.
 */
UBOOL USequence::UpdateOp(FLOAT DeltaTime)
{
	checkf(!HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetFullName());

	// Go through each unregistered event
	for( INT UnRegIdx = 0; UnRegIdx < UnregisteredEvents.Num(); UnRegIdx++ )
	{
		// And try to register it... if successful
		if( UnregisteredEvents(UnRegIdx)->RegisterEvent() )
		{
			// Remove it from the list
			UnregisteredEvents.Remove( UnRegIdx-- );
		}
	}
	if (IsEnabled())
	{
		// execute any active ops
		ExecuteActiveOps(DeltaTime);
		// iterate through all active child Sequences and update them as well
		for (INT Idx = 0; Idx < NestedSequences.Num(); Idx++)
		{
			if (NestedSequences(Idx) != NULL)
			{
				NestedSequences(Idx)->UpdateOp(DeltaTime);
			}
			else
			{
				NestedSequences.Remove(Idx--,1);
			}
		}
	}
	return FALSE;
}

void USequence::OnExport()
{
	Super::OnExport();

	for ( INT Idx = 0; Idx < SequenceObjects.Num(); Idx++ )
	{
		USequenceObject* SeqObj = SequenceObjects(Idx);
		if ( SeqObj != NULL )
		{
			SeqObj->OnExport();
		}
	}

	// only eliminate external links if this is the topmost sequence
	if ( GetTypedOuter<USequence>() == NULL )
	{
		for (INT idx = 0; idx < OutputLinks.Num(); idx++)
		{
			OutputLinks(idx).Links.Empty();
		}
		for (INT idx = 0; idx < VariableLinks.Num(); idx++)
		{
			VariableLinks(idx).LinkedVariables.Empty();
		}
		for (INT idx = 0; idx < EventLinks.Num(); idx++)
		{
			EventLinks(idx).LinkedEvents.Empty();
		}
	}
}


/**
 * Iterates through all Sequence Objects look for finish actions, creating output Links as needed.
 * Also handles inputs via SeqEvent_SequenceActivated, and variables via SeqVar_External.
 */
void USequence::UpdateConnectors()
{
	// look for changes in existing outputs, or new additions
	TArray<FName> outNames;
	TArray<FName> inNames;
	TArray<FName> varNames;

	for ( INT Idx = 0; Idx < SequenceObjects.Num(); Idx++ )
	{
		USequenceObject* SeqObj = SequenceObjects(Idx);
		if (SeqObj->IsA(USeqAct_FinishSequence::StaticClass()))
		{
			USeqAct_FinishSequence* act = (USeqAct_FinishSequence*)SeqObj;

			// look for an existing output Link
			UBOOL bFoundOutput = FALSE;
			for (INT LinkIdx = 0; LinkIdx < OutputLinks.Num(); LinkIdx++)
			{
				FSeqOpOutputLink& OutputLink = OutputLinks(LinkIdx);
				USequenceOp* Action = OutputLink.LinkedOp;
				if ( Action == act
				||	(!Action && OutputLink.LinkDesc == act->OutputLabel) )
				{
					// Update the text label
					OutputLink.LinkDesc = act->OutputLabel;
					// If action is blank - fill it in
					if( Action == NULL )
					{
						OutputLink.LinkedOp = act;
					}
					// Add to the list of known outputs
					outNames.AddItem(act->GetFName());

					// Mark as found
					bFoundOutput = TRUE;
					break;
				}
			}

			// if we didn't find an output
			if ( !bFoundOutput )
			{
				// create a new connector
				INT NewIdx = OutputLinks.AddZeroed();
				OutputLinks(NewIdx).LinkDesc = act->OutputLabel;
				OutputLinks(NewIdx).LinkedOp = act;
				outNames.AddItem(act->GetFName());
			}
		}
		else if (SequenceObjects(Idx)->IsA(USeqEvent_SequenceActivated::StaticClass()))
		{
			USeqEvent_SequenceActivated *evt = (USeqEvent_SequenceActivated*)(SequenceObjects(Idx));

			// look for an existing input Link
			UBOOL bFoundInput = FALSE;
			for (INT LinkIdx = 0; LinkIdx < InputLinks.Num(); LinkIdx++)
			{
				USequenceOp* Action = InputLinks(LinkIdx).LinkedOp;
				if(   Action == evt ||
					(!Action && InputLinks(LinkIdx).LinkDesc == evt->InputLabel) )
				{
					// Update the text label
					InputLinks(LinkIdx).LinkDesc	= evt->InputLabel;
					// If action is blank - fill it in
					if( !Action )
					{
						InputLinks(LinkIdx).LinkedOp	= evt;
					}
					// Add to the list of known inputs
					inNames.AddItem(evt->GetFName());

					// Mark as found
					bFoundInput = TRUE;
					break;
				}
			}
			// if we didn't find an input,
			if (!bFoundInput)
			{
				// create a new connector
				INT NewIdx = InputLinks.AddZeroed();
				InputLinks(NewIdx).LinkDesc = evt->InputLabel;
				InputLinks(NewIdx).LinkedOp = evt;
				inNames.AddItem(evt->GetFName());
			}
		}
		else if (SequenceObjects(Idx)->IsA(USeqVar_External::StaticClass()))
		{
			USeqVar_External* var = (USeqVar_External*)(SequenceObjects(Idx));

			// look for an existing var Link
			UBOOL bFoundVar = FALSE;
			for (INT VarIdx = 0; VarIdx < VariableLinks.Num(); VarIdx++)
			{
				FSeqVarLink& VarLink = VariableLinks(VarIdx);
				if ( VarLink.LinkVar == var->GetFName() )
				{
					bFoundVar = 1;
					// update the text label
					VarLink.LinkDesc = var->VariableLabel;
					VarLink.ExpectedType = var->ExpectedType;

					// and add to the list of known vars
					varNames.AddItem(var->GetFName());
					break;
				}
			}
			if (!bFoundVar)
			{
				// add a new entry
				INT NewIdx = VariableLinks.AddZeroed();

				FSeqVarLink& VarLink = VariableLinks(NewIdx);
				VarLink.LinkDesc = var->VariableLabel;
				VarLink.LinkVar = var->GetFName();
				VarLink.ExpectedType = var->ExpectedType;
				VarLink.MinVars = 0;
				VarLink.MaxVars = 255;
				varNames.AddItem(var->GetFName());
			}
		}
	}
	// clean up any outputs that may of been deleted
	for (INT Idx = 0; Idx < OutputLinks.Num(); Idx++)
	{
		USequenceOp* Action = OutputLinks(Idx).LinkedOp;
		if( !Action ||
			!outNames.ContainsItem( Action->GetFName() ) )
		{
			OutputLinks.Remove(Idx--,1);
		}
	}

	// and finally clean up any inputs that may of been deleted
	USequence* OuterSeq = GetTypedOuter<USequence>();
	for (INT InputLinkIndex = 0; InputLinkIndex < InputLinks.Num(); InputLinkIndex++)
	{
		INT itemIdx = 0;
		USequenceOp* Action = InputLinks(InputLinkIndex).LinkedOp;
		if( !Action ||
			!inNames.FindItem(Action->GetFName(), itemIdx) )
		{
			InputLinks.Remove(InputLinkIndex,1);

			if ( OuterSeq )
			{
				for(INT osIdx=0; osIdx<OuterSeq->SequenceObjects.Num(); osIdx++)
				{
					USequenceOp* ChkOp = Cast<USequenceOp>(OuterSeq->SequenceObjects(osIdx));
					if(ChkOp)
					{
						// iterate through this op's output Links
						for (INT OutputLinkIdx = 0; OutputLinkIdx < ChkOp->OutputLinks.Num(); OutputLinkIdx++)
						{
							FSeqOpOutputLink& OutputLink = ChkOp->OutputLinks(OutputLinkIdx);

							// iterate through all the inputs connected to this output
							for (INT InputIdx = 0; InputIdx < OutputLink.Links.Num(); InputIdx++)
							{
								FSeqOpOutputInputLink& OutLink = OutputLink.Links(InputIdx);

								// If Link is to this Sequence..
								if(OutLink.LinkedOp == this)
								{
									// If it is to the input we are removing, remove the Link.
									if(OutLink.InputLinkIdx == InputLinkIndex)
									{
										// remove the entry
										OutputLink.Links.Remove(InputIdx--, 1);
									}
									// If it is to a Link after the one we are removing, adjust accordingly.
									else if (OutLink.InputLinkIdx > InputLinkIndex)
									{
										OutLink.InputLinkIdx--;
									}
								}
							}
						}
					}
				}
			}

			InputLinkIndex--;
		}
	}
	// look for missing variable Links
	for (INT Idx = 0; Idx < VariableLinks.Num(); Idx++)
	{
		INT itemIdx = 0;
		if (!varNames.FindItem(VariableLinks(Idx).LinkVar,itemIdx))
		{
			VariableLinks.Remove(Idx--,1);
		}
	}
}

/**
 * Activates any SeqEvent_SequenceActivated contained within this Sequence that match
 * the LinkedOp of the activated InputLink.
 */
void USequence::Activated()
{
	Super::Activated();
	InitializeLinkedVariableValues();

	TArray<USeqEvent_SequenceActivated*> ActivateEvents;
	UBOOL bActivateEventListInitialized = FALSE;

	// figure out which inputs are active
	for (INT Idx = 0; Idx < InputLinks.Num(); Idx++)
	{
		if (InputLinks(Idx).bHasImpulse)
		{
			if ( !bActivateEventListInitialized )
			{
				bActivateEventListInitialized = TRUE;
				for (INT ObjIdx = 0; ObjIdx < SequenceObjects.Num(); ObjIdx++)
				{
					USeqEvent_SequenceActivated *evt = Cast<USeqEvent_SequenceActivated>(SequenceObjects(ObjIdx));
					if ( evt != NULL )
					{
						ActivateEvents.AddUniqueItem(evt);
					}
				}
			}

			// find the matching Sequence Event to activate
			for (INT ObjIdx = 0; ObjIdx < ActivateEvents.Num(); ObjIdx++)
			{
				USeqEvent_SequenceActivated *evt = ActivateEvents(ObjIdx);
				if ( evt == InputLinks(Idx).LinkedOp )
				{
					evt->CheckActivateSimple();
				}
			}
		}
	}

	// and deactivate this Sequence
	bActive = FALSE;
}

/**
 * Find all sequence objects within this sequence that have an editor-visible property value containing the provided search string
 *
 * @param	SearchString	String to search for within the editor-visible property values
 * @param	OutputObjects	Sequence objects within this sequence that have an editor-visible property value containing the search string
 * @param	bRecursive		If TRUE, also search within subsequences in this sequence
 */
void USequence::FindSeqObjectsByPropertyValue( const FString& SearchString, TArray<USequenceObject*>& OutputObjects, UBOOL bRecursive /*= TRUE*/ ) const
{
#if WITH_EDITOR
	// Iterate over every sequence object in the sequence checking each of their properties to see if they match the specified search string
	for ( TArray<USequenceObject*>::TConstIterator SeqObjIter( SequenceObjects ); SeqObjIter; ++SeqObjIter )
	{
		USequenceObject* CurObj = *SeqObjIter;
		const UClass* CurObjClass = CurObj->GetClass();
		for ( UProperty* Property = CurObjClass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext )
		{
			// Only consider properties which are editable in the editor, aren't hidden, and aren't bool properties (searching for TRUE/FALSE would return almost all of the objects)
			if ( Property->HasAnyPropertyFlags( CPF_Edit ) && !CurObjClass->HideCategories.ContainsItem( Property->Category ) && !Property->IsA( UBoolProperty::StaticClass() ) )
			{
				// Extract the property's value
				const BYTE* ValueAddress = (BYTE*)CurObj + Property->Offset;
				FString StringValue;
				Property->ExportTextItem( StringValue, ValueAddress, NULL, CurObj, PPF_PropertyWindow | PPF_Localized );

				// If the property value contains the search string, it's a match!
				if ( StringValue.InStr( SearchString, FALSE, TRUE ) != INDEX_NONE )
				{
					OutputObjects.AddItem( CurObj );
					break;
				}
			}
		}

		// If a recursive search was requested and the current object is a subsequence, run the search again on from the subsequence
		if ( bRecursive )
		{
			USequence* SubSequence = Cast<USequence>( CurObj );
			if ( SubSequence )
			{
				SubSequence->FindSeqObjectsByPropertyValue( SearchString, OutputObjects, bRecursive );
			}
		}
	}
#endif
}

/** 
 *	Find all the SequenceObjects of the specified class within this Sequence and add them to the OutputObjects array. 
 *	Will look in any subSequences as well. 
 *	Objects in parent Sequences are always added to array before children.
 *
 *	@param	DesiredClass	Subclass of SequenceObject to search for.
 *	@param	OutputObjects	Output array of Objects of the desired class.
 */
void USequence::FindSeqObjectsByClass(UClass* DesiredClass, TArray<USequenceObject*>& OutputObjects, UBOOL bRecursive) const
{
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		if(SequenceObjects(i) != NULL && SequenceObjects(i)->IsA(DesiredClass))
		{
			OutputObjects.AddItem( SequenceObjects(i) );
		}
	}

	// Look in any subSequences of this one. 
	if (bRecursive)
	{
		// In the game we can optimise by using NestedSequences array - but this might not be valid in the editor
		if(GIsGame)
		{
			for(INT i=0; i < NestedSequences.Num(); i++)
			{
				USequence* Seq = NestedSequences(i);
				if(Seq)
				{
					Seq->FindSeqObjectsByClass(DesiredClass, OutputObjects, bRecursive);
				}
			}
		}
		else
		{
			for(INT i=0; i < SequenceObjects.Num(); i++)
			{
				USequence* Seq = Cast<USequence>( SequenceObjects(i) );
				if(Seq)
				{
					Seq->FindSeqObjectsByClass(DesiredClass, OutputObjects, bRecursive);
				}
			}
		}
	}
}


/** 
 *	Searches this Sequence (and subSequences) for SequenceObjects which contain the given string in their name (ie substring match). 
 *	Will only add results to OutputObjects if they are not already there.
 *
 *	@param Name				Name to search for
 *	@param bCheckComment	Search Object comments as well
 *	@param OutputObjects	Output array of Objects matching supplied name
 */
void USequence::FindSeqObjectsByName(const FString& Name, UBOOL bCheckComment, TArray<USequenceObject*>& OutputObjects, UBOOL bRecursive, UBOOL bUseFullLevelName) const
{
	FString CapsName = Name.ToUpper();

	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		FString ObjName = bUseFullLevelName? SequenceObjects(i)->GetSeqObjFullLevelName(): SequenceObjects(i)->ObjName;
		if( (ObjName.ToUpper().InStr(*CapsName) != -1) || (bCheckComment && (SequenceObjects(i)->ObjComment.ToUpper().InStr(*CapsName) != -1)) )
		{
			OutputObjects.AddUniqueItem( SequenceObjects(i) );
		}
	
		// Check any subSequences.
		if (bRecursive)
		{
			USequence* SubSeq = Cast<USequence>( SequenceObjects(i) );
			if(SubSeq)
			{
				SubSeq->FindSeqObjectsByName( Name, bCheckComment, OutputObjects, bRecursive, bUseFullLevelName );
			}
		}
	}
}

/** 
 *	Searches this Sequence (and subSequences) for SequenceObjects which reference an Object with the supplied name. Complete match, not substring.
 *	Will only add results to OutputObjects if they are not already there.
 *
 *	@param Name				Name of referenced Object to search for
 *	@param OutputObjects	Output array of Objects referencing the Object with the supplied name.
 */
void USequence::FindSeqObjectsByObjectName(FName Name, TArray<USequenceObject*>& OutputObjects, UBOOL bRecursive) const
{
	// Iterate over Objects in this Sequence
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		// If its an SeqVar_Object, check its contents.
		USeqVar_Object* ObjVar = Cast<USeqVar_Object>( SequenceObjects(i) );
		if(ObjVar && ObjVar->ObjValue && ObjVar->ObjValue->GetFName() == Name)
		{
			OutputObjects.AddUniqueItem(ObjVar);
		}

		// If its a SequenceEvent, check the Originator.
		USequenceEvent* Event = Cast<USequenceEvent>( SequenceObjects(i) );
		if(Event && Event->Originator && Event->Originator->GetFName() == Name)
		{
			OutputObjects.AddUniqueItem(Event);
		}

		// Search for items inside object list variables
		USeqVar_ObjectList* objList = Cast<USeqVar_ObjectList>( SequenceObjects(i) );
		if( objList )
		{
			for( INT CurObjectIndex = 0; CurObjectIndex < objList->ObjList.Num(); ++CurObjectIndex )
			{
				UObject** ObjectRef = objList->GetObjectRef( CurObjectIndex );
				if( ObjectRef && *ObjectRef && (*ObjectRef)->GetFName() == Name )
				{
					OutputObjects.AddUniqueItem( objList );
				}
			}
		}

		// Check any subSequences.
		if (bRecursive)
		{
			USequence* SubSeq = Cast<USequence>( SequenceObjects(i) );
			if(SubSeq)
			{
				SubSeq->FindSeqObjectsByObjectName( Name, OutputObjects );
			}
		}
	}
}

/** 
 *	Search this Sequence and all subSequences to find USequenceVariables with the given VarName.
 *	This function can also find uses of a named variable.
 *
 *	@param	VarName			Checked against VarName (or FindVarName) to find particular Variable.
 *	@param	bFindUses		Instead of find declaration of variable with given name, will find uses of it.
 *  @param	OutputVars		Results of search - array containing found Sequence Variables which match supplied name.
 */
void USequence::FindNamedVariables(FName VarName, UBOOL bFindUses, TArray<USequenceVariable*>& OutputVars, UBOOL bRecursive) const
{
	// If no name passed in, never return variables.
	if(VarName == NAME_None)
	{
		return;
	}

	// Iterate over Objects in this Sequence
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		if(bFindUses)
		{
			// Check any USeqVar_Named.
			USeqVar_Named* NamedVar = Cast<USeqVar_Named>( SequenceObjects(i) );
			if(NamedVar && NamedVar->FindVarName == VarName)
			{
				OutputVars.AddUniqueItem(NamedVar);
			}
		}
		else
		{
			// Check any SequenceVariables.
			USequenceVariable* SeqVar = Cast<USequenceVariable>( SequenceObjects(i) );
			if(SeqVar && SeqVar->VarName == VarName)
			{
				OutputVars.AddUniqueItem(SeqVar);
			}
		}

		// Check any subSequences.
		if (bRecursive)
		{
			USequence* SubSeq = Cast<USequence>( SequenceObjects(i) );
			if(SubSeq)
			{
				SubSeq->FindNamedVariables( VarName, bFindUses, OutputVars );
			}
		}
	}
}

	
/**
 * Finds all sequence objects contained by this sequence which are linked to any of the specified objects
 *
 * @param	SearchObjects	the collection of objects to search for references to
 * @param	out_Referencers	will be filled in with the sequence objects which reference any objects in the SearchObjects set
 * @param	bRecursive		TRUE to search subsequences as well
 *
 * @return	TRUE if at least one object in the sequence objects array is referencing one of the objects in the set
 */
UBOOL USequence::FindReferencingSequenceObjects( const TArray<class UObject*>& SearchObjects, TArray<class USequenceObject*>* out_Referencers/*=NULL*/, UBOOL bRecursive/*=TRUE*/ ) const
{
	UBOOL bResult = FALSE;

	// Iterate over Objects in this Sequence
	for(INT SeqObjIndex=0; SeqObjIndex< SequenceObjects.Num(); SeqObjIndex++)
	{
		USequenceObject* SeqObj = SequenceObjects(SeqObjIndex);

		for ( INT SearchObjIndex = 0; SearchObjIndex < SearchObjects.Num(); SearchObjIndex++ )
		{
			UObject* SearchObj = SearchObjects(SearchObjIndex);

			// If its an SeqVar_Object, check its contents.
			USeqVar_Object* ObjVar = Cast<USeqVar_Object>( SeqObj );
			if( ObjVar != NULL )
			{
				if ( ObjVar->ObjValue && ObjVar->ObjValue == SearchObj )
				{
					bResult = TRUE;

					if ( out_Referencers != NULL )
					{
						out_Referencers->AddUniqueItem(ObjVar);
					}
					else
					{
						break;
					}
				}
			}
			else
			{
				// If its a SequenceEvent, check the Originator.
				USequenceEvent* Event = Cast<USequenceEvent>( SeqObj );
				if( Event != NULL )
				{
					if ( Event->Originator && Event->Originator == SearchObj )
					{
						bResult = TRUE;
						if ( out_Referencers != NULL )
						{
							out_Referencers->AddUniqueItem(Event);
						}
						else
						{
							break;
						}
					}
				}
				// Check any subSequences.
				else if ( bRecursive )
				{
					USequence* SubSeq = Cast<USequence>( SeqObj );
					if(SubSeq)
					{
						bResult = SubSeq->FindReferencingSequenceObjects(SearchObjects, out_Referencers, bRecursive) || bResult;
					}
				}
			}
		}
	}

	return bResult;
}

/**
 * Finds all sequence objects contained by this sequence which are linked to the specified object
 *
 * @param	SearchObject	the object to search for references to
 * @param	out_Referencers	will be filled in with the sequence objects which reference the specified object
 * @param	bRecursive		TRUE to search subsequences as well
 *
 * @return	TRUE if at least one object in the sequence objects array is referencing the object
 */
UBOOL USequence::FindReferencingSequenceObjects( UObject* SearchObject, TArray<class USequenceObject*>* out_Referencers/*=NULL*/, UBOOL bRecursive/*=TRUE*/ ) const
{
	TArray<UObject*> ObjArray;
	ObjArray.AddItem(SearchObject);

	return FindReferencingSequenceObjects(ObjArray, out_Referencers, bRecursive);
}

/**
 * Finds all sequence objects contained by this sequence which are linked to the specified sequence object.
 *
 * @param	SearchObject		the sequence object to search for link references to
 * @param	out_Referencers		if specified, receieves the list of sequence objects contained by this sequence
 *								which are linked to the specified op
 *
 * @return	TRUE if at least one object in the sequence objects array is linked to the specified op.
 */
UBOOL USequence::FindSequenceOpReferencers( USequenceObject* SearchObject, TArray<USequenceObject*>* out_Referencers/*=NULL*/ )
{
	UBOOL bResult = FALSE;

	if ( SearchObject != NULL )
	{
		// If this is a sequence op then we check all other ops inputs to see if any reference to it.
		USequenceOp* SeqOp = Cast<USequenceOp>(SearchObject);
		if (SeqOp != NULL)
		{
			// iterate through all other objects, looking for output links that point to this op
			for (INT chkIdx = 0; chkIdx < SequenceObjects.Num(); chkIdx++)
			{
				if ( SequenceObjects(chkIdx) != SeqOp )
				{
					// if it is a sequence op,
					USequenceOp* ChkOp = Cast<USequenceOp>(SequenceObjects(chkIdx));
					if ( ChkOp != NULL )
					{
						// iterate through this op's output links
						for (INT linkIdx = 0; linkIdx < ChkOp->OutputLinks.Num(); linkIdx++)
						{
							FSeqOpOutputLink& OutputLink = ChkOp->OutputLinks(linkIdx);

							UBOOL bBreakLoop = FALSE;

							// iterate through all the inputs linked to this output
							for (INT inputIdx = 0; inputIdx < OutputLink.Links.Num(); inputIdx++)
							{
								if ( OutputLink.Links(inputIdx).LinkedOp == SeqOp)
								{
									if ( out_Referencers != NULL )
									{
										out_Referencers->AddUniqueItem(ChkOp);
									}

									bBreakLoop = TRUE;
									bResult = TRUE;
									break;
								}
							}

							if ( bBreakLoop )
							{
								break;
							}
						}

						if ( out_Referencers == NULL && bResult )
						{
							// if we don't care about who is referencing this op, we can stop as soon as we have at least one referencer
							break;
						}
					}
				}
			}
		}
		else
		{
			// If we are searching for references to a variable - we must search the variable links of all ops
			USequenceVariable* SeqVar = Cast<USequenceVariable>( SearchObject );
			if( SeqVar != NULL )
			{
				for ( INT ChkIdx=0; ChkIdx<SequenceObjects.Num(); ChkIdx++ )
				{
					if ( SequenceObjects(ChkIdx) != SeqVar )
					{
						USequenceOp* ChkOp = Cast<USequenceOp>( SequenceObjects(ChkIdx) );
						if ( ChkOp != NULL )
						{
							for( INT LinkIndex=0; LinkIndex < ChkOp->VariableLinks.Num(); LinkIndex++ )
							{
								FSeqVarLink& VarLink = ChkOp->VariableLinks(LinkIndex);

								UBOOL bBreakLoop = FALSE;
								for ( INT VariableIndex = 0; VariableIndex < VarLink.LinkedVariables.Num(); VariableIndex++)
								{
									if ( VarLink.LinkedVariables(VariableIndex) == SeqVar)
									{
										if ( out_Referencers != NULL )
										{
											out_Referencers->AddUniqueItem(ChkOp);
										}

										bBreakLoop = TRUE;
										bResult = TRUE;
										break;
									}
								}

								if ( bBreakLoop )
								{
									break;
								}
							}
						}

						if ( out_Referencers == NULL && bResult )
						{
							// if we don't care about who is referencing this op, we can stop as soon as we have at least one referencer
							break;
						}
					}
				}
			}
			else
			{
				USequenceEvent* SeqEvt = Cast<USequenceEvent>( SearchObject );
				if( SeqEvt != NULL )
				{
					// search for any ops that have a link to this event
					for ( INT ChkIdx = 0; ChkIdx < SequenceObjects.Num(); ChkIdx++)
					{
						if ( SequenceObjects(ChkIdx) != SeqEvt )
						{
							USequenceOp* ChkOp = Cast<USequenceOp>(SequenceObjects(ChkIdx));
							if ( ChkOp != NULL )
							{
								for (INT eventIdx = 0; eventIdx < ChkOp->EventLinks.Num(); eventIdx++)
								{
									FSeqEventLink& EvtLink = ChkOp->EventLinks(eventIdx);

									UBOOL bBreakLoop = FALSE;
									for (INT chkIdx = 0; chkIdx < ChkOp->EventLinks(eventIdx).LinkedEvents.Num(); chkIdx++)
									{
										if (ChkOp->EventLinks(eventIdx).LinkedEvents(chkIdx) == SeqEvt)
										{
											if ( out_Referencers != NULL )
											{
												out_Referencers->AddUniqueItem(ChkOp);
											}

											bBreakLoop = TRUE;
											bResult = TRUE;
											break;
										}
									}

									if ( bBreakLoop )
									{
										break;
									}
								}
							}

							if ( out_Referencers == NULL && bResult )
							{
								// if we don't care about who is referencing this op, we can stop as soon as we have at least one referencer
								break;
							}
						}
					}
				}
			}
		}
	}
	return bResult;
}

/**
 * Returns a list of output links from this sequence's ops which reference the specified op.
 *
 * @param	SeqOp	the sequence object to search for output links to
 * @param	Links	[out] receives the list of output links which reference the specified op.
 * @param   DupOp   copy of the sequence object to search for self-links when doing an object update
 */
void USequence::FindLinksToSeqOp(USequenceOp* SeqOp, TArray<FSeqOpOutputLink*> &Links, USequenceOp* DupOp)
{
	if (SeqOp != NULL)
	{
		// search each object,
		for (INT ObjIdx = 0; ObjIdx < SequenceObjects.Num(); ObjIdx++)
		{
			// for any ops with output links,
			USequenceOp *Op = Cast<USequenceOp>(SequenceObjects(ObjIdx));

			// if provided, use DupOp for self-linking objects
			if (Op == SeqOp && 
				DupOp != NULL)
			{
				Op = DupOp;
			}

			if (Op != NULL &&
				Op->OutputLinks.Num() > 0)
			{
				// search each output link,
				for (INT LinkIdx = 0; LinkIdx < Op->OutputLinks.Num(); LinkIdx++)
				{
					// for a link to the op,
					FSeqOpOutputLink &Link = Op->OutputLinks(LinkIdx);
					if (Link.HasLinkTo(SeqOp))
					{
						// add to the results
						Links.AddItem(&Link);
					}
				}
			}
		}
	}
}

/**
 * Get the sequence which contains all PrefabInstance sequences.
 *
 * @param	bCreateIfNecessary		indicates whether the Prefabs sequence should be created if it doesn't exist.
 *
 * @return	pointer to the sequence which serves as the parent for all PrefabInstance sequences in the map.
 */
USequence* USequence::GetPrefabsSequence( UBOOL bCreateIfNecessary/*=TRUE*/ )
{
	USequence* Result = NULL;

	// Look through existing subsequences to see if we have one called Prefabs,
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		USequence* SubSeq = Cast<USequence>( SequenceObjects(i) );
		if ( SubSeq && SubSeq->IsPrefabSequenceContainer() )
		{
			// return it if we found it
			Result = SubSeq;
			break;
		}
	}

	if ( Result == NULL && bCreateIfNecessary )
	{
		// We didn't find one, so create it now - at (0,0)
		USequence* NewPrefabsSequence = ConstructObject<UPrefabSequenceContainer>(UPrefabSequenceContainer::StaticClass(), this, PREFAB_SEQCONTAINER_NAME, RF_Transactional);
		NewPrefabsSequence->ObjName = PREFAB_SEQCONTAINER_NAME;
		NewPrefabsSequence->bDeletable = FALSE;
		
		// Add to sequence objects array.
		if ( AddSequenceObject(NewPrefabsSequence) )
		{
			// If we have begun play..
			if( GWorld->HasBegunPlay() )
			{
				// ..add to optimised 'Nested Sequences' array.
				NestedSequences.AddUniqueItem(NewPrefabsSequence);
			}

			Result = NewPrefabsSequence;
		}
	}

	return Result;
}

/** Utility class for making a unique name within a sequence. */
static FName MakeUniqueSubsequenceName( UObject* Outer, USequence* SubSeq )
{
	TCHAR NewBase[NAME_SIZE], Result[MAX_SPRINTF];
	TCHAR TempIntStr[MAX_SPRINTF]=TEXT("");

	// Make base name sans appended numbers.
	appStrcpy( NewBase, *SubSeq->GetName() );
	TCHAR* End = NewBase + appStrlen(NewBase);
	while( End>NewBase && (appIsDigit(End[-1]) || End[-1] == TEXT('_')) )
		End--;
	*End = 0;

	// Append numbers to base name.
	INT TryNum = 0;
	do
	{
		appSprintf( TempIntStr, TEXT("_%i"), TryNum++ );
		appStrncpy( Result, NewBase, MAX_SPRINTF-appStrlen(TempIntStr)-1 );
		appStrcat( Result, TempIntStr );
	} 
	while( FindObject<USequenceObject>( Outer, Result ) );

	return Result;
}

/**
 * Look through levels contrary to the current objects scope and ensure no other name conflicts would be present
 *
 * @param	InName			the name to search for
 * @param	ParentName		the name of the parent sequence
 * @param	Flags			Flags for how the object is renamed
 *
 * @return	TRUE if at least one object was found and successfully renamed.
 */
UBOOL USequence::RenameAllObjectsInOtherLevels( const FName& InName, const FName& ParentName, ERenameFlags Flags )
{
	UBOOL bFoundRef = FALSE;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		// Compare the name of the current object to that chosen, and ensure that the parent does not match that of the object
		// as renaming of these is done in the ClearNameUsage Fn
		UObject* Object = *It;
		if( InName == Object->GetFName() && Object->GetOuter() && Object->GetOuter()->GetFName() != ParentName )
		{
			const FName OldName = Object->GetFName();
			Object->Rename(NULL, NULL, Flags );
			const FName NewName = Object->GetFName();
//		debugfSlow( TEXT("Renamed %s to %s"), *OldName.ToString(), *NewName.ToString() );
			if ( Object->IsA(USequenceObject::StaticClass()) )	// @TB if the above loop is changed to <USequenceObject>, then this line isn't needed
			{
				// Check to see if any USequenceOp's which reference this variable need to change too
				for (TObjectIterator<USequenceOp> Itt; Itt; ++Itt)
				{
					USequenceOp* SequenceOp = *Itt;
					for (INT i = 0; i < SequenceOp->VariableLinks.Num(); i++)
					{
						FSeqVarLink& SeqVarLink = SequenceOp->VariableLinks(i);
						if ( SeqVarLink.LinkVar == OldName )
						{
//						debugfSlow( TEXT("Renaming %s to %s"), *SeqVarLink.LinkVar.ToString(), *NewName.ToString() );
							SeqVarLink.LinkVar = NewName;
						}
					}
				}
			}
			bFoundRef = TRUE;
		}
	}
	return bFoundRef;
}

/**
 * Ensures that the specified name can be used to create an object using this sequence as its Outer.  If any objects are found using
 * the specified name, they will be renamed.
 *
 * @param	InName			the name to search for
 * @param	RenameFlags		a bitmask of flags used to modify the behavior of a rename operation.
 *
 * @return	TRUE if at least one object was found and successfully renamed.
 */
UBOOL USequence::ClearNameUsage(FName InName, ERenameFlags RenameFlags/*=REN_None*/)
{
	UBOOL bResult = FALSE;

	// Make sure this name is unique within this sequence.
	USequenceObject* Found=NULL;
	if( InName != NAME_None )
	{
		Found = FindObject<USequenceObject>( this, *InName.ToString() );
	}

	// If there is already a SeqObj with this name, rename it.
	if( Found )
	{
		checkSlow(Found->GetTypedOuter<USequence>() == this);
		if ( Found->GetTypedOuter<USequence>() == this )
		{
			// For renaming subsequences, we want to use its current name (not the class name) as the basis for the new name as we actually display it.
			// We also want to keep the ObjName string the same as the new name.
			USequence* FoundSeq = Cast<USequence>(Found);
			if ( FoundSeq )
			{
				FName NewFoundSeqName = MakeUniqueSubsequenceName(this, FoundSeq);
				if ( FoundSeq->Rename(*NewFoundSeqName.ToString(), this, RenameFlags) )
				{
					FoundSeq->ObjName = NewFoundSeqName.ToString();
					bResult = TRUE;
				}
			}
			else
			{
				bResult = Found->Rename(NULL, NULL, RenameFlags);
			}
		}
	}
	
	// Check for other objects in the package which may have the same name in other levels
	return RenameAllObjectsInOtherLevels( InName, this->GetFName(), RenameFlags ) || bResult;
}

/**
 * Ensures that all external variables contained within TopSequence or any nested sequences have names which are unique throughout
 * the entire sequence tree.  Any external variables that have the same name will be renamed.
 *
 * @param	TopSequence		the outermost sequence to search in.  specify NULL to start at the top-most sequence.
 * @param	RenameFlags		a bitmask of flags used to modify the behavior of a rename operation.
 *
 * @return	TRUE if at least one object was found and successfully renamed.
 */
UBOOL USequence::ClearExternalVariableNameUsage( USequence* TopSequence, ERenameFlags RenameFlags/*=REN_None*/ )
{
	UBOOL bResult = FALSE;

	if ( TopSequence == NULL )
	{
		TopSequence = GetParentSequenceRoot(TRUE);
		if ( TopSequence == NULL )
		{
			TopSequence = this;
		}
		check(TopSequence != NULL);
		bResult = TopSequence->ClearExternalVariableNameUsage(TopSequence, RenameFlags);
	}
	else
	{
		TArray<USequence*> ChildSequences;
		if ( ContainsObjectOfClass(SequenceObjects, &ChildSequences) )
		{
			for ( INT SeqIdx = 0; SeqIdx < ChildSequences.Num(); SeqIdx++ )
			{
				// must pass 'this' as value for TopSequence so that we don't cause infinite recursion (due to the fact that we call 
				bResult = ChildSequences(SeqIdx)->ClearExternalVariableNameUsage(TopSequence, RenameFlags) || bResult;
			}
		}

		USequence* NextParentSequence = GetTypedOuter<USequence>();
		if ( NextParentSequence != NULL )
		{
			TArray<USeqVar_External*> ExternalVars;
			if ( ContainsObjectOfClass(SequenceObjects, &ExternalVars) )
			{
				for ( INT VarIdx = 0; VarIdx < ExternalVars.Num(); VarIdx++ )
				{
					USeqVar_External* ExtVar = ExternalVars(VarIdx);
					if ( NextParentSequence->ClearNameUsage(ExtVar->GetFName(), RenameFlags) )
					{
						bResult = TRUE;
					}
				}
			}
		}
	}

	return bResult;
}

/** Iterate over all SequenceObjects in this Sequence, making sure that their ParentSequence pointer points back to this Sequence. */
void USequence::CheckParentSequencePointers()
{
	FString ThisName;

	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		check(SequenceObjects(i));

		USequence* ParentSeq = SequenceObjects(i)->ParentSequence;
		if(ParentSeq != this)
		{
			FString ActualParent = ParentSeq->GetPathName();
			FString ObjectName = SequenceObjects(i)->GetPathName();
#if !FINAL_RELEASE && !NO_LOGGING
			if ( ThisName.Len() == 0 )
			{
				ThisName = GetPathName();
			}
			debugf( TEXT("ERROR! ParentSequence of '%s' is '%s' but should be '%s'"), *ObjectName, *ActualParent, *ThisName );
#endif
		}		

		// See if this is a USequence, and if so, recurse into it.
		USequence* Seq = Cast<USequence>( SequenceObjects(i) );
		if(Seq)
		{
			Seq->CheckParentSequencePointers();
		}
	}
}

/**
 * @return		The ULevel this sequence occurs in.
 */
ULevel* USequence::GetLevel() const
{
	return GetTypedOuter<ULevel>();
}

/**
 * Adds the specified SequenceOp to this sequence's list of ActiveOps.
 *
 * @param	NewSequenceOp	the sequence op to add to the list
 * @param	bPushTop		if TRUE, adds the operation to the top of stack (meaning it will be executed first),
 *							rather than the bottom
 *
 * @return	TRUE if the sequence operation was successfully added to the list.
 */
UBOOL USequence::QueueSequenceOp( USequenceOp* NewSequenceOp, UBOOL bPushTop/*=FALSE*/ )
{
	UBOOL bResult = FALSE;
	if ( NewSequenceOp != NULL )
	{
		// only insert if not already in the list
		if (!ActiveSequenceOps.ContainsItem(NewSequenceOp))
		{
			INT InsertIndex = bPushTop ? ActiveSequenceOps.Num() : 0;
			ActiveSequenceOps.InsertItem(NewSequenceOp,InsertIndex);
		}
		bResult = TRUE;
	}
	return bResult;
}

/**
 * Adds the specified SequenceOp to this sequence's list of DelayedActivatedOps.
 *
 * @param	NewSequenceOp	the sequence op to add to the list
 * @param	Link			the incoming link to NewSequenceOp
 * @param   ActivateDelay	the total delay before NewSequenceOp should be executed
 *
 * @return	TRUE if the sequence operation was successfully added to the list.
 */
UBOOL USequence::QueueDelayedSequenceOp( USequenceOp* NewSequenceOp, FSeqOpOutputInputLink* Link, FLOAT ActivateDelay )
{
	USequenceOp *LinkedOp = Link->LinkedOp;

	// see if this entry is already in the list
	if( NewSequenceOp != NULL && Link != NULL )
	{
		UBOOL bFoundExisting = FALSE;
		for( INT Idx = 0; Idx < DelayedActivatedOps.Num(); ++Idx )
		{
			FActivateOp &DelayedOp = DelayedActivatedOps(Idx);
			if( DelayedOp.Op == LinkedOp &&
				DelayedOp.InputIdx == Link->InputLinkIdx )
			{
				bFoundExisting = TRUE;
				// reset the delay
				DelayedOp.RemainingDelay = ActivateDelay;
				DelayedOp.ActivatorOp = this;
				return TRUE;
			}
		}
		// if not already in the list,
		if( !bFoundExisting )
		{
			// add to the list of delayed activation
			INT aIdx = DelayedActivatedOps.AddZeroed();
			DelayedActivatedOps(aIdx).ActivatorOp = NewSequenceOp;
			DelayedActivatedOps(aIdx).Op = LinkedOp;
			DelayedActivatedOps(aIdx).InputIdx = Link->InputLinkIdx;
			DelayedActivatedOps(aIdx).RemainingDelay = ActivateDelay;
			return TRUE;
		}
	}
	return FALSE;
}


/**
 * Adds a new SequenceObject to this sequence's list of ops
 *
 * @param	NewObj		the sequence object to add.
 * @param	bRecurse	if TRUE, recursively add any sequence objects attached to this one
 *
 * @return	TRUE if the object was successfully added to the sequence.
 *
 */
UBOOL USequence::AddSequenceObject( USequenceObject* NewObj, UBOOL bRecurse/*=FALSE*/ )
{
	UBOOL bResult = FALSE;
	if ( NewObj != NULL )
	{
		NewObj->Modify();

		//@caution - we should probably check to make sure this object isn't part of another sequence already (i.e. has a ParentSequence
		// that isn't this one)
		if ( !SequenceObjects.ContainsItem(NewObj) )
		{
			// only mark the package dirty when outside of an undo transaction if the object is not marked transient
			UBOOL bObjectWillBeSaved = !NewObj->HasAnyFlags(RF_Transient);
			Modify( bObjectWillBeSaved );
			SequenceObjects.AddItem(NewObj);

			if ( bRecurse )
			{
				USequenceOp* NewOp = Cast<USequenceOp>(NewObj);
				if ( NewOp != NULL )
				{
					TArray<USequenceObject*> OpsToAdd;
					NewOp->GetLinkedObjects(OpsToAdd,NULL,TRUE);
					for ( INT OpIndex = 0; OpIndex < OpsToAdd.Num(); OpIndex++ )
					{
						USequenceObject* InnerOp = OpsToAdd(OpIndex);

						// don't recurse because we passed TRUE to GetLinkedObject for its bRecurse variable
						AddSequenceObject(InnerOp, FALSE);
					}
				}					
			}
		}

		NewObj->ParentSequence = this;
		bResult = TRUE;
	}

	return bResult;
}


/**
 * Removes the specified object from the SequenceObjects array, severing any links to that object.
 *
 * @param	ObjectToRemove	the SequenceObject to remove from this sequence.  All links to the object will be cleared.
 * @param	ModifiedObjects	a list of objects that have been modified the objects that have been
 */
void USequence::RemoveObject( USequenceObject* ObjectToRemove )
{
	INT ObjectLocation = SequenceObjects.FindItemIndex(ObjectToRemove);
	if ( ObjectLocation != INDEX_NONE && ObjectToRemove->IsDeletable() )
	{
		Modify(TRUE);
		ObjectToRemove->OnDelete();
		SequenceObjects.Remove( ObjectLocation );

		ObjectToRemove->Modify(TRUE);

		// if the object's ParentSequence reference is still pointing to us, clear it
		if ( ObjectToRemove->ParentSequence == this )
		{
			ObjectToRemove->ParentSequence = NULL;
		}

		// If this is a sequence op then we check all other ops inputs to see if any reference to it.
		USequenceOp* SeqOp = Cast<USequenceOp>(ObjectToRemove);
		if (SeqOp != NULL)
		{
			USequenceEvent* SeqEvt = Cast<USequenceEvent>( ObjectToRemove );
			if(SeqEvt)
			{
				// search for any ops that have a link to this event
				for (INT idx = 0; idx < SequenceObjects.Num(); idx++)
				{
					USequenceOp *ChkOp = Cast<USequenceOp>(SequenceObjects(idx));
					if (ChkOp != NULL &&
						ChkOp->EventLinks.Num() > 0)
					{
						for (INT eventIdx = 0; eventIdx < ChkOp->EventLinks.Num(); eventIdx++)
						{
							for (INT chkIdx = 0; chkIdx < ChkOp->EventLinks(eventIdx).LinkedEvents.Num(); chkIdx++)
							{
								if (ChkOp->EventLinks(eventIdx).LinkedEvents(chkIdx) == SeqEvt)
								{
									ChkOp->Modify(TRUE);

									ChkOp->EventLinks(eventIdx).LinkedEvents.Remove(chkIdx--,1);
								}
							}
						}
					}
				}

				UnregisteredEvents.RemoveItem(SeqEvt);
			}
			else
			{
				USequence* SubSequence = Cast<USequence>(ObjectToRemove);
				if ( SubSequence != NULL )
				{
					NestedSequences.RemoveItem(SubSequence);
				}
			}
			// iterate thraough all other objects, looking for output links that point to this op
			for (INT chkIdx = 0; chkIdx < SequenceObjects.Num(); chkIdx++)
			{
				// if it is a sequence op,
				USequenceOp *ChkOp = Cast<USequenceOp>(SequenceObjects(chkIdx));
				if (ChkOp != NULL && ChkOp != SeqOp)
				{
					// iterate through this op's output links
					for (INT linkIdx = 0; linkIdx < ChkOp->OutputLinks.Num(); linkIdx++)
					{
						// iterate through all the inputs linked to this output
						for (INT inputIdx = 0; inputIdx < ChkOp->OutputLinks(linkIdx).Links.Num(); inputIdx++)
						{
							if (ChkOp->OutputLinks(linkIdx).Links(inputIdx).LinkedOp == SeqOp)
							{
								ChkOp->Modify(TRUE);

								// remove the entry
								ChkOp->OutputLinks(linkIdx).Links.Remove(inputIdx--,1);
							}
						}
					}
				}
			}
			// clear the refs in this op, so that none are left dangling
			SeqOp->InputLinks.Empty();
			SeqOp->OutputLinks.Empty();
			SeqOp->VariableLinks.Empty();
			ActiveSequenceOps.RemoveItem(SeqOp);
		}
		else
		{
			// If we are removing a variable - we must look through all Ops to see if there are any references to it and clear them.
			USequenceVariable* SeqVar = Cast<USequenceVariable>( ObjectToRemove );
			if(SeqVar)
			{
				for(INT ChkIdx=0; ChkIdx<SequenceObjects.Num(); ChkIdx++)
				{
					USequenceOp* ChkOp = Cast<USequenceOp>( SequenceObjects(ChkIdx) );
					if(ChkOp)
					{
						for(INT ChkLnkIdx=0; ChkLnkIdx < ChkOp->VariableLinks.Num(); ChkLnkIdx++)
						{
							for (INT VarIdx = 0; VarIdx < ChkOp->VariableLinks(ChkLnkIdx).LinkedVariables.Num(); VarIdx++)
							{
								if (ChkOp->VariableLinks(ChkLnkIdx).LinkedVariables(VarIdx) == SeqVar)
								{
									ChkOp->Modify(TRUE);

									// Remove from list
#if WITH_EDITOR
									ChkOp->OnVariableDisconnect(SeqVar,ChkLnkIdx);
#endif
									ChkOp->VariableLinks(ChkLnkIdx).LinkedVariables.Remove(VarIdx--,1);
								}
							}
						}
					}
				}
			}
		}

		if( !GIsGame && (GUglyHackFlags&HACK_KeepSequenceObject) == 0 )
		{
			// Mark the sequence object as pending kill so that it won't be included in any following copy/paste actions
			// via UExporter::ExportToOutputDevice -> ExportObjectInner
			ObjectToRemove->MarkPendingKill();
		}
	}
}

/**
 * Removes the specified objects from this Sequence's SequenceObjects array, severing any links to these objects.
 *
 * @param	InObjects	the sequence objects to remove from this sequence.  All links to these objects will be cleared,
 *						and the objects will be removed from all SequenceObject arrays.
 */
void USequence::RemoveObjects(const TArray<USequenceObject*>& InObjects)
{
	for(INT ObjIdx=0; ObjIdx<InObjects.Num(); ObjIdx++)
	{
		RemoveObject(InObjects(ObjIdx));
	}
}

/** Force all named variables, remote actions, and remote events to update status in this sequence. */
void USequence::UpdateNamedVarStatus()
{
	for (INT Idx = 0; Idx < SequenceObjects.Num(); Idx++)
	{
		USequenceObject* Obj = SequenceObjects(Idx);
		if( Obj != NULL )
		{
			Obj->UpdateStatus();
		}
	}
}

/** Iterate over all SeqAct_Interp (Matinee) actions calling UpdateConnectorsFromData on them, to ensure they are up to date. */
void USequence::UpdateInterpActionConnectors()
{
	TArray<USequenceObject*> MatineeActions;
	FindSeqObjectsByClass( USeqAct_Interp::StaticClass(), MatineeActions );

	for(INT i=0; i<MatineeActions.Num(); i++)
	{
		USeqAct_Interp* TestAction = CastChecked<USeqAct_Interp>( MatineeActions(i) );
		check(TestAction);

		TestAction->UpdateConnectorsFromData();
	}
}

/** 
 * Determine if this sequence (or any of its subsequences) references a certain object.
 *
 * @param	InObject	the object to search for references to
 * @param	pReferencer	if specified, will be set to the SequenceObject that is referencing the search object.
 *
 * @return TRUE if this sequence references the specified object.
 */
UBOOL USequence::ReferencesObject( const UObject* InObject, USequenceObject** pReferencer/*=NULL*/ ) const
{
	// Return 'false' if NULL Object passed in.
	if ( InObject == NULL )
	{
		return FALSE;
	}

	USequenceObject* Referencer = NULL;

	// Iterate over Objects in this Sequence
	for ( INT OpIndex = 0; OpIndex < SequenceObjects.Num(); OpIndex++ )
	{
		USequenceOp* SeqOp = Cast<USequenceOp>(SequenceObjects(OpIndex));
		if ( SeqOp != NULL )
		{
			// If its a SequenceEvent, check the Originator.
			USequenceEvent* Event = Cast<USequenceEvent>( SeqOp );
			if( Event != NULL )
			{
				if(Event->Originator == InObject)
				{
					Referencer = Event;
					break;
				}
			}

			// If this is a subSequence, check it for the given Object. If we find it, return true.
			else
			{
				USequence* SubSeq = Cast<USequence>( SeqOp );
				if( SubSeq && SubSeq->ReferencesObject(InObject, &Referencer) )
				{
					break;
				}
			}
		}
		else
		{
			USequenceVariable* SeqVar = Cast<USequenceVariable>(SequenceObjects(OpIndex));
			if ( SeqVar != NULL )
			{
				INT RefIndex=0;
				UObject** ObjValue = SeqVar->GetObjectRef(RefIndex++);
				while ( ObjValue != NULL )
				{
					if ( InObject == *ObjValue )
					{
						Referencer = SeqVar;
						break;
					}
					ObjValue = SeqVar->GetObjectRef(RefIndex++);
				}

				if ( Referencer != NULL )
				{
					break;
				}
			}
		}
	}

	if ( pReferencer != NULL )
	{
		*pReferencer = Referencer;
	}

	return Referencer != NULL;
}

/**
 * Determines whether the specified SequenceObject is contained in the SequenceObjects array of this sequence.
 *
 * @param	SearchObject	the sequence object to look for
 * @param	bRecursive		specify FALSE to limit the search to this sequence only (do not search in sub-sequences as well)
 *
 * @return	TRUE if the specified sequence object was found in the SequenceObjects array of this sequence or one of its sub-sequences
 */
UBOOL USequence::ContainsSequenceObject( USequenceObject* SearchObject, UBOOL bRecursive/*=TRUE*/ ) const
{
	check(SearchObject);

	UBOOL bResult = SequenceObjects.ContainsItem(SearchObject);

	if ( !bResult && bRecursive )
	{
		TArray<USequence*> Subsequences;
		FindSeqObjectsByClass(USequence::StaticClass(), (TArray<USequenceObject*>&)Subsequences, FALSE);

		for ( INT SeqIndex = 0; SeqIndex < Subsequences.Num(); SeqIndex++ )
		{
			if ( Subsequences(SeqIndex)->ContainsSequenceObject(SearchObject, bRecursive) )
			{
				bResult = TRUE;
				break;
			}
		}
	}

	return bResult;
}

void USequence::execFindSeqObjectsByClass(FFrame &Stack,RESULT_DECL)
{
	P_GET_OBJECT(UClass, DesiredClass);
	P_GET_UBOOL(bRecursive);
	P_GET_TARRAY_REF(USequenceObject*,OutputObjects);
	P_FINISH;

	check( DesiredClass->IsChildOf(USequenceObject::StaticClass()) );

	FindSeqObjectsByClass(DesiredClass, OutputObjects, bRecursive);
}

void USequence::execFindSeqObjectsByName(FFrame &Stack,RESULT_DECL)
{
	P_GET_STR(ObjName);
	P_GET_UBOOL(bCheckComments);
	P_GET_TARRAY_REF(USequenceObject*,OutputObjects);
	P_GET_UBOOL(bRecursive);
	P_FINISH;

	FindSeqObjectsByName(ObjName, bCheckComments, OutputObjects, bRecursive);
}


//==========================
// USequenceEvent interface

/**
 * Adds an error message to the map check dialog if this SequenceEvent's EventActivator is bStatic
 */
#if WITH_EDITOR
void USequenceEvent::CheckForErrors()
{
	Super::CheckForErrors();

#if ENABLE_MAPCHECK_FOR_STATIC_ACTORS
	if ( GWarn != NULL && GWarn->MapCheck_IsActive() )
	{
		if ( Originator != NULL && Originator->bStatic )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, Originator, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KismetStaticActorRef" ), *GetFullName() ) ), TEXT( "KismetStaticActorRef" ), MCGROUP_KISMET );
		}
	}
#endif
}
#endif

/**
 * Adds this Event to the appropriate Object at runtime.
 */
UBOOL USequenceEvent::RegisterEvent()
{
	if ( Originator != NULL && !Originator->IsPendingKill() )
	{
		KISMET_LOG(TEXT("Event %s registering with actor %s"),*GetName(),*Originator->GetName());
		Originator->GeneratedEvents.AddUniqueItem(this);
	}

	eventRegisterEvent();
	bRegistered = TRUE;

	return bRegistered;
}


void USequenceEvent::DebugActivateEvent(AActor *InOriginator, AActor *InInstigator, TArray<INT> *ActivateIndices)
{
	ActivateEvent( InOriginator, InInstigator, ActivateIndices );
}


/**
 * Causes the Event to become active, filling out any associated variables, etc.
 */
void USequenceEvent::ActivateEvent(AActor *InOriginator, AActor *InInstigator, TArray<INT> *ActivateIndices, UBOOL bPushTop/*=FALSE*/, UBOOL bFromQueued)
{
	// fill in any properties for this Event
	Originator = InOriginator;
	Instigator = InInstigator;

	// if this isn't a queued activation
	if (!bFromQueued)
	{
		KISMET_LOG(TEXT("- Event %s activated with originator %s, instigator %s"),*GetName(),InOriginator!=NULL?*InOriginator->GetName():TEXT("NULL"),InInstigator!=NULL?*InInstigator->GetName():TEXT("NULL"));
		// note the actual activation time
		ActivationTime = GWorld->GetTimeSeconds();
		// increment the trigger count
		TriggerCount++;
	}

	// if we're already active then queue this activation
	if (bActive && ParentSequence != NULL)
	{
		KISMET_LOG(TEXT("- queuing activation"));
		INT Idx = ParentSequence->QueuedActivations.AddZeroed();
		ParentSequence->QueuedActivations(Idx).ActivatedEvent = this;
		ParentSequence->QueuedActivations(Idx).InOriginator = InOriginator;
		ParentSequence->QueuedActivations(Idx).InInstigator = InInstigator;
		ParentSequence->QueuedActivations(Idx).bPushTop = bPushTop;
		// copy over the indices if specified
		if (ActivateIndices != NULL)
		{
			for (INT ActivateIndex = 0; ActivateIndex < ActivateIndices->Num(); ActivateIndex++)
			{
				ParentSequence->QueuedActivations(Idx).ActivateIndices.AddItem((*ActivateIndices)(ActivateIndex));
			}
		}
	}
	else if( ParentSequence != NULL )
	{
		// activate this Event
		bActive = TRUE;
		Activated();
		eventActivated();

#if !CONSOLE && WITH_EDITOR
		PIEActivationTime = GWorld->GetTimeSeconds();
		ActivateCount++;
#endif

		InitializeLinkedVariableValues();
		PopulateLinkedVariableValues();

		// if specific indices were supplied,
		if (ActivateIndices != NULL)
		{
			for (INT Idx = 0; Idx < ActivateIndices->Num(); Idx++)
			{
				INT OutputIdx = (*ActivateIndices)(Idx);
				if ( OutputLinks.IsValidIndex(OutputIdx) )
				{
					OutputLinks(OutputIdx).ActivateOutputLink();
				}
			}
		}
		else
		{
			// activate all output links by default
			for (INT Idx = 0; Idx < OutputLinks.Num(); Idx++)
			{
				OutputLinks(Idx).ActivateOutputLink();
			}
		}
		// check if we should log the object comment to the screen
		if (GAreScreenMessagesEnabled && (GEngine->bOnScreenKismetWarnings) && (bOutputObjCommentToScreen))
		{
			// iterate through the controller list
			for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
			{
				// if it's a player
				if (Controller->IsA(APlayerController::StaticClass()))
				{
					((APlayerController*)Controller)->eventClientMessage(ObjComment,NAME_None);
				}
			}
		}

		// add to the sequence's list of active ops
		ParentSequence->QueueSequenceOp(this, bPushTop);
	}
}

/**
 * Checks if this Event could be activated, and if bTest == false
 * then the Event will be activated with the specified actor as the
 * instigator.
 */
UBOOL USequenceEvent::CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest, TArray<INT>* ActivateIndices, UBOOL bPushTop)
{
	UBOOL bActivated = FALSE;
	if ( (bClientSideOnly ? GWorld->GetWorldInfo()->NetMode != NM_DedicatedServer : GWorld->GetWorldInfo()->NetMode != NM_Client) &&
		GWorld->HasBegunPlay() && !IsPendingKill() && (ParentSequence == NULL || ParentSequence->IsEnabled()) )
	{
		KISMET_LOG(TEXT("%s base check activate, %s/%s, triggercount: %d/%d, test: %s"),*GetName(),InOriginator!=NULL?*InOriginator->GetName():TEXT("NULL"),InInstigator!=NULL?*InInstigator->GetName():TEXT("NULL"),TriggerCount,MaxTriggerCount,bTest?TEXT("yes"):TEXT("no"));
		// if passed a valid actor,
		// and meets player requirement
		// and match max trigger count condition
		// and retrigger delay condition
		if (InOriginator != NULL &&
			(!bPlayerOnly ||
			 (InInstigator && InInstigator->IsPlayerOwned())) &&
			(MaxTriggerCount == 0 ||
			 TriggerCount < MaxTriggerCount) &&
			(ReTriggerDelay == 0.f ||
			 TriggerCount == 0 ||
			 (GWorld->GetTimeSeconds() - ActivationTime) > ReTriggerDelay))
		{
			// if not testing, and enabled
			if (!bTest &&
				bEnabled)
			{
				// activate the Event
				ActivateEvent(InOriginator, InInstigator, ActivateIndices, bPushTop);
			}
			// note that this check met all activation requirements
			bActivated = TRUE;
		}
	}
#if !FINAL_RELEASE
	else
	{
		KISMET_LOG(TEXT("%s failed activate, %d %d %d %d"),*GetName(),(INT)(bClientSideOnly ? GWorld->IsClient() : GWorld->IsServer()),(INT)GWorld->HasBegunPlay(),(INT)!IsPendingKill(),(INT)(ParentSequence == NULL || ParentSequence->IsEnabled()));
	}
#endif
	return bActivated;
}

/**
 * Fills in the value of the "Instigator" VariableLink
 */
void USequenceEvent::InitializeLinkedVariableValues()
{
	Super::InitializeLinkedVariableValues();

	// see if any instigator variables are attached
	TArray<UObject**> ObjVars;
	GetObjectVars(ObjVars,TEXT("Instigator"));
	for (INT Idx = 0; Idx < ObjVars.Num(); Idx++)
	{
		*(ObjVars(Idx)) = Instigator;
	}
}

/**
 * Script handler for USequenceEvent::CheckActivate.
 */
void USequenceEvent::execCheckActivate(FFrame &Stack, RESULT_DECL)
{
	P_GET_ACTOR(InOriginator);
	P_GET_ACTOR(InInstigator);
	P_GET_UBOOL_OPTX(bTest,FALSE);
	P_GET_TARRAY_REF(INT,ActivateIndices); // optional, pActivateIndices will be NULL if unspecified
	P_GET_UBOOL_OPTX(bPushTop,FALSE);
	P_FINISH;

	// pass NULL for indices if empty array is specified, which is needed because if this is called from a helper function that also takes an optional argument for the indices,
	// then that function will always have an array to pass in, even if that argument is skipped when calling it
	*(UBOOL*)Result = CheckActivate(InOriginator, InInstigator, bTest, (pActivateIndices != NULL && pActivateIndices->Num() > 0) ? pActivateIndices : NULL, bPushTop);
}

//==========================
// USeqEvent_Touch interface

/**
 * Overridden to provide basic activation checks for CheckTouchActivate/CheckUnTouchActivate.
 * NOTE: This will *NOT* activate the Event, use the other activation functions instead.
 */
UBOOL USeqEvent_Touch::CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest, TArray<INT>* ActivateIndices, UBOOL bPushTop)
{
	KISMET_LOG(TEXT("Touch event %s check activate %s/%s %d"),*GetName(),*InOriginator->GetName(),*InInstigator->GetName(),bTest);
	// check class proximity types, accept any if no proximity types defined
	UBOOL bFoundIgnoredClass = FALSE;
	UBOOL bPassed = FALSE;
	if (bEnabled &&
		InInstigator != NULL)
	{
		// check for Ignored classes
		for( INT IgnoredIdx = 0; IgnoredIdx < IgnoredClassProximityTypes.Num(); ++IgnoredIdx )
		{
			if(InInstigator->IsA(IgnoredClassProximityTypes(IgnoredIdx)))
			{
				bFoundIgnoredClass = TRUE;
				break;
			}
		}

		if( bFoundIgnoredClass == FALSE )
		{
			if (ClassProximityTypes.Num() > 0)
			{
				for (INT Idx = 0; Idx < ClassProximityTypes.Num() && !bPassed; Idx++)
				{
					if (InInstigator->IsA(ClassProximityTypes(Idx)))
					{
						bPassed = TRUE;
					}
				}
			}
			else
			{
				bPassed = TRUE;
			}
		}

		if (bPassed)
		{
			// check the base activation parameters, test only
			bPassed = Super::CheckActivate(InOriginator,InInstigator,TRUE,ActivateIndices,bPushTop);
		}
	}
	return bPassed;
}

void USeqEvent_Touch::execCheckTouchActivate(FFrame &Stack,RESULT_DECL)
{
	P_GET_ACTOR(InOriginator);
	P_GET_ACTOR(InInstigator);
	P_GET_UBOOL_OPTX(bTest,FALSE);
	P_FINISH;
	*(UBOOL*)Result = CheckTouchActivate(InOriginator,InInstigator,bTest);
}

void USeqEvent_Touch::execCheckUnTouchActivate(FFrame& Stack, RESULT_DECL)
{
	P_GET_ACTOR(InOriginator);
	P_GET_ACTOR(InInstigator);
	P_GET_UBOOL_OPTX(bTest,FALSE);
	P_FINISH;
	*(UBOOL*)Result = CheckUnTouchActivate(InOriginator, InInstigator, bTest);
}

void USeqEvent_Touch::DoTouchActivation(AActor *InOriginator, AActor *InInstigator)
{
	// activate the event, first output link
	TArray<INT> ActivateIndices;
	ActivateIndices.AddItem(0);
	ActivateEvent(InOriginator,InInstigator,&ActivateIndices);
	// add to the touch list for untouch activations
	TouchedList.AddItem(InInstigator);
}

void USeqEvent_Touch::DoUnTouchActivation(AActor *InOriginator, AActor *InInstigator, INT TouchIdx)
{
	// Remove from the touched list
	TouchedList.Remove(TouchIdx,1);

	// activate the event, second output link
	TArray<INT> ActivateIndices;
	ActivateIndices.AddItem(1);
	if( TouchedList.Num() == 0 )
	{
		ActivateIndices.AddItem(2);
	}
	ActivateEvent(InOriginator,InInstigator,&ActivateIndices);
}

/**
 * Activation handler for touch events, checks base activation requirements and then adds the
 * instigator to TouchedList for any untouch activations.
 */

UBOOL USeqEvent_Touch::CheckTouchActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest)
{
	KISMET_LOG(TEXT("Touch event %s check touch activate %s/%s %d"),*GetName(),*InOriginator->GetName(),*InInstigator->GetName(),bTest);

	// See if we're tracking the instigator, not the actual actor that caused the touch event.
	if( bUseInstigator )
	{
		AProjectile *Proj = Cast<AProjectile>(InInstigator);
		if( Proj && Proj->Instigator )
		{
			InInstigator = Proj->Instigator;
		}
	}
	// reject dead pawns if requested
	if (!bAllowDeadPawns && InInstigator != NULL)
	{
		APawn* P = InInstigator->GetAPawn();
		if (P != NULL && P->Health <= 0 && (P->Controller == NULL || P->Controller->bDeleteMe))
		{
			return FALSE;
		}
	}

	// if the base activation conditions are met,
	if (CheckActivate(InOriginator,InInstigator,bTest) &&
		(!bForceOverlapping || InInstigator->IsOverlapping(InOriginator)))
	{
		if (!bTest)
		{
			DoTouchActivation(InOriginator,InInstigator);
		}
		return TRUE;
	}
	return FALSE;
}

/**
 * Activation handler for untouch events, checks base activation requirements as well as making sure
 * the instigator is in TouchedList.
 */
UBOOL USeqEvent_Touch::CheckUnTouchActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest)
{
	KISMET_LOG(TEXT("Touch event %s check untouch activate %s/%s %d"),*GetName(),*InOriginator->GetName(),*InInstigator->GetName(),bTest);

	// See if we're tracking the instigator, not the actual actor that caused the touch event.
	if( bUseInstigator )
	{
		AProjectile *Proj = Cast<AProjectile>(InInstigator);
		if( Proj && Proj->Instigator )
		{
			InInstigator = Proj->Instigator;
		}
	}

	UBOOL bActivated = FALSE;
	INT TouchIdx = -1;
	if (TouchedList.FindItem(InInstigator, TouchIdx))
	{
		// temporarily disable retriggerdelay since touch may of cause it to be set in the same frame
		FLOAT OldActivationTime = ActivationTime;
		ActivationTime = 0.f;
		// temporarily allow non-players so that we don't get mismatches when a player enters and then gets killed, enters a vehicle, etc
		// because in that case they may no longer have a Controller (and thus not be a player) by the time this gets called
		UBOOL bOldPlayerOnly = bPlayerOnly;
		bPlayerOnly = FALSE;
		// now check for activation
		bActivated = CheckActivate(InOriginator,InInstigator,bTest);
		// reset temporarily overridden values
		ActivationTime = OldActivationTime;
		bPlayerOnly = bOldPlayerOnly;
		// if the base activation conditions are met,
		if (bActivated && !bTest)
		{
			DoUnTouchActivation(InOriginator,InInstigator,TouchIdx);
		}
	}
	
	return bActivated;
}

//==========================
// USequenceAction interface

/**
 * Converts a sequence action class name into a handle function, so for example "SeqAct_ActionName"
 * would return "OnActionName".
 */
static FName GetHandlerName(USequenceAction *inAction)
{
	FName handlerName = NAME_None;
	FString actionName = inAction->GetClass()->GetName();
	INT splitIdx = actionName.InStr(TEXT("_"));
	if (splitIdx != -1)
	{
		INT actionHandlers = 0;		//!!debug
		// build the handler func name "OnThisAction"
		actionName = FString::Printf(TEXT("On%s"),
									 *actionName.Mid(splitIdx+1,actionName.Len()));
		handlerName = FName(*actionName);
	}
	return handlerName;
}

struct FActionHandler_Parms
{
	UObject*		Action;
};

/**
 * Default sequence action implementation constructs a function name using the
 * class name, and then attempts to call it on all Objects linked to the "Target"
 * variable link.
 */
void USequenceAction::Activated()
{
	checkf(!HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetFullName());
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints
	InitializeLinkedVariableValues();

	if (bCallHandler)
	{
		// split apart the action name,
		if (HandlerName == NAME_None)
		{
			HandlerName = GetHandlerName(this);
		}
		if (HandlerName != NAME_None)
		{
			// find out whether we're going to modify the targets - if so, ask the object whether that's allowed before calling the handler
			UBOOL bModifiesObject = FALSE;
			for (INT i = 0; i < VariableLinks.Num(); i++)
			{
				static FName NAME_Targets = FName(TEXT("Targets"));
				if (VariableLinks(i).bModifiesLinkedObject && VariableLinks(i).PropertyName == NAME_Targets)
				{
					bModifiesObject = TRUE;
					break;
				}
			}

			// for each Object variable associated with this action
			for (INT Idx = 0; Idx < Targets.Num(); Idx++)
			{
				UObject *Obj = Targets(Idx);
				if (Obj != NULL && !Obj->IsPendingKill())
				{
					// look up the matching function
					UFunction *HandlerFunction = Obj->FindFunction(HandlerName);
					if (HandlerFunction == NULL)
					{
						// attempt to redirect between pawn/controller
						if (Obj->IsA(APawn::StaticClass()) &&
							((APawn*)Obj)->Controller != NULL)
						{
							Obj = ((APawn*)Obj)->Controller;
							HandlerFunction = Obj->FindFunction(HandlerName);
						}
						else
						if (Obj->IsA(AController::StaticClass()) &&
							((AController*)Obj)->Pawn != NULL)
						{
							Obj = ((AController*)Obj)->Pawn;
							HandlerFunction = Obj->FindFunction(HandlerName);
						}
					}
					if (HandlerFunction != NULL && !Obj->IsPendingKill())
					{
						// verify that the function has the correct number of parameters
						if (HandlerFunction->NumParms != 1)
						{
							KISMET_WARN(TEXT("Object %s has a function named %s, but it either has a return value or doesn't have exactly one parameter"), *Obj->GetName(), *HandlerName.ToString());
						}
						// and the correct parameter type
						else if (Cast<UObjectProperty>(HandlerFunction->PropertyLink, CLASS_IsAUObjectProperty) == NULL ||
								 !GetClass()->IsChildOf(((UObjectProperty*)HandlerFunction->PropertyLink)->PropertyClass))
						{
							KISMET_WARN(TEXT("Object %s has a function named %s, but the parameter is not of the correct type (should be of class %s or a base class)"), *Obj->GetName(), *HandlerName.ToString(), *GetClass()->GetName());
						}
						else
						{
							FString Reason;
							if (bModifiesObject && Obj->IsA(AActor::StaticClass()) && !((AActor*)Obj)->SupportsKismetModification(this, Reason))
							{
								KISMET_WARN(TEXT("Object '%s' rejected Kismet action '%s' : %s"), *Obj->GetName(), *ObjName, *Reason);
							}
							else
							{
								KISMET_LOG(TEXT("--> Calling handler %s on actor %s"),*HandlerFunction->GetName(),*Obj->GetName());
								// perform any pre-handle logic
								if (Obj->IsA(AActor::StaticClass()))
								{
									PreActorHandle((AActor*)Obj);
								}
								// call the actual function, with a Pointer to this Object as the only param
								FActionHandler_Parms HandlerFunctionParms;
								HandlerFunctionParms.Action = this;
								Obj->ProcessEvent(HandlerFunction,&HandlerFunctionParms);
							}
						}
					}
					else
					{
						KISMET_WARN(TEXT("Obj %s has no handler for %s"),*Obj->GetName(),*GetName());
						warnf(TEXT("Obj %s has no handler for %s"),*Obj->GetName(),*GetName());
					}
				}
			}
		}
		else
		{
			KISMET_WARN(TEXT("Unable to determine action name for %s"),*GetName());
		}
	}
}

//==========================
// USeqAct_Latent interface

void USeqAct_Latent::Activated()
{
	bAborted = FALSE;
	Super::Activated();
	// if no actors handled this action then abort
	if (LatentActors.Num() == 0)
	{
		bAborted = TRUE;
	}
}

/**
 * Overridden to store a reference to the targeted actor.
 */
void USeqAct_Latent::PreActorHandle(AActor *inActor)
{
	if (inActor != NULL)
	{
		KISMET_LOG(TEXT("--> Attaching Latent action %s to actor %s"),*GetName(),*inActor->GetName());
		LatentActors.AddItem(inActor);
		inActor->LatentActions.AddItem(this);
	}
}

void USeqAct_Latent::AbortFor(AActor* LatentActor)
{
	// make sure the actor exists in our list
	check(LatentActor != NULL && "Trying abort Latent action with a NULL actor");
	if (!bAborted)
	{
		UBOOL bFoundEntry = FALSE;
		KISMET_LOG(TEXT("%s attempt abort by %s"),*GetName(),*LatentActor->GetName());
		for (INT Idx = 0; Idx < LatentActors.Num() && !bFoundEntry; Idx++)
		{
			if (LatentActors(Idx) == LatentActor)
			{
				bFoundEntry = TRUE;
			}
		}
		if (bFoundEntry)
		{
			KISMET_LOG(TEXT("%s aborted by %s"),*GetName(),*LatentActor->GetName());
			bAborted = TRUE;
		}
	}
}

/**
 * Checks to see if all actors associated with this action
 * have either been destroyed or have finished the latent action.
 */
UBOOL USeqAct_Latent::UpdateOp(FLOAT DeltaTime)
{										
	// check to see if this has been aborted
	if (bAborted)
	{
		// clear the Latent actors list
		LatentActors.Empty();
	}
	else
	{
		// iterate through the Latent actors list,
		for (INT Idx = 0; Idx < LatentActors.Num(); Idx++)
		{
			AActor *Actor = LatentActors(Idx);
			// if the actor is invalid or no longer is referencing this action
			if (Actor == NULL || Actor->IsPendingKill() || !Actor->LatentActions.ContainsItem(this))
			{
				// remove the actor from the latent list
				LatentActors.Remove(Idx--,1);
			}
		}
	}
	// return true when our Latentactors list is empty, to indicate all have finished processing
	return (!eventUpdate(DeltaTime) && LatentActors.Num() == 0);
}

void USeqAct_Latent::DeActivated()
{
	// if aborted then activate second Link, otherwise default Link
	if(OutputLinks.Num() > 0)
	{
		INT LinkIdx = ((bAborted && OutputLinks.Num() > 1) ? 1 : 0);
		OutputLinks(LinkIdx).ActivateOutputLink();
	}
	bAborted = FALSE;
}

//==========================
// USeqAct_Gate interface

void USeqAct_Gate::PostLoad()
{
	//Initialize the gate count
	CurrentCloseCount = AutoCloseCount;
	Super::PostLoad();
}

//==========================
// USeqAct_Toggle interface

void USeqAct_Toggle::PostLoad()
{
	Super::PostLoad();

	// fix up bModifiesLinkedObject in old objects
	if (GIsEditor && !IsTemplate(RF_ClassDefaultObject) && VariableLinks.Num() > 0)
	{
		USeqAct_Toggle* Default = GetClass()->GetDefaultObject<USeqAct_Toggle>();
		if (Default->VariableLinks.Num() > 0 && Default->VariableLinks(0).bModifiesLinkedObject)
		{
			VariableLinks(0).bModifiesLinkedObject = TRUE;
		}
	}
}

/**
 * Overriden to handle bools/events, and then forwards to the
 * normal action calling for attached actors.
 */
void USeqAct_Toggle::Activated()
{
	// for each Object variable associated with this action
	TArray<UBOOL*> boolVars;
	GetBoolVars(boolVars,TEXT("Bool"));
	for (INT VarIdx = 0; VarIdx < boolVars.Num(); VarIdx++)
	{
		UBOOL *boolValue = boolVars(VarIdx);
		if (boolValue != NULL)
		{
			// determine the new value for the variable
			if (InputLinks(0).bHasImpulse)
			{
				*boolValue = TRUE;
			}
			else
			if (InputLinks(1).bHasImpulse)
			{
				*boolValue = FALSE;
			}
			else
			if (InputLinks(2).bHasImpulse)
			{
				*boolValue = !(*boolValue);
			}
		}
	}
	// get a list of Events
	for (INT Idx = 0; Idx < EventLinks(0).LinkedEvents.Num(); Idx++)
	{
		USequenceEvent *Event = EventLinks(0).LinkedEvents(Idx);
		KISMET_LOG(TEXT("---> Event %s at %d"),Event!=NULL?*Event->GetName():TEXT("NULL"),Idx);
		if (Event != NULL)
		{
			if (InputLinks(0).bHasImpulse)
			{
				KISMET_LOG(TEXT("----> enabled"));
				Event->bEnabled = TRUE;
			}
			else
			if (InputLinks(1).bHasImpulse)
			{
				KISMET_LOG(TEXT("----> disabled"));
				Event->bEnabled = FALSE;
			}
			else
			if (InputLinks(2).bHasImpulse)
			{
				Event->bEnabled = !(Event->bEnabled);
				KISMET_LOG(TEXT("----> toggled, status? %s"),Event->bEnabled?TEXT("Enabled"):TEXT("Disabled"));
			}
			// relay the toggle to any duplicate events
			for (INT EvtIdx = 0; EvtIdx < Event->DuplicateEvts.Num(); EvtIdx++)
			{
				USequenceEvent *DupEvt = Event->DuplicateEvts(EvtIdx);
				if (DupEvt != NULL)
				{
					DupEvt->bEnabled = Event->bEnabled;
					DupEvt->eventToggled();
				}
			}
			Event->eventToggled();
		}
	}
	// perform normal action activation
	Super::Activated();
}

//==========================
// USequenceVariable

/** If we changed VarName, update NamedVar status globally. */
void USequenceVariable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("VarName")) )
	{
		ParentSequence->UpdateNamedVarStatus();
		GetRootSequence()->UpdateInterpActionConnectors();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Returns whether this SequenceObject can exist in a sequence without being linked to anything else (i.e. does not require
 * another sequence object to activate it)
 */
UBOOL USequenceVariable::IsStandalone() const
{
	UBOOL bResult = FALSE;

	USequence* RootSequence = const_cast<USequenceVariable*>(this)->GetRootSequence();
	if ( RootSequence != NULL )
	{
		TArray<USequenceVariable*> Unused;
		RootSequence->FindNamedVariables(VarName, TRUE, Unused, TRUE);

		bResult = Unused.Num() > 0;
	}

	return bResult;
}

/**
 * Finds all incoming links to this object and connects them to NewSeqObj
 * Used in ConvertObject()
 */

void USequenceVariable::ConvertObjectInternal(USequenceObject* NewSeqObj, INT LinkIdx)
{
	const USequenceVariable* OldVariable = this;
	USequenceVariable* NewVariable = Cast<USequenceVariable>(NewSeqObj);

	// iterate through all other objects, looking for variable links that point to this variable
	if ((ParentSequence != NULL) && (NewVariable != NULL))
	{
		for (INT ObjectIndex = 0; ObjectIndex < ParentSequence->SequenceObjects.Num(); ++ObjectIndex)
		{
			// if it is a sequence op,
			if (USequenceOp* OtherOp = Cast<USequenceOp>(ParentSequence->SequenceObjects(ObjectIndex)))
			{
				// iterate through this op's variable links
				for (INT VariableIndex = 0; VariableIndex < OtherOp->VariableLinks.Num(); ++VariableIndex)
				{
					// iterate through all the variables linked to variable link
					FSeqVarLink& Link = OtherOp->VariableLinks(VariableIndex);

					for (INT LinkedVarIndex = 0; LinkedVarIndex < Link.LinkedVariables.Num(); ++LinkedVarIndex)
					{
						if (Link.LinkedVariables(LinkedVarIndex) == OldVariable)
						{
							// relink the entry
							OtherOp->Modify(TRUE);
							Link.LinkedVariables(LinkedVarIndex) = NewVariable;
						}
					}
				}
			}
		}
	}
}

/* ==========================================================================================================
	USeqVar_Bool
========================================================================================================== */
void USeqVar_Bool::PublishValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		// first calculate the value
		TArray<UBOOL*> BoolVars;
		Op->GetBoolVars(BoolVars,*VarLink.LinkDesc);
		UBOOL bValue = TRUE;
		for (INT Idx = 0; Idx < BoolVars.Num() && bValue; Idx++)
		{
			bValue = bValue && (*BoolVars(Idx));
		}
		if (Property->IsA(UBoolProperty::StaticClass()))
		{
			// apply the value to the property
			// (stolen from execLetBool to handle the property bitmask madness)
			UBoolProperty *BoolProperty = (UBoolProperty*)(Property);
			BITFIELD *BoolAddr = (BITFIELD*)((BYTE*)Op + Property->Offset);
			if( bValue ) *BoolAddr |=  BoolProperty->BitMask;
			else        *BoolAddr &= ~BoolProperty->BitMask;
			// wouldn't it be cool if you could just do the following??? (isn't memory free nowadays?)
			//*(UBOOL*)((BYTE*)Op + Property->Offset) = bValue;
		}
	}
}

void USeqVar_Bool::PopulateValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		TArray<UBOOL*> BoolVars;
		Op->GetBoolVars(BoolVars,*VarLink.LinkDesc);
		UBoolProperty *BoolProperty = Cast<UBoolProperty>(Property);
		if (BoolProperty != NULL)
		{
			UBOOL bValue = *(BITFIELD*)((BYTE*)Op + Property->Offset) & BoolProperty->BitMask ? TRUE : FALSE;
			for (INT Idx = 0; Idx < BoolVars.Num(); Idx++)
			{
				*(BoolVars(Idx)) = bValue;
			}
		}
	}
}

void USeqVar_Float::PublishValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		TArray<FLOAT*> FloatVars;
		Op->GetFloatVars(FloatVars,*VarLink.LinkDesc);
		if (Property->IsA(UFloatProperty::StaticClass()))
		{
			// first calculate the value
			FLOAT Value = 0.f;
			for (INT Idx = 0; Idx < FloatVars.Num(); Idx++)
			{
				Value += *(FloatVars(Idx));
			}
			// apply the value to the property
			*(FLOAT*)((BYTE*)Op + Property->Offset) = Value;
		}
		// if dealing with an array of floats
		if (Property->IsA(UArrayProperty::StaticClass()) &&
			((UArrayProperty*)Property)->Inner->IsA(UFloatProperty::StaticClass()))
		{
			// grab the array
			UArrayProperty *ArrayProp = (UArrayProperty*)Property;
			INT ElementSize = ArrayProp->Inner->ElementSize;
			FScriptArray *DestArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
			// resize it to fit the variable count
			DestArray->Empty(FloatVars.Num(),ElementSize);
			DestArray->AddZeroed(FloatVars.Num(), ElementSize);
			for (INT Idx = 0; Idx < FloatVars.Num(); Idx++)
			{
				// assign to the array entry
				*(FLOAT*)((BYTE*)DestArray->GetData() + Idx * ElementSize) = *FloatVars(Idx);
			}
		}
	}
}

void USeqVar_Float::PopulateValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		TArray<FLOAT*> FloatVars;
		Op->GetFloatVars(FloatVars,*VarLink.LinkDesc);
		if (Property->IsA(UFloatProperty::StaticClass()))
		{
			FLOAT Value = *(FLOAT*)((BYTE*)Op + Property->Offset);
			for (INT Idx = 0; Idx < FloatVars.Num(); Idx++)
			{
				*(FloatVars(Idx)) = Value;
			}
		}
		else
		if (Property->IsA(UArrayProperty::StaticClass()) &&
			((UArrayProperty*)Property)->Inner->IsA(UFloatProperty::StaticClass()))
		{
			// grab the array
			UArrayProperty *ArrayProp = (UArrayProperty*)Property;
			INT ElementSize = ArrayProp->Inner->ElementSize;
			FScriptArray *SrcArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
			// write out as many entries as are attached
			for (INT Idx = 0; Idx < FloatVars.Num() && Idx < SrcArray->Num(); Idx++)
			{
				*(FloatVars(Idx)) = *(FLOAT*)((BYTE*)SrcArray->GetData() + Idx * ElementSize);
			}
		}
	}
}

void USeqVar_Int::PublishValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		TArray<INT*> IntVars;
		Op->GetIntVars(IntVars,*VarLink.LinkDesc);
		if (Property->IsA(UIntProperty::StaticClass()))
		{
			// first calculate the value
			INT Value = 0;
			for (INT Idx = 0; Idx < IntVars.Num(); Idx++)
			{
				Value += *(IntVars(Idx));
			}
			// apply the value to the property
			*(INT*)((BYTE*)Op + Property->Offset) = Value;
		}
		else
		// if dealing with an array of ints
		if (Property->IsA(UArrayProperty::StaticClass()) &&
			((UArrayProperty*)Property)->Inner->IsA(UIntProperty::StaticClass()))
		{
			// grab the array
			UArrayProperty *ArrayProp = (UArrayProperty*)Property;
			INT ElementSize = ArrayProp->Inner->ElementSize;
			FScriptArray *DestArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
			// resize it to fit the variable count
			DestArray->Empty(IntVars.Num(),ElementSize);
			DestArray->AddZeroed(IntVars.Num(), ElementSize);
			for (INT Idx = 0; Idx < IntVars.Num(); Idx++)
			{
				// assign to the array entry
				*(INT*)((BYTE*)DestArray->GetData() + Idx * ElementSize) = *IntVars(Idx);
			}
		}
	}
}

void USeqVar_Int::PopulateValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		TArray<INT*> IntVars;
		Op->GetIntVars(IntVars,*VarLink.LinkDesc);
		if (Property->IsA(UIntProperty::StaticClass()))
		{
			INT Value = *(INT*)((BYTE*)Op + Property->Offset);
			for (INT Idx = 0; Idx < IntVars.Num(); Idx++)
			{
				*(IntVars(Idx)) = Value;
			}
		}
		else
		if (Property->IsA(UArrayProperty::StaticClass()) &&
			((UArrayProperty*)Property)->Inner->IsA(UIntProperty::StaticClass()))
		{
			// grab the array
			UArrayProperty *ArrayProp = (UArrayProperty*)Property;
			INT ElementSize = ArrayProp->Inner->ElementSize;
			FScriptArray *SrcArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
			// write out as many entries as are attached
			for (INT Idx = 0; Idx < IntVars.Num() && Idx < SrcArray->Num(); Idx++)
			{
				*(IntVars(Idx)) = *(INT*)((BYTE*)SrcArray->GetData() + Idx * ElementSize);
			}
		}
	}
}

void USeqVar_Vector::PublishValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		TArray<FVector*> VectorVars;
		Op->GetVectorVars(VectorVars,*VarLink.LinkDesc);

		UStructProperty* StructProp = Cast<UStructProperty>(Property);
		if (StructProp && StructProp->Struct && 
			(appStricmp(*(StructProp->Struct->GetName()), TEXT("Vector")) == 0))
		{
			// first calculate the value
			FVector Value(0.0f);
			for (INT Idx = 0; Idx < VectorVars.Num(); Idx++)
			{
				Value += *(VectorVars(Idx));
			}
			// apply the value to the property
			*(FVector*)((BYTE*)Op + Property->Offset) = Value;
		}
		else
		{
			// if dealing with an array of vectors
			UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
			if (ArrayProp)
			{
				StructProp = Cast<UStructProperty>(ArrayProp->Inner);
				if (StructProp && StructProp->Struct && 
					(appStricmp(*(StructProp->Struct->GetName()), TEXT("Vector")) == 0))
				{
					// grab the array
					INT ElementSize = ArrayProp->Inner->ElementSize;
					FScriptArray *DestArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
					// resize it to fit the variable count
					DestArray->Empty(VectorVars.Num(),ElementSize);
					DestArray->AddZeroed(VectorVars.Num(), ElementSize);
					for (INT Idx = 0; Idx < VectorVars.Num(); Idx++)
					{
						// assign to the array entry
						*(FVector*)((BYTE*)DestArray->GetData() + Idx * ElementSize) = *VectorVars(Idx);
					}
				}
			}
		}
	}
}

void USeqVar_Vector::PopulateValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		TArray<FVector*> VectorVars;
		Op->GetVectorVars(VectorVars,*VarLink.LinkDesc);
		
		UStructProperty* StructProp = Cast<UStructProperty>(Property);
		if (StructProp && StructProp->Struct && 
			(appStricmp(*(StructProp->Struct->GetName()), TEXT("Vector")) == 0))
		{
			FVector Value = *(FVector*)((BYTE*)Op + Property->Offset);
			for (INT Idx = 0; Idx < VectorVars.Num(); Idx++)
			{
				*(VectorVars(Idx)) = Value;
			}
		}
		else
		{
			// if dealing with an array of vectors
			UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
			if (ArrayProp)
			{
				StructProp = Cast<UStructProperty>(ArrayProp->Inner);
				if (StructProp && StructProp->Struct && 
					(appStricmp(*(StructProp->Struct->GetName()), TEXT("Vector")) == 0))
				{
					// grab the array
					INT ElementSize = ArrayProp->Inner->ElementSize;
					FScriptArray *SrcArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
					// write out as many entries as are attached
					for (INT Idx = 0; Idx < VectorVars.Num() && Idx < SrcArray->Num(); Idx++)
					{
						*(VectorVars(Idx)) = *(FVector*)((BYTE*)SrcArray->GetData() + Idx * ElementSize);
					}
				}
			}
		}
	}
}

/** Returns a pointer to a vector representing the object's location */
FVector* USeqVar_Object::GetRef()
{
	AActor* ActorValue = Cast<AActor>(ObjValue);
	if( ActorValue )
	{
		// If the associated actor is a controller, use the pawn's location
		AController* Controller = ActorValue->GetAController();
		if( Controller != NULL )
		{
			ActorValue = Controller->Pawn;
		}
		// make a copy of the location to prevent issues if this ref is accidentally written to
		ActorLocation = ActorValue->Location;
		return &ActorLocation;
	}
	return NULL;
}

void USeqVar_Object::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Filter out UFields so arbitrary properties won't be directly accessible in Kismet
	if( PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("ObjValue")) )
	{
		if( ObjValue != NULL && ObjValue->IsA(UField::StaticClass()) )
		{
			ObjValue = NULL;
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FString USeqVar_Object::GetValueStr()
{
#if !CONSOLE && WITH_EDITOR
	if ( GKismetRealtimeDebugging )
	{
		USeqVar_Object* PIEVar = Cast<USeqVar_Object>(PIESequenceObject);
		if ( PIEVar )
		{
			return FString::Printf(TEXT("%s"), PIEVar->ObjValue != NULL ? *PIEVar->ObjValue->GetName() : TEXT("???"));
		}
	}
#endif
	return FString::Printf(TEXT("%s"), ObjValue != NULL ? *ObjValue->GetName() : TEXT("???"));
}

void USeqVar_Object::PublishValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		// first calculate the value
		TArray<UObject**> ObjectVars;
		Op->GetObjectVars(ObjectVars,*VarLink.LinkDesc);
		// if dealing with a single object ref
		if (Property->IsA(UObjectProperty::StaticClass()))
		{
			UObjectProperty *ObjProp = (UObjectProperty*)Property;
			// grab the first non-null entry
			UObject* Value = NULL;

			UBOOL bAssignValue = FALSE;
			for (INT Idx = 0; Idx < ObjectVars.Num(); Idx++)
			{
				UObject* VariableValue = *(ObjectVars(Idx));
				if ( VariableValue != NULL )
				{
					// only allow the value of the linked variable to be assigned to the member variable if the variable value
					// is of the correct type
					if ( VariableValue->IsA(ObjProp->PropertyClass) )
					{
						Value = VariableValue;
						bAssignValue = TRUE;
						break;
					}
					else
					{
						bAssignValue = FALSE;
#if !FINAL_RELEASE
						FString LogString("");
						LogString = FString::Printf(TEXT("%s: Invalid value %s linked to VariableLink '%s (%i)'.  Value is '%s', required type is '%s'"), 
													*Op->GetName(), *GetName(), *VarLink.LinkDesc, Idx, *VariableValue->GetFullName(), *ObjProp->PropertyClass->GetPathName());
						//debugf(NAME_Warning, TEXT("Kismet: %s"), *LogString);

						if (GEngine->bOnScreenKismetWarnings)
						{
							// iterate through the controller list
							for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
							{
								// if it's a player
								if (Controller->IsA(APlayerController::StaticClass()))
								{
									((APlayerController*)Controller)->eventClientMessage(LogString);
								}
							}
						}
#endif
					}
				}
			}

			if ( bAssignValue == TRUE )
			{
				// apply the value to the property
				*(UObject**)((BYTE*)Op + Property->Offset) = Value;
			}
		}
		else
		// if dealing with an array of objects
		if (Property->IsA(UArrayProperty::StaticClass()) &&
			((UArrayProperty*)Property)->Inner->IsA(UObjectProperty::StaticClass()))
		{
			// grab the array
			UArrayProperty *ArrayProp = (UArrayProperty*)Property;
			INT ElementSize = ArrayProp->Inner->ElementSize;
			UClass *InnerClass = ((UObjectProperty*)ArrayProp->Inner)->PropertyClass;
			FScriptArray *DestArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
			// resize it to fit the variable count
			DestArray->Empty(ObjectVars.Num(),ElementSize);
			DestArray->AddZeroed(ObjectVars.Num(), ElementSize);
			for (INT Idx = 0; Idx < ObjectVars.Num(); Idx++)
			{
				// if the object is of a valid type
				UObject *Obj = *ObjectVars(Idx);
				if (Obj != NULL)
				{
					if ( Obj->IsA(InnerClass))
					{
						// assign to the array entry
						*(UObject**)((BYTE*)DestArray->GetData() + Idx * ElementSize) = Obj;
					}
#if !FINAL_RELEASE
					else
					{
						FString LogString("");
						LogString = FString::Printf(TEXT("%s: Invalid value linked to VariableLink '%s (%i)'.  Value is '%s', required type is '%s'"), 
													*GetName(), *VarLink.LinkDesc, Idx, *Obj->GetFullName(), *InnerClass->GetPathName());

						//debugf(NAME_Warning, TEXT("Kismet: %s"), *LogString);
						if (GEngine->bOnScreenKismetWarnings)
						{
							// iterate through the controller list
							for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
							{
								// if it's a player
								if (Controller->IsA(APlayerController::StaticClass()))
								{
									((APlayerController*)Controller)->eventClientMessage(LogString);
								}
							}
						}
					}
#endif
				}
			}
		}
		else 
		{
			// check if the object should be treated as a vector
			UStructProperty* StructProp = Cast<UStructProperty>(Property);
			TArray<FVector*> VectorVars;
			Op->GetVectorVars(VectorVars,*VarLink.LinkDesc);

			// if dealing with a vector 
			if (StructProp && StructProp->Struct && 
				(appStricmp(*(StructProp->Struct->GetName()), TEXT("Vector")) == 0))
			{
				TArray<FVector*> VectorVars;
				Op->GetVectorVars(VectorVars,*VarLink.LinkDesc);

				UStructProperty* StructProp = Cast<UStructProperty>(Property);
				if (StructProp && StructProp->Struct && 
					(appStricmp(*(StructProp->Struct->GetName()), TEXT("Vector")) == 0))
				{
					// first calculate the value
					FVector Value(0.0f);
					for (INT Idx = 0; Idx < VectorVars.Num(); Idx++)
					{
						Value += *(VectorVars(Idx));
					}
					// apply the value to the property
					*(FVector*)((BYTE*)Op + Property->Offset) = Value;
				}
			}
			else
			{
				// if dealing with an array of vectors
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
				if (ArrayProp)
				{
					StructProp = Cast<UStructProperty>(ArrayProp->Inner);
					if (StructProp && StructProp->Struct && 
						(appStricmp(*(StructProp->Struct->GetName()), TEXT("Vector")) == 0))
					{
						// grab the array
						INT ElementSize = ArrayProp->Inner->ElementSize;
						FScriptArray *DestArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
						// resize it to fit the variable count
						DestArray->Empty(VectorVars.Num(),ElementSize);
						DestArray->AddZeroed(VectorVars.Num(), ElementSize);
						for (INT Idx = 0; Idx < VectorVars.Num(); Idx++)
						{
							// assign to the array entry
							*(FVector*)((BYTE*)DestArray->GetData() + Idx * ElementSize) = *VectorVars(Idx);
						}
					}
				}
			}
		}
	}
}

void USeqVar_Object::PopulateValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		TArray<UObject**> ObjectVars;
		Op->GetObjectVars(ObjectVars,*VarLink.LinkDesc);
		if (Property->IsA(UObjectProperty::StaticClass()))
		{
			UObject* Value = *(UObject**)((BYTE*)Op + Property->Offset);
			for (INT Idx = 0; Idx < ObjectVars.Num(); Idx++)
			{
				*(ObjectVars(Idx)) = Value;
			}
		}
		else
		if (Property->IsA(UArrayProperty::StaticClass()) &&
			((UArrayProperty*)Property)->Inner->IsA(UObjectProperty::StaticClass()))
		{
			// grab the array
			UArrayProperty *ArrayProp = (UArrayProperty*)Property;
			INT ElementSize = ArrayProp->Inner->ElementSize;
			FScriptArray *SrcArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
			// write out as many entries as are attached
			for (INT Idx = 0; Idx < ObjectVars.Num() && Idx < SrcArray->Num(); Idx++)
			{
				*(ObjectVars(Idx)) = *(UObject**)((BYTE*)SrcArray->GetData() + Idx * ElementSize);
			}
		}
	}
}

void USeqVar_String::PublishValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		// first calculate the value
		TArray<FString*> StringVars;
		Op->GetStringVars(StringVars,*VarLink.LinkDesc);

		UStrProperty* StringProp = Cast<UStrProperty>(Property);
		if (StringProp)
		{
			// first calculate the value
			FString StringValue;
			for (INT Idx = 0; Idx < StringVars.Num(); Idx++)
			{
				StringValue += *(StringVars(Idx));
			}
			// apply the value to the property
			*(FString*)((BYTE*)Op + Property->Offset) = StringValue;
		}
		else
		{
			// if dealing with an array of vectors
			UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
			if (ArrayProp)
			{
				StringProp = Cast<UStrProperty>(ArrayProp->Inner);
				if (StringProp)
				{
					// grab the array
					INT ElementSize = ArrayProp->Inner->ElementSize;
					FScriptArray *DestArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
					// resize it to fit the variable count
					DestArray->Empty(StringVars.Num(),ElementSize);
					DestArray->AddZeroed(StringVars.Num(), ElementSize);
					for (INT Idx = 0; Idx < StringVars.Num(); Idx++)
					{
						// assign to the array entry
						*(FString*)((BYTE*)DestArray->GetData() + Idx * ElementSize) = *(StringVars(Idx));
					}
				}
			}
		}
	}
}

void USeqVar_String::PopulateValue(USequenceOp *Op, UProperty *Property, FSeqVarLink &VarLink)
{
	if (Op != NULL && Property != NULL)
	{
		TArray<FString*> StringVars;
		Op->GetStringVars(StringVars,*VarLink.LinkDesc);

		UStrProperty* StringProp = Cast<UStrProperty>(Property);
		if (StringProp)
		{
			FString Value = *(FString*)((BYTE*)Op + Property->Offset);
			for (INT Idx = 0; Idx < StringVars.Num(); Idx++)
			{
				*(StringVars(Idx)) = Value;
			}
		}
		else
		{
			// if dealing with an array of vectors
			UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
			if (ArrayProp)
			{
				StringProp = Cast<UStrProperty>(ArrayProp->Inner);
				if (StringProp)
				{
					// grab the array
					INT ElementSize = ArrayProp->Inner->ElementSize;
					FScriptArray *SrcArray = (FScriptArray*)((BYTE*)Op + ArrayProp->Offset);
					// write out as many entries as are attached
					for (INT Idx = 0; Idx < StringVars.Num() && Idx < SrcArray->Num(); Idx++)
					{
						*(StringVars(Idx)) = *(FString*)((BYTE*)SrcArray->GetData() + Idx * ElementSize);
					}
				}
			}
		}
	}
}

//==========================
// USeqVar_Player interface

/** updates the Players array with the list of Controllers in the game that count as players (humans or bot-players) */
void USeqVar_Player::UpdatePlayersList()
{
	Players.Reset();
	if( GWorld != NULL )
	{
		for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
		{
			if (Controller->IsPlayerOwner())
			{
				// ControllerList is in reverse order of joins, so insert into the first element of the array so that element 0 is the first player that joined the game and so on
				Players.InsertItem(Controller, 0);
			}
		}
	}
}

/**
 * If the current Object value isn't a player it searches
 * through the level controller list for the first player.
 */
UObject** USeqVar_Player::GetObjectRef( INT Idx )
{
	// check for new players
	UpdatePlayersList();

	// if all players,
	if (bAllPlayers)
	{
		// then return the next one in the list
		if (Idx >= 0 && Idx < Players.Num())
		{
			return &(Players(Idx));
		}
	}
	else
	{
		if (Idx == 0)
		{
			// cache the player
			if (PlayerIdx >= 0 && PlayerIdx < Players.Num())
			{
				ObjValue = Players(PlayerIdx);
			}
			return &ObjValue;
		}
	}
	return NULL;
}

//==========================
// USeqVar_ObjectVolume

/**
 * If we're in the editor, return the normal ref to ObjectValue,
 * otherwise get the contents of the volume and return refs to each
 * one.
 */
UObject** USeqVar_ObjectVolume::GetObjectRef(INT Idx)
{
	if (GWorld != NULL && GWorld->HasBegunPlay())
	{
		// check to see if an update is needed
		if (GWorld->GetTimeSeconds() != LastUpdateTime)
		{
			LastUpdateTime = GWorld->GetTimeSeconds();
			ContainedObjects.Empty();
			AVolume *Volume = Cast<AVolume>(ObjValue);
			if (Volume != NULL)
			{
				if (bCollidingOnly)
				{
					for (INT TouchIdx = 0; TouchIdx < Volume->Touching.Num(); TouchIdx++)
					{
						AActor *Actor = Volume->Touching(TouchIdx);
						if (Actor != NULL &&
							!Actor->bDeleteMe &&
							!ExcludeClassList.ContainsItem(Actor->GetClass()))
						{
							ContainedObjects.AddUniqueItem(Actor);
						}
					}
				}
				else
				{
					// look for all actors encompassed by the volume
					for (FActorIterator ActorIt; ActorIt; ++ActorIt)
					{
						AActor *Actor = *ActorIt;
						if (Actor != NULL && !Actor->IsPendingKill() && Volume->Encompasses(Actor->Location) && !ExcludeClassList.ContainsItem(Actor->GetClass()))
						{
							ContainedObjects.AddItem(Actor);
						}
					}
				}
			}
		}
		if (Idx >= 0 && Idx < ContainedObjects.Num())
		{
			return &ContainedObjects(Idx);
		}
		else
		{
			return NULL;
		}
	}

	// We'll only report a value when the caller requested the first reference index.  It's dangerous to report the
	// same object for all reference indices, since we often use a return value of NULL to indicate we should stop
	// iterating over (an unknown number) of object references.
	return ( Idx == 0 ) ? &ObjValue : NULL;
}

//==========================
// UDEPRECATED_SeqVar_Group

FString UDEPRECATED_SeqVar_Group::GetValueStr()
{
	if (GroupName == NAME_None)
	{
		return FString(TEXT("Invalid"));
	}
	else
	{
		return GroupName.ToString();
	}
}

UObject** UDEPRECATED_SeqVar_Group::GetObjectRef(INT Idx)
{
	if (GWorld != NULL && GroupName != NAME_None)
	{
		// build the list if necessary
		if (!bCachedList)
		{
			debugf(TEXT("%s caching list for layer %s"),*GetName(),*GroupName.ToString());
			Actors.Empty();
			bCachedList = TRUE;
			FString LayerString = GroupName.ToString();
			for (FActorIterator ActorIt; ActorIt; ++ActorIt)
			{
				if (ActorIt->Layer != NAME_None)
				{
					TArray<FString> ActorLayers;
					ActorIt->Layer.ToString().ParseIntoArray(&ActorLayers,TEXT(","),FALSE);
					for (INT LayerIdx = 0; LayerIdx < ActorLayers.Num(); LayerIdx++)
					{
						if (ActorLayers(LayerIdx) == LayerString)
						{
							debugf(TEXT("- added %s"),*ActorIt->GetName());
							Actors.AddItem(*ActorIt);
							break;
						}
					}
				}
			}
		}
		if (Actors.IsValidIndex(Idx))
		{
			debugf(TEXT("- returning actor %s"),*Actors(Idx)->GetName());
			return &(Actors(Idx));
		}
	}
	return NULL;
}


//==========================
// USeqVar_Object List


void USeqVar_ObjectList::OnCreated()
{
	Super::OnCreated();
	ObjList.Empty();
}

void USeqVar_ObjectList::OnExport()
{
	// we need to see how to export arrays here, let's look at inventory
}


UObject** USeqVar_ObjectList::GetObjectRef( INT Idx )
{
	UObject** Retval = NULL;

	// check to to see if there are Objects in the list 
	// and if the index being passed in is a valid index.  (the caller can pass in what ever they desire)
	if ( ObjList.IsValidIndex(Idx) )
	{
		Retval = &ObjList( Idx );  // the actual Object at this index
	}

	return Retval;
}


FString USeqVar_ObjectList::GetValueStr()
{
	FString Retval = FString::Printf(TEXT("ObjectList Entries(%d):  "), ObjList.Num());

	for (INT Idx = 0; Idx < ObjList.Num(); Idx++)
	{
		if (ObjList(Idx) != NULL)
		{
			Retval = FString::Printf(TEXT("%s|%s"), *Retval, *ObjList(Idx)->GetName() );
		}
	}

	return Retval;
}

void USeqVar_ObjectList::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// Filter out UFields so arbitrary properties won't be directly accessible in Kismet
	if ( PropertyChangedEvent.PropertyChain.Num() > 0)
	{
		UProperty* MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
		if ( MemberProperty != NULL )
		{
			FName PropertyName = MemberProperty->GetFName();
			if (PropertyName == TEXT("ObjList"))
			{
				if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
				{
					INT ObjIdx = PropertyChangedEvent.GetArrayIndex(TEXT("ObjList"));
					if( ObjList(ObjIdx) != NULL && ObjList(ObjIdx)->IsA(UField::StaticClass()) )
					{
						ObjList(ObjIdx) = NULL;
					}
				}
			}
		}
	}
	
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}


//==========================
// USeqAct_ModifyObjectList

void USeqAct_ModifyObjectList::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints
	ActivatedAddRemove();
	// update count
	TArray<UObject**> ObjsInList;
	GetObjectVars( ObjsInList, TEXT("ObjectListVar") );
	ListEntriesCount = ObjsInList.Num();
}

void USeqAct_ModifyObjectList::ActivatedAddRemove()
{
	// check for adding
	//InputLinks(0)=(LinkDesc="AddObjectToList")
	if (InputLinks(0).bHasImpulse)
	{
		ActivateAddRemove_Helper( 0 );
	}
	// InputLinks(1)=(LinkDesc="RemoveObjectFromList")
	else if (InputLinks(1).bHasImpulse)
	{
		ActivateAddRemove_Helper( 1 );
	}
	//InputLinks(2)=(LinkDesc="EmptyList")
	else if (InputLinks(2).bHasImpulse)
	{
		ActivateAddRemove_Helper( 2 );
	}
}

void USeqAct_ModifyObjectList::ActivateAddRemove_Helper( INT LinkNum )
{

	// look at all of the variable Links for the List Link
	for (INT Idx = 0; Idx < VariableLinks.Num(); Idx++)
	{
		if ((VariableLinks(Idx).SupportsVariableType(USeqVar_ObjectList::StaticClass()))
			&& (VariableLinks(Idx).LinkDesc == TEXT("ObjectListVar") ) // hack for now
			)
		{
			// for each List that we are Linked to
			for (INT LinkIdx = 0; LinkIdx < VariableLinks(Idx).LinkedVariables.Num(); LinkIdx++)
			{
				// we know the Object should be an ObjectList.
				USeqVar_ObjectList* AList = Cast<USeqVar_ObjectList>((VariableLinks(Idx).LinkedVariables(LinkIdx)));

				if( AList != NULL )
				{
					// empty case
					if( 2 == LinkNum )
					{
						AList->ObjList.Empty();
					}
					else
					{
						// get all of the Objects that we want to do something with
						TArray<UObject**> ObjsList;
						GetObjectVars(ObjsList, TEXT("ObjectRef"));
						for( INT ObjIdx = 0; ObjIdx < ObjsList.Num(); ObjIdx++ )
						{
							// add case
							if( 0 == LinkNum )
							{
								AList->ObjList.AddUniqueItem( *(ObjsList(ObjIdx)) );
							}
							// remove case
							else if( 1 == LinkNum )
							{
								AList->ObjList.RemoveItem( *(ObjsList(ObjIdx)) );
							}
						}
					}
				} 
			} 
		}
	}
}


void USeqAct_ModifyObjectList::DeActivated()
{
	// activate all output impulses
	for (INT LinkIdx = 0; LinkIdx < OutputLinks.Num(); LinkIdx++)
	{
		// fire off the impulse
		OutputLinks(LinkIdx).ActivateOutputLink();
	}
}

//==========================
// USeqAct_AccessObjectList

void USeqAct_AccessObjectList::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if( VariableLinks.Num() == 0 )
	{
		OutputObject = NULL;
		return;
	}

	USeqVar_ObjectList* AList = Cast<USeqVar_ObjectList>((VariableLinks(0).LinkedVariables(0)));

	if( AList == NULL )
	{
		OutputObject = NULL;
		return;
	}

	if( AList->ObjList.Num() == 0 )
	{
		OutputObject = NULL;
		return;
	}

	if (InputLinks(0).bHasImpulse)	// Random
	{
		OutputObject = AList->ObjList( appRound( (AList->ObjList.Num() - 1) * appFrand() ) );
	}
	else if (InputLinks(1).bHasImpulse)	// First
	{
		OutputObject = AList->ObjList( 0 );
	}
	else if (InputLinks(2).bHasImpulse)	// Last
	{
		OutputObject = AList->ObjList( AList->ObjList.Num() - 1 );
	}
	else if (InputLinks(3).bHasImpulse)	// At Index
	{
		if( ObjectIndex < 0 || ObjectIndex >= AList->ObjList.Num() )
		{
			OutputObject = NULL;
		}
		else
		{
			OutputObject = AList->ObjList( ObjectIndex );
		}
	}
}

void USeqAct_AccessObjectList::DeActivated()
{
	// activate all output impulses
	for (INT LinkIdx = 0; LinkIdx < OutputLinks.Num(); LinkIdx++)
	{
		// fire off the impulse
		OutputLinks(LinkIdx).ActivateOutputLink();
	}
}

//==========================
// USeqAct_IsInObjectList


void USeqAct_IsInObjectList::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	bObjectFound = FALSE;
	if( TRUE == bCheckForAllObjects )
	{
		bObjectFound = this->TestForAllObjectsInList();
	}
	else
	{
		bObjectFound = this->TestForAnyObjectsInList();
	}
}


UBOOL USeqAct_IsInObjectList::TestForAllObjectsInList()
{
	UBOOL Retval = FALSE; // assume the item is not in the list.  The testing will tell us if this assumption is incorrect

	// get the objects in the list
	TArray<UObject**> ObjsInList;
	GetObjectVars( ObjsInList, TEXT("ObjectListVar") );

	// get all of the Objects that we want to see if they are in the list
	TArray<UObject**> ObjsToTestList;
	GetObjectVars( ObjsToTestList, TEXT("Object(s)ToTest") );


	for( INT ObjToTestIdx = 0; ObjToTestIdx < ObjsToTestList.Num(); ObjToTestIdx++ )
	{
		UBOOL bWasInList = FALSE;

		for( INT ObjListIdx = 0; ObjListIdx < ObjsInList.Num(); ObjListIdx++ )
		{
			// if any are NOT in the list then set retval to false and break
			if( ( NULL != ObjsToTestList( ObjToTestIdx ) )
				&& ( NULL != ObjsInList( ObjListIdx ) )
				&& ( *ObjsToTestList( ObjToTestIdx ) != *ObjsInList( ObjListIdx ) )
				)
			{
				bWasInList = FALSE;
			}
			else
			{
				// if we find an item in the list then we set a var and break out!
				bWasInList = TRUE;
				break;
			}
		}

		// if we didn't find any of the objects in the list then we need to break out with a FALSE value to return
		if( bWasInList == FALSE )
		{
			Retval = FALSE;
			break;
		}
		// we found the item in the list so we should try the new ObjToTest and see if it is also in the list
		else
		{
			Retval = TRUE;
		}
	}

	return Retval;
}

UBOOL USeqAct_IsInObjectList::TestForAnyObjectsInList()
{
	UBOOL Retval = FALSE; // assume the item is not in the list..  The testing will tell us if this assumption is incorrect

	// get the objects in the list
	TArray<UObject**> ObjsInList;
	GetObjectVars( ObjsInList, TEXT("ObjectListVar") );

	// get all of the Objects that we want to see if they are in the list
	TArray<UObject**> ObjsToTestList;
	GetObjectVars( ObjsToTestList, TEXT("Object(s)ToTest") );
	for( INT ObjToTestIdx = 0; ObjToTestIdx < ObjsToTestList.Num(); ObjToTestIdx++ )
	{
		for( INT ObjListIdx = 0; ObjListIdx < ObjsInList.Num(); ObjListIdx++ )
		{
			// if any are in the list set retval and return;
			if( ( NULL != ObjsToTestList( ObjToTestIdx ) )
				&& ( NULL != ObjsInList( ObjListIdx ) )
				&& ( *ObjsToTestList( ObjToTestIdx ) == *ObjsInList( ObjListIdx ) )
				)
			{
				// if it is in the list then just return TRUE as we are testing for ANY
				Retval = TRUE;
				return Retval;
			}
			else
			{
				Retval = FALSE;
			}
		}
	}

	return Retval;
}

void USeqAct_IsInObjectList::DeActivated()
{
	OutputLinks(bObjectFound ? 0 : 1).ActivateOutputLink();
}


//==========================
// USeqVar_External 


/** PostLoad to ensure color is correct. */
void USeqVar_External::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITORONLY_DATA
	if(ExpectedType)
	{
		ObjColor = ExpectedType->GetDefaultObject<USequenceVariable>()->ObjColor;
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * Overridden to update the expected variable type and parent
 * Sequence connectors.
 */
void USeqVar_External::OnConnect(USequenceObject *connObj, INT connIdx)
{
#if WITH_EDITORONLY_DATA
	USequenceOp *op = Cast<USequenceOp>(connObj);
	if ( op != NULL && op->VariableLinks.IsValidIndex(connIdx) )
	{
		FSeqVarLink& VarLink = op->VariableLinks(connIdx);

		// figure out where we're connected, and what the type is
		INT VarIdx = 0;
		if ( VarLink.LinkedVariables.FindItem(this,VarIdx) )
		{
			ExpectedType = VarLink.ExpectedType;
			USequenceVariable* Var = ExpectedType->GetDefaultObject<USequenceVariable>();
			if ( Var != NULL )
			{
				ObjColor = Var->ObjColor;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	GetOuterUSequence()->UpdateConnectors();
	Super::OnConnect(connObj,connIdx);
}

FString USeqVar_External::GetValueStr()
{
	// if we have been Linked, reflect that in the description
	if (ExpectedType != NULL &&
		ExpectedType != USequenceVariable::StaticClass())
	{
		return FString::Printf(TEXT("Ext. %s"),*ExpectedType->GetDefaultObject<USequenceObject>()->ObjName);
	}
	else
	{
		return FString(TEXT("Ext. ???"));
	}
}

//==========================
// USeqVar_Named

/** Overridden to update the expected variable type. */
void USeqVar_Named::OnConnect(USequenceObject *connObj, INT connIdx)
{
#if WITH_EDITORONLY_DATA
	USequenceOp *op = Cast<USequenceOp>(connObj);
	if ( op != NULL && op->VariableLinks.IsValidIndex(connIdx) )
	{
		FSeqVarLink& VarLink = op->VariableLinks(connIdx);

		// figure out where we're connected, and what the type is
		INT VarIdx = 0;
		if (VarLink.LinkedVariables.FindItem(this,VarIdx))
		{
			ExpectedType = VarLink.ExpectedType;
			USequenceVariable* Var = ExpectedType->GetDefaultObject<USequenceVariable>();
			if( Var != NULL )
			{
				ObjColor = Var->ObjColor;
			}			
		}
	}
#endif // WITH_EDITORONLY_DATA

	Super::OnConnect(connObj,connIdx);
}

/** Text in Named variables is VarName we are Linking to. */
FString USeqVar_Named::GetValueStr()
{
	// Show the VarName we this variable should Link to.
	if(FindVarName != NAME_None)
	{
		return FString::Printf(TEXT("< %s >"), *FindVarName.ToString());
	}
	else
	{
		return FString(TEXT("< ??? >"));
	}
}

/** PostLoad to ensure colour is correct. */
void USeqVar_Named::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if(ExpectedType)
	{
		ObjColor = ExpectedType->GetDefaultObject<USequenceVariable>()->ObjColor;
	}
#endif // WITH_EDITORONLY_DATA
}

/** If we changed FindVarName, update NamedVar status globally. */
void USeqVar_Named::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("FindVarName")) )
	{
		if( ParentSequence != NULL )
		{
			ParentSequence->UpdateNamedVarStatus();
		}

		USequence* RootSeq = GetRootSequence();
		if( RootSeq != NULL )
		{
			RootSeq->UpdateInterpActionConnectors();
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** 
 *	Check this variable is ok, and set bStatusIsOk accordingly. 
 */
void USeqVar_Named::UpdateStatus()
{
	bStatusIsOk = FALSE;
	// Do nothing if no variable name specified yet
	if (FindVarName == NAME_None)
	{
		return;
	}

	// start with the current sequence
	USequence *Seq = ParentSequence;
	while (Seq != NULL)
	{
		// look for named variables in this sequence
		TArray<USequenceVariable*> Vars;
		Seq->FindNamedVariables(FindVarName, FALSE, Vars, FALSE);

		// if one was found
		if (Vars.Num() > 0)
		{
			USequenceVariable *Var = Vars(0);
			if (Var != NULL)
			{
				// If we're referencing an External variable, use that variable's ExpectedType
				if( Var->GetClass() == USeqVar_External::StaticClass() )
				{
					ExpectedType = (Cast<USeqVar_External>(Var))->ExpectedType;
				}
				else
				{
					ExpectedType = Var->GetClass();
				}
			
				if (ValidateVarLinks())
				{
					bStatusIsOk = TRUE;
				}
			}
			return;
		}
		// otherwise move to the next sequence
		// special check for streaming levels 
		if (Seq->ParentSequence == NULL)
		{
			// look up to the persistent level base sequence
			// since it will be the parent sequence when actually streamed in
			if (GWorld->PersistentLevel->GameSequences.Num() > 0 && Seq != GWorld->PersistentLevel->GameSequences(0))
			{
				Seq = GWorld->PersistentLevel->GameSequences(0);
			}
			else
			{
				Seq = NULL;
			}
		}
		else
		{
			Seq = Seq->ParentSequence;
		}
	}
}

UBOOL USeqVar_Named::ValidateVarLinks()
{
	// iterate through all other objects, looking for output links that point to this op
	if ( ParentSequence )
	{
		for ( INT chkIdx = 0; chkIdx < ParentSequence->SequenceObjects.Num(); ++chkIdx )
		{
			// if it is a sequence op,
			USequenceOp *ChkOp = Cast<USequenceOp>(ParentSequence->SequenceObjects(chkIdx));
			if ( ChkOp != NULL )
			{
				// iterate through this op's variable links
				for ( INT linkIdx = 0; linkIdx < ChkOp->VariableLinks.Num(); ++linkIdx )
				{
					// iterate through all the variables linked to this variable link
					for ( INT inputIdx = 0; inputIdx < ChkOp->VariableLinks(linkIdx).LinkedVariables.Num(); ++inputIdx )
					{
						if ( ChkOp->VariableLinks(linkIdx).LinkedVariables(inputIdx) == this && !ChkOp->VariableLinks(linkIdx).SupportsVariableType(ExpectedType, FALSE) )
						{
							return FALSE;
						}
					}
				}
			}
		}
	}
	return TRUE;
}


//==========================
// USeqAct_SetCameraTarget interface

/* epic ===============================================
* ::Activated
*
* Build the camera target then pass off to script handler.
*
* =====================================================
*/
void USeqAct_SetCameraTarget::Activated()
{
	// clear the previous target
	CameraTarget = NULL;
	// grab all Object variables
	TArray<UObject**> ObjectVars;
	GetObjectVars(ObjectVars,TEXT("Cam Target"));
	for (INT VarIdx = 0; VarIdx < ObjectVars.Num() && CameraTarget == NULL; VarIdx++)
	{
		// pick the first one
		CameraTarget = Cast<AActor>(*(ObjectVars(VarIdx)));
	}
	Super::Activated();
}

/*
* ::UpdateOp
*
* Update parameters over time
* 
*/
UBOOL USeqAct_SetDOFParams::UpdateOp(FLOAT DeltaTime)
{
#if GEMINI_TODO
	// if this is running on the server then we need to NOT process
	if( ( GRenderDevice == NULL ) 
		|| ( GRenderDevice->PostProcessManager == NULL )
		)
	{
		return TRUE;
	}

	// Update interpolating counter
	if(InterpolateElapsed > InterpolateSeconds)
	{
		return TRUE;
	}

	InterpolateElapsed += DeltaTime;

	// Fraction of time elapsed
	FLOAT Fraction = (InterpolateElapsed/InterpolateSeconds);

	UDOFEffect* Effect = 0;
	for(INT I=0;I<GRenderDevice->PostProcessManager->Chains.Num();I++)
	{
		for(INT J=0;J<GRenderDevice->PostProcessManager->Chains(I)->Effects.Num();J++)
		{
			if(GRenderDevice->PostProcessManager->Chains(I)->Effects(J)->IsA(UDOFEffect::StaticClass()))
			{
				Effect = (UDOFEffect*)GRenderDevice->PostProcessManager->Chains(I)->Effects(J);

				if(!Effect)
					continue;

				// Propagate kismet properties to effect
				Effect->FalloffExponent = Lerp(OldFalloffExponent,FalloffExponent,Fraction);
				Effect->BlurKernelSize = Lerp(OldBlurKernelSize,BlurKernelSize,Fraction);
				Effect->MaxNearBlurAmount = Lerp(OldMaxNearBlurAmount,MaxNearBlurAmount,Fraction);
				Effect->MinBlurAmount = Lerp(OldMinBlurAmount,MinBlurAmount,Fraction);
				Effect->MaxFarBlurAmount = Lerp(OldMaxFarBlurAmount,MaxFarBlurAmount,Fraction);
				Effect->FocusInnerRadius = Lerp(OldFocusInnerRadius,FocusInnerRadius,Fraction);
				Effect->FocusDistance = Lerp(OldFocusDistance,FocusDistance,Fraction);
				Effect->FocusPosition = Lerp(OldFocusPosition,FocusPosition,Fraction);
			}
		}
	}

#endif

	return FALSE;
}

/*
* ::Activated
*
* Enable DOF with specified params
* 
*/
void USeqAct_SetDOFParams::Activated()
{
	Super::Activated();

#if GEMINI_TODO
	// if this is running on the server then we need to NOT process
	if( ( GRenderDevice == NULL ) 
		|| ( GRenderDevice->PostProcessManager == NULL )
		)
	{
		return;
	}

	if (InputLinks(0).bHasImpulse)
	{
		InterpolateElapsed = 0;
		// See if we have a DOF effect already
		UDOFEffect* Effect = 0;
		for(INT I=0;I<GRenderDevice->PostProcessManager->Chains.Num();I++)
		{
			for(INT J=0;J<GRenderDevice->PostProcessManager->Chains(I)->Effects.Num();J++)
			{
				if(GRenderDevice->PostProcessManager->Chains(I)->Effects(J)->IsA(UDOFEffect::StaticClass()))
				{
					Effect = (UDOFEffect*)GRenderDevice->PostProcessManager->Chains(I)->Effects(J);
				}
			}
		}

		// If not, create one in the first chain
		if(!Effect)
		{
			Effect = new UDOFEffect();
			GRenderDevice->PostProcessManager->Chains(0)->Effects.AddItem(Effect);
		}

		OldFalloffExponent		= Effect->FalloffExponent;
		OldBlurKernelSize		= Effect->BlurKernelSize;
		OldMaxNearBlurAmount	= Effect->MaxNearBlurAmount;
		OldMinBlurAmount		= Effect->MinBlurAmount;
		OldMaxFarBlurAmount		= Effect->MaxFarBlurAmount;
		OldFocusInnerRadius		= Effect->FocusInnerRadius;
		OldFocusDistance		= Effect->FocusDistance;
		OldFocusPosition		= Effect->FocusPosition;
	}
	else
	{
		// Find effect and delete.
		// @todo: Make this a disable, to avoid resource allocation
		for(INT I=0;I<GRenderDevice->PostProcessManager->Chains.Num();I++)
		{
			for(INT J=0;J<GRenderDevice->PostProcessManager->Chains(I)->Effects.Num();J++)
			{
				if(GRenderDevice->PostProcessManager->Chains(I)->Effects(J)->IsA(UDOFEffect::StaticClass()))
				{
					GRenderDevice->PostProcessManager->Chains(I)->Effects.Remove(J);
				}
			}
		}
	}
#endif
}


/*
* ::Activated
*
* Disable DOF
* 
*/
void USeqAct_SetDOFParams::DeActivated()
{
	Super::DeActivated();
}


/*
* ::UpdateOp
*
* Update parameters over time
* 
*/
UBOOL USeqAct_SetMotionBlurParams::UpdateOp(FLOAT DeltaTime)
{
#if GEMINI_TODO
	// if this is running on the server then we need to NOT process
	if( ( GRenderDevice == NULL ) 
		|| ( GRenderDevice->PostProcessManager == NULL )
		)
	{
		return TRUE;
	}

	// Update interpolating counter
	if(InterpolateElapsed > InterpolateSeconds)
	{
		return TRUE;
	}

	InterpolateElapsed += DeltaTime;

	// Fraction of time elapsed
	FLOAT Fraction = (InterpolateElapsed/InterpolateSeconds);

	UMotionBlurEffect* Effect = 0;
	for(INT I=0;I<GRenderDevice->PostProcessManager->Chains.Num();I++)
	{
		for(INT J=0;J<GRenderDevice->PostProcessManager->Chains(I)->Effects.Num();J++)
		{
			if(GRenderDevice->PostProcessManager->Chains(I)->Effects(J)->IsA(UMotionBlurEffect::StaticClass()))
			{
				Effect = (UMotionBlurEffect*)GRenderDevice->PostProcessManager->Chains(I)->Effects(J);

				if(!Effect)
					continue;

				// Propagate kismet properties to effect
				Effect->MotionBlurAmount = Lerp(OldMotionBlurAmount,MotionBlurAmount,Fraction);
			}
		}
	}
#endif

	return FALSE;
}

/*
* ::Activated
*
* Enable motion blur with specified params
* 
*/
void USeqAct_SetMotionBlurParams::Activated()
{
#if GEMINI_TODO
	Super::Activated();

	// if this is running on the server then we need to NOT process
	if( ( GRenderDevice == NULL ) 
		|| ( GRenderDevice->PostProcessManager == NULL )
		)
	{
		return;
	}

	if (InputLinks(0).bHasImpulse)
	{
		InterpolateElapsed = 0;
		// See if we have a DOF effect already
		UMotionBlurEffect* Effect = 0;
		for(INT I=0;I<GRenderDevice->PostProcessManager->Chains.Num();I++)
		{
			for(INT J=0;J<GRenderDevice->PostProcessManager->Chains(I)->Effects.Num();J++)
			{
				if(GRenderDevice->PostProcessManager->Chains(I)->Effects(J)->IsA(UMotionBlurEffect::StaticClass()))
				{
					Effect = (UMotionBlurEffect*)GRenderDevice->PostProcessManager->Chains(I)->Effects(J);
				}
			}
		}

		// If not, create one in the first chain
		if(!Effect)
		{
			Effect = new UMotionBlurEffect();
			GRenderDevice->PostProcessManager->Chains(0)->Effects.AddItem(Effect);
		}

		OldMotionBlurAmount		= Effect->MotionBlurAmount;
	}
	else
	{
		// Find effect and delete.
		// @todo: Make this a disable, to avoid resource allocation
		for(INT I=0;I<GRenderDevice->PostProcessManager->Chains.Num();I++)
		{
			for(INT J=0;J<GRenderDevice->PostProcessManager->Chains(I)->Effects.Num();J++)
			{
				if(GRenderDevice->PostProcessManager->Chains(I)->Effects(J)->IsA(UMotionBlurEffect::StaticClass()))
				{
					GRenderDevice->PostProcessManager->Chains(I)->Effects.Remove(J);
				}
			}
		}
	}
#endif
}


/*
* ::Activated
*
* Disable motion blur
* 
*/
void USeqAct_SetMotionBlurParams::DeActivated()
{
	Super::DeActivated();
}

//==========================
// USeqAct_ParticleEventGenerator interface

void USeqAct_ParticleEventGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void USeqAct_ParticleEventGenerator::Activated()
{
	if (bEnabled == TRUE)
	{
		Super::Activated();
	}
}

UBOOL USeqAct_ParticleEventGenerator::UpdateOp(FLOAT DeltaTime)
{
	CheckToggle();

	// Make sure this is: Enabled, has targets, and has event names to generate...
	if (bEnabled && (Targets.Num() > 0) && (EventNames.Num() > 0))
	{
		// Pass the event to the particle system
		for (INT TargetIndex = 0; TargetIndex < Targets.Num(); TargetIndex++)
		{
			AEmitter* EmitterActor = Cast<AEmitter>(Targets(TargetIndex));
			if (EmitterActor && EmitterActor->ParticleSystemComponent)
			{
				for (INT EventNameIndex = 0; EventNameIndex < EventNames.Num(); EventNameIndex++)
				{	
					FName EventName = FName(*EventNames(EventNameIndex));
					EmitterActor->ParticleSystemComponent->ReportEventKismet(EventName, 
						EventTime, EventLocation, EventDirection, EventVelocity, bUseEmitterLocation, EventNormal);
				}
			}
		}
	}

	return TRUE;
}

void USeqAct_ParticleEventGenerator::DeActivated()
{
	Super::DeActivated();
}

//==========================
// USeqAct_Possess interface

/* epic ===============================================
* ::Activated
*
* Build the Pawn Target, then pass off to script handler
*
* =====================================================
*/
void USeqAct_Possess::Activated()
{
	// clear the previous target
	PawnToPossess = NULL;

	// grab all Object variables
	TArray<UObject**> ObjectVars;
	GetObjectVars(ObjectVars,TEXT("Pawn Target"));
	for (INT VarIdx = 0; VarIdx < ObjectVars.Num(); VarIdx++)
	{
		// pick the first one
		PawnToPossess = Cast<APawn>(*(ObjectVars(VarIdx)));
		break;
	}
	Super::Activated();
}

//==========================
// USeqAct_ActorFactory interface

void USeqAct_ActorFactory::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Reject factories that can't be used in-game because they refer to unspawnable types.
	if ( Factory && Factory->NewActorClass )
	{
		if ( Factory->NewActorClass == Factory->GetClass()->GetDefaultObject<UActorFactory>()->NewActorClass &&
			Factory->NewActorClass->GetDefaultActor()->bNoDelete &&
			(Factory->GameplayActorClass == NULL || Factory->GameplayActorClass->GetDefaultActor()->bNoDelete) )
		{
			appMsgf( AMT_OK, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("ActorFactoryNotForUseByKismetF"),*Factory->GetClass()->GetName())) );
			Factory = NULL;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Resets all transient properties for spawning.
 * 
 * SeqAct_ActorFactory doesn't have any Actor Handle functions, 
 * and does not have any initialization to do in InitializeLinkedVariableValues(), 
 * so it is not be necessary to call Super::Activated()
 */
void USeqAct_ActorFactory::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if (InputLinks(0).bHasImpulse &&
		Factory != NULL)
	{
		bIsSpawning = TRUE;
		// reset all our transient properties
		RemainingDelay = 0.f;
		SpawnedCount = 0;
	}
	else
	{
		CheckToggle();
	}
}

/**
 * Checks if the delay has been met, and creates a new
 * actor, choosing a spawn Point and passing the values
 * to the selected factory.  Once an actor is created then
 * all Object variables Linked as "Spawned" are set to the
 * new actor, and the output Links are activated.  The op
 * terminates once SpawnCount has been exceeded.
 * 
 * @param		DeltaTime		time since last tick
 * @return						true to indicate that all
 * 								actors have been created
 */
UBOOL USeqAct_ActorFactory::UpdateOp(FLOAT DeltaTime)
{
	CheckToggle();
	if (bEnabled && bIsSpawning)
	{
		if ((Factory != NULL) && ((SpawnPoints.Num() > 0) || (SpawnLocations.Num() > 0)))
		{
			// if the delay has been exceeded
			if (RemainingDelay <= 0.f)
			{
				if (SpawnPoints.Num() > 0)
				{
					// process point selection if necessary
					if (SpawnPoints.Num() > 1)
					{
						switch (PointSelection)
						{
						case PS_Random:
							// randomize the list of points
							for (INT Idx = 0; Idx < SpawnPoints.Num(); Idx++)
							{
								INT NewIdx = Idx + (appRand()%(SpawnPoints.Num()-Idx));
								SpawnPoints.SwapItems(NewIdx,Idx);
							}
							LastSpawnIdx = -1;
							break;
						case PS_Normal:
							break;
						case PS_Reverse:
							// reverse the list
							for (INT Idx = 0; Idx < SpawnPoints.Num()/2; Idx++)
							{
								SpawnPoints.SwapItems(Idx,SpawnPoints.Num()-1-Idx);
							}
							break;
						default:
							break;
						}
					}
					AActor *NewSpawn = NULL;
					INT SpawnIdx = LastSpawnIdx;
					for (INT Count = 0; Count < SpawnPoints.Num() && NewSpawn == NULL; Count++)
					{
						if (++SpawnIdx >= SpawnPoints.Num())
						{
							SpawnIdx = 0;
						}
						AActor *Point = SpawnPoints(SpawnIdx);
						if( Point )
						{
							// attempt to create the new actor
							CurrentSpawnIdx = SpawnIdx;
							NewSpawn = Factory->CreateActor(&(Point->Location),&(Point->Rotation), this);
							// if we created the actor
							if (NewSpawn != NULL)
							{
								NewSpawn->bKillDuringLevelTransition = TRUE;
								NewSpawn->eventSpawnedByKismet();
								KISMET_LOG(TEXT("Spawned %s at %s"),*NewSpawn->GetName(),*Point->GetName());
								// increment the spawned count
								SpawnedCount++;
								Spawned(NewSpawn);
								LastSpawnIdx = SpawnIdx;
							}
							else
							{
								KISMET_WARN(TEXT("Failed to spawn at %s using factory %s"),*Point->GetName(),*Factory->GetName());
							}
						}
					}
				}
				else
				{
					// We assume if there are Orientations, they will match the number of locations (for now, at least)
					UBOOL bOrientationMatchesLocation = (SpawnOrientations.Num() == SpawnLocations.Num());
					// process point selection if necessary
					if (SpawnLocations.Num() > 1)
					{
						switch (PointSelection)
						{
						case PS_Random:
							// randomize the list of points
							for (INT Idx = 0; Idx < SpawnLocations.Num(); Idx++)
							{
								INT NewIdx = Idx + (appRand()%(SpawnLocations.Num()-Idx));
								SpawnLocations.SwapItems(NewIdx,Idx);
								if (bOrientationMatchesLocation)
								{
									SpawnOrientations.SwapItems(NewIdx,Idx);
								}
							}
							LastSpawnIdx = -1;
							break;
						case PS_Normal:
							break;
						case PS_Reverse:
							// reverse the list
							for (INT Idx = 0; Idx < SpawnLocations.Num()/2; Idx++)
							{
								SpawnLocations.SwapItems(Idx,SpawnLocations.Num()-1-Idx);
								if (bOrientationMatchesLocation)
								{
									SpawnOrientations.SwapItems(Idx,SpawnLocations.Num()-1-Idx);
								}
							}
							break;
						default:
							break;
						}
					}
					AActor *NewSpawn = NULL;
					INT SpawnIdx = LastSpawnIdx;
					for (INT Count = 0; Count < SpawnLocations.Num() && NewSpawn == NULL; Count++)
					{
						if (++SpawnIdx >= SpawnLocations.Num())
						{
							SpawnIdx = 0;
						}
						FVector* Point = &(SpawnLocations(SpawnIdx));
						FRotator Rotator(0,0,0);
						
						if (SpawnOrientations.Num() > 0)
						{
							if (bOrientationMatchesLocation)
							{
								Rotator = SpawnOrientations(SpawnIdx).Rotation();
							}
							else
							{
								// Just use the first (possibly only) entry...
								Rotator = SpawnOrientations(0).Rotation();
							}
						}
						if (Point)
						{
							// attempt to create the new actor
							CurrentSpawnIdx = SpawnIdx;
							NewSpawn = Factory->CreateActor(Point,&Rotator, this);
							// if we created the actor
							if (NewSpawn != NULL)
							{
								NewSpawn->bKillDuringLevelTransition = TRUE;
								NewSpawn->eventSpawnedByKismet();
								KISMET_LOG(TEXT("Spawned %s at %s"),*NewSpawn->GetName(),*(Point->ToString()));
								// increment the spawned count
								SpawnedCount++;
								Spawned(NewSpawn);
								LastSpawnIdx = SpawnIdx;
							}
							else
							{
								KISMET_WARN(TEXT("Failed to spawn at %s using factory %s"),*(Point->ToString()),*Factory->GetName());
							}
						}
					}
				}
				// set a new delay
				RemainingDelay = SpawnDelay;
			}
			else
			{
				// reduce the delay
				RemainingDelay -= DeltaTime;
			}
			// if we've spawned the limit return true to finish the op
			return (SpawnedCount >= SpawnCount);
		}
		else
		{
			// abort the spawn process
			if (Factory == NULL)
			{
				KISMET_WARN(TEXT("Actor factory action %s has an invalid factory!"),*GetFullName());
			}
			if (SpawnPoints.Num() == 0)
			{
				KISMET_WARN(TEXT("Actor factory action %s has no spawn points!"),*GetFullName());
			}
			return TRUE;
		}
	}
	else
	{
		// finish the action, no need to spawn anything
		return TRUE;
	}
}

void USeqAct_ActorFactory::Spawned(UObject *NewSpawn)
{
	// assign to any Linked variables
	TArray<UObject**> ObjVars;
	GetObjectVars(ObjVars,TEXT("Spawned"));
	for (INT Idx = 0; Idx < ObjVars.Num(); Idx++)
	{
		*(ObjVars(Idx)) = NewSpawn;
	}
	// activate the spawned output
	OutputLinks(0).bHasImpulse = TRUE;
}

void USeqAct_ActorFactory::DeActivated()
{
	// do nothing, since outputs have already been activated
	bIsSpawning = FALSE;
}

void USeqAct_ActorFactoryEx::UpdateDynamicLinks()
{
	Super::UpdateDynamicLinks();
	INT OutputLinksDelta = OutputLinks.Num() - (SpawnCount + 2);
	if (OutputLinksDelta > 0)
	{
		// remove the extras
		OutputLinks.Remove(OutputLinks.Num()-OutputLinksDelta,OutputLinksDelta); 
	}
	else
	if (OutputLinksDelta < 0)
	{
		// add new entries
		OutputLinks.AddZeroed(Abs(OutputLinksDelta));
		// save room for the finished/aborted links
		for (INT Idx = 2; Idx < SpawnCount + 2; Idx++)
		{
			OutputLinks(Idx).LinkDesc = FString::Printf(TEXT("Spawned %d"),Idx-2+1);
		}
	}

	// sync up variable links
	TArray<INT> ValidLinkIndices;
	for (INT Idx = 0; Idx < SpawnCount; Idx++)
	{
		FString LinkDesc = FString::Printf(TEXT("Spawned %d"),Idx+1);
		UBOOL bFoundLink = FALSE;
		for (INT VarIdx = 0; VarIdx < VariableLinks.Num() && !bFoundLink; VarIdx++)
		{
			if (VariableLinks(VarIdx).LinkDesc == LinkDesc)
			{
				ValidLinkIndices.AddItem(VarIdx);
				bFoundLink = TRUE;
			}
		}
		if (!bFoundLink)
		{
			// add a new entry
			INT VarIdx = VariableLinks.AddZeroed();
			VariableLinks(VarIdx).LinkDesc = LinkDesc;
			VariableLinks(VarIdx).ExpectedType = USeqVar_Object::StaticClass();
			VariableLinks(VarIdx).MinVars = 0;
			VariableLinks(VarIdx).MaxVars = 255;
			VariableLinks(VarIdx).bWriteable = TRUE;
			ValidLinkIndices.AddItem(VarIdx);
		}
	}
	// cull out dead links
	for (INT VarIdx = 0; VarIdx < VariableLinks.Num(); VarIdx++)
	{
		if (VariableLinks(VarIdx).PropertyName == NAME_None &&
			!ValidLinkIndices.ContainsItem(VarIdx))
		{
			VariableLinks.Remove(VarIdx--,1);
		}
	}
}

void USeqAct_ActorFactoryEx::Spawned(UObject *NewSpawn)
{
	INT SpawnIdx = SpawnedCount;
	// write out the appropriate variable
	FString LinkDesc = FString::Printf(TEXT("Spawned %d"),SpawnIdx);
	TArray<UObject**> ObjVars;
	GetObjectVars(ObjVars,*LinkDesc);
	for (INT Idx = 0; Idx < ObjVars.Num(); Idx++)
	{
		*(ObjVars(Idx)) = NewSpawn;
	}
	// activate the proper outputs
	OutputLinks(0).ActivateOutputLink();
	for (INT Idx = 0; Idx < OutputLinks.Num(); Idx++)
	{
		if (OutputLinks(Idx).LinkDesc == LinkDesc)
		{
			OutputLinks(Idx).ActivateOutputLink();
			break;
		}
	}
}

//
//	USeqAct_ProjectileFactory
//
void USeqAct_ProjectileFactory::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Reject factories that can't be used in-game because they refer to unspawnable types.
	if (Factory != NULL)
	{
		UActorFactoryArchetype* ArchetypeFactory = Cast<UActorFactoryArchetype>(Factory);
		if (ArchetypeFactory == NULL)
		{
			appMsgf(AMT_OK, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("ActorFactoryNotForUseByProjectileFactoryF"),*Factory->GetClass()->GetName())) );
			Factory = NULL;
		}
		if (Factory->NewActorClass)
		{
			if ((Factory->NewActorClass == Factory->GetClass()->GetDefaultObject<UActorFactory>()->NewActorClass) &&
				(Factory->NewActorClass->GetDefaultActor()->bNoDelete) &&
				((Factory->GameplayActorClass == NULL) || (Factory->GameplayActorClass->GetDefaultActor()->bNoDelete)))
			{
				appMsgf( AMT_OK, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("ActorFactoryNotForUseByKismetF"),*Factory->GetClass()->GetName())) );
				Factory = NULL;
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void USeqAct_ProjectileFactory::DeActivated()
{
	Super::DeActivated();
}

UBOOL USeqAct_ProjectileFactory::UpdateOp(FLOAT DeltaTime)
{
	CheckToggle();
	if (bEnabled && bIsSpawning)
	{
		if ((Factory != NULL) && ((SpawnPoints.Num() > 0) || (SpawnLocations.Num() > 0)))
		{
			// if the delay has been exceeded
			if (RemainingDelay <= 0.f)
			{
				if (SpawnPoints.Num() > 0)
				{
					// This sequence always uses the 0 spawn point!
					AActor *NewSpawn = NULL;
					INT SpawnIdx = 0;
					AActor* Point = SpawnPoints(SpawnIdx);
					if (Point)
					{
						FVector SpawnLocation = Point->Location;
						FRotator SpawnRotation = Point->Rotation;

						ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>(Point);
						if ((SkelMeshActor != NULL) && (SkelMeshActor->SkeletalMeshComponent != NULL))
						{
							if (SocketName != NAME_None)
							{
								// Get the socket location
								SkelMeshActor->SkeletalMeshComponent->GetSocketWorldLocationAndRotation(SocketName, SpawnLocation, &SpawnRotation);
							}
							else if (BoneName != NAME_None)
							{
								FQuat BoneQuat = SkelMeshActor->SkeletalMeshComponent->GetBoneQuaternion(BoneName);
								SpawnLocation = SkelMeshActor->SkeletalMeshComponent->GetBoneLocation(BoneName);
								SpawnRotation = BoneQuat.Rotator();
							}
						}
						// attempt to create the new actor
						CurrentSpawnIdx = SpawnIdx;
						NewSpawn = Factory->CreateActor(&SpawnLocation, &SpawnRotation, this);
						// if we created the actor
						if (NewSpawn != NULL)
						{
							NewSpawn->bKillDuringLevelTransition = TRUE;
							NewSpawn->eventSpawnedByKismet();
							KISMET_LOG(TEXT("Spawned %s at %s"),*NewSpawn->GetName(),*Point->GetName());
							// increment the spawned count
							SpawnedCount++;
							Spawned(NewSpawn);
							LastSpawnIdx = SpawnIdx;
						}
						else
						{
							KISMET_WARN(TEXT("Failed to spawn at %s using factory %s"),*Point->GetName(),*Factory->GetName());
						}
					}
				}
				else
				{
 					// This is current unsupported in this sequence!
				}
				// set a new delay
				RemainingDelay = SpawnDelay;
			}
			else
			{
				// reduce the delay
				RemainingDelay -= DeltaTime;
			}
			// if we've spawned the limit return true to finish the op
			return (SpawnedCount >= SpawnCount);
		}
		else
		{
			// abort the spawn process
			if (Factory == NULL)
			{
				KISMET_WARN(TEXT("Actor factory action %s has an invalid factory!"),*GetFullName());
			}
			if (SpawnPoints.Num() == 0)
			{
				KISMET_WARN(TEXT("Actor factory action %s has no spawn points!"),*GetFullName());
			}
			return TRUE;
		}
	}
	else
	{
		// finish the action, no need to spawn anything
		return TRUE;
	}
}

void USeqAct_ProjectileFactory::Spawned(UObject *NewSpawn)
{
	Super::Spawned(NewSpawn);
	// Spawn a muzzle flash is present...
	if (SpawnPoints.Num() > 0)
	{
		AActor* Point = SpawnPoints(0);
		ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>(Point);
		if ((SkelMeshActor != NULL) && (SkelMeshActor->SkeletalMeshComponent != NULL))
		{
			if ((PSTemplate != NULL) && (GWorld != NULL))
			{
				AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
				if (WorldInfo != NULL)
				{
					if (GWorld->GetWorldInfo()->MyEmitterPool != NULL)
					{
						UParticleSystemComponent* PSysComp = GWorld->GetWorldInfo()->MyEmitterPool->GetPooledComponent(PSTemplate, FALSE);
						if (PSysComp != NULL)
						{
							OBJ_SET_DELEGATE(PSysComp, OnSystemFinished, GWorld->GetWorldInfo()->MyEmitterPool, FName(TEXT("OnParticleSystemFinished")));
							PSysComp->TickGroup = TG_EffectsUpdateWork;
							PSysComp->AbsoluteTranslation = FALSE;
 							PSysComp->AbsoluteRotation = FALSE;
 							PSysComp->AbsoluteScale = FALSE;
							if (SocketName != NAME_None)
							{
								SkelMeshActor->SkeletalMeshComponent->AttachComponentToSocket(PSysComp, SocketName);
							}
							else if (BoneName != NAME_None)
							{
								SkelMeshActor->SkeletalMeshComponent->AttachComponent(PSysComp, BoneName);
							}
							else
							{
								// Just attach it?
								SkelMeshActor->AttachComponent(PSysComp);
							}
							PSysComp->ActivateSystem(TRUE);
						}
					}
				}
			}
		}
	}
}

/**
 * Activates the associated output Link of the outer Sequence.
 */
void USeqAct_FinishSequence::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	USequence *Seq = ParentSequence;
	if (Seq != NULL)
	{
		// iterate through output links, looking for a match
		for (INT Idx = 0; Idx < Seq->OutputLinks.Num(); Idx++)
		{
			FSeqOpOutputLink& OutputLink = Seq->OutputLinks(Idx);
			// don't activate if the link is disabled, or if we're in the editor and it's disabled for PIE only
			if (OutputLink.LinkedOp == this && !OutputLink.bDisabled &&
				!(OutputLink.bDisabledPIE && GIsEditor))
			{
				// add any linked ops to the parent's active list
				for (INT OpIdx = 0; OpIdx < OutputLink.Links.Num(); OpIdx++)
				{
					FSeqOpOutputInputLink& Link = OutputLink.Links(OpIdx);
					USequenceOp *LinkedOp = Link.LinkedOp;

					if ( LinkedOp != NULL && LinkedOp->InputLinks.IsValidIndex(Link.InputLinkIdx) )
					{
						check(Seq->ParentSequence!=NULL);

						FLOAT ActivateDelay = OutputLink.ActivateDelay + LinkedOp->InputLinks(Link.InputLinkIdx).ActivateDelay;
						if (ActivateDelay > 0.f)
						{
							Seq->ParentSequence->QueueDelayedSequenceOp(this, &Link, ActivateDelay);			
						}
						else if( LinkedOp->InputLinks(Link.InputLinkIdx).ActivateInputLink() )
						{
							Seq->ParentSequence->QueueSequenceOp(LinkedOp,TRUE);
							// check if we should log the object comment to the screen
							//@fixme - refactor this to avoid the code duplication
							if (GAreScreenMessagesEnabled && (GEngine->bOnScreenKismetWarnings) && (LinkedOp->bOutputObjCommentToScreen))
							{
								// iterate through the controller list
								for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
								{
									// if it's a player
									if (Controller->IsA(APlayerController::StaticClass()))
									{
										((APlayerController*)Controller)->eventClientMessage(LinkedOp->ObjComment,NAME_None);
									}
								}
							}          
						}
					}
				}
				break;
			}
		}
	}
}

/**
 * Force parent Sequence to update connector Links.
 */
void USeqAct_FinishSequence::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	USequence *Seq = Cast<USequence>(GetOuter());
	if (Seq != NULL)
	{
		Seq->UpdateConnectors();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Updates the Links on the outer Sequence.
 */
void USeqAct_FinishSequence::OnCreated()
{
	Super::OnCreated();
	USequence *Seq = Cast<USequence>(GetOuter());
	if (Seq != NULL)
	{
		Seq->UpdateConnectors();
	}
}

/**
 * Verifies max interact distance, then performs normal Event checks.
 */
UBOOL USeqEvent_Used::CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest, TArray<INT>* ActivateIndices, UBOOL bPushTop)
{
	check(InOriginator);
	check(InInstigator);

	UBOOL bHasUnusedOutput = FALSE;
	if (ActivateIndices != NULL)
	{
		for (INT Idx = 0; Idx < ActivateIndices->Num(); Idx++)
		{
			if ((*ActivateIndices)(Idx) == 1)
			{
				bHasUnusedOutput = TRUE;
				break;
			}
		}
	}

	UBOOL bActivated = 0;
	if ((InOriginator->Location - InInstigator->Location).Size() <= InteractDistance || 
		InOriginator->IsA(ATrigger::StaticClass()))											// No need to check InteractDistance if already touching a trigger
	{
		UBOOL bFoundIgnoredClass = FALSE;

		// check for Ignored classes
		for( INT IgnoredIdx = 0; IgnoredIdx < IgnoredClassProximityTypes.Num(); ++IgnoredIdx )
		{
			if(InInstigator->IsA(IgnoredClassProximityTypes(IgnoredIdx)))
			{
				bFoundIgnoredClass = TRUE;
				break;
			}
		}

		UBOOL bPassed = FALSE;
		if( bFoundIgnoredClass == FALSE )
		{
			if (ClassProximityTypes.Num() > 0)
			{
				for (INT Idx = 0; Idx < ClassProximityTypes.Num(); Idx++)
				{
					if (InInstigator->IsA(ClassProximityTypes(Idx)))
					{
						bPassed = TRUE;
						break;
					}
				}
			}
			else
			{
				bPassed = TRUE;
			}
		}

		if (bPassed)
		{
			bActivated = Super::CheckActivate(InOriginator,InInstigator,bTest,ActivateIndices,bPushTop);
			if (bActivated)
			{
				// set the used distance
				TArray<FLOAT*> floatVars;
				GetFloatVars(floatVars,TEXT("Distance"));
				if (floatVars.Num() > 0)
				{
					FLOAT distance = (InInstigator->Location-InOriginator->Location).Size();
					for (INT Idx = 0; Idx < floatVars.Num(); Idx++)
					{
						*(floatVars(Idx)) = distance;
					}
				}
			}
		}
	}
	// try to activate the unused output even if the normal output failed
	if (!bActivated && bHasUnusedOutput)
	{
		TArray<INT> NewIndices;
		NewIndices.AddItem(1);
		bActivated = Super::CheckActivate(InOriginator,InInstigator,bTest,&NewIndices,bPushTop);
	}
	return bActivated;
}

void USeqAct_ConvertToString::AppendVariables(TArray<USequenceVariable*> &LinkedVariables, FString &CombinedString, INT &VarCount)
{
	for (INT Idx = 0; Idx < LinkedVariables.Num(); Idx++)
	{
		USequenceVariable *Var = LinkedVariables(Idx);
		if (Var != NULL)
		{
			if (VarCount > 0)
			{
				CombinedString += VarSeparator;
			}
			if (bIncludeVarComment && Var->ObjComment.Len() > 0)
			{
				CombinedString += Var->ObjComment + TEXT(": ") + Var->GetValueStr();
			}
			else
			{
				CombinedString += Var->GetValueStr();
			}
			VarCount++;
		}
	}
}

void USeqAct_ConvertToString::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	TArray<FString*> OutStrings;
	GetStringVars(OutStrings,TEXT("Output"));
	if (OutStrings.Num() > 0 && VariableLinks.Num() > 0)
	{
		FString CombinedString;
		INT VarCount = 0;
		for (INT Idx = 0; Idx < VariableLinks.Num() - 1 && Idx < NumberOfInputs; Idx++)
		{
			AppendVariables(VariableLinks(Idx).LinkedVariables,CombinedString,VarCount);
		}
		// write out the results
		for (INT Idx = 0; Idx < OutStrings.Num(); Idx++)
		{
			*(OutStrings(Idx)) = CombinedString;
		}
	}
}

void USeqAct_ConvertToString::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("NumberOfInputs")) )
	{
		NumberOfInputs = Max(NumberOfInputs,1);
		INT CurrentNumOfInputs = VariableLinks.Num() - 1;
		if (CurrentNumOfInputs > NumberOfInputs)
		{
			VariableLinks.Remove(0,CurrentNumOfInputs - NumberOfInputs);
		}
		else if (CurrentNumOfInputs < NumberOfInputs)
		{
			INT NumToAdd = NumberOfInputs - CurrentNumOfInputs;
			INT Idx = 0;
			VariableLinks.InsertZeroed(0,NumToAdd);
			const USeqAct_ConvertToString *DefAction = GetDefault<USeqAct_ConvertToString>();
			while (NumToAdd-- > 0)
			{
				VariableLinks(Idx++) = DefAction->VariableLinks(0);
			}
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

//==========================
// USeqAct_Log interface

/**
 * Dumps the contents of all attached variables to the log, and optionally
 * to the screen.
 */
void USeqAct_Log::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints
	OutputLog();
}

void USeqAct_Log::OutputLog()
{
#if !FINAL_RELEASE	// don't log anything in FINAL_RELEASE
	// for all attached variables
	FString LogString("");
	for (INT VarIdx = 0; VarIdx < VariableLinks.Num(); VarIdx++)
	{
		if (VariableLinks(VarIdx).PropertyName == FName(TEXT("Targets")))
		{
			continue;
		}
		for (INT Idx = 0; Idx < VariableLinks(VarIdx).LinkedVariables.Num(); Idx++)
		{
			USequenceVariable *Var = VariableLinks(VarIdx).LinkedVariables(Idx);
			if (Var != NULL)
			{
				USeqVar_RandomInt *RandInt = Cast<USeqVar_RandomInt>(Var);
				if (RandInt != NULL)
				{
					INT *IntValue = RandInt->GetRef();
					LogString = FString::Printf(TEXT("%s %d"),*LogString,*IntValue);
				}
				else
				{
					LogString = FString::Printf(TEXT("%s %s"),*LogString,*Var->GetValueStr());
				}
			}
		}
	}
	if (bIncludeObjComment)
	{
		LogString += LogMessage;
	}
	debugf(NAME_Log,TEXT("Kismet: %s"),*LogString);
	if (bOutputToScreen && (GEngine->bOnScreenKismetWarnings))
	{
		// iterate through the controller list
		for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
		{
			// if it's a player
			if (Controller->IsA(APlayerController::StaticClass()))
			{
				((APlayerController*)Controller)->eventClientMessage(LogString,NAME_None);
			}
		}
	}
	if (Targets.Num() > 0)
	{
		// iterate through the controller list
		for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
		{
			// if it's a player
			if (Controller->IsA(APlayerController::StaticClass()))
			{
				APlayerController *PC = Cast<APlayerController>(Controller);
				for (INT Idx = 0; Idx < Targets.Num(); Idx++)
				{
					AActor *Actor = Cast<AActor>(Targets(Idx));
					if (Actor != NULL)
					{
						PC->eventAddDebugText(LogString,Actor,TargetDuration,TargetOffset);
					}
				}
			}
		}
	}
#endif
}

void USeqAct_Log::PostLoad()
{
#if !FINAL_RELEASE
	// Cache the obj comment which is our log message. ObjComment is cleared in USequenceObject::PostLoad to save memory on consoles.
	LogMessage = ObjComment;
#endif
	Super::PostLoad();
}

/************************
  * USeqAct_FeatureTest *
  ***********************/

void USeqAct_FeatureTest::PostLoad()
{
	Super::PostLoad();
}

// Execute the FreezeAt command and initialize the delay timer
void USeqAct_FeatureTest::Activated()
{
	// Execute the FreezeAt command
	if (GEngine->GamePlayers(0) && (FreezeAtParameters != ""))
	{
		FString FreezeAtCmd = FString::Printf(TEXT("FreezeAt %s"), *FreezeAtParameters);
		UConsole* ViewportConsole = (GEngine->GameViewport != NULL) ? GEngine->GameViewport->ViewportConsole : NULL;
		FConsoleOutputDevice StrOut(ViewportConsole);
		GEngine->GamePlayers(0)->Exec(*FreezeAtCmd, StrOut);
	}
	// Initialize the delay timer
	RemainingScreenShotDelay = ScreenShotDelay;
}

// Once the delay timer runs down, output the log message and take the screenshot
UBOOL USeqAct_FeatureTest::UpdateOp(FLOAT deltaTime)
{
	RemainingScreenShotDelay -= deltaTime;
	// See if we've reached the end of the delay
	if (RemainingScreenShotDelay <= 0.0f)
	{
		// Output the log message
		OutputLog();
		// Take the screenshot
		if (GEngine->GamePlayers(0))
		{
			FString ShotCmd = (ScreenShotName != "")? FString::Printf(TEXT("SHOT NAME=%s"), *ScreenShotName): TEXT("SHOT");
			UConsole* ViewportConsole = (GEngine->GameViewport != NULL) ? GEngine->GameViewport->ViewportConsole : NULL;
			FConsoleOutputDevice StrOut(ViewportConsole);
			GEngine->GameViewport->Exec(*ShotCmd, StrOut);
		}
		// Activate the output link and signal that this op is no longer active
		OutputLinks(0).ActivateOutputLink();
		return TRUE;
	}
	// Still actively ticking down the delay
	return FALSE;
}

void USeqAct_DrawText::Activated()
{
	UBOOL bAddedPlayers = FALSE;

	DrawTextInfo.AppendedText = TEXT("");
	for( INT VarLinkIdx = 0; VarLinkIdx < VariableLinks.Num(); ++VarLinkIdx )
	{
		if( VariableLinks(VarLinkIdx).LinkDesc == FString("String") )
		{
			for( INT VarIdx = 0; VarIdx < VariableLinks(VarLinkIdx).LinkedVariables.Num(); ++VarIdx )
			{
				USequenceVariable *Var = VariableLinks(VarLinkIdx).LinkedVariables(VarIdx);
				if (Var != NULL)
				{
					DrawTextInfo.AppendedText = FString::Printf(TEXT("%s %s"), *DrawTextInfo.AppendedText, *Var->GetValueStr());
				}
			}
		}
	}

	// Check if we should display relative to the target instead of in HUD space
	if( bDisplayOnObject )
	{
		if (Targets.Num() > 0)
		{
			// iterate through the controller list
			for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
			{
				// if it's a player
				if (Controller->IsA(APlayerController::StaticClass()))
				{
					APlayerController *PC = Cast<APlayerController>(Controller);
					for (INT Idx = 0; Idx < Targets.Num(); Idx++)
					{
						AActor *Actor = Cast<AActor>(Targets(Idx));
						if (Actor != NULL)
						{
							if( InputLinks(0).bHasImpulse )
							{
								const FVector CameraSpaceOffset = FVector(0.0f, DrawTextInfo.MessageOffset.X, DrawTextInfo.MessageOffset.Y);
								PC->eventAddDebugText(FString::Printf(TEXT("%s %s"),*DrawTextInfo.MessageText, *DrawTextInfo.AppendedText), 
													  Actor, DisplayTimeSeconds, CameraSpaceOffset, CameraSpaceOffset, DrawTextInfo.MessageColor);
							}
							else if( InputLinks(1).bHasImpulse )
							{
								PC->eventRemoveDebugText(Actor);
							}
						}
					}
				}
			}
		}
	}
	// If there are no targets then we will send this action to all player controllers
	else if ( Targets.Num() == 0 )
	{
		bAddedPlayers = TRUE;
		// send to all players
		for ( AController *Controller = GWorld->GetWorldInfo()->ControllerList; Controller != NULL; Controller = Controller->NextController )
		{
			if ( Controller->IsPlayerOwned() )
			{
				Targets.AddItem( Controller );
			}
		}
	}

	// Super class will send the action to all Targets
	if( !bDisplayOnObject )
	{
		Super::Activated();
	}
	else
	{
		USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints
	}

	// If no targets were provided and we sent the action to all player controllers, empty the Targets array now.
	if ( bAddedPlayers )
	{
		Targets.Empty();
	}

	OutputLinks(0).ActivateOutputLink();
}


UBOOL USeqAct_DrawText::UpdateOp(FLOAT deltaTime)
{
	// if we're not using a display time, just jump out right away
	if( DisplayTimeSeconds < 0.f || DrawTextInfo.MessageText.Len() < 1)
	{
		return TRUE;
	}

	UBOOL bAddedPlayers = FALSE;

	// If there are no targets then we will send this action to all player controllers
	if ( Targets.Num() == 0 )
	{
		bAddedPlayers = TRUE;
		// send to all players
		for ( AController *Controller = GWorld->GetWorldInfo()->ControllerList; Controller != NULL; Controller = Controller->NextController )
		{
			if ( Controller->IsPlayerOwned() )
			{
				Targets.AddItem( Controller );
			}
		}
	}


	// If no targets were provided and we sent the action to all player controllers, empty the Targets array now.
	if( bAddedPlayers )
	{
		Targets.Empty();
	}

	return TRUE;
}


void USeqCond_CompareObject::Activated()
{
	Super::Activated();
	// grab all associated Objects
	TArray<UObject**> ObjVarsA, ObjVarsB;
	GetObjectVars(ObjVarsA,TEXT("A"));
	GetObjectVars(ObjVarsB,TEXT("B"));
	UBOOL bResult = TRUE;
	// compare everything in A
	for (INT IdxA = 0; IdxA < ObjVarsA.Num() && bResult; IdxA++)
	{
		UObject *ObjA = *(ObjVarsA(IdxA));
		// against everything in B
		for (INT IdxB = 0; IdxB < ObjVarsB.Num() && bResult; IdxB++ )
		{
			UObject *ObjB = *(ObjVarsB(IdxB));
			bResult = ObjA == ObjB;
			if (!bResult)
			{
				// attempt to swap controller for pawn and retry compare
				if (Cast<AController>(ObjA))
				{
					ObjA = ((AController*)ObjA)->Pawn;
					bResult = ObjA == ObjB;
				}
				else if (Cast<AController>(ObjB))
				{
					ObjB = ((AController*)ObjB)->Pawn;
					bResult = ObjA == ObjB;
				}
			}
		}
	}
	OutputLinks(bResult ? 0 : 1).ActivateOutputLink();
}

/* ==========================================================================================================
	Switches (USeqCond_SwitchBase)
========================================================================================================== */
void USeqCond_SwitchBase::Activated()
{
	Super::Activated();

	TArray<INT> ActivateIndices;
	GetOutputLinksToActivate(ActivateIndices);
	for ( INT Idx = 0; Idx < ActivateIndices.Num(); Idx++ )
	{
		checkSlow(OutputLinks.IsValidIndex(ActivateIndices(Idx)));

		FSeqOpOutputLink& Link = OutputLinks(ActivateIndices(Idx));
		Link.ActivateOutputLink();
	}
}

void USeqCond_SwitchBase::UpdateDynamicLinks()
{
	Super::UpdateDynamicLinks();

	INT CurrentValueCount = GetSupportedValueCount();

	// If there are too many output Links
	if( OutputLinks.Num() > CurrentValueCount )
	{
		// Search through each output description and see if the name matches the
		// remaining class names.
		for( INT LinkIndex = OutputLinks.Num() - 1; LinkIndex >= 0; LinkIndex-- )
		{
			INT ValueIndex = FindCaseValueIndex(LinkIndex);
			if ( ValueIndex == INDEX_NONE )
			{
				OutputLinks(LinkIndex).Links.Empty();
				OutputLinks.Remove( LinkIndex );
			}
		}
	}

	// If there aren't enough output Links, add some
	if ( OutputLinks.Num() < CurrentValueCount )
	{
		// want to insert the new output links at the end of the list just before the "default" item
		INT InsertIndex	= Max<INT>( OutputLinks.Num() - 1, 0 );
		OutputLinks.InsertZeroed( InsertIndex, CurrentValueCount - OutputLinks.Num() );
	}

	// make sure we have a "default" item in the OutputLinks array
	INT DefaultIndex = OutputLinks.Num() - 1;
	if ( DefaultIndex < 0 || OutputLinks(DefaultIndex).LinkDesc != TEXT("Default") )
	{
		DefaultIndex = OutputLinks.AddZeroed();
	}

	OutputLinks(DefaultIndex).LinkDesc = TEXT("Default");

	// make sure that the value array contains a "default" item as well
	eventVerifyDefaultCaseValue();

	// Make sure the LinkDesc for each output link matches the value in the value array
	for( INT LinkIndex = 0; LinkIndex < OutputLinks.Num() - 1; LinkIndex++ )
	{
		OutputLinks(LinkIndex).LinkDesc = GetCaseValueString(LinkIndex);
	}
}	

/* ==========================================================================================================
	USeqCond_SwitchObject
========================================================================================================== */
/* === USeqCond_SwitchBase interface === */
/**
 * Returns the index of the OutputLink to activate for the specified object.
 *
 * @param	out_LinksToActivate
 *						the indexes [into the OutputLinks array] for the most appropriate OutputLinks to activate
 *						for the specified object, or INDEX_NONE if none are found.  Should only contain 0 or 1 elements
 *						unless one of the matching cases is configured to fall through.
 *
 * @return	TRUE if at least one match was found, FALSE otherwise.
 */
UBOOL USeqCond_SwitchObject::GetOutputLinksToActivate( TArray<INT>& out_LinksToActivate )
{
	UBOOL bResult = FALSE;

	TArray<UObject**> ObjVars;
	GetObjectVars(ObjVars, TEXT("Object"));

	// For each item in the Object list
	for( INT ObjIdx = 0; ObjIdx < ObjVars.Num(); ObjIdx++ )
	{
		// If this entry is invalid - skip
		if ( ObjVars(ObjIdx) == NULL )
		{
			continue;
		}

		UBOOL bFoundMatch = FALSE;
		for ( INT ValueIndex = 0; ValueIndex < SupportedValues.Num(); ValueIndex++ )
		{
			FSwitchObjectCase& Value = SupportedValues(ValueIndex);
			if ( !Value.bDefaultValue && Value.ObjectValue == *(ObjVars(ObjIdx)) )
			{
				out_LinksToActivate.AddUniqueItem(ValueIndex);
				bResult = bFoundMatch = TRUE;

				if ( !Value.bFallThru )
				{
					break;
				}
			}
		}

		if ( !bFoundMatch && SupportedValues.Num() > 0 )
		{
			out_LinksToActivate.AddUniqueItem(SupportedValues.Num() - 1);
			// don't set bResult to TRUE if we're activating the default item
		}
	}

	return bResult;
}

/**
 * Returns the index [into the switch op's array of values] that corresponds to the specified OutputLink.
 *
 * @param	OutputLinkIndex		index into [into the OutputLinks array] to find the corresponding value index for
 *
 * @return	INDEX_NONE if no value was found which matches the specified output link.
 */
INT USeqCond_SwitchObject::FindCaseValueIndex( INT OutputLinkIndex ) const
{
	INT Result = INDEX_NONE;

	if ( OutputLinks.IsValidIndex(OutputLinkIndex) )
	{
		if ( OutputLinks(OutputLinkIndex).LinkDesc == TEXT("Default") )
		{
			for ( INT ValueIndex = SupportedValues.Num() - 1; ValueIndex >= 0; ValueIndex-- )
			{
				if ( SupportedValues(ValueIndex).bDefaultValue )
				{
					Result = ValueIndex;
					break;
				}
			}
		}
		else
		{
			for ( INT ValueIndex = 0; ValueIndex < SupportedValues.Num(); ValueIndex++ )
			{
				const FSwitchObjectCase& Value = SupportedValues(ValueIndex);
				if ( Value.ObjectValue != NULL && Value.ObjectValue->GetName() == OutputLinks(OutputLinkIndex).LinkDesc )
				{
					Result = ValueIndex;
					break;
				}
			}
		}
	}

	return Result;
}

/** Returns the number of elements in this switch op's array of values. */
INT USeqCond_SwitchObject::GetSupportedValueCount() const
{
	return SupportedValues.Num();
}

/**
 * Returns a string representation of the value at the specified index.  Used to populate the LinkDesc for the OutputLinks array.
 */
FString USeqCond_SwitchObject::GetCaseValueString( INT ValueIndex ) const
{
	FString Result;

	if ( SupportedValues.IsValidIndex(ValueIndex) )
	{
		if ( SupportedValues(ValueIndex).bDefaultValue )
		{
			Result = TEXT("Default");
		}
		else
		{
			Result = SupportedValues(ValueIndex).ObjectValue->GetName();
		}
	}

	return Result;
}

/* ==========================================================================================================
	USeqCond_SwitchClass
========================================================================================================== */
/* === USeqCond_SwitchBase interface === */
/**
 * Returns the index of the OutputLink to activate for the specified object.
 *
 * @param	out_LinksToActivate
 *						the indexes [into the OutputLinks array] for the most appropriate OutputLinks to activate
 *						for the specified object, or INDEX_NONE if none are found.  Should only contain 0 or 1 elements
 *						unless one of the matching cases is configured to fall through.
 *
 * @return	TRUE if at least one match was found, FALSE otherwise.
 */
UBOOL USeqCond_SwitchClass::GetOutputLinksToActivate( TArray<INT>& out_LinksToActivate )
{
	UBOOL bResult = FALSE;

	TArray<UObject**> ObjList;
	GetObjectVars( ObjList, TEXT("Object") );

	// For each item in the Object list
	for( INT ObjIdx = 0; ObjIdx < ObjList.Num(); ObjIdx++ )
	{
		// If actor is invalid - skip
		if (ObjList(ObjIdx) == NULL || (*ObjList(ObjIdx)) == NULL)
		{
			continue;
		}

		UBOOL bExit = FALSE;
		
		// Check it against each item in the names list (while the exit flag hasn't been marked)
		for( INT OutIdx = 0; OutIdx < ClassArray.Num() && !bExit; OutIdx++ )
		{
			if ( ClassArray(OutIdx).ClassName == NAME_Default )
			{
				// Set impulse and exit if we found default
				out_LinksToActivate.AddUniqueItem(OutIdx);
				bExit = TRUE;
				// don't set bResult to TRUE if we're activating the default item
				break;
			}
			else
			{
				// Search the class tree to see if it matches the name on this output index
				UObject* CurObj = *ObjList(ObjIdx);
				for( UClass* TempClass = CurObj->GetClass(); TempClass; TempClass = TempClass->GetSuperClass() )
				{
					FName CurName = ClassArray(OutIdx).ClassName;
					// If the Object matches a class in this hierarchy
					if( TempClass->GetFName() == CurName )
					{
						// Set the output impulse
						out_LinksToActivate.AddUniqueItem(OutIdx);
						bResult = TRUE;

						// If we aren't supposed to fall through - exit
						if( !ClassArray(OutIdx).bFallThru )
						{
							bExit = TRUE;
							break;
						}
					}
				}
			}
		}
	}

	return bResult;
}

/**
 * Returns the index [into the switch op's array of values] that corresponds to the specified OutputLink.
 *
 * @param	OutputLinkIndex		index into [into the OutputLinks array] to find the corresponding value index for
 *
 * @return	INDEX_NONE if no value was found which matches the specified output link.
 */
INT USeqCond_SwitchClass::FindCaseValueIndex( INT OutputLinkIndex ) const
{
	INT Result = INDEX_NONE;

	// Search through each output description and see if the name matches the
	// remaining class names.
	for( Result = ClassArray.Num() - 1; Result >= 0; Result-- )
	{
		if ( OutputLinks(OutputLinkIndex).LinkDesc == ClassArray(Result).ClassName.ToString() )
		{
			break;
		}
	}

	return Result;
}

/** Returns the number of elements in this switch op's array of values. */
INT USeqCond_SwitchClass::GetSupportedValueCount() const
{
	return ClassArray.Num();
}

/**
 * Returns a string representation of the value at the specified index.  Used to populate the LinkDesc for the OutputLinks array.
 */
FString USeqCond_SwitchClass::GetCaseValueString( INT ValueIndex ) const
{
	FString Result;

	if ( ClassArray.IsValidIndex(ValueIndex) )
	{
		Result = ClassArray(ValueIndex).ClassName.ToString();
	}

	return Result;
}

//============================
// USeqAct_Delay interface

void USeqAct_Delay::PostLoad()
{
	Super::PostLoad();
	// check for old versions where default duration was manually set
	USeqAct_Delay *DefaultObj = GetArchetype<USeqAct_Delay>();
	checkSlow(DefaultObj);

	if (DefaultDuration != DefaultObj->DefaultDuration &&
		Duration == DefaultObj->Duration)
	{
		// update the new duration
		Duration = DefaultDuration;
	}
}

/**
 * Sets the remaining time and activates the delay.
 */
void USeqAct_Delay::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	// set the time based on the current duration
	RemainingTime = Duration;
	LastUpdateTime = GWorld->GetWorldInfo()->TimeSeconds;
}

void USeqAct_Delay::ResetDelayActive()
{
	bDelayActive = FALSE;
}


/**
 * Determines whether or not the delay has finished.
 */
UBOOL USeqAct_Delay::UpdateOp(FLOAT DeltaTime)
{
	// check for a start/stop
	if (InputLinks(0).bHasImpulse)
	{
		if(bStartWillRestart)
		{
			//reset the time
			RemainingTime = Duration;
			LastUpdateTime = GWorld->GetWorldInfo()->TimeSeconds;
		}

		// start the ticking
		bDelayActive = TRUE;
	}
	else
	if (InputLinks(1).bHasImpulse)
	{
		// kill the op w/o activating any outputs
		bDelayActive = FALSE;
		return TRUE;
	}
	else
	if (InputLinks(2).bHasImpulse)
	{
		// pause the ticking
		bDelayActive = FALSE;
	}
	if (bDelayActive)
	{
		FLOAT CurrentTime = GWorld->GetWorldInfo()->TimeSeconds;
		// only update if this is a new tick
		if (CurrentTime != LastUpdateTime)
		{
			RemainingTime -= DeltaTime;
			if (RemainingTime <= 0.f)
			{
				// activate outputs
				OutputLinks(0).ActivateOutputLink();
				return TRUE;
			}
		}
	}
	// keep on ticking
	return FALSE;
}

void USeqAct_Delay::DeActivated()
{
	// do nothing, outputs activated in UpdateOp
}

//============================
// USeqAct_StreamInTextures interface

void USeqAct_StreamInTextures::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SelectedCinematicTextureGroups = UTexture::GetTextureGroupBitfield( CinematicTextureGroups );
}

void USeqAct_StreamInTextures::PostLoad()
{
	Super::PostLoad();

	//IMPORTANT: Renames OutputLink 0 from "Finished" to "Out" without marking the package as dirty.
	// If the link configuration changes, make sure to update the version and move this code to UpdateObject, with version checking.
	if ( OutputLinks.Num() > 1 && OutputLinks(0).LinkDesc == TEXT("Finished") )
	{
		OutputLinks(0).LinkDesc = TEXT("Out");
	}

	SelectedCinematicTextureGroups = UTexture::GetTextureGroupBitfield( CinematicTextureGroups );
}

/**
 * Sets the remaining time and activates the delay.
 */
void USeqAct_StreamInTextures::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints
}

/**
 * Determines whether or not the delay has finished.
 */
UBOOL USeqAct_StreamInTextures::UpdateOp(FLOAT DeltaTime)
{
	UBOOL bWasActive = bStreamingActive;
	UBOOL bReturnValue = FALSE;

	// Start
	if (InputLinks(0).bHasImpulse)
	{
		// start the ticking
		bStreamingActive = TRUE;
		StopTimestamp = (appSeconds() - GStartTime) + Seconds;
		ApplyForceMipSettings(TRUE, Seconds);

		// This action is not supposed to block until complete so immediately activate the next action.
		OutputLinks(0).ActivateOutputLink();
	}
	// Stop
	else if (InputLinks(1).bHasImpulse)
	{
		bStreamingActive = FALSE;
		ApplyForceMipSettings(FALSE, 0.f);

		// kill the op
		bReturnValue = TRUE;
	}

	FLOAT Duration = 0.0f;

	if ( bStreamingActive )
	{
		FLOAT CurrentTime = appSeconds() - GStartTime;
		if ( CurrentTime < StopTimestamp )
		{
			Duration = StopTimestamp - CurrentTime;
		}
		else
		{
			// Timer expired, turn off.
			bStreamingActive = FALSE;

			// kill and activate outputs
			bReturnValue = TRUE;
		}
	}

	// Did we start/stop an actor-based action?
	if ( bWasActive != bStreamingActive || bReturnValue )
	{
		// Add the location of all targets to the texture streaming system.
		for (INT TargetIndex = 0; TargetIndex < LocationActors.Num(); TargetIndex++)
		{
			AActor* Target = Cast<AActor>(LocationActors(TargetIndex));
			if ( Target )
			{
				GStreamingManager->AddViewSlaveLocation( Target->Location, StreamingDistanceMultiplier, FALSE, Duration );
				for (AController* Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
				{
					APlayerController* PC = Cast<APlayerController>(Controller);
					// If it's a remote player
					if (PC != NULL && !PC->IsLocalPlayerController())
					{
						// Tell it to stream in textures for the Target actor.
						PC->eventClientAddTextureStreamingLoc(Target->Location, Duration, false);
					}
				}
			}
		}

		// Update the targets' texture streaming
		for (INT TargetIndex = 0; TargetIndex < Targets.Num(); TargetIndex++)
		{
			AActor* Target = Cast<AActor>(Targets(TargetIndex));
			if ( Target )
			{
				// If target is a Controller, defer to the Pawn
				AController* TargetController = Target->GetAController();
				if ( TargetController && TargetController->Pawn )
				{
					Target = TargetController->Pawn;
				}

				Target->PrestreamTextures( Duration, bStreamingActive, SelectedCinematicTextureGroups );

				// Find all remote players.
				for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
				{
					APlayerController* PC = Cast<APlayerController>(Controller);
					// If it's a remote player
					if ( PC && !PC->IsLocalPlayerController() )
					{
						// Tell it to stream in textures for the Target actor.
						PC->eventClientPrestreamTextures( Target, Duration, bStreamingActive, SelectedCinematicTextureGroups );
					}
				}
			}
		}
	}

	// Fire the AllLoaded output if we're finishing the action, or
	// if there are 0 NumWantingResources and the streaming system has done at least 2 full passes.
	if ( !bHasTriggeredAllLoaded )
	{
		INT CurrentNumWanting = GStreamingManager->GetNumWantingResources();
		INT CurrentStreamingID = GStreamingManager->GetNumWantingResourcesID();
		if ( bReturnValue ||
			 (NumWantingResourcesID != 0 && Abs(NumWantingResourcesID - CurrentStreamingID) >= 2 && CurrentNumWanting == 0) )
		{
			// Activate the AllLoaded output, if it's used (i.e. not an old version)
			if ( OutputLinks.Num() >= 3 )
			{
				OutputLinks(2).ActivateOutputLink();
			}

			bHasTriggeredAllLoaded = TRUE;
		}
		else if ( NumWantingResourcesID == 0 )
		{
			// Remember the current ID so we can know when the streaming system has done two full passes, from this point in time.
			NumWantingResourcesID = CurrentStreamingID;
		}
	}

	// keep on ticking
	return bReturnValue;
}

void USeqAct_StreamInTextures::DeActivated()
{
	// do nothing, no outputs activated
}

void USeqAct_StreamInTextures::UpdateObject()
{
	if ( ObjInstanceVersion < eventGetObjClassVersion() )
	{
		VariableLinks.AddZeroed(1);
		VariableLinks(0).LinkDesc = FString("Actor");
		VariableLinks(1).ExpectedType = USeqVar_Object::StaticClass();
		VariableLinks(1).LinkDesc = FString("Location");
		VariableLinks(1).PropertyName = FName(TEXT("LocationActors"));
#if WITH_EDITORONLY_DATA
		if ( bLocationBased_DEPRECATED )
		{
			VariableLinks(1).LinkedVariables = VariableLinks(0).LinkedVariables;
			VariableLinks(0).LinkedVariables.Empty();
		}
#endif // WITH_EDITORONLY_DATA
	}

	Super::UpdateObject();
}

/** Set the bForceMiplevelsToBeResident flag on all textures used by the materials in ForceMaterials */
void USeqAct_StreamInTextures::ApplyForceMipSettings( UBOOL bEnable, FLOAT Duration )
{
	// Are we stopping the action?
	if ( bEnable == FALSE )
	{
		// Turn off forced settings.
		Duration = 0.0f;
	}

	// Iterate over all textures used by specified materials
	for ( INT ForceMatIdx = 0; ForceMatIdx < ForceMaterials.Num(); ++ForceMatIdx)
	{
		UMaterialInterface* Mat = ForceMaterials(ForceMatIdx);
		if ( Mat )
		{
			Mat->SetForceMipLevelsToBeResident( FALSE, FALSE, Duration, SelectedCinematicTextureGroups );

			// Find all remote players.
			for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
			{
				APlayerController* PC = Cast<APlayerController>(Controller);
				// If it's a remote player
				if ( PC && !PC->IsLocalPlayerController() )
				{
					// Tell it to stream in textures for the Target actor.
					PC->eventClientSetForceMipLevelsToBeResident( Mat, Duration, SelectedCinematicTextureGroups );
				}
			}
		}
	}
}

/**
 * Adds an error message to the map check dialog if Duration is invalid
 */
#if WITH_EDITOR
void USeqAct_StreamInTextures::CheckForErrors()
{
	PublishLinkedVariableValues();

	if ( GWarn != NULL && GWarn->MapCheck_IsActive() )
	{
		// Is the 'Seconds' parameter invalid?
		if ( Seconds <= 0 )
		{
			// Is 'Start' hooked up to something?
			if ( InputLinks.Num() > 0 )
			{
				// Search for any output links pointing at this input link
				TArray<USequenceOp*> OutputOps;
				TArray<INT> OutputIndices;
				if ( KismetFindOutputLinkTo(ParentSequence, this, 0, OutputOps, OutputIndices) )
				{
					// Report a warning
					GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KismetStreamInTexturesInvalidDuration" ), *GetName() ) ), TEXT( "KismetStreamInTexturesInvalidDuration" ), MCGROUP_KISMET );
				}
			}
		}
	}

	Super::CheckForErrors();
}
#endif

//============================

/**
 * Compares the bools linked to the condition, directing output based on the result.
 */
void USeqCond_CompareBool::Activated()
{
	Super::Activated();
	bResult = TRUE;
	// iterate through each of the Linked bool variables
	TArray<UBOOL*> boolVars;
	GetBoolVars(boolVars,TEXT("Bool"));
	for (INT VarIdx = 0; VarIdx < boolVars.Num(); VarIdx++)
	{
		bResult = bResult && *(boolVars(VarIdx));
	}
	// activate the proper output based on the result value
	OutputLinks(bResult ? 0 : 1).ActivateOutputLink();
}

/**
 * Simple condition activates input based on the current netmode.
 */
void USeqCond_GetServerType::Activated()
{
	Super::Activated();
	switch (GWorld->GetNetMode())
	{
	case NM_Standalone:
		OutputLinks(0).ActivateOutputLink();
		break;
	case NM_DedicatedServer:
		OutputLinks(1).ActivateOutputLink();
		break;
	case NM_ListenServer:
		OutputLinks(2).ActivateOutputLink();
		break;
	case NM_Client:
		OutputLinks(3).ActivateOutputLink();
		break;
	}
}

/**
 * Compares the teams for all linked actors.
 */
void USeqCond_IsSameTeam::Activated()
{
	Super::Activated();
	TArray<UObject**> ObjVars;
	GetObjectVars(ObjVars,TEXT("Players"));
	UBOOL bSameTeam = TRUE;
	INT ActorCnt = 0;
	BYTE TeamNum = 0;
	for (INT Idx = 0; Idx < ObjVars.Num() && bSameTeam; Idx++)
	{
		AActor *Actor = Cast<AActor>(*(ObjVars(Idx)));
		if (Actor != NULL)
		{
			BYTE ActorTeamNum = Actor->GetTeamNum();
			if (ActorCnt == 0)
			{
				TeamNum = ActorTeamNum;
			}
			else
			if (ActorTeamNum != TeamNum)
			{
				bSameTeam = FALSE;
			}
			ActorCnt++;
		}
	}
	if (bSameTeam)
	{
		OutputLinks(0).bHasImpulse = TRUE;
	}
	else
	{
		OutputLinks(1).bHasImpulse = TRUE;
	}
}

/**
 * Checks to see if AI is in combat
 */
void USeqCond_IsInCombat::Activated()
{
	UBOOL bInCombat = FALSE;

	Super::Activated();

	TArray<UObject**> ObjVars;
	GetObjectVars(ObjVars,TEXT("Players"));

	for( INT Idx = 0; Idx < ObjVars.Num(); Idx++ )
	{
		AController *C = Cast<AController>(*(ObjVars(Idx)));
		if( !C )
		{
			APawn *P = Cast<APawn>(*(ObjVars(Idx)));
			if( P )
			{
				C = P->Controller;
			}
		}

		if( C && C->eventIsInCombat() )
		{
			bInCombat = TRUE;
			break;
		}
	}

	if( bInCombat )
	{
		OutputLinks(0).bHasImpulse = TRUE;
	}
	else
	{
		OutputLinks(1).bHasImpulse = TRUE;
	}
}

/**
 * Checks to see if Pawn is alive
 */
void USeqCond_IsAlive::Activated()
{
	UBOOL bAlive = FALSE;

	Super::Activated();

	TArray<UObject**> ObjVars;
	GetObjectVars(ObjVars,TEXT("Players"));

	for( INT Idx = 0; Idx < ObjVars.Num(); Idx++ )
	{
		AController *C = Cast<AController>(*(ObjVars(Idx)));
		if( !C )
		{
			APawn *P = Cast<APawn>(*(ObjVars(Idx)));
			if( P )
			{
				C = P->Controller;
			}
		}

		if( C && !C->bTearOff )
		{
			bAlive = TRUE;
			break;
		}
	}

	if( bAlive )
	{
		OutputLinks(0).bHasImpulse = TRUE;
	}
	else
	{
		OutputLinks(1).bHasImpulse = TRUE;
	}
}

/**
 * Returns the first wave node in the given sound node.
 */
static USoundNodeWave* FindFirstWaveNode(USoundNode *rootNode)
{
	USoundNodeWave* waveNode = NULL;
	TArray<USoundNode*> chkNodes;
	chkNodes.AddItem(rootNode);
	while (waveNode == NULL &&
		   chkNodes.Num() > 0)
	{
		USoundNode *node = chkNodes.Pop();
		if (node != NULL)
		{
			waveNode = Cast<USoundNodeWave>(node);
			for (INT Idx = 0; Idx < node->ChildNodes.Num() && waveNode == NULL; Idx++)
			{
				chkNodes.AddItem(node->ChildNodes(Idx));
			}
		}
	}
	return waveNode;
}

static void BuildSoundTargets(TArray<UObject**> const& ObjVars, TArray<UObject*> &Targets)
{
	for (INT Idx = 0; Idx < ObjVars.Num(); Idx++)
	{
		// use the pawn for any controllers if possible
		AController *Controller = Cast<AController>(*(ObjVars(Idx)));
		if (Controller != NULL && Controller->Pawn != NULL)
		{
			Targets.AddItem(Controller->Pawn);
		}
		else
		{
			Targets.AddItem(*(ObjVars(Idx)));
		}
	}
}

/* epic ===============================================
* ::Activated
*
* Plays the given soundcue on all attached actors.
*
* =====================================================
*/
void USeqAct_PlaySound::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	bStopped = FALSE;
	bDelayReached = FALSE;
	if (PlaySound != NULL)
	{
		// now that we have the targets, determine which input has the impulse
		if (InputLinks(0).bHasImpulse)
		{
			// no delay, go ahead and play the sound
			if (appIsNearlyZero(ExtraDelay))
			{
				ActivateSound();
			}

			// figure out the Latent duration
			USoundNodeWave *wave = FindFirstWaveNode( PlaySound->FirstNode );
			if (wave != NULL)
			{
				SoundDuration = ( wave->Duration + ExtraDelay ) * GWorld->GetWorldInfo()->TimeDilation;
			}
			else
			{
				SoundDuration = 0.0f;
			}
			KISMET_LOG(TEXT("-> sound duration: %2.1f"),SoundDuration);

			// Set this to false now, so UpdateOp doesn't call Activated again straight away.
			InputLinks(0).bHasImpulse = FALSE;
		}
		else if (InputLinks(1).bHasImpulse)
		{
			Stop();
		}
	}
	// activate the base output
	OutputLinks(0).ActivateOutputLink();
}

/** stops the sound on all targets */
void USeqAct_PlaySound::Stop()
{
	TArray<UObject**> ObjVars;
	TArray<UObject*> Targets;
	GetObjectVars(ObjVars,TEXT("Target"));
	BuildSoundTargets(ObjVars,Targets);
	if (ObjVars.Num() == 0)
	{
		// stop the sound cue on each player
		for (AController* C = GWorld->GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
		{
			APlayerController* PC = C->GetAPlayerController();
			if (PC != NULL)
			{
				PC->eventKismet_ClientStopSound(PlaySound, PC, FadeOutTime);
			}
		}
	}
	else
	{
		// stop the sound cue on all targets specified
		for (INT VarIdx = 0; VarIdx < Targets.Num(); VarIdx++)
		{
			AActor *Target = Cast<AActor>(Targets(VarIdx));
			if (Target != NULL)
			{
				for (AController* C = GWorld->GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
				{
					APlayerController* PC = C->GetAPlayerController();
					if (PC != NULL)
					{
						PC->eventKismet_ClientStopSound(PlaySound, Target, FadeOutTime);
					}
				}
			}
		}
	}
	SoundDuration = 0.f;
	InputLinks(1).bHasImpulse = FALSE;
	bStopped = TRUE;
	bDelayReached = FALSE;
}

UBOOL USeqAct_PlaySound::UpdateOp(FLOAT DeltaTime)
{
	// catch another attempt to play the sound again
	if (InputLinks(0).bHasImpulse)
	{
		Activated();
	}
	else if (InputLinks(1).bHasImpulse)
	{
		Stop();
	}
	else
	{
		SoundDuration -= DeltaTime;

		// check to see if we have a delayed sound to play
		USoundNodeWave* Wave = FindFirstWaveNode(PlaySound->FirstNode);
		if (Wave && !bStopped && !appIsNearlyZero(ExtraDelay) && !bDelayReached)
		{
			if ((Wave->Duration * GWorld->GetWorldInfo()->TimeDilation) >= SoundDuration)
			{
				bDelayReached = TRUE;
				ActivateSound();
			}
		}
		
		if (BeforeEndTime >=0 && SoundDuration <= BeforeEndTime && SoundDuration + DeltaTime > BeforeEndTime && OutputLinks.Num() >= 4)
		{
			OutputLinks(3).ActivateOutputLink();
		}
	}
	// no more duration, sound finished
	return (SoundDuration <= 0.f);
}

void USeqAct_PlaySound::DeActivated()
{
	INT LinkIndex = bStopped ? 2 : 1;
	if( LinkIndex < OutputLinks.Num() )
	{
		// activate the appropriate output link
		OutputLinks(LinkIndex).ActivateOutputLink();
	}
}

void USeqAct_PlaySound::ActivateSound()
{
	TArray<UObject**> ObjVars;
	TArray<UObject*> Targets;
	GetObjectVars(ObjVars,TEXT("Target"));
	BuildSoundTargets(ObjVars,Targets);
	if (ObjVars.Num() == 0)
	{
		// play the sound cue on each player
		for (AController* C = GWorld->GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
		{
			APlayerController* PC = C->GetAPlayerController();
			if (PC != NULL)
			{
				KISMET_LOG(TEXT("- playing sound %s on target %s"), *PlaySound->GetName(), *PC->GetName());
				PC->eventKismet_ClientPlaySound(PlaySound, PC, VolumeMultiplier, PitchMultiplier, FadeInTime, bSuppressSubtitles, TRUE);
			}
		}
	}
	else
	{
		// play the sound cue on all targets specified
		for (INT VarIdx = 0; VarIdx < Targets.Num(); VarIdx++)
		{
			AActor* Target = Cast<AActor>(Targets(VarIdx));
			if (Target != NULL)
			{
				KISMET_LOG(TEXT("- playing sound %s on target %s"),*PlaySound->GetName(),*Target->GetName());

				for (AController* C = GWorld->GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
				{
					APlayerController* PC = C->GetAPlayerController();
					if (PC != NULL)
					{
						PC->eventKismet_ClientPlaySound(PlaySound, Target, VolumeMultiplier, PitchMultiplier, FadeInTime, bSuppressSubtitles, FALSE);
					}
				}
			}
		}
	}
}

void USeqAct_PlaySound::CleanUp()
{
	Super::CleanUp();

	if (bActive)
	{
		Stop();
	}
}

void USeqAct_ApplySoundNode::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if (PlaySound != NULL &&
		ApplyNode != NULL)
	{
		TArray<UObject**> ObjVars;
		GetObjectVars(ObjVars,TEXT("Target"));
		for (INT VarIdx = 0; VarIdx < ObjVars.Num(); VarIdx++)
		{
			AActor *target = Cast<AActor>(*(ObjVars(VarIdx)));
			if (target != NULL)
			{
				// find the audio component
				for (INT compIdx = 0; compIdx < target->Components.Num(); compIdx++)
				{
					UAudioComponent *comp = Cast<UAudioComponent>(target->Components(compIdx));
					if (comp != NULL &&
						comp->SoundCue == PlaySound)
					{
						// steal the first cue
						//@fixme - this will break amazingly with one node being applied multiple times
						ApplyNode->ChildNodes.AddItem(comp->CueFirstNode);
						comp->CueFirstNode = ApplyNode;
					}
				}
			}
		}
	}
}

/**
 * Applies new value to all variables attached to "Target" connector, using
 * either the variables attached to "Value" or DefaultValue.
 */
void USeqAct_SetBool::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	// read new value
	UBOOL bValue = 1;
	TArray<UBOOL*> boolVars;
	GetBoolVars(boolVars,TEXT("Value"));
	if (boolVars.Num() > 0)
	{
		for (INT Idx = 0; Idx < boolVars.Num(); Idx++)
		{
			bValue = bValue && *(boolVars(Idx));
		}
	}
	else
	{
		// no attached variables, use default value
		bValue = DefaultValue;
	}
	// and apply the new value
	boolVars.Empty();
	GetBoolVars(boolVars,TEXT("Target"));
	for (INT Idx = 0; Idx < boolVars.Num(); Idx++)
	{
		*(boolVars(Idx)) = bValue;
	}
}

/**
 * Applies new value to all variables attached to "Target" connector, using
 * either the variables attached to "Value" or DefaultValue.
 */
void USeqAct_SetObject::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if (Value == NULL)
	{
		Value = DefaultValue;
	}
	KISMET_LOG(TEXT("New set Object value: %s"),Value!=NULL?*Value->GetName():TEXT("NULL"));
	// and apply the new value
	for( INT Idx = 0; Idx < Targets.Num(); Idx++ )
	{
		Targets(Idx) = Value;
	}
}

void USeqAct_SetLocation::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	AActor* TargetActor = Cast<AActor>(Target);
	TArray<FVector*> VectorVars;
	// assign the new value
	if( TargetActor )
	{
		UBOOL bSet = bSetLocation;
		GetVectorVars(VectorVars, TEXT("Location"));
		if( VectorVars.Num() > 0 )
		{
			LocationValue = *VectorVars(0);
			bSet = TRUE;
		}
		if( bSet )
		{
			TargetActor->SetLocation(LocationValue);
		}
		

		bSet = bSetRotation;
		VectorVars.Empty();
		GetVectorVars(VectorVars, TEXT("Rotation"));
		if( VectorVars.Num() > 0 )
		{
			RotationValue = FRotator(appTrunc(VectorVars(0)->X), appTrunc(VectorVars(0)->Y), appTrunc(VectorVars(0)->Z));
			bSet = TRUE;
		}
		if( bSet )
		{
			TargetActor->SetRotation(RotationValue);
		}		
	}
}

//============================
// USeqAct_AttachToEvent interface

/* epic ===============================================
* ::Activated
*
* Attaches the actor to a given Event, well actually
* it adds the Event to the actor, but it's basically
* the same thing, sort of.
*
* =====================================================
*/
void USeqAct_AttachToEvent::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	// get a list of actors
	TArray<UObject**> ObjVars;
	GetObjectVars(ObjVars,TEXT("Attachee"));
	TArray<AActor*> targets;
	for (INT Idx = 0; Idx < ObjVars.Num(); Idx++)
	{
		AActor *actor = Cast<AActor>(*(ObjVars(Idx)));
		if (actor != NULL)
		{
			// if target is a controller, try to use the pawn
			if (!bPreferController && actor->GetAController() != NULL &&
				((AController*)actor)->Pawn != NULL)
			{
				targets.AddUniqueItem(((AController*)actor)->Pawn);
			}
			else if (bPreferController && actor->GetAPawn() != NULL && ((APawn*)actor)->Controller != NULL)
			{
				targets.AddUniqueItem(((APawn*)actor)->Controller);
			}
			else
			{
				targets.AddUniqueItem(actor);
			}
		}
	}
	// get a list of Events
	TArray<USequenceEvent*> Events;
	for (INT Idx = 0; Idx < EventLinks(0).LinkedEvents.Num(); Idx++)
	{
		USequenceEvent *Event = EventLinks(0).LinkedEvents(Idx);
		if (Event != NULL)
		{
			Events.AddUniqueItem(Event);
		}
	}
	// if we actually have actors and Events,
	if (targets.Num() > 0 &&
		Events.Num() > 0)
	{
		USequence *Seq = ParentSequence;
		// then add the Events to the targets
		for (INT Idx = 0; Idx < targets.Num(); Idx++)
		{
			for (INT EventIdx = 0; EventIdx < Events.Num(); EventIdx++)
			{
				// create a duplicate of the Event to avoid collision issues
				// create a unique name prefix so that we can guarantee no collisions with any other creation of Kismet objects (for serializing Kismet)
				FName DupName = MakeUniqueObjectName(Seq, Events(EventIdx)->GetClass(), FName(*FString::Printf(TEXT("%s_AttachDup"), *Events(EventIdx)->GetClass()->GetName())));
				USequenceEvent *evt = (USequenceEvent*)StaticConstructObject(Events(EventIdx)->GetClass(), Seq, DupName, 0, Events(EventIdx));
				if ( Seq->AddSequenceObject(evt) )
				{
					Events(EventIdx)->DuplicateEvts.AddItem(evt);
					evt->Originator = targets(Idx);
					targets(Idx)->GeneratedEvents.AddItem(evt);
					evt->Originator->eventReceivedNewEvent(evt);
				}
			}
		}
	}
	else
	{
		if (targets.Num() == 0)
		{
			KISMET_WARN(TEXT("Attach to Event %s has no targets!"),*GetName());
		}
		if (Events.Num() == 0)
		{
			KISMET_WARN(TEXT("Attach to Event %s has no Events!"),*GetName());
		}
	}
}

/**
 * Overloaded to see if any new inputs have been activated for this
 * op, so as to add them to the list.  This is needed since scripters tend
 * to use a single action for multiple AI, and the default will only work
 * for the first activation.
 */
UBOOL USeqAct_AIMoveToActor::UpdateOp(FLOAT DeltaTime)
{
	UBOOL bNewInput = 0;
	for (INT Idx = 0; Idx < InputLinks.Num(); Idx++)
	{
		if (InputLinks(Idx).bHasImpulse)
		{
			KISMET_LOG( TEXT("Latent move %s detected new input - Idx %i"), *GetName(), Idx );
			bNewInput = 1;
			break;
		}
	}
	if (bNewInput)
	{
		USeqAct_Latent::Activated();
		OutputLinks(2).bHasImpulse = TRUE;
	}
	return Super::UpdateOp(DeltaTime);
}

void USeqAct_AIMoveToActor::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints
}

/**
 * Force parent Sequence to update connector Links.
 */
void USeqEvent_SequenceActivated::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	USequence *Seq = Cast<USequence>(GetOuter());
	if (Seq != NULL)
	{
		Seq->UpdateConnectors();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Updates the Links on the outer Sequence.
 */
void USeqEvent_SequenceActivated::OnCreated()
{
	Super::OnCreated();
	USequence *Seq = Cast<USequence>(GetOuter());
	if (Seq != NULL)
	{
		Seq->UpdateConnectors();
	}
}

/**
 * Provides a simple activation, bypassing the normal event checks.
 */
UBOOL USeqEvent_SequenceActivated::CheckActivateSimple()
{
	if (bEnabled && (bClientSideOnly ? GWorld->GetWorldInfo()->NetMode != NM_DedicatedServer : GWorld->GetWorldInfo()->NetMode != NM_Client) &&
		(MaxTriggerCount == 0 || (TriggerCount < MaxTriggerCount)))
	{
		ActivateEvent(NULL,NULL);
		return TRUE;
	}
	return FALSE;
}

/**
 * Calculates the average positions of all A Objects, then gets the
 * distance from the average position of all B Objects, and places
 * the results in all the attached Distance floats.
 */
void USeqAct_GetDistance::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	TArray<UObject**> aObjs, bObjs;
	GetObjectVars(aObjs,TEXT("A"));
	GetObjectVars(bObjs,TEXT("B"));
	if (aObjs.Num() > 0 &&
		bObjs.Num() > 0)
	{
		// get the average position of A
		FVector avgALoc(0,0,0);
		INT actorCnt = 0;
		for (INT Idx = 0; Idx < aObjs.Num(); Idx++)
		{
			AActor *testActor = Cast<AActor>(*(aObjs(Idx)));
			if (testActor != NULL)
			{
				if (testActor->IsA(AController::StaticClass()) &&
					((AController*)testActor)->Pawn != NULL)
				{
					// use the pawn instead of the controller
					testActor = ((AController*)testActor)->Pawn;
				}
				avgALoc += testActor->Location;
				actorCnt++;
			}
		}
		if (actorCnt > 0)
		{
			avgALoc /= actorCnt;
		}
		// and average position of B
		FVector avgBLoc(0,0,0);
		actorCnt = 0;
		for (INT Idx = 0; Idx < bObjs.Num(); Idx++)
		{
			AActor *testActor = Cast<AActor>(*(bObjs(Idx)));
			if (testActor != NULL)
			{
				if (testActor->IsA(AController::StaticClass()) &&
					((AController*)testActor)->Pawn != NULL)
				{
					// use the pawn instead of the controller
					testActor = ((AController*)testActor)->Pawn;
				}
				avgBLoc += testActor->Location;
				actorCnt++;
			}
		}
		if (actorCnt > 0)
		{
			avgBLoc /= actorCnt;
		}
		// set the distance
		Distance = (avgALoc - avgBLoc).Size();
	}
}

/** 
 * Takes the magnitude of the velocity of all attached Target Actors and sets the output
 * Velocity to the total. 
 */
void USeqAct_GetVelocity::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	VelocityMag = 0.f;
	VelocityVect = FVector::ZeroVector;
	TArray<UObject**> Objs;
	GetObjectVars(Objs,TEXT("Target"));
	for(INT Idx = 0; Idx < Objs.Num(); Idx++)
	{
		AActor* TestActor = Cast<AActor>( *(Objs(Idx)) );
		if (TestActor != NULL)
		{
			// use the pawn instead of the controller
			AController* C = Cast<AController>(TestActor);
			if (C != NULL && C->Pawn != NULL)
			{
				TestActor = C->Pawn;
			}
			VelocityMag += TestActor->Velocity.Size();
			VelocityVect += TestActor->Velocity;
		}
	}
}

/** 
* Sets the outputs to the location and rotation vector of the Targeted Actor
*/
void USeqAct_GetLocationAndRotation::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	Location = FVector::ZeroVector;
	RotationVector = FVector::ZeroVector;

	TArray<UObject**> ObjVars;
	GetObjectVars(ObjVars,TEXT("Target"));

	// We are only concerned with the first actor targeted
	if(ObjVars.Num() > 0)
	{
		AActor* TestActor = Cast<AActor>( *(ObjVars(0)) );
		if( TestActor != NULL )
		{
			AController* TestController = Cast<AController>(TestActor);
			if( TestController != NULL && TestController->Pawn != NULL )
			{
				TestActor = TestController->Pawn;
			}

			// See if they want the socket or bone location from a skeletal mesh
			if (SocketOrBoneName != NAME_None)
			{
				// Try and get to the skeletal mesh
				APawn* TestPawn = Cast<APawn>(TestActor);
				if (TestPawn && TestPawn->Mesh)
				{
					// Get the socket location and rotation
					FVector OutLocation;
					FRotator OutRotation;
					if (TestPawn->Mesh->GetSocketWorldLocationAndRotation(SocketOrBoneName, OutLocation, &OutRotation))
					{
						// Set the location and rotation
						Location = OutLocation;
						RotationVector = OutRotation.Vector();
						Rotation = FVector(OutRotation.Pitch, OutRotation.Yaw, OutRotation.Roll);
						// Set the location and rotation so return out (don't want actor location and rotation overwriting)
						return;
					}
					// Couldn't find the socket location and rotation so let's try finding a bone instead
					else
					{
						INT BoneIndex = TestPawn->Mesh->MatchRefBone(SocketOrBoneName);
						if (BoneIndex != INDEX_NONE)
						{
							// Found a bone with this name so grab it's rotation and location
							FMatrix BoneMatrix = TestPawn->Mesh->GetBoneMatrix(BoneIndex);
							// Set the location and rotation
							Location = BoneMatrix.GetOrigin();
							FRotator BoneRotation = BoneMatrix.Rotator();
							RotationVector = BoneRotation.Vector();
							Rotation = FVector(BoneRotation.Pitch, BoneRotation.Yaw, BoneRotation.Roll);
							// Set the location and rotation so return out (don't want actor location and rotation overwriting)
							return;
						}
					}
				}
			}

			// Either no socket/bone name was provided or attempting to find them failed, so set values to actor location and rotation
			Location = TestActor->Location;
			RotationVector = TestActor->Rotation.Vector();
			Rotation = FVector(TestActor->Rotation.Pitch, TestActor->Rotation.Yaw, TestActor->Rotation.Roll);
		}
	}
}

/**
 * Looks up all object variables attached to the source op and returns a list
 * of objects.  Automatically handles edge cases like Controller<->Pawn, adding both
 * to the final list.  Returns the number of objects in ObjList.
 */
static INT GetObjectList(USequenceOp *SrcOp,const TCHAR* Desc,TArray<UObject*> &ObjList)
{
	if (SrcOp != NULL)
	{
		// grab all variables 
		TArray<UObject**> ObjVars;
		SrcOp->GetObjectVars(ObjVars,Desc);
		for (INT Idx = 0; Idx < ObjVars.Num(); Idx++)
		{
			UObject *Obj = *(ObjVars(Idx));
			if (Obj != NULL && !Obj->IsPendingKill())
			{
				// add this object to the list
				ObjList.AddUniqueItem(Obj);
				// and look for pawn/controller
				if (Obj->IsA(APawn::StaticClass()))
				{
					APawn *Pawn = (APawn*)(Obj);
					if (Pawn->Controller != NULL && !Pawn->Controller->IsPendingKill())
					{
						// add the controller as well
						ObjList.AddUniqueItem(Pawn->Controller);
					}
				}
				else
				if (Obj->IsA(AController::StaticClass()))
				{
					AController *Controller = (AController*)(Obj);
					if (Controller->Pawn != NULL && !Controller->Pawn->IsPendingKill())
					{
						// add the pawn as well
						ObjList.AddUniqueItem(Controller->Pawn);
					}
				}
			}
		}
	}
	return ObjList.Num();
}

void USeqAct_GetProperty::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if (PropertyName != NAME_None)
	{
		TArray<UObject*> ObjList;
		GetObjectList(this,TEXT("Target"),ObjList);
		// if there are actually valid objects attached
		if (ObjList.Num() > 0)
		{
			// grab the list of attached variables to write out to
			TArray<UObject**> ObjVars;
			GetObjectVars(ObjVars,TEXT("Object"));
			TArray<INT*> IntVars;
			GetOpVars<INT,USeqVar_Int>(IntVars,TEXT("Int"));
			//GetIntVars(IntVars,TEXT("Int"));
			TArray<FLOAT*> FloatVars;
			GetFloatVars(FloatVars,TEXT("Float"));
			TArray<FString*> StringVars;
			GetStringVars(StringVars,TEXT("String"));
			TArray<UBOOL*> BoolVars;
			GetBoolVars(BoolVars,TEXT("Bool"));
			// look for the property on each object
			UObject* ObjValue = NULL;
			INT IntValue = 0;
			FLOAT FloatValue = 0.f;
			FString StringValue = TEXT("");
			UBOOL BoolValue = TRUE;
			for (INT ObjIdx = 0; ObjIdx < ObjList.Num(); ObjIdx++)
			{
				UObject *Obj = ObjList(ObjIdx);
				UProperty *Property = FindField<UProperty>(Obj->GetClass(),PropertyName);
				if (Property != NULL)
				{
					// check to see if we can write an object
					if (ObjVars.Num() > 0 && Property->IsA(UObjectProperty::StaticClass()))
					{
						Property->CopySingleValue( (&ObjValue), ((BYTE*)Obj + Property->Offset) );
					}
					// int...
					if (IntVars.Num() > 0 && (Property->IsA(UIntProperty::StaticClass()) || Property->IsA(UFloatProperty::StaticClass())))
					{
						INT TempIntValue = 0;
						Property->CopySingleValue( (&TempIntValue), ((BYTE*)Obj + Property->Offset) );
						IntValue += TempIntValue;
					}
					// float...
					if (FloatVars.Num() > 0)
					{
						if (Property->IsA(UFloatProperty::StaticClass()) || Property->IsA(UIntProperty::StaticClass()))
						{
							FLOAT TempFloatValue;
							Property->CopySingleValue( (&TempFloatValue), ((BYTE*)Obj + Property->Offset) );
							FloatValue += TempFloatValue;
						}
						else
						// check for special cases
						if (Property->IsA(UStructProperty::StaticClass()))
						{
							// vector size
							UScriptStruct *Struct = ((UStructProperty*)Property)->Struct;
							if (Struct->GetFName() == NAME_Vector)
							{
								FVector Vector = *(FVector*)((BYTE*)Obj + Property->Offset);
								FloatValue += Vector.Size();
							}
						}
					}
					// string...
					if (StringVars.Num() > 0)
					{
						if (Property->IsA(UStrProperty::StaticClass()))
						{
							FString TempStringValue = TEXT("");
							Property->CopySingleValue( (&TempStringValue), ((BYTE*)Obj + Property->Offset) );
							StringValue += TempStringValue;
						}
						else
						{
							// convert to a string
							FString PropertyString;
							Property->ExportText(0, PropertyString, (BYTE*)Obj, (BYTE*)Obj, Obj, PPF_Localized);
							StringValue += PropertyString;
						}
					}
					// bool...
					if (BoolVars.Num() > 0 && (Property->IsA(UBoolProperty::StaticClass())))
					{
						UBOOL TempBoolValue = FALSE;
						Property->CopySingleValue( (&TempBoolValue), ((BYTE*)Obj + Property->Offset) );
						BoolValue = (BoolValue && TempBoolValue);
					}
				}
			}
			// now write out all the values to attached variables
			INT Idx;
			for (Idx = 0; Idx < ObjVars.Num(); Idx++)
			{
				*(ObjVars(Idx)) = ObjValue;
			}
			for (Idx = 0; Idx < IntVars.Num(); Idx++)
			{
				*(IntVars(Idx)) = IntValue;
			}
			for (Idx = 0; Idx < FloatVars.Num(); Idx++)
			{
				*(FloatVars(Idx)) = FloatValue;
			}
			for (Idx = 0; Idx < StringVars.Num(); Idx++)
			{
				*(StringVars(Idx)) = StringValue;
			}
			for (Idx = 0; Idx < BoolVars.Num(); Idx++)
			{
				*(BoolVars(Idx)) = BoolValue;
			}
		}
	}
}


/**
 * Given a level name, returns a level name that will work with Play on Editor or Play on Console
 *
 * @param	InLevelName		Raw level name (no UEDPIE or UED<console> prefix)
 */
static FName MakeSafeLevelName( const FName& InLevelName )
{
	// Special case for PIE, the PackageName gets mangled.
	FName SafeLevelName = InLevelName;

	if( GIsPlayInEditorWorld )
	{
		SafeLevelName = *FString::Printf(PLAYWORLD_PACKAGE_PREFIX TEXT("%s"), *InLevelName.ToString() );
	}
#if !FINAL_RELEASE		// Don't bother checking for Play on Console packages in final release builds
	else if( !GIsEditor && ensure( !GIsRoutingPostLoad ) )		// Can't call GetOutermost during loading as Package outer isn't set at that time
	{
		// Are we running a "play on console" map package?
		const FString WorldPackageFilename( FFilename( GWorld->GetOutermost()->GetName() ).GetBaseFilename() );

		// Check for Play on PC.  That prefix is a bit special as it's only 5 characters. (All others are 6)
		if( WorldPackageFilename.StartsWith( FString( PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX ) + TEXT( "PC") ) )
		{
			FString PlayOnConsolePrefix = WorldPackageFilename.Left( 5 );
			SafeLevelName = *FString( PlayOnConsolePrefix + InLevelName.ToString() );
		}
		else if( WorldPackageFilename.StartsWith( FString( PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX ) + TEXT( "WiiU") ) )
		{
			FString PlayOnConsolePrefix = WorldPackageFilename.Left( 7 );
			SafeLevelName = *FString( PlayOnConsolePrefix + InLevelName.ToString() );
		}
		else if( WorldPackageFilename.StartsWith( PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX ) )
		{
			// This is a Play on Console map package prefix. (6 characters)
			FString PlayOnConsolePrefix = WorldPackageFilename.Left( 6 );
			SafeLevelName = *FString( PlayOnConsolePrefix + InLevelName.ToString() );
		}
	}
#endif

	return SafeLevelName;
}


/**
 * Helper function to potentially find a level streaming object by name and cache the result
 *
 * @param	LevelStreamingObject	[in/out]	Level streaming object, can be NULL
 * @param	LevelName							Name of level to search streaming object for in case Level is NULL
 * @return	level streaming object or NULL if none was found
 */
static ULevelStreaming* FindAndCacheLevelStreamingObject( ULevelStreaming*& LevelStreamingObject, FName LevelName )
{
	// Search for the level object by name.
	if( !LevelStreamingObject && LevelName != NAME_None )
	{
		const FName SearchName = MakeSafeLevelName( LevelName );

#if !CONSOLE
		// look for Play on PC package name
		FName SearchNamePC = *FString::Printf(PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX CONSOLESUPPORT_NAME_PC TEXT("%s"),*LevelName.ToString());
#else
		FName SearchNamePC = NAME_None;
#endif

		// Iterate over all streaming level objects in world info to find one with matching name.
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();		
		for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* CurrentStreamingObject = WorldInfo->StreamingLevels(LevelIndex);
			if( CurrentStreamingObject 
			&&	(CurrentStreamingObject->PackageName == SearchName || CurrentStreamingObject->PackageName == SearchNamePC) )
			{
				LevelStreamingObject = CurrentStreamingObject;
				break;
			}
		}
	}
	return LevelStreamingObject;
}

/**
 * Handles "Activated" for single ULevelStreaming object.
 *
 * @param	LevelStreamingObject	LevelStreaming object to handle "Activated" for.
 */
void USeqAct_LevelStreamingBase::ActivateLevel( ULevelStreaming* LevelStreamingObject )
{
	if( LevelStreamingObject != NULL )
	{
		// Loading.
		if( InputLinks(0).bHasImpulse )
		{
			debugfSuppressed( NAME_DevStreaming, TEXT("Streaming in level %s (%s)..."),*LevelStreamingObject->GetName(),*LevelStreamingObject->PackageName.ToString());
			LevelStreamingObject->bShouldBeLoaded		= TRUE;
			LevelStreamingObject->bShouldBeVisible		|= bMakeVisibleAfterLoad;
			LevelStreamingObject->bShouldBlockOnLoad	= bShouldBlockOnLoad;
		}
		// Unloading.
		else if( InputLinks(1).bHasImpulse )
		{
			debugfSuppressed( NAME_DevStreaming, TEXT("Streaming out level %s (%s)..."),*LevelStreamingObject->GetName(),*LevelStreamingObject->PackageName.ToString());
			LevelStreamingObject->bShouldBeLoaded		= FALSE;
			LevelStreamingObject->bShouldBeVisible		= FALSE;
		}
		// notify players of the change
		for (AController *Controller = GWorld->GetWorldInfo()->ControllerList; Controller != NULL; Controller = Controller->NextController)
		{
			APlayerController *PC = Cast<APlayerController>(Controller);
			if (PC != NULL)
			{
				debugf(TEXT("ActivateLevel %s %i %i %i"), 
							*LevelStreamingObject->PackageName.ToString(), 
							LevelStreamingObject->bShouldBeLoaded, 
							LevelStreamingObject->bShouldBeVisible, 
							LevelStreamingObject->bShouldBlockOnLoad );



				PC->eventLevelStreamingStatusChanged( 
					LevelStreamingObject, 
					LevelStreamingObject->bShouldBeLoaded, 
					LevelStreamingObject->bShouldBeVisible,
					LevelStreamingObject->bShouldBlockOnLoad );
			}
		}
	}
	else
	{
		KISMET_WARN(TEXT("Failed to find streaming level object associated with '%s'"),*GetFullName());
	}
}

/**
 * Handles "UpdateOp" for single ULevelStreaming object.
 *
 * @param	LevelStreamingObject	LevelStreaming object to handle "UpdateOp" for.
 *
 * @return TRUE if operation has completed, FALSE if still in progress
 */
UBOOL USeqAct_LevelStreamingBase::UpdateLevel( ULevelStreaming* LevelStreamingObject )
{
	// No level streaming object associated with this sequence.
	if( LevelStreamingObject == NULL )
	{
		return TRUE;
	}
	// Level is neither loaded nor should it be so we finished (in the sense that we have a pending GC request) unloading.
	else if( (LevelStreamingObject->LoadedLevel == NULL || LevelStreamingObject->bHasUnloadRequestPending) && !LevelStreamingObject->bShouldBeLoaded )
	{
		return TRUE;
	}
	// Level shouldn't be loaded but is as background level streaming is enabled so we need to fire finished event regardless.
	else if( LevelStreamingObject->LoadedLevel && !LevelStreamingObject->bShouldBeLoaded && !GEngine->bUseBackgroundLevelStreaming )
	{
		return TRUE;
	}
	// Level is both loaded and wanted so we finished loading.
	else if(	LevelStreamingObject->LoadedLevel && LevelStreamingObject->bShouldBeLoaded 
	// Make sure we are visible if we are required to be so.
	&&	(!bMakeVisibleAfterLoad || LevelStreamingObject->bIsVisible) )
	{
		return TRUE;
	}

	// Loading/ unloading in progress.
	return FALSE;
}

void USeqAct_LevelStreaming::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints
	ULevelStreaming* LevelStreamingObject = FindAndCacheLevelStreamingObject( Level, LevelName );
	ActivateLevel( LevelStreamingObject );
}

void USeqAct_LevelStreaming::UpdateStatus()
{
	// if both are set and the name does not match
	if (Level != NULL && LevelName != NAME_None && Level->PackageName != LevelName)
	{
		// clear the object ref assuming the name is the valid one
		Level = NULL;
	}

	// NOTE: In UpdateStatus we don't want to be holding on to the streaming level reference since that would
	//   create a cross-level object reference in the editor, so we only cache to a temporary
	ULevelStreaming* FoundStreamingLevel = Level;
	bStatusIsOk = FindAndCacheLevelStreamingObject(FoundStreamingLevel,LevelName) != NULL;
}

void USeqAct_LevelStreaming::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged != NULL && (PropertyThatChanged->GetFName() == FName(TEXT("Level")) || PropertyThatChanged->GetFName() == FName(TEXT("LevelName"))))
	{
		UpdateStatus();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Called from parent sequence via ExecuteActiveOps, return
 * TRUE to indicate this action has completed.
 */
UBOOL USeqAct_LevelStreaming::UpdateOp(FLOAT DeltaTime)
{
	ULevelStreaming* LevelStreamingObject = Level; // to avoid confusion.
	UBOOL bIsOperationFinished = UpdateLevel( LevelStreamingObject );
	return bIsOperationFinished;
}

USequenceObject* USeqAct_LevelStreaming::ConvertObject()
{
	USeqAct_MultiLevelStreaming* NewSeqObj = Cast<USeqAct_MultiLevelStreaming>(StaticDuplicateObject(this, this, GetOuter(), TEXT("None"), 0, USeqAct_MultiLevelStreaming::StaticClass()));

	FLevelStreamingNameCombo LevelStreamingData;
	LevelStreamingData.Level = Level;
	LevelStreamingData.LevelName = LevelName;
	NewSeqObj->Levels.AddItem(LevelStreamingData);

	ConvertObjectInternal(NewSeqObj);

	return NewSeqObj;
};

void USeqAct_MultiLevelStreaming::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	for( INT LevelIndex=0; LevelIndex < Levels.Num(); LevelIndex++ )
	{
		FLevelStreamingNameCombo& Combo = Levels(LevelIndex);
		ULevelStreaming* LevelStreamingObject = FindAndCacheLevelStreamingObject( Combo.Level, Combo.LevelName );
		ActivateLevel( LevelStreamingObject );
	}
	if (bUnloadAllOtherLevels)
	{
		// iterate through currently streamed in levels and see if they are in the list to be loaded
		for (INT LevelIndex = 0; LevelIndex < GWorld->GetWorldInfo()->StreamingLevels.Num(); LevelIndex++)
		{
			ULevelStreaming *StreamingLevel = GWorld->GetWorldInfo()->StreamingLevels(LevelIndex);
			if (StreamingLevel != NULL)
			{
				UBOOL bShouldBeLoaded = FALSE;
				for (INT Idx = 0; Idx < Levels.Num(); Idx++)
				{
					if (StreamingLevel == Levels(Idx).Level || StreamingLevel->PackageName == Levels(Idx).LevelName)
					{
						bShouldBeLoaded = TRUE;
						break;
					}
				}
				// if not supposed to be loaded then mark it as such
				if (!bShouldBeLoaded)
				{
					StreamingLevel->bShouldBeLoaded = FALSE;
					StreamingLevel->bShouldBeVisible = FALSE;
					// notify players of change
					for (AController* Controller = GWorld->GetWorldInfo()->ControllerList; Controller != NULL; Controller = Controller->NextController)
					{
						APlayerController* PlayerController = Controller->GetAPlayerController();
						if (PlayerController != NULL)
						{
							debugf( TEXT("Activated %s %i %i %i"), *StreamingLevel->PackageName.ToString(), StreamingLevel->bShouldBeLoaded, 
																	StreamingLevel->bShouldBeVisible, StreamingLevel->bShouldBlockOnLoad );

							PlayerController->eventLevelStreamingStatusChanged( StreamingLevel, StreamingLevel->bShouldBeLoaded,
																				StreamingLevel->bShouldBeVisible, StreamingLevel->bShouldBlockOnLoad );
						}
					}
				}
			}
		}
	}
}


void USeqAct_MultiLevelStreaming::UpdateStatus()
{
	bStatusIsOk = TRUE;
	for( INT LevelIndex=0; LevelIndex < Levels.Num() && bStatusIsOk; LevelIndex++ )
	{
		FLevelStreamingNameCombo& Combo = Levels(LevelIndex);
		// if both are set and the name does not match
		if (Combo.Level != NULL && Combo.LevelName != NAME_None && Combo.Level->PackageName != Combo.LevelName)
		{
			// clear the object ref assuming the name is the valid one
			Combo.Level = NULL;
		}

		// NOTE: In UpdateStatus we don't want to be holding on to the streaming level reference since that would
		//   create a cross-level object reference in the editor, so we only cache to a temporary
		ULevelStreaming* FoundStreamingLevel = Combo.Level;
		bStatusIsOk = bStatusIsOk && (FindAndCacheLevelStreamingObject( FoundStreamingLevel, Combo.LevelName ) != NULL);
	}
}

void USeqAct_MultiLevelStreaming::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged != NULL && (PropertyThatChanged->GetFName() == FName(TEXT("Levels")) || PropertyThatChanged->GetFName() == FName(TEXT("LevelName"))))
	{
		UpdateStatus();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}


/**
 * Called from parent sequence via ExecuteActiveOps, return
 * TRUE to indicate this action has completed.
 */
UBOOL USeqAct_MultiLevelStreaming::UpdateOp(FLOAT DeltaTime)
{
	UBOOL bIsOperationFinished = TRUE;
	for( INT LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
	{
		FLevelStreamingNameCombo& Combo = Levels(LevelIndex);
		if( UpdateLevel( Combo.Level ) == FALSE )
		{
			bIsOperationFinished = FALSE;
			break;
		}
	}
	return bIsOperationFinished;
}

void USeqAct_LevelVisibility::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	ULevelStreaming* LevelStreamingObject = FindAndCacheLevelStreamingObject( Level, LevelName );

	if( LevelStreamingObject != NULL )
	{
		// Make visible.
		if( InputLinks(0).bHasImpulse )
		{
			debugfSuppressed( NAME_DevStreaming, TEXT("Making level %s (%s) visible."), *LevelStreamingObject->GetName(), *LevelStreamingObject->PackageName.ToString() );
			LevelStreamingObject->bShouldBeVisible	= TRUE;
			// We also need to make sure that the level gets loaded.
			LevelStreamingObject->bShouldBeLoaded	= TRUE;
		}
		// Hide.
		else if( InputLinks(1).bHasImpulse )
		{
			debugfSuppressed( NAME_DevStreaming, TEXT("Hiding level %s (%s)."), *LevelStreamingObject->GetName(), *LevelStreamingObject->PackageName.ToString() );
			LevelStreamingObject->bShouldBeVisible	= FALSE;
		}
		
		// Notify players of the change.
		for( AController* Controller=GWorld->GetWorldInfo()->ControllerList; Controller!=NULL; Controller=Controller->NextController )
		{
			APlayerController* PlayerController = Controller->GetAPlayerController();
			if (PlayerController != NULL)
			{

				debugf(TEXT("Activated %s %i %i %i"), 
							*LevelStreamingObject->PackageName.ToString(), 
							LevelStreamingObject->bShouldBeLoaded, 
							LevelStreamingObject->bShouldBeVisible, 
							LevelStreamingObject->bShouldBlockOnLoad );



				PlayerController->eventLevelStreamingStatusChanged( 
					LevelStreamingObject, 
					LevelStreamingObject->bShouldBeLoaded, 
					LevelStreamingObject->bShouldBeVisible,
					LevelStreamingObject->bShouldBlockOnLoad);
			}
		}
	}
	else
	{
		KISMET_WARN(TEXT("Failed to find streaming level object, LevelName: '%s'"),*LevelName.ToString());
	}
}

void USeqAct_LevelVisibility::UpdateStatus()
{
	// if both are set and the name does not match
	if (Level != NULL && LevelName != NAME_None && Level->PackageName != LevelName)
	{
		// clear the object ref assuming the name is the valid one
		Level = NULL;
	}

	// NOTE: In UpdateStatus we don't want to be holding on to the streaming level reference since that would
	//   create a cross-level object reference in the editor, so we only cache to a temporary
	ULevelStreaming* FoundStreamingLevel = Level;
	bStatusIsOk = FindAndCacheLevelStreamingObject(FoundStreamingLevel,LevelName) != NULL;
}

void USeqAct_LevelVisibility::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged != NULL && (PropertyThatChanged->GetFName() == FName(TEXT("Level")) || PropertyThatChanged->GetFName() == FName(TEXT("LevelName"))))
	{
		UpdateStatus();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Called from parent sequence via ExecuteActiveOps, return
 * TRUE to indicate this action has completed.
 */
UBOOL USeqAct_LevelVisibility::UpdateOp(FLOAT DeltaTime)
{
	// To avoid confusion.
	ULevelStreaming* LevelStreamingObject = Level;

	if( LevelStreamingObject == NULL )
	{
		// No level streaming object associated with this sequence.
		return TRUE;
	}
	else
	if( LevelStreamingObject->bIsVisible == LevelStreamingObject->bShouldBeVisible )
	{
		// Status matches so we're done.
		return TRUE;
	}

	// Toggling visibility in progress.
	return FALSE;
}

UBOOL USeqAct_WaitForLevelsVisible::CheckLevelsVisible()
{
	UBOOL bAllLevelsVisible		= TRUE;
	UBOOL bLevelsRequireLoad	= FALSE;

	// Iterate over all level names and see whether the corresponding levels are already loaded.
	for( INT LevelIndex=0; LevelIndex<LevelNames.Num(); LevelIndex++ )
	{
		const FName LevelName = MakeSafeLevelName( LevelNames( LevelIndex ) );

		if( LevelName != NAME_None )
		{
			UPackage* LevelPackage = Cast<UPackage>( UObject::StaticFindObjectFast( UPackage::StaticClass(), NULL, LevelName ) );
			// Level package exists.
			if( LevelPackage )
			{
				UWorld* LevelWorld = Cast<UWorld>( UObject::StaticFindObjectFast( UWorld::StaticClass(), LevelPackage, NAME_TheWorld ) );
				// World object has been loaded.
				if( LevelWorld )
				{
					check( LevelWorld->PersistentLevel );
					// Level is part of world...
					if( GWorld->Levels.FindItemIndex( LevelWorld->PersistentLevel ) != INDEX_NONE 
					// ... and doesn't have a visibility request pending and hence is fully visible.
					&& !LevelWorld->PersistentLevel->bHasVisibilityRequestPending )
					{
						// Level is fully visible.
					}
					// Level isn't visible yet. Either because it hasn't been added to the world yet or because it is
					// currently in the proces of being made visible.
					else
					{
						bAllLevelsVisible = FALSE;
						break;
					}
				}
				// World object hasn't been created/ serialized yet.
				else
				{
					bAllLevelsVisible	= FALSE;
					bLevelsRequireLoad	= TRUE;
					break;
				}
			}
			// Level package is not loaded yet.
			else
			{
				bAllLevelsVisible	= FALSE;
				bLevelsRequireLoad	= TRUE;
				break;
			}
		}
	}

	// Request blocking load if there are levels that require to be loaded and action is set to block.
	if( bLevelsRequireLoad && bShouldBlockOnLoad )
	{
		GWorld->GetWorldInfo()->bRequestedBlockOnAsyncLoading = TRUE;
	}

	return bAllLevelsVisible;
}

/**
 * Called from parent sequence via ExecuteActiveOps, return
 * TRUE to indicate this action has completed.
 */
UBOOL USeqAct_WaitForLevelsVisible::UpdateOp(FLOAT DeltaTime)
{
	return CheckLevelsVisible();
}

void USeqAct_PrepareMapChange::PostLoad()
{
	Super::PostLoad();
	if( GIsEditor && MainLevelName != NAME_None )
	{
		UpdateStatus();
	}
}

/**
 * Called when this sequence action is being activated. Kicks off async background loading.
 */
void USeqAct_PrepareMapChange::Activated()
{	
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	AWorldInfo* WorldInfo = GetWorldInfo();
	if (WorldInfo->NetMode == NM_Client)
	{
		KISMET_WARN(TEXT("PrepareMapChange action only works on servers"));
	}
	else if (WorldInfo->IsPreparingMapChange())
	{
		KISMET_WARN(TEXT("PrepareMapChange already pending"));
	}
	// No need to fire off sequence if no level is specified.
	else if (MainLevelName != NAME_None)
	{
		// Create list of levels to load with the first entry being the persistent one.
		TArray<FName> LevelNames;
		LevelNames.AddItem( MakeSafeLevelName( MainLevelName ) );
		for( INT CurLevelIndex = 0; CurLevelIndex < InitiallyLoadedSecondaryLevelNames.Num(); ++CurLevelIndex )
		{
			LevelNames.AddItem( MakeSafeLevelName( InitiallyLoadedSecondaryLevelNames( CurLevelIndex ) ) );
		}

		UBOOL bFoundLocalPlayer = FALSE;
		for (AController* C = GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
		{
			APlayerController* PC = C->GetAPlayerController();
			if (PC != NULL)
			{
				bFoundLocalPlayer = (bFoundLocalPlayer || PC->IsLocalPlayerController());
				// we need to send the levels individually because we dynamic arrays can't be replicated
				for (INT i = 0; i < LevelNames.Num(); i++)
				{
					PC->eventClientPrepareMapChange(LevelNames(i), i == 0, i == LevelNames.Num() - 1);
				}
			}
		}
		// if there's no local player to handle the event (e.g. dedicated server), call it from here
		if (!bFoundLocalPlayer)
		{
			WorldInfo->PrepareMapChange(LevelNames);
		}

		if (bIsHighPriority)
		{
			WorldInfo->bHighPriorityLoading = TRUE;
			WorldInfo->bNetDirty = TRUE;
			WorldInfo->bForceNetUpdate = TRUE;
		}
	}
	else
	{
		KISMET_WARN(TEXT("%s being activated without a level to load"), *GetFullName());
	}
}

/**
 * Called from parent sequence via ExecuteActiveOps, returns TRUE to indicate this 
 * action has completed, which in this case means the engine is ready to have
 * CommitMapChange called.
 * 
 * @return TRUE if action has completed, FALSE otherwise
 */
UBOOL USeqAct_PrepareMapChange::UpdateOp(FLOAT DeltaTime)
{
	UBOOL bIsOperationFinished = FALSE;
	// Only the game can do async map changes.
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if( GameEngine )
	{
		// Figure out whether the engine is ready for the map change, aka, has all the levels pre-loaded.
		bIsOperationFinished = GameEngine->IsReadyForMapChange();
	}

	return bIsOperationFinished;
}

void USeqAct_PrepareMapChange::DeActivated()
{
	Super::DeActivated();

	if (bIsHighPriority)
	{
		AWorldInfo* WorldInfo = GetWorldInfo();
		WorldInfo->bHighPriorityLoading = FALSE;
		WorldInfo->bNetDirty = TRUE;
		WorldInfo->bForceNetUpdate = TRUE;
	}
}

void USeqAct_PrepareMapChange::UpdateStatus()
{
	FString PackageFilename;
	
	// first the level to stream in
	bStatusIsOk = GPackageFileCache->FindPackageFile(*MakeSafeLevelName( MainLevelName ).ToString(), NULL, PackageFilename);

	// if that succeeds, then check the sublevels
	if (bStatusIsOk)
	{
		for (INT SubLevelIndex = 0; SubLevelIndex < InitiallyLoadedSecondaryLevelNames.Num(); SubLevelIndex++)
		{
			if (GPackageFileCache->FindPackageFile(*MakeSafeLevelName( InitiallyLoadedSecondaryLevelNames(SubLevelIndex) ).ToString(), NULL, PackageFilename) == FALSE)
			{
				bStatusIsOk = FALSE;
				break;
			}
		}
	}
}

void USeqAct_PrepareMapChange::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged != NULL && (PropertyThatChanged->GetFName() == FName(TEXT("MainLevelName")) || PropertyThatChanged->GetFName() == FName(TEXT("InitiallyLoadedSecondaryLevelNames"))))
	{
		UpdateStatus();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}


/**
 * Called when this sequence action is being activated. Kicks off async background loading.
 */
void USeqAct_CommitMapChange::Activated()
{	
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if (GetWorldInfo()->NetMode == NM_Client)
	{
		KISMET_WARN(TEXT("PrepareMapChange action only works on servers"));
	}
	else
	{
		UBOOL bFoundLocalPlayer = FALSE;
		for (AController* C = GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
		{
			APlayerController* PC = C->GetAPlayerController();
			if (PC != NULL)
			{
				bFoundLocalPlayer = (bFoundLocalPlayer || PC->IsLocalPlayerController());
				PC->eventClientCommitMapChange();
			}
		}
		// if there's no local player to handle the event (e.g. dedicated server), call it from here
		if (!bFoundLocalPlayer)
		{
			GetWorldInfo()->CommitMapChange();
		}
	}
}

UBOOL USeqEvent_SeeDeath::CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest, TArray<INT>* ActivateIndices, UBOOL bPushTop)
{
	UBOOL bResult = Super::CheckActivate(InOriginator, InInstigator, bTest, ActivateIndices, bPushTop);

	if( bResult && bEnabled && !bTest )
	{
		APawn* Victim = Cast<APawn>(InInstigator);
		if( Victim )
		{
			// see if any victim variables are attached
			TArray<UObject**> VictimVars;
			GetObjectVars(VictimVars,TEXT("Victim"));
			for (INT Idx = 0; Idx < VictimVars.Num(); Idx++)
			{
				*(VictimVars(Idx)) = Victim;
			}

			TArray<UObject**> KillerVars;
			GetObjectVars(KillerVars,TEXT("Killer"));
			for (INT Idx = 0; Idx < KillerVars.Num(); Idx++)
			{
				if (Victim->LastHitBy != NULL)
				{
					*(KillerVars(Idx)) = Victim->LastHitBy->Pawn;
				}
				else
				{
					*(KillerVars(Idx)) = NULL;
				}
			}
			
			TArray<UObject**> WitnessVars;
			GetObjectVars(WitnessVars,TEXT("Witness"));
			for (INT Idx = 0; Idx < WitnessVars.Num(); Idx++)
			{
				*(WitnessVars(Idx)) = InOriginator;
			}
		}
	}

	return bResult;
}

UBOOL USeqEvent_ProjectileLanded::CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest, TArray<INT>* ActivateIndices, UBOOL bPushTop)
{
	UBOOL bResult = Super::CheckActivate(InOriginator, InInstigator, bTest, ActivateIndices, bPushTop);

	if( bResult && bEnabled && !bTest )
	{
		AProjectile* Proj = Cast<AProjectile>(InInstigator);
		if(  Proj &&
			(MaxDistance <= 0.f || (Proj->Location - Originator->Location).SizeSquared() <= (MaxDistance * MaxDistance)) )
		{
			// see if any victim variables are attached
			TArray<UObject**> ProjVars;
			GetObjectVars(ProjVars,TEXT("Projectile"));
			for (INT Idx = 0; Idx < ProjVars.Num(); Idx++)
			{
				*(ProjVars(Idx)) = Proj;
			}

			TArray<UObject**> ShooterVars;
			GetObjectVars(ShooterVars,TEXT("Shooter"));
			for (INT Idx = 0; Idx < ShooterVars.Num(); Idx++)
			{
				*(ShooterVars(Idx)) = Proj->Instigator;
			}

			TArray<UObject**> WitnessVars;
			GetObjectVars(WitnessVars,TEXT("Witness"));
			for (INT Idx = 0; Idx < WitnessVars.Num(); Idx++)
			{
				*(WitnessVars(Idx)) = InOriginator;
			}
		}
		else
		{
			bResult = 0;
		}
	}

	return bResult;
}

UBOOL USeqEvent_GetInventory::CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest, TArray<INT>* ActivateIndices, UBOOL bPushTop)
{
	UBOOL bResult = Super::CheckActivate(InOriginator, InInstigator, bTest, ActivateIndices, bPushTop);

	if( bResult && bEnabled && !bTest )
	{
		AInventory* Inv = Cast<AInventory>(InInstigator);
		if( Inv )
		{
			// see if any victim variables are attached
			TArray<UObject**> InvVars;
			GetObjectVars(InvVars,TEXT("Inventory"));
			for (INT Idx = 0; Idx < InvVars.Num(); Idx++)
			{
				*(InvVars(Idx)) = Inv;
			}
		}
		else
		{
			bResult = 0;
		}
	}

	return bResult;
}

void USeqEvent_LevelLoaded::UpdateObject()
{
	if ( ObjInstanceVersion < eventGetObjClassVersion() )
	{
		OutputLinks.AddZeroed(1);
		OutputLinks(0).LinkDesc = FString("Loaded and Visible");
		OutputLinks(1).LinkDesc = FString("Beginning of Level");
	}

	Super::UpdateObject();
}

USequenceObject* UDEPRECATED_SeqEvent_LevelBeginning::ConvertObject()
{
	USeqEvent_LevelLoaded* NewSeqObj = Cast<USeqEvent_LevelLoaded>(StaticDuplicateObject(this, this, GetOuter(), TEXT("None"), 0, USeqEvent_LevelLoaded::StaticClass()));
	
	NewSeqObj->OutputLinks.AddZeroed(1);
	NewSeqObj->OutputLinks(0).LinkDesc = FString("Loaded and Visible");
	NewSeqObj->OutputLinks(1).LinkDesc = FString("Beginning of Level");

	NewSeqObj->OutputLinks(1) = OutputLinks(0);
	NewSeqObj->OutputLinks(0).Links.Empty();

	return NewSeqObj;
}

USequenceObject* UDEPRECATED_SeqEvent_LevelStartup::ConvertObject()
{
	USeqEvent_LevelLoaded* NewSeqObj = Cast<USeqEvent_LevelLoaded>(StaticDuplicateObject(this, this, GetOuter(), TEXT("None"), 0, USeqEvent_LevelLoaded::StaticClass()));

	NewSeqObj->OutputLinks.AddZeroed(1);
	NewSeqObj->OutputLinks(0).LinkDesc = FString("Loaded and Visible");
	NewSeqObj->OutputLinks(1).LinkDesc = FString("Beginning of Level");

	NewSeqObj->OutputLinks(0) = OutputLinks(0);

	return NewSeqObj;
}


void USeqEvent_Mover::OnCreated()
{
	Super::OnCreated();
#if WITH_EDITORONLY_DATA
	// verify that we have the correct number of links
	if (OutputLinks.Num() < 4)
	{
		debugf(NAME_Warning, TEXT("Unable to auto-create default Mover settings because expected output links are missing"));
	}
	else
	{
		// create a default matinee action
		// FIXME: this probably should be handled by a prefab/template, once we have such a system
		USequence* ParentSeq = CastChecked<USequence>(GetOuter());
		USeqAct_Interp* InterpAct = ConstructObject<USeqAct_Interp>(USeqAct_Interp::StaticClass(), GetOuter(), NAME_None, RF_Transactional);
		InterpAct->ParentSequence = ParentSeq;
		InterpAct->ObjPosX = ObjPosX + 250;
		InterpAct->ObjPosY = ObjPosY;
		ParentSeq->SequenceObjects.AddItem(InterpAct);
		InterpAct->OnCreated();
		InterpAct->Modify();
		if (InterpAct->InputLinks.Num() < 5)
		{
			debugf(NAME_Warning, TEXT("Unable to auto-link to default Interp action because expected input links are missing"));
		}
		else
		{
			// link our "Pawn Attached" connector to its "Play" connector
			INT OutputLinkIndex = OutputLinks(0).Links.Add();
			OutputLinks(0).Links(OutputLinkIndex).LinkedOp = InterpAct;
			OutputLinks(0).Links(OutputLinkIndex).InputLinkIdx = 0;
			// link our "Open Finished" connector to its "Reverse" connector
			OutputLinkIndex = OutputLinks(2).Links.Add();
			OutputLinks(2).Links(OutputLinkIndex).LinkedOp = InterpAct;
			OutputLinks(2).Links(OutputLinkIndex).InputLinkIdx = 1;
			// link our "Hit Actor" connector to its "Change Dir" connector
			OutputLinkIndex = OutputLinks(3).Links.Add();
			OutputLinks(3).Links(OutputLinkIndex).LinkedOp = InterpAct;
			OutputLinks(3).Links(OutputLinkIndex).InputLinkIdx = 4;
			
			// if we have an Originator, create default interpolation data for it and link it up
			if (Originator != NULL)
			{
				UInterpData* InterpData = InterpAct->FindInterpDataFromVariable();
				if (InterpData != NULL)
				{
					// create the new group
					UInterpGroup* NewGroup = ConstructObject<UInterpGroup>(UInterpGroup::StaticClass(), InterpData, NAME_None, RF_Transactional);
					NewGroup->GroupName = FName(TEXT("MoverGroup"));
					NewGroup->GroupColor = FColor::MakeRandomColor();
					NewGroup->Modify();
					InterpData->InterpGroups.AddItem(NewGroup);

					// Create the movement track

					UInterpTrackMove* NewMove = ConstructObject<UInterpTrackMove>(UInterpTrackMove::StaticClass(), NewGroup, NAME_None, RF_Transactional);
					NewMove->Modify();	
					NewGroup->InterpTracks.AddItem(NewMove);

					// tell the matinee action to update, which will create a new object variable connector for the group we created
					InterpAct->UpdateConnectorsFromData();
					// create a new object variable
					USeqVar_Object* NewObjVar = ConstructObject<USeqVar_Object>(USeqVar_Object::StaticClass(), ParentSeq, NAME_None, RF_Transactional);
					NewObjVar->ObjPosX = InterpAct->ObjPosX + 50 * InterpAct->VariableLinks.Num();
					NewObjVar->ObjPosY = InterpAct->ObjPosY + 200;
					NewObjVar->ObjValue = Originator;
					NewObjVar->Modify();
					ParentSeq->AddSequenceObject(NewObjVar);

					// hook up the new variable connector to the new variable
					INT NewLinkIndex = InterpAct->FindConnectorIndex(FString(TEXT("MoverGroup")), LOC_VARIABLE);
					checkSlow(NewLinkIndex != INDEX_NONE);
					InterpAct->VariableLinks(NewLinkIndex).LinkedVariables.AddItem(NewObjVar);
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

//////////////////////////////////////////////////////////////////
// USeqEvent_ParticleEvent
//////////////////////////////////////////////////////////////////
void USeqEvent_ParticleEvent::OnCreated()
{
	Super::OnCreated();
	// Setup the output links - no need to preserve them
	SetupOutputLinks(FALSE);
}

UBOOL USeqEvent_ParticleEvent::CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest, TArray<INT>* ActivateIndices, UBOOL bPushTop)
{
	return Super::CheckActivate(InOriginator, InInstigator, bTest, ActivateIndices, bPushTop);
}

/** Called via PostEditChange(), lets ops create/remove dynamic links based on data. */
void USeqEvent_ParticleEvent::UpdateDynamicLinks()
{
	Super::UpdateDynamicLinks();
	SetupOutputLinks();
}

/** Helper function for filling in the output links according to the assigned emitter. */
void USeqEvent_ParticleEvent::SetupOutputLinks(UBOOL bPreserveExistingLinks)
{
	AEmitter* EmitterOriginator = Cast<AEmitter>(Originator);
	if (EmitterOriginator == NULL)
	{
		KISMET_WARN(*FString::Printf(*LocalizeUnrealEd("Kismet_InvalidEmitter")));
		return;
	}

	UParticleSystem* PSys = EmitterOriginator->ParticleSystemComponent ? EmitterOriginator->ParticleSystemComponent->Template : NULL;
	if (PSys == NULL)
	{
		KISMET_WARN(*FString::Printf(LocalizeSecure(LocalizeUnrealEd("Kismet_InvalidPSysTemplate"),*EmitterOriginator->GetPathName())));
		return;
	}

	if (PSys->Emitters.Num() == 0)
	{
		KISMET_WARN(*FString::Printf(LocalizeSecure(LocalizeUnrealEd("Kismet_NoEmittersInPSysTemplate"),*EmitterOriginator->GetPathName())));
		return;
	}

	// Preserve existing if requested...
	TArray<FSeqOpOutputLink> PreservedOutputLinks;
	if (bPreserveExistingLinks == TRUE)
	{
		for (INT PreserveIndex = 0; PreserveIndex < OutputLinks.Num(); PreserveIndex++)
		{
			PreservedOutputLinks.AddItem(OutputLinks(PreserveIndex));
		}
	}

	// Clear the existing links...
	OutputLinks.Empty();

	// Collect the new links...
	TArray<FName> LinksToAdd;

	for (INT EmitterIndex = 0; EmitterIndex < PSys->Emitters.Num(); EmitterIndex++)
	{
		UParticleEmitter* Emitter = PSys->Emitters(EmitterIndex);
		if (Emitter != NULL)
		{
			for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
			{
				UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIndex);
				if (LODLevel)
				{
					if (LODLevel->EventGenerator)
					{
						for (INT EventIndex = 0; EventIndex < LODLevel->EventGenerator->Events.Num(); EventIndex++)
						{
							FParticleEvent_GenerateInfo& EventInfo = LODLevel->EventGenerator->Events(EventIndex);
							LinksToAdd.AddUniqueItem(EventInfo.CustomName);
						}
					}
				}
			}
		}
	}

	// Add the links
	for (INT LinkIndex = 0; LinkIndex < LinksToAdd.Num(); LinkIndex++)
	{
		FSeqOpOutputLink* NewOutputLink = new(OutputLinks)FSeqOpOutputLink(EC_EventParm);
		check(NewOutputLink);

		NewOutputLink->LinkDesc = LinksToAdd(LinkIndex).ToString();

		// See if the link previously existed...
		if (bPreserveExistingLinks == TRUE)
		{
			for (INT RestoreIndex = 0; RestoreIndex < PreservedOutputLinks.Num(); RestoreIndex++)
			{
				if (NewOutputLink->LinkDesc == PreservedOutputLinks(RestoreIndex).LinkDesc)
				{
					// Found it - so hook it back up.
					NewOutputLink->bDisabled = PreservedOutputLinks(RestoreIndex).bDisabled;
					NewOutputLink->bDisabledPIE = PreservedOutputLinks(RestoreIndex).bDisabledPIE;
					NewOutputLink->ActivateDelay = PreservedOutputLinks(RestoreIndex).ActivateDelay;

					for (INT PrevLinkIndex = 0; PrevLinkIndex < PreservedOutputLinks(RestoreIndex).Links.Num(); PrevLinkIndex++)
					{
						NewOutputLink->Links.AddItem(PreservedOutputLinks(RestoreIndex).Links(PrevLinkIndex));
					}
				}
			}
		}
	}
}

void USeqAct_Trace::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	AActor *Start = NULL, *End = NULL;
	TArray<UObject**> ObjVars;
	// grab the start location
	GetObjectVars(ObjVars,TEXT("Start"));
	for (INT VarIdx = 0; VarIdx < ObjVars.Num() && Start == NULL; VarIdx++)
	{
		AActor *Actor = Cast<AActor>(*ObjVars(VarIdx));
		if (Actor != NULL && !Actor->IsPendingKill())
		{
			if (Actor->IsA(AController::StaticClass()) && ((AController*)Actor)->Pawn != NULL)
			{
				Start = ((AController*)Actor)->Pawn;
			}
			else
			{
				Start = Actor;
			}
		}
	}
	// grab the end location
	ObjVars.Empty();
	GetObjectVars(ObjVars,TEXT("End"));
	for (INT VarIdx = 0; VarIdx < ObjVars.Num() && End == NULL; VarIdx++)
	{
		AActor *Actor = Cast<AActor>(*ObjVars(VarIdx));
		if (Actor != NULL && !Actor->IsPendingKill())
		{
			if (Actor->IsA(AController::StaticClass()) && ((AController*)Actor)->Pawn != NULL)
			{
				End = ((AController*)Actor)->Pawn;
			}
			else
			{
				End = Actor;
			}
		}
	}
	// perform the trace
	UBOOL bHit = FALSE;
	if (Start != NULL && End != NULL && (bTraceActors || bTraceWorld))
	{
		debugfSuppressed( NAME_Dev ,TEXT("Tracing from %s to %s"),*Start->GetName(),*End->GetName());
		DWORD TraceFlags = 0;
		if (bTraceActors)
		{
			TraceFlags |= TRACE_ProjTargets;
		}
		if (bTraceWorld)
		{
			TraceFlags |= TRACE_World;
		}
		FVector StartLocation = Start->Location + FRotationMatrix(Start->Rotation).TransformFVector(StartOffset);
		FVector EndLocation = End->Location + FRotationMatrix(End->Rotation).TransformFVector(EndOffset);
		FCheckResult CheckResult;
		GWorld->SingleLineCheck(CheckResult,Start,EndLocation,StartLocation,TraceFlags,TraceExtent);
		if (CheckResult.Actor != NULL)
		{
			bHit = TRUE;
			// write out first hit actor and distance
			HitObject	= CheckResult.Actor;
			Distance	= (CheckResult.Location - StartLocation).Size();
			HitLocation = CheckResult.Location;
		}
		else
		{
			bHit = FALSE;
			// write out the distance from start to end
			HitObject = NULL;
			Distance = (StartLocation - EndLocation).Size();
			HitLocation = EndLocation;
		}
	}
	// activate the appropriate output
	if (!bHit)
	{
		OutputLinks(0).bHasImpulse = TRUE;
	}
	else
	{
		OutputLinks(1).bHasImpulse = TRUE;
	}
}

void USeqAct_ActivateRemoteEvent::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	// grab the originator/instigator
	AActor *Originator = GetWorldInfo();
	if (Instigator == NULL)
	{
		Instigator = Originator;
	}

	UBOOL bFoundRemoteEvent = FALSE;

	// look for all remote events in the entire sequence tree
	USequence *RootSeq = GetRootSequence();
	TArray<USeqEvent_RemoteEvent*> RemoteEvents;

	RootSeq->FindSeqObjectsByClass(USeqEvent_RemoteEvent::StaticClass(),(TArray<USequenceObject*>&)RemoteEvents);
	for (INT Idx = 0; Idx < RemoteEvents.Num(); Idx++)
	{
		USeqEvent_RemoteEvent *RemoteEvt = RemoteEvents(Idx);
		if (RemoteEvt != NULL && RemoteEvt->EventName == EventName )
		{
			bFoundRemoteEvent = TRUE;
			if ( RemoteEvt->bEnabled)
			{
				// check activation for the event
				RemoteEvt->PublishLinkedVariableValues();
				RemoteEvt->CheckActivate(Originator,Instigator,FALSE,NULL);
			}
		}
	}

	if ( !bFoundRemoteEvent )
	{
		debugf(NAME_Warning, TEXT("%s failed to find target event: %s"), *GetFullName(), *EventName.ToString());
		KISMET_WARN( TEXT("%s failed to find target event: %s"), *GetFullName(), *EventName.ToString() );
	}
}

void USeqAct_ActivateRemoteEvent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged != NULL && PropertyThatChanged->GetFName() == FName(TEXT("EventName")))
	{
		UpdateStatus();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void USeqAct_ActivateRemoteEvent::UpdateStatus()
{
	UBOOL bFoundRemoteEvent = FALSE;
	for (TObjectIterator<ULevel> LevelIt; LevelIt && !bFoundRemoteEvent; ++LevelIt)
	{
		if (LevelIt->GameSequences.Num() > 0)
		{
			USequence *RootSeq = LevelIt->GameSequences(0);
			TArray<USeqEvent_RemoteEvent*> RemoteEvents;
			RootSeq->FindSeqObjectsByClass(USeqEvent_RemoteEvent::StaticClass(),(TArray<USequenceObject*>&)RemoteEvents);
			for (INT Idx = 0; Idx < RemoteEvents.Num(); Idx++)
			{
				USeqEvent_RemoteEvent *RemoteEvt = RemoteEvents(Idx);
				if (RemoteEvt != NULL && RemoteEvt->EventName == EventName )
				{
					bFoundRemoteEvent = TRUE;
					// only need to find the first event for status updating
					break;
				}
			}
		}
	}
	bStatusIsOk = bFoundRemoteEvent;
}

void USeqEvent_RemoteEvent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged != NULL && PropertyThatChanged->GetFName() == FName(TEXT("EventName")))
	{
		UpdateStatus();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void USeqEvent_RemoteEvent::UpdateStatus()
{
	UBOOL bFoundRemoteAction = FALSE;
	for (TObjectIterator<ULevel> LevelIt; LevelIt && !bFoundRemoteAction; ++LevelIt)
	{
		if (LevelIt->GameSequences.Num() > 0)
		{
			USequence *RootSeq = LevelIt->GameSequences(0);
			TArray<USeqAct_ActivateRemoteEvent*> RemoteActions;
			RootSeq->FindSeqObjectsByClass(USeqAct_ActivateRemoteEvent::StaticClass(),(TArray<USequenceObject*>&)RemoteActions);
			for (INT Idx = 0; Idx < RemoteActions.Num(); Idx++)
			{
				USeqAct_ActivateRemoteEvent *RemoteAction = RemoteActions(Idx);
				if (RemoteAction != NULL && RemoteAction->EventName == EventName )
				{
					bFoundRemoteAction = TRUE;
					// only need to find the first event for status updating
					break;
				}
			}
		}
	}
	bStatusIsOk = bFoundRemoteAction;
}

// Material Instance actions

void USeqAct_SetMaterial::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// If the SetMaterial targets include a skeletal mesh, mark the material as being used with a skeletal mesh.
	for(INT TargetIndex = 0;TargetIndex < Targets.Num();TargetIndex++)
	{
		ASkeletalMeshActor* SkeletalMeshActor = Cast<ASkeletalMeshActor>(Targets(TargetIndex));
		if(SkeletalMeshActor)
		{
			NewMaterial->CheckMaterialUsage(MATUSAGE_SkeletalMesh);

			// if the skeletal mesh is also using morph targets then set morph target material usage
			USkeletalMeshComponent* SkelMeshComp = SkeletalMeshActor->SkeletalMeshComponent;
			if( SkelMeshComp && SkelMeshComp->ActiveMorphs.Num() > 0 )
			{
				NewMaterial->CheckMaterialUsage(MATUSAGE_MorphTargets);
			}

			break;
		}
	}
}

void USeqAct_SetMatInstScalarParam::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if (MatInst != NULL)
	{
		MatInst->SetScalarParameterValue(ParamName,ScalarValue);
	}
}

/* ==========================================================================================================
	USeqAct_CameraFade
========================================================================================================== */

void USeqAct_CameraFade::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	FVector2D FadeAlpha(0.f, FadeOpacity);

	//@todo - Remove this once we've updated all existing USeqAct_CameraFade objects
	if( ObjInstanceVersion < eventGetObjClassVersion() )
	{
		FadeOpacity = FadeAlpha_DEPRECATED.Y;
		FadeAlpha = FadeAlpha_DEPRECATED;
	}

	FadeTimeRemaining = FadeTime;

	// tell all PCs
	CachedPCs.Empty();
	for (INT Idx = 0; Idx < Targets.Num(); Idx++)
	{
		APlayerController* PC = Cast<APlayerController>(Targets(Idx));
		FadeAlpha.X = 0.0f;

		if (PC == NULL)
		{
			APawn* Pawn = Cast<APawn>(Targets(Idx));
			if (Pawn != NULL)
			{
				PC = Cast<APlayerController>(Pawn->Controller);
			}
		}

		if (PC != NULL)
		{
			if (PC->PlayerCamera != NULL)
			{
				FadeAlpha.X = PC->PlayerCamera->FadeAmount;
			}	
			PC->eventClientSetCameraFade(TRUE, FadeColor, FadeAlpha, FadeTime, bFadeAudio);
			CachedPCs.AddItem(PC);
		}
	}

	// If no targets are specified, default to all players
	if (Targets.Num() == 0)
	{
		for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
		{
			APlayerController* PC = Cast<APlayerController>(Controller);
			FadeAlpha.X = 0.0f;
			if (PC != NULL)
			{
				if (PC->PlayerCamera != NULL)
				{
					FadeAlpha.X = PC->PlayerCamera->FadeAmount;
				}	
				PC->eventClientSetCameraFade(TRUE, FadeColor, FadeAlpha, FadeTime, bFadeAudio);
				CachedPCs.AddItem(PC);
			}
		}
	}

	// activate the "out" output
	OutputLinks(0).ActivateOutputLink();
}

UBOOL USeqAct_CameraFade::UpdateOp(FLOAT DeltaTime)
{
	// decrement remaining time
	// note that we rely on the PlayerController to also decrement time and update the fade (since it needs to be done client side)
	FadeTimeRemaining -= DeltaTime;

	if (FadeTimeRemaining <= 0.f)
	{
		// force bPersistFade to be considered off if by "persist the fade" they meant "persist the lack of fade" :)
		if (!bPersistFade || FadeOpacity == 0.0f)
		{
			// clear the fades on cameras
			for (INT Idx = 0; Idx < CachedPCs.Num(); Idx++)
			{
				APlayerController* PC = CachedPCs(Idx);
				if (PC != NULL)
				{
					PC->eventClientSetCameraFade(FALSE);
				}
			}
		}
		CachedPCs.Empty();
		// activate the "finished" output
		OutputLinks(1).ActivateOutputLink();
	}
	return (FadeTimeRemaining <= 0.f);
}

void USeqAct_CameraFade::UpdateObject()
{
	// Copy data from old properties, so old data will work as before
	if( ObjInstanceVersion < eventGetObjClassVersion() )
	{
		FadeOpacity = FadeAlpha_DEPRECATED.Y;
	}

	Super::UpdateObject();
}

/* ==========================================================================================================
	USeqAct_CameraLookAt
========================================================================================================== */

void USeqAct_CameraLookAt::Activated()
{
	RemainingTime = TotalTime;
	if ( TotalTime > 0.0f )
	{
		bUsedTimer = TRUE;
	}

	Super::Activated();

	OutputLinks(0).ActivateOutputLink();
}

UBOOL USeqAct_CameraLookAt::UpdateOp(FLOAT DeltaTime)
{
	if (bUsedTimer)
	{
		RemainingTime -= DeltaTime;
		return RemainingTime <= 0.f;
	}
	else
	{
		return TRUE;
	}
}

void USeqAct_CameraLookAt::DeActivated()
{
	if (bUsedTimer)
	{
		UINT NumPCsFound = 0;

		// note that it is valid for multiple PCs to be affected here
		TArray<UObject**>	ObjVars;
		GetObjectVars(ObjVars,TEXT("Target"));

		for (INT Idx = 0; Idx < ObjVars.Num(); Idx++)
		{
			UObject *Obj = (*(ObjVars(Idx)));

			if (Obj != NULL)
			{
				// if this is a pawn, set the object to be the controller of the pawn
				if (Obj->IsA(APawn::StaticClass()))
				{
					Obj = ((APawn*)Obj)->Controller;
				}

				if (Obj != NULL && Obj->IsA(APlayerController::StaticClass()))
				{
					NumPCsFound++;
					((APlayerController*)Obj)->eventCameraLookAtFinished(this);
				}
			}
		}

		if (NumPCsFound == 0) 
		{
			KISMET_WARN( TEXT("%s could not find a valid APlayerController!"), *GetPathName() );
		}

		// activate the finished output link
		OutputLinks(1).ActivateOutputLink();
	}
}

void USeqAct_CameraLookAt::UpdateObject()
{
	if (ObjInstanceVersion == 3)
	{
		// when updating from version 3, propagate bAdjustCamera information to bTurnInPlace.
		bTurnInPlace = !bAdjustCamera_DEPRECATED;
	}

	Super::UpdateObject();
}


void USeqAct_ForceGarbageCollection::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	// Request garbage collection and replicate request to client.
	for( AController* C=GetWorldInfo()->ControllerList; C!=NULL; C=C->NextController)
	{
		APlayerController* PC = C->GetAPlayerController();
		if( PC )
		{
			PC->eventClientForceGarbageCollection();
		}
	}
}

UBOOL USeqAct_ForceGarbageCollection::UpdateOp(FLOAT DeltaTime)
{
	// GWorld::ForceGarbageCollection sets TimeSinceLastPendingKillPurge > TimeBetweenPurgingPendingKillObjects and it is
	// being reset to 0 when the garbage collection occurs so we know that GC is still pending if the below is true.
	if( GWorld->TimeSinceLastPendingKillPurge > GEngine->TimeBetweenPurgingPendingKillObjects )
	{
		// GC still pending. We're not done yet.
		return FALSE;
	}
	else if( UObject::IsIncrementalPurgePending() )
	{
		// GC finished, incremental purge still in progress so we're not done yet.
		return FALSE;
	}
	else
	{
		// GC occured and incremental purge finished. We're done.
		return TRUE;
	}
}

/** called when the level that contains this sequence object is being removed/unloaded */
void USequence::CleanUp()
{
	Super::CleanUp();

	// pass to inner objects
	for (INT i = 0; i < SequenceObjects.Num(); i++)
	{
		USequenceObject* Obj = SequenceObjects(i);
		if (Obj != NULL)
		{
			Obj->CleanUp();
		}
	}
}

/** When activated, perform the appropriate health modifications */
void USeqAct_ModifyHealth::Activated()
{
	if( bRadial )
	{
		// Instigator might be controller or pawn, so handle that case.
		APawn* InstigatorPawn = NULL;
		AController* InstigatorController = Cast<AController>(Instigator);
		if( !InstigatorController )
		{
			APawn* InstigatorPawn = Cast<APawn>(Instigator);
			if( InstigatorPawn )
			{
				InstigatorController = InstigatorPawn->Controller;
			}
		}
		else
		{
			InstigatorPawn = InstigatorController->Pawn;
		}

		TArray<UObject**> TargetObjects;
		GetObjectVars(TargetObjects, TEXT("Target"));

		for( INT TargetIdx = 0; TargetIdx < TargetObjects.Num(); ++TargetIdx )
		{
			AActor* TargetActor = Cast<AActor>(*(TargetObjects(TargetIdx)));

			// If Target is a Controller, use the Controller's pawn
			AController* TargetController = Cast<AController>(TargetActor);
			if( TargetController )
			{
				TargetActor = Cast<APawn>(TargetController->Pawn);
			}

			if( TargetActor && !TargetActor->bDeleteMe )
			{
				FCheckResult* OverlapList = GWorld->Hash->ActorOverlapCheck(GMainThreadMemStack, TargetActor, TargetActor->Location, Radius);
				for( FCheckResult* CurOverlap = OverlapList; CurOverlap; CurOverlap = CurOverlap->GetNext() )
				{
					AActor* DamageActor = CurOverlap->Actor;
					if( DamageActor )
					{
						FVector ToDamageActor = DamageActor->Location - TargetActor->Location;
						const FLOAT Distance = ToDamageActor.Size();
						if( Distance <= Radius )
						{
							if(Distance > KINDA_SMALL_NUMBER)
							{
								// Normalize vector from location to damaged actor.
								ToDamageActor = ToDamageActor / Distance;
							}

							FLOAT DoDamage = Amount;

							// If damage should falloff linearly
							if( bFalloff && (Radius > KINDA_SMALL_NUMBER) )
							{
								DoDamage = DoDamage * (1.f - (Distance / Radius));
							}

							// Either heal or damage with the calculated DoDamage
							if( bHeal )
							{
								DamageActor->eventHealDamage(appTrunc(DoDamage), InstigatorController, DamageType);
							}
							else
							{
								DamageActor->eventTakeDamage(appTrunc(DoDamage), InstigatorController, TargetActor->Location, ToDamageActor * Momentum, DamageType);
							}
						}
					}
				}
			}
		}
	}
	Super::Activated();
}

void USeqAct_ModifyHealth::UpdateObject()
{
	// Copy data from old properties, so old data will work as before
	if( ObjInstanceVersion < eventGetObjClassVersion() )
	{
		VariableLinks(1).ExpectedType = USeqVar_Float::StaticClass();
		VariableLinks(1).LinkDesc = FString("Amount");
		VariableLinks(1).PropertyName = FName(TEXT("Amount"));
	}

	Super::UpdateObject();
}

void USeqAct_PlayCameraAnim::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if (CameraAnim != NULL)
	{
		if (InputLinks(0).bHasImpulse)
		{
			// play the animation on each target camera
			for (INT i = 0; i < Targets.Num(); i++)
			{
				APlayerController* PC = Cast<APlayerController>(Targets(i));
				if (PC == NULL)
				{
					APawn* P = Cast<APawn>(Targets(i));
					if (P != NULL)
					{
						PC = Cast<APlayerController>(P->Controller);
					}
				}
				if (PC != NULL)
				{
					if (PC->IsLocalPlayerController())
					{
						if (PC->PlayerCamera != NULL)
						{
							UCameraAnimInst* AnimInst = PC->PlayerCamera->PlayCameraAnim(CameraAnim, Rate, IntensityScale, BlendInTime, BlendOutTime, bLoop, bRandomStartTime);
							if (AnimInst && PlaySpace != CAPS_CameraLocal)
							{
								AnimInst->SetPlaySpace(PlaySpace, (PlaySpace == CAPS_UserDefined && UserDefinedSpaceActor) ? UserDefinedSpaceActor->Rotation : FRotator::ZeroRotator);
							}
						}
					}
					else
					{
						PC->eventClientPlayCameraAnim(CameraAnim, IntensityScale, Rate, BlendInTime, BlendOutTime, bLoop, bRandomStartTime, PlaySpace, (PlaySpace == CAPS_UserDefined && UserDefinedSpaceActor) ? UserDefinedSpaceActor->Rotation : FRotator::ZeroRotator);
					}
				}
			}
		}
		else if (InputLinks(1).bHasImpulse)
		{
			// stop playing the current anim on each target camera
			for (INT i = 0; i < Targets.Num(); i++)
			{
				APlayerController* PC = Cast<APlayerController>(Targets(i));
				if (PC == NULL)
				{
					APawn* P = Cast<APawn>(Targets(i));
					if (P != NULL)
					{
						PC = Cast<APlayerController>(P->Controller);
					}
				}
				if (PC != NULL)
				{
					if (PC->PlayerCamera != NULL)
					{
						PC->PlayerCamera->StopAllCameraAnimsByType(CameraAnim);
					}
					PC->eventClientStopCameraAnim(CameraAnim);
				}
			}
		}
	}
}

void USeqAct_RandomSwitch::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	// Check for a reset impulse, and re-enable all links if so before handling other input
	if (InputLinks.Num() > 1 && InputLinks(1).bHasImpulse)
	{
		INT AutoDisabledIdx = 0;
		for(INT Idx=0; Idx<AutoDisabledIndices.Num(); Idx++)
		{
			AutoDisabledIdx = AutoDisabledIndices(Idx);
			// turn it back on for the next iteration
			OutputLinks(AutoDisabledIdx).bDisabled = FALSE;
		}
		AutoDisabledIndices.Empty();
	}

	// Handle normal gate input
	if( InputLinks(0).bHasImpulse )
	{
		// build a list of enabled links
		TArray<INT> ValidLinks;
		for (INT Idx = 0; Idx < OutputLinks.Num(); Idx++)
		{
			if (!OutputLinks(Idx).bDisabled && !(OutputLinks(Idx).bDisabledPIE && GIsEditor))
			{
				ValidLinks.AddItem(Idx);
			}
		}

		// if we're set to loop, and disable links, and we don't have any valid links re-enable any links we've auto-disabled and treat this as a new iteration
		if(bLooping && bAutoDisableLinks && ValidLinks.Num() < 1)
		{
			INT AutoDisabledIdx = 0;
			for(INT Idx=0; Idx<AutoDisabledIndices.Num(); Idx++)
			{
				AutoDisabledIdx = AutoDisabledIndices(Idx);
				// turn it back on for the next iteration
				OutputLinks(AutoDisabledIdx).bDisabled = FALSE;

				if (!(OutputLinks(AutoDisabledIdx).bDisabledPIE && GIsEditor))
				{
					ValidLinks.AddItem(AutoDisabledIdx);
				}

			}
			AutoDisabledIndices.Empty();
		}

		if (ValidLinks.Num() > 0)
		{
			// pick a random link to activate
			INT OutIdx = ValidLinks(appRand() % ValidLinks.Num());
			OutputLinks(OutIdx).bHasImpulse = TRUE;
			if (bAutoDisableLinks)
			{
				AutoDisabledIndices.AddItem(OutIdx);
				OutputLinks(OutIdx).bDisabled = TRUE;
			}
			// fill any variables attached
			for (INT Idx = 0; Idx < Indices.Num(); Idx++)
			{
				// offset by 1 for non-programmer friendliness
				Indices(Idx) = OutIdx + 1;
			}
		}
	}
}

static APawn* FindPawn(UClass* PawnClass)
{
	for (APawn *Pawn = GWorld->GetWorldInfo()->PawnList; Pawn != NULL; Pawn = Pawn->NextPawn)
	{
		if (Pawn->IsA(PawnClass))
		{
			// if the Pawn is in a vehicle, return the vehicle instead
			return (Pawn->DrivenVehicle != NULL) ? Pawn->DrivenVehicle : Pawn;
		}
	}
	return NULL;
}

UObject** USeqVar_Character::GetObjectRef(INT Idx)
{
	if (Idx == 0)
	{
		if( GWorld != NULL )
		{
			APawn* Pawn = FindPawn(PawnClass);
			if (Pawn != NULL)
			{
				ObjValue = Pawn;
				// use the controller if possible, otherwise default to the pawn
				if (Pawn->Controller != NULL)
				{
					ObjValue = Pawn->Controller;
				}
			}
			else
			{
				ObjValue = NULL;
				if ( GIsGame )
				{
					KISMET_WARN(TEXT("Failed to find Character for scripting! %s"), *GetName());
				}
			}
			if (ObjValue != NULL)
			{
				return &ObjValue;
			}
		}
	}
	return NULL;
}

#if WITH_EDITOR
void USeqVar_Character::CheckForErrors()
{
	Super::CheckForErrors();

	if (GWarn != NULL && GWarn->MapCheck_IsActive())
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, NULL, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KismetSeqVarCharObsolete" ), *GetPathName() ) ), TEXT( "KismetSeqVarCharObsolete" ), MCGROUP_KISMET );
	}
}
#endif


//==========================
// USeqAct_Gate interface

void USeqAct_AndGate::Initialize()
{
	Super::Initialize();

	TArray<FSeqOpOutputLink*> Links;
	ParentSequence->FindLinksToSeqOp(this, LinkedOutputs);

	LinkedOutputFiredStatus.Reset();
	LinkedOutputFiredStatus.AddZeroed(LinkedOutputs.Num());
}

void USeqAct_AndGate::OnReceivedImpulse( class USequenceOp* ActivatorOp, INT InputLinkIndex )
{
	Super::OnReceivedImpulse(ActivatorOp, InputLinkIndex);

	// search activatorop for the output link
	if (bOpen && ActivatorOp)
	{
		for (INT OutputIdx=0; OutputIdx<ActivatorOp->OutputLinks.Num(); ++OutputIdx)
		{
			FSeqOpOutputLink* const Link = &ActivatorOp->OutputLinks(OutputIdx);

			INT CachedLinkIdx;
			if (LinkedOutputs.FindItem(Link, CachedLinkIdx))
			{
				LinkedOutputFiredStatus(CachedLinkIdx) = TRUE;
			}
		}
	}
}


void USeqAct_AndGate::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if (bOpen)
	{
		INT NumUnfired = 0;
		// update tracking of which inputs have fired and check if we've got them all yet.
		for (INT Idx=0; Idx<LinkedOutputs.Num(); ++Idx)
		{
			if (!LinkedOutputFiredStatus(Idx))
			{
				NumUnfired++;
				break;
			}
		}

		if (NumUnfired == 0)
		{
			// every input has fired, fire the output and close self
			if (!OutputLinks(0).bDisabled && 
				!(OutputLinks(0).bDisabledPIE && GIsEditor))
			{
				OutputLinks(0).bHasImpulse = TRUE;
			}

			bOpen = false;
		}
	}

	// intentionally not calling super here
}



void USeqCond_SwitchPlatform::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	UE3::EPlatformType Platform = appGetPlatformType();

	// start with specific platform outputs
	INT DesiredOutputIndex = -1;
	if (Platform == UE3::PLATFORM_Windows || Platform == UE3::PLATFORM_WindowsConsole || 
		Platform == UE3::PLATFORM_WindowsServer)
	{
		DesiredOutputIndex = 4;
	}
	else if (Platform == UE3::PLATFORM_Xbox360)
	{
		DesiredOutputIndex = 5;
	}
	else if (Platform == UE3::PLATFORM_PS3)
	{
		DesiredOutputIndex = 6;
	}
	else if (Platform == UE3::PLATFORM_IPhone)
	{
		DesiredOutputIndex = 7;
	}
	else if (Platform == UE3::PLATFORM_Android)
	{
		DesiredOutputIndex = 8;
	}
	else if (Platform == UE3::PLATFORM_Linux)
	{
		DesiredOutputIndex = 9;
	}
	else if (Platform == UE3::PLATFORM_MacOSX)
	{
		DesiredOutputIndex = 10;
	}
	else if (Platform == UE3::PLATFORM_NGP)
	{
		DesiredOutputIndex = 11;
	}
	else if (Platform == UE3::PLATFORM_WiiU)
	{
		DesiredOutputIndex = 12;
	}
	else if (Platform == UE3::PLATFORM_Flash)
	{
		DesiredOutputIndex = 13;
	}
	else
	{
		// print to the screen that the platform isn't supported
		if (GEngine->bOnScreenKismetWarnings)
		{
			FString LogString = FString::Printf(TEXT("The current platform (%s) is not supported in SeqAct_PlatformSwitch"), *appGetPlatformString());

			// iterate through the controller list
			for (AController *Controller = GWorld->GetFirstController(); Controller != NULL; Controller = Controller->NextController)
			{
				// if it's a player
				if (Controller->IsA(APlayerController::StaticClass()))
				{
					((APlayerController*)Controller)->eventClientMessage(LogString,NAME_None);
				}
			}
		}

		// let it fall through to the platform types 
	}

	// if the platform specific output has nothing hooked up, fallback to platform type output (computer, mobile)
	// also check that we don't fall off the end of the OutputLinks array. This could happen with an old switch 
	// after adding new platforms (existing ones won't get the new links added to them)
	if (DesiredOutputIndex == -1 || DesiredOutputIndex >= OutputLinks.Num() || 
		OutputLinks(DesiredOutputIndex).Links.Num() == 0)
	{
		if (Platform & UE3::PLATFORM_Mobile)
		{
			DesiredOutputIndex = 3;
		}
		else if (Platform & UE3::PLATFORM_Console)
		{
			DesiredOutputIndex = 2;
		}
		else
		{
			// desktop
			DesiredOutputIndex = 1;
		}
	}

	// if the platform type didn't get a good output, fallback to default (always the first one)
	if (OutputLinks(DesiredOutputIndex).Links.Num() == 0)
	{
		DesiredOutputIndex = 0;
	}

	// trigger the final output
	OutputLinks(DesiredOutputIndex).ActivateOutputLink();
}


void USeqCond_IsPIE::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if( GIsPlayInEditorWorld )
	{
		OutputLinks(0).ActivateOutputLink();
	}
	else
	{
		OutputLinks(1).ActivateOutputLink();
	}		
}


void USeqCond_IsBenchmarking::Activated()
{
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	if( GIsBenchmarking )
	{
		OutputLinks(0).ActivateOutputLink();
	}
	else
	{
		OutputLinks(1).ActivateOutputLink();
	}		
}

void USeqAct_SetApexClothingParam::Activated()
{
#if WITH_APEX
	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints

	for(INT TargetIndex = 0; TargetIndex < Targets.Num(); TargetIndex++)
	{
		USkeletalMeshComponent *Component         = 0;
		ASkeletalMeshActor     *SkeletalMeshActor = Cast<ASkeletalMeshActor>(Targets(TargetIndex));
		check(SkeletalMeshActor);
		if(SkeletalMeshActor)
		{
			Component = SkeletalMeshActor->SkeletalMeshComponent;
			check(Component);
			if(Component)
			{
				Component->SetEnableClothingSimulation(bEnableApexClothingSimulation);
			}
		}
	}
#endif
}

void USeqAct_HeadTrackingControl::Activated()
{
	// enabled
	if (InputLinks(0).bHasImpulse)
	{
		ActorToComponentMap.Empty();
		for (INT I=0; I<Targets.Num(); ++I)
		{
			AActor * TargetActor = Cast<AActor>(Targets(I));
			UBOOL bAttach = FALSE;
			if (TargetActor)
			{
				USkeletalMeshComponent * MeshComponent = GetSkeletalMeshComp(TargetActor);

				if (MeshComponent)
				{
					// see if target actor already has head tracking component
					UHeadTrackingComponent * HeadTrackingComp = NULL;
					for ( INT CompID=0; CompID<TargetActor->Components.Num(); ++CompID )
					{
						if ( TargetActor->Components(CompID)->GetClass()->IsChildOf(UHeadTrackingComponent::StaticClass()) )
						{
							// use the one already exist
							HeadTrackingComp = CastChecked<UHeadTrackingComponent>(TargetActor->Components(CompID));
							break;
						}
					}

					if (!HeadTrackingComp)
					{
						HeadTrackingComp = ConstructObject<UHeadTrackingComponent>(UHeadTrackingComponent::StaticClass());
						bAttach = TRUE;
					}

					HeadTrackingComp->SkeletalMeshComp = MeshComponent;
					HeadTrackingComp->TrackControllerName = TrackControllerName;
					HeadTrackingComp->ActorClassesToLookAt.Empty();
					if (bLookAtPawns)
					{
						HeadTrackingComp->ActorClassesToLookAt.AddItem(APawn::StaticClass());
					}

					HeadTrackingComp->ActorClassesToLookAt.Append(ActorClassesToLookAt);

					HeadTrackingComp->MinLookAtTime = MinLookAtTime;
					HeadTrackingComp->MaxLookAtTime = MaxLookAtTime;
					HeadTrackingComp->MaxInterestTime = MaxInterestTime;
					HeadTrackingComp->LookAtActorRadius = LookAtActorRadius;

					HeadTrackingComp->TargetBoneNames = TargetBoneNames;

					if (bAttach)
					{
						TargetActor->AttachComponent(HeadTrackingComp);
					}

					HeadTrackingComp->EnableHeadTracking(TRUE);

					// now add this to map so that I can clean up properly
					ActorToComponentMap.Set(TargetActor, HeadTrackingComp);
				}
			}
		}
		// activate enabled output
		OutputLinks(0).ActivateOutputLink();
	}
	else
	{
		// sadly if you search empty map, it crashes
		// ideally it would be great to return nothing instead. 
		if (ActorToComponentMap.Num() > 0 )
		{
			// detach
			// find my component
			for (INT I=0; I<Targets.Num(); ++I)
			{
				AActor * TargetActor = Cast<AActor>(Targets(I));
				UHeadTrackingComponent ** HeadTrackingComp = NULL;
				if (TargetActor)
				{
					HeadTrackingComp = ActorToComponentMap.Find(TargetActor);
					if (HeadTrackingComp)
					{
						(*HeadTrackingComp)->EnableHeadTracking(FALSE);
						TargetActor->DetachComponent(*HeadTrackingComp);
						ActorToComponentMap.Remove(TargetActor);
					}
				}
			}
		}

		OutputLinks(1).ActivateOutputLink();
	}

	Super::Activated();
}

#if WITH_EDITOR
void USeqAct_ModifyCover::CheckForErrors()
{
	Super::CheckForErrors();

	if ( GWarn != NULL && GWarn->MapCheck_IsActive() )
	{
		USequence* OwningSeq = ParentSequence;

		if( OwningSeq != NULL )
		{
			for(INT i=0; i<OwningSeq->SequenceObjects.Num(); i++)
			{
				if(OwningSeq->SequenceObjects(i) != NULL)
				{
					USequenceOp* SeqObj = Cast<USequenceOp>(OwningSeq->SequenceObjects(i));
					if ( SeqObj != NULL )
					{
						for(INT SeqObjOutIdx=0;SeqObjOutIdx<SeqObj->OutputLinks.Num();++SeqObjOutIdx)
						{
							FSeqOpOutputLink& OutLink = SeqObj->OutputLinks(SeqObjOutIdx);
							for(INT OutLinkIdx=0;OutLinkIdx<OutLink.Links.Num();++OutLinkIdx)
							{
								FSeqOpOutputInputLink& OutInputLink = OutLink.Links(OutLinkIdx);
								if (OutInputLink.LinkedOp == this && OutInputLink.InputLinkIdx == 2)
								{
									GWarn->MapCheck_Add( MCTYPE_PERFORMANCEWARNING, NULL, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_KismetAutoAdjustEnabled" ), *GetPathName() ) ), TEXT( "KismetAutoAdjustEnabled" ), MCGROUP_KISMET );
									return;
								}
							}
						}
					}
				}
			}
		}
	}
}
#endif


UBOOL USeqEvent_Input::RegisterEvent()
{
	// add this event to any existing input objects
	for (INT GamePlayerIndex = 0; GamePlayerIndex < GEngine->GamePlayers.Num(); GamePlayerIndex++)
	{
		if (AllowedPlayerIndex == -1 || AllowedPlayerIndex == GamePlayerIndex)
		{
			// put this event into the list of cached kismet events that will allow the GamePlayerIndex
			if (GEngine->GamePlayers(GamePlayerIndex) && GEngine->GamePlayers(GamePlayerIndex)->Actor &&
				GEngine->GamePlayers(GamePlayerIndex)->Actor->PlayerInput)
			{
				GEngine->GamePlayers(GamePlayerIndex)->Actor->PlayerInput->CachedInputEvents.AddUniqueItem(this);
			}
		}
	}

	return TRUE;
}

/**
 * @return Does this event care about the given input name?
 */
UBOOL USeqEvent_Input::HasMatchingInput(FName InputName)
{
	// is the name in the list?
	return InputNames.ContainsItem(InputName);
}

/**
 * Trigger the event as needed. If this returns TRUE, and bTrapInput is TRUE, then the caller should
 * stop processing the input.
 */
UBOOL USeqEvent_Input::CheckInputActivate(INT InPlayerIndex, FName InputName, EInputEvent Action)
{
	// an action we care about?
	if (Action == IE_Pressed || Action == IE_Released || Action == IE_Repeat)
	{
		// do we respond to the name?
 		if (HasMatchingInput(InputName))
		{
			// do standard checking
			TArray<INT> ActivateIndices;
			ActivateIndices.AddItem(Action == IE_Pressed ? 0 : Action == IE_Repeat ? 1 : 2);
			
			Originator = GEngine->GamePlayers(InPlayerIndex)->Actor;
			if (CheckActivate(Originator,Originator,0,&ActivateIndices))
			{
				// write out which event was triggered
				TArray<FString*> StringVars;
				GetStringVars(StringVars,TEXT("Input Name"));
				for (INT Idx = 0; Idx < StringVars.Num(); Idx++)
				{
					*(StringVars(Idx)) = InputName.ToString();
				}

				return TRUE;
			}
		}
	}

	return FALSE;
}




UBOOL USeqEvent_AnalogInput::RegisterEvent()
{
	// add this event to any existing input objects
	for (INT GamePlayerIndex = 0; GamePlayerIndex < GEngine->GamePlayers.Num(); GamePlayerIndex++)
	{
		if (AllowedPlayerIndex == -1 || AllowedPlayerIndex == GamePlayerIndex)
		{
			// put this event into the list of cached kismet events that will allow the GamePlayerIndex
			if (GEngine->GamePlayers(GamePlayerIndex) && GEngine->GamePlayers(GamePlayerIndex)->Actor &&
				GEngine->GamePlayers(GamePlayerIndex)->Actor->PlayerInput)
			{
				GEngine->GamePlayers(GamePlayerIndex)->Actor->PlayerInput->CachedAnalogInputEvents.AddUniqueItem(this);
			}
		}
	}

	return TRUE;
}

/**
	* @return Does this event care about the given input name?
	*/
UBOOL USeqEvent_AnalogInput::HasMatchingInput(FName InputName)
{
	// is the name in the list?
	return InputNames.ContainsItem(InputName);
}

/**
	* Trigger the event as needed. If this returns TRUE, and bTrapInput is TRUE, then the caller should
	* stop processing the input.
	*/
UBOOL USeqEvent_AnalogInput::CheckInputActivate(INT PlayerIndex, FName InputName, FLOAT Value)
{
	// do we respond to the name?
	if (HasMatchingInput(InputName))
	{
		// do standard checking
		TArray<INT> ActivateIndices;
		ActivateIndices.AddItem(0);

		Originator = GEngine->GamePlayers(PlayerIndex)->Actor;
		if (CheckActivate(Originator,Originator,0,&ActivateIndices))
		{
			// write out which event was triggered
			TArray<FString*> StringVars;
			GetStringVars(StringVars,TEXT("Input Name"));
			for (INT Idx = 0; Idx < StringVars.Num(); Idx++)
			{
				*(StringVars(Idx)) = InputName.ToString();
			}

			// write out the actual axis value
			TArray<FLOAT*> FloatVars;
			GetFloatVars(FloatVars,TEXT("Float Value"));
			for (INT Idx = 0; Idx < FloatVars.Num(); Idx++)
			{
				*(FloatVars(Idx)) = Value;
			}

			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Trigger the event as needed. If this returns TRUE, and bTrapInput is TRUE, then the caller should
 * stop processing the input.
 */
UBOOL USeqEvent_AnalogInput::CheckInputActivate(INT PlayerIndex, FName InputName, FVector Value)
{
	// do we respond to the name?
	if (HasMatchingInput(InputName))
	{
		// do standard checking
		TArray<INT> ActivateIndices;
		ActivateIndices.AddItem(0);

		Originator = GEngine->GamePlayers(PlayerIndex)->Actor;
		if (CheckActivate(Originator,Originator,0,&ActivateIndices))
		{
			// write out which event was triggered
			TArray<FString*> StringVars;
			GetStringVars(StringVars,TEXT("Input Name"));
			for (INT Idx = 0; Idx < StringVars.Num(); Idx++)
			{
				*(StringVars(Idx)) = InputName.ToString();
			}

			// write out the actual axis value
			TArray<FVector*> VectorVars;
			GetVectorVars(VectorVars,TEXT("Vector Value"));
			for (INT Idx = 0; Idx < VectorVars.Num(); Idx++)
			{
				*(VectorVars(Idx)) = Value;
			}

			return TRUE;
		}
	}

	return FALSE;
}




UBOOL USeqEvent_TouchInput::RegisterEvent()
{
	// add this event to any existing input objects
	for (INT GamePlayerIndex = 0; GamePlayerIndex < GEngine->GamePlayers.Num(); GamePlayerIndex++)
	{
		if (AllowedPlayerIndex == -1 || AllowedPlayerIndex == GamePlayerIndex)
		{
			// put this event into the list of cached kismet events that will allow the GamePlayerIndex
			if (GEngine->GamePlayers(GamePlayerIndex) && GEngine->GamePlayers(GamePlayerIndex)->Actor &&
				GEngine->GamePlayers(GamePlayerIndex)->Actor->PlayerInput)
			{
				GEngine->GamePlayers(GamePlayerIndex)->Actor->PlayerInput->CachedTouchInputEvents.AddUniqueItem(this);
			}
		}
	}

	return TRUE;
}

/**
	* Trigger the event as needed. If this returns TRUE, and bTrapInput is TRUE, then the caller should
	* stop processing the input.
	*/
UBOOL USeqEvent_TouchInput::CheckInputActivate(INT PlayerIndex, INT TouchIndex, INT TouchpadIndex, EInputEvent Action, const FVector2D& Location)
{
	// do standard checking
	TArray<INT> ActivateIndices;
	ActivateIndices.AddItem(Action == IE_Pressed ? 0 : Action == IE_Repeat ? 1 : 2);

	Originator = GEngine->GamePlayers(PlayerIndex)->Actor;
	if (CheckActivate(Originator,Originator,0,&ActivateIndices))
	{
		// write out the location
		TArray<FLOAT*> FloatVars;

		INT FloatVarsIdx = 0;
		GetFloatVars(FloatVars,TEXT("Touch X"));
		for (; FloatVarsIdx < FloatVars.Num(); FloatVarsIdx++)
		{
			*(FloatVars(FloatVarsIdx)) = Location.X;
		}
		GetFloatVars(FloatVars,TEXT("Touch Y"));
		for (; FloatVarsIdx < FloatVars.Num(); FloatVarsIdx++)
		{
			*(FloatVars(FloatVarsIdx)) = Location.Y;
		}

		// write out touch/touchpad index
		TArray<INT*> IntVars;

		INT IntVarsIdx = 0;
		GetIntVars(IntVars,TEXT("Touch Index"));
		for (; IntVarsIdx < IntVars.Num(); IntVarsIdx++)
		{
			*(IntVars(IntVarsIdx)) = TouchIndex;
		}
		GetIntVars(IntVars,TEXT("Touchpad Index"));
		for (; IntVarsIdx < IntVars.Num(); IntVarsIdx++)
		{
			*(IntVars(IntVarsIdx)) = TouchpadIndex;
		}

		return TRUE;
	}

	return FALSE;
}

void USeqAct_SetActiveAnimChild::Activated()
{
	if ( NodeName==NAME_None || ChildIndex <= 0 )
	{
		return;
	}

	for (INT Idx = 0; Idx < Targets.Num(); Idx++)
	{
		UObject *Obj = Targets(Idx);
		if (Obj != NULL && !Obj->IsPendingKill())
		{
			USkeletalMeshComponent * MeshComponent = NULL;
			APawn * Pawn = Cast<APawn>(Obj);
			if (Pawn )
			{
				MeshComponent = Pawn->Mesh;
			}
			else
			{
				ASkeletalMeshActorMAT * SMA = Cast<ASkeletalMeshActorMAT>(Obj);
				if (SMA)
				{
					MeshComponent = SMA->SkeletalMeshComponent;
				}
			}

			if (MeshComponent)
			{
				UAnimTree * Tree = Cast<UAnimTree>(MeshComponent->Animations);
				if (Tree)
				{
					UAnimNodeBlendList * Node = Cast<UAnimNodeBlendList>(Tree->FindAnimNode(NodeName));
					if (Node)
					{
						// ChildTime starts with 1
						Node->SetActiveChild(ChildIndex-1, BlendTime);
					}
				}
			}
		}
	}
}