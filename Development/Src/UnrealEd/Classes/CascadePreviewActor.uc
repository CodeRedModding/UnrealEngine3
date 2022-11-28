class CascadePreviewActor extends Actor
	native
	collapsecategories
	hidecategories(Actor)
	editinlinenew;

var ParticleSystemComponent PartSysComp;
var CascadePreviewComponent CascPrevComp;

defaultproperties
{
	Components.Remove(Sprite)
	
	Begin Object Class=ParticleSystemComponent Name=ParticleSystemComponent0
	End Object
	PartSysComp=ParticleSystemComponent0
	Components.Add(ParticleSystemComponent0)

	Begin Object Class=CascadePreviewComponent Name=CascadePreviewComponent0
	End Object
	CascPrevComp=CascadePreviewComponent0
	Components.Add(CascadePreviewComponent0)
}
