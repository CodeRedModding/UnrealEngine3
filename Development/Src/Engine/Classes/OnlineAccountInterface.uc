/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface provides account creation and enumeration functions
 */
interface OnlineAccountInterface dependson(OnlineSubsystem);

/**
 * Creates a network enabled account on the online service
 *
 * @param UserName the unique nickname of the account
 * @param Password the password securing the account
 * @param EmailAddress the address used to send password hints to
 * @param ProductKey the unique id for this installed product
 *
 * @return true if the account was created, false otherwise
 */
function bool CreateOnlineAccount(string UserName,string Password,string EmailAddress,optional string ProductKey);

/**
 * Delegate used in notifying the UI/game that the account creation completed
 *
 * @param ErrorStatus whether the account created successfully or not
 */
delegate OnCreateOnlineAccountCompleted(EOnlineAccountCreateStatus ErrorStatus);

/**
 * Sets the delegate used to notify the gameplay code that account creation completed
 *
 * @param AccountCreateDelegate the delegate to use for notifications
 */
function AddCreateOnlineAccountCompletedDelegate(delegate<OnCreateOnlineAccountCompleted> AccountCreateDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param AccountCreateDelegate the delegate to use for notifications
 */
function ClearCreateOnlineAccountCompletedDelegate(delegate<OnCreateOnlineAccountCompleted> AccountCreateDelegate);

/**
 * Creates a non-networked account on the local system. Password is only used
 * when supplied. Otherwise the account is not secured.
 *
 * @param UserName the unique nickname of the account
 * @param Password the password securing the account
 *
 * @return true if the account was created, false otherwise
 */
function bool CreateLocalAccount(string UserName,optional string Password);

/**
 * Changes the name of a local account
 * 
 * @param UserName the unique nickname of the account
 * @param Password the password securing the account
 *
 * @return true if the account was renamed, false otherwise
 */
function bool RenameLocalAccount(string NewUserName,string OldUserName,optional string Password);

/**
 * Deletes a local account if the password matches
 * 
 * @param UserName the unique nickname of the account
 * @param Password the password securing the account
 *
 * @return true if the account was deleted, false otherwise
 */
function bool DeleteLocalAccount(string UserName,optional string Password);

/**
 * Fetches a list of local accounts
 * 
 * @param Accounts the array that is populated with the accounts
 *
 * @return true if the list was read, false otherwise
 */
function bool GetLocalAccountNames(out array<string> Accounts);