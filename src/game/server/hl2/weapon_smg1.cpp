//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Ol' Reliable - SMG
//
//	Primary attack: Shooty thingy.
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "NPCevent.h"
#include "basecombatcharacter.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "game.h"
#include "in_buttons.h"
#include "AI_Memory.h"
#include "soundent.h"
#include "rumble_shared.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SMG_EASY_DAMPEN 0.5f

//ConVar sk_weapon_smg1_lerp( "sk_weapon_smg1_lerp", "6.0" );
extern ConVar sv_funmode;

//-----------------------------------------------------------------------------
// SMG1
//-----------------------------------------------------------------------------
class CWeaponSMG1 : public CHLMachineGun
{
public:
	DECLARE_CLASS( CWeaponSMG1, CHLMachineGun );

	CWeaponSMG1();

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	
	void	Precache( void );
	void	AddViewKick( void );
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	int		GetMinBurst() { return 3; }
	int		GetMaxBurst() { return 6; }

	virtual float	GetMinRestTime();
	virtual float	GetMaxRestTime();

	virtual void Equip( CBaseCombatCharacter *pOwner );

	void FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir );
	void Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	DECLARE_ACTTABLE();
};

BEGIN_DATADESC( CWeaponSMG1 )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponSMG1, DT_WeaponSMG1)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_smg1, CWeaponSMG1 );
PRECACHE_WEAPON_REGISTER(weapon_smg1);

acttable_t	CWeaponSMG1::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SMG1,			true },
	{ ACT_RANGE_ATTACK2, 			ACT_RANGE_ATTACK_AR2_GRENADE, 	true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
	{ ACT_SPRINT,					ACT_RUN_RIFLE_STIMULATED,		true },
	
// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
//End readiness activities

	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SMG1,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },
//	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_RIFLE_LOW,		false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },
};

IMPLEMENT_ACTTABLE(CWeaponSMG1);

CWeaponSMG1::CWeaponSMG1( )
{
	m_fMinRange1		= 32;
	m_fMaxRange1		= 1536;

//	m_fMinRange2		= 224;
//	m_fMaxRange2		= 1280;

	if ( !sv_funmode.GetBool() )
	{
		m_bReloadsFullClip	= true;
		m_bCanJam			= true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSMG1::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Adjust time between bursts for different npc types.
//-----------------------------------------------------------------------------
float CWeaponSMG1::GetMinRestTime()
{
	//Default is 0.3
	Class_T OwnerClass = GetOwner()->Classify();

	if ( OwnerClass == CLASS_CITIZEN_PASSIVE	||
		 OwnerClass == CLASS_CITIZEN_REBEL	||
		 OwnerClass == CLASS_PLAYER_ALLY	||
		 OwnerClass == CLASS_METROPOLICE	)
		 return 0.4f;

	return BaseClass::GetMinRestTime();
}

//-----------------------------------------------------------------------------
float CWeaponSMG1::GetMaxRestTime()
{
	// Default is 0.6
	Class_T OwnerClass = GetOwner()->Classify();

	if ( OwnerClass == CLASS_PLAYER_ALLY	||
		 OwnerClass == CLASS_METROPOLICE	)
		 return 0.7f;

	if ( OwnerClass == CLASS_CITIZEN_PASSIVE	||
		 OwnerClass == CLASS_CITIZEN_REBEL	)
		 return 0.8f;

	return BaseClass::GetMaxRestTime();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSMG1::AddViewKick( void )
{
	#define	SMG1_MAX_VERTICAL_KICK		3.5f	//Degrees TODO; Convar
	#define	SMG1_SLIDE_LIMIT			3.0f	//Seconds TODO; Convar
	
	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
		return;

	DoMachineGunKick( pPlayer, SMG_EASY_DAMPEN, SMG1_MAX_VERTICAL_KICK, m_fFireDuration, SMG1_SLIDE_LIMIT );
}

//-----------------------------------------------------------------------------
// Purpose: Give this weapon longer range when wielded by an ally NPC.
//-----------------------------------------------------------------------------
void CWeaponSMG1::Equip( CBaseCombatCharacter *pOwner )
{
	if( pOwner->Classify() == CLASS_PLAYER_ALLY )
	{
		m_fMinRange1 = 0;	//Temp hack for lack of melee
		m_fMaxRange1 = 2000;
	}

	BaseClass::Equip( pOwner );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSMG1::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir )
{
	// FIXME: use the returned number of bullets to account for >10hz firerate
//	WeaponSoundRealtime( SINGLE_NPC );
	WeaponSound( SINGLE_NPC );

	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;

	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );
	int	TracerFreq = random->RandomInt( 1, 2 );
	pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, pOperator->GetAttackSpread(this), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, TracerFreq, entindex(), 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSMG1::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	Vector vecShootOrigin, vecShootDir;
	QAngle	angShootDir;
	GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
	AngleVectors( angShootDir, &vecShootDir );
	FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSMG1::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
	case EVENT_WEAPON_SMG1:
	case EVENT_WEAPON_SMG2:
		{
			Vector vecShootOrigin, vecShootDir;
			QAngle angDiscard;

			// Support old style attachment point firing
			if ((pEvent->options == NULL) || (pEvent->options[0] == '\0') || (!pOperator->GetAttachment(pEvent->options, vecShootOrigin, angDiscard)))
			{
				vecShootOrigin = pOperator->Weapon_ShootPosition();
			}

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );
			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

			FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
		}
		break;

	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}
