class SeqAct_ModifyProperty extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void Activated();
	virtual void PostEditChange(UProperty *PropertyThatChanged);
};

/** Class of object to modify */
var() class<Object> ObjectClass;

/** Previous object class, to determine whether or not to rebuild the property list */
var class<Object> PrevObjectClass;

/** Should the property list be autofilled? */
var() bool bAutoFill;

/**
 * Struct used to figure out which properties to modify.
 */
struct native PropertyInfo
{
	/** Name of the property to modify */
	var() Name PropertyName;

	/** Should this property be modified? */
	var() bool bModifyProperty;

	/** New value to apply to the property */
	var() string PropertyValue;
};

/** List of properties that can be modified */
var() editinline array<PropertyInfo> Properties;

defaultproperties
{
	ObjName="Modify Property"
	bAutoFill=true
}
