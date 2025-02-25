//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Train GUI
// Shows direction with bitmaps, speed with numbers
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"
#include "iclientmode.h"
#include "materialsystem/IMaterialSystem.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialVar.h"
#include <vgui_controls/controls.h>
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "hudelement.h"
#include "hud_bitmapnumericdisplay.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHudTrain: public CHudBitmapNumericDisplay, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudTrain, CHudBitmapNumericDisplay );
public:
	CHudTrain( const char *pElementName );
	void	Init( void );
	void	VidInit( void );
	bool	ShouldDraw( void );
	void	MsgFunc_Train(bf_read &msg);

private:
	void	Paint( void );
	void	ApplySchemeSettings( vgui::IScheme *scheme );

private:
	int			m_iPos;
	CHudTexture	*icon_train;
};

//
//-----------------------------------------------------
//

DECLARE_HUDELEMENT( CHudTrain );
DECLARE_HUD_MESSAGE( CHudTrain, Train )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudTrain::CHudTrain( const char *pElementName ) : CHudElement( pElementName ), CHudBitmapNumericDisplay(NULL, "HudTrain")
{
	SetHiddenBits( HIDEHUD_MISCSTATUS | HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scheme - 
//-----------------------------------------------------------------------------
void CHudTrain::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTrain::Init(void)
{
	HOOK_HUD_MESSAGE( CHudTrain, Train );

	m_iPos = 0;
}

void CHudTrain::VidInit(void)
{
//	BaseClass::VidInit();

	m_iPos = 0;

	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudTrain::ShouldDraw( void )
{
	return ( CHudElement::ShouldDraw() && m_iPos );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTrain::Paint()
{
	if ( !icon_train )
	{
		icon_train = gHUD.GetIcon( "train" );
	}

	if ( !icon_train )
		return;

	int		r, g, b, a;
	int		x, y;
	Color	clrTrain;

	(gHUD.m_clrYellowish).GetColor( r, g, b, a );
	clrTrain.SetColor( r, g, b, a );

	int nHudElemWidth, nHudElemHeight;
	GetSize( nHudElemWidth, nHudElemHeight );

	// This should show up to the right and part way up the armor number
	y = nHudElemHeight - icon_train->Height() - GetNumberFontHeight();
	x = nHudElemWidth / 3 + icon_train->Width() / 4;

	IMaterial *material = materials->FindMaterial( icon_train->szTextureFile, TEXTURE_GROUP_VGUI );
	if ( material )
	{
		bool found;
		IMaterialVar* pFrameVar = material->FindVar( "$frame", &found, false );
		if ( found )
		{
			pFrameVar->SetFloatValue( m_iPos - 1 );
		}
	}

	icon_train->DrawSelf( x, y, clrTrain );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTrain::MsgFunc_Train( bf_read &msg )
{
	m_iPos = msg.ReadByte();
}
