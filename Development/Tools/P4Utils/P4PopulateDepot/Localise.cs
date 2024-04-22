// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;

namespace P4PopulateDepot
{
	public class Localise
	{
		private Dictionary<string, string> Dictionary = new Dictionary<string, string>();
		public string UE3LanguageCode = "INT";

		public Localise()
		{
		}

		public string GetUE3Language()
		{
			return ( UE3LanguageCode );
		}

		private List<string> KnownLanguages = new List<string>() { "JPN", "RUS", "HUN", "JPN", "KOR", "HIN", "POL", "CZE", "SLO" };

		private string ConvertWindowsToUE3( string Language, string ParentLanguage )
		{
			if( Language == "ENU" || ParentLanguage == "ENU" )
			{
				UE3LanguageCode = "INT";
			}
			else if( Language == "ESN" )
			{
				UE3LanguageCode = "ESN";
			}
			else if( Language == "ESP" || ParentLanguage == "ESP" )
			{
				UE3LanguageCode = "ESM";
			}
			else if( Language == "DEU" || ParentLanguage == "DEU" )
			{
				UE3LanguageCode = "DEU";
			}
			else if( Language == "FRA" || ParentLanguage == "FRA" )
			{
				UE3LanguageCode = "FRA";
			}
			else if( Language == "ITA" || ParentLanguage == "ITA" )
			{
				UE3LanguageCode = "ITA";
			}
			else if( Language == "CHS" || Language == "CHT" || ParentLanguage == "CHS" || ParentLanguage == "CHT" )
			{
				UE3LanguageCode = "CHN";
			}
			else if( Language == "PLK" )
			{
				UE3LanguageCode = "POL";
			}
			else if( Language == "CSY" )
			{
				UE3LanguageCode = "CZE";
			}
			else if( Language == "SKY" )
			{
				UE3LanguageCode = "SLO";
			}
			else if( KnownLanguages.Contains( Language ) )
			{
				UE3LanguageCode = Language;
			}
			else
			{
				// Default if we drop through in other cases.
				UE3LanguageCode = "INT";
			}

			return ( UE3LanguageCode );
		}

		public void Init( string Language, string ParentLanguage )
		{
			UE3LanguageCode = ConvertWindowsToUE3( Language, ParentLanguage );

			Dictionary.Clear();

			switch( UE3LanguageCode )
			{
			case "FRA":
				Dictionary.Add( "R2D", "Retour au bureau" );

				Dictionary.Add( "GQOK", "OK" );
				Dictionary.Add( "GQCancel", "Annuler" );
				Dictionary.Add( "GQRetry", "Réessayer" );
				Dictionary.Add( "GQIgnore", "Ignorer" );
				Dictionary.Add( "GQYes", "Oui" );
				Dictionary.Add( "GQNo", "Non" );
				Dictionary.Add( "GQSkip", "Passer" );

				break;

			case "ITA":
				Dictionary.Add( "R2D", "Ritorna al desktop" );

				Dictionary.Add( "GQOK", "OK" );
				Dictionary.Add( "GQCancel", "Annulla" );
				Dictionary.Add( "GQRetry", "Riprova" );
				Dictionary.Add( "GQIgnore", "Ignora" );
				Dictionary.Add( "GQYes", "Sì" );
				Dictionary.Add( "GQNo", "No" );
				Dictionary.Add( "GQSkip", "Tralascia" );

				break;

			case "DEU":
				Dictionary.Add( "R2D", "Zurück zum desktop" );

				Dictionary.Add( "GQOK", "OK" );
				Dictionary.Add( "GQCancel", "Abbrechen" );
				Dictionary.Add( "GQRetry", "Erneut versuchen" );
				Dictionary.Add( "GQIgnore", "Ignorieren" );
				Dictionary.Add( "GQYes", "Ja" );
				Dictionary.Add( "GQNo", "Nein" );
				Dictionary.Add( "GQSkip", "Überspringen" );

				break;

			case "ESN":
				Dictionary.Add( "R2D", "Volver al escritorio" );

				Dictionary.Add( "GQOK", "Aceptar" );
				Dictionary.Add( "GQCancel", "Cancelar" );
				Dictionary.Add( "GQRetry", "Reintentar" );
				Dictionary.Add( "GQIgnore", "Ignorar" );
				Dictionary.Add( "GQYes", "Sí" );
				Dictionary.Add( "GQNo", "No" );
				Dictionary.Add( "GQSkip", "Omitir" );

				break;

			case "ESM":
				Dictionary.Add( "R2D", "Volver al escritorio" );

				Dictionary.Add( "GQOK", "Aceptar" );
				Dictionary.Add( "GQCancel", "Cancelar" );
				Dictionary.Add( "GQRetry", "Reintentar" );
				Dictionary.Add( "GQIgnore", "Ignorar" );
				Dictionary.Add( "GQYes", "Sí" );
				Dictionary.Add( "GQNo", "No" );
				Dictionary.Add( "GQSkip", "Saltear" );

				break;

			case "RUS":
				Dictionary.Add( "R2D", "На рабочий стол" );

				Dictionary.Add( "GQOK", "OK" );
				Dictionary.Add( "GQCancel", "Отменить" );
				Dictionary.Add( "GQRetry", "Повторить" );
				Dictionary.Add( "GQIgnore", "Игнорировать" );
				Dictionary.Add( "GQYes", "Да" );
				Dictionary.Add( "GQNo", "Нет" );
				Dictionary.Add( "GQSkip", "Пропустить" );

				break;

			case "POL":
				Dictionary.Add( "R2D", "Wróć na pulpit" );

				Dictionary.Add( "GQOK", "OK" );
				Dictionary.Add( "GQCancel", "Anuluj" );
				Dictionary.Add( "GQRetry", "Ponów" );
				Dictionary.Add( "GQIgnore", "Ignoruj" );
				Dictionary.Add( "GQYes", "Tak" );
				Dictionary.Add( "GQNo", "Nie" );
				Dictionary.Add( "GQSkip", "Pomiń" );

				break;

			case "CZE":
				Dictionary.Add( "R2D", "Zpět na pracovní plochu" );

				Dictionary.Add( "GQOK", "OK" );
				Dictionary.Add( "GQCancel", "Zrušit" );
				Dictionary.Add( "GQRetry", "Opakovat" );
				Dictionary.Add( "GQIgnore", "Ignorovat" );
				Dictionary.Add( "GQYes", "Ano" );
				Dictionary.Add( "GQNo", "Ne" );
				Dictionary.Add( "GQSkip", "Přeskočit" );

				break;

			case "HUN":
				Dictionary.Add( "R2D", "Vissza az asztalra" );

				Dictionary.Add( "GQOK", "OK" );
				Dictionary.Add( "GQCancel", "Mégse" );
				Dictionary.Add( "GQRetry", "Újra" );
				Dictionary.Add( "GQIgnore", "Kihagyás" );
				Dictionary.Add( "GQYes", "Igen" );
				Dictionary.Add( "GQNo", "Nem" );
				Dictionary.Add( "GQSkip", "Átugrás" );

				break;

			case "SLO":
				Dictionary.Add( "R2D", "Návrat na plochu" );

				Dictionary.Add( "GQOK", "OK" );
				Dictionary.Add( "GQCancel", "Zrušiť" );
				Dictionary.Add( "GQRetry", "Znova" );
				Dictionary.Add( "GQIgnore", "Ignorovať" );
				Dictionary.Add( "GQYes", "Áno" );
				Dictionary.Add( "GQNo", "Nie" );
				Dictionary.Add( "GQSkip", "Vynechať" );

				break;

			case "JPN":
				Dictionary.Add( "R2D", "デスクトップに戻る" );

				Dictionary.Add( "GQOK", "OK" );
				Dictionary.Add( "GQCancel", "キャンセル" );
				Dictionary.Add( "GQRetry", "再試行" );
				Dictionary.Add( "GQIgnore", "無視" );
				Dictionary.Add( "GQYes", "はい" );
				Dictionary.Add( "GQNo", "いいえ" );
				Dictionary.Add( "GQSkip", "スキップ" );

				break;

			case "KOR":
				Dictionary.Add( "R2D", "바탕화면으로 돌아가기" );

				Dictionary.Add( "GQOK", "확인" );
				Dictionary.Add( "GQCancel", "취소" );
				Dictionary.Add( "GQRetry", "다시 시도" );
				Dictionary.Add( "GQIgnore", "무시" );
				Dictionary.Add( "GQYes", "예" );
				Dictionary.Add( "GQNo", "아니오" );
				Dictionary.Add( "GQSkip", "건너뛰기" );

				break;

			case "CHN":
				Dictionary.Add( "R2D", "回到桌面" );

				Dictionary.Add( "GQOK", "确定" );
				Dictionary.Add( "GQCancel", "取消" );
				Dictionary.Add( "GQRetry", "重试" );
				Dictionary.Add( "GQIgnore", "忽略" );
				Dictionary.Add( "GQYes", "是" );
				Dictionary.Add( "GQNo", "否" );
				Dictionary.Add( "GQSkip", "跳过" );

				break;

			case "INT":
			default:
				Dictionary.Add( "R2D", "Return to desktop" );

				Dictionary.Add( "GQOK", "OK" );
				Dictionary.Add( "GQCancel", "Cancel" );
				Dictionary.Add( "GQRetry", "Retry" );
				Dictionary.Add( "GQIgnore", "Ignore" );
				Dictionary.Add( "GQYes", "Yes" );
				Dictionary.Add( "GQNo", "No" );
				Dictionary.Add( "GQSkip", "Skip" );

				Dictionary.Add("GQNext", "Next");
				Dictionary.Add("GQBack", "Back");
				Dictionary.Add("GQFinished", "Finished");

				Dictionary.Add("TitleWelcome", "Perforce Wizard");
				Dictionary.Add("TitleConnection", "Perforce Login");
				Dictionary.Add("TitlePreview", "Checkin Preview");
				Dictionary.Add("TitleComplete", "Checkin Complete");

				Dictionary.Add("CONDescription", "Enter your Perforce connection info below.");
				Dictionary.Add("CONServer", "Server");
				Dictionary.Add("CONUser", "User");
				Dictionary.Add("CONWorkspace", "Workspace");
				Dictionary.Add("CONWorkspaceRoot", "Workspace Root");
				Dictionary.Add("CONBrowse", "Browse...");
				Dictionary.Add("CONNew", "New...");
                Dictionary.Add("CONServerInfo", "The Server field specifies the location of the Perforce server.  If you did not setup your server, this value can likely be given to you by your Perforce administrator.  The location can be specified in multiple ways depending on your configuration.  Below are some supported formats.\n\n\n<hostname>:<port number>\n<IP address>:<port number>\n<zeroconf name> (Server must have Zeroconf enabled)\n<hostname> (Server must be on port 1666)\n<port number> (Server must be local)\n<TCP/SSL prefix>:<hostname>:<port number>");
                Dictionary.Add("CONServerInfoHeading", "Info");
                Dictionary.Add("CONPassInfo", "(Optional)");

				Dictionary.Add("BWBrowseWorkspaces", "Browse Workspaces");
				Dictionary.Add("BWWorkspaceName", "Workspace Name");
				Dictionary.Add("BWWorkspaceRoot", "Workspace Root");
				Dictionary.Add("BWDescription", "Select an available workspace below.");
                Dictionary.Add("BWShowAll", "Show All");
                Dictionary.Add("BWNoneAvailable", "No Items Available");

                Dictionary.Add("BUBrowseUsers", "Browse Users");
				Dictionary.Add("BUUserName", "User");
                Dictionary.Add("BUEmail", "Email");
                Dictionary.Add("BULastAccessed", "Last Accessed");
                Dictionary.Add("BUFullName", "Full Name");
				Dictionary.Add("BUDescription", "Select an available user name from the list below.");
                Dictionary.Add("BUNoneAvailable", "Could Not Retrieve List");

				Dictionary.Add("NWNewWorkspace", "New Workspace");
                Dictionary.Add("NWCreateFail", "Failed to create workspace.");

                Dictionary.Add("NUNewUser", "New User");
                Dictionary.Add("NUUserEmpty", "User name can not be empty.");
                Dictionary.Add("NUUserSpace", "User name can not contain spaces.");
                Dictionary.Add("NUUserExists", "User name already exists on the server.");
                Dictionary.Add("NUPassMissmatch", "Password fields do not match.  Please enter password again.");
                Dictionary.Add("NUCreateFail", "Failed to create new user.  Consult with your Perforce administrator if this issue persists.");

				Dictionary.Add("PREVDescription", "Ready to add files.  To begin adding files to the Perforce Server, click \"Next\".");
				Dictionary.Add("PREVNumFiles", "File Count");
				Dictionary.Add("PREVNumFolders", "Folder Count");
				Dictionary.Add("PREVFileSize", "Total Size");
				Dictionary.Add("PREVSrcLabel", "Local Source");
				Dictionary.Add("PREVTargetLabel", "Perforce Target");


				Dictionary.Add("PROGRESSHeading", "Submitting Files");
                Dictionary.Add("PROGRESSSubmitTitle", "Copying files to source control server");
				Dictionary.Add("PROGRESSChecking", "Checking Perforce status.");
				Dictionary.Add("PROGRESSPrepare", "Preparing For Submit");
				Dictionary.Add("PROGRESSCLCreate", "Creating Perforce changelist.");
				Dictionary.Add("PROGRESSCLAddFiles", "Adding files to Perforce changelist.");
				Dictionary.Add("PROGRESSTimeDesc", "Depending on your network connection, this operation may take a while to complete.");
				Dictionary.Add("PROGRESSCLSeach", "Searching for existing changelists.");
				Dictionary.Add("PROGRESSCLRevertFiles", "Reverting files in existing changelist.");
				Dictionary.Add("PROGRESSCLDelete", "Deleting existing changelist.");

				Dictionary.Add("ERRError", "Error");
				Dictionary.Add("ERRFileInCL", "File already exists in a changelist that was not created by this utility: ");
				Dictionary.Add("ERRFileExistsInDepot", "File already exists in the Perforce depot: ");
				Dictionary.Add("ERRGenericSubmit", "Encountered an error submitting files to Perforce.");
				Dictionary.Add("ERRLostConn", "Lost connection with the Perforce server.");
				Dictionary.Add("ERRMissingFileMSG", "Can not continue due to missing file: ");
				Dictionary.Add("ERRMissingFile", "Missing File");
				Dictionary.Add("ERRWorkspaceEmpty", "Workspace name can not be empty.");
				Dictionary.Add("ERRWorkspaceExists", "A workspace with that name already exists on the server.");
				Dictionary.Add("ERRWorkspaceBadRoot", "Must select a workspace root that contains the current project directory.");
				Dictionary.Add("ERRWorkspace", "Current project must exist under workspace root, please choose another workspace.");
                Dictionary.Add("ERRServer", "Can not connect to server.  Check server address and port. (Example: 127.0.0.1:1666)");
                Dictionary.Add("ERRPass", "Invalid password.  Please re-enter your password.");
                Dictionary.Add("ERRUser", "Invalid user name.  Please re-enter your user name.");
                Dictionary.Add("ERRConnectWorkspace", "The workspace name is not recognized by this connection.");

				Dictionary.Add("IFLaunch", "Launch ");

				Dictionary.Add("COMPLETELaunchUDK", "Launch Unreal Development Kit");
				Dictionary.Add("WPDescription", "Welcome to the Unreal Development Kit Perforce Wizard\n\nUnreal Development Kit has built-in Perforce integration which helps you colaborate with team members, manage multiple versions of your project, and helps you back-up your work.  This wizard will enable perforce integration in your UDK project and help guide you through the initial submission of UDK into your Perforce version control server.");

				break;
			}
		}

		public void Destroy()
		{
			Dictionary.Clear();
		}

		// Get a phrase from the dictionary
		public string GetPhrase( string Key )
		{
			string Value = null;
			Dictionary.TryGetValue( Key, out Value );
			if( Value == null )
			{
				Value = Key;
			}

			return ( Value );
		}
	}
}

