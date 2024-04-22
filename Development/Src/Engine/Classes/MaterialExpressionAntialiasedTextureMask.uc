/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionAntialiasedTextureMask extends MaterialExpressionTextureSampleParameter2D
	native(Material);

var() float	Threshold <UIMin=0.0 | UIMax=1.0 | ClampMin=0.0 | ClampMax=1.0>;
var() enum ETextureColorChannel
{
	TCC_Red,
	TCC_Green,
	TCC_Blue,
	TCC_Alpha
} Channel;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
	virtual UBOOL TextureIsValid( UTexture* InTexture );
	virtual const TCHAR* GetRequirements();
	virtual void SetDefaultTexture();

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);
}

defaultproperties
{
	Texture=Texture2D'EngineResources.DefaultTexture'
	MenuCategories(0)="HighLevel"  
	Threshold=0.5
	ParameterName="None"
	Channel=TCC_Alpha
	Outputs(0)=(OutputName="",Mask=1,MaskR=1,MaskG=0,MaskB=0,MaskA=0)
}
