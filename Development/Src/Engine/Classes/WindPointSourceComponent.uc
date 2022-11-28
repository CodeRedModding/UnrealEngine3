class WindPointSourceComponent extends ActorComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var vector	Position;

var() float	Strength,
			Phase,
			Frequency,
			Radius,
			Duration;

cpptext
{
	// UActorComponent interface.

	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Created();
	virtual void Destroyed();

	// GetRenderData

	FWindPointSource GetRenderData() const;
}

defaultproperties
{
	Strength=1.0
	Frequency=1.0
	Radius=1024.0
	Duration=0.0
}