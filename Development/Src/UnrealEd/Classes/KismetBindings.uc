class KismetBindings extends Object
	native
	config(Editor);

struct native KismetKeyBind
{
	var config name						Key;
	var config bool						bControl;
	var config class<SequenceObject>	SeqObjClass;
};

var config array<KismetKeyBind>	Bindings;
