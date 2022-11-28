class AnimNotify extends Object
	native(Anim)
	abstract
	editinlinenew
	hidecategories(Object)
	collapsecategories;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq ) {};

	// UObject interface.
	virtual void PostEditChange(UProperty* PropertyThatChanged);
}