/**
	A Video Player Class with full functionality
	
	The class includes seek slider, volume control/muting, looping, rewind/fastforward (for cue points), play/pause, stop, min/max video size, 
	multiple subtitle tracks + subtitle display, mutliple audio channels, & background color picker + alpha
 
*/

import gfx.core.UIComponent;
import gfx.events.EventDispatcher;
import gfx.controls.Button;
import gfx.controls.CheckBox;
import gfx.controls.Label;
import gfx.controls.OptionStepper;
import gfx.controls.ProgressBar;
import gfx.controls.Slider;
import gfx.utils.Constraints;
import gfx.utils.Delegate;
import com.scaleform.ColorPicker;

class com.scaleform.VideoPlayer extends UIComponent {
	
// Public Properties:

			
// Private Properties:

	private var c:Constraints;
	private var intervalID:Number;
	private var rIntervalID:Number;
	private var videoPath:String;
	private var videoArea:Number;
	private var videoAspectRatio:Number;
	private var currentVolume:Number;
	private var curAudioTrack:Number;
	private var numAudioTracks:Number;
	private var curSubtitleTrack:Number;
	private var numSubtitleTracks:Number;
	private var aCuePoints:Array;
	private var nextCuePoint:Number;
	private var prevCuePoint:Number;
	
	/* Video states */
	private var bMuted:Boolean;
	private var bLooping:Boolean;
	private var bPaused:Boolean;
	private var bVideoWasPaused:Boolean;
	private var bPlaying:Boolean;
	private var bStopped:Boolean;
	private var bTransparent:Boolean;
	private var bFullScreen:Boolean;
	private var bMaximized:Boolean;
	private var bSeekDrag:Boolean;
	
	/* Netstream data properties */
	private var nc:NetConnection;
	private var ns:NetStream;
	private var videoDuration:Number;
	private var videoWidth:Number;
	private var videoHeight:Number;
	private var videoAudio:Sound;
	
// UI Elements:	

	public var stopBtn:Button;
	public var rewindBtn:Button;
	public var fastForwardBtn:Button;
	public var maxBtn:Button;
	public var videoSizeBtn:Button;
	public var videoFileBtn:Button;
	public var colorPicker:ColorPicker;
	public var playBtn:CheckBox;
	public var muteBtn:CheckBox;
	public var loopBtn:CheckBox;
	public var alphaBtn:CheckBox;
	public var videoTime:Label;
	public var videoTitle:Label;
	public var subtitle:Label;
	public var videoBg:MovieClip;
	public var checkerBg:MovieClip;
	public var controlBar:MovieClip;
	public var controlBarBg:MovieClip;
	public var audioChannels:OptionStepper;
	public var subtitleTracks:OptionStepper;
	public var volumeProgress:ProgressBar;
	public var seekProgress:ProgressBar;
	public var volumeSlider:Slider;
	public var seekSlider:Slider;
	public var video:Video;

// Initialization:

	public function VideoPlayer() {
		super();
		
		/* Set up the netstream */
		nc = new NetConnection();
		nc.connect(null);
		ns = new NetStream(nc);
		ns.parent = this;
		
		/* Attach the video & audio to the netstream */
		video.attachVideo(ns);
		this.attachAudio(ns);
		videoAudio = new Sound(this);

		/* Listen for the video's meta data */
		ns.onMetaData = function(videoObj){
			/* Get the base video information (duraiton, width, height, aspect ratio) */
			this.parent.videoDuration = videoObj.duration;
			this.parent.videoWidth = videoObj.width;
			this.parent.videoHeight = videoObj.height;
			this.parent.videoAspectRatio = videoObj.width / videoObj.height;
			
			/* When a video is first loaded, ensure it is displayed at the proper aspect ratio */
			this.parent.setResolution();
			this.parent.controlBar.videoDuration.text = this.parent.formatTime(this.parent.videoDuration);
			
			/* Get the number of subtitle tracks & intialize the subtitle track option stepper UI element */
			if (videoObj.subtitleTracksNumber >= 1) {
				this.parent.numSubtitleTracks = videoObj.subtitleTracksNumber;
				var aSubTrack:Array = ["No Subs"];
				for (var i:Number = 0; i < videoObj.subtitleTracksNumber; i) {
					i++
					aSubTrack[i] = i;
				}
				this.parent.controlBar.subtitleTracks.disabled = false;
				this.parent.controlBar.subtitleTracks.dataProvider = aSubTrack;
			}
			else {
				this.parent.controlBar.subtitleTracks.disabled = true;
				this.parent.controlBar.subtitleTracks.dataProvider = ["No Subs"];
				this.parent.controlBar.subtitleTracks.selectedIndex = 0;
			}
			
			/* Get the number of audio channels & initialize the audio channel option stepper UI element */
			if (videoObj.audioTracks >= 1) {
				this.parent.numAudioTracks = videoObj.audioTracks.length;
				var aAudioChan:Array = ["No Audio"];
				for (var i:Number = 0; i < videoObj.audioTracks.length; i) {
					i++
					aAudioChan[i] = i;
				}
				this.parent.controlBar.audioChannels.disabled = false;
				this.parent.controlBar.audioChannels.dataProvider = aAudioChan;
			}
			else {
				this.parent.controlBar.audioChannels.disabled = true;
				this.parent.controlBar.audioChannels.dataProvider = ["No Audio"];
				this.parent.controlBar.audioChannels.selectedIndex = 0;
			}
			
			/* Get the cue points and store them in aCuePoints array */
			var p:String;
			for (p in videoObj) {
				switch (p) {
				case "cuePoints" :
					this.parent.aCuePoints = videoObj[p];
					break;
				default :
					break;
				}
			}
			
			/* If there are no cue points, disable the rewind & fastforward buttons */
			if (this.parent.aCuePoints.length == undefined || this.parent.aCuePoints.length == 0 || this.parent.aCuePoints.length == null) { 
				this.parent.controlBar.rewindBtn.disabled = this.parent.controlBar.fastForwardBtn.disabled = true; 
			}
			else { this.parent.controlBar.rewindBtn.disabled = this.parent.controlBar.fastForwardBtn.disabled = false; }
				this.parent.getNextCuePoint();
				
			this.parent.dispatchEvent({type: "videoLoaded"});
		}
		
		/* Get & display subtitles in the subtitle label as they occur */
		ns.onSubtitle = function(msg:String) {
			this.parent.subtitle.text = msg;
		}
	}

// Public Methods:

	/* Setter sets the video path using the data in the videoTitle label and calls playVideo */
	public function setVideo(video:String):Void {
		if (video != "*.usm") {
			videoPath = video;
			playVideo();
		}
		controlBar.videoTitle.text = video;
	}
	
	/* Getter returns videoInfo as an object which contains all the basic video meta data */
	public function getVideo():Object {
		var videoInfo:Object = {video:videoPath, duration:videoDuration, width:videoWidth, height:videoHeight, aspectRatio:videoAspectRatio, cuePoints:aCuePoints.length, subtitleTracks:numSubtitleTracks, audioTracks:numAudioTracks };
		return videoInfo;
	}
	
	public function toString():String {
		return "[Scaleform Video Player " + _name + "]";
	}
	
// Private Methods:

	private function configUI():Void {
		super.configUI();
		
		/* Set initial player control states */
		bPlaying = bSeekDrag = bMuted = bLooping = bStopped = bPaused = videoBg._visible = false;
		bFullScreen = bTransparent = true;
		controlBar.alphaBtn.selected = true;
		controlBar.playBtn.selected = false;
		controlBar.volumeSlider.value = 5;
		currentVolume = 50;
		controlBar.volumeProgress.setProgress(currentVolume,100)
		controlBar.seekProgress.setProgress(0,100)
		videoArea = Stage.height - controlBarBg._height;
		checkerBg._x = checkerBg._y = videoBg._x = videoBg._y = video._x = video._y = 0;
		subtitle.text = "";
		subtitle._y = video._y + video._height - subtitle._height - 10;
		controlBar.subtitleTracks.dataProvider = ["No Subs"];
		controlBar.subtitleTracks.selectedIndex = 0;
		controlBar.audioChannels.dataProvider = ["No Audio"];
		controlBar.audioChannels.selectedIndex = 0;
				
		if (videoPath == undefined) {
			controlBar.videoSizeBtn.disabled = controlBar.seekSlider.disabled = controlBar.playBtn.disabled = controlBar.stopBtn.disabled = controlBar.rewindBtn.disabled = controlBar.fastForwardBtn.disabled = true;
		}
				
		setBGColor();  
						
		/* Set resize constraints */
		c = new Constraints(this);
		c.addElement(controlBar, Constraints.BOTTOM);
		c.addElement(controlBarBg, Constraints.BOTTOM | Constraints.LEFT | Constraints.RIGHT);
		Stage.scaleMode = 'noScale';
		Stage.align = 'TL';
				
		/* Add event listeners to the controls */
		controlBar.playBtn.addEventListener("click", this, "handlePlay");
		controlBar.stopBtn.addEventListener("click", this, "handleStop");
		controlBar.muteBtn.addEventListener("click", this, "handleMute");
		controlBar.loopBtn.addEventListener("click", this, "handleLoop");
		controlBar.rewindBtn.addEventListener("press", this, "handleRewindPress");
		controlBar.rewindBtn.addEventListener("click", this, "handleNavRelease");
		controlBar.rewindBtn.addEventListener("releaseOutside", this, "handleNavRelease");
		controlBar.fastForwardBtn.addEventListener("press", this, "handleFastForwardPress");
		controlBar.fastForwardBtn.addEventListener("click", this, "handleNavRelease");
		controlBar.fastForwardBtn.addEventListener("releaseOutside", this, "handleNavRelease");
		controlBar.maxBtn.addEventListener("click", this, "handleMaximize");
		controlBar.alphaBtn.addEventListener("click", this, "handleBGAlpha");
		controlBar.videoSizeBtn.addEventListener("click", this, "handleVideoResize");
		controlBar.videoFileBtn.addEventListener("click", this, "handleVideoLoad");
		controlBar.volumeSlider.addEventListener("change", this, "handleVolumeChange");
		controlBar.seekSlider.addEventListener("change", this, "handleSeekChange");
		controlBar.seekSlider.thumb.addEventListener("press", this, "handleThumbPress");
		controlBar.seekSlider.thumb.addEventListener("click", this, "handleThumbRelease");
		controlBar.seekSlider.thumb.addEventListener("releaseOutside", this, "handleThumbRelease");
		controlBar.seekSlider.track.addEventListener("click", this, "handleThumbRelease");
		controlBar.colorPicker.addEventListener("colorSet", this, "handleBGColorChange");
		controlBar.audioChannels.addEventListener("change", this, "handleAudioChannels");
		controlBar.subtitleTracks.addEventListener("change", this, "handleSubtitleTracks");
		
		/* Set up the stage to listen for resize events */
		var o:Object = {};
		var _this:Object = this;
		o.onResize = Delegate.create(this,onResize);
		Stage.addListener(o);
		
		/* Listen for netstream status events */
		ns.onStatus = Delegate.create(this,onStatus);
		
		/* Dispatch an initialized event */
		dispatchEvent({type: "init"});
	}
	
	/* Update the contraints */
	private function draw():Void {
		c.update(__width, __height);
	}
	
	/* Play the video */
	private function playVideo():Void {
		bPlaying = video._visible = true;
		bStopped = bVideoWasPaused = false;
		ns.play(videoPath);
		pauseVideo(false);
		controlBar.videoSizeBtn.disabled = controlBar.seekSlider.disabled = controlBar.stopBtn.disabled = controlBar.playBtn.disabled = false;
		intervalID = setInterval(this, "setVideoTime", 50);
	}
	
	/* Pause the video */
	private function pauseVideo(pause:Boolean):Void {
		bPaused = pause;
		ns.pause(pause);
		controlBar.playBtn.selected = !pause;
	}

	/* This function ensures the video maintains its aspect ratio when resizing */
	private function setResolution():Void {
		var checkerBgRatio = checkerBg._width / checkerBg._height;

		/* if full screen mode, fit video to screen at true aspect ratio */
		if (bFullScreen) {
			if((Stage.width / videoArea) > videoAspectRatio) {
				trace("here");
				video._height = checkerBg._height = videoBg._height = videoArea;
				video._width = checkerBg._width = videoBg._width = video._height * videoAspectRatio;
			}
			else {
				video._width = Stage.width;
				if (videoAspectRatio != undefined) { 
					video._height = checkerBg._height = videoBg._height = video._width / videoAspectRatio; 
					checkerBg._width = videoBg._width = Stage.width;
				}
				else { 
					video._height = checkerBg._height = videoBg._height = videoArea;
					checkerBg._width = videoBg._width = checkerBg._height * checkerBgRatio;
				}
			}
		}
		
		/* if not full screen mode, show video at original resolution in center of play area */
		else {
			video._width = videoBg._width = checkerBg._width = videoWidth;
			video._height = videoBg._height = checkerBg._height = videoHeight;
		}
		video._x = (Stage.width / 2) - (video._width / 2);
		video._y = videoBg._y = checkerBg._y = (videoArea / 2) - (video._height / 2);
		checkerBg._x = videoBg._x = Stage.width / 2 - checkerBg._width / 2;
	}
	
	/* setVideoTime updates the seek bar & videoTime label every 50 milliseconds when a video is playing */
	private function setVideoTime():Void {
		
		/* skip updating if the video is stopped or the seek slider is being dragged */
		if(bSeekDrag || bStopped) { return; }
		
		/* update the videoTime label, the seekSlider, and seekProgress */
		controlBar.videoTime.text = formatTime(ns.time);
		var sliderRatio:Number = videoDuration / ns.time;
		controlBar.seekProgress.setProgress(100 / sliderRatio,100);
		controlBar.seekSlider.value = 10 / sliderRatio;
		videoAudio.setVolume((!bMuted) ? currentVolume : 0);
		
		/* get current play time and disable/enable fast forward & rewind buttons based on whether the video has reached the last or first cue point */
		if (ns.time > aCuePoints[0].time) { controlBar.rewindBtn.disabled = false; }
		else { controlBar.rewindBtn.disabled = true; }
		if (Math.round(ns.time) < Math.round(aCuePoints[aCuePoints.length-1].time)) { controlBar.fastForwardBtn.disabled = false; }
		else { controlBar.fastForwardBtn.disabled = true; }
	}
	
	/* Determine what the next & previous cue points are, based on the current play time */
	private function getNextCuePoint():Void {
			
		/* Determine the next cue point */
		for (var next:Number = 0; next < aCuePoints.length; next++) {
			if (Math.round(ns.time) < Math.round(aCuePoints[next].time)) {
				nextCuePoint = Math.round(aCuePoints[next].time);
				break;
			}
		}
		
		/* Determine the previous cue point */
		for (var prev:Number = aCuePoints.length; prev > 0; prev--) {
			if (ns.time >= aCuePoints[prev-1].time) {
				prevCuePoint = aCuePoints[prev-1].time;
				break;
			}
		}
	}
	
	/* Move the playhead to the next or previous cue point */
	private function seekToCuePoint(direction:String):Void {
		switch (direction) {
			case "forward" :
				if(nextCuePoint >= aCuePoints[aCuePoints.length-1].time) {
					controlBar.fastForwardBtn.disabled = true;
					handleNavRelease();
				}
				ns.seek(nextCuePoint);
			case "reverse" :
				if(prevCuePoint <= aCuePoints[0].time) {
					controlBar.rewindBtn.disabled = true;
					handleNavRelease();
				}
				ns.seek(prevCuePoint);
		}
		controlBar.videoTime.text = formatTime(ns.time);
	}
	
	 /* Returns time in hh:mm:ss format */
	private function formatTime(time:Number):String {
		var remainder:Number;
		var hours:Number = time / (60 * 60);
		remainder = hours - (Math.floor(hours));
		hours = Math.floor(hours);
		var minutes = remainder * 60;
		remainder = minutes - (Math.floor(minutes));
		minutes = Math.floor(minutes);
		var seconds = remainder * 60;
		remainder = seconds - (Math.floor(seconds));
		seconds = Math.floor(seconds);
		var hString:String = hours < 10 ? "0" + hours : "" + hours;	
		var mString:String = minutes < 10 ? "0" + minutes : "" + minutes;
		var sString:String = seconds < 10 ? "0" + seconds : "" + seconds;
		if (time < 0 || isNaN(time)) { return "00:00"; }
		if (hours > 0) { return hString + ":" + mString + ":" + sString; }
		else { return mString + ":" + sString; }
	}

/////////////////////* Play/Stop Handlers *////////////////////////
	
	/* Handle 'Play/Pause' button presses */
	private function handlePlay():Void {
		if (!bPaused && !bStopped) {
			pauseVideo(true);
			bVideoWasPaused = true;
			bPlaying = false;
		}
		else if (bPaused) {
			pauseVideo(false);
			bVideoWasPaused = false;
			bPlaying = true;
		}
		else if (bStopped) {
			bStopped = false;
			bPlaying = true;
			playVideo();
		}
	}
	
	/* Handel 'Stop' button presses */
	private function handleStop():Void {
		clearInterval(intervalID);
		clearInterval(rIntervalID);
		ns.close();
		bStopped = true;
		bPaused = false;
		controlBar.videoTime.text = "00:00";
		controlBar.seekProgress.setProgress(0,100);
		controlBar.seekSlider.value = 0;
		controlBar.seekSlider.disabled = controlBar.rewindBtn.disabled = controlBar.fastForwardBtn.disabled = controlBar.stopBtn.disabled = true;
		controlBar.playBtn.selected = controlBar.playBtn.disabled = controlBar.fastForwardBtn.disabled = false;
	}
	
	/* Handle 'Loop' button presses */
	private function handleLoop():Void {
		bLooping = !bLooping;
	}
	
/////////////////////* Que Point Seek (Rewind & Fastforward) Handlers *////////////////////////
	
	/* Handle 'Rewind' button preses */
	private function handleRewindPress():Void {
		getNextCuePoint();			
		seekToCuePoint("reverse");
		rIntervalID = setInterval(this, "rewind", 200); 
	}
	
	/* Run this function every 200 milliseocnds as long as the rewind button is held down */
	private function rewind():Void {
		pauseVideo(true);
		getNextCuePoint();
		seekToCuePoint("reverse");
	}
	
	/* Handle 'Fast Forward' button presses */
	private function handleFastForwardPress():Void {
		getNextCuePoint();
		seekToCuePoint("forward");
		rIntervalID = setInterval(this, "fastForward", 200); }
	
	/* Run this function every 200 milliseocnds as long as the fast forward button is held down */
	private function fastForward():Void {
		pauseVideo(true);
		getNextCuePoint();
		seekToCuePoint("forward");
	}
	
	/* Once the rewind or fast forward button is released, return to normal play */
	private function handleNavRelease():Void {
		clearInterval(rIntervalID);
		pauseVideo(!bPlaying);
	}
	
	/* Handle time seek slider changes */
	private function handleSeekChange(event:Object):Void {
		if (!bStopped) {
			var adjustedValue:Number = controlBar.seekSlider.value;
			adjustedValue *= 10;
			var sliderRatio:Number = 100 / adjustedValue;
			var newTime:Number = videoDuration / sliderRatio;
			ns.seek(newTime);
			controlBar.seekProgress.setProgress(newTime,videoDuration);
		}
	}
	
	/* Handle seek slider thumb control presses */
	private function handleThumbPress(event:Object):Void {
		pauseVideo(true);
		bSeekDrag = true;
	}
	
	/* Handle seek slider thumb control releases */
	private function handleThumbRelease(event:Object):Void {
		if (bVideoWasPaused == false) { pauseVideo(false); }
		bSeekDrag = false;
	}	
	
/////////////////////* Video Area Background Color/Alpha Handlers *////////////////////////

	/* Handle background color selection */
	private function handleBGColorChange():Void {
		setBGColor();
		videoBg._visible = true;
		bTransparent = checkerBg._visible = controlBar.alphaBtn.selected = false;
	}
	
	/* Set the background color */
	private function setBGColor():Void {
		var bgColor:String = controlBar.colorPicker.getColor();
		var colorchange = new Color(videoBg);  
		colorchange.setRGB(bgColor);  
	}
	
	/* Handle alpha (transparency) toggle */
	private function handleBGAlpha():Void {
		videoBg._visible = bTransparent;
		bTransparent = checkerBg._visible = !bTransparent;
	}
	
/////////////////////* Window & Video Size Handlers *////////////////////////
	
	/* Handle window maximize */
	private function handleMaximize():Void {
		if(bMaximized) {
			fscommand("restore");
			bMaximized = false;
		}
		else {
			fscommand("maximize");
			bMaximized = true;
		}
	}
	
	/* Handle video resize button presses */
	private function handleVideoResize():Void {
		bFullScreen = !bFullScreen
		setResolution();
	}

	/* Handle stage resize events */
	private function onResize():Void {
		var w = Stage.width;
		var h = Stage.height;
		setSize(w, h);
		videoArea = Stage.height - controlBarBg._height;
		subtitle._x = controlBar._x = (Stage.width / 2);
		setResolution();
		subtitle._y = video._y + video._height - subtitle._height - 10;
		invalidate();
	}
	
/////////////////////* Audio & Subtitle Handlers *////////////////////////
	
	/* Handle enable/disable mute button presses */
	private function handleMute():Void {
		videoAudio.setVolume((bMuted) ? currentVolume : 0);
		controlBar.volumeSlider.disabled = !bMuted;
		if (bMuted) { controlBar.volumeSlider.value = currentVolume / 10; }
		controlBar.volumeProgress.setProgress(((bMuted) ? currentVolume : 0),100);
		bMuted = !bMuted;
	}
	
	/* Handle change volume using volume slider */
	private function handleVolumeChange(event:Object):Void {
		var v:Number = controlBar.volumeSlider.value;
		currentVolume = v * 10;
		controlBar.volumeProgress.setProgress(currentVolume,100)
		if (!bMuted) { videoAudio.setVolume(currentVolume); }
	}
	
	/* Handle change audio track option stepper */
	private function handleAudioChannels(event:Object):Void {
		if(curAudioTrack != undefined) { 
			curAudioTrack = event.target.selectedItem;
			ns.audioTrack = curAudioTrack - 1;
			videoAudio.setVolume(currentVolume);
		}
		else {
			curAudioTrack = 0;
			controlBar.audioChannels.selectedIndex = curAudioTrack + 1;
		}
	}
	
	/* Handle change subtitle track option stepper */
	private function handleSubtitleTracks(event:Object):Void {
		curSubtitleTrack = ns.subtitleTrack = (event.target.selectedItem == "No Subs") ? 0 : event.target.selectedItem;
		subtitle.text = "";
	}
	
/////////////////////* Video Load & Status Handlers *////////////////////////
	
	/* Execute this function when a video file is loaded using the text input field & load button */
	private function handleVideoLoad():Void {
		curAudioTrack = null;
		controlBar.subtitleTracks.selectedIndex = 0;
		setVideo(controlBar.videoTitle.text);
	}

	/* Replay the video if looping is turned on, otherwise stop the video */
	private function onStatus(infoObject:Object):Void {
		var level = infoObject["level"];
		var code = infoObject["code"];
		if (level == "status") {
			if (code == "NetStream.Play.Stop") {
				if (bLooping == true) { playVideo(); }
				else { handleStop(); }
			}
			if (code == "NetStream.Play.StreamNotFound") {
				controlBar.playBtn.disabled = controlBar.fastForwardBtn.disabled = controlBar.videoSizeBtn.disabled = true;
				video._visible = false;
				subtitle.text = "Invalid Filename";
				bFullScreen = true;
				setResolution();
				handleStop();
			}
		}
	}
}