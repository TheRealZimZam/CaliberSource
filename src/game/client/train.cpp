//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// Train.cpp
//
// implementation of CHudTrain class
//
#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "materialsystem/IMaterialSystem.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialVar.h"
#include <vgui_controls/controls.h>
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "hud_bitmapnumericdisplay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

class CHudTrain: public CHudElement, public vgui::Panel /*, public CHudBitmapNumericDisplay */
{
	DECLARE_CLASS_SIMPLE( CHudTrain, vgui::Panel );
public:
	CHudTrain( const char *pElementName );
	void Init( void );
	void VidInit( void );
	bool ShouldDraw( void );
	void MsgFunc_Train(bf_read &msg);

public:
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint( void );

private:
	int m_iPos;
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
CHudTrain::CHudTrain( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "HudTrain" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_MISCSTATUS );
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTrain::VidInit(void)
{
//	BaseClass::VidInit();
	m_iPos = 0;

	CHudElement::Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudTrain::ShouldDraw( void )
{
	bool bNeedsDraw = ( m_iPos != 0 );

	return ( bNeedsDraw && CHudElement::ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTrain::Paint()
{
	// FIXME:  Rewrite using vgui materials if we still do this type of train UI!!!

#if 0	//HL2_CLIENT_DLL
	if ( !icon_train )
	{
		icon_train = gHUD.GetIcon( "train" );
	}

	if ( !icon_train )
		return;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	int		y = ScreenHeight()/2;
	int		x = ScreenWidth()/2;

/*
	int nHudElemWidth, nHudElemHeight;
	GetSize( nHudElemWidth, nHudElemHeight );

	// This should show up to the right and part way up the armor number
	y = nHudElemHeight - icon_train->Height() * 2; // - m_pNumbers[ 0 ]->Height();
	x = nHudElemWidth / 3 + icon_train->Width() / 4;
*/

	IMaterial *material = materials->FindMaterial( icon_train->szTextureFile, TEXTURE_GROUP_VGUI );
	if ( material )
	{
		bool found;
		IMaterialVar* pFrameVar = material->FindVar( "$frame", &found, false );
		if ( found )
		{
			pFrameVar->SetIntValue( m_iPos - 1 );
		}
	}

	icon_train->DrawSelf( x, y, GetFgColor() );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTrain::MsgFunc_Train( bf_read &msg )
{
	// update Train data
	m_iPos = msg.ReadByte();

/*
	if (m_iPos)
		m_iFlags |= HUD_ACTIVE;
	else
		m_iFlags &= ~HUD_ACTIVE;
*/
}
