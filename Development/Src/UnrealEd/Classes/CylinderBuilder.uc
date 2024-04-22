/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


//=============================================================================
// CylinderBuilder: Builds a 3D cylinder brush.
//=============================================================================
class CylinderBuilder
	extends BrushBuilder;

var() float Z		<ClampMin=0.000001>;
var() float OuterRadius	<ClampMin=0.000001>;
var() float InnerRadius;
var() int Sides		<ClampMin=3>;
var() name GroupName;
var() bool AlignToSide, Hollow;

function BuildCylinder( int Direction, bool InAlignToSide, int InSides, float InZ, float Radius )
{
	local int n,i,j,Ofs;
	n = GetVertexCount();
	if( InAlignToSide )
	{
		Radius /= cos(pi/InSides);
		Ofs = 1;
	}

	// Vertices.
	for( i=0; i<InSides; i++ )
		for( j=-1; j<2; j+=2 )
			Vertex3f( Radius*sin((2*i+Ofs)*pi/InSides), Radius*cos((2*i+Ofs)*pi/InSides), j*InZ/2 );

	// Polys.
	for( i=0; i<InSides; i++ )
		Poly4i( Direction, n+i*2, n+i*2+1, n+((i*2+3)%(2*InSides)), n+((i*2+2)%(2*InSides)), 'Wall' );
}

event bool Build()
{
	local int i,j;

	if( Sides<3 )
		return BadParameters();
	if( Z<=0 || OuterRadius<=0 )
		return BadParameters();
	if( Hollow && (InnerRadius<=0 || InnerRadius>=OuterRadius) )
		return BadParameters();

	BeginBrush( false, GroupName );
	BuildCylinder( +1, AlignToSide, Sides, Z, OuterRadius );
	if( Hollow )
	{
		BuildCylinder( -1, AlignToSide, Sides, Z, InnerRadius );
		for( j=-1; j<2; j+=2 )
			for( i=0; i<Sides; i++ )
				Poly4i( j, i*2+(1-j)/2, ((i+1)%Sides)*2+(1-j)/2, ((i+1)%Sides)*2+(1-j)/2+Sides*2, i*2+(1-j)/2+Sides*2, 'Cap' );
	}
	else
	{
		for( j=-1; j<2; j+=2 )
		{
			PolyBegin( j, 'Cap' );
			for( i=0; i<Sides; i++ )
				Polyi( i*2+(1-j)/2 );
			PolyEnd();
		}
	}
	return EndBrush();
}

defaultproperties
{
	Z=256
	OuterRadius=512
	InnerRadius=384
	Sides=8
	GroupName=Cylinder
	AlignToSide=true
	Hollow=false
	BitmapFilename="Btn_Cylinder"
	ToolTip="BrushBuilderName_Cylinder"
}
