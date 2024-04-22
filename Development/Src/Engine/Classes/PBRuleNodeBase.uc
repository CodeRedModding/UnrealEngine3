/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeBase extends Object
	native(ProcBuilding)
	hidecategories(Object)	
	editinlinenew
	abstract;


struct native PBRuleLink
{
	var()   instanced PBRuleNodeBase    NextRule;
	var()   name                        LinkName;
	var	    editoronly int			    DrawY;	
};

var editfixedsize array<PBRuleLink>   NextRules;

// Editor stuff

/** User defined comment, shown above node */
var() editoronly string Comment;

/** Visual X position of this rule in editor */
var editoronly int  RulePosX;
/** Visual Y position of this rule in editor */
var editoronly int  RulePosY;

var editoronly int  InDrawY;
var editoronly int  DrawWidth;
var editoronly int  DrawHeight;

cpptext
{
	/** 
	 *  Perform this nodes rule on the supplied Scope, and then call next rule nodes 
	 *  @param InScope              2D region that this rule should process
	 *  @param TopLevelScopeIndex   Index into the TopLevelScopes array in the ProcBuilding actor that this scope comes from
	 *  @param BaseBuilding         Building that results of this proc building will become part of, root of attachment tree
	 *  @param ScopeBuilding        Building that the top-level scope originally comes from.
	 */
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent) {}
	
	/** Get list of all rule nodes that follow this one (including this one) */
	virtual void GetRuleNodes(TArray<UPBRuleNodeBase*>& OutRuleNodes);

	/** Function to return the top or bottom most corner rule below this rule in the graph */
	virtual class UPBRuleNodeCorner* GetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex);

	// Editor
	/** Util to try and fix up current connections based on an old set of connections, using connection name */
	void FixUpConnections(TArray<FPBRuleLink>& OldConnections);

	virtual FString GetRuleNodeTitle();
	virtual FColor GetRuleNodeTitleColor();
	virtual FString GetRuleNodeOutputName(INT ConnIndex);

	/** Called when an instance of this rule node is placed in Facade */
	virtual void RuleNodeCreated(UProcBuildingRuleset* Ruleset) {}

	virtual void DrawRuleNode(FLinkedObjectDrawHelper* InHelper, FViewport* Viewport, FCanvas* Canvas, UBOOL bSelected);

	/** Allows custom visualization drawing*/
	virtual FIntPoint GetVisualizationSize(void) { return FIntPoint::ZeroValue();}
	/**
	 * Custom visualization that can be specified per node
	 */
	virtual void DrawVisualization(FLinkedObjectDrawHelper* InHelper, FViewport* Viewport, FCanvas* Canvas, const FIntPoint& InDrawPosition) {}
	virtual FIntPoint GetConnectionLocation(INT ConnType, INT ConnIndex);
}

	
defaultproperties
{
	NextRules[0]=(LinkName="Next")
}