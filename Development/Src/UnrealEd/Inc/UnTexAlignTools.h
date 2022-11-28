/*=============================================================================
	UnTexAlignTools.h: Tools for aligning textures on surfaces
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

// Alignment types
enum ETexAlign
{
	TEXALIGN_None			= -1,	// When passed to functions it means "use whatever the aligners default is"
	TEXALIGN_Default		= 0,	// No special alignment (just derive from UV vectors).
	TEXALIGN_Box			= 1,	// Aligns to best U and V axis based on polys normal
	TEXALIGN_Planar			= 2,	// Maps the poly to the axis it is closest to laying parallel to
	TEXALIGN_Face			= 3,	// Maps each poly individually

	// These ones are special versions of the above.
	TEXALIGN_PlanarAuto		= 100,
	TEXALIGN_PlanarWall		= 101,
	TEXALIGN_PlanarFloor	= 102,
};

class FBspSurfIdx
{
public:
	FBspSurfIdx()
	{}
	FBspSurfIdx( FBspSurf* InSurf, INT InIdx )
	{
		Surf = InSurf;
		Idx = InIdx;
	}
	~FBspSurfIdx()
	{}

	FBspSurf* Surf;
	INT Idx;
};

/*------------------------------------------------------------------------------
	UTexAligner
------------------------------------------------------------------------------*/
class UTexAligner : public UObject
{
	DECLARE_ABSTRACT_CLASS(UTexAligner,UObject,0,UnrealEd)

	UTexAligner();

	ETexAlign DefTexAlign;	// The default alignment this aligner represents
	UEnum* TAxisEnum;
	BYTE TAxis;
	FLOAT UTile, VTile;
	FString Desc;			// Description for the editor to display

	virtual void InitFields();

	void Align( ETexAlign InTexAlignType, UModel* InModel );
	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal );
};

/*------------------------------------------------------------------------------
	UTexAlignerPlanar
------------------------------------------------------------------------------*/
class UTexAlignerPlanar : public UTexAligner
{
	DECLARE_CLASS(UTexAlignerPlanar,UTexAligner,0,UnrealEd)

	UTexAlignerPlanar();

	virtual void InitFields();

	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal );
};

/*------------------------------------------------------------------------------
	UTexAlignerDefault
------------------------------------------------------------------------------*/
class UTexAlignerDefault : public UTexAligner
{
	DECLARE_CLASS(UTexAlignerDefault,UTexAligner,0,UnrealEd)

	UTexAlignerDefault();

	virtual void InitFields();

	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal );
};

/*------------------------------------------------------------------------------
	UTexAlignerBox
------------------------------------------------------------------------------*/
class UTexAlignerBox : public UTexAligner
{
	DECLARE_CLASS(UTexAlignerBox,UTexAligner,0,UnrealEd)

	UTexAlignerBox();

	virtual void InitFields();

	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal );
};

/*------------------------------------------------------------------------------
	UTexAlignerFace
------------------------------------------------------------------------------*/
class UTexAlignerFace : public UTexAligner
{
	DECLARE_CLASS(UTexAlignerFace,UTexAligner,0,UnrealEd)

	UTexAlignerFace();

	virtual void InitFields();

	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal );
};

/*------------------------------------------------------------------------------
	FTexAlignTools
------------------------------------------------------------------------------*/
class FTexAlignTools
{
public:

	TArray<UTexAligner*> Aligners;	// A list of all available aligners

	// Constructor.
	FTexAlignTools();
	virtual ~FTexAlignTools();

	void Init();
	UTexAligner* GetAligner( ETexAlign InTexAlign );
};

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

