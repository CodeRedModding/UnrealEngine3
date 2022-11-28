class UTLinkedReplicationInfo extends ReplicationInfo
	abstract;

var UTLinkedReplicationInfo NextReplicationInfo;

replication
{
	// Variables the server should send to the client.
	reliable if ( bNetInitial && (Role==ROLE_Authority) )
		NextReplicationInfo;
}

defaultproperties
{
	NetUpdateFrequency=1
}