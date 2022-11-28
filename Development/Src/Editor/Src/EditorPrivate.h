/*=============================================================================
	EditorPrivate.h: Unreal editor public header file.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INCL_EDITOR_PRIVATE_H_
#define _INCL_EDITOR_PRIVATE_H_

/*-----------------------------------------------------------------------------
	Advance editor private definitions.
-----------------------------------------------------------------------------*/

//
// Things to set in mapSetBrush.
//
enum EMapSetBrushFlags				
{
	MSB_BrushColor	= 1,			// Set brush color.
	MSB_Group		= 2,			// Set group.
	MSB_PolyFlags	= 4,			// Set poly flags.
	MSB_CSGOper		= 8,			// Set CSG operation.
};

//
// Possible positions of a child Bsp node relative to its parent (for BspAddToNode).
//
enum ENodePlace 
{
	NODE_Back		= 0, // Node is in back of parent              -> Bsp[iParent].iBack.
	NODE_Front		= 1, // Node is in front of parent             -> Bsp[iParent].iFront.
	NODE_Plane		= 2, // Node is coplanar with parent           -> Bsp[iParent].iPlane.
	NODE_Root		= 3, // Node is the Bsp root and has no parent -> Bsp[0].
};


// Byte describing effects for a mesh triangle.
enum EJSMeshTriType
{
	// Triangle types. Mutually exclusive.
	MTT_Normal				= 0,	// Normal one-sided.
	MTT_NormalTwoSided      = 1,    // Normal but two-sided.
	MTT_Translucent			= 2,	// Translucent two-sided.
	MTT_Masked				= 3,	// Masked two-sided.
	MTT_Modulate			= 4,	// Modulation blended two-sided.
	MTT_Placeholder			= 8,	// Placeholder triangle for positioning weapon. Invisible.
	// Bit flags. 
	MTT_Unlit				= 16,	// Full brightness, no lighting.
	MTT_Flat				= 32,	// Flat surface, don't do bMeshCurvy thing.
	MTT_Environment			= 64,	// Environment mapped.
	MTT_NoSmooth			= 128,	// No bilinear filtering on this poly's texture.
};

/*-----------------------------------------------------------------------------
	Editor public includes.
-----------------------------------------------------------------------------*/

#include "Editor.h"

/*-----------------------------------------------------------------------------
	Editor private includes.
-----------------------------------------------------------------------------*/

#include "UnEdTran.h"
#include "UnTopics.h"

extern class FGlobalTopicTable GTopics;
extern class FEditorModeTools GEditorModeTools;
extern struct FEditorLevelViewportClient* GCurrentLevelEditingViewportClient;

/*-----------------------------------------------------------------------------
	Factories.
-----------------------------------------------------------------------------*/

class ULevelFactoryNew : public UFactory
{
	DECLARE_CLASS(ULevelFactoryNew,UFactory,0,Editor)
	FStringNoInit LevelTitle;
	FStringNoInit Author;
	ULevelFactoryNew();
	void StaticConstructor();
	void Serialize( FArchive& Ar );
	UObject* FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn );
};

class UClassFactoryNew : public UFactory
{
	DECLARE_CLASS(UClassFactoryNew,UFactory,CLASS_CollapseCategories,Editor)
	UClass* Superclass;
	UClassFactoryNew();
	void StaticConstructor();
	void Serialize( FArchive& Ar );
	UObject* FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn );
};

class UTextureCubeFactoryNew : public UFactory
{
	DECLARE_CLASS(UTextureCubeFactoryNew,UFactory,CLASS_CollapseCategories,Editor)
	void StaticConstructor();
	UObject* FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn );
};

class UMaterialInstanceConstantFactoryNew : public UFactory
{
	DECLARE_CLASS(UMaterialInstanceConstantFactoryNew,UFactory,CLASS_CollapseCategories,Editor);
public:

	void StaticConstructor();
	UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn);
};

class UMaterialFactoryNew : public UFactory
{
	DECLARE_CLASS(UMaterialFactoryNew,UFactory,CLASS_CollapseCategories,Editor);
public:

	void StaticConstructor();
	UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn);
};

class UTerrainMaterialFactoryNew : public UFactory
{
	DECLARE_CLASS(UTerrainMaterialFactoryNew,UFactory,CLASS_CollapseCategories,Editor);
public:

	void StaticConstructor();
	UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn);
};

class UTerrainLayerSetupFactoryNew : public UFactory
{
	DECLARE_CLASS(UTerrainLayerSetupFactoryNew,UFactory,CLASS_CollapseCategories,Editor);
public:

	void StaticConstructor();
	UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn);
};

class UClassFactoryUC : public UFactory
{
	DECLARE_CLASS(UClassFactoryUC,UFactory,0,Editor)
	UClassFactoryUC();
	void StaticConstructor();
	UObject* FactoryCreateText( ULevel* InLevel, UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn );
};

class ULevelFactory : public UFactory
{
	DECLARE_CLASS(ULevelFactory,UFactory,0,Editor)
	ULevelFactory();
	void StaticConstructor();
	UObject* FactoryCreateText( ULevel* InLevel, UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn );
};

class UPolysFactory : public UFactory
{
	DECLARE_CLASS(UPolysFactory,UFactory,0,Editor)
	UPolysFactory();
	void StaticConstructor();
	UObject* FactoryCreateText( ULevel* InLevel, UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn );
};

class UModelFactory : public UFactory
{
	DECLARE_CLASS(UModelFactory,UFactory,0,Editor)
	UModelFactory();
	void StaticConstructor();
	UObject* FactoryCreateText( ULevel* InLevel, UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn );
};

class UStaticMeshFactory : public UFactory
{
	DECLARE_CLASS(UStaticMeshFactory,UFactory,0,Editor)
	INT	Pitch,
		Roll,
		Yaw;
	UStaticMeshFactory();
	void StaticConstructor();
	UObject* FactoryCreateText( ULevel* InLevel, UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn );
};

class USkeletalMeshFactory : public UFactory
{
	DECLARE_CLASS(USkeletalMeshFactory,UFactory,0,Editor)	
    UBOOL bAssumeMayaCoordinates;
	USkeletalMeshFactory();
	void StaticConstructor();	
	UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn );
};

class USoundFactory : public UFactory
{
	DECLARE_CLASS(USoundFactory,UFactory,0,Editor)
	USoundFactory();
	void StaticConstructor();
	UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn );
};

class USoundCueFactoryNew : public UFactory
{
	DECLARE_CLASS(USoundCueFactoryNew,UFactory,0,Editor)
	void StaticConstructor();
	UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn);
};

class UParticleSystemFactoryNew : public UFactory
{
	DECLARE_CLASS(UParticleSystemFactoryNew,UFactory,0,Editor)
	void StaticConstructor();
	UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn);
};

class UAnimSetFactoryNew : public UFactory
{
	DECLARE_CLASS(UAnimSetFactoryNew,UFactory,0,Editor)
	void StaticConstructor();
	UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn);
};

class UAnimTreeFactoryNew : public UFactory
{
	DECLARE_CLASS(UAnimTreeFactoryNew,UFactory,0,Editor)
	void StaticConstructor();
	UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn);
};

class UPrefabFactory : public UFactory
{
	DECLARE_CLASS(UPrefabFactory,UFactory,0,Editor)
	UPrefabFactory();
	void StaticConstructor();
	UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn );
};

class UTextureMovieFactory : public UFactory
{
	DECLARE_CLASS(UTextureMovieFactory,UFactory,CLASS_CollapseCategories,Editor)
	UTextureMovieFactory();
	void StaticConstructor();
	UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn );
};

class UTextureFactory : public UFactory
{
	DECLARE_CLASS(UTextureFactory,UFactory,CLASS_CollapseCategories,Editor)
	UBOOL						NoCompression,
								NoAlpha,
								bCreateMaterial;
	BYTE						CompressionSettings;
	UTextureFactory();
	void StaticConstructor();
	UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn );
};

class UVolumeTextureFactory : public UFactory
{
	DECLARE_CLASS(UVolumeTextureFactory,UFactory,CLASS_CollapseCategories,Editor)
	UBOOL						NoAlpha;
	UVolumeTextureFactory();
	void StaticConstructor();
	UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn );
};

class UFontFactory : public UTextureFactory
{
	DECLARE_CLASS(UFontFactory,UTextureFactory,0,Editor)
	UFontFactory();
	void StaticConstructor();
	UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn );
};

class UTrueTypeFontFactory : public UFontFactory
{
	DECLARE_CLASS(UTrueTypeFontFactory,UFontFactory,CLASS_CollapseCategories,Editor)
	FStringNoInit	FontName;
	FLOAT			Height;
	INT				USize;
	INT				VSize;
	INT				XPad;
	INT				YPad;
	INT				Count;
	FLOAT			Gamma;
	FStringNoInit	Chars;
	UBOOL			AntiAlias;
	FString			UnicodeRange;
	FString			Wildcard;
	FString			Path;
	INT				Style; 
	INT				Italic;
	INT				Underline;
	INT             Kerning;
	INT             DropShadowX;
	INT             DropShadowY;
	INT				ExtendBoxTop;
	INT				ExtendBoxBottom;
	INT				ExtendBoxRight;
	INT				ExtendBoxLeft;

	UTrueTypeFontFactory();
	void StaticConstructor();
	UObject* FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn );
	UTexture2D* CreateTextureFromDC( UFont* Font, DWORD dc, INT RowHeight, INT TextureNum );
};

class USphericalHarmonicMapFactorySHM : public UFactory
{
	DECLARE_CLASS(USphericalHarmonicMapFactorySHM,UFactory,0,Editor);
public:

	USphericalHarmonicMapFactorySHM();
	void StaticConstructor();
	UObject* FactoryCreateBinary(
		UClass* InClass,
		UObject* InOuter,
		FName InName,
		DWORD InFlags,
		UObject* Context,
		const TCHAR* Type,
		const BYTE*& Buffer,
		const BYTE* BufferEnd,
		FFeedbackContext* Warn
		);
};

class USequenceFactory : public UFactory
{
	DECLARE_CLASS(USequenceFactory,UFactory,0,Editor);
	USequenceFactory();
	void StaticConstructor();
	UObject* FactoryCreateText( ULevel* InLevel, UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn );
};

/*-----------------------------------------------------------------------------
	Exporters.
-----------------------------------------------------------------------------*/

class UTextBufferExporterTXT : public UExporter
{
	DECLARE_CLASS(UTextBufferExporterTXT,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Out, FFeedbackContext* Warn );
};

class USoundExporterWAV : public UExporter
{
	DECLARE_CLASS(USoundExporterWAV,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn );
};

class UClassExporterUC : public UExporter
{
	DECLARE_CLASS(UClassExporterUC,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

class UObjectExporterT3D : public UExporter
{
	DECLARE_CLASS(UObjectExporterT3D,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

class UComponentExporterT3D : public UExporter
{
	DECLARE_CLASS(UComponentExporterT3D,UExporter,0,Editor);
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

class UPolysExporterT3D : public UExporter
{
	DECLARE_CLASS(UPolysExporterT3D,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

class UModelExporterT3D : public UExporter
{
	DECLARE_CLASS(UModelExporterT3D,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

class UStaticMeshExporterT3D : public UExporter
{
	DECLARE_CLASS(UStaticMeshExporterT3D,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

class ULevelExporterT3D : public UExporter
{
	DECLARE_CLASS(ULevelExporterT3D,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

class ULevelExporterSTL : public UExporter
{
	DECLARE_CLASS(ULevelExporterSTL,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

class UPrefabExporterT3D : public UExporter
{
	DECLARE_CLASS(UPrefabExporterT3D,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

class UTextureExporterPCX : public UExporter
{
	DECLARE_CLASS(UTextureExporterPCX,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn );
};

class UTextureExporterBMP : public UExporter
{
	DECLARE_CLASS(UTextureExporterBMP,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn );
};
class UTextureExporterTGA : public UExporter
{
	DECLARE_CLASS(UTextureExporterTGA,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn );
};

class ULevelExporterOBJ : public UExporter
{
	DECLARE_CLASS(ULevelExporterOBJ,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};
class UPolysExporterOBJ : public UExporter
{
	DECLARE_CLASS(UPolysExporterOBJ,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};
class USequenceExporterT3D : public UExporter
{
	DECLARE_CLASS(USequenceExporterT3D,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/

// Accepts a triangle (XYZ and UV values for each point) and returns a poly base and UV vectors
// NOTE : the UV coords should be scaled by the texture size
static inline void FTexCoordsToVectors( FVector V0, FVector UV0, FVector V1, FVector UV1, FVector V2, FVector UV2, FVector* InBaseResult, FVector* InUResult, FVector* InVResult )
{
	// Create polygon normal.
	FVector PN = FVector((V0-V1) ^ (V2-V0));
	PN = PN.SafeNormal();

	// Fudge UV's to make sure no infinities creep into UV vector math, whenever we detect identical U or V's.
	if( ( UV0.X == UV1.X ) || ( UV2.X == UV1.X ) || ( UV2.X == UV0.X ) ||
		( UV0.Y == UV1.Y ) || ( UV2.Y == UV1.Y ) || ( UV2.Y == UV0.Y ) )
	{
		UV1 += FVector(0.004173f,0.004123f,0.0f);
		UV2 += FVector(0.003173f,0.003123f,0.0f);
	}

	//
	// Solve the equations to find our texture U/V vectors 'TU' and 'TV' by stacking them 
	// into a 3x3 matrix , one for  u(t) = TU dot (x(t)-x(o) + u(o) and one for v(t)=  TV dot (.... , 
	// then the third assumes we're perpendicular to the normal. 
	//
	FMatrix TexEqu = FMatrix::Identity;
	TexEqu.SetAxis( 0, FVector(	V1.X - V0.X, V1.Y - V0.Y, V1.Z - V0.Z ) );
	TexEqu.SetAxis( 1, FVector( V2.X - V0.X, V2.Y - V0.Y, V2.Z - V0.Z ) );
	TexEqu.SetAxis( 2, FVector( PN.X,        PN.Y,        PN.Z        ) );
	TexEqu = TexEqu.Inverse();

	FVector UResult( UV1.X-UV0.X, UV2.X-UV0.X, 0.0f );
	FVector TUResult = TexEqu.TransformNormal( UResult );

	FVector VResult( UV1.Y-UV0.Y, UV2.Y-UV0.Y, 0.0f );
	FVector TVResult = TexEqu.TransformNormal( VResult );

	//
	// Adjust the BASE to account for U0 and V0 automatically, and force it into the same plane.
	//				
	FMatrix BaseEqu = FMatrix::Identity;
	BaseEqu.SetAxis( 0, TUResult );
	BaseEqu.SetAxis( 1, TVResult ); 
	BaseEqu.SetAxis( 2, FVector( PN.X, PN.Y, PN.Z ) );
	BaseEqu = BaseEqu.Inverse();

	FVector BResult = FVector( UV0.X - ( TUResult|V0 ), UV0.Y - ( TVResult|V0 ),  0.0f );

	*InBaseResult = - 1.0f *  BaseEqu.TransformNormal( BResult );
	*InUResult = TUResult;
	*InVResult = TVResult;

}

/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

class UEditorPlayer : public ULocalPlayer
{
	DECLARE_CLASS(UEditorPlayer,ULocalPlayer,0,Editor);

	// FExec interface.
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	// FViewportClient interface.
	virtual void Precache(FChildViewport* Viewport,FPrecacheInterface* P) {}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

#endif // include-once blocker

