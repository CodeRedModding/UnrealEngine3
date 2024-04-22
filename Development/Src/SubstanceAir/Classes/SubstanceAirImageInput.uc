//! @file SubstanceAirImageInput.uc
//! @copyright Allegorithmic. All rights reserved.
//!

class SubstanceAirImageInput extends Object
	native(ImageInput)
	hidecategories(Object);

// RGB layer compressed image data
var native const UntypedBulkData_Mirror	CompressedImageRGB{FByteBulkData};
// Alpha layer compressed image data
var native const UntypedBulkData_Mirror	CompressedImageA{FByteBulkData};

var native int CompRGB;
var native int CompA;

// The Output instance to get the Substance data from,
// copy so the original can be updated
var native array<pointer> Inputs{struct SubstanceAir::FImageInputInstance};

var native int SizeX;
var native int SizeY;

// the uncompressed image is a raw RGBA, RGB or G
var native int NumComponents;

// path to the resource used to construct this image input
var()	editconst editoronly string		SourceFilePath;

// Date/Time-stamp of the file from the last import
var()	editconst editoronly string		SourceFileTimestamp;

cpptext
{
public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void Serialize(FArchive& Ar);
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);
	FString GetDesc();

	virtual INT GetResourceSize();
}
