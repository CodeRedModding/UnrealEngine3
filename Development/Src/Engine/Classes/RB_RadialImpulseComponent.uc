class RB_RadialImpulseComponent extends PrimitiveComponent
	collapsecategories
	hidecategories(Object)
	native(Physics);

var()	ERadialImpulseFalloff	ImpulseFalloff;
var()	float					ImpulseStrength;
var()	float					ImpulseRadius;
var()	bool					bVelChange;

var		DrawSphereComponent		PreviewSphere;

cpptext
{
	// UActorComponent interface
	virtual void Created();

	// RB_RadialImpulseComponent interface
	void FireImpulse();
}

native function FireImpulse();

defaultproperties
{
	ImpulseFalloff=RIF_Linear
	ImpulseStrength=900.0
	ImpulseRadius=200.0
}