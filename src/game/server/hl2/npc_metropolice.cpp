//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: I AM THE LAW!
// Not as cowardly as the civilians, but also not as aggressive as the soldiers
// TODO's: Stun-Grenades, retreating on squad-leader death, Optimize/Cleanup.
//
//=============================================================================//

#include "cbase.h"
#include "soundent.h"
#include "npcevent.h"
#include "globalstate.h"
#include "ai_squad.h"
#include "ai_tacticalservices.h"
#include "npc_manhack.h"
#include "npc_metropolice.h"
#include "weapon_stunstick.h"
#include "basegrenade_shared.h"
#include "ai_route.h"
#include "hl2_player.h"
#include "iservervehicle.h"
#include "items.h"
#include "hl2_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//#define SF_METROPOLICE_					0x00010000
#define SF_METROPOLICE_SIMPLE_VERSION		0x00020000
#define SF_METROPOLICE_USE_STUN_GRENADES	0x00080000
#define SF_METROPOLICE_NOCHATTER			0x00100000
#define SF_METROPOLICE_ARREST_ENEMY			0x00200000
#define SF_METROPOLICE_NO_FAR_STITCH		0x00400000
#define SF_METROPOLICE_NO_MANHACK_DEPLOY	0x00800000
#define SF_METROPOLICE_ALLOWED_TO_RESPOND	0x01000000
#define SF_METROPOLICE_MID_RANGE_ATTACK		0x02000000

#define METROPOLICE_MID_RANGE_ATTACK_RANGE	3500.0f
#define METROPOLICE_LIMP_HEALTH		20
#define	METROPOLICE_MAX_WARNINGS	3

#define RECENT_DAMAGE_INTERVAL		3.0f
#define RECENT_DAMAGE_THRESHOLD		0.2f

#define	METROPOLICE_BODYGROUP_MANHACK	1

enum
{
	// NOTE: Exact #s are important, since they are referred to by number in schedules below

	METROPOLICE_SENTENCE_FREEZE			= 0,
	METROPOLICE_SENTENCE_HES_OVER_HERE	= 1,
	METROPOLICE_SENTENCE_HES_RUNNING	= 2,
	METROPOLICE_SENTENCE_TAKE_HIM_DOWN	= 3,	
	METROPOLICE_SENTENCE_ARREST_IN_POSITION	= 4,
	METROPOLICE_SENTENCE_DEPLOY_MANHACK	= 5,
	METROPOLICE_SENTENCE_MOVE_INTO_POSITION	= 6,
	METROPOLICE_SENTENCE_HEARD_SOMETHING	= 7,
};

enum
{
	METROPOLICE_ANNOUNCE_ATTACK_PRIMARY = 1,
	METROPOLICE_ANNOUNCE_ATTACK_SECONDARY,
	METROPOLICE_ANNOUNCE_ATTACK_HARASS,
};

enum
{
	METROPOLICE_CHATTER_WAIT_FOR_RESPONSE = 0,
	METROPOLICE_CHATTER_ASK_QUESTION = 1,
	METROPOLICE_CHATTER_RESPONSE = 2,

	METROPOLICE_CHATTER_RESPONSE_TYPE_COUNT = 2,
};


enum SpeechMemory_t
{
	bits_MEMORY_PAIN_LIGHT_SOUND	= bits_MEMORY_CUSTOM1,
	bits_MEMORY_PAIN_HEAVY_SOUND	= bits_MEMORY_CUSTOM2,
	bits_MEMORY_PLAYER_HURT			= bits_MEMORY_CUSTOM3,
	bits_MEMORY_PLAYER_HARASSED		= bits_MEMORY_CUSTOM4,
};

//Metrocop
int g_interactionMetrocopIdleChatter = 0;
int g_interactionMetrocopClearSentenceQueues = 0;

extern int g_interactionHitByPlayerThrownPhysObj;

ConVar	sk_metropolice_health( "sk_metropolice_health","0");
ConVar	sk_metropolice_simple_health( "sk_metropolice_simple_health","0");

ConVar	metropolice_chase_use_follow( "metropolice_chase_use_follow", "0" );
ConVar  metropolice_move_and_melee("metropolice_move_and_melee", "1" );
ConVar  metropolice_charge("metropolice_charge", "0" );

// How many clips of pistol ammo a metropolice carries.
#define METROPOLICE_NUM_CLIPS			8	// 8+1 in gun
#define METROPOLICE_BURST_RELOAD_COUNT	12

int AE_METROPOLICE_BATON_ON;
int	AE_METROPOLICE_BATON_OFF;
int AE_METROPOLICE_SHOVE;
int AE_METROPOLICE_START_DEPLOY;
int AE_METROPOLICE_DRAW_PISTOL;		// was	50
int AE_METROPOLICE_DEPLOY_MANHACK;	// was	51

// -----------------------------------------------
//	> Squad slots
// -----------------------------------------------
enum SquadSlot_T
{
	SQUAD_SLOT_POLICE_CHARGE_ENEMY = LAST_SHARED_SQUADSLOT,
	SQUAD_SLOT_POLICE_HARASS, // Yell at the player with a megaphone, etc.
	SQUAD_SLOT_POLICE_DEPLOY_MANHACK,
	SQUAD_SLOT_POLICE_ATTACK_OCCLUDER1,
	SQUAD_SLOT_POLICE_ATTACK_OCCLUDER2,
	SQUAD_SLOT_POLICE_ARREST_ENEMY,
};

//=========================================================
// Metro Police  Activities
//=========================================================
int ACT_METROPOLICE_DRAW_PISTOL;
int ACT_METROPOLICE_DEPLOY_MANHACK;
int ACT_METROPOLICE_FLINCH_BEHIND;
int ACT_METROPOLICE_POINT;

int	ACT_WALK_BATON;
int	ACT_IDLE_ANGRY_BATON;
int	ACT_PUSH_PLAYER;
int ACT_MELEE_ATTACK_THRUST;
int ACT_ACTIVATE_BATON;
int ACT_DEACTIVATE_BATON;
 
LINK_ENTITY_TO_CLASS( npc_metropolice, CNPC_MetroPolice );
LINK_ENTITY_TO_CLASS( npc_police, CNPC_MetroPolice );
LINK_ENTITY_TO_CLASS( npc_ancorp_g, CNPC_MetroPolice );

BEGIN_DATADESC( CNPC_MetroPolice )

	DEFINE_EMBEDDED( m_BatonSwingTimer ),	
	DEFINE_EMBEDDED( m_NextChargeTimer ),
//	DEFINE_EMBEDDED( m_LeapfrogTimer ),
	DEFINE_FIELD( m_flBatonDebounceTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_bShouldActivateBaton, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iPistolClips, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_fWeaponDrawn, FIELD_BOOLEAN, "weapondrawn" ),
	DEFINE_FIELD( m_fCanPoint, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_LastShootSlot, FIELD_INTEGER ),
	DEFINE_EMBEDDED( m_TimeYieldShootSlot ),
	DEFINE_EMBEDDED( m_Sentences ),
	DEFINE_FIELD( m_bPlayerIsNear, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( m_flNextLedgeCheckTime, FIELD_TIME ),
	DEFINE_FIELD( m_flTaskCompletionTime, FIELD_TIME ),
	DEFINE_FIELD( m_flLastPhysicsFlinchTime, FIELD_TIME ),
	DEFINE_FIELD( m_flLastDamageFlinchTime, FIELD_TIME ),

	DEFINE_FIELD( m_hManhack, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hBlockingProp, FIELD_EHANDLE ),

	DEFINE_FIELD( m_nRecentDamage, FIELD_INTEGER ),
	DEFINE_FIELD( m_flRecentDamageTime, FIELD_TIME ),

	DEFINE_FIELD( m_flNextPainSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextLostSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_nIdleChatterType, FIELD_INTEGER ),

	DEFINE_FIELD( m_bSimpleCops, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flLastHitYaw, FIELD_FLOAT ),

	DEFINE_FIELD( m_bPlayerTooClose,	FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bKeepFacingPlayer,	FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flChasePlayerTime,	FIELD_TIME ),
	DEFINE_FIELD( m_vecPreChaseOrigin,	FIELD_VECTOR ),
	DEFINE_FIELD( m_flPreChaseYaw,		FIELD_FLOAT ),
	DEFINE_FIELD( m_nNumWarnings,		FIELD_INTEGER ),
	DEFINE_FIELD( m_iNumPlayerHits,		FIELD_INTEGER ),

	//								m_ActBusyBehavior (auto saved by AI)
	//								m_StandoffBehavior (auto saved by AI)
	//								m_AssaultBehavior (auto saved by AI)
	//								m_FuncTankBehavior (auto saved by AI)
	//								m_RappelBehavior (auto saved by AI)
	//								m_PolicingBehavior (auto saved by AI)
	//								m_FollowBehavior (auto saved by AI)

	DEFINE_KEYFIELD( m_iManhacks, FIELD_INTEGER, "manhacks" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnableManhackToss", InputEnableManhackToss ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetPoliceGoal", InputSetPoliceGoal ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ActivateBaton", InputActivateBaton ),
	
	DEFINE_USEFUNC( PrecriminalUse ),

	DEFINE_OUTPUT( m_OnStunnedPlayer,	"OnStunnedPlayer" ),
	DEFINE_OUTPUT( m_OnCupCopped, "OnCupCopped" ),

END_DATADESC()

//------------------------------------------------------------------------------

float CNPC_MetroPolice::gm_flTimeLastSpokePeek;

//------------------------------------------------------------------------------
// Purpose 
//------------------------------------------------------------------------------
CBaseEntity *CNPC_MetroPolice::CheckTraceHullAttack( float flDist, const Vector &mins, const Vector &maxs, int iDamage, int iDmgType, float forceScale, bool bDamageAnyNPC )
{
	// If only a length is given assume we want to trace in our facing direction
	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	Vector vStart = GetAbsOrigin();

	// The ideal place to start the trace is in the center of the attacker's bounding box.
	// however, we need to make sure there's enough clearance. Some of the smaller monsters aren't 
	// as big as the hull we try to trace with. (SJB)
	float flVerticalOffset = WorldAlignSize().z * 0.5;

	if( flVerticalOffset < maxs.z )
	{
		// There isn't enough room to trace this hull, it's going to drag the ground.
		// so make the vertical offset just enough to clear the ground.
		flVerticalOffset = maxs.z + 1.0;
	}

	vStart.z += flVerticalOffset;
	Vector vEnd = vStart + (forward * flDist );
	return CheckTraceHullAttack( vStart, vEnd, mins, maxs, iDamage, iDmgType, forceScale, bDamageAnyNPC );
}

//------------------------------------------------------------------------------
// Melee filter for police
//------------------------------------------------------------------------------
class CTraceFilterMetroPolice : public CTraceFilterEntitiesOnly
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE( CTraceFilterMetroPolice );
	
	CTraceFilterMetroPolice( const IHandleEntity *passentity, int collisionGroup, CTakeDamageInfo *dmgInfo, float flForceScale, bool bDamageAnyNPC )
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup), m_dmgInfo(dmgInfo), m_pHit(NULL), m_flForceScale(flForceScale), m_bDamageAnyNPC(bDamageAnyNPC)
	{
	}
	
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
			return false;

		if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
			return false;

		// Don't test if the game code tells us we should ignore this collision...
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		
		if ( pEntity )
		{
			if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
				return false;
			
			if ( !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
				return false;

			if ( pEntity->m_takedamage == DAMAGE_NO )
				return false;

			// Translate the vehicle into its driver for damage
			if ( pEntity->GetServerVehicle() != NULL )
			{
				CBaseEntity *pDriver = pEntity->GetServerVehicle()->GetPassenger();

				if ( pDriver != NULL )
				{
					pEntity = pDriver;
				}
			}
	
			Vector	attackDir = pEntity->WorldSpaceCenter() - m_dmgInfo->GetAttacker()->WorldSpaceCenter();
			VectorNormalize( attackDir );

			CTakeDamageInfo info = (*m_dmgInfo);				
			CalculateMeleeDamageForce( &info, attackDir, info.GetAttacker()->WorldSpaceCenter(), m_flForceScale );

			if( !(pEntity->GetFlags() & FL_ONGROUND) )
			{
				// Don't hit airborne entities so hard. They fly farther since
				// there's no friction with the ground.
				info.ScaleDamageForce( 0.001 );
			}

			CBaseCombatCharacter *pBCC = info.GetAttacker()->MyCombatCharacterPointer();
			CBaseCombatCharacter *pVictimBCC = pEntity->MyCombatCharacterPointer();

			// Only do these comparisons between NPCs
			if ( pBCC && pVictimBCC )
			{
				// Can only damage other NPCs that we hate
				if ( m_bDamageAnyNPC || pBCC->IRelationType( pEntity ) == D_HT || pEntity->IsPlayer() )
				{
					if ( info.GetDamage() )
					{
						// If gordon's a criminal, do damage now
						if ( !pEntity->IsPlayer() || GlobalEntity_GetState( "gordon_precriminal" ) == GLOBAL_OFF )
						{
							if ( pEntity->IsPlayer() && ((CBasePlayer *)pEntity)->IsSuitEquipped() )
							{
								info.ScaleDamage( .25 );
								info.ScaleDamageForce( .25 );
							}

							pEntity->TakeDamage( info );
						}
					}
					
					m_pHit = pEntity;
					return true;
				}
			}
			else
			{
				// Make sure if the player is holding this, he drops it
				Pickup_ForcePlayerToDropThisObject( pEntity );

				// Otherwise just damage passive objects in our way
				if ( info.GetDamage() )
				{
					pEntity->TakeDamage( info );
				}
			}
		}

		return false;
	}

public:
	const IHandleEntity *m_pPassEnt;
	int					m_collisionGroup;
	CTakeDamageInfo		*m_dmgInfo;
	CBaseEntity			*m_pHit;
	float				m_flForceScale;
	bool				m_bDamageAnyNPC;
};

//------------------------------------------------------------------------------
// Purpose :	start and end trace position, amount 
//				of damage to do, and damage type. Returns a pointer to
//				the damaged entity in case the NPC wishes to do
//				other stuff to the victim (punchangle, etc)
//
//				Used for many contact-range melee attacks. Bites, claws, etc.
// Input   :
// Output  :
//------------------------------------------------------------------------------
CBaseEntity *CNPC_MetroPolice::CheckTraceHullAttack( const Vector &vStart, const Vector &vEnd, const Vector &mins, const Vector &maxs, int iDamage, int iDmgType, float flForceScale, bool bDamageAnyNPC )
{

	CTakeDamageInfo	dmgInfo( this, this, iDamage, DMG_SLASH );
	
	CTraceFilterMetroPolice traceFilter( this, COLLISION_GROUP_NONE, &dmgInfo, flForceScale, bDamageAnyNPC );

	Ray_t ray;
	ray.Init( vStart, vEnd, mins, maxs );

	trace_t tr;
	enginetrace->TraceRay( ray, MASK_SHOT, &traceFilter, &tr );

	CBaseEntity *pEntity = traceFilter.m_pHit;
	
	if ( pEntity == NULL )
	{
		// See if perhaps I'm trying to claw/bash someone who is standing on my head.
		Vector vecTopCenter;
		Vector vecEnd;
		Vector vecMins, vecMaxs;

		// Do a tracehull from the top center of my bounding box.
		vecTopCenter = GetAbsOrigin();
		CollisionProp()->WorldSpaceAABB( &vecMins, &vecMaxs );
		vecTopCenter.z = vecMaxs.z + 1.0f;
		vecEnd = vecTopCenter;
		vecEnd.z += 2.0f;
		
		ray.Init( vecTopCenter, vEnd, mins, maxs );
		enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );

		pEntity = traceFilter.m_pHit;
	}

	return pEntity;
}

//-----------------------------------------------------------------------------
// My buddies got killed!
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::NotifyDeadFriend( CBaseEntity* pFriend )
{
	BaseClass::NotifyDeadFriend(pFriend);

	if ( pFriend == m_hManhack )
	{
		m_Sentences.Speak( "METROPOLICE_MANHACK_KILLED", SENTENCE_PRIORITY_NORMAL, SENTENCE_CRITERIA_NORMAL );
		DevMsg("My manhack died!\n");
		m_hManhack = NULL;
		return;
	}

	// No notifications for squadmates' dead manhacks
	if ( FClassnameIs( pFriend, "npc_manhack" ) )
		return;

	// Reset idle chatter, we may never get a response back
	if ( m_nIdleChatterType == METROPOLICE_CHATTER_WAIT_FOR_RESPONSE )
	{
		m_nIdleChatterType = METROPOLICE_CHATTER_ASK_QUESTION;
	}

	if ( GetSquad()->NumMembers() < 2 )
	{
		m_Sentences.Speak( "METROPOLICE_LAST_OF_SQUAD", SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL );
		return;
	}

	m_Sentences.Speak( "METROPOLICE_MAN_DOWN", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_NORMAL );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CNPC_MetroPolice::CNPC_MetroPolice()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::OnScheduleChange()
{
	BaseClass::OnScheduleChange();

	if ( GetEnemy() && HasCondition( COND_ENEMY_DEAD ) )
	{
		AnnounceEnemyKill( GetEnemy() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();

	// Speak any queued sentences
	m_Sentences.UpdateSentenceQueue();

	extern ConVar ai_debug_shoot_positions;
	if ( ai_debug_shoot_positions.GetBool() )
		NDebugOverlay::Cross3D( EyePosition(), 16, 0, 255, 0, false, 0.1 );

	// Look at near players, always
	m_bPlayerIsNear = false;
	if ( PlayerIsCriminal() == false )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
		
		if ( pPlayer && ( pPlayer->WorldSpaceCenter() - WorldSpaceCenter() ).LengthSqr() < (128*128) )
		{
			m_bPlayerIsNear = true;
			AddLookTarget( pPlayer, 0.75f, 5.0f );
			
			if ( ( m_PolicingBehavior.IsEnabled() == false ) && ( m_nNumWarnings >= METROPOLICE_MAX_WARNINGS ) )
			{
				m_flBatonDebounceTime = gpGlobals->curtime + random->RandomFloat( 2.5f, 4.0f );
				SetTarget( pPlayer );
				SetBatonState( true );
			}
		}
		else 
		{
			if ( m_PolicingBehavior.IsEnabled() == false && gpGlobals->curtime > m_flBatonDebounceTime )
			{
				SetBatonState( false );
			}

			m_bKeepFacingPlayer = false;
		}
	}

	if (gpGlobals->curtime > m_flRecentDamageTime + RECENT_DAMAGE_INTERVAL)
	{
		m_nRecentDamage = 0;
		m_flRecentDamageTime = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &move - 
//			flInterval - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
	// Don't do this if we're scripted
	if ( IsInAScript() )
		return BaseClass::OverrideMoveFacing( move, flInterval );
  
	// ROBIN: Disabled at request of mapmakers for now
#if 1
  	// If we're moving during a police sequence, always face our target
	if ( m_PolicingBehavior.IsEnabled() )
  	{
		CBaseEntity *pTarget = m_PolicingBehavior.GetGoalTarget();

		if ( pTarget )
		{
			AddFacingTarget( pTarget, pTarget->WorldSpaceCenter(), 1.0f, 0.2f );
		}
	}
#endif

	return BaseClass::OverrideMoveFacing( move, flInterval );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::Precache( void )
{
	if ( HasSpawnFlags( SF_NPC_START_EFFICIENT ) )
	{
		SetModelName( AllocPooledString("models/police_cheaple.mdl" ) );
	}
	else
	{
		SetModelName( AllocPooledString("models/police.mdl") );
	}

	PrecacheModel( STRING( GetModelName() ) );

	UTIL_PrecacheOther( "npc_manhack" );

	PrecacheScriptSound( "NPC_MetroPolice.OnFireScream" );
	PrecacheScriptSound( "NPC_Metropolice.Shove" );
	PrecacheScriptSound( "NPC_MetroPolice.WaterSpeech" );
	PrecacheScriptSound( "NPC_MetroPolice.HidingSpeech" );
	enginesound->PrecacheSentenceGroup( "METROPOLICE" );

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Create components
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::CreateComponents()
{
	if ( !BaseClass::CreateComponents() )
		return false;

	m_Sentences.Init( this, "NPC_Metropolice.SentenceParameters" );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::Spawn( void )
{
	Precache();

#ifdef _XBOX
	// Always fade the corpse
	AddSpawnFlags( SF_NPC_FADE_CORPSE );
#endif // _XBOX

	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();
	
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_RED );

	// Set squad-leader model, AI is taken care of real-time
//!	if ( m_pSquad && m_pSquad->IsLeader( this ) )
//!	{
//!	
//!	}
	SetModel( STRING( GetModelName() ) );

	m_nIdleChatterType = METROPOLICE_CHATTER_ASK_QUESTION; 
	m_bSimpleCops = HasSpawnFlags( SF_METROPOLICE_SIMPLE_VERSION );
	if ( HasSpawnFlags( SF_METROPOLICE_NOCHATTER ) )
	{
		AddSpawnFlags( SF_NPC_GAG );
	}

	if ( m_bSimpleCops )
	{
		m_iHealth = sk_metropolice_simple_health.GetFloat();
	}
	else
	{
		m_iHealth = sk_metropolice_health.GetFloat();
	}

	m_flFieldOfView		= 0.5;	//*120
	m_NPCState			= NPC_STATE_NONE;

	m_TimeYieldShootSlot.Set( 2, 6 );

	GetEnemies()->SetFreeKnowledgeDuration( 6.0 );

	m_bShouldActivateBaton = false;
	m_flNextGrenadeCheck = gpGlobals->curtime + 1;
	m_flNextLedgeCheckTime = -1.0f;

	m_nNumWarnings = 0;
	m_fCanPoint = true;
	m_bPlayerTooClose = false;
	m_bKeepFacingPlayer = false;
	m_flChasePlayerTime = 0;
	m_vecPreChaseOrigin = vec3_origin;
	m_flPreChaseYaw = 0;

	if ( !HasSpawnFlags( SF_NPC_START_EFFICIENT ) )
	{
		CapabilitiesAdd( bits_CAP_TURN_HEAD | bits_CAP_ANIMATEDFACE );
		CapabilitiesAdd( bits_CAP_AIM_GUN | bits_CAP_MOVE_SHOOT );
	}
	CapabilitiesAdd( bits_CAP_MOVE_GROUND );
	CapabilitiesAdd( bits_CAP_USE_WEAPONS | bits_CAP_NO_HIT_SQUADMATES );
	CapabilitiesAdd( bits_CAP_SQUAD );
	CapabilitiesAdd( bits_CAP_DUCK | bits_CAP_DOORS_GROUP );
	CapabilitiesAdd( bits_CAP_USE_SHOT_REGULATOR );

	m_HackedGunPos = Vector ( 0, 0, 55 );

	m_iPistolClips = METROPOLICE_NUM_CLIPS;

	NPCInit();

	// NOTE: This must occur *after* init, since init sets default dist look
	if ( HasSpawnFlags( SF_METROPOLICE_MID_RANGE_ATTACK ) )
	{
		m_flDistTooFar = METROPOLICE_MID_RANGE_ATTACK_RANGE;
		SetDistLook( METROPOLICE_MID_RANGE_ATTACK_RANGE );
	}

	m_hManhack = NULL;

	if ( GetActiveWeapon() )
	{
		CBaseCombatWeapon *pWeapon;

		pWeapon = GetActiveWeapon();

		if( !FClassnameIs( pWeapon, "weapon_pistol" ) )
		{
			// Pistol starts holstered.
			m_fWeaponDrawn = true;
		}
		
		if( !m_fWeaponDrawn ) 
		{
			GetActiveWeapon()->AddEffects( EF_NODRAW );
		}
	}

	SetUse( &CNPC_MetroPolice::PrecriminalUse );

	// Start us with a visible manhack if we have one
	if ( m_iManhacks )
	{
		SetBodygroup( METROPOLICE_BODYGROUP_MANHACK, true );
	}
}


//-----------------------------------------------------------------------------
// Update weapon ranges
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	BaseClass::Weapon_Equip( pWeapon );

	if ( HasSpawnFlags(SF_METROPOLICE_MID_RANGE_ATTACK) && GetActiveWeapon() )
	{
		GetActiveWeapon()->m_fMaxRange1 = METROPOLICE_MID_RANGE_ATTACK_RANGE;
	//	GetActiveWeapon()->m_fMaxRange2 = METROPOLICE_MID_RANGE_ATTACK_RANGE;
	}
}


//-----------------------------------------------------------------------------
// FuncTankBehavior-related sentences
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::SpeakFuncTankSentence( int nSentenceType )
{
	switch ( nSentenceType )
	{
	case FUNCTANK_SENTENCE_MOVE_TO_MOUNT:
		m_Sentences.Speak( "METROPOLICE_FT_APPROACH", SENTENCE_PRIORITY_MEDIUM );
		break;

	case FUNCTANK_SENTENCE_JUST_MOUNTED:
		m_Sentences.Speak( "METROPOLICE_FT_MOUNT", SENTENCE_PRIORITY_HIGH );
		break;

	case FUNCTANK_SENTENCE_SCAN_FOR_ENEMIES:
		m_Sentences.Speak( "METROPOLICE_FT_SCAN", SENTENCE_PRIORITY_NORMAL );
		break;

	case FUNCTANK_SENTENCE_DISMOUNTING:
		m_Sentences.Speak( "METROPOLICE_FT_DISMOUNT", SENTENCE_PRIORITY_HIGH );
		break;
	}
}


//-----------------------------------------------------------------------------
// Standoff Behavior-related sentences
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::SpeakStandoffSentence( int nSentenceType )
{
	switch ( nSentenceType )
	{
	case STANDOFF_SENTENCE_BEGIN_STANDOFF:
		m_Sentences.Speak( "METROPOLICE_SO_BEGIN", SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_SQUAD_LEADER );
		break;

	case STANDOFF_SENTENCE_END_STANDOFF:
		m_Sentences.Speak( "METROPOLICE_SO_END", SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_SQUAD_LEADER );
		break;

	case STANDOFF_SENTENCE_OUT_OF_AMMO:
		AnnounceOutOfAmmo( );
		break;

	case STANDOFF_SENTENCE_FORCED_TAKE_COVER:
		m_Sentences.Speak( "METROPOLICE_SO_FORCE_COVER" );
		break;

	case STANDOFF_SENTENCE_STAND_CHECK_TARGET:
		if ( gm_flTimeLastSpokePeek != 0 && gpGlobals->curtime - gm_flTimeLastSpokePeek > 20 )
		{
			m_Sentences.Speak( "METROPOLICE_SO_PEEK" );
			gm_flTimeLastSpokePeek = gpGlobals->curtime;
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Assault Behavior-related sentences
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::SpeakAssaultSentence( int nSentenceType )
{
	switch ( nSentenceType )
	{
	case ASSAULT_SENTENCE_HIT_RALLY_POINT:
		m_Sentences.SpeakQueued( "METROPOLICE_AS_HIT_RALLY", SENTENCE_PRIORITY_NORMAL );
		break;

	case ASSAULT_SENTENCE_HIT_ASSAULT_POINT:
		m_Sentences.SpeakQueued( "METROPOLICE_AS_HIT_ASSAULT", SENTENCE_PRIORITY_NORMAL );
		break;

	case ASSAULT_SENTENCE_SQUAD_ADVANCE_TO_RALLY:
		if ( m_Sentences.Speak( "METROPOLICE_AS_ADV_RALLY", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_SQUAD_LEADER ) >= 0 )
		{
			GetSquad()->BroadcastInteraction( g_interactionMetrocopClearSentenceQueues, NULL );
		}
		break;

	case ASSAULT_SENTENCE_SQUAD_ADVANCE_TO_ASSAULT:
		if ( m_Sentences.Speak( "METROPOLICE_AS_ADV_ASSAULT", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_SQUAD_LEADER ) >= 0 )
		{
			GetSquad()->BroadcastInteraction( g_interactionMetrocopClearSentenceQueues, NULL );
		}
		break;

	case ASSAULT_SENTENCE_COVER_NO_AMMO:
		AnnounceOutOfAmmo( );
		break;

	case ASSAULT_SENTENCE_UNDER_ATTACK:
		m_Sentences.Speak( "METROPOLICE_GO_ALERT" );
		break;
	}
}


//-----------------------------------------------------------------------------
// Speaking while using TASK_SPEAK_SENTENCE
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::SpeakSentence( int nSentenceType )
{
	if ( !PlayerIsCriminal() )
		return;

	if ( nSentenceType >= SENTENCE_BASE_BEHAVIOR_INDEX )
	{
		if ( GetRunningBehavior() == &m_FuncTankBehavior )
		{
			SpeakFuncTankSentence( nSentenceType );
			return;
		}

		if ( GetRunningBehavior() == &m_StandoffBehavior )
		{
			SpeakStandoffSentence( nSentenceType );
			return;
		}

		if ( GetRunningBehavior() == &m_AssaultBehavior )
		{
			SpeakAssaultSentence( nSentenceType );
			return;
		}
	}

	switch ( nSentenceType )
	{
	case METROPOLICE_SENTENCE_FREEZE:
		m_Sentences.Speak( "METROPOLICE_FREEZE", SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL );
		break;

	case METROPOLICE_SENTENCE_HES_OVER_HERE:
		m_Sentences.Speak( "METROPOLICE_OVER_HERE", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_NORMAL );
		break;

	case METROPOLICE_SENTENCE_HES_RUNNING:
		m_Sentences.Speak( "METROPOLICE_HES_RUNNING", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_NORMAL );
		break;

	case METROPOLICE_SENTENCE_TAKE_HIM_DOWN:
		m_Sentences.Speak( "METROPOLICE_TAKE_HIM_DOWN", SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL );
		break;

	case METROPOLICE_SENTENCE_ARREST_IN_POSITION:
		m_Sentences.Speak( "METROPOLICE_ARREST_IN_POS", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_NORMAL );
		break;

	case METROPOLICE_SENTENCE_DEPLOY_MANHACK:
		m_Sentences.Speak( "METROPOLICE_DEPLOY_MANHACK" );
		break;

	case METROPOLICE_SENTENCE_MOVE_INTO_POSITION:
		{
			CBaseEntity *pEntity = GetEnemy();
			
			// NOTE: This is a good time to check to see if the player is hurt.
			// Have the cops notice this and call out
			if ( pEntity && !HasSpawnFlags( SF_METROPOLICE_ARREST_ENEMY ) )
			{
				if ( pEntity->IsPlayer() && (pEntity->GetHealth() <= 20) )
				{
					if ( !HasMemory(bits_MEMORY_PLAYER_HURT) ) 
					{
						if ( m_Sentences.Speak( "METROPOLICE_PLAYERHIT", SENTENCE_PRIORITY_HIGH ) >= 0 )
						{
							m_pSquad->SquadRemember(bits_MEMORY_PLAYER_HURT);
						}
					}
				}

				if ( GetNavigator()->GetPath()->GetPathLength() > 20 * 12.0f )
				{
					m_Sentences.Speak( "METROPOLICE_FLANK" );
				}
			}
		}
		break;

	case METROPOLICE_SENTENCE_HEARD_SOMETHING:
		if ( ( GetState() == NPC_STATE_ALERT ) || ( GetState() == NPC_STATE_IDLE ) )
		{
			m_Sentences.Speak( "METROPOLICE_HEARD_SOMETHING", SENTENCE_PRIORITY_MEDIUM );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Speaking
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::AnnounceEnemyType( CBaseEntity *pEnemy )
{
	if ( !pEnemy || !m_pSquad )
		return;

	// Don't announce enemies when I'm in arrest behavior
	if ( HasSpawnFlags( SF_METROPOLICE_ARREST_ENEMY ) )
		return;

	if ( m_pSquad->IsLeader( this ) || ( m_pSquad->GetLeader() && m_pSquad->GetLeader()->GetEnemy() != GetEnemy() ) )
	{
		// First contact, and I'm the squad leader.
		const char *pSentenceName = "METROPOLICE_MONST";
		switch ( pEnemy->Classify() )
		{
		case CLASS_PLAYER:
			{
				CBasePlayer *pPlayer = assert_cast<CBasePlayer*>( pEnemy );
				if ( pPlayer && pPlayer->IsInAVehicle() )
				{
					pSentenceName = "METROPOLICE_MONST_PLAYER_VEHICLE";
				}
				else
				{
					pSentenceName = "METROPOLICE_MONST_PLAYER";
				}
			}
			break;

		case CLASS_PLAYER_ALLY:
		case CLASS_CITIZEN_REBEL:
		case CLASS_CITIZEN_PASSIVE:
		case CLASS_CONSCRIPT:
		case CLASS_VORTIGAUNT:
			pSentenceName = "METROPOLICE_MONST_CITIZENS";
			break;

		case CLASS_PLAYER_ALLY_VITAL:
			pSentenceName = "METROPOLICE_MONST_CHARACTER";
			break;

		case CLASS_ANTLION:
//		case CLASS_HOUNDEYE:
			pSentenceName = "METROPOLICE_MONST_BUGS";
			break;

		case CLASS_ZOMBIE:
			pSentenceName = "METROPOLICE_MONST_ZOMBIES";
			break;

		case CLASS_HEADCRAB:
		case CLASS_BARNACLE:
			pSentenceName = "METROPOLICE_MONST_PARASITES";
			break;
		}

		m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_HIGH );
	}
	else
	{
		if ( m_pSquad->GetLeader() && FOkToMakeSound( SENTENCE_PRIORITY_MEDIUM ) )
		{
			// squelch anything that isn't high priority so the leader can speak
			JustMadeSound( SENTENCE_PRIORITY_MEDIUM );	
		}
	}
}


//-----------------------------------------------------------------------------
// Speaking
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::AnnounceEnemyKill( CBaseEntity *pEnemy )
{
	if ( !pEnemy )
		return;

	// 50% chance
	if (random->RandomInt(0,1))
		return;

	const char *pSentenceName = "METROPOLICE_KILL_MONST";
	switch ( pEnemy->Classify() )
	{
	case CLASS_PLAYER:
		pSentenceName = "METROPOLICE_KILL_PLAYER";
		break;

	// added sentences for these guys
	case CLASS_PLAYER_ALLY:
	case CLASS_CITIZEN_REBEL:
	case CLASS_CITIZEN_PASSIVE:
	case CLASS_CONSCRIPT:
	case CLASS_VORTIGAUNT:
		pSentenceName = "METROPOLICE_KILL_CITIZENS";
		break;

	case CLASS_PLAYER_ALLY_VITAL:
		pSentenceName = "METROPOLICE_KILL_CHARACTER";
		break;

	case CLASS_ANTLION:
		pSentenceName = "METROPOLICE_KILL_BUGS";
		break;

	case CLASS_ZOMBIE:
		pSentenceName = "METROPOLICE_KILL_ZOMBIES";
		break;

	case CLASS_HEADCRAB:
	case CLASS_BARNACLE:
		pSentenceName = "METROPOLICE_KILL_PARASITES";
		break;
	}

	m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_HIGH );
}


//-----------------------------------------------------------------------------
// Announce out of ammo
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::AnnounceOutOfAmmo( )
{
	// 50% chance
	if (random->RandomInt(0,1))
		return;

	if ( HasCondition( COND_NO_PRIMARY_AMMO ) )
	{
		m_Sentences.Speak( "METROPOLICE_COVER_NO_AMMO", SENTENCE_PRIORITY_HIGH );
	}
	else
	{
		m_Sentences.Speak( "METROPOLICE_COVER_LOW_AMMO", SENTENCE_PRIORITY_NORMAL );
	}
}

//-----------------------------------------------------------------------------
// We're taking cover from danger
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::AnnounceTakeCoverFromDanger( CSound *pSound )
{
	CBaseEntity *pSoundOwner = pSound->m_hOwner;
	if ( pSoundOwner )
	{
		CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade *>(pSoundOwner);
		if ( pGrenade )
		{
			if ( IRelationType( pGrenade->GetThrower() ) != D_LI )
			{
				// call out for enemy grenades
				m_Sentences.Speak( "METROPOLICE_DANGER_GREN", SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL );
			}
			return;
		}

		if ( pSoundOwner->GetServerVehicle() )
		{
			m_Sentences.Speak( "METROPOLICE_DANGER_VEHICLE", SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL );
			return;
		}

		if ( FClassnameIs( pSoundOwner, "npc_manhack" ) )
		{
			if ( pSoundOwner->HasPhysicsAttacker( 1.0f ) )
			{
				m_Sentences.Speak( "METROPOLICE_DANGER_MANHACK", SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL );
			}
			return;
		}
	}

	// I hear something dangerous, probably need to take cover.
	// dangerous sound nearby!, call it out
	const char *pSentenceName = "METROPOLICE_DANGER";
	m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL );
}

//-----------------------------------------------------------------------------
// Is my enemy currently in an airboat?
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::IsEnemyInAVehicle() const
{
	// Should this be a condition??
	if ( !GetEnemy() || !GetEnemy()->IsPlayer() )
		return false;

	CBaseEntity *pVehicle = static_cast<CBasePlayer*>( GetEnemy() )->GetVehicleEntity(); 
	if ( !pVehicle )
		return false;

	// NOTE: Could just return true if in a vehicle maybe
	return FClassnameIs( pVehicle, "prop_vehicle_airboat" );
}


//-----------------------------------------------------------------------------
// Returns the airboat
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_MetroPolice::GetEnemyVehicle() const
{
	// Should this be a condition??
	if ( !GetEnemy() || !GetEnemy()->IsPlayer() )
		return false;

	return static_cast<CBasePlayer*>( GetEnemy() )->GetVehicleEntity(); 
}


//-----------------------------------------------------------------------------
// Which entity are we actually trying to shoot at?
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_MetroPolice::GetShootTarget()
{
	// Should this be a condition??
	CBaseEntity *pEnemy = GetEnemy();
	if ( !pEnemy || !pEnemy->IsPlayer() )
		return pEnemy;

	CBaseEntity *pVehicle = static_cast<CBasePlayer*>( pEnemy )->GetVehicleEntity(); 
	return pVehicle ? pVehicle : pEnemy;
}

//-----------------------------------------------------------------------------
// How many shots will I fire in a particular amount of time?
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::CountShotsInTime( float flDeltaTime ) const
{
	return (int)(flDeltaTime / GetActiveWeapon()->GetFireRate() + 0.5f);
}

float CNPC_MetroPolice::GetTimeForShots( int nShotCount ) const
{
	return nShotCount * GetActiveWeapon()->GetFireRate();
}

//-----------------------------------------------------------------------------
// Visualize line of death
//-----------------------------------------------------------------------------
/*
void CNPC_MetroPolice::VisualizeLineOfDeath( )
{
	Vector vecAcross, vecStart;
	CrossProduct( m_vecBurstLineOfDeathDelta, Vector( 0, 0, 1 ), vecAcross );
	VectorNormalize( vecAcross );
	NDebugOverlay::Line( m_vecBurstLineOfDeathOrigin, m_vecBurstLineOfDeathOrigin + m_vecBurstLineOfDeathDelta, 255, 255, 0, false, 5.0f );
	VectorMA( m_vecBurstLineOfDeathOrigin, m_flBurstSteerDistance, vecAcross, vecStart );
	NDebugOverlay::Line( vecStart, vecStart + m_vecBurstLineOfDeathDelta, 255, 0, 0, false, 5.0f );
	VectorMA( m_vecBurstLineOfDeathOrigin, -m_flBurstSteerDistance, vecAcross, vecStart );
	NDebugOverlay::Line( vecStart, vecStart + m_vecBurstLineOfDeathDelta, 255, 0, 0, false, 5.0f );
}
*/

//-----------------------------------------------------------------------------
// Behaviors! Lovely behaviors
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::CreateBehaviors()
{
	AddBehavior( &m_RappelBehavior );
	AddBehavior( &m_FollowBehavior );
	AddBehavior( &m_PolicingBehavior );
	AddBehavior( &m_ActBusyBehavior );
	AddBehavior( &m_AssaultBehavior );
	AddBehavior( &m_StandoffBehavior );
	AddBehavior( &m_FuncTankBehavior );
	
	return BaseClass::CreateBehaviors();
}

void CNPC_MetroPolice::InputEnableManhackToss( inputdata_t &inputdata )
{
	if ( HasSpawnFlags( SF_METROPOLICE_NO_MANHACK_DEPLOY ) )
	{
		RemoveSpawnFlags( SF_METROPOLICE_NO_MANHACK_DEPLOY );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::InputSetPoliceGoal( inputdata_t &inputdata )
{
	CBaseEntity *pGoal = gEntList.FindEntityByName( NULL, inputdata.value.String() );

	if ( pGoal == NULL )
	{
		DevMsg( "SetPoliceGoal: %s (%s) unable to find ai_goal_police: %s\n", GetClassname(), GetDebugName(), inputdata.value.String() );
		return;
	}

	CAI_PoliceGoal *pPoliceGoal = dynamic_cast<CAI_PoliceGoal *>(pGoal);

	if ( pPoliceGoal == NULL )
	{
		DevMsg( "SetPoliceGoal: %s (%s)'s target %s is not an ai_goal_police entity!\n", GetClassname(), GetDebugName(), inputdata.value.String() );
		return;
	}

	m_PolicingBehavior.Enable( pPoliceGoal );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::InputActivateBaton( inputdata_t &inputdata )
{
	SetBatonState( inputdata.value.Bool() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::AlertSound( void )
{
	EmitSound( "NPC_MetroPolice.Alert" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::DeathSound( const CTakeDamageInfo &info )
{
	if ( IsOnFire() )
		return;

	m_Sentences.Speak( "METROPOLICE_DIE", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: implemented by subclasses to give them an opportunity to make
//			a sound when they lose their enemy
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::LostEnemySound( void)
{
	if ( gpGlobals->curtime <= m_flNextLostSoundTime )
		return;

	const char *pSentence;
	if (!(CBaseEntity*)GetEnemy() || gpGlobals->curtime - GetEnemyLastTimeSeen() > 10)
	{
		pSentence = "METROPOLICE_LOST_LONG"; 
	}
	else
	{
		pSentence = "METROPOLICE_LOST_SHORT";
	}

	if ( m_Sentences.Speak( pSentence ) >= 0 )
	{
		m_flNextLostSoundTime = gpGlobals->curtime + random->RandomFloat(5.0,15.0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: implemented by subclasses to give them an opportunity to make
//			a sound when they lose their enemy
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::FoundEnemySound( void)
{
	// Don't announce enemies when I'm in arrest behavior
	if ( HasSpawnFlags( SF_METROPOLICE_ARREST_ENEMY ) )
		return;

	m_Sentences.Speak( "METROPOLICE_REFIND_ENEMY", SENTENCE_PRIORITY_HIGH );
}

//-----------------------------------------------------------------------------
// Purpose: Indicates whether or not this npc should play an idle sound now.
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::ShouldPlayIdleSound( void )
{
	// If someone is waiting for a response, then respond!
	if ( ( m_NPCState == NPC_STATE_IDLE ) || ( m_NPCState == NPC_STATE_ALERT ) )
	{
		if ( m_nIdleChatterType >= METROPOLICE_CHATTER_RESPONSE )
			return FOkToMakeSound();
	}

	return BaseClass::ShouldPlayIdleSound();
}

//-----------------------------------------------------------------------------
// IdleSound 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::IdleSound( void )
{
	bool bIsCriminal = PlayerIsCriminal();

	// This happens when the NPC is waiting for his buddies to respond to him
	switch( m_nIdleChatterType )
	{
	case METROPOLICE_CHATTER_WAIT_FOR_RESPONSE:
		break;

	case METROPOLICE_CHATTER_ASK_QUESTION:
		{
			if ( m_bPlayerIsNear && !HasMemory(bits_MEMORY_PLAYER_HARASSED) )
			{
				if ( m_Sentences.Speak( "METROPOLICE_IDLE_HARASS_PLAYER", SENTENCE_PRIORITY_NORMAL, SENTENCE_CRITERIA_NORMAL ) >= 0 )
				{
					Remember( bits_MEMORY_PLAYER_HARASSED );
					if ( GetSquad() )
					{
						GetSquad()->SquadRemember(bits_MEMORY_PLAYER_HARASSED);
					}
				}
				return;
			}

			if ( !random->RandomInt(0,1) )
				break;

			int nQuestionType = random->RandomInt( 0, METROPOLICE_CHATTER_RESPONSE_TYPE_COUNT );
			if ( !IsInSquad() || ( nQuestionType == METROPOLICE_CHATTER_RESPONSE_TYPE_COUNT ) )
			{
				m_Sentences.Speak( bIsCriminal ? "METROPOLICE_IDLE_CR" : "METROPOLICE_IDLE" );
				break;
			}

			static const char *pQuestion[2][METROPOLICE_CHATTER_RESPONSE_TYPE_COUNT] = 
			{
				{ "METROPOLICE_IDLE_CHECK",		"METROPOLICE_IDLE_QUEST" },
				{ "METROPOLICE_IDLE_CHECK_CR",	"METROPOLICE_IDLE_QUEST_CR" },
			};

			if ( m_Sentences.Speak( pQuestion[bIsCriminal][nQuestionType] ) >= 0 )
			{
				GetSquad()->BroadcastInteraction( g_interactionMetrocopIdleChatter, (void*)(METROPOLICE_CHATTER_RESPONSE + nQuestionType), this );
				m_nIdleChatterType = METROPOLICE_CHATTER_WAIT_FOR_RESPONSE;
			}
		}
		break;

	default:
		{
			int nResponseType = m_nIdleChatterType - METROPOLICE_CHATTER_RESPONSE;

			static const char *pResponse[2][METROPOLICE_CHATTER_RESPONSE_TYPE_COUNT] = 
			{
				{ "METROPOLICE_IDLE_CLEAR",		"METROPOLICE_IDLE_ANSWER" },
				{ "METROPOLICE_IDLE_CLEAR_CR",	"METROPOLICE_IDLE_ANSWER_CR" },
			};

			if ( m_Sentences.Speak( pResponse[bIsCriminal][nResponseType] ) >= 0 )
			{
				GetSquad()->BroadcastInteraction( g_interactionMetrocopIdleChatter, (void*)(METROPOLICE_CHATTER_ASK_QUESTION), this );
				m_nIdleChatterType = METROPOLICE_CHATTER_ASK_QUESTION;
			}
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::PainSound( const CTakeDamageInfo &info )
{
	if ( gpGlobals->curtime < m_flNextPainSoundTime )
		return;

	// Don't make pain sounds if I'm on fire. The looping sound will take care of that for us.
	if ( IsOnFire() )
		return;

	// This causes it to speak it no matter what; doesn't bother with setting sounds.
	m_Sentences.Speak( "METROPOLICE_PAIN", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
	m_flNextPainSoundTime = gpGlobals->curtime + 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::GetSoundInterests( void )
{
	switch( m_NPCState )
	{
	case NPC_STATE_IDLE:
		// Metropolice Don't hear the sound of the player moving when in their Idle state.
		return SOUND_WORLD | SOUND_COMBAT | SOUND_BULLET_IMPACT | SOUND_DANGER | SOUND_PHYSICS_DANGER | SOUND_MOVE_AWAY;
		break;

	default:
		return SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_PLAYER_VEHICLE | SOUND_DANGER | 
		SOUND_PHYSICS_DANGER | SOUND_BULLET_IMPACT | SOUND_MOVE_AWAY;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_MetroPolice::MaxYawSpeed( void )
{
	if ( IsMoving() && HasPoseParameter( GetSequence(), m_poseMove_Yaw ) )
	{
		return( 20 );
	}
	else
	{
		switch( GetActivity() )
		{
		case ACT_IDLE:		
		case ACT_COVER_LOW:
			return 30;
			break;
		case ACT_TURN_LEFT:
		case ACT_TURN_RIGHT:
		case ACT_RANGE_ATTACK1:
		case ACT_RANGE_ATTACK2:
		case ACT_MELEE_ATTACK1:
		case ACT_MELEE_ATTACK2:
			return 90;
		default:
			return 45;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
Class_T	CNPC_MetroPolice::Classify ( void )
{
	return CLASS_METROPOLICE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::PlayerIsCriminal( void )
{
	if ( m_PolicingBehavior.IsEnabled() && m_PolicingBehavior.TargetIsHostile() )
		return true;

	if ( GlobalEntity_GetState( "gordon_precriminal" ) == GLOBAL_ON )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Overridden because if the player is a criminal, we hate them.
// Input  : pTarget - Entity with which to determine relationship.
// Output : Returns relationship value.
//-----------------------------------------------------------------------------
Disposition_t CNPC_MetroPolice::IRelationType(CBaseEntity *pTarget)
{
	Disposition_t disp = BaseClass::IRelationType(pTarget);

	if ( pTarget == NULL )
		return disp;

	// If the player's not a criminal, then we don't necessary hate him
	if ( pTarget->Classify() == CLASS_PLAYER )
	{
		if ( !PlayerIsCriminal() && (disp == D_HT) )
		{
			// If we're pissed at the player, we're allowed to hate them.
			if ( m_flChasePlayerTime && m_flChasePlayerTime > gpGlobals->curtime )
				return D_HT;
			return D_NU;
		}
	}

	return disp;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	if( OldState == NPC_STATE_COMBAT )
	{
		// leaving combat. Now we can point again next time.
		m_fCanPoint = true;
	}

	BaseClass::OnStateChange( OldState, NewState );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::OnAnimEventStartDeployManhack( void )
{
	Assert( m_iManhacks );
	
	if ( m_iManhacks <= 0 )
	{
		DevMsg( "Error: Throwing manhack but out of manhacks!\n" );
		return;
	}

	m_iManhacks--;

	// Turn off the manhack on our body
	if ( m_iManhacks <= 0 )
	{
		SetBodygroup( METROPOLICE_BODYGROUP_MANHACK, false );
	}

	// Create the manhack to throw
	CNPC_Manhack *pManhack = (CNPC_Manhack *)CreateEntityByName( "npc_manhack" );
	
	Vector	vecOrigin;
	QAngle	vecAngles;

	int handAttachment = LookupAttachment( "LHand" );
	GetAttachment( handAttachment, vecOrigin, vecAngles );

	pManhack->SetLocalOrigin( vecOrigin );
	pManhack->SetLocalAngles( vecAngles );
	pManhack->AddSpawnFlags( (SF_MANHACK_PACKED_UP|SF_MANHACK_CARRIED|SF_NPC_WAIT_FOR_SCRIPT) );
	
	// Also fade if our parent is marked to do it
	if ( HasSpawnFlags( SF_NPC_FADE_CORPSE ) )
	{
		pManhack->AddSpawnFlags( SF_NPC_FADE_CORPSE );
	}

	pManhack->Spawn();

	// Make us move with his hand until we're deployed
	pManhack->SetParent( this, handAttachment );

	m_hManhack = pManhack;
}

//-----------------------------------------------------------------------------
// Anim event handlers
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::OnAnimEventDeployManhack( animevent_t *pEvent )
{
	// Let it go
	ReleaseManhack();

	Vector forward, right;
	GetVectors( &forward, &right, NULL );

	IPhysicsObject *pPhysObj = m_hManhack->VPhysicsGetObject();

	if ( pPhysObj )
	{
		Vector	yawOff = right * random->RandomFloat( -1.0f, 1.0f );

		Vector	forceVel = ( forward + yawOff * 16.0f ) + Vector( 0, 0, 250 );
		Vector	forceAng = vec3_origin;

		// Give us velocity
		pPhysObj->AddVelocity( &forceVel, &forceAng );
	}

	// Stop dealing with this manhack
	m_hManhack = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::OnAnimEventShove( void )
{
	CBaseEntity *pHurt = CheckTraceHullAttack( 16, Vector(-16,-16,-16), Vector(16,16,16), 15, DMG_CLUB, 1.0f, false );

	if ( pHurt )
	{
		Vector vecForceDir = ( pHurt->WorldSpaceCenter() - WorldSpaceCenter() );

		CBasePlayer *pPlayer = ToBasePlayer( pHurt );

		if ( pPlayer != NULL )
		{
			//Kick the player angles
			pPlayer->ViewPunch( QAngle( 8, 14, 0 ) );

			Vector	dir = pHurt->GetAbsOrigin() - GetAbsOrigin();
			VectorNormalize(dir);

			QAngle angles;
			VectorAngles( dir, angles );
			Vector forward, right;
			AngleVectors( angles, &forward, &right, NULL );

			//If not on ground, then don't make them fly!
			if ( !(pHurt->GetFlags() & FL_ONGROUND ) )
				  forward.z = 0.0f;

			//Push the target back
			pHurt->ApplyAbsVelocityImpulse( forward * 250.0f );

			// Force the player to drop anyting they were holding
			pPlayer->ForceDropOfCarriedPhysObjects();
		}

		// Play a random attack hit sound
		EmitSound( "NPC_Metropolice.Shove" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::OnAnimEventBatonOn( void )
{
#ifndef HL2MP

	CWeaponStunStick *pStick = dynamic_cast<CWeaponStunStick *>(GetActiveWeapon());

	if ( pStick )
	{
		pStick->SetStunState( true );
	}
#endif

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::OnAnimEventBatonOff( void )
{
#ifndef HL2MP

	CWeaponStunStick *pStick = dynamic_cast<CWeaponStunStick *>(GetActiveWeapon());
	
	if ( pStick )
	{
		pStick->SetStunState( false );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//
// Input  : *pEvent - 
//
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::HandleAnimEvent( animevent_t *pEvent )
{
	// Shove!
	if ( pEvent->event == AE_METROPOLICE_SHOVE )
	{
		OnAnimEventShove();
		return;
	}

	if ( pEvent->event == AE_METROPOLICE_BATON_ON )
	{
		OnAnimEventBatonOn();
		return;
	}

	if ( pEvent->event == AE_METROPOLICE_BATON_OFF )
	{
		OnAnimEventBatonOff();
		return;
	}

	if ( pEvent->event == AE_METROPOLICE_START_DEPLOY )
	{
		OnAnimEventStartDeployManhack();
		return;
	}

	if ( pEvent->event == AE_METROPOLICE_DRAW_PISTOL )
	{
		m_fWeaponDrawn = true;
		if( GetActiveWeapon() )
		{
			GetActiveWeapon()->RemoveEffects( EF_NODRAW );
		}
		return;
	}

	if ( pEvent->event == AE_METROPOLICE_DEPLOY_MANHACK )
	{
		OnAnimEventDeployManhack( pEvent );
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}


//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	if ( interactionType == g_interactionMetrocopIdleChatter )
	{
		m_nIdleChatterType = (int)data;
		return true;
	}

	if ( interactionType == g_interactionMetrocopClearSentenceQueues )
	{
			m_Sentences.ClearQueue();
			return true;
	}

	// React to being hit by physics objects
	if ( interactionType == g_interactionHitByPlayerThrownPhysObj )
	{
		// Ignore if I'm in scripted state
		if ( !IsInAScript() && (m_NPCState != NPC_STATE_SCRIPT) )
		{
			SetCondition( COND_METROPOLICE_PHYSOBJECT_ASSAULT );
		}
		else
		{
			AdministerJustice();
		}

		// See if the object is the cupcop can. If so, fire the output (for x360 achievement)
		CBaseProp *pProp = (CBaseProp*)data;
		if( pProp != NULL )
		{
			if( pProp->NameMatches("cupcop_can") )
				m_OnCupCopped.FireOutput( this, NULL );
		}

		return true;
	}

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//---------------------------------------------------------------------------
Activity CNPC_MetroPolice::NPC_TranslateActivity( Activity newActivity )
{
	if( IsOnFire() && newActivity == ACT_RUN )
	{
		return ACT_RUN_ON_FIRE;
	}

	// If we're shoving, see if we should be more forceful in doing so
	if ( newActivity == ACT_PUSH_PLAYER )
	{
		if ( m_nNumWarnings >= METROPOLICE_MAX_WARNINGS )
			return ACT_MELEE_ATTACK1;
	}

	newActivity = BaseClass::NPC_TranslateActivity( newActivity );

	// Do these after any behaviors have translated
	if( newActivity == ACT_RUN)
	{
		if ( m_iHealth <= METROPOLICE_LIMP_HEALTH )
		{
			// limp!
			return ACT_RUN_HURT;
		}
	}
	else if( newActivity == ACT_WALK)
	{
		if ( m_iHealth <= METROPOLICE_LIMP_HEALTH )
		{
			// limp!
			return ACT_WALK_HURT;
		}
	}

	// This will put him into an angry idle, which will then be translated
	// by the weapon to the appropriate type. 
	if ( m_fWeaponDrawn && newActivity == ACT_IDLE && ( GetState() == NPC_STATE_COMBAT || GetState() == NPC_STATE_ALERT || BatonActive() ) )
	{
		newActivity = ACT_IDLE_ANGRY;
	}

	return newActivity;
}

//-----------------------------------------------------------------------------
// Purpose: Makes the held manhack solid
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::ReleaseManhack( void )
{
	Assert( m_hManhack );

	// Make us physical
	m_hManhack->RemoveSpawnFlags( SF_MANHACK_CARRIED );
	m_hManhack->CreateVPhysics();

	// Release us
	m_hManhack->RemoveSolidFlags( FSOLID_NOT_SOLID );
	m_hManhack->SetMoveType( MOVETYPE_VPHYSICS );
	m_hManhack->SetParent( NULL );

	// Make us active
	m_hManhack->RemoveSpawnFlags( SF_NPC_WAIT_FOR_SCRIPT );
	m_hManhack->ClearSchedule( "Manhack released by metropolice" );
	
	// Start him with knowledge of our current enemy
	if ( GetEnemy() )
	{
		m_hManhack->SetEnemy( GetEnemy() );
		m_hManhack->SetState( NPC_STATE_COMBAT );

		m_hManhack->UpdateEnemyMemory( GetEnemy(), GetEnemy()->GetAbsOrigin() );
	}

	// Place him into our squad so we can communicate
	if ( m_pSquad )
	{
		m_pSquad->AddToSquad( m_hManhack );
	}
}

//-----------------------------------------------------------------------------
// Try to enter a slot where we shoot a pistol 
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::TryToEnterPistolSlot( int nSquadSlot )
{
	// This logic here will not allow us to occupy the squad slot
	// too soon after we already were in it.
	if ( ( m_LastShootSlot != nSquadSlot || !m_TimeYieldShootSlot.Expired() ) &&
			OccupyStrategySlot( nSquadSlot ) )
	{
		if ( m_LastShootSlot != nSquadSlot )
		{
			m_TimeYieldShootSlot.Reset();
			m_LastShootSlot = nSquadSlot;
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// How many squad members are trying to arrest the player?
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::SquadArrestCount()
{
	int nCount = 0;

	AISquadIter_t iter;
	CAI_BaseNPC *pSquadmate = m_pSquad->GetFirstMember( &iter );
	while ( pSquadmate )
	{
		if ( pSquadmate->IsCurSchedule(	SCHED_METROPOLICE_ARREST_ENEMY ) ||
			pSquadmate->IsCurSchedule( SCHED_METROPOLICE_WARN_AND_ARREST_ENEMY ) )
		{
			++nCount;
		}

		pSquadmate = m_pSquad->GetNextMember( &iter );
	}

	return nCount;
}


//-----------------------------------------------------------------------------
// Arrest schedule selection 
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::SelectScheduleArrestEnemy()
{
	if ( !HasSpawnFlags( SF_METROPOLICE_ARREST_ENEMY ) || !IsInSquad() )
		return SCHED_NONE;

	if ( !HasCondition( COND_SEE_ENEMY ) )
		return SCHED_NONE;

	// First guy that sees the enemy will tell him to freeze
	if ( OccupyStrategySlot( SQUAD_SLOT_POLICE_ARREST_ENEMY ) )
		return SCHED_METROPOLICE_WARN_AND_ARREST_ENEMY;

	// Squad members 1 -> n will simply gain a line of sight
	return SCHED_METROPOLICE_ARREST_ENEMY;
}


//-----------------------------------------------------------------------------
// Combat schedule selection 
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::SelectScheduleNewEnemy()
{
	// ---------------------
	// Draw Weapon
	// ---------------------
	if ( !m_fWeaponDrawn )
		return SCHED_METROPOLICE_DRAW_PISTOL;

	int nSched = SelectScheduleArrestEnemy();
	if ( nSched != SCHED_NONE )
		return nSched;

	CBaseEntity *pEnemy = GetEnemy();
	m_flNextLedgeCheckTime = gpGlobals->curtime;

	// Switch our baton on, if it's not already
	if ( HasBaton() && BatonActive() == false && IsCurSchedule( SCHED_METROPOLICE_ACTIVATE_BATON ) == false )
	{
		SetTarget( pEnemy );
		SetBatonState( true );
		m_flBatonDebounceTime = gpGlobals->curtime + random->RandomFloat( 2.5f, 4.0f );
		return SCHED_METROPOLICE_ACTIVATE_BATON;
	}

	if ( m_pSquad && pEnemy )
	{
		if( CanDeployManhack() && OccupyStrategySlot( SQUAD_SLOT_POLICE_DEPLOY_MANHACK ) )
		{
			return SCHED_METROPOLICE_DEPLOY_MANHACK;
		}
		else if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && m_fCanPoint && (OccupyStrategySlot( SQUAD_SLOT_POLICE_HARASS ) || OccupyStrategySlot( SQUAD_SLOT_ATTACK1 )) )
		{
			// Harass the target (demand surrender, freeze, etc.)
			m_fCanPoint = false;
			return SCHED_METROPOLICE_HARASS;
		}
		else if( !HasCondition( COND_SEE_ENEMY ) )
		{
			// If I for some reason cant see that enemy when in a squad, get into position!
			return SCHED_ESTABLISH_LINE_OF_FIRE;
		}

		return SCHED_TAKE_COVER_FROM_ENEMY;
	}

	return SCHED_NONE;
}


//-----------------------------------------------------------------------------
// Sound investigation 
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::SelectNonCombatSchedule()
{
	if ( HasCondition( COND_LIGHT_DAMAGE ) || HasCondition( COND_HEAVY_DAMAGE ) )
	{
		AI_EnemyInfo_t *pDanger = GetEnemies()->GetDangerMemory();
		if( pDanger && FInViewCone(pDanger->vLastKnownLocation) && !BaseClass::FVisible(pDanger->vLastKnownLocation) )
		{
			// I've been hurt, I'm facing the danger, but I don't see it, so move from this position.
			return SCHED_TAKE_COVER_FROM_ORIGIN;
		}
		else if ( SelectWeightedSequence( ACT_SMALL_FLINCH ) != ACTIVITY_NOT_AVAILABLE )
		{
			// I'm not facing the danger but it hasnt triggered an attack response
			return SCHED_ALERT_SMALL_FLINCH;
		}
	}

	// SEE_ENEMY is set if LOS is available *and* we're looking the right way
	// Different cases for criminal-or-no
	if ( !HasCondition( COND_SEE_ENEMY ) && GetEnemy() == NULL )
	{
		if ( HasCondition( COND_HEAR_COMBAT ) || HasCondition( COND_HEAR_PLAYER ) )
		{
			if ( PlayerIsCriminal() )
			{
				if ( m_pSquad && OccupyStrategySlot( SQUAD_SLOT_INVESTIGATE_SOUND ) )
				{
					return SCHED_METROPOLICE_INVESTIGATE_SOUND;
				}
				return SCHED_ALERT_FACE_BESTSOUND;	//SCHED_ALERT_REACT_TO_COMBAT_SOUND
			}
			else
			{
				return SCHED_METROPOLICE_INVESTIGATE_SOUND;	//TODO; Custom sched
			}
		}
		else if ( HasCondition( COND_HEAR_WORLD ) || HasCondition( COND_HEAR_BULLET_IMPACT ) )
		{
			// Just heard something not too loud, pay attention to/comment about it but dont move at all
			return SCHED_ALERT_FACE_BESTSOUND;
		}
	}

	// Scan around for new enemies
	if ( HasCondition( COND_ENEMY_DEAD ) && SelectWeightedSequence( ACT_VICTORY_DANCE ) != ACTIVITY_NOT_AVAILABLE )
	{
		return SCHED_VICTORY_DANCE;
	}

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Combat schedule selection 
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::SelectCombatSchedule()
{
	// -----------
	// New Enemy
	// -----------
	if ( HasCondition( COND_NEW_ENEMY ) )
	{
		if ( HasCondition( COND_SEE_ENEMY ) )
		{
			AnnounceEnemyType( GetEnemy() );
		}
		int nResult = SelectScheduleNewEnemy();
		if ( nResult != SCHED_NONE )
			return nResult;
	}

	// ---------------------
	// Low Ammo
	// ---------------------
	bool bHighHealth = ((float)GetHealth() / (float)GetMaxHealth() > 0.75f);

	// This will make the cops run backwards + shoot at the same time
	if ( !bHighHealth && !HasBaton() )
	{
		if ( GetActiveWeapon() && (GetActiveWeapon()->m_iClip1 <= 5) )
		{
			return SCHED_HIDE_AND_RELOAD;
		}
	}

	// ----------------------
	// Light Damage
	// ----------------------
	if (!HasBaton() && GetEnemy() && ((float)m_nRecentDamage / (float)GetMaxHealth()) > RECENT_DAMAGE_THRESHOLD)
	{
		m_nRecentDamage = 0;
		m_flRecentDamageTime = 0;
		m_Sentences.Speak( "METROPOLICE_COVER_HEAVY_DAMAGE", SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL );

		if ( m_bSimpleCops )
		{
			return SCHED_RUN_FROM_ENEMY;
		}
		else
		{
			return SCHED_TAKE_COVER_FROM_ENEMY;
		}
	}

	// Fallback - if I for some reason havent been able to draw, do it before attacking!
	if( !m_fWeaponDrawn )
	{
		return SCHED_METROPOLICE_DRAW_PISTOL;
	}

	// ----------------------
	// Attacking
	// ----------------------
	if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
		// Range attack if we're able
		if ( TryToEnterPistolSlot( SQUAD_SLOT_ATTACK1 ) || TryToEnterPistolSlot( SQUAD_SLOT_ATTACK2 ) || OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
		{
			return SCHED_RANGE_ATTACK1;
		}
		else if ( HasCondition( COND_WEAPON_SIGHT_OCCLUDED ) && !HasBaton() )
		{
			// If they are hiding behind something that we can destroy, start shooting at it.
			CBaseEntity *pBlocker = GetEnemyOccluder();
			if ( pBlocker && pBlocker->GetHealth() > 0 && OccupyStrategySlotRange( SQUAD_SLOT_POLICE_ATTACK_OCCLUDER1, SQUAD_SLOT_POLICE_ATTACK_OCCLUDER2 ) )
			{
				m_Sentences.Speak( "METROPOLICE_SHOOT_COVER" );
				return SCHED_SHOOT_ENEMY_COVER;
			}
		}
		else
		{
			// We're not in a shoot slot and our enemy isnt in cover
			m_LastShootSlot = SQUAD_SLOT_NONE;
			DesireCrouch();
			return SCHED_TAKE_COVER_FROM_ENEMY;
		}
	}

	if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
	{
		if ( m_BatonSwingTimer.Expired() )
		{
			// Stop chasing the player now that we've taken a swing at them
			m_flChasePlayerTime = 0;
			m_BatonSwingTimer.Set( 1.0, 1.75 );
			return SCHED_MELEE_ATTACK1;
		}
		else
			return SCHED_COMBAT_FACE;
	}

	if ( HasCondition( COND_TOO_CLOSE_TO_ATTACK ) )
	{
		return SCHED_BACK_AWAY_FROM_ENEMY;
	}

	// If you can't attack, but you can deploy a manhack, do it!
	if( CanDeployManhack() && OccupyStrategySlot( SQUAD_SLOT_POLICE_DEPLOY_MANHACK ) )
		return SCHED_METROPOLICE_DEPLOY_MANHACK;

	// ---------------------
	// No LOS
	// ---------------------
	if ( HasCondition( COND_ENEMY_OCCLUDED ) )
	{
		// stand up, just in case
		DesireStand();

		// Stun-grenade the enemy, then charge
//!		if ( CanGrenadeEnemy() && OccupyStrategySlot( SQUAD_SLOT_POLICE_ATTACK_OCCLUDER1 ) )
//!		{
//!			return SCHED_RANGE_ATTACK2;
//!		}
		if ( GetEnemy() && !(GetEnemy()->GetFlags() & FL_NOTARGET) )
		{
			// Try to get into a better position
			if ( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				return SCHED_ESTABLISH_LINE_OF_FIRE;
		}

		// If I'm a long, long way away, establish a LOF anyway. Once I get there I'll
		// start respecting the squad slots again.
		float flDistSq = GetEnemy()->WorldSpaceCenter().DistToSqr( WorldSpaceCenter() );
		if ( flDistSq > Square(3000) )
			return SCHED_MOVE_TO_WEAPON_RANGE;

		// Otherwise tuck in.
		Remember( bits_MEMORY_INCOVER );
		return SCHED_STANDOFF;
	}
	else if ( HasCondition( COND_SEE_ENEMY ) && !HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
		// Charge in!
		if ( HasCondition( COND_TOO_FAR_TO_ATTACK ) )
		{
			return SCHED_MOVE_TO_WEAPON_RANGE;
		}
		else if ( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) || OccupyStrategySlot( SQUAD_SLOT_POLICE_CHARGE_ENEMY ) )
		{
			return SCHED_METROPOLICE_ADVANCE;
		}
		// Otherwise tuck in.
		return SCHED_STANDOFF;
	}

	// If you can't attack, but you have a baton & there's a physics object in front of you, swat it
	if ( m_hBlockingProp && HasBaton() )
	{
		SetTarget( m_hBlockingProp );
		m_hBlockingProp = NULL;
		return SCHED_METROPOLICE_SMASH_PROP;
	}

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: This is a bridge between stunstick, NPC and its behavior
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::ShouldKnockOutTarget( CBaseEntity *pTarget )
{
	if ( m_PolicingBehavior.IsEnabled() && m_PolicingBehavior.ShouldKnockOutTarget( pTarget ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: This is a bridge between stunstick, NPC and its behavior
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::KnockOutTarget( CBaseEntity *pTarget )
{
	if ( m_PolicingBehavior.IsEnabled() )
	{
		m_PolicingBehavior.KnockOutTarget( pTarget );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if can throw a grenade at the target point, 
//			and hasn't checked lately.
// Input  : &vecTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::CanGrenadeEnemy( )
{
	CBaseEntity *pEnemy = GetEnemy();

	Assert( pEnemy != NULL );

	if( pEnemy )
	{
		// throw to where we last saw them.
		return CanThrowGrenade( GetEnemies()->LastSeenPosition( pEnemy ) );
	}

	return false;
}

bool CNPC_MetroPolice::CanThrowGrenade( const Vector &vecTarget )
{
//!	if ( m_iNumGrenades < 1 )	// Out of grenades!
//!		return false;

	if ( gpGlobals->curtime < m_flNextGrenadeCheck )
		return false;

	float flDist;
	flDist = ( vecTarget - GetAbsOrigin() ).Length();

	if( flDist > 800 || flDist < 128 )
	{
		// Too close or too far!
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return false;
	}

	// -----------------------
	// If moving, don't check.
	// -----------------------
	if ( m_flGroundSpeed != 0 )
		return false;

	Vector vecEnemyLKP = GetEnemyLKP();
	if ( !( GetEnemy()->GetFlags() & FL_ONGROUND ) && GetEnemy()->GetWaterLevel() == 0 && vecEnemyLKP.z > (GetAbsOrigin().z + WorldAlignMaxs().z)  )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to be grenaded.
		// don't throw grenades at anything that isn't on the ground or otherwise swimming!
		m_flNextGrenadeCheck = gpGlobals->curtime + 0.5; // half a second.
		return false;
	}

	// ---------------------------------------------------------------------
	// Are any of my squad members near the intended grenade impact area?
	// ---------------------------------------------------------------------
	if ( m_pSquad )
	{
		if (m_pSquad->SquadMemberInRange( vecTarget, 256 ))
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.

			// Tell my squad members to clear out so I can get a grenade in
			CSoundEnt::InsertSound( SOUND_MOVE_AWAY | SOUND_CONTEXT_COMBINE_ONLY, vecTarget, 256, 0.1 );
			return false;
		}
	}

	return CheckCanThrowGrenade( vecTarget );
}

bool CNPC_MetroPolice::CheckCanThrowGrenade( const Vector &vecTarget )
{
	//NDebugOverlay::Line( EyePosition(), vecTarget, 0, 255, 0, false, 5 );

	// ---------------------------------------------------------------------
	// Check that throw is legal and clear
	// ---------------------------------------------------------------------
	// FIXME: this is only valid for hand grenades, not RPG's
	Vector vecToss;
	Vector vecMins = -Vector(4,4,4);
	Vector vecMaxs = Vector(4,4,4);
	if( FInViewCone( vecTarget ) && CBaseEntity::FVisible( vecTarget ) )
	{
		vecToss = VecCheckThrow( this, EyePosition(), vecTarget, 600, 1.0, &vecMins, &vecMaxs );
	}
	else
	{
		// Have to try a high toss. Do I have enough room?
		trace_t tr;
		AI_TraceLine( EyePosition(), EyePosition() + Vector( 0, 0, 64 ), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		if( tr.fraction != 1.0 )
		{
			return false;
		}

		vecToss = VecCheckToss( this, EyePosition(), vecTarget, -1, 1.0, true, &vecMins, &vecMaxs );
	}

	if ( vecToss != vec3_origin )
	{
		m_vecTossVelocity = vecToss;

		// don't check again for a while.
		m_flNextGrenadeCheck = gpGlobals->curtime + 0.5; // 1/2 second.
		return true;
	}
	else
	{
		// don't check again for a while.
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult )
{
	if ( pMoveGoal->directTrace.pObstruction )
	{
		// Is it a physics prop? Store it off as the last thing to block me
		CPhysicsProp *pProp = dynamic_cast<CPhysicsProp*>( pMoveGoal->directTrace.pObstruction );
		if ( pProp && pProp->GetHealth() )
		{
			m_hBlockingProp = pProp;
		}
		else
		{
			m_hBlockingProp = NULL;
		}
	}

	return BaseClass::OnObstructionPreSteer( pMoveGoal, distClear, pResult );
}

//-----------------------------------------------------------------------------
// Combat schedule selection 
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::SelectMoveToLedgeSchedule()
{
	// Prevent a bunch of unnecessary raycasts.
	if ( m_flNextLedgeCheckTime > gpGlobals->curtime )
		return SCHED_NONE;

	// If the NPC is above the airboat (say, on a bridge), make sure he
	// goes to the closest ledge. (may need a spawnflag for this)
	if ( (GetAbsOrigin().z - GetShootTarget()->GetAbsOrigin().z) >= 150.0f )
	{
		m_flNextLedgeCheckTime = gpGlobals->curtime + 3.0f;

		// We need to be able to shoot downward at a 60 degree angle.
		Vector vecDelta;
		VectorSubtract( GetShootTarget()->WorldSpaceCenter(), Weapon_ShootPosition(), vecDelta );
		vecDelta.z = 0.0f;
		VectorNormalize( vecDelta );

		// At this point, vecDelta is 45 degrees below horizontal.
		vecDelta.z = -1;
		vecDelta *= 100.0f;

		trace_t tr;
		CTraceFilterWorldOnly traceFilter;
		UTIL_TraceLine( Weapon_ShootPosition(), Weapon_ShootPosition() + vecDelta, MASK_SOLID, &traceFilter, &tr );

		//!!!FIXME; Need a better sched here
		if (tr.endpos.z >= GetAbsOrigin().z - 25.0f )
			return SCHED_METROPOLICE_SMG_BURST_ATTACK;
	}
	
	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Any bullets that do more than 7 damage
	if ( info.GetDamageType() & (DMG_BULLET|DMG_BUCKSHOT|DMG_SHOCK) )
	{
		if ( info.GetDamage() >= 7 )
			return true;
	}

	// Any damage that is equal to or more than 15% of my health
	return ( info.GetDamage() >= (GetMaxHealth() * 0.15f) );
}

//-----------------------------------------------------------------------------
// TraceAttack
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	// This is needed so we can keep track of the direction of the shot
	// because we're going to use it to choose the flinch animation
	if ( m_bSimpleCops )
	{
		if ( m_takedamage == DAMAGE_YES )
		{
			Vector vecLastHitDirection;
			VectorIRotate( vecDir, EntityToWorldTransform(), vecLastHitDirection );

			// Point *at* the shooter
			vecLastHitDirection *= -1.0f;

			QAngle lastHitAngles;
			VectorAngles( vecLastHitDirection, lastHitAngles );
			m_flLastHitYaw	= lastHitAngles.y;
		}
	}

	BaseClass::TraceAttack( info, vecDir, ptr );
}

//-----------------------------------------------------------------------------
// Determines the best type of flinch anim to play.
//-----------------------------------------------------------------------------
Activity CNPC_MetroPolice::GetFlinchActivity( bool bHeavyDamage, bool bGesture )
{
	if ( !bGesture && m_bSimpleCops )
	{
		// Version for getting shot from behind
		if ( ( m_flLastHitYaw > 90 ) && ( m_flLastHitYaw < 270 ) )
		{
			Activity flinchActivity = (Activity)ACT_METROPOLICE_FLINCH_BEHIND;
			if ( SelectWeightedSequence ( flinchActivity ) != ACTIVITY_NOT_AVAILABLE )
				return flinchActivity;
		}
	}

	return BaseClass::GetFlinchActivity( bHeavyDamage, bGesture );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::PlayFlinchGesture( void )
{
	BaseClass::PlayFlinchGesture();

	// To ensure old playtested difficulty stays the same, stop cops shooting for a bit after gesture flinches
	GetShotRegulator()->FireNoEarlierThan( gpGlobals->curtime + 0.3 );
}

//-----------------------------------------------------------------------------
// We're taking cover from danger
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::AnnounceHarrassment( void )
{
	static const char *pWarnings[3] = 
	{
		"METROPOLICE_BACK_UP_A",
		"METROPOLICE_BACK_UP_B",
		"METROPOLICE_BACK_UP_C",
	};

	m_Sentences.Speak( pWarnings[ random->RandomInt( 0, ARRAYSIZE(pWarnings)-1 ) ], SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_NORMAL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::IncrementPlayerCriminalStatus( void )
{
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );

	if ( pPlayer )
	{
		AddLookTarget( pPlayer, 0.8f, 5.0f );

		if ( m_nNumWarnings < METROPOLICE_MAX_WARNINGS )
		{
			m_nNumWarnings++;
		}

		if ( m_nNumWarnings >= (METROPOLICE_MAX_WARNINGS-1) )
		{
			SetTarget( pPlayer );
			SetBatonState( true );
		}
	}

	m_flBatonDebounceTime = gpGlobals->curtime + random->RandomFloat( 2.0f, 4.0f );

	AnnounceHarrassment();

	m_bKeepFacingPlayer = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::SelectShoveSchedule( void )
{
	IncrementPlayerCriminalStatus();

	// Stop chasing the player now that we've taken a swing at them
	m_flChasePlayerTime = 0;
	return SCHED_METROPOLICE_SHOVE;
}

//-----------------------------------------------------------------------------
// Purpose: Chase after a player who's just pissed us off, and hit him
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::AdministerJustice( void )
{
	if ( !AI_IsSinglePlayer() )
		return;

	// If we're allowed to chase the player, do so. Otherwise, just threaten.
	if ( !IsInAScript() && (m_NPCState != NPC_STATE_SCRIPT) && HasSpawnFlags( SF_METROPOLICE_ALLOWED_TO_RESPOND ) )
	{
		if ( m_vecPreChaseOrigin == vec3_origin )
		{
			m_vecPreChaseOrigin = GetAbsOrigin();
			m_flPreChaseYaw = GetAbsAngles().y;
		}
		m_flChasePlayerTime = gpGlobals->curtime + RandomFloat( 3, 7 );

		// Attack the target
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);
		SetEnemy( pPlayer );
		SetState( NPC_STATE_COMBAT );
		UpdateEnemyMemory( pPlayer, pPlayer->GetAbsOrigin() );
	}
	else
	{
		// Watch the player for a time.
		m_bKeepFacingPlayer = true;

		// Try and find a nearby cop to administer justice
		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
		int nAIs = g_AI_Manager.NumAIs();
		for ( int i = 0; i < nAIs; i++ )
		{
			if ( ppAIs[i] == this )
				continue;

			if ( ppAIs[i]->Classify() == CLASS_METROPOLICE && FClassnameIs( ppAIs[i], "npc_metropolice" ) )
			{
				CNPC_MetroPolice *pNPC = assert_cast<CNPC_MetroPolice*>(ppAIs[i]);
				if ( pNPC->HasSpawnFlags( SF_METROPOLICE_ALLOWED_TO_RESPOND ) )
				{
					// Is he within site & range?
					if ( FVisible(pNPC) && pNPC->FVisible( UTIL_PlayerByIndex(1) ) && 
						UTIL_DistApprox( WorldSpaceCenter(), pNPC->WorldSpaceCenter() ) < 512 )
					{
						pNPC->AdministerJustice();
						break;
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Schedule selection 
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::SelectSchedule( void )
{
	if ( IsWaitingToRappel() && BehaviorSelectSchedule() )
	{
		return BaseClass::SelectSchedule();
	}

	if ( HasCondition( COND_ON_FIRE ) )
	{
//		m_Sentences.Speak( "METROPOLICE_ON_FIRE", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
		EmitSound( "NPC_MetroPolice.OnFireScream" );
		if ( random->RandomInt( 0, 1 ) )
		{
			return SCHED_METROPOLICE_BURNING_RUN;
		}
		else
		{
			return SCHED_METROPOLICE_BURNING_STAND;
		}
	}

#if 0
	if ( !GetEnemy() && HasCondition( COND_IN_PVS ) && AI_GetSinglePlayer() && !AI_GetSinglePlayer()->IsAlive() )
	{
		return SCHED_PATROL_WALK;
	}
#endif

	// React to being struck by a physics object
	if ( HasCondition( COND_METROPOLICE_PHYSOBJECT_ASSAULT ) )
	{
		ClearCondition( COND_METROPOLICE_PHYSOBJECT_ASSAULT );

		// See which state our player relationship is in
		if ( !PlayerIsCriminal() )
		{
			m_Sentences.Speak( "METROPOLICE_HIT_BY_PHYSOBJECT", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
			m_nNumWarnings = METROPOLICE_MAX_WARNINGS;
			AdministerJustice();
		}
		else if ( GlobalEntity_GetState( "gordon_precriminal" ) == GLOBAL_ON )
		{
			// We're not allowed to respond, but warn them
			m_Sentences.Speak( "METROPOLICE_IDLE_HARASS_PLAYER", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
		}
	}

	int nSched = SelectFlinchSchedule();
	if ( nSched != SCHED_NONE )
		return nSched;

	if ( HasBaton() )
	{
		// See if we're being told to activate our baton
		if ( m_bShouldActivateBaton && BatonActive() == false && IsCurSchedule( SCHED_METROPOLICE_ACTIVATE_BATON ) == false )
			return SCHED_METROPOLICE_ACTIVATE_BATON;

		if ( m_bShouldActivateBaton == false && BatonActive() && IsCurSchedule( SCHED_METROPOLICE_DEACTIVATE_BATON ) == false )
			return SCHED_METROPOLICE_DEACTIVATE_BATON;

		if( metropolice_chase_use_follow.GetBool() )
		{
			if( GetEnemy() )
			{
				AI_FollowParams_t params;
				params.formation = AIF_TIGHT;
				m_FollowBehavior.SetParameters( params );
				m_FollowBehavior.SetFollowTarget( GetEnemy() );
			}
		}
	}

	// See if the player is in our face (unless we're scripting)
	if ( !PlayerIsCriminal() )
	{
		if ( !IsInAScript() && (HasCondition( COND_METROPOLICE_PLAYER_TOO_CLOSE ) || m_bPlayerTooClose) )
		{
			// Don't hit the player too many times in a row, unless he's trying to push a cop who hasn't moved
			if ( m_iNumPlayerHits < 3 || m_vecPreChaseOrigin == vec3_origin )
			{
				ClearCondition( COND_METROPOLICE_PLAYER_TOO_CLOSE );
				m_bPlayerTooClose = false;
				
				return SelectShoveSchedule();
			}
		}
	}

	if ( m_NPCState != NPC_STATE_SCRIPT )
	{
		// If we're clubbing someone who threw something at us. chase them
		if ( m_NPCState == NPC_STATE_COMBAT && m_flChasePlayerTime > gpGlobals->curtime )
			return SCHED_CHASE_ENEMY;

		// Always run for cover from danger sounds
		if ( HasCondition( COND_HEAR_DANGER ) )
		{
			CSound *pSound;
			pSound = GetBestSound();

			Assert( pSound != NULL );
			if ( pSound )
			{
				if (pSound->m_iType & SOUND_DANGER)
				{
					AnnounceTakeCoverFromDanger( pSound );
					return SCHED_TAKE_COVER_FROM_BEST_SOUND;
				}
				if (!HasCondition( COND_SEE_ENEMY ) && ( pSound->m_iType & (SOUND_PLAYER | SOUND_PLAYER_VEHICLE | SOUND_COMBAT) ))
				{
					GetMotor()->SetIdealYawToTarget( pSound->GetSoundReactOrigin() );
				}
			}
		}

		// Cower when physics objects are thrown at me
		if ( HasCondition( COND_HEAR_PHYSICS_DANGER ) )
		{
			if ( m_flLastPhysicsFlinchTime + 4.0f <= gpGlobals->curtime )
			{
				m_flLastPhysicsFlinchTime = gpGlobals->curtime;
				return SCHED_FLINCH_PHYSICS;
			}
		}

		// ---------------------
		// No Ammo
		// ---------------------
		if ( HasCondition( COND_NO_PRIMARY_AMMO ) )
		{
			return SCHED_RELOAD;
		}

		// We've been told to move away from a target
		if ( HasCondition( COND_HEAR_MOVE_AWAY ) )
		{
			return SCHED_MOVE_AWAY;
		}

		if( BehaviorSelectSchedule() )
		{
			return BaseClass::SelectSchedule();
		}

		// If we've warned the player at all, watch him like a hawk
		if ( m_bKeepFacingPlayer && !PlayerIsCriminal() )
			return SCHED_TARGET_FACE;
	}

	switch( m_NPCState )
	{
	case NPC_STATE_IDLE:
		{
			int nSched = SelectNonCombatSchedule();
			if ( nSched != SCHED_NONE )
				return nSched;
		}
		break;

	case NPC_STATE_ALERT:
		{
			nSched = SelectNonCombatSchedule();
			if ( nSched != SCHED_NONE )
				return nSched;
		}
		break;

	case NPC_STATE_COMBAT:
		{
			int nSched = SelectCombatSchedule();
			if ( nSched != SCHED_NONE )
				return nSched;
		}
		break;
	}
	

	// If we're not in combat, and we've got a pre-chase origin, move back to it
	if ( ( m_NPCState != NPC_STATE_COMBAT ) && 
		 ( m_vecPreChaseOrigin != vec3_origin ) && 
		 ( m_flChasePlayerTime < gpGlobals->curtime ) )
	{
		return SCHED_METROPOLICE_RETURN_TO_PRECHASE;
	}

	// no special cases here, call the base class
	return BaseClass::SelectSchedule();
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : failedSchedule - 
//			failedTask - 
//			taskFailCode - 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( failedSchedule == SCHED_METROPOLICE_CHASE_ENEMY )
	{
		return SCHED_ESTABLISH_LINE_OF_FIRE;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_MetroPolice::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:
		return SCHED_METROPOLICE_TAKE_COVER;
	break;

	case SCHED_ALERT_FACE_BESTSOUND:
		if ( !IsCurSchedule( SCHED_METROPOLICE_ALERT_FACE_BESTSOUND, false ) )
		{
			return SCHED_METROPOLICE_ALERT_FACE_BESTSOUND;
		}
		return SCHED_ALERT_FACE_BESTSOUND;
	break;

	case SCHED_COMBAT_FACE:
		return SCHED_METROPOLICE_COMBAT_FACE;
	break;

	case SCHED_CHASE_ENEMY:
		if ( !IsRunningBehavior() )
		{
			return SCHED_METROPOLICE_CHASE_ENEMY;
		}
	break;

	case SCHED_ESTABLISH_LINE_OF_FIRE:
	{
		// If asked to advance, but ammo is very low, reload first!
#if 0
		if( Weapon_OwnsThisType( "weapon_pistol" ) )
		{
			if ( GetActiveWeapon() && (GetActiveWeapon()->m_iClip1 <= 3) )
			{
				return SCHED_HIDE_AND_RELOAD;
			}
		}
#endif
		if ( GetActiveWeapon() && HasCondition( COND_LOW_PRIMARY_AMMO ) )
		{
			return TranslateSchedule( SCHED_HIDE_AND_RELOAD );
		}
		if ( IsEnemyInAVehicle() )
		{
			int nSched = SelectMoveToLedgeSchedule();
			if ( nSched != SCHED_NONE )
				return nSched;
		}
		return SCHED_METROPOLICE_ESTABLISH_LINE_OF_FIRE;		
	}

	case SCHED_FAIL_ESTABLISH_LINE_OF_FIRE:
		if( HasCondition( COND_SEE_ENEMY ) )
		{
			return TranslateSchedule( SCHED_COMBAT_FACE );
		}
		else if ( !m_AssaultBehavior.HasAssaultCue() )
		{
			// Don't patrol if I'm in the middle of an assault, because 
			// I'll never return to the assault. 
			if ( GetEnemy() )
			{
				RememberUnreachable( GetEnemy() );
			}

			return TranslateSchedule( SCHED_PATROL_WALK );
		}
	break;

	case SCHED_WAKE_ANGRY:
		return SCHED_METROPOLICE_WAKE_ANGRY;
	break;

	case SCHED_HIDE_AND_RELOAD:
		if ( g_pGameRules->IsSkillLevel( SKILL_EASY ) )
		{
			return SCHED_RELOAD;
		}
		AnnounceOutOfAmmo();
		break;

	case SCHED_FAIL_TAKE_COVER:
		if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
		{
			//Theres absolutely no cover around, so try crouching
			if( !IsCrouching() && CouldShootIfCrouching(GetEnemy()) )
			{
				DesireCrouch();
			}
			return TranslateSchedule( SCHED_RANGE_ATTACK1 );
		}
	break;

	case SCHED_RANGE_ATTACK1:
	{
		if( !m_fWeaponDrawn )
		{
			return SCHED_METROPOLICE_DRAW_PISTOL;
		}
		if ( HasCondition( COND_NO_PRIMARY_AMMO ) )
		{
			// Ditch the strategy slot for attacking (which we just reserved!)
			VacateStrategySlot();
			return TranslateSchedule( SCHED_RELOAD );
		}

		if ( CrouchIsDesired() && !HasCondition( COND_HEAVY_DAMAGE ) && !HasCondition( COND_REPEATED_DAMAGE ) )
		{
			// See if we can crouch and shoot
			if (GetEnemy() != NULL)
			{
				float dist = (GetLocalOrigin() - GetEnemy()->GetLocalOrigin()).Length();

				// only crouch if they are relatively far away
				if (dist > 200)
				{
					// try crouching
					Crouch();

					Vector targetPos = GetEnemy()->BodyTarget(GetActiveWeapon()->GetLocalOrigin());

					// if we can't see it crouched, stand up
					if (!WeaponLOSCondition(GetLocalOrigin(),targetPos,false))
					{
						Stand();
					}
				}
			}
		}
		else
		{
			// always assume standing
			Stand();
		}

		if( Weapon_OwnsThisType( "weapon_smg1" ) )
		{
			if ( IsEnemyInAVehicle() )
			{
				return SCHED_METROPOLICE_SMG_BURST_ATTACK;
			}
			else
			{
				return SCHED_METROPOLICE_SMG_NORMAL_ATTACK;
			}
		}
		return SCHED_METROPOLICE_RANGE_ATTACK1;
	}

	case SCHED_METROPOLICE_ADVANCE:
	{
		if ( m_NextChargeTimer.Expired() && metropolice_charge.GetBool() )
		{
			if ( Weapon_OwnsThisType( "weapon_smg1" ) )
			{
				if (  GetEnemy() && GetEnemy()->GetAbsOrigin().DistToSqr( GetAbsOrigin() ) > 300*300 )
				{
					if ( OccupyStrategySlot( SQUAD_SLOT_POLICE_CHARGE_ENEMY ) )
					{
						m_NextChargeTimer.Set( 10, 20 );
						return SCHED_METROPOLICE_CHARGE;
					}
				}
			}
			else
			{
				m_NextChargeTimer.Set( 9999 );
			}
		}
		else if ( GetHintGroup() != NULL_STRING )
		{
			// Don't charge in if there's a hint group
			return TranslateSchedule( SCHED_ESTABLISH_LINE_OF_FIRE );
		}
		return SCHED_METROPOLICE_ADVANCE;
	}

	case SCHED_METROPOLICE_HARASS:
	{
		if( m_pSquad->IsLeader( this ) && GetEnemy()->IsPlayer() )
		{
			// Only harass the player, otherwise go to LOS!
			if( m_pSquad->NumMembers() == 1 )
			{
				// All alone! Hafta do my own pursuing.
				return TranslateSchedule( SCHED_ESTABLISH_LINE_OF_FIRE );
			}
			// Just shout at the player. Let a cronie advance.
			return SCHED_METROPOLICE_HARASS;
		}
		else
		{
			// Overwatch.
			return TranslateSchedule( SCHED_ESTABLISH_LINE_OF_FIRE );
		}
	}

	case SCHED_VICTORY_DANCE:
		return SCHED_METROPOLICE_VICTORY_DANCE;
	break;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}


//-----------------------------------------------------------------------------
// Movie and Shooty
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::ShouldMoveAndShoot()
{
	if ( HasSpawnFlags( SF_METROPOLICE_ARREST_ENEMY ) )
		return false;

	if ( g_pGameRules->IsSkillLevel( SKILL_EASY ) )
		return false;

	// Dont move n' shoot when hurt
	if ( m_iHealth <= METROPOLICE_LIMP_HEALTH )
		return false;

	if ( HasCondition( COND_NO_PRIMARY_AMMO, false ) )
		return false;

	if ( IsOnFire() )
		return false;

	// Do not move and shoot when running away
	if( IsCurSchedule( SCHED_TAKE_COVER_FROM_BEST_SOUND, false ) )
		return false;

	if( IsCurSchedule( SCHED_FLEE_FROM_BEST_SOUND, false ) )
		return false;
	
	// Do not move and shoot when going from idle to combat
	if( IsCurSchedule( SCHED_METROPOLICE_HARASS, false ) )
		return false;

//	if ( ShouldAttemptToStitch() )
//		return false;

	return BaseClass::ShouldMoveAndShoot();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::OnBeginMoveAndShoot()
{
	if ( BaseClass::OnBeginMoveAndShoot() )
	{
		if( HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			return true; // already have the slot I need

		if( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			return true;
	}

	return false;
}

void CNPC_MetroPolice::OnEndMoveAndShoot()
{
	VacateStrategySlot();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::StartTask( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
	case TASK_METROPOLICE_WAIT_FOR_SENTENCE:
		{
			if ( FOkToMakeSound( pTask->flTaskData ) )
			{
				TaskComplete();
			}
		}
		break;

	case TASK_METROPOLICE_GET_PATH_TO_PRECHASE:
		{
			Assert( m_vecPreChaseOrigin != vec3_origin );
			if ( GetNavigator()->SetGoal( m_vecPreChaseOrigin ) )
			{
				QAngle vecAngles( 0, m_flPreChaseYaw, 0 );
				GetNavigator()->SetArrivalDirection( vecAngles );
				TaskComplete();
			}
			else
			{
				TaskFail( FAIL_NO_ROUTE );
			}
			break;
		}

	case TASK_METROPOLICE_CLEAR_PRECHASE:
		{
			m_vecPreChaseOrigin = vec3_origin;
			m_flPreChaseYaw = 0;
			TaskComplete();
			break;
		}

	case TASK_METROPOLICE_ACTIVATE_BATON:
		{
			// Simply early out if we're in here without a baton
			if ( HasBaton() == false )
			{
				TaskComplete();
				break;
			}

			bool activate = ( pTask->flTaskData != 0 );

			if ( activate )
			{
				if ( BatonActive() || m_bShouldActivateBaton == false )
				{
					TaskComplete();
					break;
				}

				EmitSound( "NPC_MetroPolice.ActivateBaton" );
				SetIdealActivity( (Activity) ACT_ACTIVATE_BATON );
			}
			else
			{
				if ( BatonActive() == false || m_bShouldActivateBaton )
				{
					TaskComplete();
					break;
				}

				EmitSound( "NPC_MetroPolice.DeactivateBaton" );
				SetIdealActivity( (Activity) ACT_DEACTIVATE_BATON );
			}
		}
		break;

	case TASK_METROPOLICE_DIE_INSTANTLY:
		{
			CTakeDamageInfo info;

			info.SetAttacker( this );
			info.SetInflictor( this );
			info.SetDamage( m_iHealth );
			info.SetDamageType( pTask->flTaskData );
			info.SetDamageForce( Vector( 0.1, 0.1, 0.1 ) );

			TakeDamage( info );

			TaskComplete();
		}
		break;

	case TASK_METROPOLICE_RESET_LEDGE_CHECK_TIME:
		m_flNextLedgeCheckTime = gpGlobals->curtime;
		TaskComplete();
		break;

	case TASK_METROPOLICE_LEAD_ARREST_ENEMY:
	case TASK_METROPOLICE_ARREST_ENEMY:
		m_flTaskCompletionTime = gpGlobals->curtime + pTask->flTaskData;
		break;

	case TASK_METROPOLICE_SIGNAL_FIRING_TIME:
		EnemyResistingArrest();
		TaskComplete();
		break;

	case TASK_METROPOLICE_HARASS:
		{
			if( !( m_spawnflags & SF_METROPOLICE_NOCHATTER ) )
			{
				if( GetEnemy() && GetEnemy()->GetWaterLevel() > 0 )
				{
					EmitSound( "NPC_MetroPolice.WaterSpeech" );
				}
				else
				{
					EmitSound( "NPC_MetroPolice.HidingSpeech" );
				}
			}

			TaskComplete();
		}
		break;

	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

//-----------------------------------------------------------------------------
// He's resisting arrest!
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::EnemyResistingArrest()
{
	// Prevent any other arrest from being made in this squad
	// and tell them all that the player is resisting arrest!
	if ( m_pSquad != NULL )
	{
		AISquadIter_t iter;
		CAI_BaseNPC *pSquadmate = m_pSquad->GetFirstMember( &iter );
		while ( pSquadmate )
		{
			pSquadmate->RemoveSpawnFlags( SF_METROPOLICE_ARREST_ENEMY );
			pSquadmate->SetCondition( COND_METROPOLICE_ENEMY_RESISTING_ARREST );
			pSquadmate = m_pSquad->GetNextMember( &iter );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
#define FLEEING_DISTANCE_SQR (100 * 100)

void CNPC_MetroPolice::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_WAIT_FOR_MOVEMENT:
		BaseClass::RunTask( pTask );
		break;

	case TASK_METROPOLICE_WAIT_FOR_SENTENCE:
		{
			if ( FOkToMakeSound( pTask->flTaskData ) )
			{
				TaskComplete();
			}
		}
		break;

	case TASK_METROPOLICE_ACTIVATE_BATON:
		AutoMovement();
		
		if ( IsActivityFinished() )
		{
			TaskComplete();
		}	
		break;

	case TASK_METROPOLICE_LEAD_ARREST_ENEMY:
	case TASK_METROPOLICE_ARREST_ENEMY:
		{
			if ( !GetEnemy() )
			{
				TaskComplete();
				break;
			}

			if ( gpGlobals->curtime >= m_flTaskCompletionTime )
			{
				TaskComplete();
				break;
			}

			// Complete the arrest after the last squad member has a bead on the enemy
			// But only if you're the first guy who saw him
			if ( pTask->iTask == TASK_METROPOLICE_LEAD_ARREST_ENEMY )
			{
				int nArrestCount = SquadArrestCount();
				if ( nArrestCount == m_pSquad->NumMembers() )
				{
					TaskComplete();
					break;
				}

				// Do a distance check of the enemy from his initial position.
				// Shoot if he gets too far.
				if ( m_vSavePosition.DistToSqr( GetEnemy()->GetAbsOrigin() ) > FLEEING_DISTANCE_SQR )
				{
					SpeakSentence( METROPOLICE_SENTENCE_HES_RUNNING );
					EnemyResistingArrest();
					break;
				}
			}

			// Keep aiming at the enemy
			if ( GetEnemy() && FacingIdeal() )
			{
				float flNewIdealYaw = CalcIdealYaw( GetEnemy()->EyePosition() );
				if ( fabs(UTIL_AngleDiff( GetMotor()->GetIdealYaw(), flNewIdealYaw )) >= 45.0f )
				{
					GetMotor()->SetIdealYawToTarget( GetEnemy()->EyePosition() );
					SetTurnActivity(); 
				}
			}
			GetMotor()->UpdateYaw();
		}
		break;

	case TASK_METROPOLICE_GET_PATH_TO_BESTSOUND_LOS:
		{
			switch( GetTaskInterrupt() )
			{
			case 0:
				{
					CSound *pSound = GetBestSound();
					if (!pSound)
					{
						TaskFail(FAIL_NO_SOUND);
					}
					else
					{
						float flMaxRange = 2000;
						float flMinRange = 0;
						if ( GetActiveWeapon() )
						{
							flMaxRange = max( GetActiveWeapon()->m_fMaxRange1, GetActiveWeapon()->m_fMaxRange2 );
							flMinRange = min( GetActiveWeapon()->m_fMinRange1, GetActiveWeapon()->m_fMinRange2 );
						}

						// Check against NPC's max range
						if (flMaxRange > m_flDistTooFar)
						{
							flMaxRange = m_flDistTooFar;
						}

						// Why not doing lateral LOS first?

						Vector losTarget = pSound->GetSoundReactOrigin();
						if ( GetTacticalServices()->FindLos( pSound->GetSoundReactOrigin(), losTarget, flMinRange, flMaxRange, 1.0, &m_vInterruptSavePosition ) )
						{
							TaskInterrupt();
						}
						else
						{
							TaskFail(FAIL_NO_SHOOT);
						}
					}
				}
				break;

			case 1:
				{
					AI_NavGoal_t goal( m_vInterruptSavePosition, ACT_RUN, AIN_HULL_TOLERANCE );
					GetNavigator()->SetGoal( goal );
				}
				break;
			}
		}
		break;

	default:
		BaseClass::RunTask( pTask );
		break;
	}
}


		
//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
//-----------------------------------------------------------------------------
#define METROPOLICE_SCORCH_RATE				8
#define METROPOLICE_SCORCH_MIN_RENDERCOLOR	50
int CNPC_MetroPolice::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	CTakeDamageInfo newInfo = info;

	if ( HasSpawnFlags( SF_METROPOLICE_ARREST_ENEMY ) )
	{
		EnemyResistingArrest();
	}

	if( info.GetDamageType() & DMG_BURN )	//&& (info.GetDamageType() & DMG_DIRECT)
	{
		Scorch( METROPOLICE_SCORCH_RATE, METROPOLICE_SCORCH_MIN_RENDERCOLOR );
	}

	if (info.GetAttacker() == GetEnemy())
	{
		// Keep track of recent damage by my attacker. If we're under heavy fire,
		// consider running off and hiding.
		m_nRecentDamage += info.GetDamage();
		m_flRecentDamageTime = gpGlobals->curtime;
	}

	return BaseClass::OnTakeDamage_Alive( newInfo ); 
}

void CNPC_MetroPolice::Event_Killed( const CTakeDamageInfo &info )
{
	// Release the manhack if we're in the middle of deploying him
	if ( m_hManhack && m_hManhack->IsAlive() )
	{
		ReleaseManhack();
		m_hManhack = NULL;
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
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: I want to deploy a manhack. Can I?
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::CanDeployManhack( void )
{
	if ( HasSpawnFlags( SF_METROPOLICE_NO_MANHACK_DEPLOY ) )
		return false;

	// Nope, don't have any!
	if( m_iManhacks < 1 )
		return false;

	// Nope, already have one out.
	if( m_hManhack != NULL )
		return false;

	return true;
}
 
//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	if ( PlayerIsCriminal() == false )
	{
		SetCustomInterruptCondition( COND_METROPOLICE_PHYSOBJECT_ASSAULT );
	}

	//FIXME: Always interrupt for now
	if ( !IsInAScript() && 
		!IsCurSchedule( SCHED_METROPOLICE_SHOVE ) && 
		!IsCurSchedule( SCHED_MELEE_ATTACK1 ) && 
		!IsCurSchedule( SCHED_RELOAD ) && 
		!IsCurSchedule( SCHED_METROPOLICE_ACTIVATE_BATON ) && 
		!IsCurSchedule( SCHED_METROPOLICE_DRAW_PISTOL )	)
	{
		SetCustomInterruptCondition( COND_METROPOLICE_PLAYER_TOO_CLOSE );
	}

	if ( !IsCurSchedule( SCHED_METROPOLICE_BURNING_RUN ) && !IsCurSchedule( SCHED_METROPOLICE_BURNING_STAND ) && !IsMoving() )
	{
		SetCustomInterruptCondition( COND_ON_FIRE );
	}

	if ( !IsCurSchedule( SCHED_CHASE_ENEMY ) &&
		 !IsCurSchedule( SCHED_METROPOLICE_ACTIVATE_BATON ) &&
		 !IsCurSchedule( SCHED_METROPOLICE_DEACTIVATE_BATON ) &&
		 !IsCurSchedule( SCHED_METROPOLICE_SHOVE ) && 
		 !IsCurSchedule( SCHED_METROPOLICE_RETURN_TO_PRECHASE ) )
	{
		SetCustomInterruptCondition( COND_METROPOLICE_CHANGE_BATON_STATE );
	}

	if ( IsCurSchedule( SCHED_MELEE_ATTACK1 ) )
	{
		if ( gpGlobals->curtime - m_flLastDamageFlinchTime < 10.0 )
		{
			ClearCustomInterruptCondition( COND_LIGHT_DAMAGE );
			ClearCustomInterruptCondition( COND_HEAVY_DAMAGE );
		}
	}
	else if ( HasBaton() && IsCurSchedule( SCHED_COMBAT_FACE ) && !m_BatonSwingTimer.Expired() )
	{
		ClearCustomInterruptCondition( COND_CAN_MELEE_ATTACK1 );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_MetroPolice::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	if( FClassnameIs( pWeapon, "weapon_pistol" ) )
	{
		if( m_bSimpleCops )
		{
			return WEAPON_PROFICIENCY_POOR;
		}
		else
		{
			return WEAPON_PROFICIENCY_AVERAGE;
		}
	}
	else if( FClassnameIs( pWeapon, "weapon_smg1" ) )
	{
		// Better than rebels but not by much
		if( m_bSimpleCops )
		{
			return WEAPON_PROFICIENCY_POOR;
		}
		else
		{
			return WEAPON_PROFICIENCY_AVERAGE;
		}
	}
	else if( FClassnameIs( pWeapon, "weapon_shotgun" ) )
	{
		return WEAPON_PROFICIENCY_GOOD;
	}

	return BaseClass::CalcWeaponProficiency( pWeapon );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::GatherConditions( void )
{
	BaseClass::GatherConditions();

	if ( m_bPlayerTooClose == false )
	{
		ClearCondition( COND_METROPOLICE_PLAYER_TOO_CLOSE );
	}

	CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
	
	// FIXME: Player can be NULL here during level transitions.
	if ( !pPlayer )
		return;

	float distToPlayerSqr = ( pPlayer->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
	
	// See if we're too close
	if ( pPlayer->GetGroundEntity() == this )
	{
		// Always beat a player on our head
		m_iNumPlayerHits = 0;
		SetCondition( COND_METROPOLICE_PLAYER_TOO_CLOSE );
	}
	else if ( (distToPlayerSqr < (42.0f*42.0f) && FVisible(pPlayer)) )
	{
		// Ignore the player if we've been beating him, but not if we haven't moved
		if ( m_iNumPlayerHits < 3 || m_vecPreChaseOrigin == vec3_origin )
		{
			SetCondition( COND_METROPOLICE_PLAYER_TOO_CLOSE );
		}
	}
	else
	{
		ClearCondition( COND_METROPOLICE_PLAYER_TOO_CLOSE );

		// Don't clear out the player hit count for a few seconds after we last hit him
		// This avoids states where two metropolice have the player pinned between them.
		if ( (gpGlobals->curtime - GetLastAttackTime()) > 3 )
		{
			m_iNumPlayerHits = 0;
		}

		m_bPlayerTooClose = false;
	}

	if( metropolice_move_and_melee.GetBool() )
	{
		if( IsMoving() && HasCondition(COND_CAN_MELEE_ATTACK1) && HasBaton() )
		{
			if ( m_BatonSwingTimer.Expired() )
			{
				m_BatonSwingTimer.Set( 1.0, 1.75 );

				Activity activity = TranslateActivity( ACT_MELEE_ATTACK_SWING_GESTURE );
				Assert( activity != ACT_INVALID );
				AddGesture( activity );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::HasBaton( void )
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();

	if ( pWeapon )
		return FClassnameIs( pWeapon, "weapon_stunstick" );

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::BatonActive( void )
{
#ifndef HL2MP

	CWeaponStunStick *pStick = dynamic_cast<CWeaponStunStick *>(GetActiveWeapon());

	if ( pStick )
		return pStick->GetStunState();
#endif

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::SetBatonState( bool state )
{
	if ( !HasBaton() )
		return;

	if ( m_bShouldActivateBaton != state )
	{
		m_bShouldActivateBaton = state;
		SetCondition( COND_METROPOLICE_CHANGE_BATON_STATE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSound - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_MetroPolice::QueryHearSound( CSound *pSound )
{
	// Only behave differently if the player is pre-criminal
	if ( PlayerIsCriminal() == false )
	{
		// If the person making the sound was a friend, don't respond
		if ( pSound->IsSoundType( SOUND_DANGER ) && pSound->m_hOwner && IRelationType( pSound->m_hOwner ) == D_NU )
			return false;
	}

	return BaseClass::QueryHearSound( pSound );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//			*pEvent - 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	BaseClass::VPhysicsCollision( index, pEvent );

	int otherIndex = !index;
	
	CBaseEntity *pHitEntity = pEvent->pEntities[otherIndex];

	if ( pEvent->pObjects[otherIndex]->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
	{
		CHL2_Player *pPlayer = dynamic_cast<CHL2_Player *>(UTIL_PlayerByIndex( 1 ));

		// See if it's being held by the player
		if ( pPlayer != NULL && pPlayer->IsHoldingEntity( pHitEntity ) )
		{
			//TODO: Play an angry sentence, "Get that outta here!"

			if ( IsCurSchedule( SCHED_METROPOLICE_SHOVE ) == false )
			{
				SetCondition( COND_METROPOLICE_PLAYER_TOO_CLOSE );
				m_bPlayerTooClose = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTarget - 
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::StunnedTarget( CBaseEntity *pTarget )
{
	SetLastAttackTime( gpGlobals->curtime );

	if ( pTarget && pTarget->IsPlayer() )
	{
		m_OnStunnedPlayer.FireOutput( this, this );
		m_iNumPlayerHits++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Use response for when the player is pre-criminal
//-----------------------------------------------------------------------------
void CNPC_MetroPolice::PrecriminalUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( IsInAScript() )
		return;
	// Don't respond if I'm busy hating the player
	if ( IRelationType( pActivator ) == D_HT || ((GetState() != NPC_STATE_ALERT) && (GetState() != NPC_STATE_IDLE)) )
		return;
	if ( PlayerIsCriminal() )
		return;

	// Treat it like the player's bothered the cop
	IncrementPlayerCriminalStatus();

	// If we've hit max warnings, and we're allowed to chase, go for it
	if ( m_nNumWarnings == METROPOLICE_MAX_WARNINGS )
	{
		AdministerJustice();
	}
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_metropolice, CNPC_MetroPolice )

	gm_flTimeLastSpokePeek = 0;

	DECLARE_ANIMEVENT( AE_METROPOLICE_BATON_ON );
	DECLARE_ANIMEVENT( AE_METROPOLICE_BATON_OFF );
	DECLARE_ANIMEVENT( AE_METROPOLICE_SHOVE );
	DECLARE_ANIMEVENT( AE_METROPOLICE_START_DEPLOY );
	DECLARE_ANIMEVENT( AE_METROPOLICE_DRAW_PISTOL );
	DECLARE_ANIMEVENT( AE_METROPOLICE_DEPLOY_MANHACK );

	DECLARE_SQUADSLOT( SQUAD_SLOT_POLICE_CHARGE_ENEMY );
	DECLARE_SQUADSLOT( SQUAD_SLOT_POLICE_HARASS );
	DECLARE_SQUADSLOT( SQUAD_SLOT_POLICE_DEPLOY_MANHACK );
	DECLARE_SQUADSLOT( SQUAD_SLOT_POLICE_ATTACK_OCCLUDER1 );
	DECLARE_SQUADSLOT( SQUAD_SLOT_POLICE_ATTACK_OCCLUDER2 );
	DECLARE_SQUADSLOT( SQUAD_SLOT_POLICE_ARREST_ENEMY );

	DECLARE_ACTIVITY( ACT_METROPOLICE_DRAW_PISTOL );
	DECLARE_ACTIVITY( ACT_METROPOLICE_DEPLOY_MANHACK );
	DECLARE_ACTIVITY( ACT_METROPOLICE_FLINCH_BEHIND );
	DECLARE_ACTIVITY( ACT_METROPOLICE_POINT );
	DECLARE_ACTIVITY( ACT_PUSH_PLAYER );
	DECLARE_ACTIVITY( ACT_MELEE_ATTACK_THRUST );
	DECLARE_ACTIVITY( ACT_ACTIVATE_BATON );
	DECLARE_ACTIVITY( ACT_DEACTIVATE_BATON );
	DECLARE_ACTIVITY( ACT_WALK_BATON );
	DECLARE_ACTIVITY( ACT_IDLE_ANGRY_BATON );

	DECLARE_INTERACTION( g_interactionMetrocopIdleChatter );	
	DECLARE_INTERACTION( g_interactionMetrocopClearSentenceQueues );

	DECLARE_TASK( TASK_METROPOLICE_HARASS );
	DECLARE_TASK( TASK_METROPOLICE_DIE_INSTANTLY );
	DECLARE_TASK( TASK_METROPOLICE_RESET_LEDGE_CHECK_TIME );
	DECLARE_TASK( TASK_METROPOLICE_GET_PATH_TO_BESTSOUND_LOS );
	DECLARE_TASK( TASK_METROPOLICE_ARREST_ENEMY );
	DECLARE_TASK( TASK_METROPOLICE_LEAD_ARREST_ENEMY );
	DECLARE_TASK( TASK_METROPOLICE_SIGNAL_FIRING_TIME );
	DECLARE_TASK( TASK_METROPOLICE_ACTIVATE_BATON );
	DECLARE_TASK( TASK_METROPOLICE_WAIT_FOR_SENTENCE );
	DECLARE_TASK( TASK_METROPOLICE_GET_PATH_TO_PRECHASE );
	DECLARE_TASK( TASK_METROPOLICE_CLEAR_PRECHASE );

//	DECLARE_CONDITION( COND_METROPOLICE_ON_FIRE );
	DECLARE_CONDITION( COND_METROPOLICE_ENEMY_RESISTING_ARREST );
//	DECLARE_CONDITION( COND_METROPOLICE_START_POLICING );
	DECLARE_CONDITION( COND_METROPOLICE_PLAYER_TOO_CLOSE );
	DECLARE_CONDITION( COND_METROPOLICE_CHANGE_BATON_STATE );
	DECLARE_CONDITION( COND_METROPOLICE_PHYSOBJECT_ASSAULT );


//=========================================================
// > SCHED_METROPOLICE_WAKE_ANGRY
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_WAKE_ANGRY,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
	"		TASK_FACE_ENEMY					0"
	"	"
	"	Interrupts"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_INVESTIGATE_SOUND,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_STORE_LASTPOSITION			0"
	"		TASK_METROPOLICE_GET_PATH_TO_BESTSOUND_LOS		0"
	"		TASK_FACE_IDEAL					0"
	"		TASK_WAIT						1"
	"		TASK_SET_TOLERANCE_DISTANCE		32"
	"		TASK_WALK_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_STOP_MOVING				0"
	"		TASK_WAIT						5"
	"		TASK_GET_PATH_TO_LASTPOSITION	0"
	"		TASK_WALK_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_STOP_MOVING				0"
	"		TASK_CLEAR_LASTPOSITION			0"
	"		TASK_FACE_REASONABLE			0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_SEE_FEAR"
	"		COND_SEE_ENEMY"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_HEAR_PLAYER"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_MOVE_AWAY"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_HARASS,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_FACE_ENEMY					0"
	"		TASK_METROPOLICE_HARASS			0"
	"		TASK_PLAY_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_METROPOLICE_POINT"
	"		TASK_WAIT						0.5"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_HEAVY_DAMAGE"
	"		COND_NO_PRIMARY_AMMO"
	"		COND_WEAPON_BLOCKED_BY_FRIEND"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_MOVE_AWAY"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_DRAW_PISTOL,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_PLAY_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_METROPOLICE_DRAW_PISTOL"
	"		TASK_WAIT_FACE_ENEMY			0.1"
	""
	"	Interrupts"
);

//=========================================================
// > Attack Schedules
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_CHASE_ENEMY,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_METROPOLICE_ESTABLISH_LINE_OF_FIRE"
	"		TASK_SET_TOLERANCE_DISTANCE		24"
	"		TASK_GET_CHASE_PATH_TO_ENEMY	300"
	"		TASK_SPEAK_SENTENCE				6"	// METROPOLICE_SENTENCE_MOVE_INTO_POSITION
	"		TASK_RUN_PATH					0"
	"		TASK_METROPOLICE_RESET_LEDGE_CHECK_TIME 0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_FACE_ENEMY					0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_HEAVY_DAMAGE"
	"		COND_ENEMY_UNREACHABLE"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_RANGE_ATTACK2"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_TOO_CLOSE_TO_ATTACK"
	"		COND_TASK_FAILED"
	"		COND_LOST_ENEMY"
	"		COND_BETTER_WEAPON_AVAILABLE"
	"		COND_HEAR_DANGER"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_ESTABLISH_LINE_OF_FIRE,

	"	Tasks "
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAIL_ESTABLISH_LINE_OF_FIRE"
	"		TASK_STOP_MOVING				0"
	"		TASK_WAIT_FACE_ENEMY			0.2"
//	"		TASK_SET_TOLERANCE_DISTANCE		48"
	"		TASK_GET_PATH_TO_ENEMY_LOS		0"
	"		TASK_SPEAK_SENTENCE				6"	// METROPOLICE_SENTENCE_MOVE_INTO_POSITION
	"		TASK_RUN_PATH					0"
	"		TASK_METROPOLICE_RESET_LEDGE_CHECK_TIME 0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_HEAVY_DAMAGE"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_CAN_RANGE_ATTACK2"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_MOVE_AWAY"
	"		COND_NO_PRIMARY_AMMO"
	"		COND_METROPOLICE_PLAYER_TOO_CLOSE"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_TAKE_COVER,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAIL_TAKE_COVER"
	"		TASK_STOP_MOVING				0"
	"		TASK_WAIT						0.2"
	"		TASK_FIND_COVER_FROM_ENEMY		0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_REMEMBER					MEMORY:INCOVER"
	"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
	""
	"	Interrupts"
//	"		COND_NEW_ENEMY"
	"		COND_HEAVY_DAMAGE"
	"		COND_HEAR_DANGER"
);
 
DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_COMBAT_FACE,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
	"		TASK_FACE_ENEMY				0"
	"		TASK_WAIT					0.3"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_HEAVY_DAMAGE"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_CAN_RANGE_ATTACK2"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_HEAR_DANGER"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_DEPLOY_MANHACK,

	"	Tasks"
	"		TASK_SPEAK_SENTENCE					5"	// METROPOLICE_SENTENCE_DEPLOY_MANHACK
	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_METROPOLICE_DEPLOY_MANHACK"
	""
	"	Interrupts"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_ADVANCE,

	"	Tasks"
	"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE_ANGRY"
	"		TASK_WAIT_FACE_ENEMY				0.2" // give the guy some time to come out on his own
	"		TASK_GET_PATH_TO_ENEMY_LKP_LOS		0"
	"		TASK_RUN_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_COMBAT_FACE"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_HEAVY_DAMAGE"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_MOVE_AWAY"
	"		COND_METROPOLICE_PLAYER_TOO_CLOSE"
);

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_CHARGE,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_METROPOLICE_ADVANCE"
//	"		TASK_SET_TOLERANCE_DISTANCE		24"
	"		TASK_STORE_LASTPOSITION			0"
	"		TASK_GET_CHASE_PATH_TO_ENEMY	300"
	"		TASK_RUN_PATH_FOR_UNITS			150"
	"		TASK_STOP_MOVING				1"
	"		TASK_FACE_ENEMY					0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_HEAVY_DAMAGE"
	"		COND_LOST_ENEMY"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_HEAR_DANGER"
	"		COND_METROPOLICE_PLAYER_TOO_CLOSE"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_RANGE_ATTACK1,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
	"		TASK_RANGE_ATTACK1		0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_NO_PRIMARY_AMMO"
	"		COND_WEAPON_BLOCKED_BY_FRIEND"
	"		COND_TOO_CLOSE_TO_ATTACK"
	"		COND_GIVE_WAY"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_MOVE_AWAY"
	""
);

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_SMG_NORMAL_ATTACK,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
	"		TASK_RANGE_ATTACK1		0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_ENEMY_OCCLUDED"
	"		COND_NO_PRIMARY_AMMO"
	"		COND_HEAR_DANGER"
	"		COND_WEAPON_BLOCKED_BY_FRIEND"
	""
);

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_SMG_BURST_ATTACK,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
	"		TASK_WAIT				0.5"
	"		TASK_RANGE_ATTACK1		0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_NO_PRIMARY_AMMO"
	"		COND_HEAR_DANGER"
	"		COND_WEAPON_BLOCKED_BY_FRIEND"
	""
);

//=========================================================
// Do you smell pork?
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_BURNING_RUN,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_METROPOLICE_BURNING_STAND"
	"		TASK_SET_TOLERANCE_DISTANCE		24"
	"		TASK_GET_PATH_TO_RANDOM_NODE	300"
	"		TASK_RUN_PATH_TIMED				4"	
	"		TASK_METROPOLICE_DIE_INSTANTLY	DMG_BURN"
	""
	"	Interrupts"
);

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_BURNING_STAND,

	"	Tasks"
	"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE_ON_FIRE"
	"		TASK_WAIT						2.5"
	"		TASK_METROPOLICE_DIE_INSTANTLY	DMG_BURN"
	"		TASK_WAIT						1.0"
	""
	"	Interrupts"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_RETURN_TO_PRECHASE,

	"	Tasks"
	"		TASK_WAIT_RANDOM						1"
	"		TASK_METROPOLICE_GET_PATH_TO_PRECHASE	0"
	"		TASK_WALK_PATH							0"
	"		TASK_WAIT_FOR_MOVEMENT					0"
	"		TASK_STOP_MOVING						0"
	"		TASK_METROPOLICE_CLEAR_PRECHASE			0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_TASK_FAILED"
	"		COND_LOST_ENEMY"
	"		COND_HEAR_DANGER"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_ALERT_FACE_BESTSOUND,

	"	Tasks"
	"		TASK_SPEAK_SENTENCE		7"	// METROPOLICE_SENTENCE_HEARD_SOMETHING
	"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_ALERT_FACE_BESTSOUND"
	""
	"	Interrupts"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_ENEMY_RESISTING_ARREST,

	"	Tasks"
	"		TASK_METROPOLICE_SIGNAL_FIRING_TIME		0"
	""
	"	Interrupts"
);

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_WARN_AND_ARREST_ENEMY,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_METROPOLICE_ENEMY_RESISTING_ARREST"
	"		TASK_STOP_MOVING					0"
	"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_IDLE_ANGRY"
	"		TASK_SPEAK_SENTENCE					0"	// "Freeze!"
	"		TASK_METROPOLICE_ARREST_ENEMY		0.5"
	"		TASK_STORE_ENEMY_POSITION_IN_SAVEPOSITION	0"
	"		TASK_METROPOLICE_ARREST_ENEMY		1"
	"		TASK_METROPOLICE_WAIT_FOR_SENTENCE	1"
	"		TASK_SPEAK_SENTENCE					1"	// "He's over here!"
	"		TASK_METROPOLICE_LEAD_ARREST_ENEMY	5"
	"		TASK_METROPOLICE_ARREST_ENEMY		2"
	"		TASK_METROPOLICE_WAIT_FOR_SENTENCE	1"
	"		TASK_SPEAK_SENTENCE					3"	// "Take him down!"
	"		TASK_METROPOLICE_ARREST_ENEMY		1.5"
	"		TASK_METROPOLICE_WAIT_FOR_SENTENCE	2"
	"		TASK_METROPOLICE_SIGNAL_FIRING_TIME	0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_HEAR_DANGER"
	"		COND_ENEMY_DEAD"
	"		COND_METROPOLICE_ENEMY_RESISTING_ARREST"
);

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_ARREST_ENEMY,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_METROPOLICE_ENEMY_RESISTING_ARREST"
	"		TASK_GET_PATH_TO_ENEMY_LOS			0"
	"		TASK_RUN_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	"		TASK_STOP_MOVING					0"
	"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_IDLE_ANGRY"
	"		TASK_METROPOLICE_WAIT_FOR_SENTENCE	0"
	"		TASK_SPEAK_SENTENCE					4"
	"		TASK_METROPOLICE_ARREST_ENEMY		30"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_HEAR_DANGER"
	"		COND_ENEMY_DEAD"
	"		COND_METROPOLICE_ENEMY_RESISTING_ARREST"
	"		COND_WEAPON_BLOCKED_BY_FRIEND"
	"		COND_WEAPON_SIGHT_OCCLUDED"
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_VICTORY_DANCE,

	"	Tasks"
	"		TASK_FACE_ENEMY			0"
	"		TASK_STOP_MOVING		0"
	"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_VICTORY_DANCE"
	"		TASK_WAIT				0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_ENEMY_DEAD"
	"		COND_HEAR_DANGER"
	""
);
//===============================================

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_SHOVE,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_FACE_PLAYER				0.1"	//FIXME: This needs to be the target or enemy
	"		TASK_METROPOLICE_ACTIVATE_BATON	1"
	"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_PUSH_PLAYER"
	""
	"	Interrupts"
);

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_ACTIVATE_BATON,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_FACE_TARGET				0"
	"		TASK_METROPOLICE_ACTIVATE_BATON	1"
	""
	"	Interrupts"
);

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_DEACTIVATE_BATON,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_METROPOLICE_ACTIVATE_BATON	0"
	""
	"	Interrupts"
);

DEFINE_SCHEDULE
(
	SCHED_METROPOLICE_SMASH_PROP,

	"	Tasks"
	"		TASK_GET_PATH_TO_TARGET		0"
	"		TASK_MOVE_TO_TARGET_RANGE	50"
	"		TASK_STOP_MOVING			0"
	"		TASK_FACE_TARGET			0"
	"		TASK_ANNOUNCE_ATTACK		1"	// 1 = primary attack
	"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_MELEE_ATTACK1"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
);

AI_END_CUSTOM_NPC()

