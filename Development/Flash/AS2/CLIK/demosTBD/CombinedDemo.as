/**
 * This sample displays the majority of the core components in a combined demo.
 */
 
import gfx.controls.Button;
import gfx.controls.CheckBox;
import gfx.controls.OptionStepper;
import gfx.controls.StatusIndicator
import gfx.controls.ScrollBar;
import gfx.controls.Slider;
import gfx.controls.UILoader;
import gfx.controls.TextArea;
import gfx.controls.TextInput;
import gfx.controls.ProgressBar;
import gfx.controls.Label;
import gfx.controls.RadioButton;
import gfx.controls.ScrollingList;

import gfx.utils.Debugger;

class CombinedDemo extends MovieClip {
	
	public var healthBar:StatusIndicator;
	public var enemyHealthBar:ProgressBar;
	public var debugger:Debugger;
	public var healthBtn:Button;
	public var shootBtn:Button;
	public var r1:RadioButton;
	public var r2:RadioButton;
	public var r3:RadioButton;
	public var r4:RadioButton;
	public var weaponImage:UILoader;
	public var weaponList:OptionStepper;
	public var weaponSummary:TextArea;
	public var weapons:Array;
	public var scroller:ScrollBar;
	public var player:Label;
	public var playerImage:UILoader;
	public var enableWeapon:CheckBox;
	public var sb:ScrollBar;
	public var list:ScrollingList;
	public var damage:Number;
	public var totalDamage:Number = 0;
	public var slider:Slider;
	public var header:MovieClip;
	public var changeName:TextInput;
	
	public var selectedWeapon:Object = {};
	
	public function CombinedDemo() { }
	
	private function onLoad():Void { configUI(); }
	
	private function configUI():Void {
		
		enemyHealthBar.mode = 'manual';
		healthBtn.addEventListener("click", this, "onHealthClicked");
		weaponList.addEventListener("change",this, "onWeaponsChange");
		shootBtn.addEventListener("click",this, "onShootGun");
		enableWeapon.addEventListener("click",this, "onEnableWeapon");
		slider.addEventListener("change",this,"onSlide");
		changeName.addEventListener("textChange",this,"onNameChange");
		
		healthBar.minimum = 0;
		healthBar.maximum = 30;
		healthBar.value = 0;
		
		weapons = [{label:'SOCOM',damage:1,image:'images/socom.jpg',desc:'The Mk.23 SOCOM is an offensive handgun weapon system and is the basis for the Heckler & Kochs USP series of handguns. The SOCOM is chambered for the powerful .45 ACP cartridge and has a 12 round capacity. It is usually also equipped with a Laser Aiming Module (LAM) and a sound suppressor.'},
				   {label:'SAA',damage:5,image:'images/saa.jpg',desc:'The Colt Single Action Army Revolver is a powerful single action revolver holding six rounds of .45 Long Colt ammunition. The Single Action Army was designed for the US cavalry by Colts Manufacturing Company and adopted in 1873.'},
				   {label:'M1911A1',damage:10,image:'images/M1911A1.jpg',desc:'The M1911A1 is a semi-automatic handgun used by Naked Snake during Operation Snake Eater. It was used extensively from World War I through the Vietnam War, and is still in service with some United States military units to this day, as well as many law enforcement agencies.'},
				   {label:'Makarov PMM',damage:10,image:'images/MakarovPM.jpg',desc:'The Soviet Makarov Pistol (PM or Pistolet Makarovka) was the standard-issue handgun equipped to Soviet Russian forces, designed along the lines of the Walther PP, and shares a similar design. The Makarov is produced in Russia (Iszmash or Baikal), East Germany (Ernst Thaelman Factory), China (Norinco), and Bulgaria (State Arsenals), with varying quality. Most modern-era Makarov pistols are from Bulgaria.'},
				   {label:'M9',damage:20,image:'images/M9.jpg',desc:'The Beretta M92F is a semi automatic pistol, the standard-issue sidearm of the U.S Armed forces. It replaced the venerable M1911A1 in the 1980s, where it was designated the M9 Pistol. The M9 is chambered for the 9 x 19mm Parabellum cartridge; while not as powerful as the M1911 it replaced, the M9 holds 15 rounds per magazine, more than twice the capacity of the 1911s.'},
				   {label:'Mk22',damage:25,image:'images/Mk22.jpg',desc:'The Mk.22 Mod 0 "Hush Puppy" is a Smith & Wesson Model 39 modified for military purposes. It is a mil-spec pistol firing the 9x19mm Parabellum cartridge and is capable of attaching a silencer. The Mk.22 was used by special forces units during the Vietnam War; regular troops were generally issued the older and more powerful M1911A1 as a sidearm.'},
				   {label:'Famas',damage:20,image:'images/Famas.jpg',desc:'The FAMAS (French: Fusil dAssaut de la Manufacture dArmes de Saint-Étienne, Assault Rifle by St-Étienne Arms Factory) is an assault rifle firing the 5.56 x 45mm NATO cartridge from a 25-round magazine. It is the standard-issue weapon for the French military and the Genome Army. Its distinctive shape has earned it the affectionate nickname "Le Clairon" (Trumpet) among troops.'}
				   ];
		list.dataProvider = ['The Pain', 'The Fear', 'The End','Granin', 'The Fury', 'Colonel Volgin','The Boss'];
		list.scrollBar = sb;
		weaponList.dataProvider = weapons;
		this.onEnterFrame = function() {
			delete this.onEnterFrame;
			loadWeapon();
		}
	}
	
	//Handlers
	private function onHealthClicked(evtObj:Object):Void {
		var position:Number = Math.random()*100 + 1 >> 0;
		healthBar.value = position;
	}
	
	private function onEnableWeapon(evtObj:Object):Void {
		shootBtn.disabled = !shootBtn.disabled;
	}
	
	private function onNameChange(evtObj:Object):Void {
		player.text = changeName.text;
	}
	
	private function onShootGun(evtObj:Object):Void {
		if (!damage) { return; }
		var shot:Sound = new Sound(this);
		shot.attachSound("Shot");
		shot.start();
		
		totalDamage += damage;
		enemyHealthBar.setProgress(totalDamage,120);
		if (totalDamage >=120) {
			//SD: This is only for testing... 
			totalDamage = 0;
			this.onEnterFrame = function() {
				enemyHealthBar.prevFrame();
				shootBtn.disabled = true;
				if (enemyHealthBar._currentframe == 1) { 
					delete this.onEnterFrame; 
					shootBtn.disabled = false;
				}
			}
		}
	}
	
	private function loadWeapon():Void {
		var image = weapons[weaponList.selectedIndex].image;
		var desc:String = weapons[weaponList.selectedIndex].desc;
		weaponImage.source = image;
		weaponSummary.text = desc;
		selectedWeapon = weapons[weaponList.selectedIndex];
		damage = selectedWeapon.damage;
		r1.selected = (weaponList.selectedItem.label == r1.label);
	}
	
	private function onSlide(evtObj:Object):Void {
		header._alpha = slider.value;
	}
	
	private function onWeaponsChange(evtObj:Object):Void {
		r1.selected = (weaponList.selectedItem.label == r1.label);
		r2.selected = (weaponList.selectedItem.label == r2.label);
		r3.selected = (weaponList.selectedItem.label == r3.label);
		r4.selected = (weaponList.selectedItem.label == r4.label);
		
		var image:String = weaponList.selectedItem.image;
		var desc:String = weaponList.selectedItem.desc;
		weaponImage.source = image;
		weaponSummary.text = desc;
		selectedWeapon = weaponList.selectedItem;
		damage = selectedWeapon.damage;
		Debugger.trace(weaponList.selectedItem.label)
	}
}