//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HUD_BITMAPNUMERICDISPLAY_H
#define HUD_BITMAPNUMERICDISPLAY_H
#ifdef _WIN32
#pragma once
#endif

#include "hud_numericdisplay.h"
#include <vgui_controls/Panel.h>

//-----------------------------------------------------------------------------
// Purpose: Base class for all the hud elements that are just a numeric display
//			with some options for text and icons
//-----------------------------------------------------------------------------
class CHudBitmapNumericDisplay : public vgui::Panel
{	
	DECLARE_CLASS_SIMPLE( CHudBitmapNumericDisplay, vgui::Panel );

public:
	CHudBitmapNumericDisplay(vgui::Panel *parent, const char *name);

	void SetDisplayValue(int value);
	void SetSecondaryValue(int value);
	void SetShouldDisplayValue(bool state);
	void SetShouldDisplaySecondaryValue(bool state);
	void SetIsAmmo(bool state);
	void SetNumZeros(int value);
	void SetLabel(int label);

	bool ShouldDisplayValue( void ) { return m_bDisplayValue; }
	bool ShouldDisplaySecondaryValue( void ) { return m_bDisplaySecondaryValue; }

	virtual void Reset();

protected:
	// vgui overrides
	virtual void PaintBackground( void );
	virtual void Paint();
	void PaintLabel();
	void PaintAmmoBar();
	void PaintNumbers(int xpos, int ypos, int value, Color col, int numSigDigits);
	void PaintZeros(int xpos, int ypos, Color col, int numZeros);

	virtual void PaintNumbers(int xpos, int ypos, int value, Color col)
	{
		PaintNumbers(xpos, ypos, value, col, 1);
	}

	int m_iValue;
	int m_iSecondaryValue;
	bool m_bDisplayValue, m_bDisplaySecondaryValue;

	CPanelAnimationVar( float, m_flAlphaOverride, "Alpha", "255" );
	CPanelAnimationVar( Color, m_TextColor, "TextColor", "FgColor" );
	CPanelAnimationVar( float, m_flBlur, "Blur", "0" );
	CPanelAnimationVar( Color, m_Ammo2Color, "Ammo2Color", "FgColor" );

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudNumbers" );
	CPanelAnimationVar( vgui::HFont, m_hNumberGlowFont, "NumberGlowFont", "HudNumbersGlow" );
	CPanelAnimationVar( vgui::HFont, m_hSmallNumberFont, "SmallNumberFont", "HudNumbersSmall" );
	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "Default" );

	CPanelAnimationVarAliasType( float, text_xpos, "text_xpos", "8", "proportional_float" );
	CPanelAnimationVarAliasType( float, text_ypos, "text_ypos", "20", "proportional_float" );
	CPanelAnimationVarAliasType( float, text2_xpos, "text2_xpos", "8", "proportional_float" );
	CPanelAnimationVarAliasType( float, text2_ypos, "text2_ypos", "20", "proportional_float" );
	CPanelAnimationVarAliasType( float, digit_xpos, "digit_xpos", "50", "proportional_float" );
	CPanelAnimationVarAliasType( float, digit_ypos, "digit_ypos", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, digit2_xpos, "digit2_xpos", "98", "proportional_float" );
	CPanelAnimationVarAliasType( float, digit2_ypos, "digit2_ypos", "16", "proportional_float" );
	CPanelAnimationVarAliasType( float, digit_height, "digit_height", "16", "proportional_float" );

private:
	CHudTexture *m_pNumbers[10];

private:
	int m_iLabelType;
	int m_iNumZeros;
	bool m_bIsAmmo;
};

#endif //HUD_BITMAPNUMERICDISPLAY_H