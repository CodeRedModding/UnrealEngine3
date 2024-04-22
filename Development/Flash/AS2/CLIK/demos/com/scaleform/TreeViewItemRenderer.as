import gfx.controls.ListItemRenderer;
import gfx.utils.Constraints;
import gfx.utils.Delegate;

import com.scaleform.TreeViewConstants;

[InspectableList("disabled", "visible", "labelID")]
class com.scaleform.TreeViewItemRenderer extends ListItemRenderer {
	
	public var iconItemLineStraight:String = "TreeItemLine_Straight";
	public var iconFolderRoot:String = "TreeFolder_Root";
	public var iconFolderLeaf:String = "TreeFolder_Leaf";
	public var iconItemLineMiddle:String = "TreeItemLine_Middle";
	public var iconItemLineBottom:String = "TreeItemLine_Bottom";
	public var iconItemAlone:String = "TreeItem_Alone";
	public var iconItemTop:String = "TreeItem_Top";
	public var iconItemMiddle:String = "TreeItem_Middle";	
	public var iconItemBottom:String = "TreeItem_Bottom";
	
	public var iconSize:Number = 20;	
	public var textMargin:Number = 3;	
	
	private var hit:MovieClip;
	
	private var lineIconCache:Array;
	private var connectorIcon:MovieClip;
	private var folderIcon:MovieClip;
		
	public function TreeViewItemRenderer() { 
		super(); 
		lineIconCache = new Array();
		connectorIcon = null;
		folderIcon = null;
	}	
	
	public function setData(data:Object):Void {		
		this.data = data;
		textField.text = data.label;
		updateAfterStateChange();
	}	

	private function configUI():Void {
		hit.onRollOver = Delegate.create(this, handleMouseRollOver);
		hit.onRollOut = Delegate.create(this, handleMouseRollOut);
		hit.onPress = Delegate.create(this, handleMousePress);
		hit.onRelease = Delegate.create(this, handleMouseRelease);		
		hit.onDragOver = Delegate.create(this, handleDragOver);
		hit.onDragOut = Delegate.create(this, handleDragOut);
		hit.onReleaseOutside = Delegate.create(this, handleReleaseOutside);
		
		if (focusIndicator != null && !_focused && focusIndicator._totalFrames == 1) { focusIndicator._visible = false; }		
		focusTarget = owner;
		
		updateAfterStateChange();
	}	
	
	private function draw():Void {
		// Note that if this is called after a frame change, and there is a new keyframe, the size may be read incorrectly in GFx.
		if (sizeIsInvalid) { // The size has changed.	
			_width = __width;
			_height = __height;
		}
	}	
	
	private function updateAfterStateChange() {
		// Redraw should only happen AFTER the initialization.
		if (!initialized) { return; }
		validateNow();// Ensure that the width/height is up to date.
		if (data) { drawLayout(); }							
		else { clearLayout(); }
		if (textField != null && _label != null) { textField.text = _label; }		
		dispatchEvent({type:"stateChange", state:state});				
	}
	
	private function drawLayout() {
		var cscale:Number = (100/this._xscale);
		var cssz:Number = cscale*iconSize;
		// draw depth icons		
		var depthIcons:Array = data.depthIcons;
		var lineIconIdx:Number = 0;
		var d:Number = depthIcons.length;		
		for (var i:Number=0; i < d; i++) {
			if (depthIcons[i]==0) {
				continue;
			}
			var iconMC:MovieClip = lineIconCache[lineIconIdx];
			if (!iconMC) { 
				iconMC = this.attachMovie(iconItemLineStraight, "icon"+lineIconIdx, this.getNextHighestDepth(), {_x:i*cssz, _y:0});
				lineIconCache.push(iconMC);
				iconMC._width = cssz;
			} else { 
				iconMC = lineIconCache[lineIconIdx]; 
				iconMC._x = i*cssz;
			}
			iconMC._visible = true;
			lineIconIdx++;
		}
		// hide the rest of the line icons
		if (lineIconIdx < lineIconCache.length) {
			for (var i:Number=lineIconIdx; i < lineIconCache.length; i++) {
				lineIconCache[i]._visible = false;
			}
		}
		// draw state icon
		if (data.type == TreeViewConstants.TYPE_CLOSED) {
			createButtonIcon(d++, cssz).selected = false;
		} else if (data.type == TreeViewConstants.TYPE_OPEN) {
			createButtonIcon(d++, cssz).selected = true;
		} else { // leaf
			createLeafIcon(d++, cssz);
		}
		// draw folder icon
		createFolderIcon(d++, cssz);
		// layout textfield
		textField._xscale = cscale*100;
		textField._width = __width-d*20 - textMargin*2;
		textField._x = d*cssz + textMargin;
		// redraw
		invalidate();
	}
	
	private function clearLayout():Void {
		for (var i:Number=0; i<lineIconCache.length; i++) {
			lineIconCache[i]._visible = false;
		}
		if (connectorIcon) { connectorIcon._visible = false; }
		if (folderIcon) { folderIcon._visible = false }
	}
	
	private function createFolderIcon(d:Number, cssz:Number):MovieClip {
		var reqType:Number = (data.type==TreeViewConstants.TYPE_OPEN)?TreeViewConstants.ICON_FOLDER_ROOT:TreeViewConstants.ICON_FOLDER_LEAF;
		if (!folderIcon || folderIcon.type != reqType) {
			if (folderIcon) { folderIcon.removeMovieClip(); }
			folderIcon = this.attachMovie(
									(reqType==TreeViewConstants.ICON_FOLDER_ROOT)?iconFolderRoot:iconFolderLeaf, 
									"folderIcon", this.getNextHighestDepth(), {_x:d*cssz, _y:0});
			folderIcon.type = reqType;
			folderIcon._width = cssz;
		} else { 
			folderIcon._x = d*cssz;
		}
		folderIcon._visible = true;
		return folderIcon;
	}	
	
	private function createLeafIcon(d:Number, cssz:Number):MovieClip {
		var reqType:Number = (data.nextSibling) ? TreeViewConstants.ICON_LINE_MIDDLE : TreeViewConstants.ICON_LINE_BOTTOM;
		if (!connectorIcon || connectorIcon.type != reqType) {
			if (connectorIcon) { connectorIcon.removeMovieClip(); }
			connectorIcon = this.attachMovie(
									(reqType==TreeViewConstants.ICON_LINE_MIDDLE)?iconItemLineMiddle:iconItemLineBottom, 
									"connectorIcon", this.getNextHighestDepth(), {_x:d*cssz, _y:0});
			connectorIcon.type = reqType;
			connectorIcon._width = cssz;
		} else { 
			connectorIcon._x = d*cssz;
		}
		connectorIcon._visible = true;
		return connectorIcon;
	}
	
	private function createButtonIcon(d:Number, cssz:Number):MovieClip {
		var reqType:Number = (data.nextSibling) ? TreeViewConstants.ICON_BUTTON_MIDDLE : TreeViewConstants.ICON_BUTTON_BOTTOM;
		if (data.isRoot) { reqType = (data.nextSibling) ? TreeViewConstants.ICON_BUTTON_TOP	: TreeViewConstants.ICON_BUTTON_ALONE; }
		if (!connectorIcon || connectorIcon.type != reqType) {
			if (connectorIcon) { connectorIcon.removeMovieClip(); }
			var iconId:String = iconItemAlone;
			switch (reqType) {
				case TreeViewConstants.ICON_BUTTON_TOP: { iconId = iconItemTop; break; }
				case TreeViewConstants.ICON_BUTTON_MIDDLE: { iconId = iconItemMiddle; break; }
				case TreeViewConstants.ICON_BUTTON_BOTTOM: { iconId = iconItemBottom; break; }
			}
			connectorIcon = this.attachMovie(iconId, "connectorIcon", this.getNextHighestDepth(), {_x:d*cssz, _y:0});
			connectorIcon.type = reqType;
			connectorIcon._width = cssz;
			connectorIcon.disableFocus = true;
			connectorIcon.addEventListener("click", this, "handleItemButton");
		} else {
			connectorIcon._x = d*cssz;
		}
		connectorIcon._visible = true;
		return connectorIcon;
	}
	
	private function handleItemButton(e):Void {
		// flip open/close state
		data.type = (data.type==TreeViewConstants.TYPE_OPEN)?TreeViewConstants.TYPE_CLOSED:TreeViewConstants.TYPE_OPEN;
		owner.dataProvider.validateLength();
		owner.invalidateData();
	}
}