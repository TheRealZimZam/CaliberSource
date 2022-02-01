//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "hudelement.h"
#include "hud_numericdisplay.h"
#include <vgui_controls/Panel.h>
#include "cbase.h"
#include "hud.h"
#include "hud_suitpower.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>
#include "c_basehlplayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MIN_ALPHA	100

//-----------------------------------------------------------------------------
// Purpose: Shows the flashlight icon
//-----------------------------------------------------------------------------
class CHudFlashlight : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFlashlight, vgui::Panel );

public:
	CHudFlashlight( const char *pElementName );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

protected:
	virtual void Paint();

private:
	void SetFlashlightState( bool flashlightOn );
	
	bool	m_bFlashlightOn;
	CHudTexture		*icon_flash_empty;
	CHudTexture		*icon_flash_full;
	CHudTexture		*icon_flash_beam;
};	

using namespace vgui;

DECLARE_HUDELEMENT( CHudFlashlight );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudFlashlight::CHudFlashlight( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudFlashlight" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pScheme - 
//-----------------------------------------------------------------------------
void CHudFlashlight::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudFlashlight::SetFlashlightState( bool flashlightOn )
{
	if ( m_bFlashlightOn == flashlightOn )
		return;

	m_bFlashlightOn = flashlightOn;
}

//-----------------------------------------------------------------------------
// Purpose: draws the flashlight icon
//-----------------------------------------------------------------------------
void CHudFlashlight::Paint()
{
	int		r, g, b, a, nUnused;
	int		x, y;
	bool	bIsOn;
	Color	clrFlash;

	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	if ( !icon_flash_empty )
	{
		icon_flash_empty = gHUD.GetIcon( "flash_empty" );
	}

	if ( !icon_flash_full )
	{
		icon_flash_full = gHUD.GetIcon( "flash_full" );
	}

	if ( !icon_flash_beam )
	{
		icon_flash_beam = gHUD.GetIcon( "flash_beam" );
	}

	if ( !icon_flash_empty || !icon_flash_full || !icon_flash_beam )
	{
		return;
	}

	bIsOn = pPlayer->IsEffectActive( EF_DIMLIGHT );

	if ( bIsOn )
		a = 225;
	else
		a = MIN_ALPHA;

	if ( pPlayer->m_HL2Local.m_flFlashBattery < 20 )
	{
		(gHUD.m_clrCaution).GetColor( r, g, b, nUnused );
	}
	else
	{
		(gHUD.m_clrNormal).GetColor( r, g, b, nUnused );
	}

	clrFlash.SetColor( r, g, b, a );

	y = icon_flash_empty->Height() / 2;
	x = GetWide() - ( icon_flash_empty->Width() * 1.5 );

	// Draw the flashlight casing
	icon_flash_empty->DrawSelf( x, y, clrFlash );

	if ( bIsOn )
	{  // draw the flashlight beam
		x = GetWide() - icon_flash_empty->Width() / 2;

		icon_flash_beam->DrawSelf( x, y, clrFlash );
	}

	// draw the flashlight energy level
	x = GetWide() - ( icon_flash_empty->Width() * 1.5 );

	int nOffset = icon_flash_empty->Width() * ( 1.0 - ( (float)pPlayer->m_HL2Local.m_flFlashBattery / 100.0 ) );
	if ( nOffset < icon_flash_empty->Width() )
	{
		icon_flash_full->DrawSelfCropped( x + nOffset, y, nOffset, 0, icon_flash_full->Width() - nOffset, icon_flash_full->Height(), clrFlash );
	}
}
