//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Achievements
//
// TODO
//=============================================================================


#include "cbase.h"
#include "achievementmgr.h"
#include "baseachievement.h"

#ifdef GAME_DLL

#if ( defined( HL2_DLL ) || defined( HL2_EPISODIC ) )

#include "basegrenade_shared.h"
#include "basehlcombatweapon_shared.h"
#include "ammodef.h"

// Catch Molotov
class CAchievementCaliberCatchMolotov : public CBaseAchievement
{
protected:
	void Init() 
	{
		SetFlags( ACH_LISTEN_KILL_ENEMY_EVENTS | ACH_SAVE_WITH_GAME );
		SetInflictorFilter( "grenade_molotov" );
		SetVictimFilter( "npc_citizen" );
		SetGoal( 1 );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade *>( pInflictor );
		if ( pGrenade )
		{
			CBaseEntity *pThrower = pGrenade->GetThrower();
			CBaseEntity *pOriginalThrower = pGrenade->GetOriginalThrower();
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			// check if player was most recent thrower, but the victim was the original thrower
			if ( ( pPlayer == pThrower ) && ( pOriginalThrower == pVictim ) )
			{
				IncrementCount();
			}				
		}			
	}
};
DECLARE_ACHIEVEMENT( CAchievementCaliberCatchMolotov, ACHIEVEMENT_CALIBER_CATCHMOLOTOV, "CALIBER_ACHIEVEMENT_CATCHMOLOTOV", 10 );


// Catch Grenade
class CAchievementCaliberCatchGrenade : public CBaseAchievement
{
protected:
	void Init() 
	{
		SetFlags( ACH_LISTEN_KILL_ENEMY_EVENTS | ACH_SAVE_WITH_GAME );
		SetInflictorFilter( "npc_grenade_frag" );
		SetVictimFilter( "npc_grunt" );
		SetGoal( 1 );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade *>( pInflictor );
		if ( pGrenade )
		{
			CBaseEntity *pThrower = pGrenade->GetThrower();
			CBaseEntity *pOriginalThrower = pGrenade->GetOriginalThrower();
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			// check if player was most recent thrower, but the victim was the original thrower
			if ( ( pPlayer == pThrower ) && ( pOriginalThrower == pVictim ) )
			{
				IncrementCount();
			}				
		}			
	}
};
DECLARE_ACHIEVEMENT( CAchievementCaliberCatchGrenade, ACHIEVEMENT_CALIBER_CATCHGRENADE, "CALIBER_ACHIEVEMENT_CATCHGRENADE", 10 );

// Shoot Grenade
class CAchievementCaliberShootGrenade : public CBaseAchievement
{
protected:
	void Init() 
	{
		SetFlags( ACH_LISTEN_KILL_ENEMY_EVENTS | ACH_SAVE_WITH_GAME );
		SetInflictorFilter( "npc_grenade_frag" );
		SetVictimFilter( "npc_grunt" );
		SetGoal( 1 );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade *>( pInflictor );
		if ( pGrenade )
		{
			CBaseEntity *pThrower = pGrenade->GetThrower();
			CBaseEntity *pOriginalThrower = pGrenade->GetOriginalThrower();
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			// check if player was most recent thrower, but the victim was the original thrower
			if ( ( pPlayer == pThrower ) && ( pOriginalThrower == pVictim ) )
			{
				IncrementCount();
			}				
		}			
	}
};
DECLARE_ACHIEVEMENT( CAchievementCaliberShootGrenade, ACHIEVEMENT_CALIBER_SHOOTGRENADE, "CALIBER_ACHIEVEMENT_SHOOTGRENADE", 10 );

//-----------------------------------------------------------------------------
// Purpose: Counts the accumulated # of primary and secondary attacks from all
//			weapons (except grav gun).  If bBulletOnly is true, only counts
//			attacks with ammo that does bullet damage.
//-----------------------------------------------------------------------------
int CalcPlayerAttacks( bool bBulletOnly )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	CAmmoDef *pAmmoDef = GetAmmoDef();
	if ( !pPlayer || !pAmmoDef )
		return 0;

	int iTotalAttacks = 0;
	int iWeapons = pPlayer->WeaponCount();
	for ( int i = 0; i < iWeapons; i++ )
	{
		CBaseHLCombatWeapon *pWeapon = dynamic_cast<CBaseHLCombatWeapon *>( pPlayer->GetWeapon( i ) );
		if ( pWeapon )
		{
			// add primary attacks if we were asked for all attacks, or only if it uses bullet ammo if we were asked to count bullet attacks
			if ( !bBulletOnly || ( pAmmoDef->m_AmmoType[pWeapon->GetPrimaryAmmoType()].nDamageType == DMG_BULLET ) )
			{
				iTotalAttacks += pWeapon->m_iPrimaryAttacks;
			}
			// add secondary attacks if we were asked for all attacks, or only if it uses bullet ammo if we were asked to count bullet attacks
			if ( !bBulletOnly || ( pAmmoDef->m_AmmoType[pWeapon->GetSecondaryAmmoType()].nDamageType == DMG_BULLET ) )
			{
				iTotalAttacks += pWeapon->m_iSecondaryAttacks;
			}
		}
	}
	return iTotalAttacks;
}

// achievements which are won by a map event firing once
DECLARE_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_CALIBER_FULLYTRAINED, "CALIBER_ACHIEVEMENT_FULLYTRAINED", 5 );
DECLARE_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_CALIBER_NONLETHAL, "CALIBER_ACHIEVEMENT_NONLETHAL", 5 );
DECLARE_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_CALIBER_EXTRALETHAL, "CALIBER_ACHIEVEMENT_EXTRALETHAL", 5 );
DECLARE_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_CALIBER_TRIO, "CALIBER_ACHIEVEMENT_TRIO", 5 );
DECLARE_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_CALIBER_STROBELIGHT, "CALIBER_ACHIEVEMENT_STROBELIGHT", 5 );

#endif	// ( defined( HL2_DLL ) || defined( HL2_EPISODIC ) ) && ( !defined ( PORTAL ) )

#endif // GAME_DLL
