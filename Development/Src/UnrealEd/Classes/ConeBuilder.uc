//=============================================================================
// ConeBuilder: Builds a 3D cone brush, compatible with cylinder of same size.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class ConeBuilder
	extends BrushBuilder;

var() float Z		<ClampMin=0.000001>;
var() float CapZ;
var() float OuterRadius	<ClampMin=0.000001>;
var() float InnerRadius;
var() int Sides		<ClampMin=3>;
var() name GroupName;
var() bool AlignToSide, Hollow;

function BuildCone( int Direction, bool InAlignToSide, int InSides, float InZ, float Radius, name Item )
{
	local int n,i,Ofs;
	n = GetVertexCount();
	if( InAlignToSide )
	{
		Radius /= cos(pi/InSides);
		Ofs = 1;
	}

	// Vertices.
	for( i=0; i<InSides; i++ )
		Vertex3f( Radius*sin((2*i+Ofs)*pi/InSides), Radius*cos((2*i+Ofs)*pi/InSides), 0 );
	Vertex3f( 0, 0, InZ );

	// Polys.
	for( i=0; i<InSides; i++ )
		Poly3i( Direction, n+i, n+InSides, n+((i+1)%InSides), Item );
}

event bool Build()
{
	local int i;

	if( Sides<3 )
		return BadParameters();
	if( Z<=0 || OuterRadius<=0 )
		return BadParameters();
	if( Hollow && (InnerRadius<=0 || InnerRadius>=OuterRadius) )
		return BadParameters();
	if( Hollow && CapZ>Z )
		return BadParameters();
	if( Hollow && (CapZ==Z && InnerRadius==OuterRadius) )
		return BadParameters();

	BeginBrush( false, GroupName );
	BuildCone( +1, AlignToSide, Sides, Z, OuterRadius, 'Top' );
	if( Hollow )
	{
		BuildCone( -1, AlignToSide, Sides, CapZ, InnerRadius, 'Cap' );
		if( OuterRadius!=InnerRadius )
			for( i=0; i<Sides; i++ )
				Poly4i( 1, i, ((i+1)%Sides), Sides+1+((i+1)%Sides), Sides+1+i, 'Bottom' );
	}
	else
	{
		PolyBegin( 1, 'Bottom' );
		for( i=0; i<Sides; i++ )
			Polyi( i );
		PolyEnd();
	}
	return EndBrush();
}

defaultproperties
{
	Z=256
	CapZ=256
	OuterRadius=512
	InnerRadius=384
	Sides=8
	GroupName=Cone
	AlignToSide=true
	Hollow=false
	BitmapFilename="Btn_Cone"
	ToolTip="BrushBuilderName_Cone"
}
