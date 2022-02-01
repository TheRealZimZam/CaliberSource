//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Shows devbuild icon
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

#include <vgui/ILocalize.h>

using namespace vgui;

#include "hudelement.h"

#include "ConVar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define DEV_MATERIAL			"sprites/dev"
extern ConVar developer;

//-----------------------------------------------------------------------------
// Purpose: Health panel
//-----------------------------------------------------------------------------
class CHudDev : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudDev, vgui::Panel );

public:
	CHudDev( const char *pElementName );
	virtual void VidInit( void );
	bool ShouldDraw();

protected:
	virtual void Paint();

private:
	int m_textureID_IconDev;
};	

DECLARE_HUDELEMENT( CHudDev );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudDev::CHudDev( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudDev" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );

	m_textureID_IconDev = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_textureID_IconDev, DEV_MATERIAL, true, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDev::VidInit()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudDev::ShouldDraw( void )
{
	if ( !developer.GetBool() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudDev::Paint()
{
	SetPaintBorderEnabled(false);
	vgui::surface()->DrawSetTexture( m_textureID_IconDev );
	vgui::surface()->DrawTexturedRect( 2, 2, 64, 64 );
}
