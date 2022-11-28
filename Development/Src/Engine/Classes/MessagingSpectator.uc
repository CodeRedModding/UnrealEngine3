//=============================================================================
// MessagingSpectator - spectator base class for game helper spectators which receive messages
//=============================================================================

class MessagingSpectator extends PlayerController
	config(Game)
	abstract;

function PostBeginPlay()
{
	Super.PostBeginPlay();
	bIsPlayer = False;
}
