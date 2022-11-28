//=============================================================================
// LevelSummary contains the summary properties from the LevelInfo actor.
// Designed for fast loading.
//=============================================================================
class LevelSummary extends Object
	native;

//-----------------------------------------------------------------------------
// Properties.

// From LevelInfo.
var() localized string Title;
var()           string Author;
var() int	IdealPlayerCount;

cpptext
{
	// Constructors.
	ULevelSummary()
	{}

	// UObject interface.
	void PostLoad()
	{
		Super::PostLoad();
		FString Text = Localize( TEXT("LevelInfo0"), TEXT("Title"), GetOuter()->GetName(), NULL, 1 );
		if( Text.Len() )
			Title = Text;
	}
}

defaultproperties
{
}
