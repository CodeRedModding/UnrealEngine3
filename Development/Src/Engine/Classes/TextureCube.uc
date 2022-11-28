class TextureCube extends Texture
	native
	noexport
	hidecategories(Object);

var native const noexport pointer	VfTable;
var native const int				ResourceIndex;
var native const int				Dynamic;
var native const int				SizeX;
var native const int				SizeY;
var native const byte				Format;
var native const int				SizeZ;
var native const int				NumMips;
var native const int				CurrentMips;

var() const noexport Texture2D		FacePosX,
									FaceNegX,
									FacePosY,
									FaceNegY,
									FacePosZ,
									FaceNegZ;
var native const int				Valid;