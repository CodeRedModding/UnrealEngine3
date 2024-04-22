/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// VolumetricBuilder: Builds a volumetric brush (criss-crossed sheets).
//=============================================================================
class VolumetricBuilder
	extends BrushBuilder;

var() float Z		<ClampMin=0.000001>;
var() float Radius	<ClampMin=0.000001>;
var() int NumSheets	<ClampMin=2>;
var() name GroupName;

function BuildVolumetric( int Direction, int InNumSheets, float InZ, float InRadius )
{
	local int n,x,y;
	local rotator RotStep;
	local vector vtx, NewVtx;

	n = GetVertexCount();
	RotStep.Yaw = 65536.0f / (InNumSheets * 2);

	// Vertices.
	vtx.x = Radius;
	vtx.z = InZ / 2;
	for( x = 0 ; x < (InNumSheets * 2) ; x++ )
	{
		NewVtx = vtx >> (RotStep * x);
		Vertex3f( NewVtx.x, NewVtx.y, NewVtx.z );
		Vertex3f( NewVtx.x, NewVtx.y, NewVtx.z - InZ );
	}

	// Polys.
	for( x = 0 ; x < InNumSheets ; x++ )
	{
		y = (x*2) + 1;
		if( y >= (InNumSheets * 2) ) y -= (InNumSheets * 2);
		Poly4i( Direction, n+(x*2), n+y, n+y+(InNumSheets*2), n+(x*2)+(InNumSheets*2), 'Sheets', true);
	}
}

event bool Build()
{
	if( NumSheets<2 )
		return BadParameters();
	if( Z<=0 || Radius<=0 )
		return BadParameters();

	BeginBrush( true, GroupName );
	BuildVolumetric( +1, NumSheets, Z, Radius );
	return EndBrush();
}

defaultproperties
{
	Z=128
	Radius=64
	NumSheets=2
	GroupName=Volumetric
	BitmapFilename="Btn_Volumetric"
	ToolTip="BrushBuilderName_Volumetric"
}
