/*=============================================================================
	DecalVertexFactory.h: Decal vertex factory implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DECALVERTEXFACTORY_H__
#define __DECALVERTEXFACTORY_H__

/**
 *	The DecalVertexFactoryBase is intended to be used for adding decal functionality
 *	to existing vertex factories.
 */
class FDecalVertexFactoryBase
{
public:
	FDecalVertexFactoryBase()
		:	DecalMatrix(FMatrix::Identity)
		,	DecalLocation(0.f,0.f,0.f)
		,	DecalOffset(0.f,0.f)
		,	DecalLocalBinormal(0.f,0.f,0.f)
		,	DecalLocalTangent(0.f,0.f,0.f)
		,	DecalLocalNormal(0.f,0.f,0.f)
		,	DecalMinMaxBlend(-1.f,1.f)
	{}

	virtual ~FDecalVertexFactoryBase()
	{
	}

	virtual FVertexFactory* CastToFVertexFactory() = 0;

	const FMatrix& GetDecalMatrix() const
	{
		return DecalMatrix;
	}

	const FVector& GetDecalLocation() const
	{
		return DecalLocation;
	}

	const FVector2D& GetDecalOffset() const
	{
		return DecalOffset;
	}

	const FVector& GetDecalLocalBinormal() const
	{
		return DecalLocalBinormal;
	}

	const FVector& GetDecalLocalTangent() const
	{
		return DecalLocalTangent;
	}

	FVector GetDecalLocalNormal() const		
	{
		return DecalLocalNormal;
	}

	const FVector2D& GetDecalMinMaxBlend() const
	{
		return DecalMinMaxBlend;
	}

	virtual void SetDecalMatrix(const FMatrix& InDecalMatrix)
	{
		DecalMatrix = InDecalMatrix;
	}

	virtual void SetDecalLocation(const FVector& InDecalLocation)
	{
		DecalLocation = InDecalLocation;
	}

	virtual void SetDecalOffset(const FVector2D& InDecalOffset)
	{
		DecalOffset = InDecalOffset;
	}

	virtual void SetDecalLocalBinormal(const FVector& InDecalLocalBinormal)
	{
		DecalLocalBinormal = InDecalLocalBinormal.SafeNormal();
	}

	virtual void SetDecalLocalTangent(const FVector& InDecalLocalTangent)
	{
		DecalLocalTangent = InDecalLocalTangent.SafeNormal();
	}

	virtual void SetDecalLocalNormal(const FVector& InDecalLocalNormal)
	{
		DecalLocalNormal = InDecalLocalNormal.SafeNormal();
	}

	virtual void SetDecalMinMaxBlend(const FVector2D& InDecalMinMaxBlend)
	{
		DecalMinMaxBlend = InDecalMinMaxBlend;
	}

protected:
	FMatrix DecalMatrix;
	FVector DecalLocation;
	FVector2D DecalOffset;
	FVector DecalLocalBinormal;
	FVector DecalLocalTangent;
	FVector DecalLocalNormal;
	/** dot product min,max values for blending decal */
	FVector2D DecalMinMaxBlend;
};

#endif // __DECALVERTEXFACTORY_H__
