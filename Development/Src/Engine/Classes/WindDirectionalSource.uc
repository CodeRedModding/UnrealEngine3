class WindDirectionalSource extends Info
	placeable;

var() const editconst WindDirectionalSourceComponent	Component;

defaultproperties
{
	Begin Object Class=WindDirectionalSourceComponent Name=WindDirectionalSourceComponent0
	End Object
	Component=WindDirectionalSourceComponent0
	Components.Add(WindDirectionalSourceComponent0)

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=150,G=200,B=255)
	End Object
	Components.Add(ArrowComponent0)

	bStatic=True
}