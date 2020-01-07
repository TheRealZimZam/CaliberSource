//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
// STORY CHARACTER
// Purpose: A pissed-off, stuck in the closet German with a very big iron.
//
//=============================================================================//
// TODO:
// 1. Fix the response speech, add custom entries if needed.
// 2. Satchel charge ability, thrown when hinted by map.
// 3. Iron bugs
//
//
// ASSUMPTIONS MADE:
//
// You're making a character based on CAI_BaseNPC. If this 
// is not true, make sure you replace all occurances
// of 'CAI_BaseNPC' in this file with the appropriate 
// parent class.
//
// You're making a human-sized NPC that walks.
//
//=============================================================================//
// Base behavior files
#include "cbase.h"
#include "ai_default.h"
#include "ai_task.h"
#include "ai_schedule.h"
#include "ai_node.h"
#include "ai_hull.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_senses.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "ai_behavior.h"
#include "ai_baseactor.h"
#include "ai_behavior_lead.h"
#include "ai_behavior_follow.h"
#include "ai_behavior_standoff.h"
#include "ai_behavior_assault.h"
#include "ai_behavior_functank.h"
// Base engine files
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
//#include "entitylist.h"
#include "activitylist.h"
//#include "ai_basenpc.h"
#include "engine/IEngineSound.h"
#include "vstdlib/random.h"
#include "sceneentity.h"
// Extras
#include "npc_playercompanion.h"
#include "info_darknessmode_lightsource.h"
#include "weapon_flaregun.h"

#include "env_debughistory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Define stuff
#define FRANZ_MODEL "models/Barney.mdl"

#define FRANZ_MIN_MOB_DIST_SQR Square(120)		// Any enemy closer than this adds to the 'mob' 
#define FRANZ_MIN_CONSIDER_DIST Square(1200)	// Only enemies within this range are counted and considered to generate AI speech

#define FRANZ_MIN_ENEMY_DIST_TO_CROUCH			384			// Minimum distance that enemy must be to crouch
#define FRANZ_MIN_ENEMY_HEALTH_TO_CROUCH		20 			// Dont crouch for small enemies like headcrabs and the like
#define FRANZ_CROUCH_DELAY						5			// Time after crouching before Franz will consider crouching again

ConVar	sk_franz_health( "sk_franz_health","0");

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNPC_Franz : public CNPC_PlayerCompanion
{
public:
	DECLARE_CLASS( CNPC_Franz, CNPC_PlayerCompanion );
	DECLARE_SERVERCLASS();

	virtual void Precache()
	{
		// Prevents a warning
		SelectModel( );
		BaseClass::Precache();

		// Reusing boston's sounds for now
		PrecacheScriptSound( "npc_boston.FootstepLeft" );
		PrecacheScriptSound( "npc_boston.FootstepRight" );
		PrecacheScriptSound( "npc_boston.die" );

		PrecacheInstancedScene( "scenes/Expressions/FranzIdle.vcd" );
		PrecacheInstancedScene( "scenes/Expressions/FranzAlert.vcd" );
		PrecacheInstancedScene( "scenes/Expressions/FranzCombat.vcd" );
	}

	void	Spawn( void );
	void	SelectModel();
	Class_T Classify( void );
	void	Weapon_Equip( CBaseCombatWeapon *pWeapon );

	bool CreateBehaviors( void );
	void Activate();
	void PrescheduleThink( void );

	void HandleAnimEvent( animevent_t *pEvent );
	int	GetSoundInterests ( void );
	bool ShouldLookForBetterWeapon() { return true; }

	void OnChangeRunningBehavior( CAI_BehaviorBase *pOldBehavior,  CAI_BehaviorBase *pNewBehavior );

	void DeathSound( const CTakeDamageInfo &info );
	void PainSound( const CTakeDamageInfo &info );
	
	void GatherConditions();
	void UseFunc( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	bool	OnBeginMoveAndShoot();
	void	SpeakAttacking( void );
	void	EnemyIgnited( CAI_BaseNPC *pVictim );
	virtual void	BarnacleDeathSound( void );

	// Custom AI
	void	DoCustomCombatAI( void );
	void	DoMobbedCombatAI( void );
//	void	DoCustomSpeechAI( void );

	// Blinding
	virtual void PlayerHasIlluminatedNPC( CBasePlayer *pPlayer, float flDot );
	void		 CheckBlindedByFlare( void );
	bool		 CanBeBlindedByFlashlight( bool bCheckLightSources );
	bool		 PlayerFlashlightOnMyEyes( CBasePlayer *pPlayer );
	bool		 BlindedByFlare( void );
//	bool		 CanReload( void );
	
	//---------------------------------
	// Special abilities
	//---------------------------------
	
//	bool 			CanHeal();
//	bool 			ShouldHealTossTarget( CBaseEntity *pTarget, bool bActiveUse = false );
//
//	void 			Heal();
//
//	bool			ShouldLookForHealthItem();
//
//	void			TossHealthKit( CBaseCombatCharacter *pThrowAt, const Vector &offset ); // create a healthkit and throw it at someone
//	void			InputForceHealthKitToss( inputdata_t &inputdata );
	PassengerState_e	GetPassengerState( void );
	bool	RunningPassengerBehavior( void );

	CAI_FuncTankBehavior		m_FuncTankBehavior;
	COutputEvent				m_OnPlayerUse;
	
private:
	CHandle<CAI_Hint>	m_hStealthLookTarget;
	float   m_fCombatStartTime;
	float	m_fCombatEndTime;
	float	m_flNextCrouchTime;
	bool	m_bIsFlashlightBlind;
	float	m_fStayBlindUntil;
	float	m_flDontBlindUntil;
	string_t m_iszCurrentBlindScene;
	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );
	
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};

LINK_ENTITY_TO_CLASS( npc_franz, CNPC_Franz );
//IMPLEMENT_CUSTOM_AI( npc_conscript, CNPC_Franz );

//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST(CNPC_Franz, DT_NPC_Franz)
END_SEND_TABLE()

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Franz )
//					m_FuncTankBehavior
	DEFINE_FIELD( m_fCombatStartTime, FIELD_TIME ),
	DEFINE_FIELD( m_fCombatEndTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextCrouchTime, FIELD_TIME ),
	DEFINE_FIELD( m_hStealthLookTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bIsFlashlightBlind, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fStayBlindUntil, FIELD_TIME ),
	DEFINE_FIELD( m_flDontBlindUntil, FIELD_TIME ),
	DEFINE_FIELD( m_iszCurrentBlindScene, FIELD_STRING ),

	DEFINE_OUTPUT( m_OnPlayerUse, "OnPlayerUse" ),

	DEFINE_USEFUNC( Use ),
	
END_DATADESC()

//-----------------------------------------------------------------------------
// Classify - indicates this NPC's place in the 
// relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_Franz::Classify( void )
{
	return	CLASS_PLAYER_ALLY_VITAL;
}

void CNPC_Franz::Activate()
{
	// Avoids problems with players saving the game in places where he dies immediately afterwards.
	m_iHealth = sk_franz_health.GetFloat();
	
	BaseClass::Activate();
	
	// Assume boston has already said hello
	SetSpokeConcept( TLK_HELLO, NULL, false );
}

//-----------------------------------------------------------------------------
// HandleAnimEvent - catches the NPC-specific messages
// that occur when tagged animation frames are played.
//-----------------------------------------------------------------------------
void CNPC_Franz::HandleAnimEvent( animevent_t *pEvent )
{
	
	switch( pEvent->event )
	{
	case NPC_EVENT_LEFTFOOT:
		{
			EmitSound( "npc_boston.FootstepLeft", pEvent->eventtime );
		}
		break;
	case NPC_EVENT_RIGHTFOOT:
		{
			EmitSound( "npc_boston.FootstepRight", pEvent->eventtime );
		}
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

//---------------------------------------------------------------------------------
// GetSoundInterests - Franz can hear some things, but will only comment about it.
//---------------------------------------------------------------------------------
int CNPC_Franz::GetSoundInterests( void )
{
	return	SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_DANGER | SOUND_PHYSICS_DANGER | SOUND_BULLET_IMPACT | SOUND_MOVE_AWAY;
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CNPC_Franz::Spawn()
{
	// Allow custom model usage (mostly for monitors)
//	char *szModel = (char *)STRING( GetModelName() );
//	if (!szModel || !*szModel)
//	{
//		szModel = "models/Franz.mdl";
//		SetModelName( AllocPooledString(szModel) );
//	}

	Precache();

	BaseClass::Spawn();

	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_RED );
	
	m_iszIdleExpression = MAKE_STRING("scenes/Expressions/FranzIdle.vcd");
	m_iszAlertExpression = MAKE_STRING("scenes/Expressions/FranzAlert.vcd");
	m_iszCombatExpression = MAKE_STRING("scenes/Expressions/FranzCombat.vcd");
	
	m_iHealth			= sk_franz_health.GetFloat();
	m_flFieldOfView		= 0.5;// indicates the width of this NPC's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;
	
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP | bits_CAP_OPEN_DOORS | bits_CAP_ANIMATEDFACE | bits_CAP_TURN_HEAD | bits_CAP_DUCK | bits_CAP_SQUAD );
	CapabilitiesAdd( bits_CAP_AIM_GUN );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );

	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );

	NPCInit();
	
	SetUse( &CNPC_Franz::UseFunc );
	
	m_fCombatStartTime = 0.0f;
	m_fCombatEndTime   = 0.0f;

	// Dont set too low, otherwise he wont shut-up
	m_AnnounceAttackTimer.Set( 4, 6 );
	
}

void CNPC_Franz::PainSound( const CTakeDamageInfo &info )
{
	SpeakIfAllowed( TLK_SHOT );
}

void CNPC_Franz::DeathSound( const CTakeDamageInfo &info )
{
	// Sentences don't play on dead NPCs
	SentenceStop();

	EmitSound( "npc_franz.die" );

}

void CNPC_Franz::BarnacleDeathSound( void )
{
	Speak( TLK_SELF_IN_BARNACLE );
}

//-----------------------------------------------------------------------------
// Precache - precaches all resources/functions this NPC needs
//-----------------------------------------------------------------------------
void CNPC_Franz::SelectModel()
{
	SetModelName( AllocPooledString( FRANZ_MODEL ) );
}

void CNPC_Franz::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	BaseClass::Weapon_Equip( pWeapon );

	if( hl2_episodic.GetBool() && FClassnameIs( pWeapon, "weapon_ar2" ) )
	{
		// Allow Boston to defend himself at point-blank range in hydro.
		pWeapon->m_fMinRange1 = 0.0f;
	}
}

bool CNPC_Franz::CreateBehaviors( void )
{
	AddBehavior( &m_FuncTankBehavior );
	BaseClass::CreateBehaviors();

	return true;
}

void CNPC_Franz::OnChangeRunningBehavior( CAI_BehaviorBase *pOldBehavior,  CAI_BehaviorBase *pNewBehavior )
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
void CNPC_Franz::GatherConditions()
{
	BaseClass::GatherConditions();

	if( HasCondition( COND_HEAR_DANGER ) )
	{
		// A small fix copied from Alyx. Don't worry about combat sounds if in panic. 
		ClearCondition( COND_HEAR_COMBAT );
	}
	
	// Handle speech AI. Don't do AI speech if we're in scripts unless permitted by the EnableSpeakWhileScripting input.
	if ( m_NPCState == NPC_STATE_IDLE || m_NPCState == NPC_STATE_ALERT || m_NPCState == NPC_STATE_COMBAT ||
		( ( m_NPCState == NPC_STATE_SCRIPT ) && CanSpeakWhileScripting() ) )
	{
		DoCustomSpeechAI();
	}
	
	// Custom combat AI
	if ( m_NPCState == NPC_STATE_COMBAT )
	{
		DoCustomCombatAI();
	}
	
	// Flinching gestures -- Might add this for other humanoid ai's too
	// Mainly for explosions
	if( HasCondition(COND_HEAR_COMBAT) )
	{
		CSound *pSound = GetBestSound(); 

			if( (pSound->SoundTypeNoContext() & SOUND_COMBAT) && (pSound->SoundContext() & SOUND_CONTEXT_EXPLOSION) )
			{
					if ( !IsPlayingGesture(ACT_GESTURE_FLINCH_BLAST) && !IsPlayingGesture(ACT_GESTURE_FLINCH_BLAST_DAMAGED) )
					{
						RestartGesture( ACT_GESTURE_FLINCH_BLAST );
						GetShotRegulator()->FireNoEarlierThan( gpGlobals->curtime + SequenceDuration( ACT_GESTURE_FLINCH_BLAST ) + 0.5f ); // One sec cooldown
					}
			}
		
	}
	
//	if ( (GetFlags() & FL_FLY) && m_NPCState != NPC_STATE_SCRIPT && !m_ActBusyBehavior.IsActive() && !m_PassengerBehavior.IsEnabled() )
//	{
//		Warning( "Removed FL_FLY from Alyx, who wasn't running a script or actbusy. Time %.2f, map %s.\n", gpGlobals->curtime, STRING(gpGlobals->mapname) );
//		RemoveFlag( FL_FLY );
//	}
}

PassengerState_e CNPC_Franz::GetPassengerState( void )
{
	return m_PassengerBehavior.GetPassengerState();
}

//-----------------------------------------------------------------------------
// Basic thinks - Good for simple checks and actions
//-----------------------------------------------------------------------------
void CNPC_Franz::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();

	// More simple stuff copied from alyx
	// If we're in stealth mode, and we can still see the stealth node, keep using it
	if ( GetReadinessLevel() == AIRL_STEALTH )
	{
		if ( m_hStealthLookTarget && !m_hStealthLookTarget->IsDisabled() )
		{
			if ( m_hStealthLookTarget->IsInNodeFOV(this) && FVisible( m_hStealthLookTarget ) )
				return;
		}

		// Break out of stealth mode
		SetReadinessLevel( AIRL_STIMULATED, true, true );
		ClearLookTarget( m_hStealthLookTarget );
		m_hStealthLookTarget = NULL;
	}

	// If we're being blinded by the flashlight, see if we should stop
	if ( m_bIsFlashlightBlind )
	{
		if ( m_fStayBlindUntil < gpGlobals->curtime )
		{
 			CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);
 			if ( pPlayer && (!CanBeBlindedByFlashlight( true ) || !pPlayer->IsIlluminatedByFlashlight(this, NULL ) || !PlayerFlashlightOnMyEyes( pPlayer )) &&
				!BlindedByFlare() )
			{
				// Remove the actor from the flashlight scene
				ADD_DEBUG_HISTORY( HISTORY_FRANZ_BLIND, UTIL_VarArgs( "(%0.2f) Franz: end blind scene '%s'\n", gpGlobals->curtime, STRING(m_iszCurrentBlindScene) ) );
				RemoveActorFromScriptedScenes( this, true, false, STRING(m_iszCurrentBlindScene) );

				// Allow firing again, but prevent myself from firing until I'm done
				GetShotRegulator()->EnableShooting();
				GetShotRegulator()->FireNoEarlierThan( gpGlobals->curtime + 1.0 );
				
				m_bIsFlashlightBlind = false;
				m_flDontBlindUntil = gpGlobals->curtime + RandomFloat( 1, 3 );
			}
		}
	}
	else
	{
		CheckBlindedByFlare();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Franz::UseFunc( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// if I'm in the vehicle, the player is probably trying to use the vehicle
	if ( GetPassengerState() == PASSENGER_STATE_INSIDE && pActivator->IsPlayer() && GetParent() )
	{
		GetParent()->Use( pActivator, pCaller, useType, value );
		return;
	}
	m_bDontUseSemaphore = true;
	SpeakIfAllowed( TLK_USE );
	m_bDontUseSemaphore = false;

	m_OnPlayerUse.FireOutput( pActivator, pCaller );
}

//-----------------------------------------------------------------------------
// Purpose: Player has illuminated this NPC with the flashlight
//-----------------------------------------------------------------------------
void CNPC_Franz::PlayerHasIlluminatedNPC( CBasePlayer *pPlayer, float flDot )
{
 	if ( m_bIsFlashlightBlind )
		return;

	if ( !CanBeBlindedByFlashlight( true ) )
		return;

	// Ignore the flashlight if it's not shining at my eyes
	if ( PlayerFlashlightOnMyEyes( pPlayer ) )
	{
		char szResponse[AI_Response::MAX_RESPONSE_NAME];

		// Only say the blinding speech if it's time to
		if ( SpeakIfAllowed( "TLK_FLASHLIGHT_ILLUM", NULL, false, szResponse, AI_Response::MAX_RESPONSE_NAME  ) )
		{
			m_iszCurrentBlindScene = AllocPooledString( szResponse );
			ADD_DEBUG_HISTORY( HISTORY_FRANZ_BLIND, UTIL_VarArgs( "(%0.2f) Franz: start flashlight blind scene '%s'\n", gpGlobals->curtime, STRING(m_iszCurrentBlindScene) ) );
			GetShotRegulator()->DisableShooting();
			m_bIsFlashlightBlind = true;
			m_fStayBlindUntil = gpGlobals->curtime + 0.1f;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input:   bCheckLightSources - if true, checks if any light darkness lightsources are near
//-----------------------------------------------------------------------------
bool CNPC_Franz::CanBeBlindedByFlashlight( bool bCheckLightSources )
{
	// Can't be blinded if we're not in alyx darkness mode
 	/*
	if ( !HL2GameRules()->IsAlyxInDarknessMode() )
		return false;
	*/

	// Can't be blinded if I'm in a script, or in combat
	if ( IsInAScript() || GetState() == NPC_STATE_COMBAT || GetState() == NPC_STATE_SCRIPT )
		return false;
	if ( IsSpeaking() )
		return false;

	// can't be blinded if Franz is near a light source
	if ( bCheckLightSources && DarknessLightSourceWithinRadius( this, 500 ) )
		return false;

	// Not during an actbusy
	if ( m_ActBusyBehavior.IsActive() )
		return false;
	if ( m_OperatorBehavior.IsRunning() )
		return false;

	// Can't be blinded if I've been in combat recently, to fix anim snaps
	if ( GetLastEnemyTime() != 0.0 )
	{
		if ( (gpGlobals->curtime - GetLastEnemyTime()) < 2 )
			return false;
	}

	// Can't be blinded if I'm reloading
	if ( IsCurSchedule(SCHED_RELOAD, false) )
		return false;

	// Can't be blinded right after being blind, to prevent oscillation
	if ( gpGlobals->curtime < m_flDontBlindUntil )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Franz::PlayerFlashlightOnMyEyes( CBasePlayer *pPlayer )
{
	Vector vecEyes, vecPlayerForward;
 	vecEyes = EyePosition();
 	pPlayer->EyeVectors( &vecPlayerForward );

	Vector vecToEyes = (vecEyes - pPlayer->EyePosition());
	float flDist = VectorNormalize( vecToEyes ); 

	// We can be blinded in daylight, but only at close range
	if ( HL2GameRules()->IsAlyxInDarknessMode() == false )
	{
		if ( flDist > (8*12.0f) )
			return false;
	}

	float flDot = DotProduct( vecPlayerForward, vecToEyes );
	if ( flDot < 0.98 )
		return false;

	// Check facing to ensure we're in front of him
 	Vector los = ( pPlayer->EyePosition() - vecEyes );
	los.z = 0;
	VectorNormalize( los );
	Vector facingDir = EyeDirection2D();
 	flDot = DotProduct( los, facingDir );
	return ( flDot > 0.3 );
}

//-----------------------------------------------------------------------------
// Purpose: Check if player has illuminated this NPC with a flare
//-----------------------------------------------------------------------------
void CNPC_Franz::CheckBlindedByFlare( void )
{
	if ( m_bIsFlashlightBlind )
		return;

	if ( !CanBeBlindedByFlashlight( false ) )
		return;

	// Ignore the flare if it's not too close
	if ( BlindedByFlare() )
	{
		char szResponse[AI_Response::MAX_RESPONSE_NAME];

		// Only say the blinding speech if it's time to
		if ( SpeakIfAllowed( "TLK_FLASHLIGHT_ILLUM", NULL, false, szResponse, AI_Response::MAX_RESPONSE_NAME ) )
		{
			m_iszCurrentBlindScene = AllocPooledString( szResponse );
			ADD_DEBUG_HISTORY( HISTORY_FRANZ_BLIND, UTIL_VarArgs( "(%0.2f) Franz: start flare blind scene '%s'\n", gpGlobals->curtime, 
				STRING(m_iszCurrentBlindScene) ) );
			GetShotRegulator()->DisableShooting();
			m_bIsFlashlightBlind = true;
			m_fStayBlindUntil = gpGlobals->curtime + 0.1f;
		}
	}
}

//-----------------------------------------------------------------------------
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Franz::BlindedByFlare( void )
{
	Vector vecEyes = EyePosition();

	Vector los;
	Vector vecToEyes;
	Vector facingDir = EyeDirection2D();

	// use a wider radius when he's already blind to help with edge cases
	// where he flickers back and forth due to animation
	float fBlindDist = ( m_bIsFlashlightBlind ) ? 35.0f : 30.0f;

	CFlare *pFlare = CFlare::GetActiveFlares();
	while( pFlare != NULL )
	{
		vecToEyes = (vecEyes - pFlare->GetAbsOrigin());
		float fDist = VectorNormalize( vecToEyes ); 
		if ( fDist < fBlindDist )
		{
			// Check facing to ensure we're in front of him
			los = ( pFlare->GetAbsOrigin() - vecEyes );
			los.z = 0;
			VectorNormalize( los );
			float flDot = DotProduct( los, facingDir );
			if ( ( flDot > 0.3 ) && FVisible( pFlare ) )
			{
				return true;
			}
		}

		pFlare = pFlare->GetNextFlare();
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Called by enemy NPC's when they are ignited
// Input  : pVictim - entity that was ignited
//-----------------------------------------------------------------------------
void CNPC_Franz::EnemyIgnited( CAI_BaseNPC *pVictim )
{
	if ( FVisible( pVictim ) )
	{
		SpeakIfAllowed( TLK_ENEMY_BURNING );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Custom AI for Franz while in combat
//-----------------------------------------------------------------------------
void CNPC_Franz::DoCustomCombatAI( void )
{
	// Only run the following code if we're not in a vehicle
	if ( RunningPassengerBehavior() == false )
	{
		// Do mobbed by enemies logic
		DoMobbedCombatAI();
	}

	CBaseEntity *pEnemy = GetEnemy();

	if( HasCondition( COND_LOW_PRIMARY_AMMO ) )
	{
		if( pEnemy )
		{
			if( GetAbsOrigin().DistToSqr( pEnemy->GetAbsOrigin() ) < Square( 60.0f ) )
			{
				// Don't reload if an enemy is right in my face.
				ClearCondition( COND_LOW_PRIMARY_AMMO );
			}
		}
	}
}

bool CNPC_Franz::RunningPassengerBehavior( void )
{
	// Must be active and not outside the vehicle
	if ( m_PassengerBehavior.IsRunning() && m_PassengerBehavior.GetPassengerState() != PASSENGER_STATE_OUTSIDE )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Handle "mobbed" combat condition when Franz is overwhelmed by force
//-----------------------------------------------------------------------------
void CNPC_Franz::DoMobbedCombatAI( void )
{
	AIEnemiesIter_t iter;

	float visibleEnemiesScore = 0.0f;
	float closeEnemiesScore = 0.0f;

	for ( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
	{
		if ( IRelationType( pEMemory->hEnemy ) != D_NU && IRelationType( pEMemory->hEnemy ) != D_LI && pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= FRANZ_MIN_CONSIDER_DIST )
		{
			if( pEMemory->hEnemy && pEMemory->hEnemy->IsAlive() && gpGlobals->curtime - pEMemory->timeLastSeen <= 0.5f && pEMemory->hEnemy->Classify() != CLASS_BULLSEYE )
			{
				if( pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= FRANZ_MIN_MOB_DIST_SQR )
				{
					closeEnemiesScore += 1.0f;
				}
				else
				{
					visibleEnemiesScore += 1.0f;
				}
			}
		}
	}

	if( closeEnemiesScore > 2 )
	{
		SetCondition( COND_MOBBED_BY_ENEMIES );

		// mark anyone in the mob as having mobbed me
		for ( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
		{
			if ( pEMemory->bMobbedMe )
				continue;

			if ( IRelationType( pEMemory->hEnemy ) != D_NU && IRelationType( pEMemory->hEnemy ) != D_LI && pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= FRANZ_MIN_CONSIDER_DIST )
			{
				if( pEMemory->hEnemy && pEMemory->hEnemy->IsAlive() && gpGlobals->curtime - pEMemory->timeLastSeen <= 0.5f && pEMemory->hEnemy->Classify() != CLASS_BULLSEYE )
				{
					if( pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= FRANZ_MIN_MOB_DIST_SQR )
					{
						pEMemory->bMobbedMe = true;
					}
				}
			}
		}
	}
	else
	{
		ClearCondition( COND_MOBBED_BY_ENEMIES );
	}

	// Franz conviently doesnt need to reload when mobbed in cqc.
	if( HasCondition( COND_MOBBED_BY_ENEMIES ) )
	{
		ClearCondition( COND_LOW_PRIMARY_AMMO );
	}

	// Scream some combat speech
	if( HasCondition( COND_MOBBED_BY_ENEMIES ) )
	{
		SpeakIfAllowed( TLK_MOBBED );		
	}
	else if( visibleEnemiesScore > 4 )
	{
		SpeakIfAllowed( TLK_MANY_ENEMIES );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_Franz::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{

	// Main Weapon
	if( FClassnameIs( pWeapon, "weapon_ar2" ) )
	{
		return WEAPON_PROFICIENCY_VERY_GOOD;
	}
	
	// Add a check here to see if its a friendly citizen. If it is, then put it to avg.
	if( FClassnameIs( pWeapon, "weapon_pistol" ) )
	{
		return WEAPON_PROFICIENCY_GOOD;
	}
	else if( FClassnameIs( pWeapon, "weapon_357" ) )
	{
		return WEAPON_PROFICIENCY_GOOD;
	}

	if( FClassnameIs( pWeapon, "weapon_smg1" ) )
	{
		return WEAPON_PROFICIENCY_GOOD;
	}

	// Shotgun is calculated differently
	//	if( FClassnameIs( pWeapon, "weapon_shotgun" ) )
	//	{
	//		return WEAPON_PROFICIENCY_GOOD;
	//	}

	return BaseClass::CalcWeaponProficiency( pWeapon );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------



//=============================================================================
// AI Schedules Guts
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: Use custom generic attacking speech
//-----------------------------------------------------------------------------
bool CNPC_Franz::OnBeginMoveAndShoot()
{
	if ( BaseClass::OnBeginMoveAndShoot() )
	{
		// Might want to change this to a new concept, like TLK_ATTACKMOVE?
		SpeakAttacking();
		return true;
	}

	return false;
}

void CNPC_Franz::SpeakAttacking( void )
{
	if ( GetActiveWeapon() && m_AnnounceAttackTimer.Expired() )
	{
		// Also gets the weapon used
		SpeakIfAllowed( TLK_ATTACKING, UTIL_VarArgs("attacking_with_weapon:%s", GetActiveWeapon()->GetClassname()) );
		m_AnnounceAttackTimer.Set( 4, 6 );
	}
}

//------------------------------------------------------------------------------
// Purpose: Declare all the stuff at the end of init
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_franz, CNPC_Franz )
	
	// Looks kind of like this for more advanced NPCs
	//DECLARE_TASK( TASK_ALYX_FALL_TO_GROUND )
	//
	//DECLARE_ANIMEVENT( COMBINE_AE_ALTFIRE )
	//
	//DECLARE_CONDITION( COND_ALYX_IN_DARK )
	//
	//--------------------------------------------------------------------------
	//	DEFINE_SCHEDULE
	//	(
	//		SCHED_ALYX_PREPARE_TO_INTERACT_WITH_TARGET,
	//
	//		"	Tasks"
	//		"		TASK_STOP_MOVING						0"
	//		"		TASK_PLAY_SEQUENCE						ACTIVITY:ACT_ALYX_DRAW_TOOL"
	//		"		TASK_SET_ACTIVITY						ACTIVITY:ACT_ALYX_IDLE_TOOL"
	//		"		TASK_FACE_PLAYER						0"
	//		""
	//		"	Interrupts"
	//		"	COND_LIGHT_DAMAGE"
	//		"	COND_HEAVY_DAMAGE"
	//	)
	//
	//--------------------------------------------------------------------------
	
AI_END_CUSTOM_NPC()

//=============================================================================