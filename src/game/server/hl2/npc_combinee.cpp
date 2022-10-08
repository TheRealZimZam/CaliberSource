//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Combine shock troop
//
//=============================================================================//

#include "cbase.h"
#include "npc_combinee.h"
#include "ai_hull.h"
#include "ai_motor.h"
#include "ai_squadslot.h"
#include "ai_squad.h"
#include "ai_route.h"
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
ConVar	sk_combine_e_jump( "sk_combine_e_jump","1");	//Is jumping allowed?

extern ConVar sk_plr_dmg_buckshot;	
extern ConVar sk_plr_num_shotgun_pellets;
extern ConVar sk_plr_dmg_ar2;
extern ConVar npc_combine_drop_health;

LINK_ENTITY_TO_CLASS( npc_combine_e, CNPC_CombineE );
LINK_ENTITY_TO_CLASS( npc_elite, CNPC_CombineE );

//-----------------------------------------------------------------------------
// Purpose: 
//
//-----------------------------------------------------------------------------
void CNPC_CombineE::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	SetHealth( sk_combine_e_health.GetFloat() );
	SetMaxHealth( sk_combine_e_health.GetFloat() );
	SetKickDamage( sk_combine_e_kick.GetFloat() );
	m_flFieldOfView			= 0.2;	//*150

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	// Sniper elites dont use grenades
	if ( !FClassnameIs( this, "npc_sniper" ))
	{
		CapabilitiesAdd( bits_CAP_INNATE_RANGE_ATTACK2 );
	}
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );

	if ( sk_combine_e_jump.GetBool() )
	{
		CapabilitiesAdd( bits_CAP_MOVE_JUMP );
	}

	BaseClass::Spawn();

	m_MoveAndShootOverlay.SetInitialDelay( 0.25 );
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CombineE::Precache()
{
	m_fIsElite = true;

	if( !GetModelName() )
	{
		SetModelName( MAKE_STRING( "models/combine_elite.mdl" ) );
	}

	PrecacheModel( STRING( GetModelName() ) );

//	PrecacheScriptSound( "NPC_Combine.Die" );
	PrecacheScriptSound( "NPC_Combine.DieIWHBYD" );

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

	const char *pSentenceName = "COMBINE_ELITE_DIE";
	if ( ShouldGib( info ) )
	{
		pSentenceName = "COMBINE_GIB";
	}

	GetSentences()->Speak( pSentenceName, SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS ); 
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

#if 0
void CNPC_CombineE::IdleSound( void )
{
	if (random->RandomInt(0,1))
	{
		GetSentences()->Speak( "COMBINE_ELITE_IDLE" );
	}
}

void CNPC_CombineE::AlertSound( void )
{
	if ( gpGlobals->curtime > m_flNextAlertSoundTime )
	{
		GetSentences()->Speak( "COMBINE_ELITE_ALERT", SENTENCE_PRIORITY_HIGH );
		m_flNextAlertSoundTime = gpGlobals->curtime + random->RandomFloat( 15, 25 );
	}
}

void CNPC_CombineE::LostEnemySound( void )
{
	if ( gpGlobals->curtime <= m_flNextLostSoundTime )
		return;

	if (!(CBaseEntity*)GetEnemy() || gpGlobals->curtime - GetEnemyLastTimeSeen() >= 15)
	{
		GetSentences()->Speak( "COMBINE_ELITE_LOST_SHORT" );
		m_flNextLostSoundTime = gpGlobals->curtime + random->RandomFloat( 5, 15 );
	}
}

void CNPC_CombineE::AnnounceAssault( void )
{
	if (!m_pSquad)
		return;

	if (!FOkToMakeSound())
		return;

	GetSentences()->Speak( "COMBINE_ELITE_ASSAULT", SENTENCE_PRIORITY_MEDIUM );
}

void CNPC_CombineE::AnnounceEnemyType( CBaseEntity *pEnemy )
{
	AlertSound();
}

void CNPC_CombineE::AnnounceEnemyKill( CBaseEntity *pEnemy )
{
	if (!pEnemy )
		return;

	// 50% chance
	if (random->RandomInt(0,1))
		return;

	GetSentences()->Speak( "COMBINE_ELITE_KILL", SENTENCE_PRIORITY_MEDIUM );
}

void CNPC_CombineE::NotifyDeadFriend( CBaseEntity* pFriend )
{
	if ( GetSquad()->NumMembers() < 2 )
	{
		GetSentences()->Speak( "COMBINE_ELITE_LAST_OF_SQUAD", SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL );
		JustMadeSound();
		return;
	}
	BaseClass::NotifyDeadFriend(pFriend);
}

void CNPC_CombineE::SpeakSentence( int sentenceType )
{
	switch( sentenceType )
	{
	case 0: // assault
		AnnounceAssault();
		break;

	case 1: // Flanking the player
		// If I'm moving more than 20ft, I need to talk about it
		if ( GetNavigator()->GetPath()->GetPathLength() > 20 * 12.0f )
		{
			if ( m_pSquad )
			{
				AnnounceAssault();
			}
		}
		break;
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: StartTask
//-----------------------------------------------------------------------------
void CNPC_CombineE::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_ANNOUNCE_ATTACK:
		{
			// If Primary Attack
			if ((int)pTask->flTaskData == 1)
			{
				// -----------------------------------------------------------
				// If enemy isn't facing me and I haven't attacked in a while
				// annouce my attack before I start wailing away
				// -----------------------------------------------------------
				CBaseCombatCharacter *pBCC = GetEnemyCombatCharacterPointer();

				if	(pBCC && pBCC->IsPlayer() && (!pBCC->FInViewCone( this )) &&
					(gpGlobals->curtime - m_flLastAttackTime > 3.0) )
				{
					m_flLastAttackTime = gpGlobals->curtime;

					// Wait one second
					SetWait( 1.0 );

					//TODO; Is this bit necessary?
					if ( !IsCrouching() )
					{
						SetActivity(ACT_IDLE);
					}
					else
					{
						SetActivity(ACT_COWER); // This is really crouch idle???
					}
				}
				else
				{
					TaskComplete();
				}
			}
			else
			{
				GetSentences()->Speak( "COMBINE_ELITE_THROW", SENTENCE_PRIORITY_HIGH );
				SetActivity(ACT_IDLE);

				// Wait two seconds
				SetWait( 2.0 );
			}
			break;
		}

	default: 
		BaseClass:: StartTask( pTask );
		break;
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
			MakeAIFootstepSound( 240.0f );
			EmitSound( "NPC_CombineE.FootstepLeft", pEvent->eventtime );
		}
		break;
	case NPC_EVENT_RIGHTFOOT:
		{
			MakeAIFootstepSound( 240.0f );
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
	if ( npc_combine_drop_health.GetBool() == false || PlayerHasMegaPhysCannon() )
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
// 
//-----------------------------------------------------------------------------
bool CNPC_CombineE::ShouldMoveAndShoot()
{
#if 0
	m_flStopMoveShootTime = FLT_MAX;

	// If you hear danger, focus on running for the first second or two
	if( HasCondition( COND_HEAR_DANGER ) )
		m_flStopMoveShootTime = gpGlobals->curtime + random->RandomFloat( 1.0f, 2.0f );
#endif

	// Dont move n' shoot when hurt
	if ( m_iHealth <= COMBINE_ELITE_LIMP_HEALTH )
		return false;

	return BaseClass::ShouldMoveAndShoot();
}
