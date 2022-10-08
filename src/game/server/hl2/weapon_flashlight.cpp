//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:	Manual Flashlight for prolouge
//
//			Primary attack: turn on/off light
//			Secondary attack: quick smack
// TODO's:  Actually code a clientside light
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// CWeaponFlashlight
//-----------------------------------------------------------------------------

class CWeaponFlashlight : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CWeaponFlashlight, CBaseHLCombatWeapon );

	CWeaponFlashlight(void);

	DECLARE_SERVERCLASS();

	void	Precache( void );
	void	ItemPostFrame( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	
	bool	CanHolster( void );

	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	Activity	GetPrimaryAttackActivity( void );
	
	DECLARE_ACTTABLE();

private:
	bool	m_bLightOn;

};


IMPLEMENT_SERVERCLASS_ST(CWeaponFlashlight, DT_WeaponFlashlight)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_flashlight, CWeaponFlashlight );
PRECACHE_WEAPON_REGISTER( weapon_flashlight );

BEGIN_DATADESC( CWeaponFlashlight )

	DEFINE_FIELD( m_bLightOn, FIELD_BOOLEAN ),

END_DATADESC()

acttable_t	CWeaponFlashlight::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },

	{ ACT_WALK,						ACT_WALK_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN,						ACT_RUN_PISTOL,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },

	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,		false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,			false },
};


IMPLEMENT_ACTTABLE( CWeaponFlashlight );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponFlashlight::CWeaponFlashlight( void )
{
	m_fMinRange1		= 0;
	m_fMaxRange1		= 1024;

	m_bLightOn			= false;
	m_bFiresUnderwater	= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlashlight::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponFlashlight::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_PISTOL_FIRE:
		{

		}
		break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Accurate single shot
//-----------------------------------------------------------------------------
void CWeaponFlashlight::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	WeaponSound( SINGLE );

	// player "shoot" animation
	SendWeaponAnim( GetPrimaryAttackActivity() );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	pPlayer->SetAimTime( 3.0f );

	if ( m_bLightOn )
	{
		pPlayer->FlashlightTurnOff();
		m_bLightOn = false;	// The light is off
	}
	else
	{
		pPlayer->FlashlightTurnOn();
		m_bLightOn = true;	// The light is on
	}

	m_iPrimaryAttacks++;
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.2;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: Smack enemy
//-----------------------------------------------------------------------------
void CWeaponFlashlight::SecondaryAttack( void )
{
	PrimaryAttack();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponFlashlight::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();
	
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
	{
		if ( m_bLightOn )
			m_bLightOn = false;	//Turn the light off

		return;
	}

	if ( pOwner->m_afButtonPressed & IN_ATTACK )
	{
		PrimaryAttack();
	}
}

//-----------------------------------------------------------------------------
bool CWeaponFlashlight::CanHolster( void )
{
	// Turn the light off
	if ( m_bLightOn )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( pPlayer )
			pPlayer->FlashlightTurnOff();

		m_bLightOn = false;
	}

	return BaseClass::CanHolster();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeaponFlashlight::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

