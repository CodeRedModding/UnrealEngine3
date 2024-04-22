/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;

namespace UnrealLoc
{
    public partial class PickGame : Form
    {
        private UnrealLoc Main = null;

		public void CreateButton( string Name, int Index )
		{
			Button GameButton = new Button();
			GameButton.AutoSize = true;
			GameButton.Location = new System.Drawing.Point( 217, ( Index * 30 ) + 42 );
			GameButton.Name = "Button_" + Name;
			GameButton.Size = new System.Drawing.Size( 150, 23 );
			GameButton.TabIndex = Index;
			GameButton.Text = Name;
			GameButton.UseVisualStyleBackColor = true;
			GameButton.Click += new System.EventHandler( this.GameButtonClick );

			Controls.Add( GameButton );
		}

        public PickGame( UnrealLoc InMain )
        {
            Main = InMain;
            InitializeComponent();

			List<string> ValidGames = UnrealControls.GameLocator.LocateGames();

			int Index = 0;
			CreateButton( "Engine", Index++ );
			foreach( string ValidGame in ValidGames )
			{
				CreateButton( ValidGame, Index++ );

				string DLCFolder = ValidGame + "Game\\DLC";
				if( Directory.Exists( DLCFolder ) )
				{
					CreateButton( ValidGame + "DLC", Index++ );
				}
			}

			Index++;
			CreateButton( "Options", Index++ );

			ClientSize = new Size( ClientSize.Width, ( Index * 30 ) + 42 );
        }

        private void GameButtonClick( object sender, EventArgs e )
        {
			Button GameButton = ( Button )sender;

			if( GameButton.Text == "Options" )
			{
				OptionsDialog DisplayOptions = new OptionsDialog( Main, Main.Options );
				DisplayOptions.ShowDialog();
			}
			else
			{
				Main.GameName = GameButton.Text;

				if( Main.GameName.Contains( "DLC" ) )
				{
					Main.GameName = Main.GameName.Substring( 0, Main.GameName.Length - 3 );
					Main.bDLCProfile = true;
				}

				Close();
			}
        }
    }
}