/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * An interface for a Key Value Store system within the context of the CloudSaveSystem
 */
interface CloudSaveSystemKVSInterface;

/**Reads a value from the KVS. KeyNames can overlap across save slots*/
function bool ReadKeyValue(int SaveSlotIndex, string KeyName, EPlatformInterfaceDataType Type, out PlatformInterfaceDelegateResult Value);

/**Writes a value to the KVS. KeyNames can overlap across save slots*/
function bool WriteKeyValue(int SaveSlotIndex, string KeyName, const out PlatformInterfaceData Value);