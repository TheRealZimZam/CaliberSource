//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:	Manual Flashlight for prolouge
//
//			Primary attack: turn on/off light
//			Secondary attack: quick smack
// TODO's:  For npc's, just stick a env_beam on the end of the "muzzle"
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
#include "beam_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// CWeaponFlashlight
//-----------------------------------------------------------------------------

#define FLASHLIGHT_PLAYER_BLIND_DISTANCE 12*50	//Dont turn on towards a friendly player if hes this far from me

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
	int		WeaponRangeAttack1Condition( float flDot, float flDist );
	Activity	GetPrimaryAttackActivity( void );
	
	DECLARE_ACTTABLE();

private:
	bool	m_bLightOn;
	CHandle<CBeam>	m_hFlashlightBeam;

};

ConVar	sk_dmg_flashlight_smack( "sk_dmg_flashlight_smack", "5" );	//Death by spoon

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
	m_hFlashlightBeam	= NULL;
	m_bFiresUnderwater	= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlashlight::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "sprites/glow_test01.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CWeaponFlashlight::WeaponRangeAttack1Condition( float flDot, float flDist )
{
	CAI_BaseNPC *pOperator = GetOwner()->MyNPCPointer();
	Vector vecTarget = pOperator->GetEnemyLKP();

	// ---------------------------------------------------------------------
	// Is a friendly player in my way?
	// ---------------------------------------------------------------------
	if( flDist < FLASHLIGHT_PLAYER_BLIND_DISTANCE )
	{
		CBaseEntity *pTarget = NULL;
		while ( ( pTarget = gEntList.FindEntityInSphere( pTarget, vecTarget, 100 ) ) != NULL )
		{
			//Check to see if the default relationship is hatred, and if so intensify that
			if ( pOperator->IRelationType( pTarget ) == D_LI )
			{
				return COND_WEAPON_BLOCKED_BY_FRIEND;
			}
		}
	}

	return true;	//TODO
}

void CWeaponFlashlight::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_PISTOL_FIRE:
		{
			if ( m_bLightOn )
				m_bLightOn = false;	// The light is off
			else
				m_bLightOn = true;	// The light is on

			if ( m_bLightOn )
			{
				if (!pOperator->IsPlayer())
				{
					Vector vecShootOrigin, vecShootDir;
					vecShootOrigin = pOperator->Weapon_ShootPosition();
					CAI_BaseNPC *npc = pOperator->MyNPCPointer();
					ASSERT( npc != NULL );
					vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

					QAngle	angShootDir;
					GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
					AngleVectors( angShootDir, &vecShootDir );

					//Create the beam (sprite/dlight is clientside)
		#if 1
					m_hFlashlightBeam = CBeam::BeamCreate( "sprites/glow_test01.vmt", 32 );

					// Set the temporary spawnflag on the beam so it doesn't save (we'll recreate it on restore)
					m_hFlashlightBeam->AddSpawnFlags( SF_BEAM_TEMPORARY );
					m_hFlashlightBeam->SetEndWidth(32);
					m_hFlashlightBeam->SetFadeLength( 384 );
					m_hFlashlightBeam->SetBeamFlags( (FBEAM_SHADEOUT|FBEAM_NOTILE) );
					m_hFlashlightBeam->SetColor( 255, 255, 255 );
					m_hFlashlightBeam->SetBrightness( 224 );
					m_hFlashlightBeam->SetNoise( 0 );
					//
					m_hFlashlightBeam->HoseInit( vecShootOrigin, vecShootDir );
		#endif
				}
			}
			else if (m_hFlashlightBeam != NULL)
			{
				//Turn off the beam
				UTIL_Remove(m_hFlashlightBeam);
			}

			WeaponSound( SINGLE_NPC );
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
	//TODO;
	PrimaryAttack();

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponFlashlight::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

#ifdef HL2_MP
	CBaseCombatCharacter* pOwner = ToBaseCombatCharacter( GetOwner() );
	// Turn off if the owner throws it away/dies
	if ( pOwner == NULL )
	{
		if ( m_bLightOn )
			m_bLightOn = false;	//Turn the light off

		return;
	}
#endif

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

