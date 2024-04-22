/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionMeshSubUV extends MaterialExpressionTextureSample
	native(Material);

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual INT GetWidth() const;
	virtual FString GetCaption() const;
	virtual INT GetLabelPadding() { return 8; }
}

defaultproperties
{
	MenuCategories(0)="Texture"
	MenuCategories(1)="Particles"
	Outputs(0)=(OutputName="",Mask=1,MaskR=1,MaskG=1,MaskB=1,MaskA=0)
	Outputs(1)=(OutputName="",Mask=1,MaskR=1,MaskG=0,MaskB=0,MaskA=0)
	Outputs(2)=(OutputName="",Mask=1,MaskR=0,MaskG=1,MaskB=0,MaskA=0)
	Outputs(3)=(OutputName="",Mask=1,MaskR=0,MaskG=0,MaskB=1,MaskA=0)
	Outputs(4)=(OutputName="",Mask=1,MaskR=0,MaskG=0,MaskB=0,MaskA=1)
}
