"Resource/UI/weaponselect_menu/HudMenuWeaponIcon.res"
{
	"MainBackground"	
	{
		"ControlName"	"CIconPanel"
		"fieldName"		"MainBackground"
		"xpos"			"-10"
		"ypos"			"-10"
		"zpos"			"0"
		"wide"			"150"
		"tall"			"150"
		"visible"		"0"
		"enabled"		"1"
		"scaleImage"	"1"	
		"icon"			"hud_menu_bg"
		"iconColor"		"255 255 255 255"
	}
	"ActiveWeapon"
	{
		"ControlName"	"CTFImagePanel"
		"fieldName"		"ActiveWeapon"
		"xpos"			"0"
		"ypos"			"10"
		"zpos"			"1"		
		"wide"			"160"
		"tall"			"80"
		"visible"		"0"
		"enabled"		"1"
		"image"			"../hud/weapon_bucket_select_red"
		"scaleImage"	"1"	
		"teambg_2"		"../hud/weapon_bucket_select_red"
		"teambg_3"		"../hud/weapon_bucket_select_blue"	
	}	
	"WeaponBucket"
	{
		"ControlName"	"CTFImagePanel"
		"fieldName"		"WeaponBucket"
		"xpos"			"10"
		"ypos"			"20"
		"xpos"			"0"	[$X360]
		"ypos"			"0"	[$X360]
		"zpos"			"2"
		"wide"			"120"
		"tall"			"60"
		"visible"		"1"
		"enabled"		"1"
		"image"			""
		"scaleImage"	"1"

		"if_minmode"
		{
			"xpos"	"0"
			"ypos"	"0"
			"wide"	"37"
			"tall"	"37"
		}
	}
	"WeaponLabel"
	{
		"ControlName"	"CExLabel"
		"fieldName"		"WeaponLabel"
		"xpos"			"25"
		"ypos"			"70"
		"zpos"			"2"
		"wide"			"100"
		"tall"			"15"
		"autoResize"	"1"
		"pinCorner"		"2"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"	"0"
		"labelText"		""
		"textAlignment"	"center"
		"dulltext"		"0"
		"brighttext"	"0"
		"font"			"Default"

		"if_minmode"
		{
			"visible"		"0"
		}
	}
	"WeaponNumber"
	{
		"ControlName"	"CExLabel"
		"fieldName"		"WeaponNumber"
		"xpos"			"43"
		"ypos"			"22"
		"zpos"			"3"
		"wide"			"100"
		"tall"			"15"
		"autoResize"	"1"
		"pinCorner"		"2"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"	"0"
		"labelText"		""
		"textAlignment"	"center"
		"dulltext"		"0"
		"brighttext"	"0"
		"font"			"HudFontSmallest"

		"if_minmode"
		{
			"visible"		"0"
		}
	}
	"WeaponLabel"
	{
		"ControlName"	"CExLabel"
		"fieldName"		"WeaponLabel"
		"xpos"			"28"
		"ypos"			"67"
		"zpos"			"3"
		"wide"			"100"
		"tall"			"15"
		"autoResize"	"1"
		"pinCorner"		"2"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"	"0"
		"labelText"		""
		"textAlignment"	"center"
		"dulltext"		"0"
		"brighttext"	"0"
		"font"			"Default"

		"if_minmode"
		{
			"visible"		"0"
		}
	}
}