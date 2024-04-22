/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// TetrahedronBuilder: Builds an octahedron (not tetrahedron) - experimental.
//=============================================================================
class TetrahedronBuilder
	extends BrushBuilder;

var() float Radius		<ClampMin=0.000001>;
var() int SphereExtrapolation	<ClampMin=1 | ClampMax=5>;
var() name GroupName;

function Extrapolate( int a, int b, int c, int Count, float InRadius )
{
	local int ab,bc,ca;
	if( Count>1 )
	{
		ab=Vertexv( InRadius*Normal(GetVertex(a)+GetVertex(b)) );
		bc=Vertexv( InRadius*Normal(GetVertex(b)+GetVertex(c)) );
		ca=Vertexv( InRadius*Normal(GetVertex(c)+GetVertex(a)) );
		Extrapolate(a,ab,ca,Count-1,InRadius);
		Extrapolate(b,bc,ab,Count-1,InRadius);
		Extrapolate(c,ca,bc,Count-1,InRadius);
		Extrapolate(ab,bc,ca,Count-1,InRadius);
		//wastes shared vertices
	}
	else Poly3i(+1,a,b,c);
}

function BuildTetrahedron( float R, int InSphereExtrapolation )
{
	vertex3f( R,0,0);
	vertex3f(-R,0,0);
	vertex3f(0, R,0);
	vertex3f(0,-R,0);
	vertex3f(0,0, R);
	vertex3f(0,0,-R);

	Extrapolate(2,1,4,InSphereExtrapolation,Radius);
	Extrapolate(1,3,4,InSphereExtrapolation,Radius);
	Extrapolate(3,0,4,InSphereExtrapolation,Radius);
	Extrapolate(0,2,4,InSphereExtrapolation,Radius);
	Extrapolate(1,2,5,InSphereExtrapolation,Radius);
	Extrapolate(3,1,5,InSphereExtrapolation,Radius);
	Extrapolate(0,3,5,InSphereExtrapolation,Radius);
	Extrapolate(2,0,5,InSphereExtrapolation,Radius);
}

event bool Build()
{
	if( Radius<=0 || SphereExtrapolation<=0 )
		return BadParameters();
	if( SphereExtrapolation > 5 )
		return BadParameters( "Setting 'SphereExtrapolation' to more than 5 is invalid.  The resulting brush will have too many polygons to be useful.");
	
	BeginBrush( false, GroupName );
	BuildTetrahedron( Radius, SphereExtrapolation );
	return EndBrush();
}

defaultproperties
{
	Radius=256
	SphereExtrapolation=1
	GroupName=Tetrahedron
	BitmapFilename="Btn_Sphere"
	ToolTip="BrushBuilderName_Tetrahedron"
}
