//=============================================================================
// Console - A quick little command line console that accepts most commands.

//=============================================================================
class Console extends Input
	transient
	config(Input)
	native;

// Constants.
const MaxHistory=16;		// # of command histroy to remember.

// Variables

var globalconfig name ConsoleKey;			// Key used to bring up the console

var int HistoryTop, HistoryBot, HistoryCur;
var string TypedStr, History[MaxHistory]; 	// Holds the current command, and the history
var int TypedStrPos;						//Current position in TypedStr
var bool bTyping;							// Turn when someone is typing on the console
var bool bIgnoreKeys;						// Ignore Key presses until a new KeyDown is received

//-----------------------------------------------------------------------------
// Exec functions accessible from the console and key bindings.

// Begin typing a command on the console.
exec function Type()
{
	TypedStr="";
	TypedStrPos=0;
	GotoState( 'Typing' );
}

exec function Talk()
{
	TypedStr="Say ";
	TypedStrPos=4;
	GotoState( 'Typing' );
}

exec function TeamTalk()
{
	TypedStr="TeamSay ";
	TypedStrPos=8;
	GotoState( 'Typing' );
}

//-----------------------------------------------------------------------------
// Message - By default, the console ignores all output.
//-----------------------------------------------------------------------------

event Message( coerce string Msg, float MsgLife);

event PostRender(Canvas Canvas);

//-----------------------------------------------------------------------------
// Check for the console key.

event bool InputKey( name Key, EInputEvent Event, float AmountDepressed )
{
	if( Event != IE_Pressed )
		return false;
	else if( Key==ConsoleKey )
	{
		GotoState('Typing');
		return true;
	}
	else
		return false;

}

//-----------------------------------------------------------------------------
// State used while typing a command on the console.

state Typing
{
	exec function Type()
	{
		TypedStr="";
		TypedStrPos=0;
		gotoState( '' );
	}
	event bool InputChar( string Unicode )
	{
		local int	Character;

		if (bIgnoreKeys)
			return true;

		while(Len(Unicode) > 0)
		{
			Character = Asc(Left(Unicode,1));
			Unicode = Mid(Unicode,1);

			if(Character >= 0x20 && Character < 0x100 && Character != Asc("~") && Character != Asc("`"))
			{
				TypedStr = Left(TypedStr, TypedStrPos) $ Chr(Character) $ Right(TypedStr, Len(TypedStr) - TypedStrPos);
				TypedStrPos++;
			}
		};

		return true;
	}
	event bool InputKey( name Key, EInputEvent Event, float AmountDepressed )
	{
		local string Temp;

		if (Event == IE_Pressed)
		{
			bIgnoreKeys=false;
		}

		if( Key == 'Escape' && Event == IE_Released )
		{
			if( TypedStr!="" )
			{
				TypedStr="";
				TypedStrPos=0;
				HistoryCur = HistoryTop;
				return true;
			}
			else
			{
				GotoState( '' );
			}
		}
		else if( global.InputKey( Key, Event, AmountDepressed ) )
		{
			return true;
		}
		else if( Event != IE_Pressed && Event != IE_Repeat )
		{
			return false;
		}
		else if( Key=='Enter' )
		{
			if( TypedStr!="" )
			{
				// Print to console.
				Message( TypedStr, 6.0 );

				History[HistoryTop] = TypedStr;
				HistoryTop = (HistoryTop+1) % MaxHistory;

				if ( ( HistoryBot == -1) || ( HistoryBot == HistoryTop ) )
					HistoryBot = (HistoryBot+1) % MaxHistory;

				HistoryCur = HistoryTop;

				// Make a local copy of the string.
				Temp=TypedStr;
				TypedStr="";
				TypedStrPos=0;

				Player.ConsoleCommand( Temp, true );
					//Message( Localize("Errors","Exec","Core"), 6.0 );

				Message( "", 6.0 );
				GotoState('');
			}
			else
				GotoState('');

			return true;
		}
		else if( Key=='up' )
		{
			if ( HistoryBot >= 0 )
			{
				if (HistoryCur == HistoryBot)
					HistoryCur = HistoryTop;
				else
				{
					HistoryCur--;
					if (HistoryCur<0)
						HistoryCur = MaxHistory-1;
				}

				TypedStr = History[HistoryCur];
				TypedStrPos = Len(TypedStr);
			}
			return True;
		}
		else if( Key=='down' )
		{
			if ( HistoryBot >= 0 )
			{
				if (HistoryCur == HistoryTop)
					HistoryCur = HistoryBot;
				else
					HistoryCur = (HistoryCur+1) % MaxHistory;

				TypedStr = History[HistoryCur];
			}

		}
		else if( Key=='backspace' )
		{
			if( TypedStrPos>0 )
			{
				TypedStr = Left(TypedStr,TypedStrPos-1) $ Right(TypedStr, Len(TypedStr) - TypedStrPos);
				TypedStrPos--;
			}

			return true;
		}
		else if ( Key=='delete' )
		{
			if ( TypedStrPos < Len(TypedStr) )
				TypedStr = Left(TypedStr,TypedStrPos) $ Right(TypedStr, Len(TypedStr) - TypedStrPos - 1);
			return true;
		}
		else if ( Key=='left' )
		{
			TypedStrPos = Max(0, TypedStrPos - 1);
			return true;
		}
		else if ( Key=='right' )
		{
			TypedStrPos = Min(Len(TypedStr), TypedStrPos + 1);
			return true;
		}
		else if ( Key=='home' )
		{
			TypedStrPos = 0;
			return true;
		}
		else if ( Key=='end' )
		{
			TypedStrPos = Len(TypedStr);
			return true;
		}

		return true;
	}

	event PostRender(Canvas Canvas)
	{
		local float xl,yl;
		local string OutStr;

		// Blank out a space

		Canvas.Font	 = class'Engine'.Default.SmallFont;
		OutStr = "(>"@TypedStr;
		Canvas.Strlen(OutStr,xl,yl);

		Canvas.SetPos(0,Canvas.ClipY-8-yl);
		Canvas.DrawTile( Texture2D 'EngineResources.Black', Canvas.ClipX, yl+6,0,0,32,32);

		Canvas.SetPos(0,Canvas.ClipY-8-yl);
		Canvas.SetDrawColor(0,255,0);
		Canvas.DrawTile( Texture2D 'EngineResources.White', Canvas.ClipX, 2,0,0,32,32);

		Canvas.SetPos(0,Canvas.ClipY-5-yl);
		Canvas.bCenter = False;
		Canvas.DrawText( OutStr, false );

		OutStr = "(>"@Left(TypedStr,TypedStrPos);
		Canvas.StrLen(OutStr,xl,yl);
		Canvas.SetPos(xl,Canvas.ClipY-3-yl);
		Canvas.DrawText("_");
	}

	function BeginState()
	{
		bTyping = true;
		bIgnoreKeys = true;
		HistoryCur = HistoryTop;
	}
	function EndState()
	{
		bTyping = false;
	}
}


defaultproperties
{
	HistoryBot=-1
}
