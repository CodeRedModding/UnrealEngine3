/*=============================================================================
	UnMaterial.cpp: Shader implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"

IMPLEMENT_CLASS(UMaterialInstance);
IMPLEMENT_CLASS(UMaterial);
IMPLEMENT_CLASS(UMaterialInstanceConstant);
IMPLEMENT_CLASS(UMaterialExpression);
IMPLEMENT_CLASS(UMaterialExpressionTextureSample);
IMPLEMENT_CLASS(UMaterialExpressionMultiply);
IMPLEMENT_CLASS(UMaterialExpressionDivide);
IMPLEMENT_CLASS(UMaterialExpressionSubtract);
IMPLEMENT_CLASS(UMaterialExpressionLinearInterpolate);
IMPLEMENT_CLASS(UMaterialExpressionAdd);
IMPLEMENT_CLASS(UMaterialExpressionTextureCoordinate);
IMPLEMENT_CLASS(UMaterialExpressionComponentMask);
IMPLEMENT_CLASS(UMaterialExpressionDotProduct);
IMPLEMENT_CLASS(UMaterialExpressionCrossProduct);
IMPLEMENT_CLASS(UMaterialExpressionClamp);
IMPLEMENT_CLASS(UMaterialExpressionConstant);
IMPLEMENT_CLASS(UMaterialExpressionConstant2Vector);
IMPLEMENT_CLASS(UMaterialExpressionConstant3Vector);
IMPLEMENT_CLASS(UMaterialExpressionConstant4Vector);
IMPLEMENT_CLASS(UMaterialExpressionTime);
IMPLEMENT_CLASS(UMaterialExpressionCameraVector);
IMPLEMENT_CLASS(UMaterialExpressionReflectionVector);
IMPLEMENT_CLASS(UMaterialExpressionPanner);
IMPLEMENT_CLASS(UMaterialExpressionRotator);
IMPLEMENT_CLASS(UMaterialExpressionSine);
IMPLEMENT_CLASS(UMaterialExpressionCosine);
IMPLEMENT_CLASS(UMaterialExpressionBumpOffset);
IMPLEMENT_CLASS(UMaterialExpressionAppendVector);
IMPLEMENT_CLASS(UMaterialExpressionFloor);
IMPLEMENT_CLASS(UMaterialExpressionCeil);
IMPLEMENT_CLASS(UMaterialExpressionFrac);
IMPLEMENT_CLASS(UMaterialExpressionDesaturation);
IMPLEMENT_CLASS(UMaterialExpressionVectorParameter);
IMPLEMENT_CLASS(UMaterialExpressionScalarParameter);
IMPLEMENT_CLASS(UMaterialExpressionNormalize);
IMPLEMENT_CLASS(UMaterialExpressionVertexColor);
//IMPLEMENT_CLASS(UMaterialExpressionSubdivisionSelection);
IMPLEMENT_CLASS(UMaterialExpressionParticleSubUV);

//
//	FMaterialThumbnailScene
//

struct FMaterialThumbnailScene: FPreviewScene
{
	FSceneView	View;

	// Constructor/destructor.

	FMaterialThumbnailScene(UMaterialInstance* InMaterial,EThumbnailPrimType InPrimType,INT InPitch,INT InYaw,FLOAT InZoomPct,UBOOL InShowBackground)
	{
		InZoomPct = Clamp<FLOAT>( InZoomPct, 1.0, 2.0 );

		View.ViewMatrix = View.ViewMatrix * FTranslationMatrix(-FVector(-350,0,0));
		View.ViewMatrix = View.ViewMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));
		View.ProjectionMatrix = FPerspectiveMatrix(
			75.0f * (FLOAT)PI / 360.0f,
			1.0f,
			1.0f,
			NEAR_CLIPPING_PLANE
			);
		View.ShowFlags = SHOW_DefaultEditor;
		View.BackgroundColor = FColor(0,0,0);

		// Background

		UStaticMeshComponent* BackgroundComponent = NULL;
		if( InShowBackground )
		{
			BackgroundComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
			BackgroundComponent->StaticMesh = (UStaticMesh*)UObject::StaticLoadObject(UStaticMesh::StaticClass(),NULL,TEXT("EditorMeshes.TexPropCube"),NULL,LOAD_NoFail,NULL);
			BackgroundComponent->Materials.AddItem( (UMaterial*)UObject::StaticLoadObject(UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.ThumbnailBack"),NULL,LOAD_NoFail,NULL) );
			BackgroundComponent->CastShadow = 0;
			BackgroundComponent->Scene = this;
			BackgroundComponent->SetParentToWorld( FScaleMatrix( FVector(7,7,7) ) );
			BackgroundComponent->Created();
		}

		// Preview primitive

		UStaticMeshComponent*	StaticMeshComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
		switch( InPrimType )
		{
			case TPT_Cube:
				StaticMeshComponent->StaticMesh = (UStaticMesh*)UObject::StaticLoadObject(UStaticMesh::StaticClass(),NULL,TEXT("EditorMeshes.TexPropCube"),NULL,LOAD_NoFail,NULL);
				break;
			case TPT_Sphere:
				StaticMeshComponent->StaticMesh = (UStaticMesh*)UObject::StaticLoadObject(UStaticMesh::StaticClass(),NULL,TEXT("EditorMeshes.TexPropSphere"),NULL,LOAD_NoFail,NULL);
				break;
			case TPT_Cylinder:
				StaticMeshComponent->StaticMesh = (UStaticMesh*)UObject::StaticLoadObject(UStaticMesh::StaticClass(),NULL,TEXT("EditorMeshes.TexPropCylinder"),NULL,LOAD_NoFail,NULL);
				break;
			case TPT_Plane:
				StaticMeshComponent->StaticMesh = (UStaticMesh*)UObject::StaticLoadObject(UStaticMesh::StaticClass(),NULL,TEXT("EditorMeshes.TexPropPlane"),NULL,LOAD_NoFail,NULL);
				break;
			default:
				check(0);
		}
		StaticMeshComponent->Materials.AddItem(InMaterial);
		StaticMeshComponent->CastShadow = 0;
		StaticMeshComponent->Scene = this;
		FMatrix Matrix = FMatrix::Identity;
		Matrix = Matrix * FRotationMatrix( FRotator(-InPitch,-InYaw,0) );

		// Hack because Plane staticmesh points wrong way.. sigh..
		if(InPrimType == TPT_Plane)
			Matrix = Matrix * FRotationMatrix( FRotator(0, 32767, 0) );

		InZoomPct -= 1.0;
		Matrix = Matrix * FTranslationMatrix( FVector(-100*InZoomPct,0,0 ) );
		StaticMeshComponent->SetParentToWorld( Matrix );
		StaticMeshComponent->Created();
		if( InShowBackground )
			Components.AddItem(BackgroundComponent);
		Components.AddItem(StaticMeshComponent);
	}
};

//
//	UMaterialInstance thumbnail drawing
//

void UMaterialInstance::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, INT InPitch, INT InYaw, FChildViewport* InViewport, FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPCT, INT InFixedSz )
{
	FMaterialThumbnailScene	ThumbnailScene(this,InPrimType,InPitch,InYaw,InZoomPCT,InShowBackground);
	InRI->DrawScene(
		FSceneContext(
			&ThumbnailScene.View,
			&ThumbnailScene,
			InX + InRI->Origin2D.X,
			InY + InRI->Origin2D.Y,
			InFixedSz ? InFixedSz : 512 * InZoom,
			InFixedSz ? InFixedSz : 512 * InZoom
			)
		);
}

FThumbnailDesc UMaterialInstance::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	FThumbnailDesc td;
	td.Width = InFixedSz ? InFixedSz : 512*InZoom;
	td.Height = InFixedSz ? InFixedSz : 512*InZoom;
	return td;
}

INT UMaterialInstance::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();

	new( *InLabels )FString( GetName() );

	return InLabels->Num();
}

INT UMaterialInstance::GetWidth() const
{
	return ME_PREV_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

INT UMaterialInstance::GetHeight() const
{
	return ME_PREV_THUMBNAIL_SZ+ME_CAPTION_HEIGHT+(ME_STD_BORDER*2);
}

//
//	UMaterialInstance::execGetMaterial
//

void UMaterialInstance::execGetMaterial(FFrame& Stack,RESULT_DECL)
{
	*(UMaterial**) Result = GetMaterial();
}

//
//	FMICReentranceGuard - Protects the members of a UMaterialInstanceConstant from re-entrance.
//

struct FMICReentranceGuard
{
	UMaterialInstanceConstant*	Material;
	FMICReentranceGuard(UMaterialInstanceConstant* InMaterial):
		Material(InMaterial)
	{
		check(!Material->ReentrantFlag);
		Material->ReentrantFlag = 1;
	}
	~FMICReentranceGuard()
	{
		Material->ReentrantFlag = 0;
	}
};

//
//	FConstantMaterialInstance
//

struct FConstantMaterialInstance: FMaterialInstance
{
	// Update

	void Update(UMaterialInstanceConstant* Instance)
	{
		// Copy instance parameter values from the parent.
		if(Instance->Parent)
		{
			FMaterialInstance*	ParentInstance = Instance->Parent->GetInstanceInterface();
			VectorValueMap = ParentInstance->VectorValueMap;
			ScalarValueMap = ParentInstance->ScalarValueMap;
		}
		else
		{
			VectorValueMap.Empty();
			ScalarValueMap.Empty();
		}

		for(INT ValueIndex = 0;ValueIndex <	Instance->VectorParameterValues.Num();ValueIndex++)
			VectorValueMap.Set(Instance->VectorParameterValues(ValueIndex).ParameterName,Instance->VectorParameterValues(ValueIndex).ParameterValue);
		for(INT ValueIndex = 0;ValueIndex <	Instance->ScalarParameterValues.Num();ValueIndex++)
			ScalarValueMap.Set(Instance->ScalarParameterValues(ValueIndex).ParameterName,Instance->ScalarParameterValues(ValueIndex).ParameterValue);
	}

	// Constructor.

	FConstantMaterialInstance(UMaterialInstanceConstant* Instance)
	{
		Update(Instance);
	}
};

//
//	UMaterialInstanceConstant::UMaterialInstanceConstant
//

UMaterialInstanceConstant::UMaterialInstanceConstant()
{
	MaterialInstance = new FConstantMaterialInstance(this);
}

//
//	UMaterialInstanceConstant::GetMaterial
//

UMaterial* UMaterialInstanceConstant::GetMaterial()
{
	if(ReentrantFlag)
		return GEngine->DefaultMaterial;
	FMICReentranceGuard	Guard(this);
	if(Parent)
		return Parent->GetMaterial();
	else
		return GEngine->DefaultMaterial;
}

//
//	UMaterialInstanceConstant::GetLayerMask
//

DWORD UMaterialInstanceConstant::GetLayerMask()
{
	if(ReentrantFlag)
		return GEngine->DefaultMaterial->GetLayerMask();
	FMICReentranceGuard	ReentranceGuard(this);
	if(Parent)
		return Parent->GetLayerMask();
	else
		return GEngine->DefaultMaterial->GetLayerMask();
}

//
//	UMaterialInstanceConstant::GetMaterialInterface
//

FMaterial* UMaterialInstanceConstant::GetMaterialInterface(UBOOL Selected)
{
	if(ReentrantFlag)
		return GEngine->DefaultMaterial->GetMaterialInterface(Selected);
	FMICReentranceGuard	ReentranceGuard(this);
	if(Parent)
		return Parent->GetMaterialInterface(Selected);
	else
		return GEngine->DefaultMaterial->GetMaterialInterface(Selected);
}

//
//	UMaterialInstanceConstant::SetVectorParameterValue
//
bool UMaterialInstanceConstant::SetVectorParameterValue(const FString& ParameterName,FColor Value)
{
	FName name(*ParameterName);
	MaterialInstance->VectorValueMap.Set(name,Value);

	for(INT ValueIndex = 0;ValueIndex < VectorParameterValues.Num();ValueIndex++)
	{
		if(VectorParameterValues(ValueIndex).ParameterName == name)
		{
			VectorParameterValues(ValueIndex).ParameterValue = Value;
			return true;
		}
	}

	FVectorParameterValue*	NewParameterValue = new(VectorParameterValues) FVectorParameterValue;
	NewParameterValue->ParameterName = name;
	NewParameterValue->ParameterValue = Value;

	return true;
}

//
//	UMaterialInstanceConstant::SetScalarParameterValue
//
bool UMaterialInstanceConstant::SetScalarParameterValue(const FString& ParameterName,float Value)
{
	return false;
}

void UMaterialInstanceConstant::SetParent(UMaterialInstance* NewParent)
{
	Parent = NewParent;
	((FConstantMaterialInstance*)MaterialInstance)->Update(this);
}

//
//	UMaterialInstanceConstant::PostLoad
//

void UMaterialInstanceConstant::PostLoad()
{
	Super::PostLoad();
	((FConstantMaterialInstance*)MaterialInstance)->Update(this);
}

//
//	UMaterialInstanceConstant::PostEditChange
//

void UMaterialInstanceConstant::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);
	((FConstantMaterialInstance*)MaterialInstance)->Update(this);
}

//
//	UMaterialInstanceConstant::Destroy
//

void UMaterialInstanceConstant::Destroy()
{
	Super::Destroy();
	delete MaterialInstance;
}

//
//	UMaterialInstanceConstant::execSetParent
//

void UMaterialInstanceConstant::execSetParent(FFrame& Stack,RESULT_DECL)
{
	P_GET_OBJECT(UMaterialInstance,NewParent);
	P_FINISH;

	Parent = NewParent;
	((FConstantMaterialInstance*)MaterialInstance)->Update(this);
}

//
//	UMaterialInstanceConstant::execSetVectorParameterValue
//

void UMaterialInstanceConstant::execSetVectorParameterValue(FFrame& Stack,RESULT_DECL)
{
	P_GET_NAME(ParameterName);
	P_GET_STRUCT(FLinearColor,Value);
	P_FINISH;

	MaterialInstance->VectorValueMap.Set(ParameterName,Value);

	for(INT ValueIndex = 0;ValueIndex < VectorParameterValues.Num();ValueIndex++)
	{
		if(VectorParameterValues(ValueIndex).ParameterName == ParameterName)
		{
			VectorParameterValues(ValueIndex).ParameterValue = Value;
			return;
		}
	}

	FVectorParameterValue*	NewParameterValue = new(VectorParameterValues) FVectorParameterValue;
	NewParameterValue->ParameterName = ParameterName;
	NewParameterValue->ParameterValue = Value;
}

//
//	UMaterialInstanceConstant::execSetScalarParameterValue
//

void UMaterialInstanceConstant::execSetScalarParameterValue(FFrame& Stack,RESULT_DECL)
{
	P_GET_NAME(ParameterName);
	P_GET_FLOAT(Value);
	P_FINISH;

	MaterialInstance->ScalarValueMap.Set(ParameterName,Value);

	for(INT ValueIndex = 0;ValueIndex < ScalarParameterValues.Num();ValueIndex++)
	{
		if(ScalarParameterValues(ValueIndex).ParameterName == ParameterName)
		{
			ScalarParameterValues(ValueIndex).ParameterValue = Value;
			return;
		}
	}

	FScalarParameterValue*	NewParameterValue = new(ScalarParameterValues) FScalarParameterValue;
	NewParameterValue->ParameterName = ParameterName;
	NewParameterValue->ParameterValue = Value;
}

//
//	FMaterialExpressionGuard - Traps material compiler errors and re-entrancy in the context of an expression.
//

struct FMaterialExpressionGuard: FMaterialCompilerGuard
{
	UMaterialExpression*	Expression;
	FMaterialCompiler*		Compiler;

	// Constructor.

	FMaterialExpressionGuard(UMaterialExpression* InExpression,FMaterialCompiler* InCompiler):
		Expression(InExpression),
		Compiler(InCompiler)
	{
		if(Expression->Compiling)
			Compiler->Errorf(TEXT("Cyclic material expression detected."));
		Expression->Errors.Empty();
		Expression->Compiling = 1;
		Compiler->EnterGuard(this);
	}

	~FMaterialExpressionGuard()
	{
		Compiler->ExitGuard();
		Expression->Compiling = 0;
	}

	// FMaterialCompilerGuard interface.

	virtual void Error(const TCHAR* Text)
	{
		new(Expression->Errors) FString(FString::Printf(TEXT("%s: %s"),Expression->GetName(),Text));
	}
};

//
//	FExpressionInput::Compile
//

INT FExpressionInput::Compile(FMaterialCompiler* Compiler)
{
	if(Expression)
	{
		FMaterialExpressionGuard	Guard(Expression,Compiler);

		if(Mask)
			return Compiler->ComponentMask(Expression->Compile(Compiler),MaskR,MaskG,MaskB,MaskA);
		else
			return Expression->Compile(Compiler);
	}
	else
		return INDEX_NONE;
}

//
//	FColorMaterialInput::Compile
//

INT FColorMaterialInput::Compile(FMaterialCompiler* Compiler,const FColor& Default)
{
	if(UseConstant)
	{
		FLinearColor	LinearColor(Constant);
		return Compiler->Constant3(LinearColor.R,LinearColor.G,LinearColor.B);
	}
	else if(Expression)
		return FExpressionInput::Compile(Compiler);
	else
	{
		FLinearColor	LinearColor(Default);
		return Compiler->Constant3(LinearColor.R,LinearColor.G,LinearColor.B);
	}
}

//
//	FScalarMaterialInput::Compile
//

INT FScalarMaterialInput::Compile(FMaterialCompiler* Compiler,FLOAT Default)
{
	if(UseConstant)
		return Compiler->Constant(Constant);
	else if(Expression)
		return FExpressionInput::Compile(Compiler);
	else
		return Compiler->Constant(Default);
}

//
//	FVectorMaterialInput::Compile
//

INT FVectorMaterialInput::Compile(FMaterialCompiler* Compiler,const FVector& Default)
{
	if(UseConstant)
		return Compiler->Constant3(Constant.X,Constant.Y,Constant.Z);
	else if(Expression)
		return FExpressionInput::Compile(Compiler);
	else
		return Compiler->Constant3(Default.X,Default.Y,Default.Z);
}

//
//	FVector2MaterialInput::Compile
//

INT FVector2MaterialInput::Compile(FMaterialCompiler* Compiler,const FVector2D& Default)
{
	if(UseConstant)
		return Compiler->Constant2(Constant.X,Constant.Y);
	else if(Expression)
		return FExpressionInput::Compile(Compiler);
	else
		return Compiler->Constant2(Default.X,Default.Y);
}

//
//	FMaterialResource
//

struct FMaterialResource: FMaterial
{
	UMaterial*	Material;
	UBOOL		Selected;

	// UpdateProperties

	void Update()
	{
		SourceSHM = Material->SHM;
		TwoSided = Material->TwoSided;
		NonDirectionalLighting = Material->NonDirectionalLighting;
		BlendMode = Material->BlendMode;
		OpacityMaskClipValue = Material->OpacityMaskClipValue;
		Unlit = Material->Unlit;
		GResourceManager->UpdateResource(this);
	}

	// Constructor.

	FMaterialResource(UMaterial* InMaterial,UBOOL InSelected):
		Material(InMaterial),
		Selected(InSelected)
	{
		Update();
	}

	// FMaterial interface.

	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler)
	{
		FPlane	SelectionColor = FColor(10,5,60).Plane();

		switch(Property)
		{
		case MP_EmissiveColor:
			if(Selected)
				return Compiler->Add(Compiler->ForceCast(Material->EmissiveColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3),Compiler->Constant3(SelectionColor.X,SelectionColor.Y,SelectionColor.Z));
			else
				return Material->EmissiveColor.Compile(Compiler,FColor(0,0,0));
		case MP_Opacity: return Material->Opacity.Compile(Compiler,1.0f);
		case MP_OpacityMask: return Material->OpacityMask.Compile(Compiler,1.0f);
		case MP_Distortion: return Material->Distortion.Compile(Compiler,FVector2D(0,0));
		case MP_TwoSidedLightingMask: return Compiler->Mul(Compiler->ForceCast(Material->TwoSidedLightingMask.Compile(Compiler,0.0f),MCT_Float),Material->TwoSidedLightingColor.Compile(Compiler,FColor(255,255,255)));
		case MP_DiffuseColor:
			if(Selected)
				return Compiler->Mul(Compiler->ForceCast(Material->DiffuseColor.Compile(Compiler,FColor(128,128,128)),MCT_Float3),Compiler->Constant3(1.0f - SelectionColor.X,1.0f - SelectionColor.Y,1.0f - SelectionColor.Z));
			else
				return Material->DiffuseColor.Compile(Compiler,FColor(128,128,128));
		case MP_SpecularColor: return Material->SpecularColor.Compile(Compiler,FColor(128,128,128));
		case MP_SpecularPower: return Material->SpecularPower.Compile(Compiler,15.0f);
		case MP_Normal: return Material->Normal.Compile(Compiler,FVector(0,0,1));
		case MP_SHM:
			if(Material->SHM)
			{
				const FPlane&	Scale = Material->SHM->CoefficientTextures(0).CoefficientScale,
								Bias = Material->SHM->CoefficientTextures(0).CoefficientBias;
				return Compiler->Add(
						Compiler->Mul(
							Compiler->TextureSample(
								Compiler->Texture(&Material->SHM->CoefficientTextures(0)),
								Compiler->TextureCoordinate(0)
								),
							Compiler->Constant4(Scale.X,Scale.Y,Scale.Z,Scale.W)
							),
						Compiler->Constant4(Bias.X,Bias.Y,Bias.Z,Bias.W)
						);
			}
			else
				return Compiler->Constant(0);
		default:
			return INDEX_NONE;
		};
	}

	// FResource interface.

	virtual FString DescribeResource()
	{
		return Material->GetPathName();
	}

	virtual FGuid GetPersistentId()
	{
		return Material->PersistentIds[Selected];
	}
};

//
//	FDefaultMaterialInstance
//

struct FDefaultMaterialInstance: FMaterialInstance
{
	// Update

	void Update(UMaterial* Material)
	{
		VectorValueMap.Empty();

		for(TObjectIterator<UMaterialExpressionVectorParameter> It;It;++It)
		{
			if(It->IsIn(Material))
				VectorValueMap.Set(It->ParameterName,It->DefaultValue);
		}

		for(TObjectIterator<UMaterialExpressionScalarParameter> It;It;++It)
		{
			if(It->IsIn(Material))
				ScalarValueMap.Set(It->ParameterName,It->DefaultValue);
		}
	}

	// Constructor.

	FDefaultMaterialInstance(UMaterial* Material)
	{
		Update(Material);
	}
};

//
//	UMaterial::UMaterial
//

UMaterial::UMaterial()
{
	PersistentIds[0] = appCreateGuid();
	PersistentIds[1] = appCreateGuid();
	MaterialResources[0] = new FMaterialResource(this,0);
	MaterialResources[1] = new FMaterialResource(this,1);
	DefaultMaterialInstance = new FDefaultMaterialInstance(this);
}

//
//	UMaterial::GetLayerMask
//

DWORD UMaterial::GetLayerMask()
{
	switch(BlendMode)
	{
	case BLEND_Opaque:
	case BLEND_Masked:		return PLM_Opaque;
	case BLEND_Translucent:
	case BLEND_Additive:	return PLM_Translucent;
	default:				return 0;
	}
}

/**
 * Fills in the StreamingTextures array by using a dummy material compiler that parses the material
 * and collects textures that can be streamed.
 */
void UMaterial::CollectStreamingTextures()
{
	StreamingTextures.Empty();

	struct FTextureCollectingMaterialCompiler : public FMaterialCompiler
	{
		UMaterial* Material;

		// Contstructor
		FTextureCollectingMaterialCompiler( UMaterial* InMaterial )
		:	Material( InMaterial )
		{}

		// Gather textures.
		virtual INT Texture(FTextureBase* Texture) 
		{ 
			if( Texture->CanBeStreamed() )
				Material->StreamingTextures.AddItem( Texture );
			return 0; 
		}

		// Not implemented (don't need to).
		virtual INT Error(const TCHAR* Text) { throw Text; return 0; }
		virtual void EnterGuard(FMaterialCompilerGuard* Guard) {}
		virtual void ExitGuard() {}
		virtual EMaterialCodeType GetType(INT Code) { return MCT_Unknown; }
		virtual INT ForceCast(INT Code,EMaterialCodeType DestType) { return 0; }
		virtual INT VectorParameter(FName ParameterName) { return 0; }
		virtual INT ScalarParameter(FName ParameterName) { return 0; }
		virtual INT Constant(FLOAT X) { return 0; }
		virtual INT Constant2(FLOAT X,FLOAT Y) { return 0; }
		virtual INT Constant3(FLOAT X,FLOAT Y,FLOAT Z) { return 0; }
		virtual INT Constant4(FLOAT X,FLOAT Y,FLOAT Z,FLOAT W) { return 0; }
		virtual INT ObjectTime() { return 0; }
		virtual INT SceneTime() { return 0; }
		virtual INT Sine(INT X) { return 0; }
		virtual INT Cosine(INT X) { return 0; }
		virtual INT Floor(INT X) { return 0; }
		virtual INT Ceil(INT X) { return 0; }
		virtual INT Frac(INT X) { return 0; }
		virtual INT ReflectionVector() { return 0; }
		virtual INT CameraVector() { return 0; }
		virtual INT TextureCoordinate(UINT CoordinateIndex) { return 0; }
		virtual INT TextureSample(INT Texture,INT Coordinate) { return 0; }
		virtual INT VertexColor() { return 0; }
		virtual INT Add(INT A,INT B) { return 0; }
		virtual INT Sub(INT A,INT B) { return 0; }
		virtual INT Mul(INT A,INT B) { return 0; }
		virtual INT Div(INT A,INT B) { return 0; }
		virtual INT Dot(INT A,INT B) { return 0; }
		virtual INT Cross(INT A,INT B) { return 0; }
		virtual INT SquareRoot(INT X) { return 0; }
		virtual INT Lerp(INT X,INT Y,INT A) { return 0; }
		virtual INT Min(INT A,INT B) { return 0; }
		virtual INT Max(INT A,INT B) { return 0; }
		virtual INT Clamp(INT X,INT A,INT B) { return 0; }
		virtual INT ComponentMask(INT Vector,UBOOL R,UBOOL G,UBOOL B,UBOOL A) { return 0; }
		virtual INT AppendVector(INT A,INT B) { return 0; }
	} Compiler( this );

	for(INT PropertyIndex = 0;PropertyIndex < MP_MAX;PropertyIndex++)
	{
		try
		{
			GetMaterialInterface(0)->CompileProperty((EMaterialProperty)PropertyIndex, &Compiler);
		}
		catch(const TCHAR*)
		{
		}
	}

	StreamingTextures.Shrink();
}

void UMaterial::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void UMaterial::PostLoad()
{
	Super::PostLoad();

	if( !GIsUCC )
	{
		((FMaterialResource*)MaterialResources[0])->Update();
		((FMaterialResource*)MaterialResources[1])->Update();
		((FDefaultMaterialInstance*)DefaultMaterialInstance)->Update(this);
	}

	CollectStreamingTextures();
}

//
//	UMaterial::PostEditChange
//

void UMaterial::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	PersistentIds[0] = appCreateGuid();
	PersistentIds[1] = appCreateGuid();

	if( !GIsUCC )
	{
		((FMaterialResource*)MaterialResources[0])->Update();
		((FMaterialResource*)MaterialResources[1])->Update();
		((FDefaultMaterialInstance*)DefaultMaterialInstance)->Update(this);
	}

	CollectStreamingTextures();
}

//
//	UMaterial::Destroy
//

void UMaterial::Destroy()
{
	Super::Destroy();

	delete MaterialResources[0];
	delete MaterialResources[1];
	delete DefaultMaterialInstance;
}

//
//	UMaterialExpression::PostEditChange
//

void UMaterialExpression::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	check(GetOuterUMaterial());
	GetOuterUMaterial()->PostEditChange(NULL);
}

//
//	UMaterialExpression::GetOutputs
//

void UMaterialExpression::GetOutputs(TArray<FExpressionOutput>& Outputs) const
{
	new(Outputs) FExpressionOutput(0);
}

//
//	UMaterialExpression::GetWidth
//

INT UMaterialExpression::GetWidth() const
{
	return ME_STD_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

//
//	UMaterialExpression::GetHeight
//

INT UMaterialExpression::GetHeight() const
{
	TArray<FExpressionOutput>	Outputs;
	GetOutputs(Outputs);
	return Max(ME_CAPTION_HEIGHT + (Outputs.Num() * ME_STD_TAB_HEIGHT),ME_CAPTION_HEIGHT+ME_STD_THUMBNAIL_SZ+(ME_STD_BORDER*2));
}

//
//	UMaterialExpression::UsesLeftGutter
//

UBOOL UMaterialExpression::UsesLeftGutter() const
{
	return 0;
}

//
//	UMaterialExpression::UsesRightGutter
//

UBOOL UMaterialExpression::UsesRightGutter() const
{
	return 0;
}

//
//	UMaterialExpression::GetCaption
//

FString UMaterialExpression::GetCaption() const
{
	return TEXT("Expression");
}

//
//	UMaterialExpressionTextureSample::Compile
//

INT UMaterialExpressionTextureSample::Compile(FMaterialCompiler* Compiler)
{
	if(Texture)
		return Compiler->Add(
				Compiler->Mul(
					Compiler->TextureSample(
						Compiler->Texture(Texture->GetTexture()),
						Coordinates.Expression ? Coordinates.Compile(Compiler) : Compiler->TextureCoordinate(0)
						),
					Compiler->Constant4(
						Texture->UnpackMax.X - Texture->UnpackMin.X,
						Texture->UnpackMax.Y - Texture->UnpackMin.Y,
						Texture->UnpackMax.Z - Texture->UnpackMin.Z,
						Texture->UnpackMax.W - Texture->UnpackMin.W
						)
					),
				Compiler->Constant4(
					Texture->UnpackMin.X,
					Texture->UnpackMin.Y,
					Texture->UnpackMin.Z,
					Texture->UnpackMin.W
					)
				);
	else
		return Compiler->Errorf(TEXT("Missing TextureSample input texture"));
}

//
//	UMaterialExpressionTextureSample::GetOutputs
//

void UMaterialExpressionTextureSample::GetOutputs(TArray<FExpressionOutput>& Outputs) const
{
	new(Outputs) FExpressionOutput(1,1,1,1,0);
	new(Outputs) FExpressionOutput(1,1,0,0,0);
	new(Outputs) FExpressionOutput(1,0,1,0,0);
	new(Outputs) FExpressionOutput(1,0,0,1,0);
	new(Outputs) FExpressionOutput(1,0,0,0,1);
}

INT UMaterialExpressionTextureSample::GetWidth() const
{
	return ME_STD_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

//
//	UMaterialExpressionTextureSample::GetCaption
//

FString UMaterialExpressionTextureSample::GetCaption() const
{
	return TEXT("Texture Sample");
}

//
//	UMaterialExpressionAdd::Compile
//

INT UMaterialExpressionAdd::Compile(FMaterialCompiler* Compiler)
{
	if(!A.Expression)
		return Compiler->Errorf(TEXT("Missing Add input A"));
	else if(!B.Expression)
		return Compiler->Errorf(TEXT("Missing Add input B"));
	else
		return Compiler->Add(
			A.Compile(Compiler),
			B.Compile(Compiler)
			);
}

//
//	UMaterialExpressionAdd::GetCaption
//

FString UMaterialExpressionAdd::GetCaption() const
{
	return TEXT("Add");
}

//
//	UMaterialExpressionMultiply::Compile
//

INT UMaterialExpressionMultiply::Compile(FMaterialCompiler* Compiler)
{
	if(!A.Expression)
		return Compiler->Errorf(TEXT("Missing Multiply input A"));
	else if(!B.Expression)
		return Compiler->Errorf(TEXT("Missing Multiply input B"));
	else
		return Compiler->Mul(
			A.Compile(Compiler),
			B.Compile(Compiler)
			);
}

//
//	UMaterialExpressionMultiply::GetCaption
//

FString UMaterialExpressionMultiply::GetCaption() const
{
	return TEXT("Multiply");
}

//
//	UMaterialExpressionDivide::Compile
//

INT UMaterialExpressionDivide::Compile(FMaterialCompiler* Compiler)
{
	if(!A.Expression)
		return Compiler->Errorf(TEXT("Missing Divide input A"));
	else if(!B.Expression)
		return Compiler->Errorf(TEXT("Missing Divide input B"));
	else
		return Compiler->Div(
			A.Compile(Compiler),
			B.Compile(Compiler)
			);
}

//
//	UMaterialExpressionDivide::GetCaption
//

FString UMaterialExpressionDivide::GetCaption() const
{
	return TEXT("Divide");
}

//
//	UMaterialExpressionSubtract::Compile
//

INT UMaterialExpressionSubtract::Compile(FMaterialCompiler* Compiler)
{
	if(!A.Expression)
		return Compiler->Errorf(TEXT("Missing Subtract input A"));
	else if(!B.Expression)
		return Compiler->Errorf(TEXT("Missing Subtract input B"));
	else
		return Compiler->Sub(
			A.Compile(Compiler),
			B.Compile(Compiler)
			);
}

//
//	UMaterialExpressionSubtract::GetCaption
//

FString UMaterialExpressionSubtract::GetCaption() const
{
	return TEXT("Subtract");
}

//
//	UMaterialExpressionLinearInterpolate::Compile
//

INT UMaterialExpressionLinearInterpolate::Compile(FMaterialCompiler* Compiler)
{
	if(!A.Expression)
		return Compiler->Errorf(TEXT("Missing LinearInterpolate input A"));
	else if(!B.Expression)
		return Compiler->Errorf(TEXT("Missing LinearInterpolate input B"));
	else if(!Alpha.Expression)
		return Compiler->Errorf(TEXT("Missing LinearInterpolate input Alpha"));
	else
		return Compiler->Lerp(
			A.Compile(Compiler),
			B.Compile(Compiler),
			Alpha.Compile(Compiler)
			);
}

//
//	UMaterialExpressionLinearInterpolate::GetCaption
//

FString UMaterialExpressionLinearInterpolate::GetCaption() const
{
	return TEXT("Linear Interpolat");
}

//
//	UMaterialExpressionConstant::Compile
//

INT UMaterialExpressionConstant::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->Constant(R);
}

//
//	UMaterialExpressionConstant::GetCaption
//

FString UMaterialExpressionConstant::GetCaption() const
{
	return TEXT("Constant");
}

//
//	UMaterialExpressionConstant2Vector::Compile
//

INT UMaterialExpressionConstant2Vector::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->Constant2(R,G);
}

//
//	UMaterialExpressionConstant2Vector::GetCaption
//

FString UMaterialExpressionConstant2Vector::GetCaption() const
{
	return TEXT("Constant 2 Vector");
}

//
//	UMaterialExpressionConstant3Vector::Compile
//

INT UMaterialExpressionConstant3Vector::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->Constant3(R,G,B);
}

//
//	UMaterialExpressionConstant3Vector::GetCaption
//

FString UMaterialExpressionConstant3Vector::GetCaption() const
{
	return TEXT("Constant 3 Vector");
}

//
//	UMaterialExpressionConstant4Vector::Compile
//

INT UMaterialExpressionConstant4Vector::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->Constant4(R,G,B,A);
}

//
//	UMaterialExpressionConstant4Vector::GetCaption
//

FString UMaterialExpressionConstant4Vector::GetCaption() const
{
	return TEXT("Constant 4 Vector");
}

//
//	UMaterialExpressionClamp::Compile
//

INT UMaterialExpressionClamp::Compile(FMaterialCompiler* Compiler)
{
	if(!Input.Expression)
		return Compiler->Errorf(TEXT("Missing Clamp input"));
	else
	{
		if(!Min.Expression && !Max.Expression)
			return Input.Compile(Compiler);
		else if(!Min.Expression)
			return Compiler->Min(
				Input.Compile(Compiler),
				Max.Compile(Compiler)
				);
		else if(!Max.Expression)
			return Compiler->Max(
				Input.Compile(Compiler),
				Min.Compile(Compiler)
				);
		else
			return Compiler->Clamp(
				Input.Compile(Compiler),
				Min.Compile(Compiler),
				Max.Compile(Compiler)
				);
	}
}

//
//	UMaterialExpressionClamp::GetCaption
//

FString UMaterialExpressionClamp::GetCaption() const
{
	return TEXT("Clamp");
}

//
//	UMaterialExpressionTextureCoordinate::Compile
//

INT UMaterialExpressionTextureCoordinate::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->Mul(Compiler->TextureCoordinate(CoordinateIndex),Compiler->Constant(Tiling));
}

//
//	UMaterialExpressionTextureCoordinate::GetCaption
//

FString UMaterialExpressionTextureCoordinate::GetCaption() const
{
	return TEXT("Texture Coordinate");
}

//
//	UMaterialExpressionDotProduct::Compile
//

INT UMaterialExpressionDotProduct::Compile(FMaterialCompiler* Compiler)
{
	if(!A.Expression)
		return Compiler->Errorf(TEXT("Missing DotProduct input A"));
	else if(!B.Expression)
		return Compiler->Errorf(TEXT("Missing DotProduct input B"));
	else
		return Compiler->Dot(
			A.Compile(Compiler),
			B.Compile(Compiler)
			);
}

//
//	UMaterialExpressionDotProduct::GetCaption
//

FString UMaterialExpressionDotProduct::GetCaption() const
{
	return TEXT("Dot Product");
}

//
//	UMaterialExpressionCrossProduct::Compile
//

INT UMaterialExpressionCrossProduct::Compile(FMaterialCompiler* Compiler)
{
	if(!A.Expression)
		return Compiler->Errorf(TEXT("Missing CrossProduct input A"));
	else if(!B.Expression)
		return Compiler->Errorf(TEXT("Missing CrossProduct input B"));
	else
		return Compiler->Cross(
			A.Compile(Compiler),
			B.Compile(Compiler)
			);
}

//
//	UMaterialExpressionCrossProduct::GetCaption
//

FString UMaterialExpressionCrossProduct::GetCaption() const
{
	return TEXT("Cross Product");
}

//
//	UMaterialExpressionComponentMask::Compile
//

INT UMaterialExpressionComponentMask::Compile(FMaterialCompiler* Compiler)
{
	if(!Input.Expression)
		return Compiler->Errorf(TEXT("Missing ComponentMask input"));
	else
		return Compiler->ComponentMask(
			Input.Compile(Compiler),
			R,
			G,
			B,
			A
			);
}

//
//	UMaterialExpressionComponentMask::GetCaption
//

FString UMaterialExpressionComponentMask::GetCaption() const
{
	return TEXT("Component Mask");
}

//
//	UMaterialExpressionTime::Compile
//

INT UMaterialExpressionTime::Compile(FMaterialCompiler* Compiler)
{
	if(Absolute)
		return Compiler->SceneTime();
	else
		return Compiler->ObjectTime();
}

//
//	UMaterialExpressionTime::GetCaption
//

FString UMaterialExpressionTime::GetCaption() const
{
	return TEXT("Time");
}

//
//	UMaterialExpressionCameraVector::Compile
//

INT UMaterialExpressionCameraVector::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->CameraVector();
}

//
//	UMaterialExpressionCameraVector::GetCaption
//

FString UMaterialExpressionCameraVector::GetCaption() const
{
	return TEXT("Camera Vector");
}

//
//	UMaterialExpressionReflectionVector::Compile
//

INT UMaterialExpressionReflectionVector::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->ReflectionVector();
}

//
//	UMaterialExpressionReflectionVector::GetCaption
//

FString UMaterialExpressionReflectionVector::GetCaption() const
{
	return TEXT("Reflection Vector");
}

//
//	UMaterialExpressionPanner::Compile
//

INT UMaterialExpressionPanner::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->Add(
			Compiler->AppendVector(
				Compiler->PeriodicHint(Compiler->Mul(Time.Expression ? Time.Compile(Compiler) : Compiler->ObjectTime(),Compiler->Constant(SpeedX))),
				Compiler->PeriodicHint(Compiler->Mul(Time.Expression ? Time.Compile(Compiler) : Compiler->ObjectTime(),Compiler->Constant(SpeedY)))
				),
			Coordinate.Expression ? Coordinate.Compile(Compiler) : Compiler->TextureCoordinate(0)
			);
}

//
//	UMaterialExpressionPanner::GetCaption
//

FString UMaterialExpressionPanner::GetCaption() const
{
	return TEXT("Panner");
}

//
//	UMaterialExpressionRotator::Compile
//

INT UMaterialExpressionRotator::Compile(FMaterialCompiler* Compiler)
{
	INT	Cosine = Compiler->Cosine(Compiler->Mul(Time.Expression ? Time.Compile(Compiler) : Compiler->ObjectTime(),Compiler->Constant(Speed))),
		Sine = Compiler->Sine(Compiler->Mul(Time.Expression ? Time.Compile(Compiler) : Compiler->ObjectTime(),Compiler->Constant(Speed))),
		RowX = Compiler->AppendVector(Cosine,Compiler->Mul(Compiler->Constant(-1.0f),Sine)),
		RowY = Compiler->AppendVector(Sine,Cosine),
		Origin = Compiler->Constant2(CenterX,CenterY),
		BaseCoordinate = Coordinate.Expression ? Coordinate.Compile(Compiler) : Compiler->TextureCoordinate(0);

	if(Compiler->GetType(BaseCoordinate) == MCT_Float3)
		return Compiler->AppendVector(
				Compiler->Add(
					Compiler->AppendVector(
						Compiler->Dot(RowX,Compiler->Sub(Compiler->ComponentMask(BaseCoordinate,1,1,0,0),Origin)),
						Compiler->Dot(RowY,Compiler->Sub(Compiler->ComponentMask(BaseCoordinate,1,1,0,0),Origin))
						),
					Origin
					),
				Compiler->ComponentMask(BaseCoordinate,0,0,1,0)
				);
	else
		return Compiler->Add(
				Compiler->AppendVector(
					Compiler->Dot(RowX,Compiler->Sub(BaseCoordinate,Origin)),
					Compiler->Dot(RowY,Compiler->Sub(BaseCoordinate,Origin))
					),
				Origin
				);
}

//
//	UMaterialExpressionRotator::GetCaption
//

FString UMaterialExpressionRotator::GetCaption() const
{
	return TEXT("Rotator");
}

//
//	UMaterialExpressionSine::Compile
//

INT UMaterialExpressionSine::Compile(FMaterialCompiler* Compiler)
{
	if(!Input.Expression)
		return Compiler->Errorf(TEXT("Missing Sine input"));
	return Compiler->Sine(Period > 0.0f ? Compiler->Mul(Input.Compile(Compiler),Compiler->Constant(2.0f * (FLOAT)PI / Period)) : Input.Compile(Compiler));
}

//
//	UMaterialExpressionSine::GetCaption
//

FString UMaterialExpressionSine::GetCaption() const
{
	return TEXT("Sine");
}

//
//	UMaterialExpressionCosine::Compile
//

INT UMaterialExpressionCosine::Compile(FMaterialCompiler* Compiler)
{
	if(!Input.Expression)
		return Compiler->Errorf(TEXT("Missing Cosine input"));
	return Compiler->Cosine(Compiler->Mul(Input.Compile(Compiler),Period > 0.0f ? Compiler->Constant(2.0f * (FLOAT)PI / Period) : 0.0f));
}

//
//	UMaterialExpressionCosine::GetCaption
//

FString UMaterialExpressionCosine::GetCaption() const
{
	return TEXT("Cosine");
}

//
//	UMaterialExpressionBumpOffset::Compile
//

INT UMaterialExpressionBumpOffset::Compile(FMaterialCompiler* Compiler)
{
	if(!Height.Expression)
		return Compiler->Errorf(TEXT("Missing Height input"));

	return Compiler->Add(
			Compiler->Mul(
				Compiler->ComponentMask(Compiler->CameraVector(),1,1,0,0),
				Compiler->Add(
					Compiler->Mul(
						Compiler->Constant(HeightRatio),
						Compiler->ForceCast(Height.Compile(Compiler),MCT_Float1)
						),
					Compiler->Constant(-ReferencePlane * HeightRatio)
					)
				),
			Coordinate.Expression ? Coordinate.Compile(Compiler) : Compiler->TextureCoordinate(0)
			);
}

//
//	UMaterialExpressionBumpOffset::GetCaption
//

FString UMaterialExpressionBumpOffset::GetCaption() const
{
	return TEXT("BumpOffset");
}

//
//	UMaterialExpressionAppendVector::Compile
//

INT UMaterialExpressionAppendVector::Compile(FMaterialCompiler* Compiler)
{
	if(!A.Expression)
		return Compiler->Errorf(TEXT("Missing AppendVector input A"));
	else if(!B.Expression)
		return Compiler->Errorf(TEXT("Missing AppendVector input B"));
	else
	{
		return Compiler->AppendVector(
			A.Compile(Compiler),
			B.Compile(Compiler)
			);
	}
}

//
//	UMaterialExpressionAppendVector::GetCaption
//

FString UMaterialExpressionAppendVector::GetCaption() const
{
	return TEXT("AppendVector");
}

//
//	UMaterialExpressionFloor::Compile
//

INT UMaterialExpressionFloor::Compile(FMaterialCompiler* Compiler)
{
	if(!Input.Expression)
		return Compiler->Errorf(TEXT("Missing Floor input"));
	return Compiler->Floor(Input.Compile(Compiler));
}

//
//	UMaterialExpressionFloor::GetCaption
//

FString UMaterialExpressionFloor::GetCaption() const
{
	return TEXT("Floor");
}

//
//	UMaterialExpressionCeil::Compile
//

INT UMaterialExpressionCeil::Compile(FMaterialCompiler* Compiler)
{
	if(!Input.Expression)
		return Compiler->Errorf(TEXT("Missing Ceil input"));
	return Compiler->Ceil(Input.Compile(Compiler));
}

//
//	UMaterialExpressionCeil::GetCaption
//

FString UMaterialExpressionCeil::GetCaption() const
{
	return TEXT("Ceil");
}

//
//	UMaterialExpressionFrac::Compile
//

INT UMaterialExpressionFrac::Compile(FMaterialCompiler* Compiler)
{
	if(!Input.Expression)
		return Compiler->Errorf(TEXT("Missing Frac input"));
	return Compiler->Frac(Input.Compile(Compiler));
}

//
//	UMaterialExpressionFrac::GetCaption
//

FString UMaterialExpressionFrac::GetCaption() const
{
	return TEXT("Frac");
}

//
//	UMaterialExpressionDesaturation::Compile
//

INT UMaterialExpressionDesaturation::Compile(FMaterialCompiler* Compiler)
{
	if(!Input.Expression)
		return Compiler->Errorf(TEXT("Missing Desaturation input"));

	INT	Color = Input.Compile(Compiler),
		Grey = Compiler->Dot(Color,Compiler->Constant3(LuminanceFactors.R,LuminanceFactors.G,LuminanceFactors.B));

	if(Percent.Expression)
		return Compiler->Lerp(Color,Grey,Percent.Compile(Compiler));
	else
		return Grey;
}

//
//	UMaterialExpressionVectorParameter::Compile
//

INT UMaterialExpressionVectorParameter::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->VectorParameter(ParameterName);
}

//
//	UMaterialExpressionVectorParameter::GetOutputs
//

void UMaterialExpressionVectorParameter::GetOutputs(TArray<FExpressionOutput>& Outputs) const
{
	new(Outputs) FExpressionOutput(1,1,1,1,0);
	new(Outputs) FExpressionOutput(1,1,0,0,0);
	new(Outputs) FExpressionOutput(1,0,1,0,0);
	new(Outputs) FExpressionOutput(1,0,0,1,0);
	new(Outputs) FExpressionOutput(1,0,0,0,1);
}

//
//	UMaterialExpressionScalarParameter::Compile
//

INT UMaterialExpressionScalarParameter::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->ScalarParameter(ParameterName);
}

//
//	UMaterialExpressionNormalize::Compile
//

INT UMaterialExpressionNormalize::Compile(FMaterialCompiler* Compiler)
{
	if(!Vector.Expression)
		return Compiler->Errorf(TEXT("Missing Normalize input"));

	INT	V = Vector.Compile(Compiler);

	return Compiler->Div(V,Compiler->SquareRoot(Compiler->Dot(V,V)));
}

INT UMaterialExpressionVertexColor::Compile(FMaterialCompiler* Compiler)
{
	return Compiler->VertexColor();
}

void UMaterialExpressionVertexColor::GetOutputs(TArray<FExpressionOutput>& Outputs) const
{
	new(Outputs) FExpressionOutput(1,1,1,1,0);
	new(Outputs) FExpressionOutput(1,1,0,0,0);
	new(Outputs) FExpressionOutput(1,0,1,0,0);
	new(Outputs) FExpressionOutput(1,0,0,1,0);
	new(Outputs) FExpressionOutput(1,0,0,0,1);
}

FString UMaterialExpressionVertexColor::GetCaption() const
{
	return TEXT("Vertex Color");
}

//
//	MaterialExpressionParticleSubUV
//
INT UMaterialExpressionParticleSubUV::Compile(FMaterialCompiler* Compiler)
{
	if (Texture)
	{
		// Out	 = linear interpolate... using 2 sub-images of the texture
		// A	 = RGB sample texture with TexCoord0
		// B	 = RGB sample texture with TexCoord1
		// Alpha = x component of TexCoord2
		//
/***
		INT Lerp_A, Lerp_B, Lerp_Alpha;

		// Lerp unit 'alpha' comes from the 3rd texture set...
		Lerp_Alpha = 
			Compiler->ComponentMask(
				Compiler->TextureCoordinate(2), 
				1, 0, 0, 0
			);

		// Lerp A
		Lerp_A =
			Compiler->Add(
				Compiler->Mul(
					Compiler->TextureSample(
						Compiler->Texture(
							Texture->GetTexture()
							), 
						Compiler->TextureCoordinate(0)
					),
				Compiler->Constant4(
					Texture->UnpackMax.X - Texture->UnpackMin.X,
					Texture->UnpackMax.Y - Texture->UnpackMin.Y,
					Texture->UnpackMax.Z - Texture->UnpackMin.Z,
					Texture->UnpackMax.W - Texture->UnpackMin.W
					)
				),
				Compiler->Constant4(
					Texture->UnpackMin.X,
					Texture->UnpackMin.Y,
					Texture->UnpackMin.Z,
					Texture->UnpackMin.W
				)
			);

		Lerp_B = 
			Compiler->Add(
				Compiler->Mul(
					Compiler->TextureSample(
						Compiler->Texture(
							Texture->GetTexture()
							), 
						Compiler->TextureCoordinate(1)
					),
				Compiler->Constant4(
					Texture->UnpackMax.X - Texture->UnpackMin.X,
					Texture->UnpackMax.Y - Texture->UnpackMin.Y,
					Texture->UnpackMax.Z - Texture->UnpackMin.Z,
					Texture->UnpackMax.W - Texture->UnpackMin.W
					)
				),
				Compiler->Constant4(
					Texture->UnpackMin.X,
					Texture->UnpackMin.Y,
					Texture->UnpackMin.Z,
					Texture->UnpackMin.W
				)
			);


		return Compiler->Lerp(Lerp_A, Lerp_B, Lerp_Alpha);
***/
		return 
			Compiler->Lerp(
			// Lerp_A, 
			Compiler->Add(
				Compiler->Mul(
					Compiler->TextureSample(
						Compiler->Texture(
							Texture->GetTexture()
							), 
						Compiler->TextureCoordinate(0)
					),
				Compiler->Constant4(
					Texture->UnpackMax.X - Texture->UnpackMin.X,
					Texture->UnpackMax.Y - Texture->UnpackMin.Y,
					Texture->UnpackMax.Z - Texture->UnpackMin.Z,
					Texture->UnpackMax.W - Texture->UnpackMin.W
					)
				),
				Compiler->Constant4(
					Texture->UnpackMin.X,
					Texture->UnpackMin.Y,
					Texture->UnpackMin.Z,
					Texture->UnpackMin.W
				)
			),
			// Lerp_B, 
			Compiler->Add(
				Compiler->Mul(
					Compiler->TextureSample(
						Compiler->Texture(
							Texture->GetTexture()
							), 
						Compiler->TextureCoordinate(1)
					),
				Compiler->Constant4(
					Texture->UnpackMax.X - Texture->UnpackMin.X,
					Texture->UnpackMax.Y - Texture->UnpackMin.Y,
					Texture->UnpackMax.Z - Texture->UnpackMin.Z,
					Texture->UnpackMax.W - Texture->UnpackMin.W
					)
				),
				Compiler->Constant4(
					Texture->UnpackMin.X,
					Texture->UnpackMin.Y,
					Texture->UnpackMin.Z,
					Texture->UnpackMin.W
				)
			),
			// Lerp 'alpha' comes from the 3rd texture set...
			Compiler->ComponentMask(
				Compiler->TextureCoordinate(2), 
				1, 0, 0, 0
			)
		);

	}
	else
	{
		return Compiler->Errorf(TEXT("Missing ParticleSubUV input texture"));
	}
}

void UMaterialExpressionParticleSubUV::GetOutputs(TArray<FExpressionOutput>& Outputs) const
{
	new(Outputs) FExpressionOutput(1,1,1,1,0);
	new(Outputs) FExpressionOutput(1,1,0,0,0);
	new(Outputs) FExpressionOutput(1,0,1,0,0);
	new(Outputs) FExpressionOutput(1,0,0,1,0);
	new(Outputs) FExpressionOutput(1,0,0,0,1);
}

INT UMaterialExpressionParticleSubUV::GetWidth() const
{
	return ME_STD_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

FString UMaterialExpressionParticleSubUV::GetCaption() const
{
	return TEXT("Particle SubUV");
}
