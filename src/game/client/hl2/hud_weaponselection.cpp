//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_selection.h"
#include "iclientmode.h"
#include "ammodef.h"
#include "input.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Panel.h>

#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar hud_ws_showemptyslots( "hud_ws_showemptyslots", "0", FCVAR_ARCHIVE, "Shows slots for missing weapons when recieving weapons out of order" );
ConVar hud_ws_oldstyle( "hud_ws_oldstyle", "0", FCVAR_ARCHIVE, "Use HL1-style hud" );

//Shouldnt these be offloaded?
#define SELECTION_TIMEOUT_THRESHOLD		0.75f	// Seconds
#define SELECTION_FADEOUT_TIME			0.5f

#define FASTSWITCH_DISPLAY_TIMEOUT		1.5f
#define FASTSWITCH_FADEOUT_TIME			1.5f

#define HL2_MAX_WEAPON_SLOTS	6

//-----------------------------------------------------------------------------
// Purpose: hl2 weapon selection hud element
//-----------------------------------------------------------------------------
class CHudWeaponSelection : public CBaseHudWeaponSelection, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudWeaponSelection, vgui::Panel );

public:
	CHudWeaponSelection(const char *pElementName );

	virtual	bool ShouldDraw();

	virtual	void CycleToNextWeapon( void );
	virtual	void CycleToPrevWeapon( void );

	virtual	C_BaseCombatWeapon *GetWeaponInSlot( int iSlot, int iSlotPos );
	virtual	void SelectWeaponSlot( int iSlot );

	virtual	C_BaseCombatWeapon	*GetSelectedWeapon( void )
	{ 
		return m_hSelectedWeapon;
	}

	virtual void OpenSelection( bool bDoSound = false );
	virtual void HideSelection( void );
	
	virtual void LevelInit();
	void VidInit( void );
	C_BaseCombatWeapon *GetNextActivePos( int iSlot, int iSlotPos );

protected:
	virtual void OnThink();
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	virtual bool IsWeaponSelectable()
	{ 
		if (IsInSelectionMode())
			return true;

		return false;
	}

	virtual void SetWeaponSelected()
	{
		CBaseHudWeaponSelection::SetWeaponSelected();

		switch( hud_fastswitch.GetInt() )
		{
		case HUDTYPE_PLUS:
			ActivateWeaponHighlight( GetSelectedWeapon() );
			break;
		default:
			// do nothing
			break;
		}
	}

private:
	C_BaseCombatWeapon *FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);
	C_BaseCombatWeapon *FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);

	int GetLastPosInSlot( int iSlot ) const;

	void ActivateWeaponHighlight( C_BaseCombatWeapon *pWeapon );
	void FastWeaponSwitch( int iWeaponSlot );

	void DrawAmmoBar( C_BaseCombatWeapon *pWeapon, int x, int y, int nWidth, int nHeight );
	int DrawBar( int x, int y, int width, int height, float f );

	void SetSelectedWeapon( C_BaseCombatWeapon *pWeapon ) 
	{ 
		m_hSelectedWeapon = pWeapon;
	}

	virtual	void SetSelectedSlot( int slot ) 
	{ 
		m_iSelectedSlot = slot;
	}

	void SetSelectedSlideDir( int dir )
	{
		m_iSelectedSlideDir = dir;
	}

	CPanelAnimationVar( float, m_flBlur, "Blur", "0" );
	CPanelAnimationVar( Color, m_NumberColor, "NumberColor", "SelectionNumberFg" );
	CPanelAnimationVar( Color, m_EmptyBoxColor, "EmptyBoxColor", "SelectionEmptyBoxBg" );
	CPanelAnimationVar( Color, m_BoxColor, "BoxColor", "SelectionBoxBg" );
	CPanelAnimationVar( Color, m_SelectedBoxColor, "SelectedBoxColor", "SelectionSelectedBoxBg" );

	bool m_bFadingOut;

	// fastswitch weapon display
	struct WeaponBox_t
	{
		int m_iSlot;
		int m_iSlotPos;
   	};
	CUtlVector<WeaponBox_t>	m_WeaponBoxes;
	int						m_iSelectedWeaponBox;
	int						m_iSelectedSlideDir;
	int						m_iSelectedBoxPosition;
	int						m_iSelectedSlot;
	C_BaseCombatWeapon		*m_pLastWeapon;

	CHudTexture		*icon_buckets[ HL2_MAX_WEAPON_SLOTS ];
	CHudTexture		*icon_selection;
};

DECLARE_HUDELEMENT( CHudWeaponSelection );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudWeaponSelection::CHudWeaponSelection( const char *pElementName ) : CBaseHudWeaponSelection(pElementName), BaseClass(NULL, "HudWeaponSelection")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	m_bFadingOut = false;
}

void CHudWeaponSelection::VidInit()
{
	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	for ( int i = 0; i < HL2_MAX_WEAPON_SLOTS; i++ )
	{
		char szNumString[ 10 ];

		sprintf( szNumString, "bucket%d", i );
		icon_buckets[ i ] = gHUD.GetIcon( szNumString );
	}

	icon_selection = gHUD.GetIcon( "selection" );
	
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the panel should draw
//-----------------------------------------------------------------------------
bool CHudWeaponSelection::ShouldDraw()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		if ( IsInSelectionMode() )
		{
			HideSelection();
		}
		return false;
	}

	bool bret = CBaseHudWeaponSelection::ShouldDraw();
	if ( !bret )
		return false;

	// draw weapon selection a little longer if in fastswitch so we can see what we've selected
	if ( hud_fastswitch.GetBool() && ( gpGlobals->curtime - m_flSelectionTime ) < (FASTSWITCH_DISPLAY_TIMEOUT + FASTSWITCH_FADEOUT_TIME) )
		return true;

	return ( m_bSelectionVisible ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponSelection::LevelInit()
{
	CHudElement::LevelInit();

	m_iSelectedWeaponBox = -1;
	m_iSelectedSlideDir  = 0;
	m_pLastWeapon        = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: updates animation status
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OnThink( void )
{
	CBaseHudWeaponSelection::OnThink();

	float flSelectionTimeout = SELECTION_TIMEOUT_THRESHOLD;
	float flSelectionFadeoutTime = SELECTION_FADEOUT_TIME;
	if ( hud_fastswitch.GetBool() )
	{
		flSelectionTimeout = FASTSWITCH_DISPLAY_TIMEOUT;
		flSelectionFadeoutTime = FASTSWITCH_FADEOUT_TIME;
	}

	// Time out after awhile of inactivity
	if ( ( gpGlobals->curtime - m_flSelectionTime ) > flSelectionTimeout )
	{
		if (!m_bFadingOut)
		{
			// start fading out
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "FadeOutWeaponSelectionMenu" );
			m_bFadingOut = true;
		}
		else if ( gpGlobals->curtime - m_flSelectionTime > flSelectionTimeout + flSelectionFadeoutTime )
		{
			// finished fade, close
			CancelWeaponSelection( true );
		}
	}
	else if (m_bFadingOut)
	{
		// stop us fading out, show the animation again
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "OpenWeaponSelectionMenu" );
		m_bFadingOut = false;
	}
}

//-------------------------------------------------------------------------
// Purpose: draws the selection area
//-------------------------------------------------------------------------
void CHudWeaponSelection::Paint()
{
	if (!ShouldDraw())
		return;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	int width;
	int xpos;
	int ypos;

	// find and display our current selection
	C_BaseCombatWeapon *pSelectedWeapon = GetSelectedWeapon();
	if ( !pSelectedWeapon )
		return;

	// bucket style
	width = (MAX_WEAPON_SLOTS);
	xpos  = 0 + width;	//(GetWide() - width) / 2
	ypos  = 0;

	int iActiveSlot = (pSelectedWeapon ? pSelectedWeapon->GetSlot() : -1);

	// draw the bucket set
	// iterate over all the weapon slots
	for ( int i = 0; i < HL2_MAX_WEAPON_SLOTS; i++ )
	{
		int r1, g1, b1, a1;

		m_NumberColor.GetColor( r1, g1, b1, a1 );

		icon_buckets[ i ]->DrawSelf( xpos, ypos, Color( r1, g1, b1, 255 ) );
		
		ypos = icon_buckets[ i ]->Height() + 10;

		if ( i == iActiveSlot )
		{
			int iLastPos = GetLastPosInSlot( i );
			bool bFirstItem = true;
			width = icon_buckets[ i ]->Width();

			for (int slotpos = 0; slotpos <= iLastPos; slotpos++)
			{
				C_BaseCombatWeapon *pWeapon = GetWeaponInSlot( i, slotpos );
				if ( !pWeapon )
					continue;

				// icons use old system, drawing in screen space
				if ( pWeapon->GetSpriteActive() )
				{
					int r, g, b, a;

					m_SelectedBoxColor.GetColor( r, g, b, a );

					if (pWeapon == pSelectedWeapon)
					{
						pWeapon->GetSpriteActive()->DrawSelf( xpos, ypos, Color( r, g, b, a ) );
						if ( icon_selection )
						{
							icon_selection->DrawSelf( xpos, ypos, Color( r, g, b, a ) );
						}
					}
					else
					{
						if ( pWeapon->HasAmmo() )
						{
							m_BoxColor.GetColor( r, g, b, a );
						}
						else
						{
							m_EmptyBoxColor.GetColor( r, g, b, a );
						}

						if ( pWeapon->GetSpriteInactive() )
						{
							pWeapon->GetSpriteInactive()->DrawSelf( xpos, ypos, Color( r, g, b, a ) );
						}
					}
				}

				// Draw Ammo Bar
				DrawAmmoBar( pWeapon, xpos + 10, ypos, 20, 4 );

				// move down to the next bucket
				ypos += pWeapon->GetSpriteActive()->Height() + 5;

				if ( bFirstItem )
				{
					width		= pWeapon->GetSpriteActive()->Width();
					bFirstItem	= false;
				}
			}
		}
		else
		{
			// Draw Row of weapons.
			if ( hud_ws_oldstyle.GetBool() )
			{
				for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
				{
					int r2, g2, b2, a2;
					C_BaseCombatWeapon *pWeapon = GetWeaponInSlot( i, iPos );
					
					if ( !pWeapon )
						continue;

					if ( pWeapon->HasAmmo() )
					{
						m_BoxColor.GetColor( r2, g2, b2, a2 );
					}
					else
					{
						m_EmptyBoxColor.GetColor( r2, g2, b2, a2 );
					}

					Color clrBox( r2, g2, b2, a2 );
					vgui::surface()->DrawSetColor( clrBox );
					vgui::surface()->DrawFilledRect( xpos, ypos, xpos + icon_buckets[ i ]->Width(), ypos + icon_buckets[ i ]->Height() );

					ypos += icon_buckets[ i ]->Height() + 5;
				}
			}
			else
			{
				width = icon_buckets[ i ]->Width();
				if ( icon_selection )
				{
					// If we have a selection icon, use the size of that instead
					width = icon_selection->Width();
				}
			}
		}

		// advance position
		ypos = 0;
		xpos += width;
	}
}

void CHudWeaponSelection::DrawAmmoBar( C_BaseCombatWeapon *pWeapon, int x, int y, int nWidth, int nHeight )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer->IsSuitEquipped() )
		return;

	if ( pWeapon->GetPrimaryAmmoType() != -1 )
	{
		int nAmmoCount = pPlayer->GetAmmoCount( pWeapon->GetPrimaryAmmoType() );
		if ( !nAmmoCount )
			return;

		float flPct = (float)nAmmoCount / (float)GetAmmoDef()->MaxCarry( pWeapon->GetPrimaryAmmoType() );
		
		x = DrawBar( x, y, nWidth, nHeight, flPct );

		// Do we have secondary ammo too?
		if ( pWeapon->GetSecondaryAmmoType() != -1 )
		{
			flPct = (float)pPlayer->GetAmmoCount( pWeapon->GetSecondaryAmmoType() ) / (float)GetAmmoDef()->MaxCarry( pWeapon->GetSecondaryAmmoType() );

			x += 5; //!!!

			DrawBar( x, y, nWidth, nHeight, flPct );
		}
	}
}

int CHudWeaponSelection::DrawBar( int x, int y, int nWidth, int nHeight, float flPct )
{
	Color clrBar;
	int r, g, b, a;

	if ( flPct < 0 )
	{
		flPct = 0;
	}
	else if ( flPct > 1 )
	{
		flPct = 1;
	}

	if ( flPct )
	{
		int nBarWidth = flPct * nWidth;

		// Always show at least one pixel if we have ammo.
		if (nBarWidth <= 0)
			nBarWidth = 1;

		m_SelectedBoxColor.GetColor( r, g, b, a );

		clrBar.SetColor( r, g, b, 255  );
		vgui::surface()->DrawSetColor( clrBar );
		vgui::surface()->DrawFilledRect( x, y, x + nBarWidth, y + nHeight );

		x += nBarWidth;
		nWidth -= nBarWidth;
	}

	m_EmptyBoxColor.GetColor( r, g, b, a );

	clrBar.SetColor( r, g, b, 128 );
	vgui::surface()->DrawSetColor( clrBar );
	vgui::surface()->DrawFilledRect( x, y, x + nWidth, y + nHeight );

	return ( x + nWidth );
}

//-----------------------------------------------------------------------------
// Purpose: hud scheme settings
//-----------------------------------------------------------------------------
void CHudWeaponSelection::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);

	// set our size
	int screenWide, screenTall;
	int x, y;
	GetPos(x, y);
	GetHudSize(screenWide, screenTall);

	SetBounds( x, y, screenWide - x, screenTall - y );
}

//-----------------------------------------------------------------------------
// Purpose: Opens weapon selection control
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OpenSelection( bool bDoSound )
{
	Assert(!IsInSelectionMode());
	CBaseHudWeaponSelection::OpenSelection();

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	if ( bDoSound )
		player->EmitSound( "Player.WeaponSelectionOpen" );

	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("OpenWeaponSelectionMenu");
	m_iSelectedBoxPosition = 0;
	m_iSelectedSlot = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Closes weapon selection control immediately
//-----------------------------------------------------------------------------
void CHudWeaponSelection::HideSelection( void )
{
	CBaseHudWeaponSelection::HideSelection();

	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("CloseWeaponSelectionMenu");
	m_bFadingOut = false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the next available weapon item in the weapon selection
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition)
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return NULL;

	C_BaseCombatWeapon *pNextWeapon = NULL;

	// search all the weapons looking for the closest next
	int iLowestNextSlot = HL2_MAX_WEAPON_SLOTS;
	int iLowestNextPosition = MAX_WEAPON_POSITIONS;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
		if ( !pWeapon )
			continue;

		if ( pWeapon->CanBeSelected() )
		{
			int weaponSlot = pWeapon->GetSlot(), weaponPosition = pWeapon->GetPosition();

			// see if this weapon is further ahead in the selection list
			if ( weaponSlot > iCurrentSlot || (weaponSlot == iCurrentSlot && weaponPosition > iCurrentPosition) )
			{
				// see if this weapon is closer than the current lowest
				if ( weaponSlot < iLowestNextSlot || (weaponSlot == iLowestNextSlot && weaponPosition < iLowestNextPosition) )
				{
					iLowestNextSlot = weaponSlot;
					iLowestNextPosition = weaponPosition;
					pNextWeapon = pWeapon;
				}
			}
		}
	}

	return pNextWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the prior available weapon item in the weapon selection
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition)
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return NULL;

	C_BaseCombatWeapon *pPrevWeapon = NULL;

	// search all the weapons looking for the closest next
	int iLowestPrevSlot = -1;
	int iLowestPrevPosition = -1;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
		if ( !pWeapon )
			continue;

		if ( pWeapon->CanBeSelected() )
		{
			int weaponSlot = pWeapon->GetSlot(), weaponPosition = pWeapon->GetPosition();

			// see if this weapon is further ahead in the selection list
			if ( weaponSlot < iCurrentSlot || (weaponSlot == iCurrentSlot && weaponPosition < iCurrentPosition) )
			{
				// see if this weapon is closer than the current lowest
				if ( weaponSlot > iLowestPrevSlot || (weaponSlot == iLowestPrevSlot && weaponPosition > iLowestPrevPosition) )
				{
					iLowestPrevSlot = weaponSlot;
					iLowestPrevPosition = weaponPosition;
					pPrevWeapon = pWeapon;
				}
			}
		}
	}

	return pPrevWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Moves the selection to the next item in the menu
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToNextWeapon( void )
{
	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	m_pLastWeapon = pPlayer->GetActiveWeapon();

	C_BaseCombatWeapon *pNextWeapon = NULL;
	if ( IsInSelectionMode() )
	{
		// find the next selection spot
		C_BaseCombatWeapon *pWeapon = GetSelectedWeapon();
		if ( !pWeapon )
			return;

		pNextWeapon = FindNextWeaponInWeaponSelection( pWeapon->GetSlot(), pWeapon->GetPosition() );
	}
	else
	{
		// open selection at the current place
		pNextWeapon = pPlayer->GetActiveWeapon();
		if ( pNextWeapon )
		{
			pNextWeapon = FindNextWeaponInWeaponSelection( pNextWeapon->GetSlot(), pNextWeapon->GetPosition() );
		}
	}

	if ( !pNextWeapon )
	{
		// wrap around back to start
		pNextWeapon = FindNextWeaponInWeaponSelection(-1, -1);
	}

	if ( pNextWeapon )
	{
		SetSelectedWeapon( pNextWeapon );
		SetSelectedSlideDir( 1 );

		if( hud_fastswitch.GetInt() > 0 )
		{
			SelectWeapon();
		}
		else if ( !IsInSelectionMode() )
		{
			OpenSelection();
		}

		// Play the "cycle to next weapon" sound
		pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Moves the selection to the previous item in the menu
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToPrevWeapon( void )
{
	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	m_pLastWeapon = pPlayer->GetActiveWeapon();

	C_BaseCombatWeapon *pNextWeapon = NULL;
	if ( IsInSelectionMode() )
	{
		// find the next selection spot
		C_BaseCombatWeapon *pWeapon = GetSelectedWeapon();
		if ( !pWeapon )
			return;

		pNextWeapon = FindPrevWeaponInWeaponSelection( pWeapon->GetSlot(), pWeapon->GetPosition() );
	}
	else
	{
		// open selection at the current place
		pNextWeapon = pPlayer->GetActiveWeapon();
		if ( pNextWeapon )
		{
			pNextWeapon = FindPrevWeaponInWeaponSelection( pNextWeapon->GetSlot(), pNextWeapon->GetPosition() );
		}
	}

	if ( !pNextWeapon )
	{
		// wrap around back to end of weapon list
		pNextWeapon = FindPrevWeaponInWeaponSelection( MAX_WEAPON_SLOTS, MAX_WEAPON_POSITIONS );
	}

	if ( pNextWeapon )
	{
		SetSelectedWeapon( pNextWeapon );
		SetSelectedSlideDir( -1 );

		if( hud_fastswitch.GetInt() > 0 )
		{
			SelectWeapon();
		}
		else if ( !IsInSelectionMode() )
		{
			OpenSelection();
		}

		// Play the "cycle to next weapon" sound
		pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the # of the last weapon in the specified slot
//-----------------------------------------------------------------------------
int CHudWeaponSelection::GetLastPosInSlot( int iSlot ) const
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	int iMaxSlotPos;

	if ( !player )
		return -1;

	iMaxSlotPos = -1;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = player->GetWeapon(i);
		
		if ( pWeapon == NULL )
			continue;

		if ( pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() > iMaxSlotPos )
			iMaxSlotPos = pWeapon->GetPosition();
	}

	return iMaxSlotPos;
}

//-----------------------------------------------------------------------------
// Purpose: returns the weapon in the specified slot
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::GetWeaponInSlot( int iSlot, int iSlotPos )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return NULL;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = player->GetWeapon(i);
		
		if ( pWeapon == NULL )
			continue;

		if ( pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() == iSlotPos )
			return pWeapon;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Moves selection to the specified slot
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SelectWeaponSlot( int iSlot )
{
	// iSlot is one higher than it should be, since it's the number key, not the 0-based index into the weapons
	--iSlot;

	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// Don't try and read past our possible number of slots
	if ( iSlot > HL2_MAX_WEAPON_SLOTS )
		return;
	
	// Make sure the player's allowed to switch weapons
	if ( pPlayer->IsAllowedToSwitchWeapons() == false )
		return;

	// do a fast switch if set
	if ( hud_fastswitch.GetBool() )
	{
		FastWeaponSwitch( iSlot );
		return;
	}

	int slotPos = 0;
	C_BaseCombatWeapon *pActiveWeapon = GetSelectedWeapon();

	// start later in the list
	if ( IsInSelectionMode() && pActiveWeapon && pActiveWeapon->GetSlot() == iSlot )
	{
		slotPos = pActiveWeapon->GetPosition() + 1;
	}

	// find the weapon in this slot
	pActiveWeapon = GetNextActivePos( iSlot, slotPos );
	if ( !pActiveWeapon )
	{
		pActiveWeapon = GetNextActivePos( iSlot, 0 );
	}
	
	if ( pActiveWeapon != NULL )
	{
		if( hud_fastswitch.GetInt() > 0 )
		{
			// only one active item in bucket, so change directly to weapon
			SelectWeapon();
		}
		else if ( !IsInSelectionMode() )
		{
			// open the weapon selection
			OpenSelection( true );
		}

		// Mark the change
		SetSelectedWeapon( pActiveWeapon );
		SetSelectedSlideDir( 0 );
	}

	pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
}

//-----------------------------------------------------------------------------
// Purpose: starts animating the highlight for the selected weapon
//-----------------------------------------------------------------------------
void CHudWeaponSelection::ActivateWeaponHighlight( C_BaseCombatWeapon *pSelectedWeapon )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// make sure all our configuration data is read
	MakeReadyForUse();

	C_BaseCombatWeapon *pWeapon = GetWeaponInSlot( m_iSelectedSlot, m_iSelectedBoxPosition );
	if ( !pWeapon )
		return;

	// start the highlight after the scroll completes
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "WeaponHighlight" );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the next weapon in the slot
//-----------------------------------------------------------------------------
void CHudWeaponSelection::FastWeaponSwitch( int iWeaponSlot )
{
	// get the slot the player's weapon is in
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// see where we should start selection
	int iPosition = -1;
	C_BaseCombatWeapon *pActiveWeapon = pPlayer->GetActiveWeapon();
	if ( pActiveWeapon && pActiveWeapon->GetSlot() == iWeaponSlot )
	{
		// start after this weapon
		iPosition = pActiveWeapon->GetPosition();
	}

	C_BaseCombatWeapon *pNextWeapon = NULL;

	// search for the weapon after the current one
	pNextWeapon = FindNextWeaponInWeaponSelection(iWeaponSlot, iPosition);
	// make sure it's in the same bucket
	if ( !pNextWeapon || pNextWeapon->GetSlot() != iWeaponSlot )
	{
		// just look for any weapon in this slot
		pNextWeapon = FindNextWeaponInWeaponSelection(iWeaponSlot, -1);
	}

	// see if we found a weapon that's different from the current and in the selected slot
	if ( pNextWeapon && pNextWeapon != pActiveWeapon && pNextWeapon->GetSlot() == iWeaponSlot )
	{
		// select the new weapon
		::input->MakeWeaponSelection( pNextWeapon );
	}
	else if ( pNextWeapon != pActiveWeapon )
	{
		// error sound
		pPlayer->EmitSound( "Player.DenyWeaponSelection" );
	}
}

C_BaseCombatWeapon *CHudWeaponSelection::GetNextActivePos( int iSlot, int iSlotPos )
{
	if ( iSlot >= HL2_MAX_WEAPON_SLOTS )
		return NULL;

	return CBaseHudWeaponSelection::GetNextActivePos( iSlot, iSlotPos );
}
