/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AICommandBase extends Object within AIController
	native(AI)
	abstract;

/** 
 * When determining the utility value one can think of it in these terms:
 *
 * -how important is doing this action compared to other actions
 * -how exciting is doing this action compared to other actions
 * 
 * e.g. I have 2 idle actions; reading a newspaper and picking on a pedestrian.   Picking on the pedestrian is more existing
 *      so it should be higher rated than reading the newspaper
 *
 * e.g. I have an action am drinking ambrosia and responding to a threat.  In this case drinking ambrosia is really 
 *      important.   At the same importance as "EngageThreat" classification.  So we will add EngageThreat.UtilityStartVal
 *      to our utilty score to represent that.
 *
 *
 * Utility functions should be checking for the data that says whether or not something occured.  They should NOT be
 * checking for things like:  If you were in an Idle Action and/or if your current Action has some property set. 
 * That is bad as that is causing undue coupling between Actions.
 *
 * Additionally, the Utilty Function rules all.  Period.
 *
 * If things are not correctly occurring then the utility function is broken in some way.
 * One should not try to set special bools on blackboard/controller/active state and then look for them
 * 
 * If the current set of stimuli is not "valid" / "able to have data for the utility" then we need to
 * more than likely add some generalized functionality to it.
 *
 * If that can not be done then we need to start along the "bool cloud" path in the stimulus struct  But that should be the last option.
 *
 *
 *
 **/
static event int GetUtility( AIController InAI )
{
	`warn( "AICommandBase Base Class GetUtility was called.  Please have your Parent Type or your indiv AICmd implement this function" );
	ScriptTrace();
	return -1;
}

defaultproperties
{
}