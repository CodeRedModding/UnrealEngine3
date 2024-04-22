/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionComment extends MaterialExpression
	native(Material);

var int		PosX;
var int		PosY;
var int		SizeX;
var int		SizeY;
var() string	Text;

cpptext
{
	/**
	 * Text description of this expression.
	 */
	virtual FString GetCaption() const;

	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return TRUE if the expression matches the search query
     */
	virtual UBOOL MatchesSearchQuery( const TCHAR* SearchQuery );
}

defaultproperties
{
	MenuCategories(0)="Utility"
}
