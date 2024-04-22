package gfx.launchpanel {
	
	import adobe.utils.MMExecute;
	
	import com.gskinner.util.UserPreferences;
	
	import fl.controls.Button;
	import fl.controls.ComboBox;
	import fl.controls.TextArea;
	import fl.data.DataProvider;
	import fl.managers.StyleManager;
	
	import flash.display.MovieClip;
	import flash.display.Sprite;
	import flash.display.StageAlign;
	import flash.display.StageScaleMode;
	import flash.events.AsyncErrorEvent;
	import flash.events.Event;
	import flash.events.IOErrorEvent;
	import flash.events.MouseEvent;
	import flash.events.SecurityErrorEvent;
	import flash.events.StatusEvent;
	import flash.net.SharedObject;
	import flash.net.URLLoader;
	import flash.net.URLRequest;
	import flash.system.Capabilities;
	import flash.text.TextField;
	import flash.text.TextFormat;
	
	import gfx.launchpanel.data.Player;
	import gfx.launchpanel.data.Profile;
	import gfx.launchpanel.events.DialogEvent;
	import gfx.launchpanel.events.EditEvent;
	import gfx.launchpanel.managers.ExternalInterfaceManager;
	import gfx.launchpanel.views.AlertDialog;
	import gfx.launchpanel.views.BaseDialog;
	import gfx.launchpanel.views.Dialog;
	import gfx.launchpanel.views.EditDialog;
	
	public class Launcher extends Sprite {
		// Static properties:
		public static var SCRIPT_PATH:String = "Scaleform/jsfl/LaunchPanel.jsfl";
		public static var SEP:String = "$__%__$";
		public static const PROFILES_DEFAULT_TEXT:String = "No Profiles";
		public static const PLAYERS_DEFAULT_TEXT:String = "No Players";
		
		// UI Elements:
		public var publishBtn:Button;
		public var addProfileBtn:Button;
		public var removeProfileBtn:Button;
		public var editBtn:Button;
		public var profileList:ComboBox;
		public var addPlayerBtn:Button;
		public var removePlayerBtn:Button;
		public var settingsBtn:Button;	
		public var output:TextArea;
		//public var panelText:MovieClip;
		public var dialog:Dialog;
		protected var editDialog:EditDialog;
		protected var alertDialog:AlertDialog;
		public var LP_LBL_PROFILES:TextField;
		
		// Protected properties:
		protected var so:SharedObject;
		protected var playerList:Array;
		protected var preferencesDelegate:Function;
		protected var prefs:UserPreferences;
		protected var dataLoader:URLLoader;
		protected var profiles:Array;
		protected var players:Array;
		protected var commandline:String;
		protected var idealWidth:Number = 190;
		protected var data:XML;
		protected var itemsHash:Object;
		protected var selectedProfile:Profile;
		protected var selectedProfileOriginal:Profile; // Used if user cancels editing.
		protected var isPublishable:Boolean;
		protected var isEditingNewProfile:Boolean;
		protected var externalInterfaceManager:ExternalInterfaceManager;
		
		// Initialization:
		
		public function Launcher() {
			init();
			configUI();
		}
		
		// Public Methods:
		
		public function publish():void {
			if (isPublishable) {
				var command:String = selectedProfile.command;
				if (command == null) { command = profiles[profileList.selectedIndex].command; }
				runFunction("launchSWF", profiles[profileList.selectedIndex].playerPath, command || "");
			} else {
				var message:String = "You need to add a player to a profile before publishing.";
				
				alertDialog = Dialog.show(AlertDialog, {title:"Oops...", message:message}) as AlertDialog;
				if (itemsHash != null) { alertDialog.setLabels(itemsHash); }
			}
		}
		
		// Protected Methods:
		
		protected function init():void {
			so = SharedObject.getLocal("ScaleformLauncher");
			
			profiles = [];
			players = [];
			
			prefs = new UserPreferences(MMExecute("fl.configURI") + "Scaleform/preferences/prefs");
			
			// Load Profile Prefs:
			var profilePrefsData:Array = (prefs.getPref("profiles") != null) ? prefs.getPref("profiles") as Array : [];
			if (profilePrefsData == null || profilePrefsData.length == 0) {
				addProfile(new Profile(PROFILES_DEFAULT_TEXT, "", "", null), false);
			} else {
				loadPrefs("profile", profilePrefsData);
			}
			
			// Load Player Prefs:
			var playerPrefsData:Array = (prefs.getPref("players") != null) ? prefs.getPref("players") as Array : [];
			if (playerPrefsData == null || playerPrefsData.length == 0) {
				addPlayer(new Player(PLAYERS_DEFAULT_TEXT, ""), false);
			} else {
				loadPrefs("player", playerPrefsData);
			}
			
			externalInterfaceManager = new ExternalInterfaceManager(this);
			
			dataLoader = new URLLoader();
			dataLoader.addEventListener(Event.COMPLETE, handleDataLoaderComplete, false, 0, true);
			dataLoader.addEventListener(IOErrorEvent.IO_ERROR, handleDataLoaderIOError, false, 0, true);
			dataLoader.load(new URLRequest(MMExecute("fl.configURI") + "Scaleform/locale/" + Capabilities.language + ".xml"));
		}
		
		protected function loadPrefs(listType:String, prefsData:Array):void {
			var l:Number = prefsData.length;
			var name:String;
			var path:String;
			var i:Number;
			if (listType == "profile") {
				var command:String;
				var playerName:String;
				var profile:Profile;
				for (i = 0; i < l; i++) {
					name = prefsData[i].name;
					command = prefsData[i].command;
					path = prefsData[i].playerPath;
					playerName = prefsData[i].playerName;
					
					profile = new Profile(name, command, path, playerName);
					profiles.push(profile);
				}
				prefs.setPref("profiles", profiles, true);
			} else if (listType == "player") {
				var player:Player;
				for (i = 0; i < l; i++) {
					name = prefsData[i].name;
					path = prefsData[i].path;
					
					player = new Player(name, path);
					players.push(player);
				}
				prefs.setPref("players", players, true);
			}
			
			updateList(listType);
		}
		
		protected function configUI():void {
			stage.scaleMode = StageScaleMode.NO_SCALE;
			stage.align = StageAlign.TOP_LEFT;
			stage.addEventListener(Event.RESIZE, handleStageResize, false, 0, true);
			
			StyleManager.setStyle("textFormat", new TextFormat("_sans", 10, 0x000000));
			//_global.style.setStyle("themeColor", 0xAAAAAA); // AS2-->AS3: need to reskin.
			
			publishBtn.addEventListener(MouseEvent.CLICK, handlePublishClick, false, 0, true);
			publishBtn.setStyle("textFormat", new TextFormat("_sans", 12, 0x000000, true));
			publishBtn.drawNow();
			publishBtn.label = "Test"; // Do not set this in Component Inspector.
			addProfileBtn.addEventListener(MouseEvent.CLICK, handleAddProfileClick, false, 0, true);
			addProfileBtn.label = "+"; // Do not set this in Component Inspector.
			removeProfileBtn.addEventListener(MouseEvent.CLICK, handleRemoveProfileClick, false, 0, true);
			removeProfileBtn.label = "-"; // Do not set this in Component Inspector.
			editBtn.addEventListener(MouseEvent.CLICK, handleEditClick, false, 0, true);
			editBtn.label = "E"; // Do not set this in Component Inspector.
			editBtn.setStyle("icon", ButtonIcon);
			profileList.addEventListener(Event.CHANGE, handleProfileListChange, false, 0, true);
			profileList.drawNow();
			profileList.rowCount = 3;
			profileList.labelField = "name";
			
			updateUIState();
			resizeUI();
			updateUIPositions();
		}
		
		// Update labels and accessability based on the currently selected profile's information:
		protected function updateUIState():void {
			if (selectedProfile.name == PROFILES_DEFAULT_TEXT || selectedProfile.playerName == "") {
				profileList.enabled = publishBtn.enabled = removeProfileBtn.enabled = editBtn.enabled = false;
			} else {
				profileList.enabled = removeProfileBtn.enabled = editBtn.enabled = true;
				profileList.drawNow();
				publishBtn.enabled = (selectedProfile.playerName != PLAYERS_DEFAULT_TEXT);
			}
			publishBtn.label = (publishBtn.enabled) ? "Test with: " + selectedProfile.playerName : "Please select profile player.";
			isPublishable = publishBtn.enabled;
		}
		
		// Resize UI elements based on stage width and height:
		protected function resizeUI():void {
			if (editDialog != null) {
				editDialog.setSize(stage.stageWidth, stage.stageHeight);
			}
			var w:Number = Math.max(stage.stageWidth - 24, idealWidth);
			publishBtn.setSize(w, 55);
			profileList.setSize(stage.stageWidth - addProfileBtn.width - removeProfileBtn.width - editBtn.width - 30, 22);
			Dialog.setSize(stage.stageWidth, stage.stageHeight);
		}
		
		// Update UI elements' positions based on stage width and height:
		protected function updateUIPositions():void {
			var PADDING:Number = 2;
			
			//panelText.x = stage.stageWidth - (panelText.width + 20);
			if (stage.stageWidth - 24 <= idealWidth) { 
				PADDING = 3;
			}
			
			addProfileBtn.x = profileList.x + profileList.width + PADDING;
			removeProfileBtn.x = addProfileBtn.x + addProfileBtn.width + PADDING;
			editBtn.x = removeProfileBtn.x + removeProfileBtn.width + PADDING;
		}
		
		// Update the specified list (ComboBox) and its data provider:
		protected function updateList(listType:String, selectedIndex:int = -1):void {
			if (listType == "profile") {
				if (selectedIndex >= 0) {
					saveSelectedIndex(selectedIndex);
				}
				profileList.dataProvider = new DataProvider(profiles);
				profileList.selectedIndex = getSelectedIndex();
				profileList.drawNow();
				selectedProfile = profiles[profileList.selectedIndex];
			}
			updateUIState();
		}
		
		protected function saveSelectedIndex(index:int):void {
			so.data.lastSelected = index;
			so.flush();
		}
		
		protected function getSelectedIndex():int {
			return so.data.lastSelected;
		}
		
		// Adds a new profile, updates relevant UI list, and saves profiles' data:
		protected function addProfile(profile:Profile, checkForDefaultProfile:Boolean):void {
			if (checkForDefaultProfile && profiles[0].name == PROFILES_DEFAULT_TEXT) {
				profiles.shift();
			}
			profiles.push(profile);
			updateList("profile", profiles.length - 1);
			prefs.setPref("profiles", profiles, true);
		}
		
		// Removes specified profile, updates relevant UI list, and saves profiles' data:
		protected function removeProfile(profileIndex:int, checkForDefaultProfile:Boolean):void {
			profiles.splice(profileIndex, 1);
			if (checkForDefaultProfile && profiles.length == 0) {
				addProfile(new Profile(PROFILES_DEFAULT_TEXT, "", "", null), false);
				return;
			}
			
			updateList("profile", (profileIndex >= profiles.length) ? profiles.length - 1 : profileIndex);
			prefs.setPref("profiles", profiles, true);
		}
		
		// Adds a new player to the selected profile, saves profiles' & players' data, and updates relevant UI list:
		protected function addPlayer(player:Player, checkForDefaultPlayer:Boolean):void {
			if (checkForDefaultPlayer && players[0].name == PLAYERS_DEFAULT_TEXT) {
				players.shift();
			}
			players.push(player);
			selectedProfile.playerName = player.name;
			selectedProfile.playerPath = player.path;
			prefs.setPref("profiles", profiles, true);
			prefs.setPref("players", players, true);
			updateList("profile");
		}
		
		// Removes specified player from all relevant profiles and saves profiles' & players' data:
		protected function removePlayer(playerIndex:int, checkForDefaultPlayer:Boolean):void {
			var removedPlayer:Player = players.splice(playerIndex, 1)[0] as Player;
			var removedPlayerName:String = removedPlayer.name;
			if (checkForDefaultPlayer && players.length == 0) {
				addPlayer(new Player(PLAYERS_DEFAULT_TEXT, ""), false);
			}
			prefs.setPref("players", players, true);
			editDialog.loadPlayers(players);
			var l:int = profiles.length;
			var profile:Profile;
			for (var i:int = 0; i < l; i++) {
				profile = profiles[i] as Profile;
				if (profile.playerName == removedPlayerName) {
					profile.playerName = PLAYERS_DEFAULT_TEXT;
					profile.playerPath = "";
				}
			}
			if (selectedProfileOriginal != null && selectedProfileOriginal.playerName == removedPlayerName) {
				selectedProfileOriginal.playerName = PLAYERS_DEFAULT_TEXT;
				selectedProfileOriginal.playerPath = "";
			}
			prefs.setPref("profiles", profiles, true);
			updateUIState();
		}
		
		protected function editSelectedProfile(isEditingNewProfile:Boolean):void {
			this.isEditingNewProfile = isEditingNewProfile;
			if (!isEditingNewProfile) {
				selectedProfileOriginal = new Profile(selectedProfile.name, selectedProfile.command, selectedProfile.playerPath, selectedProfile.playerName);
			}
			editDialog = Dialog.show(EditDialog, {title:"Edit Profile"}) as EditDialog;
			editDialog.addEventListener(DialogEvent.COMPLETE, handleEditDialogComplete, false, 0, true);
			editDialog.addEventListener(DialogEvent.CANCEL, handleEditDialogCancel, false, 0, true);
			editDialog.addEventListener(EditEvent.ADD_PLAYER, handleEditDialogAddPlayer, false, 0, true);
			editDialog.addEventListener(EditEvent.REMOVE_PLAYER, handleEditDialogRemovePlayer, false, 0, true);
			editDialog.addEventListener(EditEvent.PLAYER_CHANGE, handleEditDialogPlayerChange, false, 0, true);
			if (itemsHash != null) { editDialog.setLabels(itemsHash); }
			editDialog.setPlayersDefaultText(PLAYERS_DEFAULT_TEXT);
			editDialog.loadPlayers(players);
			editDialog.loadProfile(selectedProfile);
			resizeUI();
		}
		
		// JSFL.  Move to JSFL class if it gets heavy.
		protected function runFunction(... params):String {
			var functionName:String = params.shift().toString();
			
			var args:Array = [];
			var l:Number = params.length;
			for (var i:int = 0; i < l; i++) {
				if (params[i] == undefined) { params[i] = ""; }
				args.push("'" + params[i].split("'").join("\'") + "'");
			}
			var scriptPath:String;
			if (l == 0) {
				scriptPath = "fl.runScript(fl.configURI + '" + SCRIPT_PATH + "', '" + functionName + "');";
			} else {
				scriptPath = "fl.runScript(fl.configURI + '" + SCRIPT_PATH + "', '" + functionName + "', " + args.join(",") + ");";
			}
			
			return MMExecute(scriptPath);
		}
		
		// Event Handlers:
		
		protected function handlePublishClick(event:MouseEvent):void {
			publish();
		}
		
		protected function handleAddProfileClick(event:MouseEvent):void {
			addProfile(new Profile("", "%SWF_PATH%", "", ""), true);
			editSelectedProfile(true);
		}
		
		protected function handleRemoveProfileClick(event:MouseEvent):void {
			removeProfile(profileList.selectedIndex, true);
		}
		
		protected function handleEditClick(event:MouseEvent):void {
			editSelectedProfile(false);
		}
		
		// Update the selected profile, save profiles' data, and update the UI list:
		protected function handleEditDialogComplete(event:DialogEvent):void {
			isEditingNewProfile = undefined;
			var profile:Profile = event.data.profile as Profile;
			if (profile.name == PROFILES_DEFAULT_TEXT) {
				// The user has given the profile a reserved name. Add quotes so it isn't reserved.
				profile.name = "\"" + profile.name + "\"";
			}
			selectedProfile.name = profile.name;
			selectedProfile.command = profile.command;
			selectedProfile.playerName = profile.playerName;
			selectedProfile.playerPath = profile.playerPath;
			
			profiles = profiles.concat();
			
			prefs.setPref("profiles", profiles, true);
			updateList("profile", profileList.selectedIndex);
		}
		
		// Run JSFL to aquire new player:
		protected function handleEditDialogAddPlayer(event:EditEvent):void {
			var result:String;
			// Try/catch must be called in case of a script timeout (when the user takes longer than 15 seconds to select a player).
			try {
				result = runFunction("selectPlayer");
				
				// Do not replace this function call with its code (script timeouts will cause weird halting of code execution):
				createPlayer(result);
			} catch (e:*) {
				// Error is expected after user taking > 15 seconds... therefore no need to report error.
				//MMExecute("fl.trace('Error at Launcher.handleEditDialogAddPlayer(): "+e+"')");
				// Do not replace this function call with its code (script timeouts will cause weird halting of code execution):
				createPlayer(result);
			}
		}
		
		protected function createPlayer(result:String) {
			if (result == null) { return; }
			var parts:Array = result.split(SEP);
			if (parts[1] == null || parts[1] == "null" || parts[1].length == 0) { return; }
			if (parts[1] == PLAYERS_DEFAULT_TEXT) {
				// The user has given the player a reserved name. Add quotes so it isn't reserved.
				parts[1] = "\"" + parts[1] + "\"";
			}
			addPlayer(new Player(parts[1], parts[0]), true);
			editDialog.loadPlayers(players);
		}
		
		protected function handleEditDialogRemovePlayer(event:EditEvent):void {
			removePlayer(event.data.removedPlayerIndex, true);
		}
		
		// The user has changed the player in the EditDialog. Update the selected profile's player:
		protected function handleEditDialogPlayerChange(event:EditEvent):void {
			var player:Player = event.data.changedPlayer as Player;
			selectedProfile.playerName = player.name;
			selectedProfile.playerPath = player.path;
			prefs.setPref("profiles", profiles, true);
			updateUIState();
		}
		
		protected function handleEditDialogCancel(event:DialogEvent):void {
			if (isEditingNewProfile == true) {
				removeProfile(profileList.selectedIndex, true);
			} else if (isEditingNewProfile == false) {
				// Update edited profile to its original data.
				selectedProfile = selectedProfileOriginal;
				profiles[profileList.selectedIndex] = selectedProfile;
				prefs.setPref("profiles", profiles, true);
				updateUIState();
			} else {
				throw new Error("Error at Launcher.handleEditDialogCancel(): The user is not editing.");
			}
			isEditingNewProfile = undefined;
		}
		
		protected function handleProfileListChange(event:Event):void {
			updateList("profile", profileList.selectedIndex);
		}
		
		// Language XML has been successfully loaded. Update labels and check if user was editing:
		protected function handleDataLoaderComplete(event:Event):void {
			data = new XML();
			data.ignoreWhitespace = true;
			try {
				data = XML(dataLoader.data);
			} catch (error:*) {
				trace("Launcher.handleDataLoaderComplete() Error parsing XML: " + error);
			}
			
			var a:XMLList = data.children();
			var l:int = a.length();
			itemsHash = {};
			for (var i:int = 0; i < l; i++) {
				var item = a[i];
				var key = item.@key;
				var label:String = item.toString();
				if (!itemsHash[key]) {
					itemsHash[key] = label;
				}
				
			}
			
			LP_LBL_PROFILES.text = itemsHash["LP_LBL_PROFILES"];
			//panelText.LP_LBL_TITLE.text = itemsHash["LP_LBL_TITLE"];
			
			if (selectedProfile.name == "" || selectedProfile.playerName == "") {
				editSelectedProfile(true);
			}
		}
		
		protected function handleDataLoaderIOError(event:IOErrorEvent):void {
			MMExecute("fl.trace('Scaleform Launch Panel.Launcher.handleDataLoaderIOError(): Loading XML Failed')");
		}
		
		protected function handleStageResize(event:Event):void {
			resizeUI();
			updateUIPositions();
		}
		
	}
	
}