/**

 *

 */

 

import gfx.controls.ListItemRenderer;

import gfx.controls.UILoader;

import gfx.motion.Tween;

import gfx.utils.Constraints;



class gfx.controls.TileListItemRenderer extends ListItemRenderer {

	

// Public Properties

// Private Properties

	private var lastPath:String;

// UI Elements

	private var loader:UILoader;

	private var image:MovieClip;

	

// Initialization

	public function TileListItemRenderer() { super(); }

	

// Public Methods

	public function setData(data:Object):Void {

		super.setData(data);

		var path:String = data.source;

		loadImage(path);

	}		

	

	private function configUI():Void {

		super.configUI();

		loader._alpha = 100;

	}

	

	private function loadImage(path:String):Void {

		// Clean up

		if (lastPath == path) { return; }

		loader.visible = false;

		image.removeMovieClip();		

		lastPath = path;

		if (path == null) { return; }

				

		image = attachMovie(path, "image", 1);

		if (image == null) {

			loader.visible = true;

			loader.source = path;

		}

	}	

	

}