/*=============================================================================
	UBrushBuilder.cpp: UnrealEd brush builder.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	UBrushBuilder.
-----------------------------------------------------------------------------*/

void UBrushBuilder::execBeginBrush( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL_OPTX(Merge,0);
	P_GET_NAME_OPTX(GroupName,NAME_None);
	P_FINISH;
	Group = GroupName;
	MergeCoplanars = Merge;
	Vertices.Empty();
	Polys.Empty();
}
void UBrushBuilder::execEndBrush( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	//!!validate
	UModel* Brush = GEditor->Level ? GEditor->Level->Brush()->Brush : NULL;
	if( Brush )
	{
		GEditor->Trans->Begin( TEXT("Brush Set") );
		Brush->Modify();
		GEditor->Level->Brush()->Modify();
		GEditor->Level->Brush()->Group = Group;
		FRotator Temp(0.0f,0.0f,0.0f);
		GEditor->Constraints.Snap( NULL, GEditor->Level->Brush()->Location, FVector(0,0,0), Temp );
		GEditor->Level->Brush()->Location -= GEditor->Level->Brush()->PrePivot;
		GEditor->Level->Brush()->PrePivot = FVector(0,0,0);
		{
			Brush->Polys->Element.Empty();
			for( TArray<FBuilderPoly>::TIterator It(Polys); It; ++It )
			{
				if( It->Direction<0 )
					for( INT i=0; i<It->VertexIndices.Num()/2; i++ )
						Exchange( It->VertexIndices(i), It->VertexIndices.Last(i) );
				for( ; ; )
				{
					INT iMax = Min<INT>(FPoly::MAX_VERTICES,It->VertexIndices.Num());
					FPoly Poly;
					Poly.Init();
					Poly.ItemName = It->ItemName;
					Poly.Base = Vertices(It->VertexIndices(0));
					Poly.PolyFlags = It->PolyFlags;
					for( INT j=0; j<iMax; j++ )
						Poly.Vertex[Poly.NumVertices++] = Vertices(It->VertexIndices(j));
					Poly.Finalize( NULL, 1 );
					new(Brush->Polys->Element)FPoly(Poly);
					if( iMax==It->VertexIndices.Num() )
						break;
					It->VertexIndices.Remove(1,iMax-2);
				}
			}
		}
		if( MergeCoplanars )
		{
			GEditor->bspMergeCoplanars( Brush, 0, 1 );
			GEditor->bspValidateBrush( Brush, 1, 1 );
		}
		Brush->Linked = 1;
		GEditor->bspValidateBrush( Brush, 0, 1 );
		Brush->BuildBound();
		GEditor->Trans->End();
		GEditor->RedrawLevel( GEditor->Level );
		GEditor->SetPivot( GEditor->Level->Brush()->Location, 0, 0, 1 );
		GEditor->Level->Brush()->ClearComponents();
		GEditor->Level->Brush()->UpdateComponents();
		//GEditor->NoteSelectionChange( GEditor->Level );
	}
	*(BITFIELD*)Result=1;
}
void UBrushBuilder::execGetVertexCount( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(INT*)Result = Vertices.Num();
}
void UBrushBuilder::execGetVertex( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(i);
	P_FINISH;
	*(FVector*)Result = Vertices.IsValidIndex(i) ? Vertices(i) : FVector(0,0,0);
}
void UBrushBuilder::execGetPolyCount( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(INT*)Result = Polys.Num();
}
void UBrushBuilder::execBadParameters( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Msg);
	P_FINISH;
	warnf(NAME_UserPrompt,Msg!=TEXT("") ? *Msg : TEXT("Bad parameters in brush builder"));
}
void UBrushBuilder::execVertexv( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT(FVector,V);
	P_FINISH;
	*(INT*)Result = Vertices.Num();
	new(Vertices)FVector(V);
}
void UBrushBuilder::execVertex3f( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(X);
	P_GET_FLOAT(Y);
	P_GET_FLOAT(Z);
	P_FINISH;
	*(INT*)Result = Vertices.Num();
	new(Vertices)FVector(X,Y,Z);
}
void UBrushBuilder::execPoly3i( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(Direction);
	P_GET_INT(i);
	P_GET_INT(j);
	P_GET_INT(k);
	P_GET_NAME_OPTX(ItemName,NAME_None);
	P_GET_INT_OPTX(PolyFlags,0);
	P_FINISH;
	new(Polys)FBuilderPoly;
	Polys.Last().Direction=Direction;
	Polys.Last().ItemName=ItemName;
	new(Polys.Last().VertexIndices)INT(i);
	new(Polys.Last().VertexIndices)INT(j);
	new(Polys.Last().VertexIndices)INT(k);
	Polys.Last().PolyFlags = PolyFlags;
}
void UBrushBuilder::execPoly4i( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(Direction);
	P_GET_INT(i);
	P_GET_INT(j);
	P_GET_INT(k);
	P_GET_INT(l);
	P_GET_NAME_OPTX(ItemName,NAME_None);
	P_GET_INT_OPTX(PolyFlags,0);
	P_FINISH;
	new(Polys)FBuilderPoly;
	Polys.Last().Direction=Direction;
	Polys.Last().ItemName=ItemName;
	new(Polys.Last().VertexIndices)INT(i);
	new(Polys.Last().VertexIndices)INT(j);
	new(Polys.Last().VertexIndices)INT(k);
	new(Polys.Last().VertexIndices)INT(l);
	Polys.Last().PolyFlags = PolyFlags;
}
void UBrushBuilder::execPolyBegin( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(Direction);
	P_GET_NAME_OPTX(ItemName,NAME_None);
	P_GET_INT_OPTX(PolyFlags,0);
	P_FINISH;
	new(Polys)FBuilderPoly;
	Polys.Last().ItemName=ItemName;
	Polys.Last().Direction = Direction;
	Polys.Last().PolyFlags = PolyFlags;
}
void UBrushBuilder::execPolyi( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(i);
	P_FINISH;
	new(Polys.Last().VertexIndices)INT(i);
}
void UBrushBuilder::execPolyEnd( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
}
IMPLEMENT_CLASS(UBrushBuilder)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
