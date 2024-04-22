//! @file SubstanceAirImageInput.cpp
//! @brief Implementation of the USubstanceAirImageInput class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirInput.h"
#include "SubstanceAirImageInputClasses.h"

IMPLEMENT_CLASS( USubstanceAirImageInput )


void USubstanceAirImageInput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void USubstanceAirImageInput::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	CompressedImageRGB.Serialize(Ar, this);
	CompressedImageA.Serialize(Ar, this);

	// image inputs can be used multiple times
	CompressedImageRGB.ClearBulkDataFlags(BULKDATA_SingleUse); 
	CompressedImageA.ClearBulkDataFlags(BULKDATA_SingleUse);
	Ar << SizeX;
	Ar << SizeY;
	Ar << NumComponents;
	Ar << SourceFilePath;
	Ar << SourceFileTimestamp;

	Ar << CompRGB;
	Ar << CompA;
}


void USubstanceAirImageInput::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData);
}


FString USubstanceAirImageInput::GetDesc()
{
	return FString::Printf( TEXT("%dx%d (%d kB)"), SizeX, SizeY, GetResourceSize()/1024);
}


INT	USubstanceAirImageInput::GetResourceSize()
{
	return CompressedImageRGB.GetBulkDataSize() + CompressedImageA.GetBulkDataSize();
}
