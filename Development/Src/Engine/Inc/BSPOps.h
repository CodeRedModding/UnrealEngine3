/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __BSPOPS_H__
#define __BSPOPS_H__

class IndexPoly {
public:
    IndexPoly(){}
    TArray<INT> Indices;
};
 
class NodePolys {
public:
    TArray<IndexPoly> Polys;
    TArray<FVector> Vertices;
    TArray<FVector2D> ShadowTexCoords;
    NodePolys(){}
 
    static NodePolys* create(UModel* TheModel, FBspNode* Node);
};
 
class TempPoly
{
public:
    TArray<FVector>        Vertices;
    TArray<FVector2D>    ShadowTexCoords;
 
    /**
     * Constructor, initializing all member variables.
     */
    TempPoly(){}
 
    INT SplitWithPlane(const FVector &Base,const FVector &Normal,TempPoly *FrontPoly,TempPoly *BackPoly,INT VeryPrecise) const;
    INT Fix();
 
    friend UBOOL operator==(const TempPoly& A,const TempPoly& B)
    {
        if(A.Vertices.Num() != B.Vertices.Num())
        {
            return FALSE;
        }
 
        for(INT VertexIndex = 0;VertexIndex < A.Vertices.Num();VertexIndex++)
        {
            if(A.Vertices(VertexIndex) != B.Vertices(VertexIndex))
            {
                return FALSE;
            }
        }
 
        return TRUE;
    }
    friend UBOOL operator!=(const TempPoly& A,const TempPoly& B)
    {
       return !(A == B);
    }
};

#if !CONSOLE

class FBSPOps
{
public:
	/** Quality level for rebuilding Bsp. */
	enum EBspOptimization
	{
		BSP_Lame,
		BSP_Good,
		BSP_Optimal
	};

	/** Possible positions of a child Bsp node relative to its parent (for BspAddToNode) */
	enum ENodePlace 
	{
		NODE_Back		= 0, // Node is in back of parent              -> Bsp[iParent].iBack.
		NODE_Front		= 1, // Node is in front of parent             -> Bsp[iParent].iFront.
		NODE_Plane		= 2, // Node is coplanar with parent           -> Bsp[iParent].iPlane.
		NODE_Root		= 3, // Node is the Bsp root and has no parent -> Bsp[0].
	};

	static void csgPrepMovingBrush( ABrush* Actor );
	static void csgCopyBrush( ABrush* Dest, ABrush* Src, DWORD PolyFlags, EObjectFlags ResFlags, UBOOL NeedsPrep, UBOOL bCopyPosRotScale );
	static ABrush*	csgAddOperation( ABrush* Actor, DWORD PolyFlags, ECsgOper CSG );

	static INT bspAddVector( UModel* Model, FVector* V, UBOOL Exact );
	static INT bspAddPoint( UModel* Model, FVector* V, UBOOL Exact );
	static void bspBuild( UModel* Model, enum EBspOptimization Opt, INT Balance, INT PortalBias, INT RebuildSimplePolys, INT iNode );
	static void bspRefresh( UModel* Model, UBOOL NoRemapSurfs );

	static void bspBuildBounds( UModel* Model );

	static void bspValidateBrush( UModel* Brush, UBOOL ForceValidate, UBOOL DoStatusUpdate );
	static void bspUnlinkPolys( UModel* Brush );
	static INT	bspAddNode( UModel* Model, INT iParent, enum ENodePlace ENodePlace, DWORD NodeFlags, FPoly* EdPoly );

	static void SubdividePolys(TArray<TempPoly>* nodes);
	static void SubdividePoly(TArray<TempPoly>* nodes, TempPoly* poly, UBOOL added);
	static UBOOL TooBig(TempPoly* node);
	static FVector BigMidPoint(TempPoly* node);
	static FVector BigNormal(TempPoly* node);

	/**
	 * Rebuild some brush internals
	 */
	static void RebuildBrush(UModel* Brush);

	static FPoly BuildInfiniteFPoly( UModel* Model, INT iNode );
	static FZoneSet BuildZoneMasks( UModel* Model, INT iNode );

	/**
	 * Rotates the specified brush's vertices.
	 */
	static void RotateBrushVerts(ABrush* Brush, const FRotator& Rotation, UBOOL bClearComponents);

	/** Errors encountered in Csg operation. */
	static INT GErrors;
	static UBOOL GFastRebuild;

protected:
	static void SplitPolyList
	(
		UModel				*Model,
		INT                 iParent,
		ENodePlace			NodePlace,
		INT                 NumPolys,
		FPoly				**PolyList,
		EBspOptimization	Opt,
		INT					Balance,
		INT					PortalBias,
		INT					RebuildSimplePolys
	);
};

/** @todo: remove when uses of ENodePlane have been replaced with FBSPOps::ENodePlace. */
typedef FBSPOps::ENodePlace ENodePlace;

#else 

class FBSPOps
{
public:
	static void SubdividePolys(TArray<TempPoly>* nodes);
	static void SubdividePoly(TArray<TempPoly>* nodes, TempPoly* poly, UBOOL added);
	static UBOOL TooBig(TempPoly* node);
	static FVector BigMidPoint(TempPoly* node);
	static FVector BigNormal(TempPoly* node);
};

#endif // !CONSOLE

#endif // __BSPOPS_H__
