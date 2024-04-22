/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// SheetBuilder: Builds a simple sheet.
//=============================================================================
class SheetBuilder
	extends BrushBuilder;

var() int X		<ClampMin=1>;
var() int Y		<ClampMin=1>;
var() int XSegments	<ClampMin=1 | ClampMax=100 | UIMin=1 | UIMax=20>;
var() int YSegments	<ClampMin=1 | ClampMax=100 | UIMin=1 | UIMax=20>;
var() enum ESheetAxis
{
	AX_Horizontal,
	AX_XAxis,
	AX_YAxis,
} Axis;
var() name GroupName;

event bool Build()
{
	local int i, j, XStep, YStep, count;

	if( Y<=0 || X<=0 || XSegments<=0 || YSegments<=0 )
		return BadParameters();

	BeginBrush( false, GroupName );
	XStep = X/XSegments;
	YStep = Y/YSegments;

	count = 0;
	for( i = 0 ; i < XSegments ; i++ )
	{
		for( j = 0 ; j < YSegments ; j++ )
		{
			if( Axis==AX_Horizontal )
			{
				Vertex3f(  (i*XStep)-X/2, (j*YStep)-Y/2, 0 );
				Vertex3f(  (i*XStep)-X/2, ((j+1)*YStep)-Y/2, 0 );
				Vertex3f(  ((i+1)*XStep)-X/2, ((j+1)*YStep)-Y/2, 0 );
				Vertex3f(  ((i+1)*XStep)-X/2, (j*YStep)-Y/2, 0 );
			}
			else if( Axis==AX_XAxis )
			{
				Vertex3f(  0, (i*XStep)-X/2, (j*YStep)-Y/2 );
				Vertex3f(  0, (i*XStep)-X/2, ((j+1)*YStep)-Y/2 );
				Vertex3f(  0, ((i+1)*XStep)-X/2, ((j+1)*YStep)-Y/2 );
				Vertex3f(  0, ((i+1)*XStep)-X/2, (j*YStep)-Y/2 );
			}
			else
			{
				Vertex3f(  (i*XStep)-X/2, 0, (j*YStep)-Y/2 );
				Vertex3f(  (i*XStep)-X/2, 0, ((j+1)*YStep)-Y/2 );
				Vertex3f(  ((i+1)*XStep)-X/2, 0, ((j+1)*YStep)-Y/2 );
				Vertex3f(  ((i+1)*XStep)-X/2, 0, (j*YStep)-Y/2 );
			}

			Poly4i(+1,count,count+1,count+2,count+3,'Sheet',true);
			count = GetVertexCount();
		}
	}

	return EndBrush();
}

defaultproperties
{
	X=256
	Y=256
	XSegments=1
	YSegments=1
	Axis=AX_Horizontal
	GroupName=Sheet
	BitmapFilename="Btn_Sheet"
	ToolTip="BrushBuilderName_Sheet"
}
