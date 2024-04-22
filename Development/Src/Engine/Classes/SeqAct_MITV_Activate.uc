/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_MITV_Activate extends SequenceAction;


/** This is how long this MITV should last **/
var() float DurationOfMITV;


static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}


/**
 * Called when this event is activated.
 */
event Activated()
{
	local SeqVar_Object ObjVar;
	local MaterialInstanceTimeVaryingActor MITVA;

	// find the possibly linked ChaosZoneInfo 
	foreach LinkedVariables( class'SeqVar_Object', ObjVar, "MITV" )
	{
		MITVA = MaterialInstanceTimeVaryingActor(ObjVar.GetObjectValue());
		
		if( MITVA != None )
		{
			if( MITVA.MatInst != None )
			{
				MITVA.MatInst.SetDuration( DurationOfMITV );
			}
		}
	}	
}





defaultproperties
{
	ObjName="MITV Activate"
	ObjCategory="Actor"

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="MITV",PropertyName=Targets)
	bCallHandler=FALSE
}

