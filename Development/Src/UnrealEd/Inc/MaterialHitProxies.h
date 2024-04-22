/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

struct HMaterial: HHitProxy
{
	DECLARE_HIT_PROXY(HMaterial,HHitProxy);

	UMaterial*	Material;

	HMaterial(UMaterial* InMaterial): HHitProxy(HPP_UI), Material(InMaterial) {}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << *(UObject**)&Material;
	}
};

struct HMaterialExpression : HHitProxy
{
	DECLARE_HIT_PROXY(HMaterialExpression,HHitProxy);

	UMaterialExpression*	Expression;

	HMaterialExpression( UMaterialExpression* InExpression ): HHitProxy(HPP_UI), Expression(InExpression) {}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Expression;
	}
};

struct HInputHandle : HHitProxy
{
	DECLARE_HIT_PROXY(HInputHandle,HHitProxy);

	FExpressionInput*	Input;

	HInputHandle(FExpressionInput* InInput): HHitProxy(HPP_UI), Input(InInput) {}
};

struct HOutputHandle : HHitProxy
{
	DECLARE_HIT_PROXY(HOutputHandle,HHitProxy);

	UMaterialExpression*	Expression;
	FExpressionOutput		Output;

	HOutputHandle(UMaterialExpression* InExpression,const FExpressionOutput& InOutput):
		HHitProxy(HPP_UI),
		Expression(InExpression),
		Output(InOutput)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Expression;
	}
};

struct HDeleteExpression : HHitProxy
{
	DECLARE_HIT_PROXY(HDeleteExpression,HHitProxy);

	UMaterialExpression*	Expression;

	HDeleteExpression(UMaterialExpression* InExpression):
		HHitProxy(HPP_UI),
		Expression(InExpression)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Expression;
	}
};
