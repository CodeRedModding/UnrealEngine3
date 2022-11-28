class ActorFactoryVehicle extends ActorFactory
	config(Editor)
	native;

cpptext
{
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);
};

var() class<Vehicle>	VehicleClass;

defaultproperties
{
	VehicleClass=class'Vehicle'
	bPlaceable=false
}
