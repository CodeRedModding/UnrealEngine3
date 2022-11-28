class Texture extends Object
	native
	noexport
	abstract;

// This needs to be mirrored in UnTex.h, Texture.uc and UnEdFact.cpp.
enum TextureCompressionSettings
{
	TC_Default,
	TC_Normalmap,
	TC_Displacementmap,
	TC_NormalmapAlpha,
	TC_Grayscale,
	TC_HighDynamicRange
};


//@warning: make sure to update UTexture::PostEditChange if you add an option that might require recompression.

// Texture settings.

var()	bool							SRGB;
var		bool							RGBE;
				
var()	float							UnpackMin[4],
										UnpackMax[4];

var native const LazyArray_Mirror		SourceArt;

var()	bool							CompressionNoAlpha;
var		bool							CompressionNone;
var		bool							CompressionNoMipmaps;
var()	bool							CompressionFullDynamicRange;
/** Allows artists to specify that a texture should never have its miplevels dropped which is useful for e.g. HUD and menu textures */
var()	bool							NeverStream;

var()	TextureCompressionSettings		CompressionSettings;


defaultproperties
{
	SRGB=True
	UnpackMax(0)=1.0
	UnpackMax(1)=1.0
	UnpackMax(2)=1.0
	UnpackMax(3)=1.0
}