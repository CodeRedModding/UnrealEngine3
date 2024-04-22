/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


/** this handles replicating cover changes to a client
 * can't use variable replication on the CoverLinks because the slots list is a dynamic array
 * also, that could potentially be a whole lot of channels if LDs mess with a lot of cover via Kismet, so this is more efficient
 */
class CoverReplicator extends ReplicationInfo;

struct ManualCoverTypeInfo
{
	var byte SlotIndex;
	var ECoverType ManualCoverType;
};

struct CoverReplicationInfo
{
	/** CoverLink that was changed */
	var CoverLink Link;
	/** indices of slots that were enabled */
	var array<byte> SlotsEnabled;
	/** indices of slots that were disabled */
	var array<byte> SlotsDisabled;
	/** indices of slots that were adjusted */
	var array<byte> SlotsAdjusted;
	/** slots that have had cover type manually set */
	var array<ManualCoverTypeInfo> SlotsCoverTypeChanged;
};

var array<CoverReplicationInfo> CoverReplicationData;

/** removes entries that are no longer valid (i.e. the CoverLink has been streamed out) */
function PurgeOldEntries()
{
	local int i;

	for (i = 0; i < CoverReplicationData.length; i++)
	{
		if (CoverReplicationData[i].Link == None)
		{
			CoverReplicationData.Remove(i--, 1);
		}
	}
}

/** copies and starts replicating already changed cover info */
function ReplicateInitialCoverInfo()
{
	local CoverReplicator CoverReplicatorBase;

	CoverReplicatorBase = WorldInfo.Game.GetCoverReplicator();
	// clean out old entries first
	CoverReplicatorBase.PurgeOldEntries();
	// copy over data
	CoverReplicationData = CoverReplicatorBase.CoverReplicationData;
	if (PlayerController(Owner) != None)
	{
		//@HACK: bSkipActorPropertyReplication prevents Owner from being replicated. Should fix that eventually, but for now, force it by RPC
		ClientSetOwner(PlayerController(Owner));

		// start replicating it out
		ServerSendInitialCoverReplicationInfo(0);
	}
}

//@HACK: bSkipActorPropertyReplication prevents Owner from being replicated. Should fix that eventually, but for now, force it by RPC
reliable client function ClientSetOwner(PlayerController PC)
{
	SetOwner(PC);
}

/** sends info for one CoverReplicationData to the client */
reliable server function ServerSendInitialCoverReplicationInfo(int Index)
{
	local byte SlotsArrayIndex,
			NumSlotsEnabled, NumSlotsDisabled, NumSlotsAdjusted, NumCoverTypesChanged,
			SlotsEnabled[8], SlotsDisabled[8], SlotsAdjusted[8];
	local ManualCoverTypeInfo SlotsCoverTypeChanged[8];
	local int i;
	local bool bDone;

	// verify there's no None entries as the client assumes that means the data must be resent
	while (Index < CoverReplicationData.length && CoverReplicationData[Index].Link == None)
	{
		CoverReplicationData.Remove(Index, 1);
	}

	if (Index < CoverReplicationData.length)
	{
		SlotsArrayIndex = 0;
		do
		{
			// figure out how many elements of each array to send
			NumSlotsEnabled = Clamp(CoverReplicationData[Index].SlotsEnabled.length - SlotsArrayIndex, 0, 8);
			NumSlotsDisabled = Clamp(CoverReplicationData[Index].SlotsDisabled.length - SlotsArrayIndex, 0, 8);
			NumSlotsAdjusted = Clamp(CoverReplicationData[Index].SlotsAdjusted.length - SlotsArrayIndex, 0, 8);
			NumCoverTypesChanged = Clamp(CoverReplicationData[Index].SlotsCoverTypeChanged.length - SlotsArrayIndex, 0, 8);

			// if we're sending nothing, make sure we zero the array so the netcode doesn't send that parameter at all
			if (NumSlotsEnabled == 0)
			{
				for (i = 0; i < 8; i++)
				{
					SlotsEnabled[i] = 0;
				}
			}
			else
			{
				// compose the data to send
				// we don't bother with elements after NumSlotsEnabled as the netcode will send them regardless
				for (i = 0; i < NumSlotsEnabled; i++)
				{
					SlotsEnabled[i] = CoverReplicationData[Index].SlotsEnabled[SlotsArrayIndex + i];
				}
			}

			// do the same for the other three arrays

			if (NumSlotsDisabled == 0)
			{
				for (i = 0; i < 8; i++)
				{
					SlotsDisabled[i] = 0;
				}
			}
			else
			{
				for (i = 0; i < NumSlotsDisabled; i++)
				{
					SlotsDisabled[i] = CoverReplicationData[Index].SlotsDisabled[SlotsArrayIndex + i];
				}
			}

			if (NumSlotsAdjusted == 0)
			{
				for (i = 0; i < 8; i++)
				{
					SlotsAdjusted[i] = 0;
				}
			}
			else
			{
				for (i = 0; i < NumSlotsAdjusted; i++)
				{
					SlotsAdjusted[i] = CoverReplicationData[Index].SlotsAdjusted[SlotsArrayIndex + i];
				}
			}

			if (NumCoverTypesChanged == 0)
			{
				for (i = 0; i < 8; i++)
				{
					SlotsCoverTypeChanged[i].SlotIndex = 0;
					SlotsCoverTypeChanged[i].ManualCoverType = CT_None;
				}
			}
			else
			{
				for (i = 0; i < NumCoverTypesChanged; i++)
				{
					SlotsCoverTypeChanged[i] = CoverReplicationData[Index].SlotsCoverTypeChanged[SlotsArrayIndex + i];
				}
			}

			// we're done if all of the arrays have 8 elements to send or less
			bDone = ( CoverReplicationData[Index].SlotsEnabled.length - SlotsArrayIndex <= 8 &&
					CoverReplicationData[Index].SlotsDisabled.length - SlotsArrayIndex <= 8 &&
					CoverReplicationData[Index].SlotsAdjusted.length - SlotsArrayIndex <= 8 &&
					CoverReplicationData[Index].SlotsCoverTypeChanged.length - SlotsArrayIndex <= 8 );

			// send out the data
			ClientReceiveInitialCoverReplicationInfo( Index, CoverReplicationData[Index].Link, CoverReplicationData[Index].Link.bDisabled,
								NumSlotsEnabled, SlotsEnabled,
								NumSlotsDisabled, SlotsDisabled,
								NumSlotsAdjusted, SlotsAdjusted,
								NumCoverTypesChanged, SlotsCoverTypeChanged,
								bDone );

			// increment base array index
			SlotsArrayIndex += 8;

		} until (bDone);
	}
}

/** replicates the information for one CoverReplicationData entry
 * bDone indicates whether or not there is more data coming for this entry (because some arrays have more than 8 elements)
 */
reliable client function ClientReceiveInitialCoverReplicationInfo( int Index, CoverLink Link, bool bLinkDisabled,
								byte NumSlotsEnabled, byte SlotsEnabled[8],
								byte NumSlotsDisabled, byte SlotsDisabled[8],
								byte NumSlotsAdjusted, byte SlotsAdjusted[8],
								byte NumCoverTypesChanged, ManualCoverTypeInfo SlotsCoverTypeChanged[8],
								bool bDone )
{
	local int i;

	// if we received an invalid CoverLink, we might not have loaded that level yet, so ask for a resend
	if (Link == None)
	{
		if (bDone)
		{
			ServerSendInitialCoverReplicationInfo(Index);
		}
	}
	else
	{
		// process the data
		Link.bDisabled = bLinkDisabled;
		for (i = 0; i < NumSlotsEnabled; i++)
		{
			Link.SetSlotEnabled(SlotsEnabled[i], true);
		}
		for (i = 0; i < NumSlotsDisabled; i++)
		{
			Link.SetSlotEnabled(SlotsDisabled[i], false);
		}
		for (i = 0; i < NumSlotsAdjusted; i++)
		{
			if (Link.AutoAdjustSlot(SlotsAdjusted[i], false) && Link.Slots[SlotsAdjusted[i]].SlotOwner != None && Link.Slots[SlotsAdjusted[i]].SlotOwner.Controller != None)
			{
				Link.Slots[SlotsAdjusted[i]].SlotOwner.Controller.NotifyCoverAdjusted();
			}
		}
		for (i = 0; i < NumCoverTypesChanged; i++)
		{
			Link.Slots[SlotsCoverTypeChanged[i].SlotIndex].CoverType = SlotsCoverTypeChanged[i].ManualCoverType;
			if (Link.Slots[SlotsCoverTypeChanged[i].SlotIndex].SlotOwner != None && Link.Slots[SlotsCoverTypeChanged[i].SlotIndex].SlotOwner.Controller != None)
			{
				Link.Slots[SlotsCoverTypeChanged[i].SlotIndex].SlotOwner.Controller.NotifyCoverAdjusted();
			}
		}

		// ask for next
		if (bDone)
		{
			ServerSendInitialCoverReplicationInfo(Index + 1);
		}
	}
}

/** notification that slots on the given CoverLink have been enabled */
function NotifyEnabledSlots(CoverLink Link, const out array<int> SlotIndices)
{
	local int Index, SlotIndex;
	local int i;
	local PlayerController PC;

	Index = CoverReplicationData.Find('Link', Link);
	if (Index == INDEX_NONE)
	{
		Index = CoverReplicationData.length;
		CoverReplicationData.length = CoverReplicationData.length + 1;
		CoverReplicationData[Index].Link = Link;
		for (i = 0; i < SlotIndices.length; i++)
		{
			CoverReplicationData[Index].SlotsEnabled[i] = SlotIndices[i];
		}
	}
	else
	{
		for (i = 0; i < SlotIndices.length; i++)
		{
			// add to enabled list if not already there
			SlotIndex = CoverReplicationData[Index].SlotsEnabled.Find(SlotIndices[i]);
			if (SlotIndex == INDEX_NONE)
			{
				CoverReplicationData[Index].SlotsEnabled[CoverReplicationData[Index].SlotsEnabled.length] = SlotIndices[i];
			}
			// remove from disabled list, if necessary
			SlotIndex = CoverReplicationData[Index].SlotsDisabled.Find(SlotIndices[i]);
			if (SlotIndex != INDEX_NONE)
			{
				CoverReplicationData[Index].SlotsDisabled.Remove(SlotIndex, 1);
			}
		}
	}

	if (WorldInfo.Game.GetCoverReplicator() == self)
	{
		// we are the base info; inform players of the change now
		foreach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			if (PC.MyCoverReplicator == None)
			{
				PC.SpawnCoverReplicator();
			}
			else
			{
				PC.MyCoverReplicator.NotifyEnabledSlots(Link, SlotIndices);
			}
		}
	}

	if (PlayerController(Owner) != None)
	{
		// replicate the data for this action
		ServerSendEnabledSlots(Index);
	}
}

/** send just the enabled slots for the CoverLink at the given index */
reliable server function ServerSendEnabledSlots(int Index)
{
	local int SlotsArrayIndex;
	local byte NumSlotsEnabled, SlotsEnabled[8];
	local int i;
	local bool bDone;

	if (CoverReplicationData[Index].Link != None)
	{
		SlotsArrayIndex = 0;
		do
		{
			NumSlotsEnabled = Clamp(CoverReplicationData[Index].SlotsEnabled.length - SlotsArrayIndex, 0, 8);

			// compose the data to send
			// we don't bother with elements after NumSlotsEnabled as the netcode will send them regardless
			for (i = 0; i < NumSlotsEnabled; i++)
			{
				SlotsEnabled[i] = CoverReplicationData[Index].SlotsEnabled[SlotsArrayIndex + i];
			}

			// we're done if the array has 8 elements to send or less
			bDone = (CoverReplicationData[Index].SlotsEnabled.length - SlotsArrayIndex <= 8);

			// send out the data
			ClientReceiveEnabledSlots(Index, CoverReplicationData[Index].Link, NumSlotsEnabled, SlotsEnabled, bDone);

			// increment base array index
			SlotsArrayIndex += 8;

		} until (bDone);
	}
}

/** client receives just the enabled slots for the given CoverLink */
reliable client function ClientReceiveEnabledSlots(int Index, CoverLink Link, byte NumSlotsEnabled, byte SlotsEnabled[8], bool bDone)
{
	local int i;

	// if we received an invalid CoverLink, we might not have loaded that level yet, so ask for a resend
	if (Link == None)
	{
		if (bDone)
		{
			ServerSendEnabledSlots(Index);
		}
	}
	else
	{
		// process the data
		for (i = 0; i < NumSlotsEnabled; i++)
		{
			Link.SetSlotEnabled(SlotsEnabled[i], true);
		}
	}
}

/** notification that the slots on the given CoverLink have been disabled */
function NotifyDisabledSlots(CoverLink Link, const out array<int> SlotIndices)
{
	local int Index, SlotIndex;
	local int i;
	local PlayerController PC;

	Index = CoverReplicationData.Find('Link', Link);
	if (Index == INDEX_NONE)
	{
		Index = CoverReplicationData.length;
		CoverReplicationData.length = CoverReplicationData.length + 1;
		CoverReplicationData[Index].Link = Link;
		for (i = 0; i < SlotIndices.length; i++)
		{
			CoverReplicationData[Index].SlotsDisabled[i] = SlotIndices[i];
		}
	}
	else
	{
		for (i = 0; i < SlotIndices.length; i++)
		{
			// add to disabled list if not already there
			SlotIndex = CoverReplicationData[Index].SlotsDisabled.Find(SlotIndices[i]);
			if (SlotIndex == INDEX_NONE)
			{
				CoverReplicationData[Index].SlotsDisabled[CoverReplicationData[Index].SlotsDisabled.length] = SlotIndices[i];
			}
			// remove from enabled list, if necessary
			SlotIndex = CoverReplicationData[Index].SlotsEnabled.Find(SlotIndices[i]);
			if (SlotIndex != INDEX_NONE)
			{
				CoverReplicationData[Index].SlotsEnabled.Remove(SlotIndex, 1);
			}
		}
	}

	if (WorldInfo.Game.GetCoverReplicator() == self)
	{
		// we are the base info; inform players of the change now
		foreach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			if (PC.MyCoverReplicator == None)
			{
				PC.SpawnCoverReplicator();
			}
			else
			{
				PC.MyCoverReplicator.NotifyDisabledSlots(Link, SlotIndices);
			}
		}
	}

	if (PlayerController(Owner) != None)
	{
		// replicate the data for this action
		ServerSendDisabledSlots(Index);
	}
}

/** send just the disabled slots for the CoverLink at the given index */
reliable server function ServerSendDisabledSlots(int Index)
{
	local int SlotsArrayIndex;
	local byte NumSlotsDisabled, SlotsDisabled[8];
	local int i;
	local bool bDone;

	if (CoverReplicationData[Index].Link != None)
	{
		SlotsArrayIndex = 0;
		do
		{
			NumSlotsDisabled = Clamp(CoverReplicationData[Index].SlotsDisabled.length - SlotsArrayIndex, 0, 8);

			// compose the data to send
			// we don't bother with elements after NumSlotsDisabled as the netcode will send them regardless
			for (i = 0; i < NumSlotsDisabled; i++)
			{
				SlotsDisabled[i] = CoverReplicationData[Index].SlotsDisabled[SlotsArrayIndex + i];
			}

			// we're done if the array has 8 elements to send or less
			bDone = (CoverReplicationData[Index].SlotsDisabled.length - SlotsArrayIndex <= 8);

			// send out the data
			ClientReceiveDisabledSlots(Index, CoverReplicationData[Index].Link, NumSlotsDisabled, SlotsDisabled, bDone);

			// increment base array index
			SlotsArrayIndex += 8;

		} until (bDone);
	}
}

/** client receives just the disabled slots for the given CoverLink */
reliable client function ClientReceiveDisabledSlots(int Index, CoverLink Link, byte NumSlotsDisabled, byte SlotsDisabled[8], bool bDone)
{
	local int i;

	// if we received an invalid CoverLink, we might not have loaded that level yet, so ask for a resend
	if (Link == None)
	{
		if (bDone)
		{
			ServerSendDisabledSlots(Index);
		}
	}
	else
	{
		// process the data
		for (i = 0; i < NumSlotsDisabled; i++)
		{
			Link.SetSlotEnabled(SlotsDisabled[i], false);
		}
	}
}

/** notification that the slots on the given CoverLink have been auto-adjusted */
function NotifyAutoAdjustSlots(CoverLink Link, const out array<int> SlotIndices)
{
	local int Index, SlotIndex;
	local int i;
	local PlayerController PC;

	Index = CoverReplicationData.Find('Link', Link);
	if (Index == INDEX_NONE)
	{
		Index = CoverReplicationData.length;
		CoverReplicationData.length = CoverReplicationData.length + 1;
		CoverReplicationData[Index].Link = Link;
		for (i = 0; i < SlotIndices.length; i++)
		{
			CoverReplicationData[Index].SlotsAdjusted[i] = SlotIndices[i];
		}
	}
	else
	{
		for (i = 0; i < SlotIndices.length; i++)
		{
			// add to adjusted list if not already there
			SlotIndex = CoverReplicationData[Index].SlotsAdjusted.Find(SlotIndices[i]);
			if (SlotIndex == INDEX_NONE)
			{
				CoverReplicationData[Index].SlotsAdjusted[CoverReplicationData[Index].SlotsAdjusted.length] = SlotIndices[i];
			}
			// remove from manual list, if necessary
			SlotIndex = CoverReplicationData[Index].SlotsCoverTypeChanged.Find('SlotIndex', SlotIndices[i]);
			if (SlotIndex != INDEX_NONE)
			{
				CoverReplicationData[Index].SlotsCoverTypeChanged.Remove(SlotIndex, 1);
			}
		}
	}

	if (WorldInfo.Game.GetCoverReplicator() == self)
	{
		// we are the base info; inform players of the change now
		foreach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			if (PC.MyCoverReplicator == None)
			{
				PC.SpawnCoverReplicator();
			}
			else
			{
				PC.MyCoverReplicator.NotifyAutoAdjustSlots(Link, SlotIndices);
			}
		}
	}

	if (PlayerController(Owner) != None)
	{
		// replicate the data for this action
		ServerSendAdjustedSlots(Index);
	}
}

/** send just the auto-adjusted slots for the CoverLink at the given index */
reliable server function ServerSendAdjustedSlots(int Index)
{
	local int SlotsArrayIndex;
	local byte NumSlotsAdjusted, SlotsAdjusted[8];
	local int i;
	local bool bDone;

	if (CoverReplicationData[Index].Link != None)
	{
		SlotsArrayIndex = 0;
		do
		{
			NumSlotsAdjusted = Clamp(CoverReplicationData[Index].SlotsAdjusted.length - SlotsArrayIndex, 0, 8);

			// compose the data to send
			// we don't bother with elements after NumSlotsDisabled as the netcode will send them regardless
			for (i = 0; i < NumSlotsAdjusted; i++)
			{
				SlotsAdjusted[i] = CoverReplicationData[Index].SlotsAdjusted[SlotsArrayIndex + i];
			}

			// we're done if the array has 8 elements to send or less
			bDone = (CoverReplicationData[Index].SlotsAdjusted.length - SlotsArrayIndex <= 8);

			// send out the data
			ClientReceiveAdjustedSlots(Index, CoverReplicationData[Index].Link, NumSlotsAdjusted, SlotsAdjusted, bDone);

			// increment base array index
			SlotsArrayIndex += 8;

		} until (bDone);
	}
}

/** client receives just the auto-adjusted slots for the given CoverLink */
reliable client function ClientReceiveAdjustedSlots(int Index, CoverLink Link, byte NumSlotsAdjusted, byte SlotsAdjusted[8], bool bDone)
{
	local int i;

	// if we received an invalid CoverLink, we might not have loaded that level yet, so ask for a resend
	if (Link == None)
	{
		if (bDone)
		{
			ServerSendAdjustedSlots(Index);
		}
	}
	else
	{
		// process the data
		for (i = 0; i < NumSlotsAdjusted; i++)
		{
			if (Link.AutoAdjustSlot(SlotsAdjusted[i], true) && Link.Slots[SlotsAdjusted[i]].SlotOwner != None && Link.Slots[SlotsAdjusted[i]].SlotOwner.Controller != None)
			{
				Link.Slots[SlotsAdjusted[i]].SlotOwner.Controller.NotifyCoverAdjusted();
			}
		}
	}
}

/** notification that the slots on the given CoverLink have been manually adjusted */
function NotifySetManualCoverTypeForSlots(CoverLink Link, const out array<int> SlotIndices, ECoverType NewCoverType)
{
	local int Index, SlotIndex;
	local int i;
	local PlayerController PC;

	Index = CoverReplicationData.Find('Link', Link);
	if (Index == INDEX_NONE)
	{
		Index = CoverReplicationData.length;
		CoverReplicationData.length = CoverReplicationData.length + 1;
		CoverReplicationData[Index].Link = Link;
		CoverReplicationData[Index].SlotsCoverTypeChanged.length = SlotIndices.length;
		for (i = 0; i < SlotIndices.length; i++)
		{
			CoverReplicationData[Index].SlotsCoverTypeChanged[i].SlotIndex = SlotIndices[i];
			CoverReplicationData[Index].SlotsCoverTypeChanged[i].ManualCoverType = NewCoverType;
		}
	}
	else
	{
		for (i = 0; i < SlotIndices.length; i++)
		{
			// add to adjusted list if not already there
			SlotIndex = CoverReplicationData[Index].SlotsCoverTypeChanged.Find('SlotIndex', SlotIndices[i]);
			if (SlotIndex == INDEX_NONE)
			{
				SlotIndex = CoverReplicationData[Index].SlotsCoverTypeChanged.length;
				CoverReplicationData[Index].SlotsCoverTypeChanged.length = CoverReplicationData[Index].SlotsCoverTypeChanged.length + 1;
				CoverReplicationData[Index].SlotsCoverTypeChanged[SlotIndex].SlotIndex = SlotIndices[i];
			}
			CoverReplicationData[Index].SlotsCoverTypeChanged[SlotIndex].ManualCoverType = NewCoverType;
			// remove from auto list, if necessary
			SlotIndex = CoverReplicationData[Index].SlotsAdjusted.Find(SlotIndices[i]);
			if (SlotIndex != INDEX_NONE)
			{
				CoverReplicationData[Index].SlotsAdjusted.Remove(SlotIndex, 1);
			}
		}
	}

	if (WorldInfo.Game.GetCoverReplicator() == self)
	{
		// we are the base info; inform players of the change now
		foreach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			if (PC.MyCoverReplicator == None)
			{
				PC.SpawnCoverReplicator();
			}
			else
			{
				PC.MyCoverReplicator.NotifySetManualCoverTypeForSlots(Link, SlotIndices, NewCoverType);
			}
		}
	}

	if (PlayerController(Owner) != None)
	{
		// replicate the data for this action
		ServerSendManualCoverTypeSlots(Index);
	}
}

/** send just the manual adjusted slots for the CoverLink at the given index */
reliable server function ServerSendManualCoverTypeSlots(int Index)
{
	local int SlotsArrayIndex;
	local byte NumCoverTypesChanged;
	local ManualCoverTypeInfo SlotsCoverTypeChanged[8];
	local int i;
	local bool bDone;

	if (CoverReplicationData[Index].Link != None)
	{
		SlotsArrayIndex = 0;
		do
		{
			NumCoverTypesChanged = Clamp(CoverReplicationData[Index].SlotsCoverTypeChanged.length - SlotsArrayIndex, 0, 8);

			// compose the data to send
			// we don't bother with elements after NumSlotsDisabled as the netcode will send them regardless
			for (i = 0; i < NumCoverTypesChanged; i++)
			{
				SlotsCoverTypeChanged[i] = CoverReplicationData[Index].SlotsCoverTypeChanged[SlotsArrayIndex + i];
			}

			// we're done if the array has 8 elements to send or less
			bDone = (CoverReplicationData[Index].SlotsCoverTypeChanged.length - SlotsArrayIndex <= 8);

			// send out the data
			ClientReceiveManualCoverTypeSlots(Index, CoverReplicationData[Index].Link, NumCoverTypesChanged, SlotsCoverTypeChanged, bDone);

			// increment base array index
			SlotsArrayIndex += 8;

		} until (bDone);
	}
}

/** client receives just the manual adjusted slots for the given CoverLink */
reliable client function ClientReceiveManualCoverTypeSlots( int Index, CoverLink Link, byte NumCoverTypesChanged,
								ManualCoverTypeInfo SlotsCoverTypeChanged[8], bool bDone )
{
	local int i;

	// if we received an invalid CoverLink, we might not have loaded that level yet, so ask for a resend
	if (Link == None)
	{
		if (bDone)
		{
			ServerSendManualCoverTypeSlots(Index);
		}
	}
	else
	{
		// process the data
		for (i = 0; i < NumCoverTypesChanged; i++)
		{
			Link.Slots[SlotsCoverTypeChanged[i].SlotIndex].CoverType = SlotsCoverTypeChanged[i].ManualCoverType;
			if (Link.Slots[SlotsCoverTypeChanged[i].SlotIndex].SlotOwner != None && Link.Slots[SlotsCoverTypeChanged[i].SlotIndex].SlotOwner.Controller != None)
			{
				Link.Slots[SlotsCoverTypeChanged[i].SlotIndex].SlotOwner.Controller.NotifyCoverAdjusted();
			}
		}
	}
}

function NotifyLinkDisabledStateChange(CoverLink Link)
{
	local int Index;
	local PlayerController PC;

	Index = CoverReplicationData.Find('Link', Link);
	if (Index == INDEX_NONE)
	{
		Index = CoverReplicationData.length;
		CoverReplicationData.length = CoverReplicationData.length + 1;
		CoverReplicationData[Index].Link = Link;
	}

	if (WorldInfo.Game.GetCoverReplicator() == self)
	{
		// we are the base info; inform players of the change now
		foreach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			if (PC.MyCoverReplicator == None)
			{
				PC.SpawnCoverReplicator();
			}
			else
			{
				PC.MyCoverReplicator.NotifyLinkDisabledStateChange(Link);
			}
		}
	}

	if (PlayerController(Owner) != None)
	{
		// replicate the data for this action
		ServerSendLinkDisabledState(Index);
	}
}

reliable server function ServerSendLinkDisabledState(int Index)
{
	if (CoverReplicationData[Index].Link != None)
	{
		ClientReceiveLinkDisabledState(Index, CoverReplicationData[Index].Link, CoverReplicationData[Index].Link.bDisabled);
	}
}

reliable client function ClientReceiveLinkDisabledState(int Index, CoverLink Link, bool bLinkDisabled)
{
	// if we received an invalid CoverLink, we might not have loaded that level yet, so ask for a resend
	if (Link == None)
	{
		ServerSendLinkDisabledState(Index);
	}
	else
	{
		Link.bDisabled = bLinkDisabled;
	}
}

defaultproperties
{
	bOnlyRelevantToOwner=true
	bAlwaysRelevant=false
	bOnlyDirtyReplication=true
	NetUpdateFrequency=0.1
}
