/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
interface CloudStorageUpgradeHelper
	native(PlatformInterface)
	dependson(PlatformInterfaceBase);

/**
 * Handle each local document found by UpgradeLocalStorageToCloud(). By default it will move
 * each doc to the cloud with its current name.
 */
event HandleLocalDocument(out string DocName, out int bShouldMoveToCloud, out int bShouldDeleteLocalFile);

/**
 * Handle each local key/value found by UpgradeLocalStorageToCloud(). By default it will move
 * each value to the cloud with its current name.
 */
event HandleLocalKeyValue(out string CloudKeyName, out PlatformInterfaceData CloudValue, out int bShouldMoveToCloud, out int bShouldDeleteLocalKey);

/**
 * Fill an array of all the Keys you want to transfer to the cloud when upgrading from local storage.
 * This will already have been filled with any values in IPhoneDrv.CloudUpgradeKeys in Engine.ini.
 * Each entry in the array should have the format "(KeyName=XXX,KeyType=PIDT_XXX)"
 */
event GetCloudUpgradeKeys(out array<string> CloudKeys);