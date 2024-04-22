/*=============================================================================
	UnTexAlignTools.h: Tools for aligning textures on surfaces
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNTEXALIGNTOOLS_H__
#define __UNTEXALIGNTOOLS_H__

/** Alignment types. */
enum ETexAlign
{
	TEXALIGN_None			= -1,	// When passed to functions it means "use whatever the aligners default is"
	TEXALIGN_Default		= 0,	// No special alignment (just derive from UV vectors).
	TEXALIGN_Box			= 1,	// Aligns to best U and V axis based on polys normal
	TEXALIGN_Planar			= 2,	// Maps the poly to the axis it is closest to laying parallel to
	TEXALIGN_Fit			= 3,	// Fits texture to a polygon

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

	FBspSurf* Surf;
	INT Idx;
};

/**
 * Base class for all texture aligners.
 */
class UTexAligner : public UObject
{
	DECLARE_ABSTRACT_CLASS_INTRINSIC(UTexAligner,UObject,0,UnrealEd);
	
	UTexAligner();

	/**
	 * Perform any static initialization, such as adding UProperties to the class (for intrinsic classes), or initializing static variable values.
	 * It is not recommended to use this function to initialize member property values, since those values will be overwritten when the class's default
	 * object is re-initialized against its archetype.  Use InitializeInstrinsicPropertyValues instead.
	 */
	void StaticConstructor();

	/** The default alignment this aligner represents. */
	ETexAlign DefTexAlign;

	UEnum* TAxisEnum;
	BYTE TAxis;
	FLOAT UTile, VTile;

	/** Description for the editor to display. */
	FString Desc;

	void Align( ETexAlign InTexAlignType );
	void Align( ETexAlign InTexAlignType, UModel* InModel );
	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal );
};

/**
 * Aligns according to which axis the poly is most facing.
 */
class UTexAlignerPlanar : public UTexAligner
{
	DECLARE_CLASS_INTRINSIC(UTexAlignerPlanar,UTexAligner,0,UnrealEd)

	UTexAlignerPlanar();

	/**
	 * Perform any static initialization, such as adding UProperties to the class (for intrinsic classes), or initializing static variable values.
	 * It is not recommended to use this function to initialize member property values, since those values will be overwritten when the class's default
	 * object is re-initialized against its archetype.  Use InitializeInstrinsicPropertyValues instead.
	 */
	void StaticConstructor();

	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal );
};

/**
 * Aligns to a default setting.
 */
class UTexAlignerDefault : public UTexAligner
{
	DECLARE_CLASS_INTRINSIC(UTexAlignerDefault,UTexAligner,0,UnrealEd)

	UTexAlignerDefault();

	/**
	 * Perform any static initialization, such as adding UProperties to the class (for intrinsic classes), or initializing static variable values.
	 * It is not recommended to use this function to initialize member property values, since those values will be overwritten when the class's default
	 * object is re-initialized against its archetype.  Use InitializeInstrinsicPropertyValues instead.
	 */
	void StaticConstructor();

	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal );
};

/**
 * Aligns to the best U and V axis according to the polys normal.
 */
class UTexAlignerBox : public UTexAligner
{
	DECLARE_CLASS_INTRINSIC(UTexAlignerBox,UTexAligner,0,UnrealEd)

	UTexAlignerBox();

	/**
	 * Perform any static initialization, such as adding UProperties to the class (for intrinsic classes), or initializing static variable values.
	 * It is not recommended to use this function to initialize member property values, since those values will be overwritten when the class's default
	 * object is re-initialized against its archetype.  Use InitializeInstrinsicPropertyValues instead.
	 */
	void StaticConstructor();

	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal );
};


/**
 * Fits the texture to a face
 */
class UTexAlignerFit : public UTexAligner
{
	DECLARE_CLASS_INTRINSIC(UTexAlignerFit,UTexAligner,0,UnrealEd)

	UTexAlignerFit();

	/**
	 * Perform any static initialization, such as adding UProperties to the class (for intrinsic classes), or initializing static variable values.
	 * It is not recommended to use this function to initialize member property values, since those values will be overwritten when the class's default
	 * object is re-initialized against its archetype.  Use InitializeInstrinsicPropertyValues instead.
	 */
	void StaticConstructor();

	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal );
};


/**
 * A helper class to store the state of the various texture alignment tools.
 */
class FTexAlignTools
	: public FCallbackEventDevice
{
public:

	/** Constructor */
	FTexAlignTools();

	/** Destructor */
	virtual ~FTexAlignTools();

	/** A list of all available aligners. */
	TArray<UTexAligner*> Aligners;

	/**
	 * Creates the list of aligners.
	 */
	void Init();

	/**
	 * Returns the most appropriate texture aligner based on the type passed in.
	 */
	UTexAligner* GetAligner( ETexAlign InTexAlign );

	/**
	 * Routes the event to the appropriate handlers
	 *
	 * @param InType the event that was fired
	 */
	virtual void Send( ECallbackEventType InType );

};

extern FTexAlignTools GTexAlignTools;

#endif // __UNTEXALIGNTOOLS_H__
