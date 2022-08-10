//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "text_message.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "view.h"
#include <KeyValues.h>
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>
#include "vguimatsurface/IMatSystemSurface.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMesh.h"
#include "materialsystem/imaterialvar.h"
#include "ieffects.h"
#include "hudelement.h"
#include "ClientEffectPrecacheSystem.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: HDU Damage indication
//-----------------------------------------------------------------------------
class CHudDamageIndicator : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudDamageIndicator, vgui::Panel );

public:
	CHudDamageIndicator( const char *pElementName );
	void Init( void );
	void Reset( void );
	virtual bool ShouldDraw( void );

	// Handler for our message
	void MsgFunc_Damage( bf_read &msg );

private:
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	void	CalcDamageDirection( const Vector &vecFrom );
	void	DrawDamageIndicatorFront( float flFade );
	void	DrawDamageIndicatorRear( float flFade );
	void	DrawDamageIndicatorLeft( float flFade );
	void	DrawDamageIndicatorRight( float flFade );

private:
	float	m_flAttackFront;
	float	m_flAttackRear;
	float	m_flAttackLeft;
	float	m_flAttackRight;

	Color	m_clrIndicator;

	CHudTexture	*icon_pain;
//	CHudTexture	*icon_up;
//	CHudTexture	*icon_down;
//	CHudTexture	*icon_left;
//	CHudTexture	*icon_right;
};

DECLARE_HUDELEMENT( CHudDamageIndicator );
DECLARE_HUD_MESSAGE( CHudDamageIndicator, Damage );

#define USE_HUDANIMATIONS	0

#ifdef USE_HUDANIMATIONS
enum
{
	DAMAGE_ANY,
	DAMAGE_LOW,
	DAMAGE_HIGH,
};

#define ANGLE_ANY	0.0f
#define DMG_ANY		0

struct DamageAnimation_t
{
	const char *name;
	int bitsDamage;
	int damage; 
};

//-----------------------------------------------------------------------------
// Purpose: List of damage animations, finds first that matches criteria
//-----------------------------------------------------------------------------
static DamageAnimation_t g_DamageAnimations[] =
{
	{ "HudTakeDamageDrown",		DMG_DROWN,	DAMAGE_ANY },
	{ "HudTakeDamagePoison",	DMG_POISON,	DAMAGE_ANY },
	{ "HudTakeDamageBurn",		DMG_BURN,	DAMAGE_ANY },
	{ "HudTakeDamageRadiation",	DMG_RADIATION,	DAMAGE_ANY },
	{ "HudTakeDamageRadiation",	DMG_ACID,	DAMAGE_ANY },

	{ "HudTakeDamageHigh",		DMG_ANY,	DAMAGE_HIGH },

	// fall through to front damage
	{ "HudTakeDamageFront",		DMG_ANY,	DAMAGE_ANY },
	{ NULL },
};
#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudDamageIndicator::CHudDamageIndicator( const char *pElementName ) : CHudElement( pElementName ), BaseClass(NULL, "HudDamageIndicator")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDamageIndicator::Reset( void )
{
	m_flAttackFront	= 0.0;
	m_flAttackRear	= 0.0;
	m_flAttackRight	= 0.0;
	m_flAttackLeft	= 0.0;

	m_clrIndicator.SetColor( 250, 0, 0, 255 );
}

void CHudDamageIndicator::Init( void )
{
	HOOK_HUD_MESSAGE( CHudDamageIndicator, Damage );
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudDamageIndicator::ShouldDraw( void )
{
	if ( ( m_flAttackFront <= 0.0 ) && ( m_flAttackRear <= 0.0 ) && ( m_flAttackLeft <= 0.0 ) && ( m_flAttackRight <= 0.0 ) )
		return false;

	return ( CHudElement::ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: Draws a damage quad
//-----------------------------------------------------------------------------
void CHudDamageIndicator::DrawDamageIndicatorFront( float flFade )
{
	if ( m_flAttackFront > 0.4 )
	{
		IMaterial *material = materials->FindMaterial( icon_pain->szTextureFile, TEXTURE_GROUP_VGUI );
		if ( material )
		{
			bool found;
			IMaterialVar* pFrameVar = material->FindVar( "$frame", &found, false );
			if ( found )
			{
				pFrameVar->SetFloatValue( 0 );
			}
		}

		int	x = ( ScreenWidth() / 2 ) - icon_pain->Width() / 2;
		int	y = ( ScreenHeight() / 2 ) - icon_pain->Height() * 3;
		icon_pain->DrawSelf( x, y, m_clrIndicator );

		m_flAttackFront = max( 0.0, m_flAttackFront - flFade );
	}
	else
	{
		m_flAttackFront = 0.0;
	}
}

void CHudDamageIndicator::DrawDamageIndicatorRear( float flFade )
{
	if ( m_flAttackRear > 0.4 )
	{
		IMaterial *material = materials->FindMaterial( icon_pain->szTextureFile, TEXTURE_GROUP_VGUI );
		if ( material )
		{
			bool found;
			IMaterialVar* pFrameVar = material->FindVar( "$frame", &found, false );
			if ( found )
			{
				pFrameVar->SetFloatValue( 2 );
			}
		}

		int	x = ( ScreenWidth() / 2 ) - icon_pain->Width() / 2;
		int	y = ( ScreenHeight() / 2 ) + icon_pain->Height() * 2;
		icon_pain->DrawSelf( x, y, m_clrIndicator );

		m_flAttackRear = max( 0.0, m_flAttackRear - flFade );
	}
	else
	{
		m_flAttackRear = 0.0;
	}
}


void CHudDamageIndicator::DrawDamageIndicatorLeft( float flFade )
{
	if ( m_flAttackLeft > 0.4 )
	{
		IMaterial *material = materials->FindMaterial( icon_pain->szTextureFile, TEXTURE_GROUP_VGUI );
		if ( material )
		{
			bool found;
			IMaterialVar* pFrameVar = material->FindVar( "$frame", &found, false );
			if ( found )
			{
				pFrameVar->SetFloatValue( 3 );
			}
		}

		int	x = ( ScreenWidth() / 2 ) - icon_pain->Width() * 3;
		int	y = ( ScreenHeight() / 2 ) - icon_pain->Height() / 2;
		icon_pain->DrawSelf( x, y, m_clrIndicator );

		m_flAttackLeft = max( 0.0, m_flAttackLeft - flFade );
	}
	else
	{
		m_flAttackLeft = 0.0;
	}
}


void CHudDamageIndicator::DrawDamageIndicatorRight( float flFade )
{
	if ( m_flAttackRight > 0.4 )
	{
		IMaterial *material = materials->FindMaterial( icon_pain->szTextureFile, TEXTURE_GROUP_VGUI );
		if ( material )
		{
			bool found;
			IMaterialVar* pFrameVar = material->FindVar( "$frame", &found, false );
			if ( found )
			{
				pFrameVar->SetFloatValue( 1 );
			}
		}

		int	x = ( ScreenWidth() / 2 ) + icon_pain->Width() * 2;
		int	y = ( ScreenHeight() / 2 ) - icon_pain->Height() / 2;
		icon_pain->DrawSelf( x, y, m_clrIndicator );

		m_flAttackRight = max( 0.0, m_flAttackRight - flFade );
	}
	else
	{
		m_flAttackRight = 0.0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Paints the damage display
//-----------------------------------------------------------------------------
void CHudDamageIndicator::Paint()
{
	if ( !icon_pain )
	{
		icon_pain = gHUD.GetIcon( "pain" );
	}

	if ( !icon_pain )
		return;

	// draw damage indicators	
	float flFade = gpGlobals->frametime * 2;
	DrawDamageIndicatorFront( flFade );
	DrawDamageIndicatorRear( flFade );
	DrawDamageIndicatorLeft( flFade );
	DrawDamageIndicatorRight( flFade);
}

//-----------------------------------------------------------------------------
// Purpose: Message handler for Damage message
//-----------------------------------------------------------------------------
void CHudDamageIndicator::MsgFunc_Damage( bf_read &msg )
{
	int armor = msg.ReadByte();			// armor
	int damageTaken = msg.ReadByte();	// health
	long bitsDamage = msg.ReadLong(); 	// damage bits

	Vector vecFrom;
	vecFrom.x = msg.ReadFloat();
	vecFrom.y = msg.ReadFloat();
	vecFrom.z = msg.ReadFloat();

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// player has just died, just run the dead damage animation
	if ( pPlayer->GetHealth() <= 0 )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HudPlayerDeath" );
		return;
	}

	// ignore damage without direction
	// this should never happen, unless it's drowning damage, 
	// or the player is forcibly killed, handled above
	if ( vecFrom == vec3_origin && !(bitsDamage & DMG_DROWN))
		return;

	int highDamage = DAMAGE_LOW;
	if ( damageTaken > 25 )
	{
		highDamage = DAMAGE_HIGH;
	}

	// if we have no suit, all damage is high
	if ( !pPlayer->IsSuitEquipped() )
	{
		highDamage = DAMAGE_HIGH;
	}

	if ( damageTaken > 0 || armor > 0 )
	{
#ifdef USE_HUDANIMATIONS
		// see which effect to play
		DamageAnimation_t *dmgAnim = g_DamageAnimations;
		for ( ; dmgAnim->name != NULL; ++dmgAnim )
		{
			if ( dmgAnim->bitsDamage && !(bitsDamage & dmgAnim->bitsDamage) )
				continue;

			if ( dmgAnim->damage && dmgAnim->damage != highDamage )
				continue;

			// we have a match, break
			break;
		}

		if ( dmgAnim->name )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( dmgAnim->name );
		}
#endif
		CalcDamageDirection( vecFrom );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Convert a damage position in world units to the screen's units
//-----------------------------------------------------------------------------
void CHudDamageIndicator::CalcDamageDirection( const Vector &vecFrom )
{
	// Now display the indicator
	if ( vecFrom == vec3_origin )
	{
		m_flAttackFront	= 0.0;
		m_flAttackRear	= 0.0;
		m_flAttackRight	= 0.0;
		m_flAttackLeft	= 0.0;

		return;
	}

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
	{
		return;
	}

	Vector vecDelta = ( vecFrom - pLocalPlayer->GetRenderOrigin() );

	if ( vecDelta.Length() <= 50 )
	{
		m_flAttackFront	= 1.0;
		m_flAttackRear	= 1.0;
		m_flAttackRight	= 1.0;
		m_flAttackLeft	= 1.0;

		return;
	}

	VectorNormalize( vecDelta );

	Vector forward;
	Vector right;
	AngleVectors( MainViewAngles(), &forward, &right, NULL );

	float flFront	= DotProduct( vecDelta, forward );
	float flSide	= DotProduct( vecDelta, right );

	if ( flFront > 0 )
	{
		if ( flFront > 0.3 )
			m_flAttackFront = max( m_flAttackFront, flFront );
	}
	else
	{
		float f = fabs( flFront );
		if ( f > 0.3 )
			m_flAttackRear = max( m_flAttackRear, f );
	}

	if ( flSide > 0 )
	{
		if ( flSide > 0.3 )
			m_flAttackRight = max( m_flAttackRight, flSide );
	}
	else
	{
		float f = fabs( flSide );
		if ( f > 0.3 )
			m_flAttackLeft = max( m_flAttackLeft, f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: hud scheme settings
//-----------------------------------------------------------------------------
void CHudDamageIndicator::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);

	int wide, tall;
	GetHudSize(wide, tall);
	SetSize(wide, tall);
}
