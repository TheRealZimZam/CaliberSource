//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include <vgui_controls/controls.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudLogo: public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudLogo, vgui::Panel );
public:
	CHudLogo( const char *pElementName );
	void Init( void );
	void VidInit( void );
	bool ShouldDraw( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint( void );
	void MsgFunc_Logo(bf_read &msg);
	
private:
	int	m_iLogo;
	CHudTexture		*icon_logo; 

};

DECLARE_HUDELEMENT( CHudLogo );
DECLARE_HUD_MESSAGE( CHudLogo, Logo );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudLogo::CHudLogo( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "HudLogo" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	icon_logo = NULL;

	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

void CHudLogo::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudLogo::Init(void)
{
	HOOK_HUD_MESSAGE( CHudLogo, Logo );

	m_iLogo = 0;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudLogo::VidInit(void)
{
	icon_logo = gHUD.GetIcon( "logo" );
	m_iLogo = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudLogo::MsgFunc_Logo( bf_read &msg )
{
	m_iLogo = msg.ReadByte();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudLogo::ShouldDraw( void )
{
	return ( m_iLogo > 0 && CHudElement::ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudLogo::Paint()
{
	if ( !icon_logo )
		return;

	int y = icon_logo->Height();
	int x = icon_logo->Width();

	icon_logo->DrawSelf( x, y, GetFgColor() );
}