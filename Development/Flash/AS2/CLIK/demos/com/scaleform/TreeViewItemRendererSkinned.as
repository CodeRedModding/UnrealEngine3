import com.scaleform.TreeViewItemRenderer;

class com.scaleform.TreeViewItemRendererSkinned extends TreeViewItemRenderer {
	
	public function TreeViewItemRendererSkinned() { 
		super(); 
		
		iconItemLineStraight = "s_TreeItemLine_Straight";
		iconFolderRoot = "s_TreeFolder_Root";
		iconFolderLeaf = "s_TreeFolder_Leaf";
		iconItemLineMiddle = "s_TreeItemLine_Middle";
		iconItemLineBottom = "s_TreeItemLine_Bottom";
		iconItemAlone = "s_TreeItem_Alone";
		iconItemTop = "s_TreeItem_Top";
		iconItemMiddle = "s_TreeItem_Middle";	
		iconItemBottom = "s_TreeItem_Bottom";
		
		iconSize = 20;
		textMargin = 3;
	}		
}