//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "tier0/vprof.h"
#include "hl2_gamerules.h"

#ifdef CLIENT_DLL
#include "prediction.h"
#include "hl2_player_shared.h"
#define CRecipientFilter C_RecipientFilter
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Consider the weapon's built-in accuracy, this character's proficiency with
// the weapon, and the status of the target. Use this information to determine
// how accurately to shoot at the target.
//-----------------------------------------------------------------------------
Vector CHL2_Player::GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	if ( pWeapon )
		return pWeapon->GetBulletSpread( WEAPON_PROFICIENCY_PERFECT );

	return VECTOR_CONE_15DEGREES;
}

// This is taken care of by gamerules
#if 0
bool CHL2_Player::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	if ( CHalfLife2().IsTeamplay() )
	{
		if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT || collisionGroup == COLLISION_GROUP_PROJECTILE )
		{
			switch( GetTeamNumber() )
			{
			case TEAM_REBELS:
				if ( !( contentsMask & CONTENTS_TEAM2 ) )
					return false;
				break;

			case TEAM_COMBINE:
				if ( !( contentsMask & CONTENTS_TEAM1 ) )
					return false;
				break;
			}
		}
	}

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}
#endif

