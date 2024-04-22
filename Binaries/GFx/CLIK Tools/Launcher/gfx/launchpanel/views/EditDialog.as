package gfx.launchpanel.views {
	
	import fl.controls.Button;
	import fl.controls.ComboBox;
	import fl.controls.TextInput;
	import fl.data.DataProvider;
	
	import flash.display.Sprite;
	import flash.events.Event;
	import flash.events.MouseEvent;
	import flash.text.TextField;
	
	import gfx.launchpanel.data.Player;
	import gfx.launchpanel.data.Profile;
	import gfx.launchpanel.events.DialogEvent;
	import gfx.launchpanel.events.EditEvent;
	import gfx.launchpanel.views.BaseDialog;
	
	public class EditDialog extends BaseDialog {
		public var nameInput:TextInput;
		public var cmdInput:TextInput;
		public var playerList:ComboBox;
		public var addPlayerBtn:Button;
		public var removePlayerBtn:Button;
		public var LP_BTN_OK:Button;
		public var LP_BTN_CANCEL:Button;
		public var LP_LBL_PLAYER:TextField;
		public var LP_LBL_PROFILE_NAME:TextField;
		public var LP_LBL_COMMAND_PARAS:TextField;
		public var bg:Sprite;
		
		protected var playersDefaultText:String;
		protected var selectedProfile:Object;
		protected var players:Array;
		protected var minWidth:Number = 214;
		
		public function EditDialog() {
			super();
		}
		
		public override function setLabels(itemsHash:Object):void {
			LP_BTN_CANCEL.label = itemsHash["LP_BTN_CANCEL"];
			LP_BTN_OK.label = itemsHash["LP_BTN_OK"];
			LP_LBL_PLAYER.text = itemsHash["LP_LBL_PLAYER"];
			LP_LBL_PROFILE_NAME.text = itemsHash["LP_LBL_PROFILE_NAME"];
			LP_LBL_COMMAND_PARAS.text = itemsHash["LP_LBL_COMMAND_PARAS"];
		}
		
		public function setPlayersDefaultText(text:String):void {
			playersDefaultText = text;
		}
		
		public function loadPlayers(players:Array):void {
			if (players.length == 0) {
				var defaultPlayer:Player = new Player(playersDefaultText, "");
				players.push(defaultPlayer);
			}
			this.players = players;
			playerList.dataProvider = new DataProvider(this.players);
			playerList.selectedIndex = this.players.length - 1;
			playerList.drawNow();
			updateDialog();
		}
		
		public function loadProfile(p_selectedProfile:Profile):void {
			selectedProfile = p_selectedProfile;
			nameInput.text = selectedProfile.name;
			cmdInput.text = selectedProfile.command || "";
			var playerName = selectedProfile.playerName;
			var l:Number = playerList.length;
			for(var i:Number = 0; i < l; i++) {
				if (playerList.getItemAt(i).path == selectedProfile.playerPath && playerList.getItemAt(i).name == selectedProfile.playerName) {
					playerList.selectedIndex = i;
				}
			}
			updateDialog();
		}
		
		public function setSize(width:Number, height:Number):void {
			if (width < minWidth) {
				if (bg.width == minWidth) { return; }
				width = minWidth;
			}
			bg.width = width;
			removePlayerBtn.x = bg.x + bg.width - removePlayerBtn.width - PADDING;
			addPlayerBtn.x = removePlayerBtn.x - addPlayerBtn.width - PADDING;
			playerList.width = addPlayerBtn.x - PADDING - playerList.x;
			nameInput.width = removePlayerBtn.x + removePlayerBtn.width - nameInput.x;
			cmdInput.width = nameInput.width;
			LP_BTN_OK.x = bg.x + bg.width - LP_BTN_OK.width - PADDING;
			LP_BTN_CANCEL.x = LP_BTN_OK.x - LP_BTN_CANCEL.width - PADDING;
		}
		
		// Protected Methods:
		
		protected override function configUI():void {
			super.configUI();
			
			LP_BTN_OK.enabled = false;
			LP_BTN_OK.drawNow();
			LP_BTN_OK.addEventListener(MouseEvent.CLICK, handleOKClick, false, 0, true);
			LP_BTN_CANCEL.addEventListener(MouseEvent.CLICK, handleCancelClick, false, 0, true);
			LP_BTN_CANCEL.drawNow();
			addPlayerBtn.addEventListener(MouseEvent.CLICK, handleAddPlayerClick, false, 0, true);
			addPlayerBtn.drawNow();
			removePlayerBtn.addEventListener(MouseEvent.CLICK, handleRemovePlayerClick, false, 0, true);
			removePlayerBtn.label = "-"; // Do not set this in Component Inspector or enabling won't work.
			removePlayerBtn.drawNow();
			playerList.addEventListener(Event.CHANGE, handlePlayerListChange, false, 0, true);
			playerList.labelField = "name";
			playerList.rowCount = 4;
			playerList.drawNow();
			nameInput.addEventListener(Event.CHANGE, handleNameInputChange, false, 0, true);
			nameInput.drawNow();
			cmdInput.drawNow();
		}
		
		protected function updateDialog():void {
			removePlayerBtn.enabled = playerList.enabled = (players[0].name != playersDefaultText);
			LP_BTN_OK.enabled = !(nameInput.length < 1 || players[0].name == playersDefaultText);
		}
		
		// Event Handlers:
		
		protected function handleAddPlayerClick(event:MouseEvent):void {
			dispatchEvent(new EditEvent(EditEvent.ADD_PLAYER));
		}
		
		protected function handleRemovePlayerClick(event:MouseEvent):void {
			dispatchEvent(new EditEvent(EditEvent.REMOVE_PLAYER, {removedPlayerIndex:playerList.selectedIndex}));
		}
		
		protected function handlePlayerListChange(event:Event):void {
			dispatchEvent(new EditEvent(EditEvent.PLAYER_CHANGE, {changedPlayer:playerList.selectedItem}));
		}
		
		protected function handleNameInputChange(event:Event):void {
			updateDialog();
		}
		
		protected function handleOKClick(event:MouseEvent):void {
			var selectedPlayer:Object = {};
			if (playerList.selectedItem == null) {
				selectedPlayer = playerList.getItemAt(playerList.length - 1); 
			} else {
				selectedPlayer = playerList.selectedItem;
			}
			
			var profile:Profile = new Profile(nameInput.text, cmdInput.text, selectedPlayer.path, selectedPlayer.name);
			dispatchEvent(new DialogEvent(DialogEvent.COMPLETE, {profile:profile}));
		}
		
		protected function handleCancelClick(event:MouseEvent):void {
			dispatchEvent(new DialogEvent(DialogEvent.CANCEL));
			close();
		}
		
	}
	
}