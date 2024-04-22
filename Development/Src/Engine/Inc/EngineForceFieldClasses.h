/*===========================================================================
    C++ class definitions exported from UnrealScript.
    This is automatically generated by the tools.
    DO NOT modify this manually! Edit the corresponding .uc files instead!
    Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
===========================================================================*/
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif

#include "EngineNames.h"

// Split enums from the rest of the header so they can be included earlier
// than the rest of the header file by including this file twice with different
// #define wrappers. See Engine.h and look at EngineClasses.h for an example.
#if !NO_ENUMS && !defined(NAMES_ONLY)

#ifndef INCLUDED_ENGINE_FORCEFIELD_ENUMS
#define INCLUDED_ENGINE_FORCEFIELD_ENUMS 1

enum FFB_ForceFieldCoordinates
{
    FFB_CARTESIAN           =0,
    FFB_SPHERICAL           =1,
    FFB_CYLINDRICAL         =2,
    FFB_TOROIDAL            =3,
    FFB_MAX                 =4,
};
#define FOREACH_ENUM_FFB_FORCEFIELDCOORDINATES(op) \
    op(FFB_CARTESIAN) \
    op(FFB_SPHERICAL) \
    op(FFB_CYLINDRICAL) \
    op(FFB_TOROIDAL) 
enum FFG_ForceFieldCoordinates
{
    FFG_CARTESIAN           =0,
    FFG_SPHERICAL           =1,
    FFG_CYLINDRICAL         =2,
    FFG_TOROIDAL            =3,
    FFG_MAX                 =4,
};
#define FOREACH_ENUM_FFG_FORCEFIELDCOORDINATES(op) \
    op(FFG_CARTESIAN) \
    op(FFG_SPHERICAL) \
    op(FFG_CYLINDRICAL) \
    op(FFG_TOROIDAL) 
enum ERadialForceType
{
    RFT_Force               =0,
    RFT_Impulse             =1,
    RFT_MAX                 =2,
};
#define FOREACH_ENUM_ERADIALFORCETYPE(op) \
    op(RFT_Force) \
    op(RFT_Impulse) 

#endif // !INCLUDED_ENGINE_FORCEFIELD_ENUMS
#endif // !NO_ENUMS

#if !ENUMS_ONLY

#ifndef NAMES_ONLY
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#endif


#ifndef NAMES_ONLY

#ifndef INCLUDED_ENGINE_FORCEFIELD_CLASSES
#define INCLUDED_ENGINE_FORCEFIELD_CLASSES 1
#define ENABLE_DECLARECLASS_MACRO 1
#include "UnObjBas.h"
#undef ENABLE_DECLARECLASS_MACRO

class ANxGenericForceFieldBrush : public AVolume
{
public:
    //## BEGIN PROPS NxGenericForceFieldBrush
    INT ExcludeChannel;
    FRBCollisionChannelContainer CollideWithChannels;
    BYTE RBChannel;
    BYTE Coordinates;
    SCRIPT_ALIGN;
    FVector Constant;
    FVector PositionMultiplierX;
    FVector PositionMultiplierY;
    FVector PositionMultiplierZ;
    FVector PositionTarget;
    FVector VelocityMultiplierX;
    FVector VelocityMultiplierY;
    FVector VelocityMultiplierZ;
    FVector VelocityTarget;
    FVector Noise;
    FVector FalloffLinear;
    FVector FalloffQuadratic;
    FLOAT TorusRadius;
    class UserForceField* ForceField;
    TArrayNoInit<FPointer> ConvexMeshes;
    TArrayNoInit<FPointer> ExclusionShapes;
    TArrayNoInit<FPointer> ExclusionShapePoses;
    class UserForceFieldLinearKernel* LinearKernel;
    //## END PROPS NxGenericForceFieldBrush

    DECLARE_CLASS(ANxGenericForceFieldBrush,AVolume,0,Engine)
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
	virtual void TickSpecial(FLOAT DeltaSeconds);
};

class ARB_ForceFieldExcludeVolume : public AVolume
{
public:
    //## BEGIN PROPS RB_ForceFieldExcludeVolume
    INT ForceFieldChannel;
    INT SceneIndex;
    //## END PROPS RB_ForceFieldExcludeVolume

    DECLARE_CLASS(ARB_ForceFieldExcludeVolume,AVolume,0,Engine)
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
};

class ANxForceField : public AActor
{
public:
    //## BEGIN PROPS NxForceField
    INT ExcludeChannel;
    BITFIELD bForceActive:1;
    SCRIPT_ALIGN;
    FRBCollisionChannelContainer CollideWithChannels;
    BYTE RBChannel;
    SCRIPT_ALIGN;
    class UserForceField* ForceField;
    TArrayNoInit<FPointer> ConvexMeshes;
    TArrayNoInit<FPointer> ExclusionShapes;
    TArrayNoInit<FPointer> ExclusionShapePoses;
    FPointer U2NRotation;
    INT SceneIndex;
    //## END PROPS NxForceField

    virtual void DoInitRBPhys();
    DECLARE_FUNCTION(execDoInitRBPhys)
    {
        P_FINISH;
        this->DoInitRBPhys();
    }
    DECLARE_ABSTRACT_CLASS(ANxForceField,AActor,0,Engine)
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual void TickSpecial(FLOAT DeltaSeconds);

	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void SetForceFieldPose(FPointer ForceFieldDesc);

#if WITH_NOVODEX
	void CreateExclusionShapes(NxScene* nxScene);
#endif
};

class ANxCylindricalForceField : public ANxForceField
{
public:
    //## BEGIN PROPS NxCylindricalForceField
    FLOAT RadialStrength;
    FLOAT RotationalStrength;
    FLOAT LiftStrength;
    FLOAT ForceRadius;
    FLOAT ForceTopRadius;
    FLOAT LiftFalloffHeight;
    FLOAT EscapeVelocity;
    FLOAT ForceHeight;
    FLOAT HeightOffset;
    BITFIELD UseSpecialRadialForce:1;
    SCRIPT_ALIGN;
    class NxForceFieldKernelSample* Kernel;
    //## END PROPS NxCylindricalForceField

    DECLARE_ABSTRACT_CLASS(ANxCylindricalForceField,ANxForceField,0,Engine)
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
};

class ANxCylindricalForceFieldCapsule : public ANxCylindricalForceField
{
public:
    //## BEGIN PROPS NxCylindricalForceFieldCapsule
    class UDrawCapsuleComponent* RenderComponent;
    //## END PROPS NxCylindricalForceFieldCapsule

    virtual void DoInitRBPhys();
    DECLARE_CLASS(ANxCylindricalForceFieldCapsule,ANxCylindricalForceField,0,Engine)
	virtual void InitRBPhys();
	virtual void TickSpecial(FLOAT DeltaSeconds);
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif

	virtual FPointer DefineForceFieldShapeDesc();

};

class ANxForceFieldGeneric : public ANxForceField
{
public:
    //## BEGIN PROPS NxForceFieldGeneric
    class UForceFieldShape* Shape;
    class UActorComponent* DrawComponent;
    FLOAT RoughExtentX;
    FLOAT RoughExtentY;
    FLOAT RoughExtentZ;
    BYTE Coordinates;
    SCRIPT_ALIGN;
    FVector Constant;
    FVector PositionMultiplierX;
    FVector PositionMultiplierY;
    FVector PositionMultiplierZ;
    FVector PositionTarget;
    FVector VelocityMultiplierX;
    FVector VelocityMultiplierY;
    FVector VelocityMultiplierZ;
    FVector VelocityTarget;
    FVector Noise;
    FVector FalloffLinear;
    FVector FalloffQuadratic;
    FLOAT TorusRadius;
    class UserForceFieldLinearKernel* LinearKernel;
    //## END PROPS NxForceFieldGeneric

    virtual void DoInitRBPhys();
    DECLARE_CLASS(ANxForceFieldGeneric,ANxForceField,0,Engine)
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual void PostLoad();
};

class ANxForceFieldRadial : public ANxForceField
{
public:
    //## BEGIN PROPS NxForceFieldRadial
    class UForceFieldShape* Shape;
    class UActorComponent* DrawComponent;
    FLOAT ForceStrength;
    FLOAT ForceRadius;
    FLOAT SelfRotationStrength;
    BYTE ForceFalloff;
    SCRIPT_ALIGN;
    class NxForceFieldKernelRadial* Kernel;
    //## END PROPS NxForceFieldRadial

    virtual void DoInitRBPhys();
    DECLARE_CLASS(ANxForceFieldRadial,ANxForceField,0,Engine)
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual void PostLoad();
};

class ANxForceFieldTornado : public ANxForceField
{
public:
    //## BEGIN PROPS NxForceFieldTornado
    class UForceFieldShape* Shape;
    class UActorComponent* DrawComponent;
    FLOAT RadialStrength;
    FLOAT RotationalStrength;
    FLOAT LiftStrength;
    FLOAT ForceRadius;
    FLOAT ForceTopRadius;
    FLOAT LiftFalloffHeight;
    FLOAT EscapeVelocity;
    FLOAT ForceHeight;
    FLOAT HeightOffset;
    BITFIELD BSpecialRadialForceMode:1;
    FLOAT SelfRotationStrength;
    class NxForceFieldKernelTornadoAngular* Kernel;
    //## END PROPS NxForceFieldTornado

    virtual void DoInitRBPhys();
    DECLARE_CLASS(ANxForceFieldTornado,ANxForceField,0,Engine)
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void SetForceFieldPose(FPointer ForceFieldDesc);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual void PostLoad();
};

class ANxGenericForceField : public ANxForceField
{
public:
    //## BEGIN PROPS NxGenericForceField
    BYTE Coordinates;
    SCRIPT_ALIGN;
    FVector Constant;
    FVector PositionMultiplierX;
    FVector PositionMultiplierY;
    FVector PositionMultiplierZ;
    FVector PositionTarget;
    FVector VelocityMultiplierX;
    FVector VelocityMultiplierY;
    FVector VelocityMultiplierZ;
    FVector VelocityTarget;
    FVector Noise;
    FVector FalloffLinear;
    FVector FalloffQuadratic;
    FLOAT TorusRadius;
    class UserForceFieldLinearKernel* LinearKernel;
    //## END PROPS NxGenericForceField

    DECLARE_ABSTRACT_CLASS(ANxGenericForceField,ANxForceField,0,Engine)
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual void TickSpecial(FLOAT DeltaSeconds);

	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
};

class ANxGenericForceFieldBox : public ANxGenericForceField
{
public:
    //## BEGIN PROPS NxGenericForceFieldBox
    class UDrawBoxComponent* RenderComponent;
    FVector BoxExtent;
    //## END PROPS NxGenericForceFieldBox

    virtual void DoInitRBPhys();
    DECLARE_CLASS(ANxGenericForceFieldBox,ANxGenericForceField,0,Engine)
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual FPointer DefineForceFieldShapeDesc();
};

class ANxGenericForceFieldCapsule : public ANxGenericForceField
{
public:
    //## BEGIN PROPS NxGenericForceFieldCapsule
    class UDrawCapsuleComponent* RenderComponent;
    FLOAT CapsuleHeight;
    FLOAT CapsuleRadius;
    //## END PROPS NxGenericForceFieldCapsule

    DECLARE_CLASS(ANxGenericForceFieldCapsule,ANxGenericForceField,0,Engine)
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual FPointer DefineForceFieldShapeDesc();
};

class ANxRadialForceField : public ANxForceField
{
public:
    //## BEGIN PROPS NxRadialForceField
    class UDrawSphereComponent* RenderComponent;
    FLOAT ForceStrength;
    FLOAT ForceRadius;
    BYTE ForceFalloff;
    SCRIPT_ALIGN;
    class UserForceFieldLinearKernel* LinearKernel;
    //## END PROPS NxRadialForceField

    DECLARE_CLASS(ANxRadialForceField,ANxForceField,0,Engine)
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void TickSpecial(FLOAT DeltaSeconds);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
};

class ANxRadialCustomForceField : public ANxRadialForceField
{
public:
    //## BEGIN PROPS NxRadialCustomForceField
    FLOAT SelfRotationStrength;
    class NxForceFieldKernelRadial* Kernel;
    //## END PROPS NxRadialCustomForceField

    DECLARE_CLASS(ANxRadialCustomForceField,ANxRadialForceField,0,Engine)
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
};

class ANxTornadoAngularForceField : public ANxForceField
{
public:
    //## BEGIN PROPS NxTornadoAngularForceField
    FLOAT RadialStrength;
    FLOAT RotationalStrength;
    FLOAT LiftStrength;
    FLOAT ForceRadius;
    FLOAT ForceTopRadius;
    FLOAT LiftFalloffHeight;
    FLOAT EscapeVelocity;
    FLOAT ForceHeight;
    FLOAT HeightOffset;
    BITFIELD BSpecialRadialForceMode:1;
    FLOAT SelfRotationStrength;
    class NxForceFieldKernelTornadoAngular* Kernel;
    //## END PROPS NxTornadoAngularForceField

    DECLARE_ABSTRACT_CLASS(ANxTornadoAngularForceField,ANxForceField,0,Engine)
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
};

class ANxTornadoAngularForceFieldCapsule : public ANxTornadoAngularForceField
{
public:
    //## BEGIN PROPS NxTornadoAngularForceFieldCapsule
    class UDrawCapsuleComponent* RenderComponent;
    //## END PROPS NxTornadoAngularForceFieldCapsule

    DECLARE_CLASS(ANxTornadoAngularForceFieldCapsule,ANxTornadoAngularForceField,0,Engine)
	virtual void InitRBPhys();
	virtual void TickSpecial(FLOAT DeltaSeconds);
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual FPointer DefineForceFieldShapeDesc();

};

class ANxTornadoForceField : public ANxForceField
{
public:
    //## BEGIN PROPS NxTornadoForceField
    FLOAT RadialStrength;
    FLOAT RotationalStrength;
    FLOAT LiftStrength;
    FLOAT ForceRadius;
    FLOAT ForceTopRadius;
    FLOAT LiftFalloffHeight;
    FLOAT EscapeVelocity;
    FLOAT ForceHeight;
    FLOAT HeightOffset;
    BITFIELD BSpecialRadialForceMode:1;
    SCRIPT_ALIGN;
    class NxForceFieldKernelTornado* Kernel;
    //## END PROPS NxTornadoForceField

    DECLARE_ABSTRACT_CLASS(ANxTornadoForceField,ANxForceField,0,Engine)
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
};

class ANxTornadoForceFieldCapsule : public ANxTornadoForceField
{
public:
    //## BEGIN PROPS NxTornadoForceFieldCapsule
    class UDrawCapsuleComponent* RenderComponent;
    //## END PROPS NxTornadoForceFieldCapsule

    DECLARE_CLASS(ANxTornadoForceFieldCapsule,ANxTornadoForceField,0,Engine)
	virtual void InitRBPhys();
	virtual void TickSpecial(FLOAT DeltaSeconds);
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual FPointer DefineForceFieldShapeDesc();

};

class ANxForceFieldSpawnable : public AActor
{
public:
    //## BEGIN PROPS NxForceFieldSpawnable
    class UNxForceFieldComponent* ForceFieldComponent;
    //## END PROPS NxForceFieldSpawnable

    DECLARE_CLASS(ANxForceFieldSpawnable,AActor,0,Engine)

};

class ARB_CylindricalForceActor : public ARigidBodyBase
{
public:
    //## BEGIN PROPS RB_CylindricalForceActor
    class UDrawCylinderComponent* RenderComponent;
    FLOAT RadialStrength;
    FLOAT RotationalStrength;
    FLOAT LiftStrength;
    FLOAT LiftFalloffHeight;
    FLOAT EscapeVelocity;
    FLOAT ForceRadius;
    FLOAT ForceTopRadius;
    FLOAT ForceHeight;
    FLOAT HeightOffset;
    BITFIELD bForceActive:1;
    BITFIELD bForceApplyToCloth:1;
    BITFIELD bForceApplyToFluid:1;
    BITFIELD bForceApplyToRigidBodies:1;
    BITFIELD bForceApplyToProjectiles:1;
    SCRIPT_ALIGN;
    FRBCollisionChannelContainer CollideWithChannels;
    //## END PROPS RB_CylindricalForceActor

    DECLARE_CLASS(ARB_CylindricalForceActor,ARigidBodyBase,0,Engine)
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void TickSpecial(FLOAT DeltaSeconds);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
};

class ARB_RadialForceActor : public ARigidBodyBase
{
public:
    //## BEGIN PROPS RB_RadialForceActor
    class UDrawSphereComponent* RenderComponent;
    FLOAT ForceStrength;
    FLOAT ForceRadius;
    FLOAT SwirlStrength;
    FLOAT SpinTorque;
    BYTE ForceFalloff;
    BYTE RadialForceMode;
    SCRIPT_ALIGN;
    BITFIELD bForceActive:1;
    BITFIELD bForceApplyToCloth:1;
    BITFIELD bForceApplyToFluid:1;
    BITFIELD bForceApplyToRigidBodies:1;
    BITFIELD bForceApplyToProjectiles:1;
    SCRIPT_ALIGN;
    FRBCollisionChannelContainer CollideWithChannels;
    //## END PROPS RB_RadialForceActor

    DECLARE_CLASS(ARB_RadialForceActor,ARigidBodyBase,0,Engine)
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void TickSpecial(FLOAT DeltaSeconds);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
};

class UNxForceFieldComponent : public UPrimitiveComponent
{
public:
    //## BEGIN PROPS NxForceFieldComponent
    class UForceFieldShape* Shape;
    class UActorComponent* DrawComponent;
    INT ExcludeChannel;
    BITFIELD bForceActive:1;
    BITFIELD bDestroyWhenInactive:1;
    SCRIPT_ALIGN;
    FRBCollisionChannelContainer CollideWithChannels;
    FLOAT Duration;
    class UserForceField* ForceField;
    TArrayNoInit<FPointer> ConvexMeshes;
    TArrayNoInit<FPointer> ExclusionShapes;
    TArrayNoInit<FPointer> ExclusionShapePoses;
    INT SceneIndex;
    FLOAT ElapsedTime;
    class UPrimitiveComponent* RenderComponent;
    class FRBPhysScene* RBPhysScene;
    //## END PROPS NxForceFieldComponent

    virtual void DoInitRBPhys();
    DECLARE_FUNCTION(execDoInitRBPhys)
    {
        P_FINISH;
        this->DoInitRBPhys();
    }
    DECLARE_ABSTRACT_CLASS(UNxForceFieldComponent,UPrimitiveComponent,0,Engine)
	virtual void Tick(FLOAT DeltaSeconds); //This will call ApplyForces on derived classes if the force is active
	virtual void CreateKernel();
	virtual UBOOL AllowBeingMarkedPendingKill(void);
	virtual void  InitComponentRBPhys (UBOOL bFixed);
	virtual void  InitForceField(FRBPhysScene *InScene);
	virtual void  TermComponentRBPhys (FRBPhysScene *InScene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void SetForceFieldPose(FPointer ForceFieldDesc);
	virtual void PostLoad();
	virtual void UpdateTransform();

#if WITH_NOVODEX
	void CreateExclusionShapes(NxScene* nxScene);
#endif
};

class UNxForceFieldCylindricalComponent : public UNxForceFieldComponent
{
public:
    //## BEGIN PROPS NxForceFieldCylindricalComponent
    FLOAT RadialStrength;
    FLOAT RotationalStrength;
    FLOAT LiftStrength;
    FLOAT ForceRadius;
    FLOAT ForceTopRadius;
    FLOAT LiftFalloffHeight;
    FLOAT EscapeVelocity;
    FLOAT ForceHeight;
    FLOAT HeightOffset;
    BITFIELD UseSpecialRadialForce:1;
    SCRIPT_ALIGN;
    class NxForceFieldKernelSample* Kernel;
    //## END PROPS NxForceFieldCylindricalComponent

    DECLARE_CLASS(UNxForceFieldCylindricalComponent,UNxForceFieldComponent,0,Engine)
	virtual void  TermComponentRBPhys (FRBPhysScene *InScene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void CreateKernel();
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif

};

class UNxForceFieldGenericComponent : public UNxForceFieldComponent
{
public:
    //## BEGIN PROPS NxForceFieldGenericComponent
    FLOAT RoughExtentX;
    FLOAT RoughExtentY;
    FLOAT RoughExtentZ;
    BYTE Coordinates;
    SCRIPT_ALIGN;
    FVector Constant;
    FVector PositionMultiplierX;
    FVector PositionMultiplierY;
    FVector PositionMultiplierZ;
    FVector PositionTarget;
    FVector VelocityMultiplierX;
    FVector VelocityMultiplierY;
    FVector VelocityMultiplierZ;
    FVector VelocityTarget;
    FVector Noise;
    FVector FalloffLinear;
    FVector FalloffQuadratic;
    FLOAT TorusRadius;
    class UserForceFieldLinearKernel* Kernel;
    //## END PROPS NxForceFieldGenericComponent

    DECLARE_CLASS(UNxForceFieldGenericComponent,UNxForceFieldComponent,0,Engine)
	virtual void  TermComponentRBPhys (FRBPhysScene *InScene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void CreateKernel();
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
};

class UNxForceFieldRadialComponent : public UNxForceFieldComponent
{
public:
    //## BEGIN PROPS NxForceFieldRadialComponent
    FLOAT ForceStrength;
    FLOAT ForceRadius;
    FLOAT SelfRotationStrength;
    BYTE ForceFalloff;
    SCRIPT_ALIGN;
    class NxForceFieldKernelRadial* Kernel;
    //## END PROPS NxForceFieldRadialComponent

    DECLARE_CLASS(UNxForceFieldRadialComponent,UNxForceFieldComponent,0,Engine)
	virtual void  TermComponentRBPhys (FRBPhysScene *InScene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void CreateKernel();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
};

class UNxForceFieldTornadoComponent : public UNxForceFieldComponent
{
public:
    //## BEGIN PROPS NxForceFieldTornadoComponent
    FLOAT RadialStrength;
    FLOAT RotationalStrength;
    FLOAT LiftStrength;
    FLOAT ForceRadius;
    FLOAT ForceTopRadius;
    FLOAT LiftFalloffHeight;
    FLOAT EscapeVelocity;
    FLOAT ForceHeight;
    FLOAT HeightOffset;
    BITFIELD BSpecialRadialForceMode:1;
    FLOAT SelfRotationStrength;
    class NxForceFieldKernelTornadoAngular* Kernel;
    //## END PROPS NxForceFieldTornadoComponent

    DECLARE_CLASS(UNxForceFieldTornadoComponent,UNxForceFieldComponent,0,Engine)
	virtual void  TermComponentRBPhys (FRBPhysScene *InScene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void CreateKernel();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
	
#endif
};

struct ForceFieldShape_eventGetDrawComponent_Parms
{
    class UPrimitiveComponent* ReturnValue;
    ForceFieldShape_eventGetDrawComponent_Parms(EEventParm)
    {
    }
};
struct ForceFieldShape_eventFillByCylinder_Parms
{
    FLOAT BottomRadius;
    FLOAT TopRadius;
    FLOAT Height;
    FLOAT HeightOffset;
    ForceFieldShape_eventFillByCylinder_Parms(EEventParm)
    {
    }
};
struct ForceFieldShape_eventFillByCapsule_Parms
{
    FLOAT Height;
    FLOAT Radius;
    ForceFieldShape_eventFillByCapsule_Parms(EEventParm)
    {
    }
};
struct ForceFieldShape_eventFillByBox_Parms
{
    FVector Dimension;
    ForceFieldShape_eventFillByBox_Parms(EEventParm)
    {
    }
};
struct ForceFieldShape_eventFillBySphere_Parms
{
    FLOAT Radius;
    ForceFieldShape_eventFillBySphere_Parms(EEventParm)
    {
    }
};
class UForceFieldShape : public UObject
{
public:
    //## BEGIN PROPS ForceFieldShape
    //## END PROPS ForceFieldShape

    class UPrimitiveComponent* eventGetDrawComponent()
    {
        ForceFieldShape_eventGetDrawComponent_Parms Parms(EC_EventParm);
        Parms.ReturnValue=NULL;
        ProcessEvent(FindFunctionChecked(ENGINE_GetDrawComponent),&Parms);
        return Parms.ReturnValue;
    }
    void eventFillByCylinder(FLOAT BottomRadius,FLOAT TopRadius,FLOAT Height,FLOAT HeightOffset)
    {
        ForceFieldShape_eventFillByCylinder_Parms Parms(EC_EventParm);
        Parms.BottomRadius=BottomRadius;
        Parms.TopRadius=TopRadius;
        Parms.Height=Height;
        Parms.HeightOffset=HeightOffset;
        ProcessEvent(FindFunctionChecked(ENGINE_FillByCylinder),&Parms);
    }
    void eventFillByCapsule(FLOAT Height,FLOAT Radius)
    {
        ForceFieldShape_eventFillByCapsule_Parms Parms(EC_EventParm);
        Parms.Height=Height;
        Parms.Radius=Radius;
        ProcessEvent(FindFunctionChecked(ENGINE_FillByCapsule),&Parms);
    }
    void eventFillByBox(FVector Dimension)
    {
        ForceFieldShape_eventFillByBox_Parms Parms(EC_EventParm);
        Parms.Dimension=Dimension;
        ProcessEvent(FindFunctionChecked(ENGINE_FillByBox),&Parms);
    }
    void eventFillBySphere(FLOAT Radius)
    {
        ForceFieldShape_eventFillBySphere_Parms Parms(EC_EventParm);
        Parms.Radius=Radius;
        ProcessEvent(FindFunctionChecked(ENGINE_FillBySphere),&Parms);
    }
    DECLARE_ABSTRACT_CLASS(UForceFieldShape,UObject,0,Engine)
#if WITH_NOVODEX
	virtual class NxForceFieldShapeDesc * CreateNxDesc(){ return NULL; }
#endif
};

struct ForceFieldShapeBox_eventGetRadii_Parms
{
    FVector ReturnValue;
    ForceFieldShapeBox_eventGetRadii_Parms(EEventParm)
    {
    }
};
class UForceFieldShapeBox : public UForceFieldShape
{
public:
    //## BEGIN PROPS ForceFieldShapeBox
    class UDrawBoxComponent* Shape;
    //## END PROPS ForceFieldShapeBox

    FVector eventGetRadii()
    {
        ForceFieldShapeBox_eventGetRadii_Parms Parms(EC_EventParm);
        appMemzero(&Parms.ReturnValue,sizeof(Parms.ReturnValue));
        ProcessEvent(FindFunctionChecked(ENGINE_GetRadii),&Parms);
        return Parms.ReturnValue;
    }
    DECLARE_CLASS(UForceFieldShapeBox,UForceFieldShape,0,Engine)
#if WITH_NOVODEX
	virtual class NxForceFieldShapeDesc * CreateNxDesc();
#endif
};

struct ForceFieldShapeCapsule_eventGetRadius_Parms
{
    FLOAT ReturnValue;
    ForceFieldShapeCapsule_eventGetRadius_Parms(EEventParm)
    {
    }
};
struct ForceFieldShapeCapsule_eventGetHeight_Parms
{
    FLOAT ReturnValue;
    ForceFieldShapeCapsule_eventGetHeight_Parms(EEventParm)
    {
    }
};
class UForceFieldShapeCapsule : public UForceFieldShape
{
public:
    //## BEGIN PROPS ForceFieldShapeCapsule
    class UDrawCapsuleComponent* Shape;
    //## END PROPS ForceFieldShapeCapsule

    FLOAT eventGetRadius()
    {
        ForceFieldShapeCapsule_eventGetRadius_Parms Parms(EC_EventParm);
        Parms.ReturnValue=0;
        ProcessEvent(FindFunctionChecked(ENGINE_GetRadius),&Parms);
        return Parms.ReturnValue;
    }
    FLOAT eventGetHeight()
    {
        ForceFieldShapeCapsule_eventGetHeight_Parms Parms(EC_EventParm);
        Parms.ReturnValue=0;
        ProcessEvent(FindFunctionChecked(ENGINE_GetHeight),&Parms);
        return Parms.ReturnValue;
    }
    DECLARE_CLASS(UForceFieldShapeCapsule,UForceFieldShape,0,Engine)
#if WITH_NOVODEX
	virtual class NxForceFieldShapeDesc * CreateNxDesc();
#endif
};

struct ForceFieldShapeSphere_eventGetRadius_Parms
{
    FLOAT ReturnValue;
    ForceFieldShapeSphere_eventGetRadius_Parms(EEventParm)
    {
    }
};
class UForceFieldShapeSphere : public UForceFieldShape
{
public:
    //## BEGIN PROPS ForceFieldShapeSphere
    class UDrawSphereComponent* Shape;
    //## END PROPS ForceFieldShapeSphere

    FLOAT eventGetRadius()
    {
        ForceFieldShapeSphere_eventGetRadius_Parms Parms(EC_EventParm);
        Parms.ReturnValue=0;
        ProcessEvent(FindFunctionChecked(ENGINE_GetRadius),&Parms);
        return Parms.ReturnValue;
    }
    DECLARE_CLASS(UForceFieldShapeSphere,UForceFieldShape,0,Engine)
#if WITH_NOVODEX
	virtual class NxForceFieldShapeDesc * CreateNxDesc();
#endif
};

#undef DECLARE_CLASS
#undef DECLARE_CASTED_CLASS
#undef DECLARE_ABSTRACT_CLASS
#undef DECLARE_ABSTRACT_CASTED_CLASS
#endif // !INCLUDED_ENGINE_FORCEFIELD_CLASSES
#endif // !NAMES_ONLY

AUTOGENERATE_FUNCTION(ANxForceField,-1,execDoInitRBPhys);
AUTOGENERATE_FUNCTION(ANxCylindricalForceFieldCapsule,-1,execDoInitRBPhys);
AUTOGENERATE_FUNCTION(ANxForceFieldGeneric,-1,execDoInitRBPhys);
AUTOGENERATE_FUNCTION(ANxForceFieldRadial,-1,execDoInitRBPhys);
AUTOGENERATE_FUNCTION(ANxForceFieldTornado,-1,execDoInitRBPhys);
AUTOGENERATE_FUNCTION(ANxGenericForceFieldBox,-1,execDoInitRBPhys);
AUTOGENERATE_FUNCTION(UNxForceFieldComponent,-1,execDoInitRBPhys);

#ifndef NAMES_ONLY
#undef AUTOGENERATE_FUNCTION
#endif

#ifdef STATIC_LINKING_MOJO
#ifndef ENGINE_FORCEFIELD_NATIVE_DEFS
#define ENGINE_FORCEFIELD_NATIVE_DEFS

#define AUTO_INITIALIZE_REGISTRANTS_ENGINE_FORCEFIELD \
	ANxGenericForceFieldBrush::StaticClass(); \
	ARB_ForceFieldExcludeVolume::StaticClass(); \
	ANxForceField::StaticClass(); \
	GNativeLookupFuncs.Set(FName("NxForceField"), GEngineANxForceFieldNatives); \
	ANxCylindricalForceField::StaticClass(); \
	ANxCylindricalForceFieldCapsule::StaticClass(); \
	GNativeLookupFuncs.Set(FName("NxCylindricalForceFieldCapsule"), GEngineANxCylindricalForceFieldCapsuleNatives); \
	ANxForceFieldGeneric::StaticClass(); \
	GNativeLookupFuncs.Set(FName("NxForceFieldGeneric"), GEngineANxForceFieldGenericNatives); \
	ANxForceFieldRadial::StaticClass(); \
	GNativeLookupFuncs.Set(FName("NxForceFieldRadial"), GEngineANxForceFieldRadialNatives); \
	ANxForceFieldTornado::StaticClass(); \
	GNativeLookupFuncs.Set(FName("NxForceFieldTornado"), GEngineANxForceFieldTornadoNatives); \
	ANxGenericForceField::StaticClass(); \
	ANxGenericForceFieldBox::StaticClass(); \
	GNativeLookupFuncs.Set(FName("NxGenericForceFieldBox"), GEngineANxGenericForceFieldBoxNatives); \
	ANxGenericForceFieldCapsule::StaticClass(); \
	ANxRadialForceField::StaticClass(); \
	ANxRadialCustomForceField::StaticClass(); \
	ANxTornadoAngularForceField::StaticClass(); \
	ANxTornadoAngularForceFieldCapsule::StaticClass(); \
	ANxTornadoForceField::StaticClass(); \
	ANxTornadoForceFieldCapsule::StaticClass(); \
	ANxForceFieldSpawnable::StaticClass(); \
	ARB_CylindricalForceActor::StaticClass(); \
	ARB_RadialForceActor::StaticClass(); \
	UNxForceFieldComponent::StaticClass(); \
	GNativeLookupFuncs.Set(FName("NxForceFieldComponent"), GEngineUNxForceFieldComponentNatives); \
	UNxForceFieldCylindricalComponent::StaticClass(); \
	UNxForceFieldGenericComponent::StaticClass(); \
	UNxForceFieldRadialComponent::StaticClass(); \
	UNxForceFieldTornadoComponent::StaticClass(); \
	UForceFieldShape::StaticClass(); \
	UForceFieldShapeBox::StaticClass(); \
	UForceFieldShapeCapsule::StaticClass(); \
	UForceFieldShapeSphere::StaticClass(); \

#endif // ENGINE_FORCEFIELD_NATIVE_DEFS

#ifdef NATIVES_ONLY
FNativeFunctionLookup GEngineANxForceFieldNatives[] = 
{ 
	MAP_NATIVE(ANxForceField, execDoInitRBPhys)
	{NULL, NULL}
};

FNativeFunctionLookup GEngineANxCylindricalForceFieldCapsuleNatives[] = 
{ 
	MAP_NATIVE(ANxCylindricalForceFieldCapsule, execDoInitRBPhys)
	{NULL, NULL}
};

FNativeFunctionLookup GEngineANxForceFieldGenericNatives[] = 
{ 
	MAP_NATIVE(ANxForceFieldGeneric, execDoInitRBPhys)
	{NULL, NULL}
};

FNativeFunctionLookup GEngineANxForceFieldRadialNatives[] = 
{ 
	MAP_NATIVE(ANxForceFieldRadial, execDoInitRBPhys)
	{NULL, NULL}
};

FNativeFunctionLookup GEngineANxForceFieldTornadoNatives[] = 
{ 
	MAP_NATIVE(ANxForceFieldTornado, execDoInitRBPhys)
	{NULL, NULL}
};

FNativeFunctionLookup GEngineANxGenericForceFieldBoxNatives[] = 
{ 
	MAP_NATIVE(ANxGenericForceFieldBox, execDoInitRBPhys)
	{NULL, NULL}
};

FNativeFunctionLookup GEngineUNxForceFieldComponentNatives[] = 
{ 
	MAP_NATIVE(UNxForceFieldComponent, execDoInitRBPhys)
	{NULL, NULL}
};

#endif // NATIVES_ONLY
#endif // STATIC_LINKING_MOJO

#ifdef VERIFY_CLASS_SIZES
VERIFY_CLASS_OFFSET_NODIE(ANxGenericForceFieldBrush,NxGenericForceFieldBrush,ExcludeChannel)
VERIFY_CLASS_OFFSET_NODIE(ANxGenericForceFieldBrush,NxGenericForceFieldBrush,LinearKernel)
VERIFY_CLASS_SIZE_NODIE(ANxGenericForceFieldBrush)
VERIFY_CLASS_OFFSET_NODIE(ARB_ForceFieldExcludeVolume,RB_ForceFieldExcludeVolume,ForceFieldChannel)
VERIFY_CLASS_OFFSET_NODIE(ARB_ForceFieldExcludeVolume,RB_ForceFieldExcludeVolume,SceneIndex)
VERIFY_CLASS_SIZE_NODIE(ARB_ForceFieldExcludeVolume)
VERIFY_CLASS_OFFSET_NODIE(ANxForceField,NxForceField,ExcludeChannel)
VERIFY_CLASS_OFFSET_NODIE(ANxForceField,NxForceField,SceneIndex)
VERIFY_CLASS_SIZE_NODIE(ANxForceField)
VERIFY_CLASS_OFFSET_NODIE(ANxCylindricalForceField,NxCylindricalForceField,RadialStrength)
VERIFY_CLASS_OFFSET_NODIE(ANxCylindricalForceField,NxCylindricalForceField,Kernel)
VERIFY_CLASS_SIZE_NODIE(ANxCylindricalForceField)
VERIFY_CLASS_OFFSET_NODIE(ANxCylindricalForceFieldCapsule,NxCylindricalForceFieldCapsule,RenderComponent)
VERIFY_CLASS_SIZE_NODIE(ANxCylindricalForceFieldCapsule)
VERIFY_CLASS_OFFSET_NODIE(ANxForceFieldGeneric,NxForceFieldGeneric,Shape)
VERIFY_CLASS_OFFSET_NODIE(ANxForceFieldGeneric,NxForceFieldGeneric,LinearKernel)
VERIFY_CLASS_SIZE_NODIE(ANxForceFieldGeneric)
VERIFY_CLASS_OFFSET_NODIE(ANxForceFieldRadial,NxForceFieldRadial,Shape)
VERIFY_CLASS_OFFSET_NODIE(ANxForceFieldRadial,NxForceFieldRadial,Kernel)
VERIFY_CLASS_SIZE_NODIE(ANxForceFieldRadial)
VERIFY_CLASS_OFFSET_NODIE(ANxForceFieldTornado,NxForceFieldTornado,Shape)
VERIFY_CLASS_OFFSET_NODIE(ANxForceFieldTornado,NxForceFieldTornado,Kernel)
VERIFY_CLASS_SIZE_NODIE(ANxForceFieldTornado)
VERIFY_CLASS_OFFSET_NODIE(ANxGenericForceField,NxGenericForceField,Coordinates)
VERIFY_CLASS_OFFSET_NODIE(ANxGenericForceField,NxGenericForceField,LinearKernel)
VERIFY_CLASS_SIZE_NODIE(ANxGenericForceField)
VERIFY_CLASS_OFFSET_NODIE(ANxGenericForceFieldBox,NxGenericForceFieldBox,RenderComponent)
VERIFY_CLASS_OFFSET_NODIE(ANxGenericForceFieldBox,NxGenericForceFieldBox,BoxExtent)
VERIFY_CLASS_SIZE_NODIE(ANxGenericForceFieldBox)
VERIFY_CLASS_OFFSET_NODIE(ANxGenericForceFieldCapsule,NxGenericForceFieldCapsule,RenderComponent)
VERIFY_CLASS_OFFSET_NODIE(ANxGenericForceFieldCapsule,NxGenericForceFieldCapsule,CapsuleRadius)
VERIFY_CLASS_SIZE_NODIE(ANxGenericForceFieldCapsule)
VERIFY_CLASS_OFFSET_NODIE(ANxRadialForceField,NxRadialForceField,RenderComponent)
VERIFY_CLASS_OFFSET_NODIE(ANxRadialForceField,NxRadialForceField,LinearKernel)
VERIFY_CLASS_SIZE_NODIE(ANxRadialForceField)
VERIFY_CLASS_OFFSET_NODIE(ANxRadialCustomForceField,NxRadialCustomForceField,SelfRotationStrength)
VERIFY_CLASS_OFFSET_NODIE(ANxRadialCustomForceField,NxRadialCustomForceField,Kernel)
VERIFY_CLASS_SIZE_NODIE(ANxRadialCustomForceField)
VERIFY_CLASS_OFFSET_NODIE(ANxTornadoAngularForceField,NxTornadoAngularForceField,RadialStrength)
VERIFY_CLASS_OFFSET_NODIE(ANxTornadoAngularForceField,NxTornadoAngularForceField,Kernel)
VERIFY_CLASS_SIZE_NODIE(ANxTornadoAngularForceField)
VERIFY_CLASS_OFFSET_NODIE(ANxTornadoAngularForceFieldCapsule,NxTornadoAngularForceFieldCapsule,RenderComponent)
VERIFY_CLASS_SIZE_NODIE(ANxTornadoAngularForceFieldCapsule)
VERIFY_CLASS_OFFSET_NODIE(ANxTornadoForceField,NxTornadoForceField,RadialStrength)
VERIFY_CLASS_OFFSET_NODIE(ANxTornadoForceField,NxTornadoForceField,Kernel)
VERIFY_CLASS_SIZE_NODIE(ANxTornadoForceField)
VERIFY_CLASS_OFFSET_NODIE(ANxTornadoForceFieldCapsule,NxTornadoForceFieldCapsule,RenderComponent)
VERIFY_CLASS_SIZE_NODIE(ANxTornadoForceFieldCapsule)
VERIFY_CLASS_OFFSET_NODIE(ANxForceFieldSpawnable,NxForceFieldSpawnable,ForceFieldComponent)
VERIFY_CLASS_SIZE_NODIE(ANxForceFieldSpawnable)
VERIFY_CLASS_OFFSET_NODIE(ARB_CylindricalForceActor,RB_CylindricalForceActor,RenderComponent)
VERIFY_CLASS_OFFSET_NODIE(ARB_CylindricalForceActor,RB_CylindricalForceActor,CollideWithChannels)
VERIFY_CLASS_SIZE_NODIE(ARB_CylindricalForceActor)
VERIFY_CLASS_OFFSET_NODIE(ARB_RadialForceActor,RB_RadialForceActor,RenderComponent)
VERIFY_CLASS_OFFSET_NODIE(ARB_RadialForceActor,RB_RadialForceActor,CollideWithChannels)
VERIFY_CLASS_SIZE_NODIE(ARB_RadialForceActor)
VERIFY_CLASS_OFFSET_NODIE(UNxForceFieldComponent,NxForceFieldComponent,Shape)
VERIFY_CLASS_OFFSET_NODIE(UNxForceFieldComponent,NxForceFieldComponent,RBPhysScene)
VERIFY_CLASS_SIZE_NODIE(UNxForceFieldComponent)
VERIFY_CLASS_OFFSET_NODIE(UNxForceFieldCylindricalComponent,NxForceFieldCylindricalComponent,RadialStrength)
VERIFY_CLASS_OFFSET_NODIE(UNxForceFieldCylindricalComponent,NxForceFieldCylindricalComponent,Kernel)
VERIFY_CLASS_SIZE_NODIE(UNxForceFieldCylindricalComponent)
VERIFY_CLASS_OFFSET_NODIE(UNxForceFieldGenericComponent,NxForceFieldGenericComponent,RoughExtentX)
VERIFY_CLASS_OFFSET_NODIE(UNxForceFieldGenericComponent,NxForceFieldGenericComponent,Kernel)
VERIFY_CLASS_SIZE_NODIE(UNxForceFieldGenericComponent)
VERIFY_CLASS_OFFSET_NODIE(UNxForceFieldRadialComponent,NxForceFieldRadialComponent,ForceStrength)
VERIFY_CLASS_OFFSET_NODIE(UNxForceFieldRadialComponent,NxForceFieldRadialComponent,Kernel)
VERIFY_CLASS_SIZE_NODIE(UNxForceFieldRadialComponent)
VERIFY_CLASS_OFFSET_NODIE(UNxForceFieldTornadoComponent,NxForceFieldTornadoComponent,RadialStrength)
VERIFY_CLASS_OFFSET_NODIE(UNxForceFieldTornadoComponent,NxForceFieldTornadoComponent,Kernel)
VERIFY_CLASS_SIZE_NODIE(UNxForceFieldTornadoComponent)
VERIFY_CLASS_SIZE_NODIE(UForceFieldShape)
VERIFY_CLASS_OFFSET_NODIE(UForceFieldShapeBox,ForceFieldShapeBox,Shape)
VERIFY_CLASS_SIZE_NODIE(UForceFieldShapeBox)
VERIFY_CLASS_OFFSET_NODIE(UForceFieldShapeCapsule,ForceFieldShapeCapsule,Shape)
VERIFY_CLASS_SIZE_NODIE(UForceFieldShapeCapsule)
VERIFY_CLASS_OFFSET_NODIE(UForceFieldShapeSphere,ForceFieldShapeSphere,Shape)
VERIFY_CLASS_SIZE_NODIE(UForceFieldShapeSphere)
#endif // VERIFY_CLASS_SIZES
#endif // !ENUMS_ONLY

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif