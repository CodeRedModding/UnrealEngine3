class RB_BodyInstance extends Object
	native(Physics);

cpptext
{
	void InitBody(class URB_BodySetup* setup, const FMatrix& transform, const FVector& Scale3D, UBOOL bFixed, UPrimitiveComponent* PrimComp);
	void TermBody();

	void SetFixed(UBOOL bNewFixed);

	FMatrix GetUnrealWorldTM();
	FVector GetUnrealWorldVelocity();

	void DrawCOMPosition( FPrimitiveRenderInterface* PRI, FLOAT COMRenderSize, const FColor& COMRenderColor );

#ifdef WITH_NOVODEX
	class NxActor* GetNxActor();
#endif
}

/** Actor using a PhysicsAsset containing this body. */
var const transient Actor					Owner; 

/** Index of this BodyInstance within the PhysicsAssetInstance/PhysicsAsset. */
var const int								BodyIndex;

/** Current linear velocity of this BodyInstance. */
var vector									Velocity;

/** Internal use. Physics-engine representation of this body. */
var const native pointer					BodyData;
