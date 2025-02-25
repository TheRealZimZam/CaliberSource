//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: FLASH incendiary launcher
//
//			Primary attack: fire missile
//			Secondary attack: fancy scope
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon_shared.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "NPCevent.h"
#include "AI_BaseNPC.h"
#include "baseplayer_shared.h"
#include "hl2_player_shared.h"
#include "in_buttons.h"
#include "soundent.h"
#include "shake.h"
#include "rumble_shared.h"
#include "hl2_shareddefs.h"
#include "grenade_homer.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sk_plr_dmg_rpg_round;
extern ConVar sk_npc_dmg_rpg_round;

#define	FLASH_MISSILE_SPEED sk_flash_missile_speed.GetInt()
ConVar sk_flash_missile_speed( "sk_flash_missile_speed","1200");

#define	FLASH_MISSILE_GRAVITY sk_flash_missile_gravity.GetFloat()
ConVar sk_flash_missile_gravity( "sk_flash_missile_gravity","4");

//-----------------------------------------------------------------------------
// Dumbfire clip-fed rpg
//-----------------------------------------------------------------------------
class CWeaponFLASH : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponFLASH, CBaseHLCombatWeapon );

	CWeaponFLASH();
	~CWeaponFLASH();

	DECLARE_SERVERCLASS();

	void	Precache( void );

	void	PrimaryAttack( void );
	bool	Reload( void );

#ifndef CLIENT_DLL
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	int		GetMinBurst() { return 4; }
	int		GetMaxBurst() { return 4; }
	float	GetMinRestTime() { return .5; }
	float	GetMaxRestTime() { return .75; }

	bool	WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );
	int		WeaponRangeAttack1Condition( float flDot, float flDist );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif

	DECLARE_ACTTABLE();

private:

};

IMPLEMENT_SERVERCLASS_ST(CWeaponFLASH, DT_WeaponFLASH)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_flash, CWeaponFLASH );
PRECACHE_WEAPON_REGISTER(weapon_flash);

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CWeaponFLASH )

END_DATADESC()

//-----------------------------------------------------------------------------
// Maps base activities to weapons-specific ones so our characters do the right things.
//-----------------------------------------------------------------------------
acttable_t	CWeaponFLASH::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_RPG, true },

	{ ACT_IDLE_RELAXED,				ACT_IDLE_RPG_RELAXED,			true },
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_ANGRY_RPG,				true },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_RPG,				true },

	{ ACT_IDLE,						ACT_IDLE_RPG,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_RPG,				true },
	{ ACT_WALK,						ACT_WALK_RPG,					true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RPG,			true },
	{ ACT_RUN,						ACT_RUN_RPG,					true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RPG,				true },
	{ ACT_COVER_LOW,				ACT_COVER_LOW_RPG,				true },
};

IMPLEMENT_ACTTABLE(CWeaponFLASH);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponFLASH::CWeaponFLASH()
{
	m_bReloadsSingly = false;
	m_bReloadsFullClip = true;

	m_fMinRange1 = 180;
	m_fMaxRange1 = 400*10;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponFLASH::~CWeaponFLASH()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFLASH::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "Missile.Accelerate" );
	PrecacheModel( "models/weapons/w_missile.mdl" );
	UTIL_PrecacheOther( "grenade_homer" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFLASH::PrimaryAttack( void )
{
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		Reload();
		return;
	}

	// Can't be reloading
	if ( GetActivity() == ACT_VM_RELOAD )
		return;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	m_flNextPrimaryAttack = gpGlobals->curtime + GetMinRestTime();

	Vector vForward, vRight, vUp;
	pOwner->EyeVectors( &vForward, &vRight, &vUp );
	Vector muzzlePoint = pOwner->Weapon_ShootPosition() + vForward * 20.0f + vRight * 6.0f + vUp * -3.0f;

	QAngle vecAngles;
	VectorAngles( vForward, vecAngles );
	CGrenadeHomer *pGrenade = CGrenadeHomer::CreateGrenadeHomer( MAKE_STRING("models/weapons/w_missile.mdl"), MAKE_STRING("Missile.Accelerate"), muzzlePoint, vecAngles, edict() );
	pGrenade->Spawn();
	pGrenade->SetSpin(1,20);
	pGrenade->SetHoming(0.0,0,0.0,0,0.0);
	pGrenade->SetRocketType(EXPLOSIVEINCENDIARY);
	pGrenade->SetDamage( sk_plr_dmg_rpg_round.GetInt() );
	pGrenade->SetDamageRadius( 200 );
	pGrenade->Launch(pOwner,NULL,(vForward*FLASH_MISSILE_SPEED),FLASH_MISSILE_SPEED,FLASH_MISSILE_GRAVITY,HOMER_SMOKE_TRAIL_ON);

	// Remove a rocket
	if ( UsesClipsForAmmo1() )
		m_iClip1--;
	else
		pOwner->RemoveAmmo( 1, GetPrimaryAmmoType() );

	// Do animation
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	WeaponSound( SINGLE );

	// Register a muzzleflash for the AI
	pOwner->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );
	pOwner->RumbleEffect( RUMBLE_RPG_MISSILE, 0, RUMBLE_FLAG_RESTART );

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pOwner, true, GetClassname() );
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 1000, 0.2, GetOwner(), SOUNDENT_CHANNEL_WEAPON );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponFLASH::Reload( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	
	if ( pOwner == NULL )
		return false;

	if ( pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		return false;

	bool fRet;
	fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( fRet )
	{
		WeaponSound( RELOAD );
	}

	return fRet;
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CWeaponFLASH::WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	bool bResult = BaseClass::WeaponLOSCondition( ownerPos, targetPos, bSetConditions );

	if( bResult )
	{
		CAI_BaseNPC* npcOwner = GetOwner()->MyNPCPointer();

		if( npcOwner )
		{
			trace_t tr;

			Vector vecRelativeShootPosition;
			VectorSubtract( npcOwner->Weapon_ShootPosition(), npcOwner->GetAbsOrigin(), vecRelativeShootPosition );
			Vector vecMuzzle = ownerPos + vecRelativeShootPosition;
			Vector vecShootDir = npcOwner->GetActualShootTrajectory( vecMuzzle );

			// Make sure I have a good 8 feet of wide clearance in front, or I'll blow my teeth out.
			AI_TraceHull( vecMuzzle, vecMuzzle + vecShootDir * (8.0f*12.0f), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), MASK_NPCSOLID, npcOwner, COLLISION_GROUP_NONE, &tr );

			if( tr.fraction != 1.0f )
				bResult = false;
		}
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flDot - 
//			flDist - 
// Output : int
//-----------------------------------------------------------------------------
int CWeaponFLASH::WeaponRangeAttack1Condition( float flDot, float flDist )
{
	// Ignore vertical distance when doing our RPG distance calculations
	CAI_BaseNPC *pOwner = GetOwner()->MyNPCPointer();
	ASSERT( pOwner != NULL );
	if ( pOwner )
	{
		CBaseEntity *pEnemy = pOwner->GetEnemy();
		Vector vecToTarget = (pEnemy->GetAbsOrigin() - pOwner->GetAbsOrigin());
		vecToTarget.z = 0;
		flDist = vecToTarget.Length();
	}

	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return COND_NONE;

	if ( flDist < m_fMinRange1 )
		return COND_TOO_CLOSE_TO_ATTACK;
	if ( flDist > m_fMaxRange1 )
		return COND_TOO_FAR_TO_ATTACK;

	if( pOwner )
	{
		Vector vecEnemyLKP = pOwner->GetEnemyLKP();
		CBaseEntity *pTarget = NULL;

		while ( ( pTarget = gEntList.FindEntityInSphere( pTarget, vecEnemyLKP, 200 ) ) != NULL )
		{
			if ( pOwner->IRelationType( pTarget ) == D_LI )
			{
				CSoundEnt::InsertSound( SOUND_MOVE_AWAY, vecEnemyLKP, 200, 0.1 );
				return COND_WEAPON_BLOCKED_BY_FRIEND;
			}
		}
	}

	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponFLASH::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_MISSILE_FIRE:
		case EVENT_WEAPON_SMG1:
		{

		}
		break;

		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}
#endif
