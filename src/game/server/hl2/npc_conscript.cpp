//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The shamed and castrated military of Earf.
//
//=============================================================================//

#include "cbase.h"
#include "soundent.h"
#include "npcevent.h"
#include "npc_playercompanion.h"
#include "ai_squad.h"
#include "ai_pathfinder.h"
#include "ai_route.h"
#include "ai_hint.h"
#include "ai_interactions.h"
#include "ai_looktarget.h"
#include "ai_behavior.h"
#include "ai_behavior_functank.h"
#include "sceneentity.h"
#include "tier0/ICommandLine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define CONSCRIPT_MODEL "models/Conscript.mdl"

ConVar	sk_conscript_health( "sk_conscript_health","0");

Activity ACT_CONSCRIPT_SUPPRESS;

//=========================================================
// Conscript
//=========================================================

class CNPC_Conscript : public CNPC_PlayerCompanion
{
	DECLARE_CLASS( CNPC_Conscript, CNPC_PlayerCompanion );
public:
//	DECLARE_SERVERCLASS();

	virtual void Precache()
	{
		// Prevents a warning
		SelectModel( );
		BaseClass::Precache();

		PrecacheScriptSound( "NPC_Conscript.FootstepLeft" );
		PrecacheScriptSound( "NPC_Conscript.FootstepRight" );
		PrecacheScriptSound( "NPC_Conscript.Die" );

		PrecacheInstancedScene( "scenes/expressions/citizenidle.vcd" );
		PrecacheInstancedScene( "scenes/expressions/citizenalert_loop.vcd" );
		PrecacheInstancedScene( "scenes/expressions/citizencombat_loop.vcd" );
	}

	void	Spawn( void );
	void	SelectModel();
	Class_T Classify( void );

	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

	bool CreateBehaviors( void );
	int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual int		SelectSchedule( void );
	
	void HandleAnimEvent( animevent_t *pEvent );
//	string_t 		GetModelName() const;
	
	bool ShouldLookForBetterWeapon() { return true; }

	void OnChangeRunningBehavior( CAI_BehaviorBase *pOldBehavior,  CAI_BehaviorBase *pNewBehavior );

	void DeathSound( const CTakeDamageInfo &info );
	void UseFunc( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	CAI_FuncTankBehavior		m_FuncTankBehavior;
	COutputEvent				m_OnPlayerUse;

private:
	//=========================================================
	// Schedules
	//=========================================================
	enum
	{
		SCHED_CONSCRIPT_SUPPRESS = BaseClass::NEXT_SCHEDULE,
		NEXT_SCHEDULE,
	};

	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};


LINK_ENTITY_TO_CLASS( npc_conscript, CNPC_Conscript );
//!!!IMPLEMENT_CUSTOM_AI( npc_citizen, CNPC_Conscript );

//---------------------------------------------------------
// 
//---------------------------------------------------------
#if 0
IMPLEMENT_SERVERCLASS_ST(CNPC_Conscript, DT_NPC_Conscript)
END_SEND_TABLE()
#endif

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Conscript )
//						m_FuncTankBehavior
	DEFINE_OUTPUT( m_OnPlayerUse, "OnPlayerUse" ),
	DEFINE_USEFUNC( UseFunc ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::SelectModel()
{
	SetModelName( AllocPooledString( CONSCRIPT_MODEL ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::Spawn( void )
{
	Precache();

	m_iHealth = sk_conscript_health.GetFloat();

	m_iszIdleExpression = MAKE_STRING("scenes/expressions/citizenidle.vcd");
	m_iszAlertExpression = MAKE_STRING("scenes/expressions/citizenalert_loop.vcd");
	m_iszCombatExpression = MAKE_STRING("scenes/expressions/citizencombat_loop.vcd");

	BaseClass::Spawn();

	m_NPCState			= NPC_STATE_NONE;
	
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT | bits_CAP_DUCK );
	AddEFlags( EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );

	NPCInit();

	SetUse( &CNPC_Conscript::UseFunc );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_Conscript::Classify( void )
{
	return	CLASS_CONSCRIPT;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_Conscript::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	// Conscripts are better with the AR2 than citizens
	if( FClassnameIs( pWeapon, "weapon_ar2" ) )
	{
		return WEAPON_PROFICIENCY_VERY_GOOD;
	}
	else if( FClassnameIs( pWeapon, "weapon_smg1" ) )
	{
		return WEAPON_PROFICIENCY_GOOD;
	}

	return BaseClass::CalcWeaponProficiency( pWeapon );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Conscript::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case NPC_EVENT_LEFTFOOT:
		{
			EmitSound( "NPC_Conscript.FootstepLeft", pEvent->eventtime );
		}
		break;
	case NPC_EVENT_RIGHTFOOT:
		{
			EmitSound( "NPC_Conscript.FootstepRight", pEvent->eventtime );
		}
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_Conscript::DeathSound( const CTakeDamageInfo &info )
{
	// Sentences don't play on dead NPCs
	SentenceStop();

	EmitSound( "NPC_Conscript.die" );
}

bool CNPC_Conscript::CreateBehaviors( void )
{
	BaseClass::CreateBehaviors();
	AddBehavior( &m_FuncTankBehavior );

	return true;
}

void CNPC_Conscript::OnChangeRunningBehavior( CAI_BehaviorBase *pOldBehavior,  CAI_BehaviorBase *pNewBehavior )
{
	if ( pNewBehavior == &m_FuncTankBehavior )
	{
		m_bReadinessCapable = false;
	}
	else if ( pOldBehavior == &m_FuncTankBehavior )
	{
		m_bReadinessCapable = IsReadinessCapable();
	}

	BaseClass::OnChangeRunningBehavior( pOldBehavior, pNewBehavior );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CNPC_Conscript::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// make sure friends talk about it if player hurts allies
	int ret = BaseClass::OnTakeDamage_Alive( info );
	if (!IsAlive())
	{
		return ret;
	}

	CTakeDamageInfo newInfo = info;

	if( IsInSquad() && (info.GetDamageType() & DMG_BLAST) && info.GetInflictor() )
	{
		// Blast damage. If this kills a squad member, give the 
		// remaining citizens a resistance bonus to this inflictor
		// to try to avoid having the entire squad wiped out by a
		// single explosion.
		if( m_pSquad->IsSquadInflictor( info.GetInflictor() ) )
		{
			newInfo.ScaleDamage( 0.5 );
		}
		else
		{
			// If this blast is going to kill me, designate the inflictor
			// so that the rest of the squad can enjoy a damage resist.
			if( info.GetDamage() >= GetHealth() )
			{
					m_pSquad->SetSquadInflictor( info.GetInflictor() );
			}
		}
	}

#if 0
	if ( m_NPCState != NPC_STATE_PRONE && (info.GetAttacker()->GetFlags() & FL_CLIENT) )
	{
		// This is a heurstic to determine if the player intended to harm me
		// If I have an enemy, we can't establish intent (may just be crossfire)
		if ( GetEnemy() == NULL )
		{
			// If I'm already suspicious, get mad
			if (m_afMemory & bits_MEMORY_SUSPICIOUS)
			{
				
				Speak( TLK_BETRAYED );

				Remember( bits_MEMORY_PROVOKED );

				CapabilitiesRemove(bits_CAP_NO_HIT_PLAYER);

			//	StopFollowing();
			}
			else
			{
				// Watch where you're bloody shooting!
				Speak( TLK_NOSHOOT );
				Remember( bits_MEMORY_SUSPICIOUS );
			}
		}
		else if ( !(GetEnemy()->IsPlayer()) && (m_lifeState != LIFE_DEAD ))
		{
			Speak( TLK_SHOT );
		}
	}
#endif
	return ret;
}

int CNPC_Conscript::SelectSchedule( void )
{
	switch	( m_NPCState )
	{
	case NPC_STATE_COMBAT:
		{
			// -----------
			// New Enemy
			// -----------
			if ( HasCondition( COND_ENEMY_OCCLUDED ) )
			{
				// Conscripts never wait to establish LOS
				if( GetEnemy() && !(GetEnemy()->GetFlags() & FL_NOTARGET) )
				{
					// Charge in and break the enemy's cover!
					return SCHED_ESTABLISH_LINE_OF_FIRE;
				}

				// Fallback case
				Remember( bits_MEMORY_INCOVER );
				return SCHED_STANDOFF;
			}
			else if ( HasCondition( COND_SEE_ENEMY ) && !HasCondition( COND_CAN_RANGE_ATTACK1 ) )
			{
				// Get into range!
				if ( HasCondition( COND_TOO_FAR_TO_ATTACK ) )
				{
					return SCHED_MOVE_TO_WEAPON_RANGE;
				}
				return SCHED_ESTABLISH_LINE_OF_FIRE;
			}
		}
		break;
	}

	// no special cases here, call the base class
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Conscript::UseFunc( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_bDontUseSemaphore = true;
	SpeakIfAllowed( TLK_USE );
	m_bDontUseSemaphore = false;

	m_OnPlayerUse.FireOutput( pActivator, pCaller );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_conscript, CNPC_Conscript )

DECLARE_ACTIVITY( ACT_CONSCRIPT_SUPPRESS )

 //=========================================================
 // SUPPRESS
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_CONSCRIPT_SUPPRESS,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_FACE_ENEMY				0"
 "		TASK_WAIT					0.5"
 "		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_CONSCRIPT_SUPPRESS"
 "		TASK_RANGE_ATTACK1			0"
 ""
 "	Interrupts"
 "		COND_ENEMY_DEAD"
 "		COND_HEAVY_DAMAGE"
 "		COND_NO_PRIMARY_AMMO"
 "		COND_TOO_CLOSE_TO_ATTACK"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 "		COND_WEAPON_BLOCKED_BY_FRIEND"
 );

AI_END_CUSTOM_NPC()
