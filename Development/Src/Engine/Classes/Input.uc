//=============================================================================
// Input
// Object that maps key events to key bindings
//=============================================================================

class Input extends Interaction
	native
	config(Input)
	noexport;

struct KeyBind
{
	var config name		Name;
	var config string	Command;
	var config bool		Control,
						Shift,
						Alt;
};

var config array<KeyBind>	Bindings;
var const array<name>		PressedKeys;

var const EInputEvent		CurrentEvent;
var const float				CurrentDelta;
var const float				CurrentDeltaTime;

var native const Map_Mirror		NameToPtr;
var native const array<pointer>	AxisArray;
	
exec function SetBind(name Name,string Command)
{
	local KeyBind	NewBind;
	local int		BindIndex;

	for(BindIndex = 0;BindIndex < Bindings.Length;BindIndex++)
		if(Bindings[BindIndex].Name == Name)
		{
			Bindings[BindIndex].Command = Command;
			return;
		}

	NewBind.Name = Name;
	NewBind.Command = Command;
	Bindings[Bindings.Length] = NewBind;
	SaveConfig();
}