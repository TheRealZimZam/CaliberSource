//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Schmessier SMG
//
//	Primary attack: Shooty thingy.
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"
#include "basehlcombatweapon_shared.h"
#include "baseplayer_shared.h"
#ifndef CLIENT_DLL
	#include "basecombatcharacter.h"
	#include "AI_BaseNPC.h"
	#include "soundent.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SMG_EASY_DAMPEN 0.5f

//ConVar sk_weapon_smg1_lerp( "sk_weapon_smg1_lerp", "6.0" );
extern ConVar sv_funmode;

#ifdef CLIENT_DLL
#define CWeaponMP40 C_WeaponMP40
#endif
class CWeaponMP40 : public CHLMachineGun
{
public:
	DECLARE_CLASS( CWeaponMP40, CHLMachineGun );

	CWeaponMP40();

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	void	Precache( void );
	void	AddViewKick( void );

	int		GetMinBurst() { return 3; }
	int		GetMaxBurst() { return 5; }

#if !defined(CLIENT_DLL)
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	virtual float	GetMinRestTime();
	virtual float	GetMaxRestTime();
	void FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir );
	void Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif

	DECLARE_ACTTABLE();

private:
	CWeaponMP40( const CWeaponMP40 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponMP40, DT_WeaponMP40 )

BEGIN_NETWORK_TABLE( CWeaponMP40, DT_WeaponMP40 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponMP40 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_mp40, CWeaponMP40 );
PRECACHE_WEAPON_REGISTER(weapon_mp40);

acttable_t	CWeaponMP40::m_acttable[] = 
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

IMPLEMENT_ACTTABLE(CWeaponMP40);

//=========================================================
CWeaponMP40::CWeaponMP40( )
{
	m_fMinRange1		= 32;
	m_fMaxRange1		= 1536;

//	m_fMinRange2		= 224;
//	m_fMaxRange2		= 1280;

	if ( sv_funmode.GetBool() )
	{
		m_bReloadsFullClip	= true;
		m_bCanJam			= true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMP40::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMP40::AddViewKick( void )
{
	#define	SMG1_MAX_VERTICAL_KICK		3.5f	//Degrees TODO; Convar
	#define	SMG1_SLIDE_LIMIT			3.0f	//Seconds TODO; Convar
	
	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
		return;

	DoMachineGunKick( pPlayer, SMG_EASY_DAMPEN, SMG1_MAX_VERTICAL_KICK, m_fFireDuration, SMG1_SLIDE_LIMIT );
}

#if !defined(CLIENT_DLL)
//-----------------------------------------------------------------------------
// Purpose: Adjust time between bursts for different npc types.
//-----------------------------------------------------------------------------
float CWeaponMP40::GetMinRestTime()
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
float CWeaponMP40::GetMaxRestTime()
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
void CWeaponMP40::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir )
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
void CWeaponMP40::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
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
void CWeaponMP40::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
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

#endif
