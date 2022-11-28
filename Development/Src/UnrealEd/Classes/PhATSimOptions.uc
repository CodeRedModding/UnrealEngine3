class PhATSimOptions extends Object	
	hidecategories(Object)
	config(Editor)
	native;	

var(Simulation)		config float		SimSpeed;
var(Simulation)		config bool			bDrawContacts;
var(Simulation)		config float		FloorGap;

var(MouseSpring)	config float		MouseLinearDamping;
var(MouseSpring)	config float		MouseLinearStiffness;
var(MouseSpring)	config float		MouseLinearMaxDistance;

var(MouseSpring)	config float		MouseAngularDamping;
var(MouseSpring)	config float		MouseAngularStiffness;

var(Lighting)		config float		LightDirection; // degrees
var(Lighting)		config float		LightElevation; // degrees
var(Lighting)		config float		SkyBrightness;
var(Lighting)		config color		SkyColor;
var(Lighting)		config float		Brightness;
var(Lighting)		config color		Color;

var(Snap)			config float		AngularSnap;
var(Snap)			config float		LinearSnap;

var(Advanced)		config bool			bPromptOnBoneDelete;

defaultproperties
{
	SkyBrightness=0.25
	SkyColor=(R=255,G=255,B=255)
	Brightness=1.0
	Color=(R=255,G=255,B=255)
	LightDirection=45
	LightElevation=45

	AngularSnap=15.0
	LinearSnap=2.0

	SimSpeed=1.0
	bDrawContacts=false
	FloorGap=25.0
	bPromptOnBoneDelete=true
}