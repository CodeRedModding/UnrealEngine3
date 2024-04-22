/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;

namespace UnSetup
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
				// If all else fails, use American
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

				Dictionary.Add( "ErrorMessage", "Message d'erreur : " );
				Dictionary.Add( "UDKTrouble", "Pour des solutions, visitez : " );
				Dictionary.Add( "RegUDK", "Enregistrement de l'UDK" );
				Dictionary.Add( "RegGame", "Enregistrement du jeu" );
				Dictionary.Add( "CleaningUp", "Nettoyage" );

				Dictionary.Add( "GQCaptionNoEULA", "CLUF introuvable" );
				Dictionary.Add( "GQDescNoEULA", "CLUF introuvable. Abandon de l'installation." );
				Dictionary.Add( "GQCaptionInstallFail", "Échec de l'installation." );
				Dictionary.Add( "GQCaptionUninstallAllSure", "Supprimer tous les fichiers ?" );
				Dictionary.Add( "GQDescUninstallAllSure", "Tous les fichiers seront supprimés, y compris les cartes et données que vous avez créées. Continuer ?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "Désinstallation terminée !" );
				Dictionary.Add( "GQDescUninstallComplete", "Tous les fichiers ont été supprimés avec succès." );
				Dictionary.Add( "GQCaptionSubscribe", "Inscription à la liste de diffusion" );
				Dictionary.Add( "GQDescSubscribe", "Saisissez votre adresse électronique si vous souhaitez recevoir des mises à jour techniques et commerciales pour l'Unreal Development Kit." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "Installation redistribuable terminée !" );
				Dictionary.Add( "GQDescRedistInstallComplete", "Tous les éléments redistribuables prérequis ont été installés correctement." );
				Dictionary.Add( "GQCaptionWaiting", "En attente de traitement..." );
				Dictionary.Add( "GQDescWaiting", "En attente de l'installation d'un élément prérequis..." );
				Dictionary.Add( "GQCaptionDelFileFail", "Échec de la suppression du fichier" );
				Dictionary.Add( "GQCaptionDelFolderFail", "Échec de la suppression du dossier" );
				Dictionary.Add( "GQCaptionCorrupt", "Package corrompu" );
				Dictionary.Add( "GQDescCorrupt1", "Le package UDK est corrompu. Veuillez le télécharger à nouveau (signature introuvable)." );
				Dictionary.Add( "GQDescCorrupt2", "Le package UDK est corrompu. Veuillez le télécharger à nouveau (exception à l'ouverture du fichier .zip)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Définir le nom du jeu" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Définissez les champs 'GameName' et 'GameLongName' avec des chaînes valides." );

				Dictionary.Add( "ButAccept", "Accepter" );
				Dictionary.Add( "ButReject", "Refuser" );
				Dictionary.Add( "ButInstall", "Installer" );
				Dictionary.Add( "ButUninstall", "Désinstaller" );

				Dictionary.Add( "EULATitle", "Contrat de Licence Utilisateur Final" );
				Dictionary.Add( "EULALegalese", "J'ai bien lu et compris le contrat de licence du logiciel UDK et j'accepte ses termes et conditions en cliquant sur le bouton « Accepter ». Mon acceptation fait office de signature au bas du présent contrat." );

				Dictionary.Add( "GBInstallLocation", "Emplacement pour l'installation" );
				Dictionary.Add( "GBInstallLocationBrowse", "Parcourir..." );
				Dictionary.Add( "PrivacyStatement", "[Facultatif] S'inscrire pour recevoir les mises à jour et nouveautés relatives à l'UDK." );
				Dictionary.Add( "PrivacyStatement2", "Lisez notre déclaration de confidentialité : http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Options d'installation : " );

				Dictionary.Add( "UpdateFileManifests", "Mettre à jour les listes de fichiers" );

				Dictionary.Add( "IFInstallFinished", "Installation terminée" );
				Dictionary.Add( "IFInstallContentFinished", "Installation terminée pour : " );
				Dictionary.Add( "IFLaunch", "Exécuter " );
				Dictionary.Add( "IFFinished", "Terminé" );

				Dictionary.Add( "UIOLocation", "Emplacement : " );
				Dictionary.Add( "UIOOptionsGame", "Options de désinstallation : " );
				Dictionary.Add( "DeletingFiles", "Suppression des fichiers" );
				Dictionary.Add( "DeletingShortcuts", "Suppression des raccourcis" );
				Dictionary.Add( "UnableDeleteFile", "Le fichier ci-dessus n'a pas pu être supprimé. Il est probablement utilisé par une autre application." );
				Dictionary.Add( "UnableDeleteFolder", "Le dossier ci-dessus n'a pas pu être supprimé. Il est probablement ouvert dans une autre application." );
				Dictionary.Add( "DeleteRetry", "Retenter la suppression ?" );

				Dictionary.Add( "PBDecompressing", "Décompression des fichiers" );
				Dictionary.Add( "PBCompressing", "Compression des fichiers" );
				Dictionary.Add( "PBPackaging", "Empaquetage des fichiers" );
				Dictionary.Add( "PBDecompPrereq", "Décompression des fichiers prérequis" );
				Dictionary.Add( "PBPackagingPrereqs", "Empaquetage des fichiers prérequis" );
				Dictionary.Add( "PBSettingInitial", "Préparation du premier tour..." );

				Dictionary.Add( "RedistPleaseWait", "Installation des fichiers prérequis" );

				Dictionary.Add( "RedistVCRedistHeader", "Éléments redistribuables de Visual Studio 2010" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "Pilotes pour microprocesseurs AMD (XP uniquement)" );

				Dictionary.Add( "RedistVCRedistContent", "Accédez à de la documentation et à des ressources UDK gratuites à l'adresse http://www.unrealengine.com/udk/documentation/." );
				Dictionary.Add( "RedistDXRedistContent", "L'UDK vous offre toute la puissance de l'Unreal Engine 3 avec un accès aux mêmes outils dernier cri pour vous permettre de créer des jeux vidéo sensationnels." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "Échec de l'installation de VCRedist x86." );
				Dictionary.Add( "RedistVCRedistx64Fail", "Échec de l'installation de VCRedist x64." );
				Dictionary.Add( "RedistDXRedistFail", "Échec de l'installation de DirectX." );
				Dictionary.Add( "RedistAMDCPUFail", "Échec de l'installation des pilotes pour microprocesseurs AMD." );

				Dictionary.Add( "IFDependsFailed", "Les dépendances n'ont pas été installées. Exécutez « UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe »." );
				Dictionary.Add( "GQCaptionInvalidInstall", "Emplacement d'installation non valide" );
				Dictionary.Add( "GQDescInvalidInstall", "Assurez-vous que l'emplacement est bien valide et qu'il y a assez d'espace disponible sur le disque." );

				Dictionary.Add( "UE3RedistEULALegalese", "J'ai lu et compris le contrat de licence et j'accepte ses termes et conditions en cliquant sur le bouton « Accepter »." );
				Dictionary.Add( "NetCodeWarn", "L'Unreal Development Kit (UDK) est un ensemble d'outils pour le développement de jeux. Les applications créées avec l'UDK contiennent du code qui n'a pas été vérifié par Epic Games, Inc. Comme toute application téléchargée sur Internet, vous ne devez installer ce logiciel que si vous avez confiance en sa source. Notre responsabilité ne saurait être engagée." );

				Dictionary.Add( "ExtractFailCaption", "Échec de l'installation" );
				Dictionary.Add( "ExtractFailMessage1", "Échec de l'extraction de la totalité des fichiers avec l'exception : " );
				Dictionary.Add( "ExtractFailMessage2", "En cas de problème, rendez-vous sur le site http://udk.com/troubleshooting." );
				Dictionary.Add( "UIODeleteOnly", "Désinstaller les fichiers originaux et temporaires (et non ceux créés par l'utilisateur)." );
				Dictionary.Add( "UIODeleteAll", "Désinstaller tous les fichiers (y compris ceux créés par l'utilisateur)." );

				Dictionary.Add( "RedistVCRedistContentTech", "Microsoft Visual C++ 2010 Redistributable Package (x86) installe des composants d'exécution des bibliothèques Visual C++ nécessaires à l'exécution d'applications développées avec Visual C++ SP1 sur un ordinateur ne contenant pas Visual C++ 2010. Ce package installe des composants d'exécution des bibliothèques C Runtime (CRT), Standard C++, ATL, MFC, OpenMP et MSDIA." );
				Dictionary.Add( "RedistDXRedistContentTech", "La suite DirectX est un ensemble de technologies destinées à faire des ordinateurs Windows une plateforme idéale pour l'exécution et l'affichage d'applications riches en éléments multimédia tels que des graphismes couleur, des vidéos, des animations 3D et un son exceptionnel." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Permet au système de régler automatiquement la vitesse, la tension et la puissance du microprocesseur en fonction des besoins en performances actuelles de l'utilisateur. Ce pilote fonctionne avec les processeurs AMD Athlon 64 X2 Dual Core fonctionnant avec Windows XP SP2 ou Windows 2003 SP1 x86 et x64." );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "Adresse électronique" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "Messaggio di errore: " );
				Dictionary.Add( "UDKTrouble", "Per possibili soluzioni andare su: " );
				Dictionary.Add( "RegUDK", "Registrarsi a UDK" );
				Dictionary.Add( "RegGame", "Registrare partita" );
				Dictionary.Add( "CleaningUp", "Ripulire" );

				Dictionary.Add( "GQCaptionNoEULA", "Impossibile trovare EULA" );
				Dictionary.Add( "GQDescNoEULA", "Impossibile trovare EULA; interruzione dell'installazione in corso." );
				Dictionary.Add( "GQCaptionInstallFail", "Installazione non riuscita" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "Eliminare tutti i file?" );
				Dictionary.Add( "GQDescUninstallAllSure", "Verranno eliminati tutti i file, compresi eventuali tesori o mappe creati. Continuare?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "Disinstallazione completata!" );
				Dictionary.Add( "GQDescUninstallComplete", "Tutti i file sono stati eliminati con successo." );
				Dictionary.Add( "GQCaptionSubscribe", "Iscriversi alla mailing list" );
				Dictionary.Add( "GQDescSubscribe", "Registrare il proprio indirizzo e-mail per ricevere aggiornamenti commerciali e tecnici per Unreal Development Kit." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "Installazione ridistribuibile completata!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "Tutti i prerequisiti ridistribuibili sono stati installati correttamente." );
				Dictionary.Add( "GQCaptionWaiting", "In attesa dell'installazione..." );
				Dictionary.Add( "GQDescWaiting", "In attesa che venga completata l'installazione di un prerequisito..." );
				Dictionary.Add( "GQCaptionDelFileFail", "Impossibile eliminare il file" );
				Dictionary.Add( "GQCaptionDelFolderFail", "Impossibile eliminare la cartella" );
				Dictionary.Add( "GQCaptionCorrupt", "Pacchetto danneggiato" );
				Dictionary.Add( "GQDescCorrupt1", "Il pacchetto UDK è danneggiato, scaricare di nuovo (impossibile trovare firma)." );
				Dictionary.Add( "GQDescCorrupt2", "Il pacchetto UDK è danneggiato, scaricare di nuovo (eccezione durante apertura file zip)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Imposta il nome del gioco" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Imposta i campi 'GameName' e 'GameLongName' con stringhe valide." );

				Dictionary.Add( "ButAccept", "Accetto" );
				Dictionary.Add( "ButReject", "Non accetto" );
				Dictionary.Add( "ButInstall", "Installare" );
				Dictionary.Add( "ButUninstall", "Disinstallare" );

				Dictionary.Add( "EULATitle", "Contratto di licenza utente finale" );
				Dictionary.Add( "EULALegalese", "Ho letto e compreso la licenza del software UDK, e ne accetto le condizioni facendo clic sul tasto 'Accetto'. La mia accettazione funge da firma in fondo al presente contratto." );

				Dictionary.Add( "GBInstallLocation", "Posizione di installazione" );
				Dictionary.Add( "GBInstallLocationBrowse", "Sfoglia..." );
				Dictionary.Add( "PrivacyStatement", "[Opzionale] Iscriviti per aggiornamenti e notizie su UDK." );
				Dictionary.Add( "PrivacyStatement2", "Leggi la nostra politica di riservatezza: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Installa opzioni: " );

				Dictionary.Add( "UpdateFileManifests", "Aggiorna file manifest" );

				Dictionary.Add( "IFInstallFinished", "Installazione completata" );
				Dictionary.Add( "IFInstallContentFinished", "Terminato di installare: " );
				Dictionary.Add( "IFLaunch", "Avvia " );
				Dictionary.Add( "IFFinished", "Terminato" );

				Dictionary.Add( "UIOLocation", "Posizione: " );
				Dictionary.Add( "UIOOptionsGame", "Disinstalla opzioni: " );
				Dictionary.Add( "DeletingFiles", "Eliminazione dei file" );
				Dictionary.Add( "DeletingShortcuts", "Eliminazione dei tasti di scelta rapida" );
				Dictionary.Add( "UnableDeleteFile", "Impossibile eliminare il suddetto file, molto probabilmente è aperto in un'altra applicazione." );
				Dictionary.Add( "UnableDeleteFolder", "Impossibile eliminare la suddetta cartella, molto probabilmente è aperta in un'altra applicazione." );
				Dictionary.Add( "DeleteRetry", "Vuoi riprovare a procedere con l'eliminazione?" );

				Dictionary.Add( "PBDecompressing", "Decomprimere file" );
				Dictionary.Add( "PBCompressing", "Comprimere file" );
				Dictionary.Add( "PBPackaging", "Assemblare i file" );
				Dictionary.Add( "PBDecompPrereq", "Decomprimere file prerequisiti" );
				Dictionary.Add( "PBPackagingPrereqs", "Assemblare file prerequisiti" );
				Dictionary.Add( "PBSettingInitial", "Preparazione per la prima esecuzione..." );

				Dictionary.Add( "RedistPleaseWait", "Installare prerequisiti" );
				Dictionary.Add( "RedistVCRedistHeader", "Redistribuibili Visual Studio 2010" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "Driver AMD CPU (unico passo per XP)" );

				Dictionary.Add( "RedistVCRedistContent", "Accedi gratuitamente alla documentazione e alle risorse di supporto di UDK su http://www.unrealengine.com/udk/documentation/" );
				Dictionary.Add( "RedistDXRedistContent", "UDK ti offre tutta la potenza di Unreal Engine 3 con la possibilità di accedere agli stessi straordinari strumenti utilizzati per realizzare i videogiochi campioni d'incasso." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "Impossibile installare VCRedist x86." );
				Dictionary.Add( "RedistVCRedistx64Fail", "Impossibile installare VCRedist x64." );
				Dictionary.Add( "RedistDXRedistFail", "Impossibile installare DirectX." );
				Dictionary.Add( "RedistAMDCPUFail", "Impossibile installare driver AMD CPU." );

				Dictionary.Add( "IFDependsFailed", "Le dipendenze non sono state installate; eseguire 'UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe'" );
				Dictionary.Add( "GQCaptionInvalidInstall", "Posizione di installazione non valida" );
				Dictionary.Add( "GQDescInvalidInstall", "Verifica che la posizione di installazione sia valida e che ci sia spazio sufficiente sul disco." );

				Dictionary.Add( "UE3RedistEULALegalese", "Ho letto e compreso la licenza del software, e ne accetto le condizioni facendo clic sul tasto 'Accetto'." );
				Dictionary.Add( "NetCodeWarn", "Unreal Development Kit (UDK) è un pacchetto di strumenti per lo sviluppo di videogiochi. Le applicazioni realizzate con UDK contengono un codice non verificato da Epic Games, Inc. Al pari di qualsiasi applicazione scaricata da Internet, vanno installati software esclusivamente da fonti affidabili. L'uso è a proprio rischio." );

				Dictionary.Add( "ExtractFailCaption", "Installazione non riuscita" );
				Dictionary.Add( "ExtractFailMessage1", "Estrazione dei file non riuscita con eccezione: " );
				Dictionary.Add( "ExtractFailMessage2", "Consulta http://udk.com/troubleshooting per l'assistenza." );
				Dictionary.Add( "UIODeleteOnly", "Disinstalla file originali e temporanei (esclusi i file creati dall'utente)." );
				Dictionary.Add( "UIODeleteAll", "Disinstalla tutti i file (inclusi i file creati dall'utenti)." );

				Dictionary.Add( "RedistVCRedistContentTech", "Il pacchetto ridistribuibile Microsoft Visual C++ 2010 (x86) installa componenti runtime di librerie Visual C++ richiesti per eseguire applicazioni sviluppate con Visual C++ SP1 su un computer sul quale non è installato Visual C++ 2010. Il presente pacchetto installa componenti runtime di librerie C Runtime (CRT), Standard C++, ATL, MFC, OpenMP e MSDIA." );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX è un pacchetto che fa dei computer con sistema operativo Windows una piattaforma ideale per eseguire e visualizzare applicazioni ricche di elementi multimediali quali grafica full-color, video, animazione 3D e rich audio." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Consente al sistema di regolare automaticamente la velocità della CPU, il voltaggio e la combinazione di potenza adeguati alla necessità istantanea di performance dell'utente. Questo driver supporta processori AMD Athlon 64 X2 Dual Core sulle edizioni Windows XP SP2, Windows 2003 SP1 x86 e x64." );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "Indirizzo e-mail" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "Fehlermeldung: " );
				Dictionary.Add( "UDKTrouble", "Mögliche Lösungen unter: " );
				Dictionary.Add( "RegUDK", "UDK wird registriert" );
				Dictionary.Add( "RegGame", "Spiel wird registriert" );
				Dictionary.Add( "CleaningUp", "Datenbereinigung" );

				Dictionary.Add( "GQCaptionNoEULA", "Endbenutzer-Lizenzvertrag nicht gefunden" );
				Dictionary.Add( "GQDescNoEULA", "Endbenutzer-Lizenzvertrag nicht gefunden – Installation wird abgebrochen." );
				Dictionary.Add( "GQCaptionInstallFail", "Installation fehlgeschlagen" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "Alle Dateien löschen?" );
				Dictionary.Add( "GQDescUninstallAllSure", "Hiermit werden alle Dateien gelöscht, einschließlich sämtlicher erstellter Karten und anderer Daten. Fortsetzen?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "Deinstallation erfolgreich!" );
				Dictionary.Add( "GQDescUninstallComplete", "Alle Dateien wurden gelöscht." );
				Dictionary.Add( "GQCaptionSubscribe", "Anmeldung in der Mailingliste" );
				Dictionary.Add( "GQDescSubscribe", "Ihre E-Mail-Adresse wird für den Empfang der neusten Marketing- und Technik-Nachrichten zum Unreal Development Kit angemeldet." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "Installation des Redistributable vollständig!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "Alle erforderlichen Redistributable-Dateien wurden ordnungsgemäß installiert." );
				Dictionary.Add( "GQCaptionWaiting", "Wartet auf Installation ..." );
				Dictionary.Add( "GQDescWaiting", "Wartet zum Fertigstellen auf Installation einer erforderlichen Datei ..." );
				Dictionary.Add( "GQCaptionDelFileFail", "Löschen der Datei fehlgeschlagen" );
				Dictionary.Add( "GQCaptionDelFolderFail", "Löschen des Ordners fehlgeschlagen" );
				Dictionary.Add( "GQCaptionCorrupt", "Fehlerhaftes Paket" );
				Dictionary.Add( "GQDescCorrupt1", "Das UDK-Paket ist fehlerhaft. Bitte erneut herunterladen (Signatur nicht auffindbar)." );
				Dictionary.Add( "GQDescCorrupt2", "Das UDK-Paket ist fehlerhaft. Bitte erneut herunterladen (außer beim Öffnen einer Zip-Datei)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Bitte Spielbezeichnung festlegen" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Bitte gültige Strings für die Felder 'GameName' und 'GameLongName' eingeben." );

				Dictionary.Add( "ButAccept", "Akzeptieren" );
				Dictionary.Add( "ButReject", "Ablehnen" );
				Dictionary.Add( "ButInstall", "Installieren" );
				Dictionary.Add( "ButUninstall", "Deinstallieren" );

				Dictionary.Add( "EULATitle", "Endbenutzer-Lizenzvertrag" );
				Dictionary.Add( "EULALegalese", "Ich habe die UDK-Softwarelizenz gelesen und zur Kenntnis genommen. Durch Anklicken der Schaltfläche 'Akzeptieren' erkläre ich mich mit den Bedingungen einverstanden. Mein Einverständnis ersetzt meine Unterschrift am Ende des Vertrags." );

				Dictionary.Add( "GBInstallLocation", "Installationsort" );
				Dictionary.Add( "GBInstallLocationBrowse", "Durchsuchen ..." );
				Dictionary.Add( "PrivacyStatement", "[Optional] Melden Sie sich für UDK-Updates und -Nachrichten an." );
				Dictionary.Add( "PrivacyStatement2", "Lesen Sie unsere Datenschutzrichtlinien: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Optionen für die Installation: " );

				Dictionary.Add( "UpdateFileManifests", "Dateilisten aktualisieren" );

				Dictionary.Add( "IFInstallFinished", "Installation abgeschlossen" );
				Dictionary.Add( "IFInstallContentFinished", "Installation beendet: " );
				Dictionary.Add( "IFLaunch", "Starten " );
				Dictionary.Add( "IFFinished", "Abgeschlossen" );

				Dictionary.Add( "UIOLocation", "Ort: " );
				Dictionary.Add( "UIOOptionsGame", "Optionen für die Deinstallation: " );
				Dictionary.Add( "DeletingFiles", "Löscht Dateien" );
				Dictionary.Add( "DeletingShortcuts", "Löscht Shortcuts" );
				Dictionary.Add( "UnableDeleteFile", "Die oben genannte Datei konnte nicht gelöscht werden, weil sie wahrscheinlich von einer anderen Anwendung verwendet wird." );
				Dictionary.Add( "UnableDeleteFolder", "Der oben genannte Ordner konnte nicht gelöscht werden, weil er wahrscheinlich von einer anderen Anwendung verwendet wird." );
				Dictionary.Add( "DeleteRetry", "Soll der Löschvorgang abgebrochen werden?" );

				Dictionary.Add( "PBDecompressing", "Dekomprimiert die Dateien" );
				Dictionary.Add( "PBCompressing", "Komprimiert die Dateien" );
				Dictionary.Add( "PBPackaging", "Packt die Dateien" );
				Dictionary.Add( "PBDecompPrereq", "Dekomprimiert die erforderlichen Dateien" );
				Dictionary.Add( "PBPackagingPrereqs", "Packt die erforderlichen Dateien" );
				Dictionary.Add( "PBSettingInitial", "Vorbereitung auf die erste Ausführung..." );

				Dictionary.Add( "RedistPleaseWait", "Installiert die erforderlichen Dateien" );
				Dictionary.Add( "RedistVCRedistHeader", "Redistributables für Visual Studio 2010" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "AMD CPU-Treiber (nur für XP)" );

				Dictionary.Add( "RedistVCRedistContent", "Unter http://www.unrealengine.com/udk/documentation/ finden Sie Gratis-Referenzwerke und -Hilfsmittel für das UDK." );
				Dictionary.Add( "RedistDXRedistContent", "Das UDK bietet Ihnen die volle Leistung der Unreal Engine 3 und ermöglicht Ihnen den Zugriff auf dieselben Weltklasse-Tools, die zur Produktion großer Videospiel-Hits verwendet werden." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "Installation von VCRedist x86 fehlgeschlagen." );
				Dictionary.Add( "RedistVCRedistx64Fail", "Installation von VCRedist x64 fehlgeschlagen." );
				Dictionary.Add( "RedistDXRedistFail", "Installation von DirectX fehlgeschlagen." );
				Dictionary.Add( "RedistAMDCPUFail", "Installation der AMD CPU-Treiber fehlgeschlagen." );

				Dictionary.Add( "IFDependsFailed", "Die abhängigen Komponenten wurden nicht installiert – bitte 'UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe' ausführen" );
				Dictionary.Add( "GQCaptionInvalidInstall", "Ungültiger Installationspfad " );
				Dictionary.Add( "GQDescInvalidInstall", "Überprüfen Sie bitte, ob der Installationspfad gültig und genügend freier Speicher auf dem Laufwerk vorhanden ist." );

				Dictionary.Add( "UE3RedistEULALegalese", "Ich habe die Softwarelizenz gelesen und zur Kenntnis genommen. Durch Anklicken der Schaltfläche 'Akzeptieren' erkläre ich mich mit den Bedingungen einverstanden." );
				Dictionary.Add( "NetCodeWarn", "Das Unreal Development Kit (UDK) ist ein Werkzeugset zur Spieleentwicklung. Anwendungen, die mit dem UDK erstellt wurden, enthalten Codes, die nicht von Epic Games, Inc. geprüft wurden. Wie bei jeder anderen Anwendung, die Sie aus dem Internet herunterladen, sollten Sie auch diese Software nur dann installieren, wenn sie von einer vertrauenswürdigen Quelle stammt. Die Nutzung erfolgt auf eigene Gefahr." );

				Dictionary.Add( "ExtractFailCaption", "Installation fehlgeschlagen" );
				Dictionary.Add( "ExtractFailMessage1", "Extraktion aller fehlgeschlagenen Dateien mit Ausnahme von: " );
				Dictionary.Add( "ExtractFailMessage2", "Weitere Hilfe erhalten Sie unter http://udk.com/troubleshooting." );
				Dictionary.Add( "UIODeleteOnly", "Originaldateien und and temporäre Dateien deinstallieren (nicht durch den Anwender erstellte Dateien)." );
				Dictionary.Add( "UIODeleteAll", "Alle Dateien deinstallieren (einschließlich der durch den Anwender erstellten Dateien)." );

				Dictionary.Add( "RedistVCRedistContentTech", "Das Redistributable-Paket für Microsoft Visual C++ 2010 (x86) installiert Laufzeitkomponenten von Visual C++-Bibliotheken, die notwendig sind, um Anwendungen auszuführen, die mit Visual C++ SP1 auf Computern ohne Visual C++ 2010-Installation entwickelt wurden. Dieses Paket installiert Laufzeitkomponenten der folgenden Bibliotheken: C-Laufzeitbibliothek (CRT), C++-Standardbibliothek, ATL, MFC, OpenMP und MSDIA." );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX ist eine Gruppe von Technologien, dank derer Windows-basierte Computer zu einer idealen Plattform zum Betreiben und Anzeigen von Anwendungen werden, die viele multimediale Elemente wie vollfarbige Grafiken, Videos, 3-D-Animationen und große Audiodateien enthalten." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Hiermit wählt das System automatisch die beste Kombination aus CPU-Geschwindigkeit, Spannung und Leistung, die für die aktuell anstehenden Arbeitsschritte notwendig ist. Dieser Treiber unterstützt unter Windows XP SP2 sowie den x86- und x64-Versionen von Windows 2003 SP1 die Dualcore-Prozessoren des Typs AMD Athlon 64 X2." );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "E-Mail-Adresse" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "Mensaje de error: " );
				Dictionary.Add( "UDKTrouble", "Para ver posibles soluciones, vaya a: " );
				Dictionary.Add( "RegUDK", "Registrando UDK" );
				Dictionary.Add( "RegGame", "Registrando el juego" );
				Dictionary.Add( "CleaningUp", "Limpiando" );

				Dictionary.Add( "GQCaptionNoEULA", "No se encontró el CLUF" );
				Dictionary.Add( "GQDescNoEULA", "No se encontró el CLUF; anulando la instalación." );
				Dictionary.Add( "GQCaptionInstallFail", "Error de instalación" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "¿Eliminar todos los archivos?" );
				Dictionary.Add( "GQDescUninstallAllSure", "Se eliminarán todos los archivos, incluyendo cualquier mapa u objeto que haya creado. ¿Continuar?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "¡Desinstalación completa!" );
				Dictionary.Add( "GQDescUninstallComplete", "Se han eliminado todos los archivos correctamente." );
				Dictionary.Add( "GQCaptionSubscribe", "Suscribiéndose a la lista de distribución de correo" );
				Dictionary.Add( "GQDescSubscribe", "Suscribiendo su dirección de correo electrónico para que reciba actualizaciones técnicas y de marketing sobre Unreal Development Kit." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "¡Instalación redistribuible completa!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "Todos los requisitos previos redistribuibles se han instalado correctamente." );
				Dictionary.Add( "GQCaptionWaiting", "Procesamiento en curso..." );
				Dictionary.Add( "GQDescWaiting", "Esperando a que se complete la instalación del requisito previo..." );
				Dictionary.Add( "GQCaptionDelFileFail", "No se pudo eliminar el archivo" );
				Dictionary.Add( "GQCaptionDelFolderFail", "No se pudo eliminar la carpeta" );
				Dictionary.Add( "GQCaptionCorrupt", "Paquete dañado" );
				Dictionary.Add( "GQDescCorrupt1", "El paquete de UDK está dañado. Vuelva a cargarlo (no se pudo encontrar la firma)." );
				Dictionary.Add( "GQDescCorrupt2", "El paquete de UDK está dañado. Vuelva a cargarlo (excepción al abrir el zip)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Configure el nombre del juego" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Configure los campos 'GameName' y 'GameLongName' para validar las secuencias." );

				Dictionary.Add( "ButAccept", "Acepto" );
				Dictionary.Add( "ButReject", "Rechazar" );
				Dictionary.Add( "ButInstall", "Instalar" );
				Dictionary.Add( "ButUninstall", "Desinstalar" );

				Dictionary.Add( "EULATitle", "Contrato de licencia para el usuario final" );
				Dictionary.Add( "EULALegalese", "He leído y entendido el contrato de licencia de software y, al hacer clic en el botón 'Acepto', confirmo que estoy de acuerdo con sus términos. Mi aceptación se considerará equivalente a mi firma al final del contrato." );

				Dictionary.Add( "GBInstallLocation", "Instalar ubicación" );
				Dictionary.Add( "GBInstallLocationBrowse", "Examinar..." );
				Dictionary.Add( "PrivacyStatement", "[Opcional] Suscribirse para recibir actualizaciones y noticias de UDK." );
				Dictionary.Add( "PrivacyStatement2", "Lea nuestra política de privacidad: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Opciones de instalación: " );

				Dictionary.Add( "UpdateFileManifests", "Actualizar manifiestos" );

				Dictionary.Add( "IFInstallFinished", "Instalación completa" );
				Dictionary.Add( "IFInstallContentFinished", "Finalizó la instalación: " );
				Dictionary.Add( "IFLaunch", "Iniciar " );
				Dictionary.Add( "IFFinished", "Finalizado" );

				Dictionary.Add( "UIOLocation", "Ubicación: " );
				Dictionary.Add( "UIOOptionsGame", "Opciones de desinstalación: " );
				Dictionary.Add( "DeletingFiles", "Eliminando archivos" );
				Dictionary.Add( "DeletingShortcuts", "Eliminando accesos directos" );
				Dictionary.Add( "UnableDeleteFile", "No se pudo eliminar el archivo indicado, probablemente porque está siendo utilizado por otra aplicación." );
				Dictionary.Add( "UnableDeleteFolder", "No se pudo eliminar la carpeta indicada, probablemente porque está siendo utilizado por otra aplicación." );
				Dictionary.Add( "DeleteRetry", "¿Desea volver a intentar eliminar?" );

				Dictionary.Add( "PBDecompressing", "Descomprimiendo archivos" );
				Dictionary.Add( "PBCompressing", "Comprimiendo archivos" );
				Dictionary.Add( "PBPackaging", "Empaquetando archivos" );
				Dictionary.Add( "PBDecompPrereq", "Descomprimiendo archivos de requisitos previos" );
				Dictionary.Add( "PBPackagingPrereqs", "Empaquetando archivos de requisitos previos" );
				Dictionary.Add( "PBSettingInitial", "Preparación para la primera ejecución..." );

				Dictionary.Add( "RedistPleaseWait", "Instalando requisitos previos" );
				Dictionary.Add( "RedistVCRedistHeader", "Redistribuibles de Visual Studio 2010" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "AMD CPU Drivers (solo para XP)" );

				Dictionary.Add( "RedistVCRedistContent", "Acceda de manera gratuita a los recursos y a la documentación de ayuda de UDK en http://www.unrealengine.com/udk/documentation/" );
				Dictionary.Add( "RedistDXRedistContent", "UDK le ofrece toda la potencia de Unreal Engine 3 y acceso a las mismas herramientas universales usadas para crear videojuegos de gran éxito." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "No se pudo instalar VCRedist x86." );
				Dictionary.Add( "RedistVCRedistx64Fail", "No se pudo instalar VCRedist x64." );
				Dictionary.Add( "RedistDXRedistFail", "No se pudo instalar DirectX." );
				Dictionary.Add( "RedistAMDCPUFail", "No se pudieron instalar los AMD CPU Drivers." );

				Dictionary.Add( "IFDependsFailed", "No se instalaron dependencias; ejecute 'UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe'." );
				Dictionary.Add( "GQCaptionInvalidInstall", "Ubicación de instalación no válida" );
				Dictionary.Add( "GQDescInvalidInstall", "Compruebe si la ubicación de la instalación es válida y si hay suficiente espacio en el disco." );

				Dictionary.Add( "UE3RedistEULALegalese", "He leído y entendido el contrato de licencia de software y, al hacer clic en el botón 'Acepto', confirmo que estoy de acuerdo con sus términos." );
				Dictionary.Add( "NetCodeWarn", "Unreal Development Kit (UDK) es un conjunto de herramientas de desarrollo de juegos. Las aplicaciones creadas con UDK contienen un código que no ha sido examinado por Epic Games, Inc. Como con cualquier otra aplicación descargada de Internet, sólo debería instalar este software si confía en su procedencia. Úselo bajo su responsabilidad." );

				Dictionary.Add( "ExtractFailCaption", "Error de instalación" );
				Dictionary.Add( "ExtractFailMessage1", "Extrayendo todos los archivos que han fallado con la excepción: " );
				Dictionary.Add( "ExtractFailMessage2", "Visite http://udk.com/troubleshooting para obtener ayuda." );
				Dictionary.Add( "UIODeleteOnly", "Desinstalar archivos originales y temporales (no los archivos creados por el usuario)." );
				Dictionary.Add( "UIODeleteAll", "Desinstalar todos los archivos (incluyendo los archivos creados por el usuario)." );

				Dictionary.Add( "RedistVCRedistContentTech", "El Paquete Redistribuible Microsoft Visual C++ 2010 (x86) instala los componentes de tiempo de ejecución de las Bibliotecas Visual C++ necesarios para ejecutar aplicaciones desarrolladas con Visual C++ SP1 en un equipo que no tenga instalado Visual C++ 2010. Este paquete instala componentes de tiempo de ejecución de las librerías C Runtime (CRT), Standard C++, ATL, MFC, OpenMP y MSDIA." );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX es un grupo de tecnologías diseñadas para convertir los equipos de Windows en una plataforma ideal para ejecutar y mostrar aplicaciones con numerosos elementos multimedia, como gráficos a todo color, vídeo, animación en 3D y sonido enriquecido." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Permite que el sistema ajuste automáticamente la velocidad de la CPU, el voltaje y la combinación de potencia para que coincida con las necesidades de rendimiento instantáneas del usuario. Este controlador es compatible con los procesadores AMD Athlon 64 X2 Dual Core en Windows XP SP2, Windows 2003 SP1 x86 y x64 Editions." );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "Dirección de correo electrónico" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "Mensaje de error: " );
				Dictionary.Add( "UDKTrouble", "Para ver las soluciones posibles, vaya a: " );
				Dictionary.Add( "RegUDK", "Registrando UDK" );
				Dictionary.Add( "RegGame", "Registrando Juego" );
				Dictionary.Add( "CleaningUp", "Limpiando" );

				Dictionary.Add( "GQCaptionNoEULA", "No se encuentra el CLUF" );
				Dictionary.Add( "GQDescNoEULA", "No se encuentra el CLUF; cancelando instalación." );
				Dictionary.Add( "GQCaptionInstallFail", "Error al instalar" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "¿Eliminar todos los archivos?" );
				Dictionary.Add( "GQDescUninstallAllSure", "Se eliminarán todos los archivos, incluidos los mapas o activos que haya creado. ¿Desea continuar?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "¡Se completó la desinstalación!" );
				Dictionary.Add( "GQDescUninstallComplete", "Todos los archivos se eliminaron correctamente." );
				Dictionary.Add( "GQCaptionSubscribe", "Suscribiéndose a la Lista de correo" );
				Dictionary.Add( "GQDescSubscribe", "Suscribiendo su dirección de correo electrónico para recibir actualizaciones comerciales y técnicas de Unreal Development Kit." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "Se completó la instalación redistribuible!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "Todos los prerrequisitos redistribuibles se instalaron correctamente." );
				Dictionary.Add( "GQCaptionWaiting", "Esperando al Proceso..." );
				Dictionary.Add( "GQDescWaiting", "Esperando que se complete la instalación de un prerrequisito..." );
				Dictionary.Add( "GQCaptionDelFileFail", "Error al eliminar el archivo" );
				Dictionary.Add( "GQCaptionDelFolderFail", "Error al eliminar la carpeta" );
				Dictionary.Add( "GQCaptionCorrupt", "Paquete dañado" );
				Dictionary.Add( "GQDescCorrupt1", "El paquete de UDK está dañado, vuelva a descargarlo (No se encontró la firma)." );
				Dictionary.Add( "GQDescCorrupt2", "El paquete de UDK está dañado, vuelva a descargarlo (Error al abrir el archivo comprimido)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Establezca el nombre del juego" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Establezca los campos 'GameName' y 'GameLongName' como cadenas válidas." );

				Dictionary.Add( "ButAccept", "Acepto" );
				Dictionary.Add( "ButReject", "No acepto" );
				Dictionary.Add( "ButInstall", "Instalar" );
				Dictionary.Add( "ButUninstall", "Desinstalar" );

				Dictionary.Add( "EULATitle", "Contrato de licencia de usuario final" );
				Dictionary.Add( "EULALegalese", "He leído y comprendido la licencia del software de UDK y acepto sus términos al hacer clic en el botón 'Acepto'. Mi aceptación debe considerarse como mi firma al final del acuerdo." );

				Dictionary.Add( "GBInstallLocation", "Ubicación de la instalación" );
				Dictionary.Add( "GBInstallLocationBrowse", "Explorar..." );
				Dictionary.Add( "PrivacyStatement", "[Opcional] Regístrese para obtener noticias y actualizaciones de UDK." );
				Dictionary.Add( "PrivacyStatement2", "Lea nuestra Política de privacidad: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Opciones para la instalación: " );

				Dictionary.Add( "UpdateFileManifests", "Actualizar manifiestos de los archivos" );

				Dictionary.Add( "IFInstallFinished", "Se completó la instalación" );
				Dictionary.Add( "IFInstallContentFinished", "Instalación finalizada: " );
				Dictionary.Add( "IFLaunch", "Iniciar " );
				Dictionary.Add( "IFFinished", "Finalizado" );

				Dictionary.Add( "UIOLocation", "Ubicación: " );
				Dictionary.Add( "UIOOptionsGame", "Opciones para la desinstalación: " );
				Dictionary.Add( "DeletingFiles", "Eliminando los archivos" );
				Dictionary.Add( "DeletingShortcuts", "Eliminando los accesos directos" );
				Dictionary.Add( "UnableDeleteFile", "No se pudo eliminar el archivo anterior, seguramente porque está abierto en otra aplicación." );
				Dictionary.Add( "UnableDeleteFolder", "No se pudo eliminar la carpeta anterior, seguramente porque está abierta en otra aplicación." );
				Dictionary.Add( "DeleteRetry", "¿Quiere volver a intentar eliminar?" );

				Dictionary.Add( "PBDecompressing", "Descomprimiendo archivos" );
				Dictionary.Add( "PBCompressing", "Comprimiendo archivos" );
				Dictionary.Add( "PBPackaging", "Empaquetando archivos" );
				Dictionary.Add( "PBDecompPrereq", "Descomprimiendo archivos prerrequeridos" );
				Dictionary.Add( "PBPackagingPrereqs", "Empaquetando archivos prerrequeridos" );
				Dictionary.Add( "PBSettingInitial", "Preparándose para la primera ejecución..." );

				Dictionary.Add( "RedistPleaseWait", "Instalando prerrequisitos" );
				Dictionary.Add( "RedistVCRedistHeader", "Redistribuibles de Visual Studio 2010" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "Controladores de CPU AMD(paso único de XP)" );

				Dictionary.Add( "RedistVCRedistContent", "Puede disponer de los recursos y la documentación de soporte de UDK gratuita en http://www.unrealengine.com/udk/documentation/" );
				Dictionary.Add( "RedistDXRedistContent", "UDK le da todo el poder del Unreal Engine 3 con acceso a las mismas herramientas de primera clase empleadas en la creación de algunos de los video juegos más exitosos." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "Falló la instalación de VCRedist x86." );
				Dictionary.Add( "RedistVCRedistx64Fail", "Falló la instalación de VCRedist x64." );
				Dictionary.Add( "RedistDXRedistFail", "Falló la instalación de DirectX." );
				Dictionary.Add( "RedistAMDCPUFail", "Falló la instalación de los controladores de AMD CPU." );

				Dictionary.Add( "IFDependsFailed", "No se instalaron las dependencias; ejecute 'UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe'" );
				Dictionary.Add( "GQCaptionInvalidInstall", "Ubicación de instalación inválida" );
				Dictionary.Add( "GQDescInvalidInstall", "Compruebe que la ubicación de la instalación sea válida y que haya espacio suficiente en el disco." );

				Dictionary.Add( "UE3RedistEULALegalese", "He leído la licencia del software, la he comprendido y acepto sus condiciones haciendo clic en el botón 'Acepto'." );
				Dictionary.Add( "NetCodeWarn", "Unreal Development Kit (UDK) es un conjunto de herramientas de desarrollo de juegos. Las aplicaciones creadas con UDK contienen códigos  que no han sido verificados por Epic Games, Inc. Como con cualquier aplicación que descargue de internet, usted debe instalar este software únicamente si confía en la fuente. Úselo bajo su propio riesgo." );

				Dictionary.Add( "ExtractFailCaption", "Falló la instalación" );
				Dictionary.Add( "ExtractFailMessage1", "Falló la extracción de todos los archivos con la excepción: " );
				Dictionary.Add( "ExtractFailMessage2", "Visite http://udk.com/troubleshooting para más información." );
				Dictionary.Add( "UIODeleteOnly", "Desinstalar archivos originales y temporales (no los archivos creados por el usuario)." );
				Dictionary.Add( "UIODeleteAll", "Desinstalar todos los archivos (incluyendo los archivos creados por el usuario)." );

				Dictionary.Add( "RedistVCRedistContentTech", "El Paquete Redistribuible de Microsoft Visual C++ 2010 (x86) instala los componentes en tiempo de ejecución de Visual C++ Libraries necesarios para ejecutar aplicaciones desarrolladas con Visual C++ SP1 en una computadora que no tiene instalado Visual C++ 2010. Este paquete instala componentes en tiempo de ejecución de bibliotecas de C Runtime (CRT), Standard C++, ATL, MFC, OpenMP y MSDIA libraries." );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX es un grupo de tecnologías diseñadas para hacer que las computadoras con Windows sean una plataforma ideal para ejecutar y mostrar aplicaciones con un gran contenido de elementos de multimedia como gráficos a todo color, video, animación 3D y audio de gran calidad." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Le permite al sistema ajustar automáticamente la combinación de velocidad del CPU, voltaje y energía de acuerdo a la necesidad de rendimiento instantáneo del usuario. Este controlador es compatible con procesadores AMD Athlon 64 X2 Dual Core con Windows XP SP2, Windows 2003 SP1 x86 y x64." );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "Dirección de correo electrónico" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "Сообщение об ошибке: " );
				Dictionary.Add( "UDKTrouble", "Для поиска возможных решений перейдите на: " );
				Dictionary.Add( "RegUDK", "Регистрация UDK" );
				Dictionary.Add( "RegGame", "Регистрация игры" );
				Dictionary.Add( "CleaningUp", "Очистка" );

				Dictionary.Add( "GQCaptionNoEULA", "Не удалось найти ЛСКП" );
				Dictionary.Add( "GQDescNoEULA", "Не удалось найти ЛСКП; прерывание установки." );
				Dictionary.Add( "GQCaptionInstallFail", "Установка не выполнена" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "Удалить все файлы?" );
				Dictionary.Add( "GQDescUninstallAllSure", "Это приведет к удалению всех файлов, включая любые созданные вами карты или цифровые объекты. Продолжить?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "Удаление завершено!" );
				Dictionary.Add( "GQDescUninstallComplete", "Все файлы успешно удалены." );
				Dictionary.Add( "GQCaptionSubscribe", "Подписка на рассылку" );
				Dictionary.Add( "GQDescSubscribe", "Подписка на ваш адрес электронной почты для получения маркетинговых и технических обновлений для Unreal Development Kit." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "Установка Redistributable завершена!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "Все необходимые компоненты для Redistributable были установлены правильно." );
				Dictionary.Add( "GQCaptionWaiting", "Ожидание процесса..." );
				Dictionary.Add( "GQDescWaiting", "Ожидание завершения установки необходимого компонента..." );
				Dictionary.Add( "GQCaptionDelFileFail", "Не удалось удалить файл" );
				Dictionary.Add( "GQCaptionDelFolderFail", "Не удалось удалить папку" );
				Dictionary.Add( "GQCaptionCorrupt", "Поврежденный пакет" );
				Dictionary.Add( "GQDescCorrupt1", "Пакет UDK поврежден, загрузите повторно (не удалось найти подпись)." );
				Dictionary.Add( "GQDescCorrupt2", "Пакет UDK поврежден, загрузите повторно (исключение при открытии zip)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Укажите название игры" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Укажите действительные строки для полей «GameName» и «GameLongName»." );

				Dictionary.Add( "ButAccept", "Я принимаю" );
				Dictionary.Add( "ButReject", "Отклонить" );
				Dictionary.Add( "ButInstall", "Установить" );
				Dictionary.Add( "ButUninstall", "Удалить" );

				Dictionary.Add( "EULATitle", "Лицензионное соглашение конечного пользователя" );
				Dictionary.Add( "EULALegalese", "Я прочел лицензию на программное обеспечение UDK, понимаю ее, и соглашаюсь с ее условиями, нажав на кнопку «Я принимаю». Мое принятие лицензии должно считаться моей подписью в конце договора." );

				Dictionary.Add( "GBInstallLocation", "Установить расположение" );
				Dictionary.Add( "GBInstallLocationBrowse", "Обзор..." );
				Dictionary.Add( "PrivacyStatement", "[Дополнительно] Подпишитесь на обновления UDK и новости." );
				Dictionary.Add( "PrivacyStatement2", "Ознакомьтесь с нашей политикой конфиденциальности: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Установить параметры: " );

				Dictionary.Add( "UpdateFileManifests", "Обновить списки файлов" );

				Dictionary.Add( "IFInstallFinished", "Установка завершена" );
				Dictionary.Add( "IFInstallContentFinished", "Завершенная установка: " );
				Dictionary.Add( "IFLaunch", "Запуск " );
				Dictionary.Add( "IFFinished", "Завершено" );

				Dictionary.Add( "UIOLocation", "Расположение: " );
				Dictionary.Add( "UIOOptionsGame", "Параметры удаления: " );
				Dictionary.Add( "DeletingFiles", "Удаление файлов" );
				Dictionary.Add( "DeletingShortcuts", "Удаление ярлыков" );
				Dictionary.Add( "UnableDeleteFile", "Не удалось удалить указанный выше файл, вероятно, потому, что он открыт в другом приложении." );
				Dictionary.Add( "UnableDeleteFolder", "Не удалось удалить указанную выше папку, вероятно, потому, что она открыта в другом приложении." );
				Dictionary.Add( "DeleteRetry", "Повторить попытку удаления?" );

				Dictionary.Add( "PBDecompressing", "Распаковка файлов" );
				Dictionary.Add( "PBCompressing", "Сжатие файлов" );
				Dictionary.Add( "PBPackaging", "Упаковка файлов" );
				Dictionary.Add( "PBDecompPrereq", "Обязательные файлы распаковки" );
				Dictionary.Add( "PBPackagingPrereqs", "Обязательные файлы упаковки" );
				Dictionary.Add( "PBSettingInitial", "Подготовка к первому запуску..." );

				Dictionary.Add( "RedistPleaseWait", "Необходимые компоненты для установки" );
				Dictionary.Add( "RedistVCRedistHeader", "Redistributables Visual Studio 2010" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "Драйверы процессоров AMD (шаг только для XP)" );

				Dictionary.Add( "RedistVCRedistContent", "Свободный доступ к сопроводительной документации и ресурсам UDK на http://www.unrealengine.com/udk/documentation/" );
				Dictionary.Add( "RedistDXRedistContent", "UDK дает вам полную мощь Unreal Engine 3 с доступом к тем же инструментам мирового класса, которые использовались для создания видеоигр–бестселлеров." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "Не удалось установить VCRedist x86." );
				Dictionary.Add( "RedistVCRedistx64Fail", "Не удалось установить VCRedist x64." );
				Dictionary.Add( "RedistDXRedistFail", "Не удалось установить DirectX." );
				Dictionary.Add( "RedistAMDCPUFail", "Не удалось установить драйверы для процессора AMD." );

				Dictionary.Add( "IFDependsFailed", "Зависимости не установлены; запустите «UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe»" );
				Dictionary.Add( "GQCaptionInvalidInstall", "Неверная папка для установки" );
				Dictionary.Add( "GQDescInvalidInstall", "Убедитесь, что папка для установки указана правильно и на диске достаточно места." );

				Dictionary.Add( "UE3RedistEULALegalese", "Я прочел лицензию на программное обеспечение, понимаю ее и соглашаюсь с ее условиями, нажав на кнопку «Я принимаю»." );
				Dictionary.Add( "NetCodeWarn", "Unreal Development Kit (UDK) представляет собой набор инструментов для разработки игр. Приложения в соавторстве с UDK содержат код, который не был проверен Epic Games, Inc. Как и любое приложение, загружаемое через Интернет, следует устанавливать эту программу, если вы доверяете источнику. Используйте на свой страх и риск." );

				Dictionary.Add( "ExtractFailCaption", "Установка не выполнена" );
				Dictionary.Add( "ExtractFailMessage1", "Не удалось извлечь все файлы за исключением: " );
				Dictionary.Add( "ExtractFailMessage2", "Посетите http://udk.com/troubleshooting для получения помощи." );
				Dictionary.Add( "UIODeleteOnly", "Удалите оригинальный и временный файлы (не созданные пользователем файлы)." );
				Dictionary.Add( "UIODeleteAll", "Удалите все файлы (в том числе созданные пользователем файлы)." );

				Dictionary.Add( "RedistVCRedistContentTech", "Пакет Redistributable (x86) Microsoft Visual C++ 2010 устанавливает компоненты среды выполнения библиотек Visual C++, необходимые для запуска приложений, разработанных с помощью Visual C++ SP1, на компьютере, на котором не установлен пакет Visual C++ 2010. Этот пакет устанавливает компоненты среды выполнения из библиотек C Runtime (CRT), Standard C++, ATL, MFC, OpenMP и MSDIA." );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX представляет собой группу технологий, разработанных для того, чтобы сделать компьютеры на базе Windows идеальной платформой для запуска и отображения приложений, насыщенных мультимедийными элементами, такими как полноцветная графика, видео, 3D-анимация и богатый звук." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Позволяет системе автоматически регулировать сочетание скорости процессора, напряжения и мощности, которое соответствует немедленной потребности пользователя в производительности. Этот драйвер поддерживает процессоры AMD Athlon 64 X2 Dual Core на Windows XP SP2, Windows 2003 SP1 x86 Edition и x64 Edition." );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "Адрес электронной почты" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "Komunikat błędu: " );
				Dictionary.Add( "UDKTrouble", "Możliwe rozwiązania: " );
				Dictionary.Add( "RegUDK", "Rejestrowanie zestawu UDK" );
				Dictionary.Add( "RegGame", "Rejestrowanie gry" );
				Dictionary.Add( "CleaningUp", "Oczyszczanie" );

				Dictionary.Add( "GQCaptionNoEULA", "Nie znaleziono umowy licencyjnej (EULA)" );
				Dictionary.Add( "GQDescNoEULA", "Nie znaleziono umowy licencyjnej (EULA). Zamykanie programu instalacyjnego." );
				Dictionary.Add( "GQCaptionInstallFail", "Instalacja nie powiodła się" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "Czy usunąć wszystkie pliki?" );
				Dictionary.Add( "GQDescUninstallAllSure", "Czynność spowoduje usunięcie wszystkich plików, w tym wszystkich map lub materiałów utworzonych przez użytkownika. Czy kontynuować?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "Dezinstalacja zakończona pomyślnie!" );
				Dictionary.Add( "GQDescUninstallComplete", "Wszystkie pliki zostały usunięte pomyślnie." );
				Dictionary.Add( "GQCaptionSubscribe", "Subskrypcja listy wysyłkowej" );
				Dictionary.Add( "GQDescSubscribe", "Dodawanie adresu e-mail użytkownika do listy wysyłkowej aktualizacji technicznych i informacji marketingowych o zestawie Unreal Development Kit." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "Instalacja pakietu redystrybucyjnego zakończona pomyślnie!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "Wszystkie wstępnie wymagane pakiety redystrybucyjne zostały zainstalowane poprawnie." );
				Dictionary.Add( "GQCaptionWaiting", "Oczekiwanie na proces..." );
				Dictionary.Add( "GQDescWaiting", "Oczekiwanie na zakończenie instalacji pakietu wstępnie wymaganego..." );
				Dictionary.Add( "GQCaptionDelFileFail", "Nie można usunąć pliku" );
				Dictionary.Add( "GQCaptionDelFolderFail", "Nie można usunąć folderu" );
				Dictionary.Add( "GQCaptionCorrupt", "Pakiet jest uszkodzony" );
				Dictionary.Add( "GQDescCorrupt1", "Pakiet zestawu UDK jest uszkodzony i należy pobrać go ponownie (nie można znaleźć podpisu)." );
				Dictionary.Add( "GQDescCorrupt2", "Pakiet zestawu UDK jest uszkodzony i należy pobrać go ponownie (wystąpił wyjątek podczas otwierania pliku archiwum)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Ustaw wartość pola GameName" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Ustaw wartości pól 'GameName' i 'GameLongName' na prawidłowe ciągi tekstowe." );

				Dictionary.Add( "ButAccept", "Zgadzam się" );
				Dictionary.Add( "ButReject", "Nie zgadzam się" );
				Dictionary.Add( "ButInstall", "Zainstaluj" );
				Dictionary.Add( "ButUninstall", "Odinstaluj" );

				Dictionary.Add( "EULATitle", "Umowa licencyjna użytkownika oprogramowania" );
				Dictionary.Add( "EULALegalese", "Oświadczam, że zapoznałem się z umową licencyjną zestawu UDK ze zrozumieniemi  wyrażam zgodę na jej warunki przez kliknięcie przycisku „Zgadzam się”. Wyrażenie przeze mnie zgody powinno zostać uznane za podpis pod umową licencyjną." );

				Dictionary.Add( "GBInstallLocation", "Lokalizacja instalacji" );
				Dictionary.Add( "GBInstallLocationBrowse", "Przeglądaj..." );
				Dictionary.Add( "PrivacyStatement", "[Opcjonalne] Zapisz się na listę wysyłkową, aby otrzymywać aktualizacje i informacje o zestawie UDK." );
				Dictionary.Add( "PrivacyStatement2", "Zapoznaj się z naszą Polityką ochrony: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Opcje instalacji: " );

				Dictionary.Add( "UpdateFileManifests", "Aktualizuj pliki manifestu" );

				Dictionary.Add( "IFInstallFinished", "Instalacja zakończona pomyślnie" );
				Dictionary.Add( "IFInstallContentFinished", "Zakończono instalację: " );
				Dictionary.Add( "IFLaunch", "Uruchom " );
				Dictionary.Add( "IFFinished", "Zakończ" );

				Dictionary.Add( "UIOLocation", "Lokalizacja: " );
				Dictionary.Add( "UIOOptionsGame", "Opcje dezinstalacji: " );
				Dictionary.Add( "DeletingFiles", "Usuwanie plików" );
				Dictionary.Add( "DeletingShortcuts", "Usuwanie plików skrótu" );
				Dictionary.Add( "UnableDeleteFile", "Nie można usunąć wymienionego pliku. Prawdopodobnie plik jest otwarty w innej aplikacji." );
				Dictionary.Add( "UnableDeleteFolder", "Nie można usunąć wymienionego folderu. Prawdopodobnie folder jest otwarty w innej aplikacji." );
				Dictionary.Add( "DeleteRetry", "Czy ponowić próbę usunięcia?" );

				Dictionary.Add( "PBDecompressing", "Dekompresowanie plików" );
				Dictionary.Add( "PBCompressing", "Kompresowanie plików" );
				Dictionary.Add( "PBPackaging", "Upakowywanie plików" );
				Dictionary.Add( "PBDecompPrereq", "Dekompresowanie plików wstępnie wymaganych" );
				Dictionary.Add( "PBPackagingPrereqs", "Upakowywanie plików wstępnie wymaganych" );
				Dictionary.Add( "PBSettingInitial", "Przygotowanie do pierwszego uruchomienia..." );

				Dictionary.Add( "RedistPleaseWait", "Instalowanie pakietów wstępnie wymaganych" );
				Dictionary.Add( "RedistVCRedistHeader", "Pakiet redystrybucyjny programu Visual Studio 2010" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "Sterowniki procesora AMD (tylko Windows XP)" );

				Dictionary.Add( "RedistVCRedistContent", "Dokumentacja techniczna i zasoby dotyczące zestawu UDK są dostępne na stronie http://www.unrealengine.com/udk/documentation/." );
				Dictionary.Add( "RedistDXRedistContent", "Zestaw UDK oferuje wszystkie możliwości silnika Unreal Engine 3 i zapewnia dostęp do światowej klasy narzędzi używanych do tworzenia przebojowych gier wideo." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "Instalacja pakietu VCRedist x86 nie powiodła się." );
				Dictionary.Add( "RedistVCRedistx64Fail", "Instalacja pakietu VCRedist x64 nie powiodła się." );
				Dictionary.Add( "RedistDXRedistFail", "Instalacja DirectX nie powiodła się." );
				Dictionary.Add( "RedistAMDCPUFail", "Instalacja sterowników procesora AMD nie powiodła się." );

				Dictionary.Add( "IFDependsFailed", "Pakiety zależne nie zostały zainstalowane. Uruchom program „UDK-FOLDER-INSTALACJI/Binaries/Redist/UE3Redist.exe”." );
				Dictionary.Add( "GQCaptionInvalidInstall", "Nieprawidłowa lokalizacja instalacji." );
				Dictionary.Add( "GQDescInvalidInstall", "Sprawdź, czy lokalizacja instalacji jest prawidłowa i czy na dysku jest wystarczająca ilość wolnego miejsca." );

				Dictionary.Add( "UE3RedistEULALegalese", "Oświadczam, że zapoznałem się z umową licencyjną zestawu UDK ze zrozumieniemi  wyrażam zgodę na jej warunki przez kliknięcie przycisku „Zgadzam się”." );
				Dictionary.Add( "NetCodeWarn", "Zestaw Unreal Development Kit (UDK) jest zbiorem narzędzi do tworzenia gier. Aplikacje utworzone za pomocą zestawu UDK zawierają fragmenty kodu, które nie zostały sprawdzone przez firmę Epic Games, Inc. Podobnie jak w przypadku innych aplikacji pobieranych z sieci Internet, niniejsze oprogramowanie należy instalować wyłącznie wtedy, gdy pochodzi ono z zaufanego źródła. Zestaw jest używany na wyłączną odpowiedzialność użytkownika." );

				Dictionary.Add( "ExtractFailCaption", "Instalacja nie powiodła się." );
				Dictionary.Add( "ExtractFailMessage1", "Wyodrębnianie plików nie powiodło się. Wyjątek: " );
				Dictionary.Add( "ExtractFailMessage2", "Aby uzyskać pomoc, odwiedź stronę http://udk.com/troubleshooting." );
				Dictionary.Add( "UIODeleteOnly", "Odinstaluj pliki programu i tymczasowe (bez plików utworzonych przez użytkownika)." );
				Dictionary.Add( "UIODeleteAll", "Odinstaluj wszystkie pliki (w tym pliki utworzone przez użytkownika)." );

				Dictionary.Add( "RedistVCRedistContentTech", "Pakiet redystrybucyjny programu Microsoft Visual C++ 2010 (x86) jest instalowany ze składnikami czasu wykonywania do bibliotek Visual C++ wymaganymi do uruchamiania aplikacji stworzonych w Visual C++ SP1 na komputerze, na którym program Visual C++ 2010 nie jest zainstalowany. Instalowane są składniki czasu wykonywania do bibliotek C Runtime (CRT), Standard C++, ATL, MFC, OpenMP i MSDIA." );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX jest zestawem technologii, które zamieniają komputery z systemem Windows w idealną platformę do uruchamiania i wyświetlania aplikacji multimedialnych z elementami takimi, jak grafika, wideo, animacje trójwymiarowe i dźwięk wysokiej jakości." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Pozwala systemowi automatycznie dostosowywać częstotliwość taktowania i napięcie procesora w zależności od wymaganej mocy obliczeniowej. Sterownik obsługuje dwurdzeniowe procesory AMD Athlon 64 X2 w systemach Windows XP SP2, Windows 2003 SP1 wersje x86 i x64." );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "Adres e-mail" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "Chybové hlášení: " );
				Dictionary.Add( "UDKTrouble", "Pro možná řešení jděte na: " );
				Dictionary.Add( "RegUDK", "Registruji UDK" );
				Dictionary.Add( "RegGame", "Registruji hru" );
				Dictionary.Add( "CleaningUp", "Uklízím po sobě" );

				Dictionary.Add( "GQCaptionNoEULA", "Nepodařilo se najít EULA" );
				Dictionary.Add( "GQDescNoEULA", "Nepodařilo se najít EULA; končím instalaci." );
				Dictionary.Add( "GQCaptionInstallFail", "Instalace se nezdařila" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "Odstranit všechny soubory?" );
				Dictionary.Add( "GQDescUninstallAllSure", "Toto odstraní všechny soubory včetně všech map nebo aktiv, které jste vytvořili. Pokračovat?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "Odinstalování dokončeno!" );
				Dictionary.Add( "GQDescUninstallComplete", "Všechny soubory úspěšně odstraněny." );
				Dictionary.Add( "GQCaptionSubscribe", "Zapisuji do seznamu odběratelů" );
				Dictionary.Add( "GQDescSubscribe", "Zapisuji vaši emailovou adresu pro zasílání marketingových a technických aktualizací Unreal Development Kit." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "Redistributovatelná instalace dokončena!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "Všechny redistributovatelné prerekvizity byly správně nainstalovány." );
				Dictionary.Add( "GQCaptionWaiting", "Čekám na zpracování..." );
				Dictionary.Add( "GQDescWaiting", "Čekám na dokončení instalace prerekvizit..." );
				Dictionary.Add( "GQCaptionDelFileFail", "Nepodařilo se odstranit soubor" );
				Dictionary.Add( "GQCaptionDelFolderFail", "Nepodařilo se odstranit složku" );
				Dictionary.Add( "GQCaptionCorrupt", "Poškozený balíček" );
				Dictionary.Add( "GQDescCorrupt1", "Balíček UDK je poškozený, stáhněte si laskavě znovu (nenalezen podpis)." );
				Dictionary.Add( "GQDescCorrupt2", "Balíček UDK je poškozený, stáhněte si laskavě znovu (výjimka při otvírání zip)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Zadejte prosím název hry" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Zadejte prosím do polí 'GameName' a 'GameLongName' platné řetězce znaků." );

				Dictionary.Add( "ButAccept", "Přijímám" );
				Dictionary.Add( "ButReject", "Odmítnout" );
				Dictionary.Add( "ButInstall", "Instalovat" );
				Dictionary.Add( "ButUninstall", "Odinstalovat" );

				Dictionary.Add( "EULATitle", "Licenční smlouva s koncovým uživatelem" );
				Dictionary.Add( "EULALegalese", "Přečetl jsem si licenční ustanovení UDK, porozuměl jsem mu a potvrzuji souhlas s podmínkami kliknutím na tlačítko 'Souhlasím'. Můj souhlas je považován za můj podpis na konci smlouvy." );

				Dictionary.Add( "GBInstallLocation", "Místo instalace" );
				Dictionary.Add( "GBInstallLocationBrowse", "Procházet..." );
				Dictionary.Add( "PrivacyStatement", "[Volitelné] Přihlaste se k odběru aktualizací a novinek UDK." );
				Dictionary.Add( "PrivacyStatement2", "Přečtěte si podmínky na ochranu osobních dat: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Možnosti instalace: " );

				Dictionary.Add( "UpdateFileManifests", "Aktualizovat seznamy souborů" );

				Dictionary.Add( "IFInstallFinished", "Instalace dokončena" );
				Dictionary.Add( "IFInstallContentFinished", "Instalace ukončena: " );
				Dictionary.Add( "IFLaunch", "Spustit " );
				Dictionary.Add( "IFFinished", "Ukončeno" );

				Dictionary.Add( "UIOLocation", "Umístění: " );
				Dictionary.Add( "UIOOptionsGame", "Možnosti odinstalace: " );
				Dictionary.Add( "DeletingFiles", "Odstraňuji soubory" );
				Dictionary.Add( "DeletingShortcuts", "Odstraňuji zkratky" );
				Dictionary.Add( "UnableDeleteFile", "Výše uvedený soubor nemohl být odstraněn s největší pravděpodobností proto, že je otevřený v jiné aplikaci." );
				Dictionary.Add( "UnableDeleteFolder", "Výše uvedená složka nemohla být odstraněna s největší pravděpodobností proto, že je otevřená v jiné aplikaci." );
				Dictionary.Add( "DeleteRetry", "Přejete si pokus o odstranění opakovat?" );

				Dictionary.Add( "PBDecompressing", "Rozbaluji soubory" );
				Dictionary.Add( "PBCompressing", "Sbaluji soubory" );
				Dictionary.Add( "PBPackaging", "Vytvářím balíčky souborů" );
				Dictionary.Add( "PBDecompPrereq", "Rozbaluji soubory prerekvizit" );
				Dictionary.Add( "PBPackagingPrereqs", "Vytvářím balíčky souborů prerekvizit" );
				Dictionary.Add( "PBSettingInitial", "Příprava na první spuštění…." );

				Dictionary.Add( "RedistPleaseWait", "Instaluji prerekvizity" );
				Dictionary.Add( "RedistVCRedistHeader", "Visual Studio 2010 redistributovatelné" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "Ovladače AMD CPU (krok jen u XP)" );

				Dictionary.Add( "RedistVCRedistContent", "Volně přístupná podpůrná dokumentace UDK a zdroje na http://www.unrealengine.com/udk/documentation/" );
				Dictionary.Add( "RedistDXRedistContent", "UDK vám poskytuje plný výkon Unreal Engine 3 s přístupem ke stejným nástrojům světové třídy, které se používají pro vytváření senzačních videoher." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "Instalace VCRedist x86 se nezdařila." );
				Dictionary.Add( "RedistVCRedistx64Fail", "Instalace VCRedist x64 se nezdařila." );
				Dictionary.Add( "RedistDXRedistFail", "Instalace DirectX se nezdařila." );
				Dictionary.Add( "RedistAMDCPUFail", "Instalace ovladačů AMD CPU se nezdařila." );

				Dictionary.Add( "IFDependsFailed", "Nebyly splněny závislosti; prosím spusťte 'UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe'" );
				Dictionary.Add( "GQCaptionInvalidInstall", "Neplatné místo instalace" );
				Dictionary.Add( "GQDescInvalidInstall", "Zkontrolujte platnost místa instalace a dostatek místa na disku." );

				Dictionary.Add( "UE3RedistEULALegalese", "Přečetl jsem si licenční ustanovení k softwaru, porozuměl jsem mu a potvrzuji souhlas s podmínkami kliknutím na tlačítko 'Souhlasím'." );
				Dictionary.Add( "NetCodeWarn", "Unreal Development Kit (UDK) je sada vývojových nástrojů pro hry. Aplikace schválené s UDK obsahují kód, který není zkontrolovaný od Epic Games, Inc. Jako všechny aplikace stahované prostřednictvím internetu byste měli instalovat tento software, jen pokud důvěřujete zdroji. Použití na vlastní nebezpečí." );

				Dictionary.Add( "ExtractFailCaption", "Instalace se nezdařila" );
				Dictionary.Add( "ExtractFailMessage1", "Rozbalení všech souborů se nezdařilo s výjimkou: " );
				Dictionary.Add( "ExtractFailMessage2", "Pomoc najdete na://udk.com/troubleshooting." );
				Dictionary.Add( "UIODeleteOnly", "Odinstalovat originální a dočasné soubory (ne soubory vytvořené uživatelem)." );
				Dictionary.Add( "UIODeleteAll", "Odinstalovat všechny soubory (včetně souborů vytvořených uživatelem)." );

				Dictionary.Add( "RedistVCRedistContentTech", "Redistributovatelný balíček (x86) Microsoft Visual C++ 2010 instaluje runtime komponenty knihoven Visual C++, které jsou nutné pro chod aplikací vyvinutých s Visual C++ SP1 na počítači, kde není nainstalovaný Visual C++ 2010. Tento balíček instaluje runtime komponenty knihoven C Runtime (CRT), Standard C++, ATL, MFC, OpenMP a MSDIA." );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX je skupina technologií navržených k tomu, aby vytvořily z počítačů s Windows ideální platformu ke spouštění a zobrazování aplikací se silným zastoupením multimediálních prvků jako je plně barevná grafika, video, 3D animace a bohaté audio." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Nechte systém automaticky nastavit kombinaci rychlosti CPU, napětí a výkonu, které odpovídají momentálním potřebám výkonnosti uživatele. Tento ovladač podporuje dvoujádrové procesory AMD Athlon 64 X2 na Windows XP SP2, Windows 2003 SP1 x86 a x64." );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "Adres e-mail" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "Hibaüzenet: " );
				Dictionary.Add( "UDKTrouble", "Lehetséges megoldások itt találhatók: " );
				Dictionary.Add( "RegUDK", "UDK regisztrálása" );
				Dictionary.Add( "RegGame", "Játék regisztrálása" );
				Dictionary.Add( "CleaningUp", "Rendezés" );

				Dictionary.Add( "GQCaptionNoEULA", "Az EULA nem található" );
				Dictionary.Add( "GQDescNoEULA", "Az EULA nem található; a telepítés megszakad." );
				Dictionary.Add( "GQCaptionInstallFail", "Telepítés sikertelen" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "Összes fájl törlése?" );
				Dictionary.Add( "GQDescUninstallAllSure", "Ezzel törli az összes fájlt, beleértve a saját térképeket és eszközöket is. Folytatja?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "Az eltávolítás befejeződött!" );
				Dictionary.Add( "GQDescUninstallComplete", "Az összes fájl törlése sikeresen befejeződött." );
				Dictionary.Add( "GQCaptionSubscribe", "Feliratkozás a levelezési listára" );
				Dictionary.Add( "GQDescSubscribe", "Ha feliratkozik a listára az e-mail címével, megkapja az Unreal Development Kittel kapcsolatos marketing- és technikai frissítéseket." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "A Redistributable telepítése befejeződött!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "A redistributable összes előfeltétele sikeresen telepítve lett." );
				Dictionary.Add( "GQCaptionWaiting", "Várakozás a folyamatra..." );
				Dictionary.Add( "GQDescWaiting", "Várakozás egy előfeltétel telepítésének befejezésére..." );
				Dictionary.Add( "GQCaptionDelFileFail", "Fájl törlése sikertelen" );
				Dictionary.Add( "GQCaptionDelFolderFail", "Mappa törlése sikertelen" );
				Dictionary.Add( "GQCaptionCorrupt", "Sérült csomag" );
				Dictionary.Add( "GQDescCorrupt1", "Az UDK csomag sérült, kérjük, töltse le újra (aláírás nem található)." );
				Dictionary.Add( "GQDescCorrupt2", "Az UDK csomag sérült, kérjük, töltse le újra (kivétel történt a zip megnyitásakor)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Állítsa be a játék nevét" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Adjon meg érvényes karakterláncokat a 'GameName' és 'GameLongName' mezőkben." );

				Dictionary.Add( "ButAccept", "Elfogadom" );
				Dictionary.Add( "ButReject", "Nem fogadom el" );
				Dictionary.Add( "ButInstall", "Telepítés" );
				Dictionary.Add( "ButUninstall", "Eltávolítás" );

				Dictionary.Add( "EULATitle", "Végfelhasználói licencszerződés" );
				Dictionary.Add( "EULALegalese", "Elolvastam az UDK szoftverlicencét, megértettem annak tartalmát, és az 'Elfogadom' gombra kattintva elfogadom a licenc feltételeit. Elfogadásom a szerződés utolsó oldalának aláírásával egyenértékű." );

				Dictionary.Add( "GBInstallLocation", "Telepítés helye" );
				Dictionary.Add( "GBInstallLocationBrowse", "Tallózás..." );
				Dictionary.Add( "PrivacyStatement", "[Opcionális] Feliratkozás az UDK frissítések és hírek levelezési listára." );
				Dictionary.Add( "PrivacyStatement2", "Olvassa el az Adatvédelmi szabályzatunkat: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Telepítési beállítások: " );

				Dictionary.Add( "UpdateFileManifests", "Fájllisták frissítése" );

				Dictionary.Add( "IFInstallFinished", "A telepítés befejeződött" );
				Dictionary.Add( "IFInstallContentFinished", "Telepítés befejezve: " );
				Dictionary.Add( "IFLaunch", "Indítás " );
				Dictionary.Add( "IFFinished", "Kész" );

				Dictionary.Add( "UIOLocation", "Hely: " );
				Dictionary.Add( "UIOOptionsGame", "Eltávolítási beállítások: " );
				Dictionary.Add( "DeletingFiles", "Fájlok törlése" );
				Dictionary.Add( "DeletingShortcuts", "Parancsikonok törlése" );
				Dictionary.Add( "UnableDeleteFile", "A fenti fájl nem törölhető, valószínűleg azért, mert meg van nyitva egy másik alkalmazásban." );
				Dictionary.Add( "UnableDeleteFolder", "A fenti mappa nem törölhető, valószínűleg azért, mert meg van nyitva egy másik alkalmazásban." );
				Dictionary.Add( "DeleteRetry", "Megpróbálja újra a törlést?" );

				Dictionary.Add( "PBDecompressing", "Fájlok kicsomagolása" );
				Dictionary.Add( "PBCompressing", "Fájlok tömörítése" );
				Dictionary.Add( "PBPackaging", "Fájlok csomagolása" );
				Dictionary.Add( "PBDecompPrereq", "Előfeltétel-fájlok kicsomagolása" );
				Dictionary.Add( "PBPackagingPrereqs", "Előfeltétel-fájlok csomagolása" );
				Dictionary.Add( "PBSettingInitial", "Előkészület az első futtatásra..." );

				Dictionary.Add( "RedistPleaseWait", "Előfeltételek telepítése" );
				Dictionary.Add( "RedistVCRedistHeader", "Visual Studio 2010 Redistributables" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "AMD processzor-illesztőprogramok (csak XP esetén végrehajtott lépés)" );

				Dictionary.Add( "RedistVCRedistContent", "Az UDK támogatási dokumentáció és egyéb erőforrások szabadon elérhetők a webhelyen" );
				Dictionary.Add( "RedistDXRedistContent", "Az UDK az Unreal Engine 3 teljes teljesítményét biztosítja, olyan világszínvonalú eszközök használatának lehetőségével, amilyenekkel a legsikeresebb videojátékokat is készítették." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "VCRedist x86 telepítése sikertelen." );
				Dictionary.Add( "RedistVCRedistx64Fail", "VCRedist x64 telepítése sikertelen." );
				Dictionary.Add( "RedistDXRedistFail", "DirectX telepítése sikertelen." );
				Dictionary.Add( "RedistAMDCPUFail", "AMD processzor-illesztőprogramok telepítése sikertelen." );

				Dictionary.Add( "IFDependsFailed", "A függőségek nem lettek telepítve; futtassa az 'UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe' fájlt" );
				Dictionary.Add( "GQCaptionInvalidInstall", "Érvénytelen telepítési hely" );
				Dictionary.Add( "GQDescInvalidInstall", "Ellenőrizze, hogy a telepítési hely érvényes, valamint azt, hogy elegendő szabad hely van a lemezen." );

				Dictionary.Add( "UE3RedistEULALegalese", "Elolvastam a szoftverlicencet, megértettem annak tartalmát, és az 'Elfogadom' gombra kattintva elfogadom a licenc feltételeit." );
				Dictionary.Add( "NetCodeWarn", "Az Unreal Development Kit (UDK) egy játékfejlesztő eszközkészlet. Az UDK-val írt alkalmazások olyan kódot tartalmaznak, amelyet az Epic Games, Inc. nem vizsgált meg. Mint bármely, az internetről letöltött alkalmazást, az ilyen szoftvert is csak akkor telepítse, ha megbízik a forrásában. Csak saját felelősségére használja." );

				Dictionary.Add( "ExtractFailCaption", "Telepítés sikertelen" );
				Dictionary.Add( "ExtractFailMessage1", "A fájlok kicsomagolása sikertelen, kivétel: " );
				Dictionary.Add( "ExtractFailMessage2", "Segítségért látogasson el a http://udk.com/troubleshooting webhelyre." );
				Dictionary.Add( "UIODeleteOnly", "Eredeti és ideiglenes fájlok (nem a felhasználó által létrehozott fájlok) eltávolítása." );
				Dictionary.Add( "UIODeleteAll", "Az összes fájl eltávolítása (beleértve a felhasználó által létrehozott fájlokat is)." );

				Dictionary.Add( "RedistVCRedistContentTech", "A Microsoft Visual C++ 2010 Redistributable Package (x86) a Visual C++ Libraries futásidejű összetevőit telepíti, amelyek a Visual C++ SP1 segítségével fejlesztett alkalmazások futtatásához szükségesek olyan számítógépen, amelyen nincs telepítve a Visual C++ 2010 csomag. Ez a csomag a C Runtime (CRT), Standard C++, ATL, MFC, OpenMP és MSDIA könyvtárak futásidejű összetevőit telepíti." );
				Dictionary.Add( "RedistDXRedistContentTech", "A Microsoft DirectX olyan technológiák csoportja, amely a Windows-alapú számítógépeket ideális platformmá teszi multimédiás tartalmakban (például színes grafika, videó, 3D animációk és összetett hangok) gazdag alkalmazások futtatására és megjelenítésére." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Lehetővé teszi, hogy a rendszer automatikusan beállítsa a CPU sebességének, feszültségének és teljesítményének a felhasználó éppen aktuális teljesítményszükségleteihez megfelelő kombinációját. Ez az illesztőprogram az AMD Athlon 64 X2 Dual Core processzorokat támogatja Windows XP SP2, Windows 2003 SP1 x86 és x64 operációs rendszereken." );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "E-mail cím" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "Chybové hlásenie: " );
				Dictionary.Add( "UDKTrouble", "Pre možné riešenia prejdite na: " );
				Dictionary.Add( "RegUDK", "Registrovanie UDK" );
				Dictionary.Add( "RegGame", "Registrovanie hry" );
				Dictionary.Add( "CleaningUp", "Čistenie" );

				Dictionary.Add( "GQCaptionNoEULA", "Nepodarilo sa nájsť Licenčnú zmluvu koncového používateľa" );
				Dictionary.Add( "GQDescNoEULA", "Nepodarilo sa nájsť Licenčnú zmluvu koncového používateľa; inštalácia sa prerušuje." );
				Dictionary.Add( "GQCaptionInstallFail", "Inštalácia zlyhala" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "Odstrániť všetky súbory?" );
				Dictionary.Add( "GQDescUninstallAllSure", "Týmto sa odstránia všetky súbory, vrátane všetkých máp alebo aktív, ktoré ste vytvorili. Pokračovať?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "Odinštalovanie je dokončené!" );
				Dictionary.Add( "GQDescUninstallComplete", "Všetky súbory boli úspešne odstránené." );
				Dictionary.Add( "GQCaptionSubscribe", "Prihlásenie sa do zoznamu adries" );
				Dictionary.Add( "GQDescSubscribe", "Prihlásenie vašej e-mailovej adresy na odber marketingových a technických aktualizácií súpravy Unreal Development Kit." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "Redistribuovateľná inštalácia je dokončená!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "Všetky redistribuovateľné súbory podmienok boli nainštalované správne." );
				Dictionary.Add( "GQCaptionWaiting", "Čakanie na proces..." );
				Dictionary.Add( "GQDescWaiting", "Čakanie na dokončenie inštalácie súboru podmienky..." );
				Dictionary.Add( "GQCaptionDelFileFail", "Nepodarilo sa odstrániť súbor" );
				Dictionary.Add( "GQCaptionDelFolderFail", "Nepodarilo sa odstrániť priečinok" );
				Dictionary.Add( "GQCaptionCorrupt", "Poškodený balík" );
				Dictionary.Add( "GQDescCorrupt1", "Balík UDK je poškodený, prevezmite ho znovu (Nie je možné nájsť podpis)." );
				Dictionary.Add( "GQDescCorrupt2", "Balík UDK je poškodený, prevezmite ho znovu (Výnimka pri otváraní súboru zip)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Nastavte názov hry" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Nastavte polia „GameName” (Názov hry) a „GameLongName” (Dlhý názov hry) do platného reťazca." );

				Dictionary.Add( "ButAccept", "Súhlasím" );
				Dictionary.Add( "ButReject", "Odmietnuť" );
				Dictionary.Add( "ButInstall", "Inštalovať" );
				Dictionary.Add( "ButUninstall", "Odinštalovať" );

				Dictionary.Add( "EULATitle", "Licenčná zmluva koncového používateľa" );
				Dictionary.Add( "EULALegalese", "Prečítal som si softvérovú licenciu UDK, rozumiem jej a súhlasím s jej podmienkami kliknutím na tlačidlo „Súhlasím”. Za súhlas sa považuje môj podpis na konci dohody." );

				Dictionary.Add( "GBInstallLocation", "Umiestnenie inštalácie" );
				Dictionary.Add( "GBInstallLocationBrowse", "Prehľadávať..." );
				Dictionary.Add( "PrivacyStatement", "[Voliteľné] Zaregistrovať sa na zasielanie aktualizácií a noviniek UDK." );
				Dictionary.Add( "PrivacyStatement2", "Prečítajte si naše zásady používania osobných údajov: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Možnosti inštalácie: " );

				Dictionary.Add( "UpdateFileManifests", "Manifesty aktualizačných súborov" );

				Dictionary.Add( "IFInstallFinished", "Inštalácia je dokončená" );
				Dictionary.Add( "IFInstallContentFinished", "Dokončená inštalácia: " );
				Dictionary.Add( "IFLaunch", "Spustiť " );
				Dictionary.Add( "IFFinished", "Dokončené" );

				Dictionary.Add( "UIOLocation", "Umiestnenie: " );
				Dictionary.Add( "UIOOptionsGame", "Možnosti odinštalovania: " );
				Dictionary.Add( "DeletingFiles", "Odstránenie súborov" );
				Dictionary.Add( "DeletingShortcuts", "Odstránenie odkazov" );
				Dictionary.Add( "UnableDeleteFile", "Uvedený súbor nebolo možné odstrániť, pretože je s najväčšou pravdepodobnosťou otvorený v inej aplikácii." );
				Dictionary.Add( "UnableDeleteFolder", "Uvedený priečinok nebolo možné odstrániť, pretože je s najväčšou pravdepodobnosťou otvorený v inej aplikácii." );
				Dictionary.Add( "DeleteRetry", "Chcete zopakovať odstránenie?" );

				Dictionary.Add( "PBDecompressing", "Dekompresia súborov" );
				Dictionary.Add( "PBCompressing", "Kompresia súborov" );
				Dictionary.Add( "PBPackaging", "Balenie súborov" );
				Dictionary.Add( "PBDecompPrereq", "Dekompresia súborov podmienok" );
				Dictionary.Add( "PBPackagingPrereqs", "Balenie súborov podmienok" );
				Dictionary.Add( "PBSettingInitial", "Príprava pred prvým spustením" );

				Dictionary.Add( "RedistPleaseWait", "Inštalácia súborov podmienok" );
				Dictionary.Add( "RedistVCRedistHeader", "Redistribuovateľné súbory Visual Studio 2010" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "Ovládače procesora AMD (krok iba XP)" );

				Dictionary.Add( "RedistVCRedistContent", "Bezplatnú dokumentáciu a zdroje UDK nájdete na stránke http://www.unrealengine.com/udk/documentation/" );
				Dictionary.Add( "RedistDXRedistContent", "UDK vám dáva plnú moc nad Unreal Engine 3 s prístupom k tým istým špičkovým nástrojom, ktoré sa používajú na vytváranie najlepších videohier." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "VCRedist x86 sa nepodarilo nainštalovať." );
				Dictionary.Add( "RedistVCRedistx64Fail", "VCRedist x64 sa nepodarilo nainštalovať." );
				Dictionary.Add( "RedistDXRedistFail", "DirectX sa nepodarilo nainštalovať." );
				Dictionary.Add( "RedistAMDCPUFail", "Ovládače procesora AMD sa nepodarilo nainštalovať." );

				Dictionary.Add( "IFDependsFailed", "Závislosti neboli nainštalované; spustite „UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe”" );
				Dictionary.Add( "GQCaptionInvalidInstall", "Neplatné umiestnenie inštalácie" );
				Dictionary.Add( "GQDescInvalidInstall", "Skontrolujte, či je umiestnenie inštalácie platné a či je na disku dostatok miesta." );

				Dictionary.Add( "UE3RedistEULALegalese", "Prečítal som si softvérovú licenciu, rozumiem jej a súhlasím s jej podmienkami kliknutím na tlačidlo „Súhlasím”." );
				Dictionary.Add( "NetCodeWarn", "Unreal Development Kit (UDK) je súbor nástrojov pre vývoj hier. Aplikácie vytvorené pomocou UDK obsahujú kód, ktorý neschválila spoločnosť Epic Games, Inc. Podobne ako pri všetkých aplikáciách, ktoré preberáte z Internetu, tento softvér by ste mali nainštalovať, len ak dôverujete zdroju. Použitie na vlastné riziko." );

				Dictionary.Add( "ExtractFailCaption", "Inštalácia zlyhala" );
				Dictionary.Add( "ExtractFailMessage1", "Extrahovanie všetkých súborov zlyhalo s výnimkou: " );
				Dictionary.Add( "ExtractFailMessage2", "Pomoc nájdete na stránke http://udk.com/troubleshooting." );
				Dictionary.Add( "UIODeleteOnly", "Odinštalovať pôvodné a dočasné súbory (súbory, ktoré nevytvoril používateľ)." );
				Dictionary.Add( "UIODeleteAll", "Odinštalovať všetky súbory (vrátane súborov, ktoré vytvoril používateľ)." );

				Dictionary.Add( "RedistVCRedistContentTech", "Redistribuovateľný balík Microsoft Visual C++ 2010 (x86) nainštaluje súčasti typu runtime knižníc Visual C++ potrebných na spustenie aplikácií vyvinutých pomocou Visual C++ SP1 na počítači, ktorý nemá nainštalovaný Visual C++ 2010 SP1. Tento balík nainštaluje súčasti typu runtime knižníc C Runtime (CRT), Standard C++, ATL, MFC, OpenMP a MSDIA." );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX je skupina technológií navrhnutá tak, aby počítače so systémom Windows boli ideálnou platformou pre spustenie a zobrazenie aplikácií bohatých na multimediálne prvky, ako napr. plnofarebná grafika, video, 3D animácia, a sýty zvuk." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Umožňuje systému automaticky upravovať rýchlosť, napätie a kombináciu výkonu procesora, ktoré zodpovedajú okamžitej potrebe používateľa. Tento ovládač podporuje procesory AMD Athlon 64 X2 Dual Core v systémoch Windows XP SP2, Windows 2003 SP1 x86 a x64 Edition." );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "E-mailová adresa" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "エラーメッセージ：" );
				Dictionary.Add( "UDKTrouble", "可能な解決策へのリンク：" );
				Dictionary.Add( "RegUDK", "UDK に登録中" );
				Dictionary.Add( "RegGame", "ゲームを登録中" );
				Dictionary.Add( "CleaningUp", "クリーンアップ中" );

				Dictionary.Add( "GQCaptionNoEULA", "EULAの検出に失敗しました" );
				Dictionary.Add( "GQDescNoEULA", "EULAの検出に失敗しました。インストールを中止します。" );
				Dictionary.Add( "GQCaptionInstallFail", "インストールに失敗しました" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "すべてのファイルを削除しますか？" );
				Dictionary.Add( "GQDescUninstallAllSure", "マップや作成したアセットすべてを削除します。操作を続行しますか?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "アンインストールに失敗しました！" );
				Dictionary.Add( "GQDescUninstallComplete", "すべてのファイルの削除に成功しました。" );
				Dictionary.Add( "GQCaptionSubscribe", "メーリングリストを購読します" );
				Dictionary.Add( "GQDescSubscribe", "Unreal Development Kitの販売や技術的なアップデートを受信するためメールアドレスを登録します。" );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "Redistributable のインストールを完了しました！" );
				Dictionary.Add( "GQDescRedistInstallComplete", "すべてのredistributable に最低限必要なものが正常にインストールされました。" );
				Dictionary.Add( "GQCaptionWaiting", "処理中です…" );
				Dictionary.Add( "GQDescWaiting", "必須インストールの完了を待っています…" );
				Dictionary.Add( "GQCaptionDelFileFail", "ファイルの削除に失敗しました" );
				Dictionary.Add( "GQCaptionDelFolderFail", "フォルダの削除に失敗しました" );
				Dictionary.Add( "GQCaptionCorrupt", "破損パッケージ" );
				Dictionary.Add( "GQDescCorrupt1", "UDK パッケージが破損しています。もう一度パッケージをダウンロードしてください(署名が見つかりませんでした)。" );
				Dictionary.Add( "GQDescCorrupt2", "UDK パッケージが破損しています。もう一度パッケージをダウンロードしてください(zip 形式ファイルを開く時を除く)。" );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Game Nameを設定してください" );
				Dictionary.Add( "GQDescPleaseSetGameName", "'GameName' および'GameLongName' フィールドを有効な文字列に設定してください。." );

				Dictionary.Add( "ButAccept", "同意する" );
				Dictionary.Add( "ButReject", "同意しない" );
				Dictionary.Add( "ButInstall", "インストール" );
				Dictionary.Add( "ButUninstall", "アンインストール" );

				Dictionary.Add( "EULATitle", "エンドユーザーライセンス使用承諾契約" );
				Dictionary.Add( "EULALegalese", "私は「同意する」ボタンをクリックすることでUDK ソフトウェアライセンシー使用承諾契約に同意します。また、すべての本契約の条項に同意したとみなされるものとします。" );

				Dictionary.Add( "GBInstallLocation", "インストールロケーション" );
				Dictionary.Add( "GBInstallLocationBrowse", "検索…" );
				Dictionary.Add( "PrivacyStatement", "[任意]アップデートおよびニュースへのサインアップ。" );
				Dictionary.Add( "PrivacyStatement2", "プライバシーポリシーを読む： http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "インストールオプション： " );

				Dictionary.Add( "UpdateFileManifests", "アップデートファイルマニフェスト" );

				Dictionary.Add( "IFInstallFinished", "インストール完了" );
				Dictionary.Add( "IFInstallContentFinished", "完了したインストール： " );
				Dictionary.Add( "IFLaunch", "起動 " );
				Dictionary.Add( "IFFinished", "完了" );

				Dictionary.Add( "UIOLocation", "ロケーション： " );
				Dictionary.Add( "UIOOptionsGame", "アンインストールオプション： " );
				Dictionary.Add( "DeletingFiles", "ファイルを削除中" );
				Dictionary.Add( "DeletingShortcuts", "ショートカットを削除中" );
				Dictionary.Add( "UnableDeleteFile", "他のアプリケーションで開かれているため、上記のファイルは削除することができませんでした。" );
				Dictionary.Add( "UnableDeleteFolder", "他のアプリケーションで開かれているため、上記のフォルダは削除することができませんでした。" );
				Dictionary.Add( "DeleteRetry", "もう一度削除し直しますか？" );

				Dictionary.Add( "PBDecompressing", "ファイルを解凍中" );
				Dictionary.Add( "PBCompressing", "ファイルを圧縮中" );
				Dictionary.Add( "PBPackaging", "ファイルをパッキング中" );
				Dictionary.Add( "PBDecompPrereq", "必須ファイルを解凍中" );
				Dictionary.Add( "PBPackagingPrereqs", "必須ファイルをパッキング中" );
				Dictionary.Add( "PBSettingInitial", "初期起動の準備をしています..." );

				Dictionary.Add( "RedistPleaseWait", "必須要件をインストール中" );

				Dictionary.Add( "RedistVCRedistHeader", "Visual Studio 2010 Redistributables" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "AMD CPU ドライバ" );

				Dictionary.Add( "RedistVCRedistContent", "「無料のUDKサポートドキュメンテーションおよびリソースへのアクセスは www.udk.com/documentation をご覧ください」" );
				Dictionary.Add( "RedistDXRedistContent", "「UDKは、blockbuster ビデオゲーム製作に使用されたワールドクラスのツールへのアクセスがついて、Unreal Engine 3 をさらにパワーアップさせてくれます。」" );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "VCRedist x86 はインストールに失敗しました。" );
				Dictionary.Add( "RedistVCRedistx64Fail", "VCRedist x64 はインストールに失敗しました。" );
				Dictionary.Add( "RedistDXRedistFail", "DirectX はインストールに失敗しました。" );
				Dictionary.Add( "RedistAMDCPUFail", "AMD CPU ドライバはインストールに失敗しました。" );

				Dictionary.Add( "IFDependsFailed", "依存性が欠如しています。'UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe'を実行してください。" );
				Dictionary.Add( "GQCaptionInvalidInstall", "無効なインストール位置" );
				Dictionary.Add( "GQDescInvalidInstall", "インストール位置が有効で、ディスクに十分な容量があるかを確認してください。" );

				Dictionary.Add( "UE3RedistEULALegalese", "私は「同意する」ボタンをクリックすることでUDK ソフトウェアライセンシー使用承諾契約に同意します" );
				Dictionary.Add( "NetCodeWarn", "アプリケーションは、Epic Games, Incにより検証されているコードを含んだUDKで書かれています。インターネット上でダウンロードするアプリケーションと同様に、ソースが信頼できる場合にのみ、このソフトウェアをインストールしてください。お客様各自の責任においてご利用ください。" );

				Dictionary.Add( "ExtractFailCaption", "インストールに失敗しました" );
				Dictionary.Add( "ExtractFailMessage1", "例外で失敗したすべてのファイルを抽出しています: " );
				Dictionary.Add( "ExtractFailMessage2", "ヘルプが必要な場合は http://udk.com/troubleshooting をご覧ください。" );
				Dictionary.Add( "UIODeleteOnly", "オリジナルおよびテンポラリファイル(ユーザーにより作成されたものでない)をアンインストールする。" );
				Dictionary.Add( "UIODeleteAll", "すべてのファイルをアンインストールする(ユーザーにより作成されたファイルを含む)。" );

				Dictionary.Add( "RedistVCRedistContentTech", "Microsoft Visual C++ 2010 Redistributable Package (x86) は、Visual C++ 2010 がインストールされていないPC上にて、Visual C++ SP1 で開発されたアプリケーションを実行するため要求されたVisual C++ ライブラリのランタイムコンポーネントをインストールします。このパッケージは、C Runtime (CRT)、 Standard C++、, ATL, MFC、 OpenMP および MSDIA ライブラリのランタイムコンポーネントをインストールします。" );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX は、フルカラーグラフィックス、ビデオ、3D アニメーションおよびオーディオなどのマルチメディア要素を豊富に搭載したアプリケーションの起動や表示をするため、Windowsベースのコンピューターを理想的なプラットフォームにすることを目的にデザインされたテクノロジーの一群です。" );
				Dictionary.Add( "RedistAMDCPUContentTech", "これは、システムがCPU スピード、ボルテージおよびユーザーが必要としているパフォーマンスを即座に一致させるパワーの組み合わせを自動的に調節するようにします。このドライバは、Windows XP SP2、 Windows 2003 SP1 x86 およびx64 版のAMD Athlon デュアルコア・プロセッサをサポートしています。" );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "メールアドレス" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
				break;

			case "KOR":
				Dictionary.Add( "R2D", "바탕화면으로 돌아가기" );

				Dictionary.Add( "GQOK", "확인" );
				Dictionary.Add( "GQCancel", "취소" );
				Dictionary.Add( "GQRetry", "재시도" );
				Dictionary.Add( "GQIgnore", "무시" );
				Dictionary.Add( "GQYes", "예" );
				Dictionary.Add( "GQNo", "아뇨" );
				Dictionary.Add( "GQSkip", "생략" );

				Dictionary.Add( "ErrorMessage", "에러 메시지:" );
				Dictionary.Add( "UDKTrouble", "가능한 해결책은 다음을 확인해 주세요:" );
				Dictionary.Add( "RegUDK", "UDK 등록" );
				Dictionary.Add( "RegGame", "게임 등록" );
				Dictionary.Add( "CleaningUp", "정리" );

				Dictionary.Add( "GQCaptionNoEULA", "최종 사용자 사용권 계약서를 찾을 수 없습니다." );
				Dictionary.Add( "GQDescNoEULA", "최종 사용자 사용권 계약서를 찾을 수 없습니다. 설치를 취소합니다." );
				Dictionary.Add( "GQCaptionInstallFail", "설치에 실패했습니다." );
				Dictionary.Add( "GQCaptionUninstallAllSure", "모든 파일을 지울까요?" );
				Dictionary.Add( "GQDescUninstallAllSure", "만드신 모든 맵과 애셋을 포함해서 모든 파일이 지워집니다. 계속할까요?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "설치 해제가 완료되었습니다!" );
				Dictionary.Add( "GQDescUninstallComplete", "모든 파일이 성공적으로 삭제되었습니다." );
				Dictionary.Add( "GQCaptionSubscribe", "메일링 리스트 구독" );
				Dictionary.Add( "GQDescSubscribe", "UDK 관련 홍보, 기술 업데이트 자료를 구독 신청하신 이메일로 받아봅니다." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "재배포 가능 패키지의 설치가 완료되었습니다!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "모든 재배포 가능 필수 구성 요소가 올바르게 설치되었습니다." );
				Dictionary.Add( "GQCaptionWaiting", "처리 중..." );
				Dictionary.Add( "GQDescWaiting", "필수 구성 요소 설치 중..." );
				Dictionary.Add( "GQCaptionDelFileFail", "파일 삭제 실패" );
				Dictionary.Add( "GQCaptionDelFolderFail", "폴더 삭제 실패" );
				Dictionary.Add( "GQCaptionCorrupt", "손상된 패키지" );
				Dictionary.Add( "GQDescCorrupt1", "UDK 패키지가 손상되었습니다. 다시 내려받아 주세요. (시그너처를 찾을 수 없습니다)." );
				Dictionary.Add( "GQDescCorrupt2", "UDK 패키지가 손상되었습니다. 다시 내려받아 주세요. (zip 파일을 여는 도중 예외가 발생했습니다.)" );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Game Name 을 설정해 주세요." );
				Dictionary.Add( "GQDescPleaseSetGameName", "'GameName', 'GameLongName' 필드에 유효한 문자열을 설정해 주세요." );

				Dictionary.Add( "ButAccept", "동의" );
				Dictionary.Add( "ButReject", "거부" );
				Dictionary.Add( "ButInstall", "설치" );
				Dictionary.Add( "ButUninstall", "설치 해제" );

				Dictionary.Add( "EULATitle", "최종 사용자 사용권 계약서 (EULA)" );
				Dictionary.Add( "EULALegalese", "본인은 UDK 소프트웨어 사용권 계약서를 읽고 이해하였으며, '동의' 버튼을 누름으로써 계약서의 모든 내용에 동의합니다. 이는 계약서 말미의 서명으로 간주됩니다." );

				Dictionary.Add( "GBInstallLocation", "설치 위치" );
				Dictionary.Add( "GBInstallLocationBrowse", "탐색..." );
				Dictionary.Add( "GBInstallLocationBrowseDest", "UDK 를 어디에 설치할까요?" );
				Dictionary.Add( "PrivacyStatement", "[선택 사항] UDK 업데이트 뉴스 구독" );
				Dictionary.Add( "PrivacyStatement2", "개인정보 취급방침: http://epicgames.com/privacynotice/ko" );
				Dictionary.Add( "IOInstallOptionsGame", "설치 옵션: " );

				Dictionary.Add( "UpdateFileManifests", "파일 명세서 업데이트" );

				Dictionary.Add( "IFInstallFinished", "설치가 완료되었습니다." );
				Dictionary.Add( "IFInstallContentFinished", "설치 완료: " );
				Dictionary.Add( "IFLaunch", "실행 " );
				Dictionary.Add( "IFFinished", "완료" );
				Dictionary.Add( "IFBack", "뒤로" );
				Dictionary.Add( "IFPerforcePopulate", "게임 파일을 퍼포스 서버에 복사합니다.");
				Dictionary.Add( "IFP4Description", "UDK 에디터에 소스 콘트롤을 활용하려면 먼저 환경설정을 해 줘야 합니다. 퍼포스 서버와 클라이언트 웍스페이스 환경설정 이후 에디터 하단 상태바의 링크 아이콘을 클릭하면 소스 콘트롤 기능이 활성화되어 서버에 접속됩니다!");

				Dictionary.Add( "IEInstallExtras", "추가 설치" );
				Dictionary.Add( "IENext", "다음" );
				Dictionary.Add( "IEServerInstall", "서버 설치" );
				Dictionary.Add( "IEServerInstallDesc", "[옵션] 퍼포스 서버를 설치합니다." );
				Dictionary.Add( "IEServerInstallDescLink", "자세한 설명(영문): http://www.perforce.com/product/components/perforce_server" );
				Dictionary.Add( "IEClientInstall", "클라이언트 설치" );
				Dictionary.Add( "IEClientInstallDesc", "[옵션] 퍼포스 비주얼 클라이언트를 설치합니다." );
				Dictionary.Add( "IEClientInstallDescLink", "자세한 설명(영문): http://www.perforce.com/product/components/perforce_visual_client" );
				Dictionary.Add( "IEMissingInstaller", "인스톨러를 찾을 수 없습니다: ");
				Dictionary.Add( "IEPerforceDescription", "UDK 는 퍼포스 버전 콘트롤 시스템을 자체적으로 지원하고 있어, 팀원간의 협업이나 프로젝트의 여러 버전 관리, 작업 내용 백업에 도움이 됩니다." );
				Dictionary.Add( "IEP4ClientUpToDateMessage", "퍼포스 비주얼 클라이언트(P4V) 최신 버전이 이미 설치되어 있습니다.\n\n그래도 인스톨러를 실행할까요?" );
				Dictionary.Add( "IEP4ClientUpToDateCaption", "P4V 이미 설치됨" );
				Dictionary.Add( "IEP4ServerUpToDateLocalMessage", "퍼포스 서버(P4D) 최신 버전이 이미 설치되어 있습니다.\n\n그래도 인스톨러를 실행할까요?" );
				Dictionary.Add( "IEP4ServerUpToDateRemoteMessage", "원격 머신의 최신 퍼포스 서버에 이미 무언가 접속되어 있습니다.\n\n그래도 서버 인스톨러를 실행할까요?");
				Dictionary.Add( "IEp4ServerUpToDateCaption", "P4D 이미 설치됨" );
				Dictionary.Add( "IEP4ServerOutOfDateLocalMessage", "구버전 퍼포스 서버가 설치되어 있습니다. 서버 인스톨러로 업그레이드를 할 수는 있지만 그 전에 퍼포스 시스템 관리자 안내서 (http://www.perforce.com/perforce/doc.current/manuals/p4sag/01_install.html) 에서 그에 관련된 모든 정보를 읽어보시는 편이 좋습니다. \n\n지금 서버 인스톨러를 실행하시겠습니까?" );
				Dictionary.Add( "IEP4ServerOutOfDateRemoteMessage", "원격 머신의 구버전 퍼포스 서버에 무언가 접속되어 있습니다. 이 서버는 로컬 머신에서 업그레이드할 수 없습니다. 기존 서버 업그레이드 관련 정보는 퍼포스 시스템 관리자 안내서 (http://www.perforce.com/perforce/doc.current/manuals/p4sag/01_install.html) 를 참고해 보시기 바랍니다. \n\n지금 서버 인스톨러를 실행할까요?" );
				Dictionary.Add( "IEP4ServerOutOfDateCaption", "퍼포스 서버 업그레이드" );
				Dictionary.Add( "IEP4ServerLocalConfirmMessage", "퍼포스 서버(P4D) 인스톨러를 실행하면 퍼포스 서버를 로컬에서 설치할 수 있습니다. 그 전에 퍼포스 시스템 관리자 안내서 (http://www.perforce.com/perforce/doc.current/manuals/p4sag/01_install.html) 를 읽어보시기 바랍니다. \n\n로컬 머신에 퍼포스 서버를 설치할까요?" );
				Dictionary.Add( "IEP4ServerLocalConfirmCaption", "퍼포스 서버 설치" );

                Dictionary.Add("PSProjectSelect", "프로젝트 선택");
                Dictionary.Add("PSGameUT3", "프로젝트 개발 시작점으로 삼아볼 수 있는 언리얼 토너먼트 콘텐츠와 게임 코드 샘플을 포함해서 UDK 를 설치합니다.");
                Dictionary.Add("PSGameEmpty", "빈 프로젝트로 UDK 를 설치합니다.");
                Dictionary.Add("PSGameUT3Title", "UT 샘플 게임");
                Dictionary.Add("PSGameEmptyTitle", "빈 게임");

				Dictionary.Add( "UIOLocation", "위치: " );
				Dictionary.Add( "UIOOptionsGame", "설치 해제 옵션: " );
				Dictionary.Add( "DeletingFiles", "파일 삭제 중" );
				Dictionary.Add( "DeletingShortcuts", "바로가기 삭제 중" );
				Dictionary.Add( "UnableDeleteFile", "위의 파일을 삭제할 수 없습니다. 아마도 다른 어플리케이션에서 열려있는 것 같습니다." );
				Dictionary.Add( "UnableDeleteFolder", "위의 폴더를 삭제할 수 없습니다. 아마도 다른 어플리케이션에서 열려있는 것 같습니다." );
				Dictionary.Add( "DeleteRetry", "다시 삭제해 볼까요?" );

				Dictionary.Add( "PBDecompressing", "파일 압축해제 중" );
				Dictionary.Add( "PBCompressing", "파일 압축 중" );
				Dictionary.Add( "PBPackaging", "파일 묶는 중" );
				Dictionary.Add( "PBDecompPrereq", "필수 파일 압축해제 중" );
				Dictionary.Add( "PBPackagingPrereqs", "필수 파일 묶는 중" );
				Dictionary.Add( "PBSettingInitial", "첫 실행 준비중..." );
				Dictionary.Add( "PBCreatingBackup", "백업 파일 복사중");

				Dictionary.Add( "RedistPleaseWait", "필수 파일 설치중" );
				Dictionary.Add( "RedistVCRedistHeader", "비주얼 스튜디오 2010 재배포파일" );
				Dictionary.Add( "RedistDXRedistHeader", "마이크로소프트 DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "AMD CPU 드라이버 (XP 만)" );

				Dictionary.Add( "RedistVCRedistContent", "UDK 기술지원 문서와 자료를 무료로 열람할 수 있습니다. http://www.unrealengine.com/udk/documentation/" );
				Dictionary.Add( "RedistDXRedistContent", "블록버스터 게임 제작에 사용된 언리얼 엔진 3, 그 세계 정상급 툴 그대로를 UDK 를 통해 접해볼 수 있습니다." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "VCRedist x86 설치에 실패했습니다." );
				Dictionary.Add( "RedistVCRedistx64Fail", "VCRedist x64 설치에 실패했습니다." );
				Dictionary.Add( "RedistDXRedistFail", "DirectX 설치에 실패했습니다." );
				Dictionary.Add( "RedistAMDCPUFail", "AMD CPU 드라이버 설치에 실패했습니다." );

				Dictionary.Add( "IFDependsFailed", "의존성 파일이 설치되지 않았습니다. 'UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe' 를 실행하세요." );
				Dictionary.Add( "GQCaptionInvalidInstall", "잘못된 설치 위치입니다." );
				Dictionary.Add( "GQDescInvalidInstall", "설치 위치가 제대로 되었는지, 디스크에 공간은 충분한지 확인해 주세요." );

				Dictionary.Add( "UE3RedistEULALegalese", "'동의' 버튼을 누르므로써 소프트웨어 라이선스를 읽고 이해하여 동의하였음을 인정합니다." );
				Dictionary.Add( "NetCodeWarn", "UDK 는 게임 개발 툴셋입니다. UDK 로 저작된 어플리케이션은 인터넷을 통해 내려받은 다른 어플리케이션과 마찬가지로 에픽 게임스의 검열을 받지 않은 코드가 들어있을 수 있으니, 믿을만한 출처의 소프트웨어인 경우에만 설치하시기 바랍니다." );

				Dictionary.Add( "ExtractFailCaption", "설치 실패" );
				Dictionary.Add( "ExtractFailMessage1", "모든 파일 압축해제 중 다음 예외가 발생했습니다: " );
				Dictionary.Add( "ExtractFailMessage2", "UDK Korea 포럼에서 다른 사용자의 도움을 받을 수도 있습니다. http://forums.epicgames.com/forums/406-UDK-Korea " );
				Dictionary.Add( "UIODeleteOnly", "원본 및 임시 (사용자가 만들지 않은) 파일을 설치해제합니다." );
				Dictionary.Add( "UIODeleteAll", "모든 (사용자가 만든 파일을 포함한) 파일을 설치해제합니다." );

				Dictionary.Add( "RedistVCRedistContentTech", "Microsoft Visual C++ 2010 Redistributable Package (x86) 은 Visual C++ 2010 이 설치되지 않은 컴퓨터에서 Visual C++ SP1 으로 개발된 어플리케이션을 실행하는 데 필요한 Visual C++ 라이브러리 런타임 컴포넌트를 설치합니다. 이 패키지가 설치하는 런타임 컴포넌트는 C Runtime (CRT), 표준 C++, ATL, MFC, OpenMP, MSDIA 라이브러리 입니다." );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX 는 윈도우 기반 컴퓨터를 원색 그래픽, 비디오, 3D 애니메이션, 오디오 등 다채로운 멀티미디어 어플리케이션 실행을 위한 이상적 플랫폼으로 만들기 위해 디자인된 기술의 집합체입니다." );
				Dictionary.Add( "RedistAMDCPUContentTech", "사용자의 즉각적인 퍼포먼스 요구에 따라 시스템에서 CPU 속도, 전압, 전력 조합을 자동 조절할 수 있도록 합니다. 이 드라이버는 윈도우 XP SP2, 윈도우 2003 SP1 x86 & x64 버전에서 AMD 애슬론 64 X2 듀얼 코어 프로세서를 지원합니다." );

				Dictionary.Add( "GQAddingShortcuts", "바로가기 추가중" );
				Dictionary.Add( "LabEmail", "이메일 주소" );
				Dictionary.Add( "IOInvalidEmail", "   잘못된 이메일 주소입니다." );
				Dictionary.Add( "IOInvalidProjectName", "   잘못된 프로젝트 이름입니다." );
				Dictionary.Add( "IOProjectOptions", "프로젝트 옵션" );
				Dictionary.Add( "IOProjectNameLabel", "프로젝트 이름" );
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

				Dictionary.Add( "ErrorMessage", "错误信息" );
				Dictionary.Add( "UDKTrouble", "要获得可能的解决方案,请到:" );
				Dictionary.Add( "RegUDK", "注册UDK" );
				Dictionary.Add( "RegGame", "注册游戏" );
				Dictionary.Add( "CleaningUp", "清除" );

				Dictionary.Add( "GQCaptionNoEULA", "查找EULA 失败" );
				Dictionary.Add( "GQDescNoEULA", "查找EULA失败;放弃安装." );
				Dictionary.Add( "GQCaptionInstallFail", "安装失败" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "删除所有文件?" );
				Dictionary.Add( "GQDescUninstallAllSure", "这将删除所有文件,包括你已经创建的任何地图和资源.继续?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "卸载完成!" );
				Dictionary.Add( "GQDescUninstallComplete", "成功删除所有文件." );
				Dictionary.Add( "GQCaptionSubscribe", "订阅邮件列表" );
				Dictionary.Add( "GQDescSubscribe", "提交你的电子邮件地址来接受关于虚幻引擎开发工具包的市场和技术更新." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "可再发行的安装完成." );
				Dictionary.Add( "GQDescRedistInstallComplete", "所有的可再发布的先决条件已经正确地安装." );
				Dictionary.Add( "GQCaptionWaiting", "等待处理…" );
				Dictionary.Add( "GQDescWaiting", "等待先决条件安装完成…" );
				Dictionary.Add( "GQCaptionDelFileFail", "删除文件失败" );
				Dictionary.Add( "GQCaptionDelFolderFail", "删除文件夹失败" );
				Dictionary.Add( "GQCaptionCorrupt", "破损的包" );
				Dictionary.Add( "GQDescCorrupt1", "UDK包破损,请重新下载(不能找到签名)." );
				Dictionary.Add( "GQDescCorrupt2", "UDK包破损,请重新下载(在解压时出现异常)" );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "请设置游戏名称" );
				Dictionary.Add( "GQDescPleaseSetGameName", "请将'GameName'和'GameLongName' 设置为有效的字符串." );

				Dictionary.Add( "ButAccept", "我接受" );
				Dictionary.Add( "ButReject", "拒绝" );
				Dictionary.Add( "ButInstall", "安装" );
				Dictionary.Add( "ButUninstall", "卸载" );

				Dictionary.Add( "EULATitle", "最终用户授权协议" );
				Dictionary.Add( "EULALegalese", "我已经阅读了UDK软件的授权文件、理解该文件并来通过点击'我接受'按钮来接受的所有条款.我的接受应该被认为是我在协议的末尾部分的签名." );

				Dictionary.Add( "GBInstallLocation", "安装位置" );
				Dictionary.Add( "GBInstallLocationBrowse", "浏览" );
				Dictionary.Add( "PrivacyStatement", "[可选的]注册来获得UDK的更新和消息" );
				Dictionary.Add( "PrivacyStatement2", "阅读我们的隐私声明: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "安装选项:" );

				Dictionary.Add( "UpdateFileManifests", "更新文件说明" );

				Dictionary.Add( "IFInstallFinished", "安装完成" );
				Dictionary.Add( "IFInstallContentFinished", "完成安装:" );
				Dictionary.Add( "IFLaunch", "启动" );
				Dictionary.Add( "IFFinished", "完成" );

				Dictionary.Add( "UIOLocation", "位置:" );
				Dictionary.Add( "UIOOptionsGame", "卸载选项:" );
				Dictionary.Add( "DeletingFiles", "删除文件" );
				Dictionary.Add( "DeletingShortcuts", "删除快捷方式" );
				Dictionary.Add( "UnableDeleteFile", "以上文件不能删除,最可能是因为它在被另一个应用程序打开." );
				Dictionary.Add( "UnableDeleteFolder", "以上文件夹不能删除,最大可能是因为它在被另一个应用程序打开." );
				Dictionary.Add( "DeleteRetry", "您想再次尝试删除吗?" );

				Dictionary.Add( "PBDecompressing", "解压文件" );
				Dictionary.Add( "PBCompressing", "压缩文件" );
				Dictionary.Add( "PBPackaging", "打包文件" );
				Dictionary.Add( "PBDecompPrereq", "解压先决条件文件" );
				Dictionary.Add( "PBPackagingPrereqs", "打包先决条件文件" );
				Dictionary.Add( "PBSettingInitial", "准备首次运行..." );

				Dictionary.Add( "RedistPleaseWait", "安装先决条件文件" );

				Dictionary.Add( "RedistVCRedistHeader", "Visual Studio 2010 Redistributables" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "AMD CPU 驱动程序" );

				Dictionary.Add( "RedistVCRedistContent", "请在http://www.unrealengine.com/udk/documentation/中访问关于UDK的免费的支持文档和资源。" );
				Dictionary.Add( "RedistDXRedistContent", "UDK为您提供了虚幻引擎3的所有强大功能及用于制作惊人的视频游戏的世界级工具。" );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "VCRedist x86安装失败." );
				Dictionary.Add( "RedistVCRedistx64Fail", "VCRedist x64安装失败." );
				Dictionary.Add( "RedistDXRedistFail", "DirectX安装失败." );
				Dictionary.Add( "RedistAMDCPUFail", "AMD CPU驱动程序安装失败." );

				Dictionary.Add( "IFDependsFailed", "没有安装依赖软件；请运行'UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe'。" );
				Dictionary.Add( "GQCaptionInvalidInstall", "无效的安装位置。" );
				Dictionary.Add( "GQDescInvalidInstall", "请检查安装位置是否有效，磁盘上没有足够的空间。" );

				Dictionary.Add( "UE3RedistEULALegalese", "我已经阅读了软件授权条款、理解了条款内容并愿意通过点击'我接受'按钮来接受这些条款。" );
				Dictionary.Add( "NetCodeWarn", "虚幻开发工具包(UDK)是一个游戏开发工具包。使用UDK创作的应用程序包含着没有经过Epic Games, Inc审查的代码。像您从网络上下载的其它任何应用程序一样，如果您信任这个源代码您便可以安装这个软件。您在使用时需要自行承担风险。" );

				Dictionary.Add( "ExtractFailCaption", "安装失败" );
				Dictionary.Add( "ExtractFailMessage1", "提取所有文件失败，出现以下异常： " );
				Dictionary.Add( "ExtractFailMessage2", "请访问以下网站来获得帮助：http://udk.com/troubleshooting 。" );
				Dictionary.Add( "UIODeleteOnly", "卸载原始文件和临时文件(不包括用户创建的文件)。" );
				Dictionary.Add( "UIODeleteAll", "卸载所有文件(包括用户创建的文件)。" );

				Dictionary.Add( "RedistVCRedistContentTech", "Microsoft Visual C++ 2010 Redistributable Package (x86)安装了当您在没有安装Visual C++ 2010 的计算机中运行使用Visual C++ SP1开发的程序时所需要的Visual C++ Libraries的运行时系统组件。" );
				Dictionary.Add( "RedistDXRedistContentTech", "微软DirectX是一组技术,旨在使基于Windows的计算机成为一个可运行和显示丰富多媒体元素的应用程序(例如全彩图像，视频，三维动画，以及丰富的音频)的理想平台。" );
				Dictionary.Add( "RedistAMDCPUContentTech", "允许系统自动调整CPU速度以及电压和功率的结合物来即时地满足用户的性能需要。这个驱动程序支持在Windows XP SP2上的AMD Athlon 64 X2 Dual Core 处理器、Windows 2003 SP1 x86 及x64 版本。" );

				Dictionary.Add( "GQAddingShortcuts", "" );
				Dictionary.Add( "LabEmail", "邮件地址" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
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

				Dictionary.Add( "ErrorMessage", "Error Message: " );
				Dictionary.Add( "UDKTrouble", "For possible solutions go to: " );
				Dictionary.Add( "RegUDK", "Registering UDK" );
				Dictionary.Add( "RegGame", "Registering Game" );
				Dictionary.Add( "CleaningUp", "Cleaning up" );

				Dictionary.Add( "GQCaptionNoEULA", "Failed to Find EULA" );
				Dictionary.Add( "GQDescNoEULA", "Failed to find EULA; aborting install." );
				Dictionary.Add( "GQCaptionInstallFail", "Installation Failed" );
				Dictionary.Add( "GQCaptionUninstallAllSure", "Delete All Files?" );
				Dictionary.Add( "GQDescUninstallAllSure", "This will delete all files, including any maps or assets that you have created. Continue?" );

				Dictionary.Add( "GQCaptionUninstallComplete", "Uninstallation Complete!" );
				Dictionary.Add( "GQDescUninstallComplete", "All requested files deleted successfully." );
				Dictionary.Add( "GQCaptionSubscribe", "Subscribing to Mailing List" );
				Dictionary.Add( "GQDescSubscribe", "Subscribing your email address to receive marketing and technical updates for the Unreal Development Kit." );
				Dictionary.Add( "GQCaptionRedistInstallComplete", "Redistributable Installation Complete!" );
				Dictionary.Add( "GQDescRedistInstallComplete", "All redistributable prerequisites have been installed correctly." );
				Dictionary.Add( "GQCaptionWaiting", "Waiting for Process..." );
				Dictionary.Add( "GQDescWaiting", "Waiting for a prerequisite install to complete..." );
				Dictionary.Add( "GQCaptionDelFileFail", "Failed to Delete File" );
				Dictionary.Add( "GQCaptionDelFolderFail", "Failed to Delete Folder" );
				Dictionary.Add( "GQCaptionCorrupt", "Corrupt Package" );
				Dictionary.Add( "GQDescCorrupt1", "The UDK package is corrupt, please redownload (Could not find signature)." );
				Dictionary.Add( "GQDescCorrupt2", "The UDK package is corrupt, please redownload (Exception when opening zip)." );
				Dictionary.Add( "GQCaptionPleaseSetGameName", "Please Set Game Name" );
				Dictionary.Add( "GQDescPleaseSetGameName", "Please set the 'GameName' and 'GameLongName' fields to valid strings." );
                Dictionary.Add( "GQCaptioSubmitEmailFailed", "Subscribing to Mailing List Unsuccessful");
                Dictionary.Add( "GQDescSubmitEmailFailed", "We were not able to subscribe your email address to the mailing list at this time.  This will not affect your install process which will continue as normal.");

				Dictionary.Add( "ButAccept", "I Accept" );
				Dictionary.Add( "ButReject", "Reject" );
				Dictionary.Add( "ButInstall", "Install" );
				Dictionary.Add( "ButUninstall", "Uninstall" );

				Dictionary.Add( "EULATitle", "End User License Agreement" );
				Dictionary.Add( "EULALegalese", "I have read the UDK software license, understand it, and agree to its terms by clicking the 'I Accept' button. My acceptance should be deemed to be my signature at the end of the agreement." );

				Dictionary.Add( "GBInstallLocation", "Install Location" );
				Dictionary.Add( "GBInstallLocationBrowse", "Browse..." );
				Dictionary.Add( "GBInstallLocationBrowseDest", "Where do you wish to install the Unreal Development Kit?" );
				Dictionary.Add( "PrivacyStatement", "[Optional] Sign up for UDK updates and news." );
				Dictionary.Add( "PrivacyStatement2", "Read our Privacy Policy: http://epicgames.com/privacynotice" );
				Dictionary.Add( "IOInstallOptionsGame", "Install Options: " );

				Dictionary.Add( "UpdateFileManifests", "Update File Manifests" );

				Dictionary.Add( "IFInstallFinished", "Installation Complete" );
				Dictionary.Add( "IFInstallContentFinished", "Finished installing: " );
				Dictionary.Add( "IFLaunch", "Launch " );
				Dictionary.Add( "IFFinished", "Finished" );
				Dictionary.Add( "IFBack", "Back" );
				Dictionary.Add( "IFPerforcePopulate", "Copy game files to Perforce server");
				Dictionary.Add( "IFP4Description", "Source control will need to be configured before it can be used with the UDK Editor.  After configuring your Perforce server and client workspace, click the link icon in the editor's lower status bar area to enable source control features and connect to the server!");

				Dictionary.Add( "IEInstallExtras", "Install Extras" );
				Dictionary.Add( "IENext", "Next" );
				Dictionary.Add( "IEServerInstall", "Install Server" );
				Dictionary.Add( "IEServerInstallDesc", "[Optional] Install the Perforce Server." );
				Dictionary.Add( "IEServerInstallDescLink", "Learn more: http://www.perforce.com/product/components/perforce_server" );
				Dictionary.Add( "IEClientInstall", "Install Client" );
				Dictionary.Add( "IEClientInstallDesc", "[Optional] Install the Perforce Visual Client." );
				Dictionary.Add( "IEClientInstallDescLink", "Learn more: http://www.perforce.com/product/components/perforce_visual_client" );
				Dictionary.Add( "IEMissingInstaller", "Could not find installer: ");
				Dictionary.Add( "IEPerforceDescription", "UDK has built-in support for the Perforce revision control system, which helps you collaborate with team members, manage multiple versions of your project, and helps you back-up your work." );
				Dictionary.Add( "IEP4ClientUpToDateMessage", "The Perforce Visual Client(P4V) is already installed and up to date.\n\nDo you wish to launch the installer anyway?" );
				Dictionary.Add( "IEP4ClientUpToDateCaption", "P4V Already Installed" );
				Dictionary.Add( "IEP4ServerUpToDateLocalMessage", "The Perforce Server(P4D) is already installed and up to date.\n\nDo you wish to launch the installer anyway?" );
				Dictionary.Add( "IEP4ServerUpToDateRemoteMessage", "An existing connection to an up to date Perforce server has been detected on a remote machine.\n\nWould you like to run the server installer anyway?");
				Dictionary.Add( "IEp4ServerUpToDateCaption", "P4D Already Installed" );
				Dictionary.Add( "IEP4ServerOutOfDateLocalMessage", "An older version of the Perforce server is installed.  The server installer will allow you to upgrade but we recommend you read all information pertaining to the process in the Perforce System Administrator's Guide before attempting an upgrade of the existing installation.  The guide can be found here: http://www.perforce.com/perforce/doc.current/manuals/p4sag/01_install.html \n\nWould you like to run the server installer now?" );
				Dictionary.Add( "IEP4ServerOutOfDateRemoteMessage", "An existing connection to an older Perforce server has been detected on a remote machine.  This server cannot be upgraded from the local machine.  For information on upgrading this existing server, please read the Perforce System Administrator's Guide here:  http://www.perforce.com/perforce/doc.current/manuals/p4sag/01_install.html \n\nWould you like to run the server installer now?" );
				Dictionary.Add( "IEP4ServerOutOfDateCaption", "Upgrade Perforce Server" );
				Dictionary.Add( "IEP4ServerLocalConfirmMessage", "Running the Perforce Server(P4D) installer will allow you to install the perforce server locally.  We recommend you read the Perforce System Administrator's Guide before proceeding.  The guide can be found here: http://www.perforce.com/perforce/doc.current/manuals/p4sag/01_install.html \n\nAre you sure you would like to install a Perforce server on the local machine?" );
				Dictionary.Add( "IEP4ServerLocalConfirmCaption", "Perforce Server Install" );

                Dictionary.Add("PSProjectSelect", "Project Select");
                Dictionary.Add("PSGameUT3", "Installs Unreal Development Kit with sample Unreal Tournament content and game code as a starting point for your project.");
                Dictionary.Add("PSGameEmpty", "Installs the Unreal Development Kit with an empty project.");
                Dictionary.Add("PSGameUT3Title", "UT Sample Game");
                Dictionary.Add("PSGameEmptyTitle", "Empty Game");

				Dictionary.Add( "UIOLocation", "Location: " );
				Dictionary.Add( "UIOOptionsGame", "Uninstall Options: " );
				Dictionary.Add( "DeletingFiles", "Deleting Files" );
				Dictionary.Add( "DeletingShortcuts", "Deleting Shortcuts" );
				Dictionary.Add( "UnableDeleteFile", "The above file could not be deleted, most likely because it is open in another application." );
				Dictionary.Add( "UnableDeleteFolder", "The above folder could not be deleted, most likely because it is open in another application." );
				Dictionary.Add( "DeleteRetry", "Would you like to retry the delete?" );

				Dictionary.Add( "PBDecompressing", "Decompressing Files" );
				Dictionary.Add( "PBCompressing", "Compressing Files" );
				Dictionary.Add( "PBPackaging", "Packaging Files" );
				Dictionary.Add( "PBDecompPrereq", "Decompressing Prerequisite Files" );
				Dictionary.Add( "PBPackagingPrereqs", "Packaging Prerequisite Files" );
				Dictionary.Add( "PBSettingInitial", "Preparing for First Run..." );
				Dictionary.Add( "PBCreatingBackup", "Copying Backup Files");

				Dictionary.Add( "RedistPleaseWait", "Installing Prerequisites" );
				Dictionary.Add( "RedistVCRedistHeader", "Visual Studio 2010 Redistributables" );
				Dictionary.Add( "RedistDXRedistHeader", "Microsoft DirectX 9.0c" );
				Dictionary.Add( "RedistAMDCPUHeader", "AMD CPU Drivers (XP Only)" );

				Dictionary.Add( "RedistVCRedistContent", "Access free UDK support documentation and resources at http://www.unrealengine.com/udk/documentation/" );
				Dictionary.Add( "RedistDXRedistContent", "UDK gives you the full power of Unreal Engine 3 with access to the same world class tools used to make blockbuster video games." );
				Dictionary.Add( "RedistAMDCPUContent", "" );

				Dictionary.Add( "RedistVCRedistx86Fail", "VCRedist x86 failed to install." );
				Dictionary.Add( "RedistVCRedistx64Fail", "VCRedist x64 failed to install." );
				Dictionary.Add( "RedistDXRedistFail", "DirectX failed to install." );
				Dictionary.Add( "RedistAMDCPUFail", "AMD CPU Drivers failed to install." );

				Dictionary.Add( "IFDependsFailed", "Dependencies were not installed; please run 'UDK-INSTALL-FOLDER/Binaries/Redist/UE3Redist.exe'" );
				Dictionary.Add( "GQCaptionInvalidInstall", "Invalid Install Location" );
				Dictionary.Add( "GQDescInvalidInstall", "Please check the install location is valid, and there is enough space on the disk." );

				Dictionary.Add( "UE3RedistEULALegalese", "I have read the software license, understand it, and agree to its terms by clicking the 'I Accept' button." );
				Dictionary.Add( "NetCodeWarn", "Unreal Development Kit (UDK) is a game development toolset. Applications authored with UDK contain code that has not been vetted by Epic Games, Inc. Like any application you download over the internet, you should only install this software if you trust the source. Use at your own risk." );

				Dictionary.Add( "ExtractFailCaption", "Installation Failed" );
				Dictionary.Add( "ExtractFailMessage1", "Extracting all files failed with the exception: " );
				Dictionary.Add( "ExtractFailMessage2", "Please visit http://udk.com/troubleshooting for assistance." );
				Dictionary.Add( "UIODeleteOnly", "Uninstall original and temporary files (not user created files)." );
				Dictionary.Add( "UIODeleteAll", "Uninstall all files (including user created files)." );

				Dictionary.Add( "RedistVCRedistContentTech", "The Microsoft Visual C++ 2010 Redistributable Package (x86) installs runtime components of Visual C++ Libraries required to run applications developed with Visual C++ SP1 on a computer that does not have Visual C++ 2010 installed. This package installs runtime components of C Runtime (CRT), Standard C++, ATL, MFC, OpenMP and MSDIA libraries." );
				Dictionary.Add( "RedistDXRedistContentTech", "Microsoft DirectX is a group of technologies designed to make Windows-based computers an ideal platform for running and displaying applications rich in multimedia elements such as full-color graphics, video, 3D animation, and rich audio." );
				Dictionary.Add( "RedistAMDCPUContentTech", "Allows the system to automatically adjust the CPU speed, voltage and power combination that match the instantaneous user performance need. This driver supports AMD Athlon 64 X2 Dual Core processors on Windows XP SP2, Windows 2003 SP1 x86 and x64 Editions." );

				Dictionary.Add( "GQAddingShortcuts", "Adding Shortcuts" );
				Dictionary.Add( "LabEmail", "Email Address" );
				Dictionary.Add( "IOInvalidEmail", "   Invalid email address" );
				Dictionary.Add( "IOInvalidProjectName", "   Invalid project name" );
				Dictionary.Add( "IOProjectOptions", "Project Options" );
				Dictionary.Add( "IOProjectNameLabel", "Project Name" );
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
