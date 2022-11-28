/*=============================================================================
	UnShape.cpp: Implementation functions for AShape objects

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "EditorPrivate.h"

IMPLEMENT_CLASS(AShape);

void AShape::Spawned()
{
	// When spawning a shape in the editor, set it up with an initial quad polygon.

	if( GIsEditor )
	{
		FPoly Poly;
		Poly.Init();
		Poly.Base = GEditorModeTools.SnappedLocation;

		Poly.Vertex[0] = Poly.Base + FVector( -64,-64,0 );
		Poly.Vertex[1] = Poly.Base + FVector( +64,-64,0 );
		Poly.Vertex[2] = Poly.Base + FVector( +64,+64,0 );
		Poly.Vertex[3] = Poly.Base + FVector( -64,+64,0 );

		Poly.NumVertices = 4;

		Poly.Finalize( this, 1 );

		Brush			= new( GEditor->GetLevel(), TEXT("Shape"), 0 )UModel( NULL, 1 );
		Brush->Polys	= new( GEditor->GetLevel(), NAME_None, 0 )UPolys;

		new(Brush->Polys->Element)FPoly(Poly);

		BrushComponent->Brush = Brush;
		SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );
		Brush->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );

		Brush->BuildBound();
		ClearComponents();
		UpdateComponents();
	}
}

FColor AShape::GetWireColor()
{
	return FColor(128,128,0);
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
