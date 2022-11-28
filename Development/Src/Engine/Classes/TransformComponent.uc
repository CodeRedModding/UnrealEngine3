class TransformComponent extends ActorComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var native const matrix	CachedParentToWorld;

var() const editinlinenotify ActorComponent	TransformedComponent;
var() const vector							Translation;
var() const rotator							Rotation;
var() const float							Scale;
var() const vector							Scale3D;

var() const bool	AbsoluteTranslation,
					AbsoluteRotation,
					AbsoluteScale;

cpptext
{
	/**
	 * Calls SetParentToWorld on TransformedComponent.
	 */
	void SetTransformedToWorld();

	// UActorComponent interface.

	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Created();
	virtual void Update();
	virtual void Destroyed();
	virtual void Tick(FLOAT DeltaTime);
	virtual UBOOL IsValidComponent() const { return TransformedComponent && TransformedComponent->IsValidComponent(); }
}

native function SetTransformedComponent(ActorComponent NewTransformedComponent);
native function SetTranslation(vector NewTranslation);
native function SetRotation(rotator NewRotation);
native function SetScale(float NewScale);
native function SetScale3D(vector NewScale3D);
native function SetAbsolute(optional bool NewAbsoluteTranslation,optional bool NewAbsoluteRotation,optional bool NewAbsoluteScale);

defaultproperties
{
	Scale=1.0
	Scale3D=(X=1.0,Y=1.0,Z=1.0)
}