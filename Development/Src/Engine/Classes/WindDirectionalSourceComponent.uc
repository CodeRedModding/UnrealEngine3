class WindDirectionalSourceComponent extends ActorComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var vector	Direction;

var() float	Strength,
			Phase,
			Frequency;

cpptext
{
	// UActorComponent interface.

	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Created();
	virtual void Destroyed();

	// GetRenderData

	FWindDirectionSource GetRenderData() const;
}

defaultproperties
{
	Strength=1.0
	Frequency=1.0
}