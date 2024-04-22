/*=============================================================================
	Spline.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineSplineClasses.h"

IMPLEMENT_CLASS(ASplineActor);
IMPLEMENT_CLASS(USplineComponent);

// HSplineProxy
void HSplineProxy::Serialize(FArchive& Ar)
{
	Ar << SplineComp;
}	


//////////////////////////////////////////////////////////////////////////
// SPLINE SCENE PROXY

/** Represents a USplineComponent to the scene manager. */
class FSplineSceneProxy : public FPrimitiveSceneProxy
{
private:
	FColor						SplineColor;
	FInterpCurveInitVector		SplineInfo;
	FLOAT						SplineDrawRes;
	FLOAT						SplineArrowSize;
	ASplineActor*				SplineActor;
	FLOAT						bSplineDisabled;

public:

	FSplineSceneProxy(USplineComponent* Component):
		FPrimitiveSceneProxy(Component),
		SplineColor(Component->SplineColor),
		SplineDrawRes(Component->SplineDrawRes),
		SplineArrowSize(Component->SplineArrowSize),
		bSplineDisabled(Component->bSplineDisabled)
	{
		SplineInfo = *(FInterpCurveInitVector*)&(Component->SplineInfo);
		SplineActor = Cast<ASplineActor>(Component->GetOwner());
	}

	virtual HHitProxy* CreateHitProxies(const UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
	{
		HSplineProxy* SplineHitProxy = new HSplineProxy( CastChecked<USplineComponent>(Component) );
		OutHitProxies.AddItem(SplineHitProxy);
		return SplineHitProxy;
	}

	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		// Determine the DPG the primitive should be drawn in for this view.
		if (GetDepthPriorityGroup(View) == DPGIndex)
		{
			// Override specified color with red if spline is 'disabled'
			FColor UseColor = bSplineDisabled ? FColor(255,0,0) : SplineColor;
		
			//DrawDirectionalArrow(PDI, LocalToWorld, SplineColor, 1.0 * 3.0f, 1.0f, DPGIndex);		
			FVector OldKeyPos(0);
			FLOAT OldKeyTime = 0.f;
			for(INT i=0; i<SplineInfo.Points.Num(); i++)
			{
				FLOAT NewKeyTime = SplineInfo.Points(i).InVal;
				FVector NewKeyPos = SplineInfo.Eval(NewKeyTime, FVector(0.f));

				// If not the first keypoint, draw a line to the last keypoint.
				if(i>0)
				{
					INT NumSteps = appCeil( (NewKeyTime - OldKeyTime)/SplineDrawRes );
					FLOAT DrawSubstep = (NewKeyTime - OldKeyTime)/NumSteps;

					// Find position on first keyframe.
					FLOAT OldTime = OldKeyTime;
					FVector OldPos = SplineInfo.Eval(OldKeyTime, FVector(0.f));

					// For constant interpolation - don't draw ticks - just draw dotted line.
					if(SplineInfo.Points(i-1).InterpMode == CIM_Constant)
					{
						DrawDashedLine(PDI, OldPos, NewKeyPos, UseColor, 20, DPGIndex);
					}
					else
					{
						// Find step that we want to draw the arrow head on.
						INT ArrowStep = Max<INT>(0, NumSteps-2);
					
						// Then draw a line for each substep.
						for(INT j=1; j<NumSteps+1; j++)
						{
							FLOAT NewTime = OldKeyTime + j*DrawSubstep;
							FVector NewPos = SplineInfo.Eval(NewTime, FVector(0.f));

							// If this is where we want to draw the arrow, do that
							if(j == ArrowStep)
							{
								// Only draw arrow if SplineArrowSize > 0.0
								if(SplineArrowSize > KINDA_SMALL_NUMBER)
								{
									FMatrix ArrowMatrix;								
									FVector ArrowDir = (NewPos - OldPos);
									FLOAT ArrowLen = ArrowDir.Size();
									if(ArrowLen > KINDA_SMALL_NUMBER)
									{
										ArrowDir /= ArrowLen;
									}

									ArrowMatrix = FRotationTranslationMatrix(ArrowDir.Rotation(), OldPos);

									DrawDirectionalArrow(PDI, ArrowMatrix, UseColor, ArrowLen, SplineArrowSize, DPGIndex);								
								}
								else
								{
									PDI->DrawLine(OldPos, NewPos, UseColor, DPGIndex);
								}							
							}
							// Otherwise normal line
							else
							{
								PDI->DrawLine(OldPos, NewPos, UseColor, DPGIndex);														
							}

							// Don't draw point for last one - its the keypoint drawn above.
							if(j != NumSteps)
							{
								PDI->DrawPoint(NewPos, UseColor, 3.f, DPGIndex);
							}

							OldTime = NewTime;
							OldPos = NewPos;
						}
					}
				}

				OldKeyTime = NewKeyTime;
				OldKeyPos = NewKeyPos;
			}			
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;

		const EShowFlags ShowFlags = View->Family->ShowFlags;
		if (IsShown(View) && (ShowFlags & SHOW_Splines))
		{
			Result.bDynamicRelevance = IsShown(View);
			Result.SetDPG(SDPG_World, TRUE);		
		}
	
		return Result;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }


};

//////////////////////////////////////////////////////////////////////////
// SPLINE COMPONENT

/** Create scene proxy */
FPrimitiveSceneProxy* USplineComponent::CreateSceneProxy()
{
	return new FSplineSceneProxy(this);
}

/** Update bounds */
void USplineComponent::UpdateBounds()
{
	FVector BoxMin, BoxMax;
	SplineInfo.CalcBounds(BoxMin, BoxMax, LocalToWorld.GetOrigin());

	Bounds = FBoxSphereBounds( FBox(BoxMin, BoxMax) );
}

/** Update the SplineReparamTable */
void USplineComponent::UpdateSplineReparamTable()
{
	// Start by clearing it
	SplineReparamTable.Reset();
	
	// Nothing to do if no points
	if(SplineInfo.Points.Num() < 2)
	{
		return;
	}
	
	const INT NumSteps = 10; // TODO: Make this adaptive...
	
	// Find range of input
	FLOAT Param = SplineInfo.Points(0).InVal;
	const FLOAT MaxInput = SplineInfo.Points(SplineInfo.Points.Num()-1).InVal;
	const FLOAT Interval = (MaxInput - Param)/((FLOAT)(NumSteps-1)); 
	
	// Add first entry, using first point on curve, total distance will be 0
	FVector OldSplinePos = SplineInfo.Eval(Param, FVector(0.f));
	FLOAT TotalDist = 0.f;
	SplineReparamTable.AddPoint(TotalDist, Param);
	Param += Interval;

	// Then work over rest of points	
	for(INT i=1; i<NumSteps; i++)
	{
		// Iterate along spline at regular param intervals
		const FVector NewSplinePos = SplineInfo.Eval(Param, FVector(0.f));
		
		TotalDist += (NewSplinePos - OldSplinePos).Size();
		OldSplinePos = NewSplinePos;

		SplineReparamTable.AddPoint(TotalDist, Param);

		// move along
		Param += Interval;
	}
}


void USplineComponent::UpdateSplineCurviness()
{
	const FLOAT SplineLength = GetSplineLength();

	const FVector Start = GetLocationAtDistanceAlongSpline( 0.0f );;
	const FVector End = GetLocationAtDistanceAlongSpline( SplineLength );

	SplineCurviness =(Start - End).Size() / SplineLength;

	//GWorld->PersistentLineBatcher->DrawLine( Start, Start+FVector(0,0,200), FColor(255,0,0), SDPG_World );
	//GWorld->PersistentLineBatcher->DrawLine( End, End+FVector(0,0,200), FColor(255,0,0), SDPG_World );

	//warnf( TEXT( "hahah %s %f %f %f"), *GetName(), SplineCurviness, (Start - End).Size(), SplineLength );
}

/** Returns total length along this spline */
FLOAT USplineComponent::GetSplineLength() const
{
	// This is given by the input of the last entry in the remap table
	if(SplineReparamTable.Points.Num() > 0)
	{
		return SplineReparamTable.Points(SplineReparamTable.Points.Num()-1).InVal;
	}
	
	return 0.f;
}

/** Given a distance along the length of this spline, return the point in space where this puts you */
FVector USplineComponent::GetLocationAtDistanceAlongSpline(FLOAT Distance) const
{
	const FLOAT Param = SplineReparamTable.Eval(Distance, 0.f);
	const FVector Location = SplineInfo.Eval(Param, FVector(0.f));
	return Location;
}

/** Given a distance along the length of this spline, return the direction of the spline there. Note, result is non-unit length. */
FVector USplineComponent::GetTangentAtDistanceAlongSpline(FLOAT Distance) const
{
	const FLOAT Param = SplineReparamTable.Eval(Distance, 0.f);
	const FVector Tangent = SplineInfo.EvalDerivative(Param, FVector(0.f));
	return Tangent;
}

//////////////////////////////////////////////////////////////////////////
// SPLINE ACTOR

void ASplineActor::PostLoad()
{
	for(INT i=0; i<Connections.Num(); i++)
	{
		if(Connections(i).SplineComponent)
		{
			Components.AddItem(Connections(i).SplineComponent);	
		}
		
		// To keep older maps working, recreate the LinksFrom array now
		if(Connections(i).ConnectTo && GetLinker() && (GetLinker()->Ver() < VER_ADDED_SPLINEACTOR_BACK_REFS))
		{
			Connections(i).ConnectTo->LinksFrom.AddUniqueItem(this);
		}
	}

#if !FINAL_RELEASE
	// There was a bug which resulted in LinksFrom having invalid entries - clean them up here!
	for(INT LinkIdx=LinksFrom.Num()-1; LinkIdx>=0; LinkIdx--)
	{
		if((LinksFrom(LinkIdx) == NULL) || !LinksFrom(LinkIdx)->IsConnectedTo(this,FALSE))
		{
			debugf(TEXT("WARNING: Removed incorrect back-link from %s to %s."), *this->GetName(), *LinksFrom(LinkIdx)->GetName());
			LinksFrom.Remove(LinkIdx);
		}
	}
#endif // !FINAL_RELEASE

	Super::PostLoad();
}

void ASplineActor::PostScriptDestroyed()
{
	Super::PostScriptDestroyed();

	// Break all connections, such that other spline's LinksFrom and Connections array are up-to-date
	BreakAllConnections();
}

void ASplineActor::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove(bFinished);
	
	UpdateConnectedSplineComponents(bFinished);
}

void ASplineActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	UpdateConnectedSplineComponents(TRUE);	
}

/** Returns spline tangent at this actor, in world space */
FVector ASplineActor::GetWorldSpaceTangent()
{
	return LocalToWorld().TransformNormal(SplineActorTangent);
}

/** Create/destroy/update SplineComponents belonging to this SplineActor. */
void ASplineActor::UpdateSplineComponents()
{
	for(INT i=0; i<Connections.Num(); i++)
	{
		// First we check that presence of actor and presence of spline component match up
	
		// See if we are connected but have no component
		if(Connections(i).ConnectTo && !Connections(i).SplineComponent)
		{
			Connections(i).SplineComponent = ConstructObject<USplineComponent>(USplineComponent::StaticClass(), this);
			check(Connections(i).SplineComponent);
						
			AttachComponent(Connections(i).SplineComponent);
		}
		// See if we have a component but are not connected.
		else if(!Connections(i).ConnectTo && Connections(i).SplineComponent)
		{
			DetachComponent(Connections(i).SplineComponent);
			Connections(i).SplineComponent = NULL;
		}
	
		// Now we either update the component, or remove the entry from the array
	
		// Invalid entry - remove it
		if(!Connections(i).ConnectTo)
		{
			Connections.Remove(i);
			i--;
		}
		// Valid entry - update spline info
		else
		{	
			ASplineActor* ConnectTo = Connections(i).ConnectTo;
			USplineComponent* SplineComp = Connections(i).SplineComponent;
			check(SplineComp);

			FComponentReattachContext ReattachContext(SplineComp);

			// clear and update spline info
			SplineComp->SplineInfo.Reset();

			FVector WorldTangent = GetWorldSpaceTangent();
			INT NewIndex = SplineComp->SplineInfo.AddPoint(0.f, Location);
			SplineComp->SplineInfo.Points(NewIndex).InterpMode = CIM_CurveUser;
			SplineComp->SplineInfo.Points(NewIndex).ArriveTangent = WorldTangent;
			SplineComp->SplineInfo.Points(NewIndex).LeaveTangent = WorldTangent;

			WorldTangent = ConnectTo->GetWorldSpaceTangent();
			NewIndex = SplineComp->SplineInfo.AddPoint(1.f, ConnectTo->Location);
			SplineComp->SplineInfo.Points(NewIndex).InterpMode = CIM_CurveUser;		
			SplineComp->SplineInfo.Points(NewIndex).ArriveTangent = WorldTangent;
			SplineComp->SplineInfo.Points(NewIndex).LeaveTangent = WorldTangent;	
			
			// update disabledness
			SplineComp->bSplineDisabled = Connections(i).ConnectTo->bDisableDestination;
			// and color
			SplineComp->SplineColor = SplineColor;
			
			// Build distance-to-param lookup table
			SplineComp->UpdateSplineReparamTable();			
			SplineComp->UpdateSplineCurviness();

			// update hidden
			SplineComp->SetHiddenGame( bHidden );
		}
	}
}

/** Create/destroy/update SplineComponents belonging to this SplineActor, and all SplineActors that link to this. */
void ASplineActor::UpdateConnectedSplineComponents(UBOOL bFinish)
{
	UpdateSplineComponents();
	
	// Update any splines that refer to this one
	for(INT i=0; i<LinksFrom.Num(); i++)
	{
		ASplineActor* OtherSpline = LinksFrom(i);
		if(OtherSpline)
		{
			OtherSpline->UpdateSplineComponents();
		}
	}
}

/** Create a connection to another SplineActor */
void ASplineActor::AddConnectionTo(class ASplineActor* NextActor)
{
	Modify(TRUE);

	// If this is non-NULL and not already connected..
	if(NextActor && !IsConnectedTo(NextActor,FALSE))
	{
		// make new entry
		const INT NewConnIndex = Connections.AddZeroed();
		Connections(NewConnIndex).ConnectTo = NextActor;
		
		// Add back ref to target actor 
		NextActor->Modify(TRUE);
		NextActor->LinksFrom.AddUniqueItem(this);
		
		// this will create new component and set up spline
		UpdateSplineComponents();
	}
}

/** Returns TRUE if there is a connection from this SplineActor to NextActor */	
UBOOL ASplineActor::IsConnectedTo(class ASplineActor* NextActor, UBOOL bCheckForDisableDestination ) const
{
	// Never say TRUE if this is NULL
	if(!NextActor)
	{
		return FALSE;
	}

	// Look through connections
	for(INT i=0; i<Connections.Num(); i++)
	{
		if( bCheckForDisableDestination == FALSE )
		{
			if(Connections(i).ConnectTo == NextActor)
			{
				return TRUE;
			}
		}
		// we want to make certain that we are connected to an actor that doesn't have their ConnectTo disabled
		else
		{
			if( (Connections(i).ConnectTo == NextActor)
				&& ( Connections(i).ConnectTo->bDisableDestination == FALSE )
				)
			{
				return TRUE;
			}
		}

	}
	
	// No connection found
	return FALSE;
}

/** Returns the SplineComponent that connects this SplineActor to NextActor. Returns None if not connected */
USplineComponent* ASplineActor::FindSplineComponentTo(ASplineActor* NextActor)
{
	if(!NextActor)
	{
		return NULL;
	}

	for(INT i=0; i<Connections.Num(); i++)
	{
		if(Connections(i).ConnectTo == NextActor)
		{
			return Connections(i).SplineComponent;
		}
	}
	
	return NULL;
}

/** Returns the SplineActor that this SplineComponent connects to. */
ASplineActor* ASplineActor::FindTargetForComponent(USplineComponent* SplineComp)
{
	if(!SplineComp)
	{
		return NULL;
	}

	for(INT i=0; i<Connections.Num(); i++)
	{
		if(Connections(i).SplineComponent == SplineComp)
		{
			return Connections(i).ConnectTo;
		}
	}

	return NULL;
}

/** Breaks a connection from this SplineActor to NextActor */
void ASplineActor::BreakConnectionTo(class ASplineActor* NextActor)
{
	Modify(TRUE);

	// Never say TRUE if this is NULL
	if(!NextActor)
	{
		return;
	}

	// Look through connections
	for(INT i=0; i<Connections.Num(); i++)
	{
		// Found the one we want to break
		if(Connections(i).ConnectTo == NextActor)
		{		
			// Clear pointer
			Connections(i).ConnectTo = NULL;
			
			// Clear back pointer			
			NextActor->Modify(TRUE);
			NextActor->LinksFrom.RemoveItem(this);
			
			// This will clean up the component and remove the entry
			UpdateSplineComponents();
			break;
		}
	}
}

/** Break all connections from this SplineActor */
void ASplineActor::BreakAllConnectionsFrom()
{
	Modify(TRUE);
	
	// Make copy of all nodes we link to
	TArray<ASplineActor*> LinksTo;
	for(INT i=0; i<Connections.Num(); i++)
	{
		if(Connections(i).ConnectTo)
		{
			LinksTo.AddItem(Connections(i).ConnectTo);
		}
	}
	
	// Break link to each
	for(INT i=0; i<LinksTo.Num(); i++)
	{
		BreakConnectionTo( LinksTo(i) );
	}
}

/** Break all connections to and from this SplineActor */
void ASplineActor::BreakAllConnections()
{
	Modify(TRUE);

	// Clear all pointers from this SplineActor
	for(INT i=0; i<Connections.Num(); i++)
	{
		if( Connections(i).ConnectTo != NULL )
		{
			Connections(i).ConnectTo->Modify(TRUE);
			Connections(i).ConnectTo->LinksFrom.RemoveItem(this);
		}

		Connections(i).ConnectTo = NULL;
	}
	
	// This will clean up the components and remove all entries
	UpdateSplineComponents();
	
	// Now break connections from other SplineActors to this one.
		
	TArray<ASplineActor*> LinksFromCopy = LinksFrom; // Make a copy of array, as it will be modified by BreakConnectionTo	
	for(INT i=0; i<LinksFromCopy.Num(); i++)
	{
		ASplineActor* OtherSpline = LinksFromCopy(i);
		if(OtherSpline)
		{
			if(!OtherSpline->IsConnectedTo(this,FALSE))
			{
				debugf(TEXT("BreakAllConnections (%s) : %s in LinksFrom array, but does not connect."), *GetName(), *OtherSpline->GetName());
			}
		
			OtherSpline->BreakConnectionTo(this);
		}
	}
	
	if(LinksFrom.Num() != 0)
	{
		debugf(TEXT("BreakAllConnections (%s) : LinksFrom array not empty!"), *GetName());
	}	
	LinksFrom.Empty();
}

/** Returns a random SplineActor that is connected to from this SplineActor. If no connections, returns None. */
ASplineActor* ASplineActor::GetRandomConnection(UBOOL bUseLinksFrom)
{
	// Make list of options
	TArray<ASplineActor*> Options;
	if( !bUseLinksFrom )
	{
		// Use forward paths for determining connections
		for(INT i=0; i<Connections.Num(); i++)
		{
			// If non-NULL, and not disabled
			if(	Connections(i).ConnectTo && 
				Connections(i).SplineComponent && 
				!Connections(i).ConnectTo->bDisableDestination )
			{
				Options.AddItem(Connections(i).ConnectTo);
			}
		}
	}
	else
	{
		// Use backward paths for determining connections
		for(INT i=0; i<LinksFrom.Num(); i++)
		{
			// If non-NULL, and not disabled
			if(	LinksFrom(i) && 
				LinksFrom(i)->IsConnectedTo(this, FALSE) && 
				!LinksFrom(i)->bDisableDestination )
			{
				Options.AddItem(LinksFrom(i));
			}
		}
	}
	
	// Pick one randomly
	ASplineActor* Selection = NULL;
	if(Options.Num() > 0)
	{
		Selection = Options(appRand() % Options.Num());
	}
	return Selection;
}

/** Returns a SplineActor that takes you most in the specified direction. If no connections, returns None. */
ASplineActor* ASplineActor::GetBestConnectionInDirection(FVector DesiredDir, UBOOL bUseLinksFrom)
{
	// Iterate over each option
	ASplineActor* BestOption = NULL;
	FLOAT BestDist = -BIG_NUMBER;

	const INT ConnectionNum = bUseLinksFrom ? LinksFrom.Num() : Connections.Num();

	for(INT i=0; i<ConnectionNum; i++)
	{
		ASplineActor* TestConn = bUseLinksFrom ? LinksFrom(i) : Connections(i).ConnectTo;
		
		// If non-NULL, and not disabled
		if(TestConn && !TestConn->bDisableDestination)
		{
			// If non-null, project vector from this node to next along given direction
			const FVector ConnDir = (TestConn->Location - Location).SafeNormal();
			const FLOAT Dist = ConnDir | DesiredDir;

			//GWorld->GetWorldInfo()->DrawDebugLine( TestConn->Location, Location, 255, 0, 0, TRUE );	
			//warnf( TEXT( "NumConnection: %d  Dist: %f"), Connections.Num(), Dist );
			// If more than current best, remember it
			if(Dist > BestDist)
			{
				BestDist = Dist;
				BestOption = TestConn;
			}
		}
	}

	// Return best. If there were no options this will be NULL.
	return BestOption;
}

/** Recursively add all spline nodes connected to this one */
static void GetAllConnectedWorker(ASplineActor* InSpline, TArray<ASplineActor*>& InAllConnected)
{
	InAllConnected.AddItem(InSpline);

	// Find all things we connect to
	TArray<ASplineActor*> ExploreSet = InSpline->LinksFrom;
	for(INT i=0; i<InSpline->Connections.Num(); i++)
	{
		if(InSpline->Connections(i).ConnectTo)
		{
			ExploreSet.AddItem(InSpline->Connections(i).ConnectTo);
		}
	}
	
	// Go to them if not already visited
	for(INT i=0; i<ExploreSet.Num(); i++)
	{
		if(!InAllConnected.ContainsItem(ExploreSet(i)))
		{
			GetAllConnectedWorker(ExploreSet(i), InAllConnected);
		}
	}
}

/** Find all SplineActors connected to (and including) this one */
void ASplineActor::GetAllConnectedSplineActors(TArray<class ASplineActor*>& OutSet)
{
	OutSet.Empty();
	
	GetAllConnectedWorker(this, OutSet);
}


//////////
// PATH FINDING

static ASplineActor* PopBestNode(ASplineActor*& OpenList)
{
	ASplineActor* Best = OpenList;
	OpenList = Best->nextOrdered;
	if(OpenList != NULL)
	{
		OpenList->prevOrdered = NULL;
	}


	// indicate this node is no longer on the open list
	Best->prevOrdered = NULL;
	Best->nextOrdered = NULL;
	return Best;
}

static UBOOL InsertSorted(ASplineActor* NodeForInsertion, ASplineActor*& OpenList)
{
	// if list is empty insert at the beginning
	if(OpenList == NULL)
	{
		OpenList = NodeForInsertion;
		NodeForInsertion->nextOrdered = NULL;
		NodeForInsertion->prevOrdered = NULL;
		return TRUE;
	}

	ASplineActor* CurrentNode = OpenList;
	INT LoopCounter = 0;
	for(;CurrentNode != NULL; CurrentNode = CurrentNode->nextOrdered)
	{
		check(LoopCounter++ <= 2048);
		
		if(NodeForInsertion->bestPathWeight <= CurrentNode->bestPathWeight)
		{
			checkSlow(NodeForInsertion != CurrentNode);
			NodeForInsertion->nextOrdered = CurrentNode;
			NodeForInsertion->prevOrdered = CurrentNode->prevOrdered;
			if(CurrentNode->prevOrdered != NULL)
			{
				CurrentNode->prevOrdered->nextOrdered = NodeForInsertion;
			}
			else
			{
				OpenList = NodeForInsertion;
			}
			CurrentNode->prevOrdered = NodeForInsertion;
			return TRUE;
		}

		if(CurrentNode->nextOrdered == NULL)
		{
			break;
		}
	}

	// if we got here, append to the end
	CurrentNode->nextOrdered = NodeForInsertion;
	NodeForInsertion->prevOrdered = CurrentNode;
	return TRUE;
}

struct FSplineEdge
{
	ASplineActor* Start;
	ASplineActor* End;
	INT Length;
	
	FSplineEdge(ASplineActor* InStart, ASplineActor* InEnd, INT InLength)
	{
		Start = InStart;
		End = InEnd;
		Length = InLength;
	}
	
	FVector GetDirection() const
	{
		return (End->Location - Start->Location).SafeNormal();
	}
	
	FLOAT AdjustedCostFor(const FVector& StartToGoalDir, ASplineActor* Goal, INT Cost)
	{
		// scale cost to make paths away from the goal direction more expensive
		const FLOAT DotToGoal = Clamp<FLOAT>(1.f - (GetDirection() | StartToGoalDir), 0.1f, 2.f);

		// Additional cost based on the distance to goal, and based on the distance travelled
		Cost += appTrunc(((End->Location - Goal->Location).Size() * DotToGoal) + (Length * DotToGoal));

		return Cost;
	}	
};



static UBOOL AddToOpen(ASplineActor*& OpenList, ASplineActor* NodeToAdd, ASplineActor* GoalNode, INT EdgeCost, FSplineEdge& Edge)
{
	const FVector DirToGoal = (GoalNode->Location - NodeToAdd->Location).SafeNormal2D();
	ASplineActor* Predecessor = Edge.Start;
	NodeToAdd->visitedWeight = EdgeCost + Predecessor->visitedWeight;
	NodeToAdd->previousPath = Predecessor;
	NodeToAdd->bestPathWeight = appTrunc( Edge.AdjustedCostFor( DirToGoal, GoalNode, NodeToAdd->visitedWeight ));
	if(	NodeToAdd->bestPathWeight <= 0 )
	{
		debugf( TEXT("Path Warning!!! Got neg/zero adjusted cost to %s"), *Edge.End->GetName());
		NodeToAdd->bAlreadyVisited = TRUE;
		return TRUE;
	}

	return InsertSorted(NodeToAdd, OpenList);
}

static UBOOL AddNodeToOpen( ASplineActor*& OpenList, ASplineActor* NodeToAdd, INT EdgeCost, INT HeuristicCost, FSplineEdge& Edge)
{
	ASplineActor* Predecessor = Edge.Start;
	NodeToAdd->visitedWeight = EdgeCost + Predecessor->visitedWeight;
	check(Predecessor->previousPath != NodeToAdd);

	NodeToAdd->previousPath	 = Predecessor;

	//debug
	//DEBUGPATHLOG(FString::Printf(TEXT("Set %s prev path = %s"), *NodeToAdd->GetName(), *Predecessor->GetName()));
	//DEBUGREGISTERCOST( NodeToAdd, TEXT("Previous"), Predecessor->visitedWeight );

	NodeToAdd->bestPathWeight = NodeToAdd->visitedWeight + HeuristicCost;
	return InsertSorted( NodeToAdd, OpenList );
}

// walks the A* previousPath list and adds the path to the routecache (reversing it so the order is start->goal)
static void SaveResultingPath(TArray<ASplineActor*>& OutPath, ASplineActor* Start, ASplineActor* Goal)
{
	// fill in the route cache with the new path
	ASplineActor *CurrentNav = Goal;
	INT LoopCounter = 0;
	while (CurrentNav != Start && CurrentNav != NULL)
	{
		check(LoopCounter++ <= 2048);
		OutPath.InsertItem(CurrentNav, 0);
		check(CurrentNav->previousPath == NULL || CurrentNav->previousPath != CurrentNav->previousPath->previousPath);
		CurrentNav = CurrentNav->previousPath;
	}
	
	OutPath.InsertItem(Start, 0);
}

static void RemoveNodeFromOpen(ASplineActor* NodeToRemove, ASplineActor*& OpenList)
{
	if(NodeToRemove->prevOrdered != NULL)
	{
		NodeToRemove->prevOrdered->nextOrdered = NodeToRemove->nextOrdered;
		check(NodeToRemove->nextOrdered != NodeToRemove);
	}
	else // it's the top of the stack, so pop it off
	{
		OpenList = NodeToRemove->nextOrdered;
	}
	if(NodeToRemove->nextOrdered != NULL)
	{
		NodeToRemove->nextOrdered->prevOrdered = NodeToRemove->prevOrdered;
		NodeToRemove->nextOrdered = NULL;
	}						

	NodeToRemove->prevOrdered = NULL;
}

static UBOOL AStarBestPathTo(ASplineActor *Start, ASplineActor *Goal, TArray<ASplineActor*>& OutPath)
{
	OutPath.Empty();

	if(Goal == NULL)
	{
		return FALSE;
	}

	if(Start == Goal)
	{
		OutPath.InsertItem(Start, 0);
		return TRUE;
	}

	// add start node to the open list
	ASplineActor* OpenList = Start;
	Start->visitedWeight = 0;
	Start->bestPathWeight = 0;

	UBOOL bPathComplete = FALSE;

	// maximum number of nodes to be popped from the open list before we bail out
	const INT MaxPathVisits = 1024;
	INT NumVisits = 0;

	// ++ Begin A* Loop
	while(OpenList != NULL)
	{
		// pop best node from Open list
		ASplineActor* CurrentNode = PopBestNode(OpenList);

		// if the node we just pulled from the open list is the goal, we're done!
		if(CurrentNode == Goal)
		{
			bPathComplete = TRUE;
			break;
		}

		// cap Open list pops at MaxPathVisits (after we check for goal, because if we just popped the goal off we don't want to do this :) )
		if(	++NumVisits > MaxPathVisits )
		{
			debugf( TEXT("Spline Path Warning!!! Exceeded maximum path visits while trying to path from %s to %s, Returning best guess."), *Start->GetName(), *Goal->GetName() );
			Goal->previousPath = CurrentNode;
			bPathComplete = TRUE;
			break;
		}

		// for each edge leaving CurrentNode
		for (INT PathIdx = 0; PathIdx < CurrentNode->Connections.Num(); PathIdx++)
		{
			ASplineActor* EdgeEnd = CurrentNode->Connections(PathIdx).ConnectTo;
			USplineComponent* EdgeComp = CurrentNode->Connections(PathIdx).SplineComponent;
			
			if (EdgeEnd == NULL || EdgeComp == NULL || EdgeEnd->ActorIsPendingKill())
			{
				continue;
			}
			
			// Do not use disabled spline points for pathfinding
			if( EdgeEnd->bDisableDestination == TRUE )
			{
				continue;
			}

			FSplineEdge Edge(CurrentNode, EdgeEnd, appCeil(EdgeComp->GetSplineLength()));

			// Cache the cost of this node
			INT InitialCost = 1; 


			const UBOOL bIsOnClosed = EdgeEnd->bAlreadyVisited;
			const UBOOL bIsOnOpen = EdgeEnd->prevOrdered != NULL || EdgeEnd->nextOrdered != NULL || OpenList == EdgeEnd;

			if(bIsOnClosed == TRUE || bIsOnOpen == TRUE)
			{
				// as long as the costs already in the list is as good or better, throw this sucker out
				if(EdgeEnd->visitedWeight <= InitialCost + CurrentNode->visitedWeight)
				{
					continue; 
				}
				else // otherwise the incoming value is better, so pull it out of the lists
				{
					if(bIsOnClosed == TRUE)
					{
						EdgeEnd->bAlreadyVisited = FALSE;
					}
					if(bIsOnOpen == TRUE)
					{
						RemoveNodeFromOpen(EdgeEnd,OpenList);	
					}
				}
			}

			// add the new node to the open list
			if( AddToOpen(OpenList, EdgeEnd, Goal, InitialCost, Edge) == FALSE )
			{
				break;
			}
		}

		// mark the node we just explored from as closed (e.g. add to closed list)
		CurrentNode->bAlreadyVisited = TRUE;
	}
	// -- End A* loop

	if( bPathComplete == TRUE )
	{
		SaveResultingPath(OutPath, Start, Goal);
	}
	else
	{
		// removing this, since the function returns a bool, and sometimes it is safe for no path to be found (e.g. elevators traversing spline paths in reverse)
		//debugf(TEXT("SPLINE PATH ERROR!!!! No path found from %s for %s"), *Goal->GetName(), *Start->GetName());
	}

	return bPathComplete;
}

/** Find a path through network from this point to Goal, and put results into OutRoute. */
UBOOL ASplineActor::FindSplinePathTo(class ASplineActor* Goal, TArray<class ASplineActor*>& OutRoute)
{
	// Clear transient pathfinding vars
	// TEMP - need to find better way to do this!
	for( FActorIterator It; It; ++It )
	{
		ASplineActor* Spline = Cast<ASplineActor>( *It );
		if( Spline && !Spline->IsTemplate() )
		{		
			Spline->ClearForSplinePathFinding();
		}
	}

	// Then do path find
	return AStarBestPathTo(this, Goal, OutRoute);
}

/** Clear the transient properties used for path finding */
void ASplineActor::ClearForSplinePathFinding()
{
	visitedWeight	= UCONST_INFINITE_PATH_COST;
	nextOrdered		= NULL;
	prevOrdered		= NULL;
	previousPath	= NULL;
	bAlreadyVisited = FALSE;
}