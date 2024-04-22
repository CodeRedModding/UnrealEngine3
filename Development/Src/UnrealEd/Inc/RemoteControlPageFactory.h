/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REMOTECONTROLPAGEFACTORY_H__
#define __REMOTECONTROLPAGEFACTORY_H__

// Forward declarations.
class WxRemoteControlPage;
class FRemoteControlGame;
class wxNotebook;

/**
 * Factory interface for creating RemoteControl pages. Clients register these interfaces
 * to register additional RemoteControl page types.
 */
class FRemoteControlPageFactory
{
public:
	/**
	 * The constructor registers this factory object with the RemoteControl frame.
	 */
	FRemoteControlPageFactory();
	virtual ~FRemoteControlPageFactory() {}

	/**
	 * Called by the RemoteControl frame's constructor to instance pages.
	 */
	virtual WxRemoteControlPage *CreatePage(FRemoteControlGame *pGame, wxNotebook *pNotebook) const=0;
};

/**
 * Template implementation of FRemoteControlPageFactory that probably will handle most cases.
 */
template <class T>
class TRemoteControlPageFactory : public FRemoteControlPageFactory
{
public:
	/**
	 * Called by the RemoteControl frame's constructor to instance pages.
	 */
	virtual WxRemoteControlPage *CreatePage(FRemoteControlGame *pGame, wxNotebook *pNotebook) const
	{
		return new T(pGame, pNotebook);
	}
};

#endif // __REMOTECONTROLPAGEFACTORY_H__
