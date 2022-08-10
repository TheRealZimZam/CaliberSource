//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_suitpower.h"
#include "hud_macros.h"
#include "c_basehlplayer.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudSuitPower );

#define SUITPOWER_INIT -1
#define SOLID_BAR 0

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudSuitPower::CHudSuitPower( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudSuitPower" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudSuitPower::Init( void )
{
	m_flSuitPower = SUITPOWER_INIT;
	m_nSuitPowerLow = -1;
	m_iActiveSuitDevices = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudSuitPower::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudSuitPower::ShouldDraw()
{
	bool bNeedsDraw = false;

	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	// needs draw if suit power changed or animation in progress
	bNeedsDraw = ( ( pPlayer->m_HL2Local.m_flSuitPower != m_flSuitPower ) || ( m_AuxPowerColor[3] > 0 ) );

	return ( bNeedsDraw && CHudElement::ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudSuitPower::OnThink( void )
{
	float flCurrentPower = 0;
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	flCurrentPower = pPlayer->m_HL2Local.m_flSuitPower;

	// Only update if we've changed suit power
	if ( flCurrentPower == m_flSuitPower )
		return;

	if ( flCurrentPower >= 100.0f && m_flSuitPower < 100.0f )
	{
		// we've reached max power
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerMax");
	}
	else if ( flCurrentPower < 100.0f && (m_flSuitPower >= 100.0f || m_flSuitPower == SUITPOWER_INIT) )
	{
		// we've lost power
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerNotMax");
	}

	bool flashlightActive = pPlayer->IsFlashlightActive();
	bool sprintActive = pPlayer->IsSprinting();
	bool breatherActive = pPlayer->IsBreatherActive();
	int activeDevices = (int)flashlightActive + (int)sprintActive + (int)breatherActive;

	if (activeDevices != m_iActiveSuitDevices)
	{
		m_iActiveSuitDevices = activeDevices;

		switch ( m_iActiveSuitDevices )
		{
		default:
		case 3:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerThreeItemsActive");
			break;
		case 2:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerTwoItemsActive");
			break;
		case 1:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerOneItemActive");
			break;
		case 0:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerNoItemsActive");
			break;
		}
	}

	m_flSuitPower = flCurrentPower;
}

//-----------------------------------------------------------------------------
// Purpose: draws the power bar
//-----------------------------------------------------------------------------
void CHudSuitPower::Paint()
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	int		r, g, b, a;
	int		xpos = m_flBarInsetX, ypos = m_flBarInsetY;
	Color	clrBar;

	if ( !icon_bar )
	{
		icon_bar = gHUD.GetIcon( "progress_bar" );
		icon_bar_empty = gHUD.GetIcon( "empty" );
	}

	if ( !icon_suit )
	{
		icon_suit = gHUD.GetIcon( "suit_full" );
	}

	if ( !icon_suit_empty )
	{
		icon_suit_empty = gHUD.GetIcon( "suit_empty" );
	}

	if ( !icon_bar )
		return;

	if ( pPlayer->m_HL2Local.m_flSuitPower < 20 )
	{
		(gHUD.m_clrCaution).GetColor( r, g, b, a );
	}
	else
	{
		(gHUD.m_clrNormal).GetColor( r, g, b, a );
	}
	clrBar.SetColor( r, g, b, a );

#if SOLID_BAR
	// Just draw a solid progress bar
	gHUD.DrawIconProgressBar( xpos, ypos, m_flBarWidth, m_flBarHeight, icon_bar, icon_bar_empty, m_flSuitPower, clrBar, CHud::HUDPB_HORIZONTAL );
#else
	// get bar chunks
	int chunkCount = m_flBarWidth / (m_flBarChunkWidth + m_flBarChunkGap);
	int enabledChunks = (int)((float)chunkCount * (m_flSuitPower * 1.0f/100.0f) + 0.5f );

	// see if we've changed power state
	int lowPower = 0;
	if (enabledChunks <= (chunkCount / 4))
	{
		lowPower = 1;
	}
	if (m_nSuitPowerLow != lowPower)
	{
		if (m_iActiveSuitDevices || m_flSuitPower < 100.0f)
		{
			if (lowPower)
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerDecreasedBelow25");
			}
			else
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerIncreasedAbove25");
			}
			m_nSuitPowerLow = lowPower;
		}
	}

	// Draw the chunks
	for (int i = 0; i < enabledChunks; i++)
	{
		icon_bar->DrawSelf( xpos, ypos, m_flBarChunkWidth, m_flBarHeight, clrBar );

		xpos += (m_flBarChunkWidth + m_flBarChunkGap);
	}
#endif
	// Draw the suit icon


	// draw our name
	surface()->DrawSetTextFont(m_hTextFont);
	surface()->DrawSetTextColor(m_AuxPowerColor);
	surface()->DrawSetTextPos(text_xpos, text_ypos);

	wchar_t *tempString = g_pVGuiLocalize->Find("#Valve_Hud_AUX_POWER");

	if (tempString)
	{
		surface()->DrawPrintText(tempString, wcslen(tempString));
	}
	else
	{
		surface()->DrawPrintText(L"AUX POWER", wcslen(L"AUX POWER"));
	}

	if ( m_iActiveSuitDevices )
	{
		// draw the additional text
		int ypos = text2_ypos;

		if (pPlayer->IsBreatherActive())
		{
			tempString = g_pVGuiLocalize->Find("#Valve_Hud_OXYGEN");

			surface()->DrawSetTextPos(text2_xpos, ypos);

			if (tempString)
			{
				surface()->DrawPrintText(tempString, wcslen(tempString));
			}
			else
			{
				surface()->DrawPrintText(L"OXYGEN", wcslen(L"OXYGEN"));
			}
			ypos += text2_gap;
		}

		if (pPlayer->IsFlashlightActive())
		{
			tempString = g_pVGuiLocalize->Find("#Valve_Hud_FLASHLIGHT");

			surface()->DrawSetTextPos(text2_xpos, ypos);

			if (tempString)
			{
				surface()->DrawPrintText(tempString, wcslen(tempString));
			}
			else
			{
				surface()->DrawPrintText(L"FLASHLIGHT", wcslen(L"FLASHLIGHT"));
			}
			ypos += text2_gap;
		}

		if (pPlayer->IsSprinting())
		{
			tempString = g_pVGuiLocalize->Find("#Valve_Hud_SPRINT");

			surface()->DrawSetTextPos(text2_xpos, ypos);

			if (tempString)
			{
				surface()->DrawPrintText(tempString, wcslen(tempString));
			}
			else
			{
				surface()->DrawPrintText(L"SPRINT", wcslen(L"SPRINT"));
			}
			ypos += text2_gap;
		}
	}
}

