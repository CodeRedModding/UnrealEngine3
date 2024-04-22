//! @file SubstanceAirInterpolation.cpp
//! @brief The interpolation classes implementation
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "Engine.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"

#include "SubstanceAirActorClasses.h"
#include "SubstanceAirInterpolationClasses.h"

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirInput.h"


void UInterpTrackInstSubstanceInput::InitTrackInst(UInterpTrack* Track)
{
	if (GIsEditor && !GIsGame)
	{
		UInterpTrackSubstanceInput* ParamTrack = Cast<UInterpTrackSubstanceInput>(Track);

		if (ParamTrack != NULL)
		{
			InstancedTrack = ParamTrack;
		}
	}
}


void UInterpTrackInstSubstanceInput::SaveActorState(UInterpTrack* Track)
{
	UInterpTrackSubstanceInput* ParamTrack = CastChecked<UInterpTrackSubstanceInput>(Track);
	AActor* Actor = GetGroupActor();
	ASubstanceAirGraphActor* GraphInstActor = Cast<ASubstanceAirGraphActor>(Actor);
	if (!GraphInstActor || !GraphInstActor->GraphInst)
	{
		return;
	}

	input_inst_t* Inst = 
		GraphInstActor->GraphInst->Instance->GetInput(
			ParamTrack->ParamName.ToString());
		
	if (NULL == Inst || !Inst->IsNumerical())
	{
		ResetValue.Reset();
		return;
	}
	else
	{
		num_input_inst_t* NumInst = 
			(num_input_inst_t*)Inst;

		ResetValue.Reset();
		NumInst->GetValue(ResetValue);
		ParamTrack->NumSubcurve = ResetValue.Num();
	}
}


void UInterpTrackInstSubstanceInput::RestoreActorState(UInterpTrack* Track)
{
	UInterpTrackSubstanceInput* ParamTrack = CastChecked<UInterpTrackSubstanceInput>(Track);
	AActor* Actor = GetGroupActor();
	ASubstanceAirGraphActor* GraphInstActor = Cast<ASubstanceAirGraphActor>(Actor);

	if (!GraphInstActor || 
		!GraphInstActor->GraphInst || 
		!GraphInstActor->GraphInst->Instance ||
		0 == ParamTrack->NumSubcurve)
	{
		return;
	}

	GraphInstActor->GraphInst->Instance->UpdateInput( 
		ParamTrack->ParamName.ToString(),
		ResetValue);

	SubstanceAir::Helpers::RenderAsync(GraphInstActor->GraphInst->Instance);
}


INT UInterpTrackSubstanceInput::GetNumSubCurves() const
{
	return NumSubcurve;
}


INT UInterpTrackSubstanceInput::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = LinearColorTrack.AddPoint(Time, FLinearColor(0.f, 0.f, 0.f, 1.f));
	LinearColorTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;
	LinearColorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackSubstanceInput::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, FALSE);
}


static void PreEditChangeGraphInstanceParamTrack()
{
	if (GIsEditor && !GIsGame)
	{
		for (TObjectIterator<UInterpTrackInstSubstanceInput> It; It; ++It)
		{
			UInterpGroupInst* GrInst = Cast<UInterpGroupInst>(It->GetOuter());
			if (GrInst != NULL && GrInst->TrackInst.ContainsItem(*It))
			{
				USeqAct_Interp* InterpAct = Cast<USeqAct_Interp>(GrInst->GetOuter());
				if (InterpAct != NULL && InterpAct->bIsBeingEdited && InterpAct->GroupInst.ContainsItem(GrInst))
				{
					It->RestoreActorState(It->InstancedTrack);
					It->TermTrackInst(It->InstancedTrack);
				}
			}
		}
	}
}


static void PostEditChangeGraphInstanceParamTrack()
{
	if (GIsEditor && !GIsGame)
	{
		for (TObjectIterator<UInterpTrackInstSubstanceInput> It; It; ++It)
		{
			UInterpGroupInst* GrInst = Cast<UInterpGroupInst>(It->GetOuter());
			if (GrInst != NULL && GrInst->TrackInst.ContainsItem(*It))
			{
				USeqAct_Interp* InterpAct = Cast<USeqAct_Interp>(GrInst->GetOuter());
				if (InterpAct != NULL && InterpAct->bIsBeingEdited && InterpAct->GroupInst.ContainsItem(GrInst))
				{
					It->InitTrackInst(It->InstancedTrack);
					It->SaveActorState(It->InstancedTrack);
					It->InstancedTrack->PreviewUpdateTrack(InterpAct->Position, *It);
				}
			}
		}
	}
}


void UInterpTrackSubstanceInput::PreEditChange(UProperty* PropertyThatWillChange)
{
	if (PropertyThatWillChange->GetName() == TEXT("ParamName"))
	{
		PreEditChangeGraphInstanceParamTrack();
	}

	Super::PreEditChange(PropertyThatWillChange);
}


void UInterpTrackSubstanceInput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property->GetName() == TEXT("ParamName"))
	{
		PostEditChangeGraphInstanceParamTrack();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UInterpTrackSubstanceInput::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	ASubstanceAirGraphActor* GraphInstActor = Cast<ASubstanceAirGraphActor>(Actor);

	if(!GraphInstActor || !GraphInstActor->GraphInst)
	{
		return;
	}

	UInterpTrackInstSubstanceInput* TrInstInp = CastChecked<UInterpTrackInstSubstanceInput>(TrInst);

	if (NumSubcurve)
	{
		FLinearColor Value = LinearColorTrack.Eval(NewPosition, FLinearColor(0.0f,0.0f,0.0f,0.0f));

		TArray<FLOAT> NewTweakValue;
		for (INT I = 0 ; I < NumSubcurve ; ++I)
		{
			NewTweakValue.AddItem(Value.Component(I));
		}

		GraphInstActor->GraphInst->Instance->UpdateInput( ParamName.ToString(), NewTweakValue );
		SubstanceAir::Helpers::RenderAsync(GraphInstActor->GraphInst->Instance);
	}
}

IMPLEMENT_CLASS( UInterpTrackSubstanceInput )
IMPLEMENT_CLASS( UInterpTrackInstSubstanceInput )

