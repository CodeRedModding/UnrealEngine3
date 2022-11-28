class UTLocalMessage extends LocalMessage
	abstract;

// information about Announcements

static function byte AnnouncementLevel(byte MessageIndex)
{
	return 1;
}

static function bool IsRewardAnnouncement(byte MessageIndex)
{
	return false;
}

static function Name AnnouncementSound(byte MessageIndex)
{
	return '';
}

static function AddAnnouncement(UTAnnouncer Announcer, byte MessageIndex)
{
	local UTQueuedAnnouncement NewAnnouncement, A;

	NewAnnouncement = Announcer.Spawn(class'UTQueuedAnnouncement');
	NewAnnouncement.AnnouncementClass = Default.Class;
	NewAnnouncement.MessageIndex = MessageIndex;

	// default implementation is just add to end of queue
	if ( Announcer.Queue == None )
	{
		NewAnnouncement.nextAnnouncement = Announcer.Queue;
		Announcer.Queue = NewAnnouncement;
	}
	else
	{
		for ( A=Announcer.Queue; A!=None; A=A.nextAnnouncement )
		{
			if ( A.nextAnnouncement == None )
			{
				A.nextAnnouncement = NewAnnouncement;
				break;
			}
		}
	}
}
