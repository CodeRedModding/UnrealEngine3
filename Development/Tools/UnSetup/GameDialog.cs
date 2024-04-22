/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace UnSetup
{
	public partial class GameDialog : Form
	{
		private Utils.GameOptions Game = null;

		public GameDialog()
		{
			InitializeComponent();

			Game = Program.Util.Game;
			Game.GameUniqueID = Guid.NewGuid();
			Game.MachineUniqueID = Program.Util.GetMachineID();

			GamePropertyGrid.SelectedObject = Game;

			DialogResult = DialogResult.Cancel;
		}

		private bool GameNamesValid()
		{
			if( Game.GameName == Utils.GameDefaultName || Game.GameLongName == Utils.GameDefaultLongName )
			{
				return ( false );
			}

			if( Game.GameName.Length == 0 || Game.GameLongName.Length == 0 )
			{
				return ( false );
			}

			if( Game.GameName.IndexOfAny( " /\\:*?\"<>|'".ToCharArray() ) >= 0 )
			{
				return ( false );
			}

			if( Game.GameLongName.IndexOfAny( "/\\:*?\"<>|'".ToCharArray() ) >= 0 )
			{
				return ( false );
			}

			return ( true );
		}

		private void GameDialogOKButton( object sender, EventArgs e )
		{
			if( Game.GameName.Length > 12 )
			{
				Game.GameName = Game.GameName.Substring( 0, 12 );
				Console.WriteLine( "GameName truncated to: " + Game.GameName );
			}

			if( Game.GameLongName.Length > 30 )
			{
				Game.GameLongName = Game.GameLongName.Substring( 0, 30 );
				Console.WriteLine( "GameLongName truncated to: " + Game.GameLongName );
			}

			if( !GameNamesValid() )
			{
				GenericQuery Query = new GenericQuery( "GQCaptionPleaseSetGameName", "GQDescPleaseSetGameName", false, "", true, "GQOK" );
				Query.ShowDialog();
			}
			else
			{
				Program.Util.Game = Game;
				DialogResult = DialogResult.OK;
				Close();
			}
		}

		private void GameSettingsCancelClick( object sender, EventArgs e )
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void OnLoad( object sender, EventArgs e )
		{
			Utils.CenterFormToPrimaryMonitor( this );
		}
	}
}
