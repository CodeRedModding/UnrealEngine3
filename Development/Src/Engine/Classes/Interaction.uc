class Interaction extends Object
	native
	noexport
	transient;

enum EInputEvent
{
	IE_Pressed,
	IE_Released,
	IE_Repeat,
	IE_DoubleClick,
	IE_Axis
};

var PlayerController	Player;

/**
 * Callback used to receive keyboard presses/button presses
 *
 * @param Key The name of the key/button being pressed
 * @param Event The type of key/button event being fired
 * @param AmountDepressed The amount the button/key is being depressed (0.0 to 1.0)
 *
 * @return Whether the event was processed or not
 */
event bool InputKey(name Key,EInputEvent Event,float AmountDepressed);

event bool InputAxis(name Key,float Delta,float DeltaTime);
event bool InputChar(string Unicode);
event Tick(float DeltaTime);

defaultproperties
{
}
