//=============================================================================
// TerrainBuilder: Builds a 3D cube brush, with a tessellated bottom.
//=============================================================================
class TerrainBuilder
	extends BrushBuilder;

var() float X, Y, Z;
var() int XSegments, YSegments;		// How many breaks to have in each direction
var() name GroupName;

function BuildTerrain( int Direction, float dx, float dy, float dz, int YSeg, int XSeg )
{
	local int n,nbottom,ntop,i,j,k,x,y;
	local float YStep, XStep;

	//
	// TOP
	//

	n = GetVertexCount();

	// Create vertices
	for( i=-1; i<2; i+=2 )
		for( j=-1; j<2; j+=2 )
			for( k=-1; k<2; k+=2 )
				Vertex3f( i*dx/2, j*dy/2, k*dz/2 );

	// Create the top
	Poly4i(Direction,n+3,n+1,n+5,n+7, 'sky');

	//
	// BOTTOM
	//

	nbottom = GetVertexCount();

	// Create vertices
	YStep = dx / YSeg;
	XStep = dy / XSeg;

	for( x = 0 ; x < YSeg + 1 ; x++ )
		for( y = 0 ; y < XSeg + 1 ; y++ )
			Vertex3f( (YStep * x) - dx/2, (XStep * y) - dy/2, -(dz/2) );

	ntop = GetVertexCount();

	for( x = 0 ; x < YSeg + 1 ; x++ )
		for( y = 0 ; y < XSeg + 1 ; y++ )
			Vertex3f( (YStep * x) - dx/2, (XStep * y) - dy/2, dz/2 );

	// Create the bottom as a mesh of triangles
	for( x = 0 ; x < YSeg ; x++ )
		for( y = 0 ; y < XSeg ; y++ )
		{
			Poly3i(-Direction,
				(nbottom+y)		+ ((XSeg+1) * x),
				(nbottom+y)		+ ((XSeg+1) * (x+1)),
				((nbottom+1)+y)	+ ((XSeg+1) * (x+1)),
				'ground');
			Poly3i(-Direction,
				(nbottom+y)		+ ((XSeg+1) * x),
				((nbottom+1)+y) + ((XSeg+1) * (x+1)),
				((nbottom+1)+y) + ((XSeg+1) * x),
				'ground');
		}

	//
	// SIDES
	//
	// The bottom poly of each side is basically a triangle fan.
	//
	for( x = 0 ; x < YSeg ; x++ )
	{
		Poly4i(-Direction,
			nbottom + XSeg + ((XSeg+1) * x),
			nbottom + XSeg + ((XSeg+1) * (x + 1)),
			ntop + XSeg + ((XSeg+1) * (x + 1)),
			ntop + XSeg + ((XSeg+1) * x),
			'sky' );
		Poly4i(-Direction,
			nbottom + ((XSeg+1) * (x + 1)),
			nbottom + ((XSeg+1) * x),
			ntop + ((XSeg+1) * x),
			ntop + ((XSeg+1) * (x + 1)),
			'sky' );
	}
	for( y = 0 ; y < XSeg ; y++ )
	{
		Poly4i(-Direction,
			nbottom + y,
			nbottom + (y + 1),
			ntop + (y + 1),
			ntop + y,
			'sky' );
		Poly4i(-Direction,
			nbottom + ((XSeg+1) * YSeg) + (y + 1),
			nbottom + ((XSeg+1) * YSeg) + y,
			ntop + ((XSeg+1) * YSeg) + y,
			ntop + ((XSeg+1) * YSeg) + (y + 1),
			'sky' );
	}
}

event bool Build()
{
	if( Z<=0 || Y<=0 || X<=0 || YSegments<=0 || XSegments<=0 )
		return BadParameters();

	BeginBrush( false, GroupName );
	BuildTerrain( +1, X, Y, Z, YSegments, XSegments );
	return EndBrush();
}

defaultproperties
{
	Z=256
	Y=256
	X=512
	YSegments=4
	XSegments=2
	GroupName=Terrain
	BitmapFilename="Btn_BSPTerrain"
	ToolTip="BSP Based Terrain"
}
