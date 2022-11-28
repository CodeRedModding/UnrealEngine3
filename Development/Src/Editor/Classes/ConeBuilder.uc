//=============================================================================
// ConeBuilder: Builds a 3D cone brush, compatible with cylinder of same size.
//=============================================================================
class ConeBuilder
	extends BrushBuilder;

var() float Z, CapZ, OuterRadius, InnerRadius;
var() int Sides;
var() name GroupName;
var() bool AlignToSide, Hollow;

function BuildCone( int Direction, bool AlignToSide, int Sides, float Z, float Radius, name Item )
{
	local int n,i,Ofs;
	n = GetVertexCount();
	if( AlignToSide )
	{
		Radius /= cos(pi/Sides);
		Ofs = 1;
	}

	// Vertices.
	for( i=0; i<Sides; i++ )
		Vertex3f( Radius*sin((2*i+Ofs)*pi/Sides), Radius*cos((2*i+Ofs)*pi/Sides), 0 );
	Vertex3f( 0, 0, Z );

	// Polys.
	for( i=0; i<Sides; i++ )
		Poly3i( Direction, n+i, n+Sides, n+((i+1)%Sides), Item );
}

function bool Build()
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
	ToolTip="Cone"
}
