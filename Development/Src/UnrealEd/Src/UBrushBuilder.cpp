/*=============================================================================
	UBrushBuilder.cpp: UnrealEd brush builder.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "ScopedTransaction.h"
#include "BSPOps.h"

/*-----------------------------------------------------------------------------
	UBrushBuilder.
-----------------------------------------------------------------------------*/

void UBrushBuilder::BeginBrush(UBOOL InMergeCoplanars, FName InLayer)
{
	Layer = InLayer;
	MergeCoplanars = InMergeCoplanars;
	Vertices.Empty();
	Polys.Empty();
}
UBOOL UBrushBuilder::EndBrush()
{
	//!!validate
	ABrush* BuilderBrush = GWorld->GetBrush();

	// Ensure the builder brush is unhidden.
	BuilderBrush->bHidden = FALSE;
	BuilderBrush->bHiddenEdLayer = FALSE;

	AActor* Actor = GEditor->GetSelectedActors()->GetTop<AActor>();
	FVector Location = Actor ? Actor->Location : BuilderBrush->Location;

	UModel* Brush = BuilderBrush->Brush;
	if( Brush )
	{
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("BrushSet")) );
			Brush->Modify();
			BuilderBrush->Modify();
			BuilderBrush->Layer = Layer;
			FRotator Temp(0.0f,0.0f,0.0f);
			GEditor->Constraints.Snap( Location, FVector(0,0,0), Temp );
			BuilderBrush->Location = Location;
			BuilderBrush->PrePivot = FVector(0,0,0);
			{
				Brush->Polys->Element.Empty();
				for( TArray<FBuilderPoly>::TIterator It(Polys); It; ++It )
				{
					if( It->Direction<0 )
					{
						for( INT i=0; i<It->VertexIndices.Num()/2; i++ )
						{
							Exchange( It->VertexIndices(i), It->VertexIndices.Last(i) );
						}
					}

					FPoly Poly;
					Poly.Init();
					Poly.ItemName = It->ItemName;
					Poly.Base = Vertices(It->VertexIndices(0));
					Poly.PolyFlags = It->PolyFlags;
					for( INT j=0; j<It->VertexIndices.Num(); j++ )
					{
						new(Poly.Vertices) FVector(Vertices(It->VertexIndices(j)));
					}
					if( Poly.Finalize( BuilderBrush, 1 ) == 0 )
					{
						new(Brush->Polys->Element)FPoly(Poly);
					}
				}
			}
			if( MergeCoplanars )
			{
				GEditor->bspMergeCoplanars( Brush, 0, 1 );
				FBSPOps::bspValidateBrush( Brush, 1, 1 );
			}
			Brush->Linked = 1;
			FBSPOps::bspValidateBrush( Brush, 0, 1 );
			Brush->BuildBound();
		}

		GEditor->RedrawLevelEditingViewports();
		GEditor->SetPivot( BuilderBrush->Location, FALSE, TRUE );
		BuilderBrush->ClearComponents();
		BuilderBrush->ConditionalUpdateComponents();
	}
	return TRUE;
}
INT UBrushBuilder::GetVertexCount()
{
	return Vertices.Num();
}
FVector UBrushBuilder::GetVertex(INT i)
{
	return Vertices.IsValidIndex(i) ? Vertices(i) : FVector(0,0,0);
}
INT UBrushBuilder::GetPolyCount()
{
	return Polys.Num();
}
UBOOL UBrushBuilder::BadParameters(const FString& Msg)
{
	warnf(NAME_UserPrompt,Msg!=TEXT("") ? *Msg : TEXT("Bad parameters in brush builder"));
	return 0;
}
INT UBrushBuilder::Vertexv(FVector V)
{
	INT Result = Vertices.Num();
	new(Vertices)FVector(V);

	return Result;
}
INT UBrushBuilder::Vertex3f(FLOAT X, FLOAT Y, FLOAT Z)
{
	INT Result = Vertices.Num();
	new(Vertices)FVector(X,Y,Z);
	return Result;
}
void UBrushBuilder::Poly3i(INT Direction, INT i, INT j, INT k, FName ItemName, UBOOL bIsTwoSidedNonSolid )
{
	new(Polys)FBuilderPoly;
	Polys.Last().Direction=Direction;
	Polys.Last().ItemName=ItemName;
	new(Polys.Last().VertexIndices)INT(i);
	new(Polys.Last().VertexIndices)INT(j);
	new(Polys.Last().VertexIndices)INT(k);
	Polys.Last().PolyFlags = PF_DefaultFlags | (bIsTwoSidedNonSolid ? (PF_TwoSided|PF_NotSolid) : 0);
}
void UBrushBuilder::Poly4i(INT Direction, INT i, INT j, INT k, INT l, FName ItemName, UBOOL bIsTwoSidedNonSolid )
{
	new(Polys)FBuilderPoly;
	Polys.Last().Direction=Direction;
	Polys.Last().ItemName=ItemName;
	new(Polys.Last().VertexIndices)INT(i);
	new(Polys.Last().VertexIndices)INT(j);
	new(Polys.Last().VertexIndices)INT(k);
	new(Polys.Last().VertexIndices)INT(l);
	Polys.Last().PolyFlags = PF_DefaultFlags | (bIsTwoSidedNonSolid ? (PF_TwoSided|PF_NotSolid) : 0);
}
void UBrushBuilder::PolyBegin(INT Direction, FName ItemName)
{
	new(Polys)FBuilderPoly;
	Polys.Last().ItemName=ItemName;
	Polys.Last().Direction = Direction;
	Polys.Last().PolyFlags = PF_DefaultFlags;
}
void UBrushBuilder::Polyi(INT i)
{
	new(Polys.Last().VertexIndices)INT(i);
}
void UBrushBuilder::PolyEnd()
{
}
IMPLEMENT_CLASS(UBrushBuilder)

IMPLEMENT_CLASS(UCubeBuilder)
