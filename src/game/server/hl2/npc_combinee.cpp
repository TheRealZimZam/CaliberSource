//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Combine shock troop
//
//=============================================================================//

#include "cbase.h"
#include "ai_hull.h"
#include "ai_motor.h"
#include "npc_combinee.h"
#include "bitstring.h"
#include "engine/IEngineSound.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "npcevent.h"
#include "hl2/hl2_player.h"
#include "game.h"
#include "ammodef.h"
#include "explode.h"
#include "ai_memory.h"
#include "Sprite.h"
#include "soundenvelope.h"
#include "weapon_physcannon.h"
#include "hl2_gamerules.h"
#include "gameweaponmanager.h"
#include "vehicle_base.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define AE_SOLDIER_BLOCK_PHYSICS		20 // trying to block an incoming physics object

extern ConVar npc_combine_limp_health;
#define COMBINE_ELITE_LIMP_HEALTH		npc_combine_limp_health.GetFloat()

ConVar	sk_combine_e_health( "sk_combine_e_health","0");
ConVar	sk_combine_e_kick( "sk_combine_e_kick","0");

extern ConVar sk_plr_dmg_buckshot;	
extern ConVar sk_plr_num_shotgun_pellets;
extern ConVar sk_plr_dmg_ar2;
extern ConVar combine_drop_health;

LINK_ENTITY_TO_CLASS( npc_combine_e, CNPC_CombineE );
LINK_ENTITY_TO_CLASS( npc_ancorp_e, CNPC_CombineE );

//-----------------------------------------------------------------------------
// Purpose: 
//
//-----------------------------------------------------------------------------
void CNPC_CombineE::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	m_iHealth = sk_combine_e_health.GetFloat();
	SetKickDamage( sk_combine_e_kick.GetFloat() );
	m_flFieldOfView			= 0.2;	//*150

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
//	if ( !FClassnameIs( GetActiveWeapon(), "weapon_sniperrifle" ) )
//	{
		CapabilitiesAdd( bits_CAP_INNATE_RANGE_ATTACK2 );
//	}
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT | bits_CAP_MOVE_JUMP );
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );

	m_flStopMoveShootTime = FLT_MAX; // Move and shoot defaults on.
	m_MoveAndShootOverlay.SetInitialDelay( 0.75 ); // But with a bit of a delay.

	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CombineE::Precache()
{
	if( !GetModelName() )
	{
		SetModelName( MAKE_STRING( "models/combine_elite.mdl" ) );
	}

	PrecacheModel( STRING( GetModelName() ) );

	UTIL_PrecacheOther( "item_healthvial" );
	UTIL_PrecacheOther( "weapon_frag" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Sounds
//-----------------------------------------------------------------------------
void CNPC_CombineE::DeathSound( const CTakeDamageInfo &info )
{
	// NOTE: The response system deals with this at the moment
	if ( GetFlags() & FL_DISSOLVING )
		return;

	GetSentences()->Speak( "COMBINE_ELITE_DIE", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS ); 
}

void CNPC_CombineE::PainSound( const CTakeDamageInfo &info )
{
	if ( gpGlobals->curtime < m_flNextPainSoundTime )
		return;
	
	// NOTE: The response system deals with this at the moment
	if ( GetFlags() & FL_DISSOLVING )
		return;

	float healthRatio = (float)GetHealth() / (float)GetMaxHealth();
	if ( healthRatio > 0.0f )
	{
		// This causes it to speak it no matter what; doesn't bother with setting sounds.
		GetSentences()->Speak( "COMBINE_ELITE_PAIN", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS ); 
		m_flNextPainSoundTime = gpGlobals->curtime + 1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Soldiers use CAN_RANGE_ATTACK2 to indicate whether they can throw
//			a grenade. Because they check only every half-second or so, this
//			condition must persist until it is updated again by the code
//			that determines whether a grenade can be thrown, so prevent the 
//			base class from clearing it out. (sjb)
//-----------------------------------------------------------------------------
void CNPC_CombineE::ClearAttackConditions( )
{
	bool fCanRangeAttack2 = HasCondition( COND_CAN_RANGE_ATTACK2 );

	// Call the base class.
	BaseClass::ClearAttackConditions();

	if( fCanRangeAttack2 )
	{
		// We don't allow the base class to clear this condition because we
		// don't sense for it every frame.
		SetCondition( COND_CAN_RANGE_ATTACK2 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_CombineE::BuildScheduleTestBits( void )
{
	//Interrupt any schedule with physics danger (as long as I'm not moving or already trying to block)
	if ( m_flGroundSpeed == 0.0 && !IsCurSchedule( SCHED_FLINCH_PHYSICS ) )
	{
		SetCustomInterruptCondition( COND_HEAR_PHYSICS_DANGER );
	}

	BaseClass::BuildScheduleTestBits();
}

//---------------------------------------------------------
// Different footstep
//---------------------------------------------------------
void CNPC_CombineE::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case NPC_EVENT_LEFTFOOT:
		{
			EmitSound( "NPC_CombineE.FootstepLeft", pEvent->eventtime );
		}
		break;
	case NPC_EVENT_RIGHTFOOT:
		{
			EmitSound( "NPC_CombineE.FootstepRight", pEvent->eventtime );
		}
		break;
	case AE_SOLDIER_BLOCK_PHYSICS:
		DevMsg( "BLOCKING!\n" );
		m_fIsBlocking = true;
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CNPC_CombineE::Event_Killed( const CTakeDamageInfo &info )
{
	// Don't bother if we've been told not to, or the player has a megaphyscannon
	if ( combine_drop_health.GetBool() == false || PlayerHasMegaPhysCannon() )
	{
		BaseClass::Event_Killed( info );
		return;
	}

	CBasePlayer *pPlayer = ToBasePlayer( info.GetAttacker() );

	if ( pPlayer != NULL )
	{
		CHalfLife2 *pHL2GameRules = static_cast<CHalfLife2 *>(g_pGameRules);

		// Attempt to drop health
		if ( pHL2GameRules->NPC_ShouldDropHealth( pPlayer ) )
		{
			DropItem( "item_healthvial", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			pHL2GameRules->NPC_DroppedHealth();
		}
		
		if ( HasSpawnFlags( SF_COMBINE_NO_GRENADEDROP ) == false )
		{
			// Attempt to drop a grenade
			if ( pHL2GameRules->NPC_ShouldDropGrenade( pPlayer ) )
			{
				DropItem( "weapon_frag", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				pHL2GameRules->NPC_DroppedGrenade();
			}
		}
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CombineE::IsLightDamage( const CTakeDamageInfo &info )
{
	return BaseClass::IsLightDamage( info );
}

bool CNPC_CombineE::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// AR2 fire to chest/head is always heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("AR2") )
	{
		if ( info.GetDamage() >= sk_plr_dmg_ar2.GetFloat() )
			return true;
	}

	// Shotgun blasts where at least half the pellets hit me are heavy damage
	if ( info.GetDamageType() & DMG_BUCKSHOT )
	{
		int iHalfMax = sk_plr_dmg_buckshot.GetFloat() * sk_plr_num_shotgun_pellets.GetInt() * 0.5;
		if ( info.GetDamage() >= iHalfMax )
			return true;
	}
	else if ( info.GetDamageType() & (DMG_BULLET|DMG_SHOCK) )
	{
		// Any bullets that do more than 10 damage
		if ( info.GetDamage() >= 10 )
			return true;
	}

	// Rifle-grenades (This grenade is too quick to yell "Grenade!", so yell the heavy damage after it hits)
	if( info.GetAmmoType() == GetAmmoDef()->Index("SMG1_Grenade") )
	{
		return true;
	}
	
	return BaseClass::IsHeavyDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CombineE::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	if( gpGlobals->curtime >= m_flStopMoveShootTime )
	{
		// Time to stop move and shoot and start facing the way I'm running.
		// This makes the combine look attentive when disengaging, but prevents
		// them from always running around facing you.
		//
		// Only do this if it won't be immediately shut off again.
		if( GetNavigator()->GetPathTimeToGoal() > 1.0f )
		{
			m_MoveAndShootOverlay.SuspendMoveAndShoot( 5.0f );
			m_flStopMoveShootTime = FLT_MAX;
		}
	}

	if( m_flGroundSpeed > 0 && GetState() == NPC_STATE_COMBAT && m_MoveAndShootOverlay.IsSuspended() )
	{
		// Return to move and shoot when near my goal so that I 'tuck into' the location facing my enemy.
		if( GetNavigator()->GetPathTimeToGoal() <= 1.0f )
		{
			m_MoveAndShootOverlay.SuspendMoveAndShoot( 0 );
		}
	}

}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CNPC_CombineE::ShouldMoveAndShoot()
{
	m_flStopMoveShootTime = FLT_MAX;

	// Dont move n' shoot when hurt
	if ( m_iHealth <= COMBINE_ELITE_LIMP_HEALTH )
		return false;

	// If you hear danger, focus on running for the first second or two
	if( HasCondition( COND_HEAR_DANGER, false ) )
		m_flStopMoveShootTime = gpGlobals->curtime + random->RandomFloat( 1.0f, 2.0f );
	
	return BaseClass::ShouldMoveAndShoot();
}
