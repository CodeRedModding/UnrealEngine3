/*=============================================================================
	UnActorComponent.cpp: Actor component implementation.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

IMPLEMENT_CLASS(UActorComponent);
IMPLEMENT_CLASS(ULightComponent);
IMPLEMENT_CLASS(UPointLightComponent);
IMPLEMENT_CLASS(USkyLightComponent);
IMPLEMENT_CLASS(UDirectionalLightComponent);
IMPLEMENT_CLASS(USpotLightComponent);
IMPLEMENT_CLASS(UPrimitiveComponent);
IMPLEMENT_CLASS(USpriteComponent);
IMPLEMENT_CLASS(UMeshComponent);
IMPLEMENT_CLASS(UStaticMeshComponent);
IMPLEMENT_CLASS(UBrushComponent);
IMPLEMENT_CLASS(UAudioComponent);
IMPLEMENT_CLASS(UCylinderComponent);
IMPLEMENT_CLASS(UArrowComponent);
IMPLEMENT_CLASS(UDrawSphereComponent);
IMPLEMENT_CLASS(UDrawFrustumComponent);
IMPLEMENT_CLASS(UCameraConeComponent);
IMPLEMENT_CLASS(UTransformComponent);
IMPLEMENT_CLASS(UWindPointSourceComponent);
IMPLEMENT_CLASS(UWindDirectionalSourceComponent);
IMPLEMENT_CLASS(UHeightFogComponent);

IMPLEMENT_CLASS(UPrimitiveComponentFactory);
IMPLEMENT_CLASS(UMeshComponentFactory);
IMPLEMENT_CLASS(UStaticMeshComponentFactory);

IMPLEMENT_CLASS(ULightFunction);

//
//	AActor::ClearComponents
//

void AActor::ClearComponents()
{
	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Components.Num();ComponentIndex++)
	{
		if(Components(ComponentIndex))
		{
			if(Components(ComponentIndex)->Initialized)
				Components(ComponentIndex)->Destroyed();
			Components(ComponentIndex)->Scene = NULL;
			Components(ComponentIndex)->Owner = NULL;
		}
	}
}

//
//	AActor::UpdateComponents
//

void AActor::UpdateComponents()
{
	if(!XLevel || bDeleteMe)
		return;

	const FMatrix&	ActorToWorld = LocalToWorld();

	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Components.Num();ComponentIndex++)
	{
		if(Components(ComponentIndex))
		{
			Components(ComponentIndex)->SetParentToWorld(ActorToWorld);

			if(!Components(ComponentIndex)->Initialized)
			{
				Components(ComponentIndex)->Scene = XLevel;
				Components(ComponentIndex)->Owner = this;
				if(Components(ComponentIndex)->IsValidComponent())
					Components(ComponentIndex)->Created();
			}
			else
				Components(ComponentIndex)->Update();
		}
	}
}

//
//	AActor::CacheLighting
//

void AActor::CacheLighting()
{
	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Components.Num();ComponentIndex++)
		if(Components(ComponentIndex))
			Components(ComponentIndex)->CacheLighting();
}

//
//	AActor::InvalidateLightingCache
//

void AActor::InvalidateLightingCache()
{
	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Components.Num();ComponentIndex++)
		if(Components(ComponentIndex))
			Components(ComponentIndex)->InvalidateLightingCache();
}

UBOOL AActor::ActorLineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	UBOOL Hit = 0;
	for(INT ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Components(ComponentIndex));
		if(Primitive && !Primitive->LineCheck(Result,End,Start,Extent,TraceFlags))
		{
			Hit = 1;
		}
	}
	return !Hit;
}

///////////////////////////////////////////////////////////////////////////////
// ACTOR COMPONENT
///////////////////////////////////////////////////////////////////////////////

void UActorComponent::Destroy()
{
	Super::Destroy();
	if(Initialized)
		appErrorf(TEXT("Actor component destroyed without being removed from the scene: %s"),*GetFullName());
}

//
//	UActorComponent::PreEditChange
//

void UActorComponent::PreEditChange()
{
	Super::PreEditChange();
	if(Initialized)
		Destroyed();
}

//
//	UActorComponent::PostEditChange
//

void UActorComponent::PostEditChange(UProperty* PropertyThatChanged)
{
	if(Scene && IsValidComponent())
		Created();
	Super::PostEditChange(PropertyThatChanged);
}

//
//	UActorComponent::Created
//

void UActorComponent::Created()
{
	Initialized = 1;
	check(Scene);
	check(IsValidComponent());
}

//
//	UActorComponent::Update
//

void UActorComponent::Update()
{
	check(Initialized);
}

//
//	UActorComponent::Destroyed
//

void UActorComponent::Destroyed()
{
	check(Initialized);
	Initialized = 0;
}

//
//	UActorComponent::BeginPlay
//

void UActorComponent::BeginPlay()
{
	check(Initialized);
}

//
//	UActorComponent::Tick
//

void UActorComponent::Tick(FLOAT DeltaTime)
{
	check(Initialized);
}

///////////////////////////////////////////////////////////////////////////////
// LIGHT COMPONENT
///////////////////////////////////////////////////////////////////////////////

UBOOL ULightComponent::HasStaticShadowing() const
{
	return !RequireDynamicShadows && (!Owner || Owner->HasStaticShadowing());
}

//
//	ULightComponent::SetParentToWorld
//

void ULightComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	LightToWorld = FMatrix(
						FPlane(+0,+0,+1,+0),
						FPlane(+0,+1,+0,+0),
						FPlane(+1,+0,+0,+0),
						FPlane(+0,+0,+0,+1)
						) *
					ParentToWorld;
	LightToWorld.RemoveScaling();
	WorldToLight = LightToWorld.Inverse();
}

//
//	ULightComponent::Created
//

void ULightComponent::Created()
{
	Super::Created();
	Scene->AddLight(this);
}

//
//	ULightComponent::Update
//

void ULightComponent::Update()
{
	Super::Update();
	Scene->RemoveLight(this);
	Scene->AddLight(this);
}

//
//	ULightComponent::Destroyed
//

void ULightComponent::Destroyed()
{
	Super::Destroyed();
	Scene->RemoveLight(this);
}

//
//	ULightComponent::InvalidateLightingCache
//

void ULightComponent::InvalidateLightingCache()
{
	for(TObjectIterator<UPrimitiveComponent> It;It;++It)
		It->InvalidateLightingCache(this);
}

///////////////////////////////////////////////////////////////////////////////
// DIRECTIONAL LIGHT COMPONENT
///////////////////////////////////////////////////////////////////////////////

//
//	UDirectionalLightComponent::GetPosition
//

FPlane UDirectionalLightComponent::GetPosition() const
{
	return FPlane(-GetDirection() * WORLD_MAX,0);
}

///////////////////////////////////////////////////////////////////////////////
// SKY LIGHT COMPONENT
///////////////////////////////////////////////////////////////////////////////

//
//	USkyLightComponent::GetPosition
//

FPlane USkyLightComponent::GetPosition() const
{
	return FPlane(0,0,1,0);
}

///////////////////////////////////////////////////////////////////////////////
// POINT LIGHT COMPONENT
///////////////////////////////////////////////////////////////////////////////

//
//	UPointLightComponent::AffectsSphere
//

UBOOL UPointLightComponent::AffectsSphere(const FSphere& Sphere)
{
	if((Sphere - GetOrigin()).SizeSquared() > Square(Radius + Sphere.W))
		return 0;

	return 1;
}

//
//	UPointLightComponent::GetPosition
//

FPlane UPointLightComponent::GetPosition() const
{
	return FPlane(LightToWorld.GetOrigin(),1);
}

//
//	UPointLightComponent::GetBoundingBox
//

FBox UPointLightComponent::GetBoundingBox() const
{
	return FBox(GetOrigin() - FVector(Radius,Radius,Radius),GetOrigin() + FVector(Radius,Radius,Radius));
}

///////////////////////////////////////////////////////////////////////////////
// SPOT LIGHT COMPONENT
///////////////////////////////////////////////////////////////////////////////

//
//	USpotLightComponent::AffectsSphere
//

UBOOL USpotLightComponent::AffectsSphere(const FSphere& Sphere)
{
	if(!Super::AffectsSphere(Sphere))
		return 0;

	FLOAT	Sin = appSin(OuterConeAngle * PI / 180.0f),
			Cos = appCos(OuterConeAngle * PI / 180.0f);

	FVector	U = GetOrigin() - (Sphere.W / Sin) * GetDirection(),
			D = (FVector)Sphere - U;
	FLOAT	dsqr = D | D,
			E = GetDirection() | D;
	if(E > 0.0f && E * E >= dsqr * Square(Cos))
	{
		D = (FVector)Sphere - GetOrigin();
		dsqr = D | D;
		E = -(GetDirection() | D);
		if(E > 0.0f && E * E >= dsqr * Square(Sin))
			return dsqr <= Square(Sphere.W);
		else
			return 1;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// PRIMITIVE COMPONENT
///////////////////////////////////////////////////////////////////////////////

FPrimitiveLightAttachment::FPrimitiveLightAttachment(ULightComponent* InLight,UPrimitiveComponent* InPrimitive):
	Light(InLight),
	DynamicPrimitiveLink(InPrimitive),
	StaticPrimitiveLink(NULL)
{
	DynamicPrimitiveLink.Link(Light->FirstDynamicPrimitiveLink);
}

FPrimitiveLightAttachment::FPrimitiveLightAttachment(ULightComponent* InLight,FStaticLightPrimitive* InStaticPrimitive):
	Light(InLight),
	DynamicPrimitiveLink(NULL),
	StaticPrimitiveLink(InStaticPrimitive)
{
	StaticPrimitiveLink.Link(Light->FirstStaticPrimitiveLink);
}

FPrimitiveLightAttachment::~FPrimitiveLightAttachment()
{
	if(*DynamicPrimitiveLink)
	{
		DynamicPrimitiveLink.Unlink();
	}
	else if(*StaticPrimitiveLink)
	{
		StaticPrimitiveLink.Unlink();
	}
	else
	{
		appErrorf(TEXT("Detaching light which isn't either static or dynamic: %s"),*Light->GetPathName());
	}
}

//
//	FPrimitiveViewInterface::RenderHitTest
//

void FPrimitiveViewInterface::RenderHitTest(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if(GetPrimitive()->Owner)
		PRI->SetHitProxy(new HActor(GetPrimitive()->Owner));
	else
		PRI->SetHitProxy(NULL);

	Render(Context,PRI);

	PRI->SetHitProxy(NULL);
}

//
//	FPrimitiveViewInterface::RenderBackgroundHitTest
//

void FPrimitiveViewInterface::RenderBackgroundHitTest(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if(GetPrimitive()->Owner)
		PRI->SetHitProxy(new HActor(GetPrimitive()->Owner));
	else
		PRI->SetHitProxy(NULL);

	RenderBackground(Context,PRI);

	PRI->SetHitProxy(NULL);
}

//
//	FPrimitiveViewInterface::RenderForegroundHitTest
//

void FPrimitiveViewInterface::RenderForegroundHitTest(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if(GetPrimitive()->Owner)
		PRI->SetHitProxy(new HActor(GetPrimitive()->Owner));
	else
		PRI->SetHitProxy(NULL);

	RenderForeground(Context,PRI);

	PRI->SetHitProxy(NULL);
}

UBOOL UPrimitiveComponent::HasStaticShadowing() const
{
	return !Owner || Owner->HasStaticShadowing();
}

/** 
 *	Indicates whether this PrimitiveComponent should be considered for collision, inserted into Octree for collision purposes etc.
 *	Basically looks at CollideActors, and Owner's bCollideActors (if present).
 */
UBOOL UPrimitiveComponent::ShouldCollide() const
{
	return CollideActors && (!Owner || Owner->bCollideActors); 
}


//
//	UPrimitiveComponent::GetCachedShadowVolume
//

FCachedShadowVolume* UPrimitiveComponent::GetCachedShadowVolume(ULightComponent* Light)
{
	for(UINT CacheIndex = 0;CacheIndex < (UINT)CachedShadowVolumes.Num();CacheIndex++)
		if(CachedShadowVolumes(CacheIndex)->Light == Light)
			return CachedShadowVolumes(CacheIndex);
	return NULL;
}

//
//	UPrimitiveComponent::AttachLight
//

void UPrimitiveComponent::AttachLight(ULightComponent* Light)
{
	check(Initialized);

	// Check if any static lighting is cached.

	FStaticLightPrimitive* StaticLightingPrimitive = NULL;
	EStaticLightingAvailability	StaticLightingAvailability = GetStaticLightPrimitive(Light,StaticLightingPrimitive);

	if(StaticLightingAvailability == SLA_Available)
	{
		// Attach to the light as a static primitive.
		new(Lights) FPrimitiveLightAttachment(Light,StaticLightingPrimitive);
	}
	else if(StaticLightingAvailability == SLA_Unavailable)
	{
		// Attach to the light as a dynamic primitive.
		new(Lights) FPrimitiveLightAttachment(Light,this);
	}
	else if(StaticLightingAvailability == SLA_Shadowed)
	{
		// This primitive is entirely shadowed from the light, ignore it.
	}
}

//
//	UPrimitiveComponent::DetachLight
//

void UPrimitiveComponent::DetachLight(ULightComponent* Light)
{
	check(Initialized);

	// Free the cached shadow volume for this light.

	FCachedShadowVolume*	CachedShadowVolume = GetCachedShadowVolume(Light);
	if(CachedShadowVolume)
	{
		CachedShadowVolumes.RemoveItem(CachedShadowVolume);
		delete CachedShadowVolume;
	}

	// Delete the light attachment object.  This removes the primitive from the light's linked list of attachments as well.

	for(INT LightIndex = 0;LightIndex < Lights.Num();LightIndex++)
	{
		if(Lights(LightIndex).GetLight() == Light)
		{
			Lights.Remove(LightIndex);
			return;
		}
	}
}

//
//	UPrimitiveComponent::Visible
//

UBOOL UPrimitiveComponent::Visible(FSceneView* View)
{
	if(View->ShowFlags & SHOW_Editor)
	{
		if(HiddenEditor || (Owner && Owner->IsHiddenEd()))
			return 0;
	}
	else
	{
		if(HiddenGame || (Owner && Owner->bHidden))
			return 0;

		if(bOnlyOwnerSee && (!Owner || !Owner->IsOwnedBy(View->ViewActor)))
			return 0;

		if(bOwnerNoSee && Owner && Owner->IsOwnedBy(View->ViewActor))
			return 0;
	}

	return 1;

}

//
//	UPrimitiveComponent::CreationTime
//

FLOAT UPrimitiveComponent::CreationTime()
{
	if(Owner)
		return Owner->CreationTime;
	else
		return 0.0f;

}

//
//	UPrimitiveComponent::SelectedColor
//

FColor UPrimitiveComponent::SelectedColor(const FSceneContext& Context,FColor BaseColor) const
{
    if((Context.View->ShowFlags & SHOW_Selection) && Owner && GSelectionTools.IsSelected( Owner ) )
		return BaseColor;
	else
		return FColor(BaseColor.Plane() * 0.5f);
}

//
//	UPrimitiveComponent::SelectedMaterial
//

FMaterial* UPrimitiveComponent::SelectedMaterial(const FSceneContext& Context,UMaterialInstance* Material) const
{
	return Material->GetMaterialInterface((Context.View->ShowFlags & SHOW_Selection) && Owner && GSelectionTools.IsSelected( Owner ) );
}

//
//	UPrimitiveComponent::UpdateBounds
//

void UPrimitiveComponent::UpdateBounds()
{
	Bounds.Origin = FVector(0,0,0);
	Bounds.BoxExtent = FVector(HALF_WORLD_MAX,HALF_WORLD_MAX,HALF_WORLD_MAX);
	Bounds.SphereRadius = appSqrt(3.0f * Square(HALF_WORLD_MAX));
}

//
//	UPrimitiveComponent::GetZoneMask
//

void UPrimitiveComponent::GetZoneMask(UModel* Model)
{
	// Determine the set of zones the primitive is in by filtering its bounds through the BSP tree.
	ZoneMask = Model->GetZoneMask(Bounds);
}

//
//	UPrimitiveComponent::SetParentToWorld
//

void UPrimitiveComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	LocalToWorld = ParentToWorld;
	LocalToWorldDeterminant = ParentToWorld.Determinant();

}

//
//	UPrimitiveComponent::Created
//

void UPrimitiveComponent::Created()
{
	Super::Created();

	UpdateBounds();

	// CollideActors must be true on the component and the Owner actor (or there is no owner actor).
	if( GIsEditor || !HiddenGame || ShouldCollide() )
	{
		Scene->AddPrimitive(this);

		if(ShadowParent)
		{
			NextShadowChild = ShadowParent->FirstShadowChild;
			ShadowParent->FirstShadowChild = this;
		}
	}
}

//
//	UPrimitiveComponent::Update
//

void UPrimitiveComponent::Update()
{
	Super::Update();

	UpdateBounds();

	if( GIsEditor || !HiddenGame || ShouldCollide() )
	{
		Scene->RemovePrimitive(this);
		Scene->AddPrimitive(this);
	}
}

//
//	UPrimitiveComponent::Destroyed
//

void UPrimitiveComponent::Destroyed()
{
	if(GIsEditor || !HiddenGame || ShouldCollide() )
	{
		Scene->RemovePrimitive(this);
	}

	Super::Destroyed();

	if(ShadowParent && ( GIsEditor || !HiddenGame || ShouldCollide() ))
	{
		UPrimitiveComponent** ShadowChildPtr = &ShadowParent->FirstShadowChild;
		while(*ShadowChildPtr && *ShadowChildPtr != this)
			ShadowChildPtr = &(*ShadowChildPtr)->NextShadowChild;
		*ShadowChildPtr = NextShadowChild;
		NextShadowChild = NULL;
	}
}

//
//	UPrimitiveComponent::Serialize
//

void UPrimitiveComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!Ar.IsSaving() && !Ar.IsLoading())
		Ar << FirstShadowChild << NextShadowChild << BodyInstance;
}

//
//	UPrimitiveComponent::Destroy
//

void UPrimitiveComponent::Destroy()
{
	if(BodyInstance)
	{
		BodyInstance->TermBody();
		delete BodyInstance;
		BodyInstance = NULL;
	}

	Super::Destroy();
}

UBOOL UPrimitiveComponent::NeedsLoadForClient() const
{
	if(HiddenGame && !ShouldCollide() && !AlwaysLoadOnClient)
	{
		return 0;
	}
	else
	{
		return Super::NeedsLoadForClient();
	}
}

UBOOL UPrimitiveComponent::NeedsLoadForServer() const
{
	if(!ShouldCollide() && !AlwaysLoadOnServer)
	{
		return 0;
	}
	else
	{
		return Super::NeedsLoadForServer();
	}
}

void UPrimitiveComponent::execSetHidden(FFrame& Stack,RESULT_DECL)
{
	P_GET_UBOOL(NewHidden);
	P_FINISH;

	FComponentRecreateContext RecreateContext(this);
	HiddenGame = NewHidden;
}

IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetHidden);

///////////////////////////////////////////////////////////////////////////////
// MESH COMPONENT
///////////////////////////////////////////////////////////////////////////////

//
//	UMeshComponent::GetMaterial
//

UMaterialInstance* UMeshComponent::GetMaterial(INT MaterialIndex) const
{
	if(MaterialIndex < Materials.Num() && Materials(MaterialIndex))
		return Materials(MaterialIndex);
	else
		return GEngine->DefaultMaterial;
}

//
//	UMeshComponent::execGetMaterial
//

void UMeshComponent::execGetMaterial(FFrame& Stack,RESULT_DECL)
{
	P_GET_INT(SkinIndex);
	P_FINISH;

	*(UMaterialInstance**)Result = GetMaterial(SkinIndex);
}

IMPLEMENT_FUNCTION(UMeshComponent,INDEX_NONE,execGetMaterial);

//
//	UMeshComponent::execSetMaterial
//

void UMeshComponent::execSetMaterial(FFrame& Stack,RESULT_DECL)
{
	P_GET_INT(SkinIndex);
	P_GET_OBJECT(UMaterialInstance,Material);
	P_FINISH;

	if(Materials.Num() <= SkinIndex)
		Materials.AddZeroed(SkinIndex + 1 - Materials.Num());

	Materials(SkinIndex) = Material;
}

IMPLEMENT_FUNCTION(UMeshComponent,INDEX_NONE,execSetMaterial);

///////////////////////////////////////////////////////////////////////////////
// SPRITE COMPONENT
///////////////////////////////////////////////////////////////////////////////

//
//	USpriteComponent::Render
//

void USpriteComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if(Sprite)
	{
		FLOAT	Scale = Owner ? Owner->DrawScale : 1.0f;
		PRI->DrawSprite(LocalToWorld.GetOrigin(),Sprite->SizeX * Scale,Sprite->SizeY * Scale,Sprite,Owner && GSelectionTools.IsSelected( Owner ) ? FColor(128,230,128) : FColor(255,255,255));
	}

}

//
//	USpriteComponent::UpdateBounds
//

void USpriteComponent::UpdateBounds()
{
	FLOAT	Scale = (Owner ? Owner->DrawScale : 1.0f) * (Sprite ? (FLOAT)Max(Sprite->SizeX,Sprite->SizeY) : 1.0f);
	Bounds = FBoxSphereBounds(LocalToWorld.GetOrigin(),FVector(Scale,Scale,Scale),appSqrt(3.0f * Square(Scale)));
}

///////////////////////////////////////////////////////////////////////////////
// CYLINDER COMPONENT
///////////////////////////////////////////////////////////////////////////////

//
//	UCylinderComponent::Render
//

void UCylinderComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if(Context.View->ShowFlags & SHOW_Collision)
		PRI->DrawWireCylinder(LocalToWorld.GetOrigin(),FVector(1,0,0),FVector(0,1,0),FVector(0,0,1),GEngine->C_ScaleBoxHi,CollisionRadius,CollisionHeight,16);
}

//
//	UCylinderComponent::UpdateBounds
//

void UCylinderComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds(LocalToWorld.GetOrigin(),FVector(CollisionRadius,CollisionRadius,CollisionHeight),Bounds.BoxExtent.Size());
}

//
//	UCylinderComponent::PointCheck
//

UBOOL UCylinderComponent::PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent)
{
	if (	Owner
		&&	Square(Owner->Location.Z-Location.Z)                                      < Square(CollisionHeight+Extent.Z)
		&&	Square(Owner->Location.X-Location.X)+Square(Owner->Location.Y-Location.Y) < Square(CollisionRadius+Extent.X) )
	{
		// Hit.
		Result.Actor    = Owner;
		Result.Normal   = (Location - Owner->Location).SafeNormal();
		if     ( Result.Normal.Z < -0.5 ) Result.Location = FVector( Location.X, Location.Y, Owner->Location.Z - Extent.Z);
		else if( Result.Normal.Z > +0.5 ) Result.Location = FVector( Location.X, Location.Y, Owner->Location.Z - Extent.Z);
		else                              Result.Location = (Owner->Location - Extent.X * (Result.Normal*FVector(1,1,0)).SafeNormal()) + FVector(0,0,Location.Z);
		return 0;
	}
	else return 1;

}

//
//	UCylinderComponent::LineCheck
//

UBOOL UCylinderComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	Result.Time = 1.f;
	if( !Owner )
		return 1;

	// Treat this actor as a cylinder.
	FVector CylExtent( CollisionRadius, CollisionRadius, CollisionHeight );
	FVector NetExtent = Extent + CylExtent;

	// Quick X reject.
	FLOAT MaxX = Owner->Location.X + NetExtent.X;
	if( Start.X>MaxX && End.X>MaxX )
		return 1;
	FLOAT MinX = Owner->Location.X - NetExtent.X;
	if( Start.X<MinX && End.X<MinX )
		return 1;

	// Quick Y reject.
	FLOAT MaxY = Owner->Location.Y + NetExtent.Y;
	if( Start.Y>MaxY && End.Y>MaxY )
		return 1;
	FLOAT MinY = Owner->Location.Y - NetExtent.Y;
	if( Start.Y<MinY && End.Y<MinY )
		return 1;

	// Quick Z reject.
	FLOAT TopZ = Owner->Location.Z + NetExtent.Z;
	if( Start.Z>TopZ && End.Z>TopZ )
		return 1;
	FLOAT BotZ = Owner->Location.Z - NetExtent.Z;
	if( Start.Z<BotZ && End.Z<BotZ )
		return 1;

	// Clip to top of cylinder.
	FLOAT T0=0.f, T1=1.f;
	if( Start.Z>TopZ && End.Z<TopZ )
	{
		FLOAT T = (TopZ - Start.Z)/(End.Z - Start.Z);
		if( T > T0 )
		{
			T0 = ::Max(T0,T);
			Result.Normal = FVector(0,0,1);
		}
	}
	else if( Start.Z<TopZ && End.Z>TopZ )
		T1 = ::Min( T1, (TopZ - Start.Z)/(End.Z - Start.Z) );

	// Clip to bottom of cylinder.
	if( Start.Z<BotZ && End.Z>BotZ )
	{
		FLOAT T = (BotZ - Start.Z)/(End.Z - Start.Z);
		if( T > T0 )
		{
			T0 = ::Max(T0,T);
			Result.Normal = FVector(0,0,-1);
		}
	}
	else if( Start.Z>BotZ && End.Z<BotZ )
		T1 = ::Min( T1, (BotZ - Start.Z)/(End.Z - Start.Z) );

	// Reject.
	if( T0 >= T1 )
		return 1;

	// Test setup.
	FLOAT   Kx        = Start.X - Owner->Location.X;
	FLOAT   Ky        = Start.Y - Owner->Location.Y;

	// 2D circle clip about origin.
	FLOAT   Vx        = End.X - Start.X;
	FLOAT   Vy        = End.Y - Start.Y;
	FLOAT   A         = Vx*Vx + Vy*Vy;
	FLOAT   B         = 2.f * (Kx*Vx + Ky*Vy);
	FLOAT   C         = Kx*Kx + Ky*Ky - Square(NetExtent.X);
	FLOAT   Discrim   = B*B - 4.f*A*C;

	// If already inside sphere, oppose further movement inward.
	if( C<Square(1.f) && Start.Z>BotZ && Start.Z<TopZ )
	{
		FLOAT Dir = ((End-Start)*FVector(1,1,0)) | (Start-Owner->Location);
		if( Dir < -0.1f )
		{
			Result.Time      = 0.f;
			Result.Location  = Start;
			Result.Normal    = ((Start-Owner->Location)*FVector(1,1,0)).SafeNormal();
			Result.Actor     = Owner;
			Result.Component = this;
			Result.Material = NULL;
			return 0;
		}
		else return 1;
	}

	// No intersection if discriminant is negative.
	if( Discrim < 0 )
		return 1;

	// Unstable intersection if velocity is tiny.
	if( A < Square(0.0001f) )
	{
		// Outside.
		if( C > 0 )
			return 1;
	}
	else
	{
		// Compute intersection times.
		Discrim   = appSqrt(Discrim);
		FLOAT R2A = 0.5/A;
		T1        = ::Min( T1, +(Discrim-B) * R2A );
		FLOAT T   = -(Discrim+B) * R2A;
		if( T > T0 )
		{
			T0 = T;
			Result.Normal   = (Start + (End-Start)*T0 - Owner->Location);
			Result.Normal.Z = 0;
			Result.Normal.Normalize();
		}
		if( T0 >= T1 )
			return 1;
	}
	Result.Time      = Clamp(T0-0.001f,0.f,1.f);
	Result.Location  = Start + (End-Start) * Result.Time;
	Result.Actor     = Owner;
	Result.Component = this;
	return 0;

}

//
//	UCylinderComponent::SetSize
//

void UCylinderComponent::SetSize(FLOAT NewHeight, FLOAT NewRadius)
{
	FComponentRecreateContext RecreateContext(this);
	CollisionHeight = NewHeight;
	CollisionRadius = NewRadius;
}

//
//	UCylinderComponent::execSetSize
//

void UCylinderComponent::execSetSize( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(NewHeight);
	P_GET_FLOAT(NewRadius);
	P_FINISH;

	SetSize(NewHeight, NewRadius);

}
IMPLEMENT_FUNCTION(UCylinderComponent,INDEX_NONE,execSetSize);

///////////////////////////////////////////////////////////////////////////////
// ARROW COMPONENT
///////////////////////////////////////////////////////////////////////////////

#define ARROW_SCALE	16.0f

//
//	UArrowComponent::Render
//

void UArrowComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	PRI->DrawDirectionalArrow(FScaleMatrix(FVector(ARROW_SCALE,ARROW_SCALE,ARROW_SCALE)) * LocalToWorld,ArrowColor,ArrowSize * 3.0f);
}

//
//	UArrowComponent::UpdateBounds
//

void UArrowComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds(FBox(FVector(0,-ARROW_SCALE,-ARROW_SCALE),FVector(ArrowSize * ARROW_SCALE * 3.0f,ARROW_SCALE,ARROW_SCALE))).TransformBy(LocalToWorld);
}


///////////////////////////////////////////////////////////////////////////////
// SPHERE COMPONENT
///////////////////////////////////////////////////////////////////////////////

//
//	UDrawSphereComponent::Render
//

void UDrawSphereComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if(bDrawWireSphere)
	{
		PRI->DrawCircle(LocalToWorld.GetOrigin(), LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1), SphereColor, SphereRadius, SphereSides);
		PRI->DrawCircle(LocalToWorld.GetOrigin(), LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(2), SphereColor, SphereRadius, SphereSides);
		PRI->DrawCircle(LocalToWorld.GetOrigin(), LocalToWorld.GetAxis(1), LocalToWorld.GetAxis(2), SphereColor, SphereRadius, SphereSides);
	}

	if(bDrawLitSphere && SphereMaterial)
	{
		PRI->DrawSphere(LocalToWorld.GetOrigin(), FVector(SphereRadius), SphereSides, SphereSides/2, SelectedMaterial(Context,SphereMaterial), SphereMaterial->GetInstanceInterface() );
	}
}

//
//	UDrawSphereComponent::UpdateBounds
//

void UDrawSphereComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds( FVector(0,0,0), FVector(SphereRadius), SphereRadius ).TransformBy(LocalToWorld);
}

///////////////////////////////////////////////////////////////////////////////
// FRUSTUM COMPONENT
///////////////////////////////////////////////////////////////////////////////

//
//	UDrawFrustumComponent::Render
//

void UDrawFrustumComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if(Context.View->ShowFlags & SHOW_CamFrustums)
	{
		// FOVAngle controls the horizontal angle.
		FLOAT HozHalfAngle = (FrustumAngle) * ((FLOAT)PI/360.f);
		FLOAT VertHalfAngle = (FrustumAngle/FrustumAspectRatio) * ((FLOAT)PI/360.f);

		FLOAT HozTan = appTan(HozHalfAngle);
		FLOAT VertTan = appTan(VertHalfAngle);

		FVector Direction(1,0,0);
		FVector LeftVector(0,1,0);
		FVector UpVector(0,0,1);

		FVector Verts[8];

		Verts[0] = (Direction * FrustumStartDist) + (UpVector * FrustumStartDist * VertTan) + (LeftVector * FrustumStartDist * HozTan);
		Verts[1] = (Direction * FrustumStartDist) + (UpVector * FrustumStartDist * VertTan) - (LeftVector * FrustumStartDist * HozTan);
		Verts[2] = (Direction * FrustumStartDist) - (UpVector * FrustumStartDist * VertTan) - (LeftVector * FrustumStartDist * HozTan);
		Verts[3] = (Direction * FrustumStartDist) - (UpVector * FrustumStartDist * VertTan) + (LeftVector * FrustumStartDist * HozTan);

		Verts[4] = (Direction * FrustumEndDist) + (UpVector * FrustumEndDist * VertTan) + (LeftVector * FrustumEndDist * HozTan);
		Verts[5] = (Direction * FrustumEndDist) + (UpVector * FrustumEndDist * VertTan) - (LeftVector * FrustumEndDist * HozTan);
		Verts[6] = (Direction * FrustumEndDist) - (UpVector * FrustumEndDist * VertTan) - (LeftVector * FrustumEndDist * HozTan);
		Verts[7] = (Direction * FrustumEndDist) - (UpVector * FrustumEndDist * VertTan) + (LeftVector * FrustumEndDist * HozTan);

		for( INT x = 0 ; x < 8 ; ++x )
		{
			Verts[x] = LocalToWorld.TransformFVector( Verts[x] );
		}

		PRI->DrawLine( Verts[0], Verts[1], FrustumColor );
		PRI->DrawLine( Verts[1], Verts[2], FrustumColor );
		PRI->DrawLine( Verts[2], Verts[3], FrustumColor );
		PRI->DrawLine( Verts[3], Verts[0], FrustumColor );

		PRI->DrawLine( Verts[4], Verts[5], FrustumColor );
		PRI->DrawLine( Verts[5], Verts[6], FrustumColor );
		PRI->DrawLine( Verts[6], Verts[7], FrustumColor );
		PRI->DrawLine( Verts[7], Verts[4], FrustumColor );

		PRI->DrawLine( Verts[0], Verts[4], FrustumColor );
		PRI->DrawLine( Verts[1], Verts[5], FrustumColor );
		PRI->DrawLine( Verts[2], Verts[6], FrustumColor );
		PRI->DrawLine( Verts[3], Verts[7], FrustumColor );
	}
}

//
//	UDrawFrustumComponent::UpdateBounds
//

void UDrawFrustumComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds( LocalToWorld.TransformFVector(FVector(0,0,0)), FVector(FrustumEndDist), FrustumEndDist );
}

///////////////////////////////////////////////////////////////////////////////
// CAMERA CONE COMPONENT
///////////////////////////////////////////////////////////////////////////////

//
//	UCameraConeComponent::Render
//

void UCameraConeComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if( !GSelectionTools.IsSelected( Owner ) )
		return;

	// Camera View Cone
	FVector Direction(1,0,0);
	FVector UpVector(0,1,0);
	FVector LeftVector(0,0,1);

	FVector Verts[8];

	Verts[0] = (Direction * 24) + (32 * (UpVector + LeftVector).SafeNormal());
	Verts[1] = (Direction * 24) + (32 * (UpVector - LeftVector).SafeNormal());
	Verts[2] = (Direction * 24) + (32 * (-UpVector - LeftVector).SafeNormal());
	Verts[3] = (Direction * 24) + (32 * (-UpVector + LeftVector).SafeNormal());

	Verts[4] = (Direction * 128) + (64 * (UpVector + LeftVector).SafeNormal());
	Verts[5] = (Direction * 128) + (64 * (UpVector - LeftVector).SafeNormal());
	Verts[6] = (Direction * 128) + (64 * (-UpVector - LeftVector).SafeNormal());
	Verts[7] = (Direction * 128) + (64 * (-UpVector + LeftVector).SafeNormal());

	for( INT x = 0 ; x < 8 ; ++x )
		Verts[x] = LocalToWorld.TransformFVector( Verts[x] );

	PRI->DrawLine( Verts[0], Verts[1], FColor(150,200,255) );
	PRI->DrawLine( Verts[1], Verts[2], FColor(150,200,255) );
	PRI->DrawLine( Verts[2], Verts[3], FColor(150,200,255) );
	PRI->DrawLine( Verts[3], Verts[0], FColor(150,200,255) );

	PRI->DrawLine( Verts[4], Verts[5], FColor(150,200,255) );
	PRI->DrawLine( Verts[5], Verts[6], FColor(150,200,255) );
	PRI->DrawLine( Verts[6], Verts[7], FColor(150,200,255) );
	PRI->DrawLine( Verts[7], Verts[4], FColor(150,200,255) );

	PRI->DrawLine( Verts[0], Verts[4], FColor(150,200,255) );
	PRI->DrawLine( Verts[1], Verts[5], FColor(150,200,255) );
	PRI->DrawLine( Verts[2], Verts[6], FColor(150,200,255) );
	PRI->DrawLine( Verts[3], Verts[7], FColor(150,200,255) );

}

//
//	UCameraConeComponent::UpdateBounds
//

void UCameraConeComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds(LocalToWorld.TransformFVector(FVector(0,0,0)),FVector(128,128,128),128);
}

///////////////////////////////////////////////////////////////////////////////
// TRANSFORM COMPONENT
///////////////////////////////////////////////////////////////////////////////

void UTransformComponent::SetTransformedToWorld()
{
	check(TransformedComponent);
	check(TransformedComponent->IsValidComponent());

	FMatrix LocalToWorld = CachedParentToWorld;

	if(AbsoluteTranslation)
		LocalToWorld.M[3][0] = LocalToWorld.M[3][1] = LocalToWorld.M[3][2] = 0.0f;

	if(AbsoluteRotation || AbsoluteScale)
	{
		FVector	X(LocalToWorld.M[0][0],LocalToWorld.M[1][0],LocalToWorld.M[2][0]),
				Y(LocalToWorld.M[0][1],LocalToWorld.M[1][1],LocalToWorld.M[2][1]),
				Z(LocalToWorld.M[0][2],LocalToWorld.M[1][2],LocalToWorld.M[2][2]);

		if(AbsoluteScale)
		{
			X.Normalize();
			Y.Normalize();
			Z.Normalize();
		}

		if(AbsoluteRotation)
		{
			X = FVector(X.Size(),0,0);
			Y = FVector(0,Y.Size(),0);
			Z = FVector(0,0,Z.Size());
		}

		LocalToWorld.M[0][0] = X.X;
		LocalToWorld.M[1][0] = X.Y;
		LocalToWorld.M[2][0] = X.Z;
		LocalToWorld.M[0][1] = Y.X;
		LocalToWorld.M[1][1] = Y.Y;
		LocalToWorld.M[2][1] = Y.Z;
		LocalToWorld.M[0][2] = Z.X;
		LocalToWorld.M[1][2] = Z.Y;
		LocalToWorld.M[2][2] = Z.Z;
	}

	TransformedComponent->SetParentToWorld(FScaleMatrix(Scale * Scale3D) * FRotationMatrix(Rotation) * FTranslationMatrix(Translation) * LocalToWorld);
}

//
//	UTransformComponent::SetParentToWorld
//

void UTransformComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	CachedParentToWorld = ParentToWorld;
}

//
//	UTransformedComponent::Created
//

void UTransformComponent::Created()
{
	Super::Created();

	TransformedComponent->Scene = Scene;
	TransformedComponent->Owner = Owner;
	if(TransformedComponent->IsValidComponent())
	{
		if(TransformedComponent->Initialized)
			appErrorf(TEXT("Target %s of TransformComponent %s is already initialized.  Is it in the actor's component array as well?"),*TransformedComponent->GetPathName(),*GetPathName());
		SetTransformedToWorld();
		TransformedComponent->Created();
	}
}

//
//	UTransformedComponent::Update
//

void UTransformComponent::Update()
{
	Super::Update();

	if(TransformedComponent->Initialized)
	{
		SetTransformedToWorld();
		TransformedComponent->Update();
	}
}

//
//	UTransformedComponent::Destroyed
//

void UTransformComponent::Destroyed()
{
	Super::Destroyed();

	if(TransformedComponent->Initialized)
		TransformedComponent->Destroyed();
	TransformedComponent->Scene = NULL;
	TransformedComponent->Owner = NULL;

}

//
//	UTransformedComponent::Tick
//

void UTransformComponent::Tick(FLOAT DeltaTime)
{
	Super::Tick(DeltaTime);

	if(TransformedComponent)
		TransformedComponent->Tick(DeltaTime);
}

//
//	UTransformComponent::execSetTransformedComponent
//

void UTransformComponent::execSetTransformedComponent(FFrame& Stack,RESULT_DECL)
{
	P_GET_OBJECT(UActorComponent,NewTransformedComponent);
	P_FINISH;

	FComponentRecreateContext RecreateContext(this);
	TransformedComponent = NewTransformedComponent;
}

//
//	UTransformComponent::execSetTranslation
//

void UTransformComponent::execSetTranslation(FFrame& Stack,RESULT_DECL)
{
	P_GET_VECTOR(NewTranslation);
	P_FINISH;

	FComponentRecreateContext RecreateContext(this);
	Translation = NewTranslation;
}

//
//	UTransformComponent::execSetRotation
//

void UTransformComponent::execSetRotation(FFrame& Stack,RESULT_DECL)
{
	P_GET_ROTATOR(NewRotation);
	P_FINISH;

	FComponentRecreateContext RecreateContext(this);
	Rotation = NewRotation;
}

//
//	UTransformComponent::execSetScale
//

void UTransformComponent::execSetScale(FFrame& Stack,RESULT_DECL)
{
	P_GET_FLOAT(NewScale);
	P_FINISH;

	FComponentRecreateContext RecreateContext(this);
	Scale = NewScale;

}

//
//	UTransformComponent::execSetScale3D
//

void UTransformComponent::execSetScale3D(FFrame& Stack,RESULT_DECL)
{
	P_GET_VECTOR(NewScale3D);
	P_FINISH;

	FComponentRecreateContext RecreateContext(this);
	Scale3D = NewScale3D;

}

//
//	UTransformComponent::execSetAbsolute
//

void UTransformComponent::execSetAbsolute(FFrame& Stack,RESULT_DECL)
{
	P_GET_UBOOL_OPTX(NewAbsoluteTranslation,AbsoluteTranslation);
	P_GET_UBOOL_OPTX(NewAbsoluteRotation,AbsoluteRotation);
	P_GET_UBOOL_OPTX(NewAbsoluteScale,AbsoluteScale);
	P_FINISH;

	FComponentRecreateContext RecreateContext(this);
	AbsoluteTranslation = NewAbsoluteTranslation;
	AbsoluteRotation = NewAbsoluteRotation;
	AbsoluteScale = NewAbsoluteScale;

}

//
//	UWindPointSourceComponent::SetParentToWorld
//

void UWindPointSourceComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(ParentToWorld);
	Position = ParentToWorld.GetOrigin();
}

//
//	UWindPointSourceComponent::Created
//

void UWindPointSourceComponent::Created()
{
	Super::Created();
	check(Owner && Owner->XLevel);
	Scene->WindPointSources.AddItem(this);
}

//
//	UWindPointSourceComponent::Destroyed
//

void UWindPointSourceComponent::Destroyed()
{
	Super::Destroyed();
	check(Owner && Owner->XLevel);
	Scene->WindPointSources.RemoveItem(this);
}

//
//	UWindPointSourceComponent::GetRenderData
//

FWindPointSource UWindPointSourceComponent::GetRenderData() const
{
	FWindPointSource	Result;
	Result.SourceLocation = Position;
	Result.Strength = Strength;
	check(Owner);
	Result.Phase = Scene->GetSceneTime() - Owner->CreationTime + Phase;
	Result.Frequency = Frequency;
	Result.InvRadius = Radius == 0.0f ? 0.0f : 1.0f / Radius;
	Result.InvDuration = !Owner->Level->bBegunPlay || Duration == 0.0f ? 0.0f : 1.0f / Duration;
	return Result;
}

//
//	UWindDirectionalSourceComponent::SetParentToWorld
//

void UWindDirectionalSourceComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(ParentToWorld);
	Direction = ParentToWorld.TransformNormal(FVector(1,0,0)).SafeNormal();
}

//
//	UWindDirectionalSourceComponent::Created
//

void UWindDirectionalSourceComponent::Created()
{
	Super::Created();
	check(Owner && Owner->XLevel);
	Scene->WindDirectionalSources.AddItem(this);
}

//
//	UWindDirectionalSourceComponent::Destroyed
//

void UWindDirectionalSourceComponent::Destroyed()
{
	Super::Destroyed();
	check(Owner && Owner->XLevel);
	Scene->WindDirectionalSources.RemoveItem(this);
}

//
//	UWindDirectionalSourceComponent::GetRenderData
//

FWindDirectionSource UWindDirectionalSourceComponent::GetRenderData() const
{
	FWindDirectionSource	Result;
	Result.Direction = Direction;
	Result.Strength = Strength;
	Result.Phase = Scene->GetSceneTime() + Phase;
	Result.Frequency = Frequency;
	return Result;
}

//
//	UHeightFogComponent::SetParentToWorld
//

void UHeightFogComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(ParentToWorld);
	Height = ParentToWorld.GetOrigin().Z;
}

//
//	UHeightFogComponent::Created
//

void UHeightFogComponent::Created()
{
	Super::Created();
	Scene->Fogs.AddItem(this);
}

//
//	UHeightFogComponent::Destroyed
//

void UHeightFogComponent::Destroyed()
{
	Super::Destroyed();
	Scene->Fogs.RemoveItem(this);
}

//
//	UStaticMeshComponentFactory::CreatePrimitiveComponent
//

UPrimitiveComponent* UStaticMeshComponentFactory::CreatePrimitiveComponent(UObject* InOuter)
{
	UStaticMeshComponent*	Component = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(),InOuter);

	Component->CollideActors = CollideActors;
	Component->BlockActors = BlockActors;
	Component->BlockZeroExtent = BlockZeroExtent;
	Component->BlockNonZeroExtent = BlockNonZeroExtent;
	Component->BlockRigidBody = BlockRigidBody;
	Component->HiddenGame = HiddenGame;
	Component->HiddenEditor = HiddenEditor;
	Component->CastShadow = CastShadow;
	Component->Materials = Materials;
	Component->StaticMesh = StaticMesh;

	return Component;
}
