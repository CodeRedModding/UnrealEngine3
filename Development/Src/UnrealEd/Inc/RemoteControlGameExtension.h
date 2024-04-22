/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REMOTECONTROLGAMEEXTENSION_H__
#define __REMOTECONTROLGAMEEXTENSION_H__

// Forward declarations.
class FRemoteControlGame;
class FRemoteControlGameExtension;

/**
 * An abstract extension interface for communicating with the game. This allows other
 * systems to extend RemoteControl's game interface without having to add it to the core interface.
 */
class FRemoteControlGameExtension
{
public:
	explicit FRemoteControlGameExtension(FRemoteControlGame& InGame)
		:	Game( InGame )
	{
	}

	virtual ~FRemoteControlGameExtension()
	{
	}

	FRemoteControlGame& GetGame() const
	{ 
		return Game;
	}

private:
	FRemoteControlGame& Game;
};

/**
 * Factory interface for creating RemoteControl game extensions
 */
class FRemoteControlGameExtensionFactory
{
public:
	virtual ~FRemoteControlGameExtensionFactory() {}

	virtual FRemoteControlGameExtension* CreateExtension(FRemoteControlGame* InGame) const=0;
};

#define REMOTECONTROL_EXTENSION_TYPE(ExtensionInterface) \
	G##ExtensionInterface##_Factory

#define DECLARE_REMOTECONTROL_EXTENSION_FACTORY(ExtensionInterface) \
class ExtensionInterface##_Factory : public FRemoteControlGameExtensionFactory \
{ \
public: \
	virtual FRemoteControlGameExtension* CreateExtension(FRemoteControlGame* InGame) const; \
}; \
extern ExtensionInterface##_Factory  G##ExtensionInterface##_Factory;

#define BEGIN_IMPLEMENT_REMOTECONTROL_EXTENSION_FACTORY(ExtensionInterface) \
	ExtensionInterface##_Factory  G##ExtensionInterface##_Factory; \
	FRemoteControlGameExtension* ExtensionInterface##_Factory :: CreateExtension(FRemoteControlGame* InGame) const \
	{ \

// @todo: actually make this return the right extension implementation based on the passed in RemoteControl game interface
#define PC_REMOTECONTROL_EXTENSION(Implementation) return new Implementation(*InGame);
#define XBOX_REMOTECONTROL_EXTENSION(Implementation)
#define PS3_REMOTECONTROL_EXTENSION(Implementation)

#define END_IMPLEMENT_REMOTECONTROL_EXTENSION_FACTORY(ExtensionInterface) \
	}

#endif // __REMOTECONTROLGAMEEXTENSION_H__
