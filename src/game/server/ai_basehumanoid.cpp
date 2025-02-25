//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Humanoid baseclass - this doesnt contain any proper ai, all that still
// needs to be defined in the subclass - this is for utility functions and very basic
// states applicable to ALL human npcs like falling/jumping, heavydamage, gibs, sneakattacks, etc.
//
// TODO; Limb gibbing
//=============================================================================//

#include "cbase.h"

#include "activitylist.h"
#include "npcevent.h"
#include "ai_basehumanoid.h"
#include "ai_blended_movement.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "movevars_shared.h"
#include "BasePropDoor.h"

#ifdef HL2_DLL
#include "ai_interactions.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar temp_demofixes;	//TEMPTEMP
ConVar ai_limited_ammo( "ai_limited_ammo", "0", FCVAR_NONE, "Do NPCs that use weapons have limited ammo?" );
ConVar ai_stun_duration( "ai_stun_duration","5");
ConVar ai_stun_duration_max( "ai_stun_duration_max","8");

BEGIN_DATADESC( CAI_BaseHumanoid )
	DEFINE_KEYFIELD( m_iClips, FIELD_INTEGER, "ammo" ),
	DEFINE_KEYFIELD( m_spawnEquipmentSecondary, FIELD_STRING, "secondaryequipment" ),
	DEFINE_FIELD( m_bHeadGibbed, FIELD_BOOLEAN ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CAI_BaseHumanoid, DT_BaseHumanoid )
	// Send it to the client, that part takes care of the bone shrinkage
	SendPropBool( SENDINFO( m_bHeadGibbed ) ),
END_SEND_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CAI_BaseHumanoid::Precache( void )
{
	if ( m_spawnEquipmentSecondary != NULL_STRING && strcmp(STRING(m_spawnEquipmentSecondary), "0") )
	{
		UTIL_PrecacheOther( STRING(m_spawnEquipmentSecondary) );
	}

	BaseClass::Precache();
}

void CAI_BaseHumanoid::Spawn( void )
{
	BaseClass::Spawn();

	CalculateClips();
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CAI_BaseHumanoid::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
#ifdef HL2_DLL
	if ( interactionType == g_interactionBarnacleVictimDangle )
	{
		// Force choosing of a new schedule
		ClearSchedule( "Grabbed by a barnacle" );
		return true;
	}
	else if ( interactionType == g_interactionBarnacleVictimReleased )
	{
		// Destroy the entity, the barnacle is going to use the ragdoll that it is releasing
		// as the corpse.
		UTIL_Remove( this );
		return true;
	}
#endif
	return BaseClass::HandleInteraction( interactionType, data, sourceEnt);
}

void CAI_BaseHumanoid::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	// EVENT_WEAPON_RELOAD is the old way of doing it
	// a EVENT_WEAPON_RELOAD_SOUND + EVENT_WEAPON_RELOAD_FILL_CLIP is the new way of doing it
	case EVENT_WEAPON_RELOAD:
	case EVENT_WEAPON_RELOAD_FILL_CLIP:
		// Remove a clip
		if (GetActiveWeapon() && m_iClips > 0)
			m_iClips -= 1;
		break;
	}

	// Baseclass handles the rest
	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: check ammo
//-----------------------------------------------------------------------------
void CAI_BaseHumanoid::CheckAmmo( void )
{
	BaseClass::CheckAmmo();

	// FIXME: why isn't this a baseclass function? NPCS with inate weapons could easily overwrite it
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if (!pWeapon)
		return;

	// Don't do this while holstering / unholstering
	if ( IsWeaponStateChanging() )
		return;

	if (pWeapon->UsesPrimaryAmmo())
	{
		// Set the conditions
		if (!pWeapon->HasPrimaryAmmo() )
		{
			SetCondition(COND_NO_PRIMARY_AMMO);
		}
		else if (pWeapon->UsesClipsForAmmo1() && pWeapon->Clip1() < (pWeapon->GetMaxClip1() / 4 + 1))
		{
			SetCondition(COND_LOW_PRIMARY_AMMO);
		}
	}

	if (!pWeapon->HasSecondaryAmmo() )
	{
		if ( pWeapon->UsesClipsForAmmo2() )
		{
			SetCondition(COND_NO_SECONDARY_AMMO);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseHumanoid::CanReload( void )
{
	if ( !GetActiveWeapon() || HasCondition( COND_NO_WEAPON ) )
		return false;

	if ( ai_limited_ammo.GetBool() )
	{
		if ( GetActiveWeapon()->UsesClipsForAmmo1() && m_iClips == 0 )
			return false;
	}

	if ( IsRunningDynamicInteraction() )
		return false;

	if ( IsWeaponStateChanging() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: check ammo
//-----------------------------------------------------------------------------
void CAI_BaseHumanoid::CalculateClips( void )
{
	// If our clips are negative, then mapmaker wants us to have no limit
	if ( m_iClips < 0 )
		m_iClips = 900001;	//WOW

	// Otherwise default to lucky number 7
	if ( m_iClips == 0 )
		m_iClips = 7;
}

//-----------------------------------------------------------------------------
// TASK_RANGE_ATTACK1
//-----------------------------------------------------------------------------
void CAI_BaseHumanoid::BuildScheduleTestBits( )
{
	BaseClass::BuildScheduleTestBits();

	if ( CapabilitiesGet() & bits_CAP_USE_SHOT_REGULATOR )
	{
		if ( GetShotRegulator()->IsInRestInterval() )
		{
			ClearCustomInterruptCondition( COND_CAN_RANGE_ATTACK1 );
		}
	}
}

//------------------------------------------------------------------------------
bool CAI_BaseHumanoid::ShouldMoveAndShoot( void )
{
	if ( IsOnFire() )
		return false;

	if( HasCondition( COND_TOO_FAR_TO_ATTACK ) )	//|| HasCondition( COND_NO_PRIMARY_AMMO )
		return false;

#ifdef HL2_DLL
	if( GetEnemy() && g_pGameRules->IsSkillLevel( SKILL_EASY ) && HasCondition( COND_BEHIND_ENEMY ) )
		return false;
#endif

	return BaseClass::ShouldMoveAndShoot();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CAI_BaseHumanoid::GetIdealAccel()
{
	// Normal accel based on animation
	float fAccel = sqrt(sv_accelerate.GetFloat() * GetIdealSpeed()) * sv_accelerate.GetFloat();

	// Change accel rate based on my activity
	// TODO; This should be offloaded somehow
	switch( GetActivity() )
	{
	case ACT_IDLE_HURT:
	case ACT_IDLE_PRONE:
	case ACT_CROUCHIDLE:
	case ACT_COVER_LOW:
	case ACT_COVER_MED:
		fAccel *= 0.80;
		break;

	case ACT_IDLE_ANGRY:
	case ACT_IDLE_AGITATED:
	case ACT_IDLE_STIMULATED:
	case ACT_IDLE_ON_FIRE:
	case ACT_JUMP:
	case ACT_GLIDE:
		fAccel *= 1.20;
		break;

	case ACT_SWIM:
	case ACT_IDLE_STEALTH:
	case ACT_IDLE_RELAXED:
	case ACT_IDLE_SCARED:
		fAccel *= 0.60;
		break;

		default:
		break;
	}

	// Return a rounded number
	return fAccel;	//Ceil2Int(fAccel)
}

//-----------------------------------------------------------------------------
float CAI_BaseHumanoid::GetIdealSpeed()
{
	float fSpeed = m_flGroundSpeed;
	// Wading through water
	if ( GetWaterLevel() >= WL_Feet && GetActivity() != ACT_SWIM )
		return fSpeed *= 0.7;
	else
	{
		// Injured, but not using an injured animation
		if ( IsInjured() && !HaveSequenceForActivity( ACT_RUN_HURT ) ) // && temp_demofixes.GetBool()
			return fSpeed *= 0.8;

		// Reloading
		if ( GetActivity() == ACT_RELOAD || 
			IsPlayingGesture( ACT_GESTURE_RELOAD ) || 
			IsCurSchedule( SCHED_RELOAD, false ) )
		{
			return fSpeed *= 0.7;
		}

		// Multiply by the friction of the current surface im standing on.
		// HOWEVER! Clamp to a reasonable amount -MM
		return fSpeed * clamp( GetFriction(), 0.6, 1.5 );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static bool IsSmall( CBaseEntity *pBlocker )
{
	CCollisionProperty *pCollisionProp = pBlocker->CollisionProp();
	int  nSmaller = 0;
	Vector vecSize = pCollisionProp->OBBMaxs() - pCollisionProp->OBBMins();
	for ( int i = 0; i < 3; i++ )
	{
		if ( vecSize[i] >= 42 )
			return false;

		if ( vecSize[i] <= 30 )
		{
			nSmaller++;
		}
	}

	return ( nSmaller >= 2 );
}

bool CAI_BaseHumanoid::OnMoveBlocked( AIMoveResult_t *pResult )
{
	if ( *pResult != AIMR_BLOCKED_NPC && GetNavigator()->GetBlockingEntity() && !GetNavigator()->GetBlockingEntity()->IsNPC() )
	{
		CBaseEntity *pBlocker = GetNavigator()->GetBlockingEntity();

		float massBonus = ( IsNavigationUrgent() ) ? 40.0 : 0;

		if ( pBlocker->GetMoveType() == MOVETYPE_VPHYSICS && 
			 pBlocker != GetGroundEntity() && 
			 !pBlocker->IsNavIgnored() &&
			 !dynamic_cast<CBasePropDoor *>(pBlocker) &&
			 pBlocker->VPhysicsGetObject() && 
			 pBlocker->VPhysicsGetObject()->IsMoveable() && 
			 ( pBlocker->VPhysicsGetObject()->GetMass() <= 35.0 + massBonus + 0.1 || 
			   ( pBlocker->VPhysicsGetObject()->GetMass() <= 50.0 + massBonus + 0.1 && IsSmall( pBlocker ) ) ) )
		{
			DbgNavMsg1( this, "Setting ignore on object %s", pBlocker->GetDebugName() );
			pBlocker->SetNavIgnore( 2.5 );
		}
#if 0
		else
		{
			CPhysicsProp *pProp = dynamic_cast<CPhysicsProp*>( pBlocker );
			if ( pProp && pProp->GetHealth() && pProp->GetExplosiveDamage() == 0.0 && GetActiveWeapon() && !GetActiveWeapon()->ClassMatches( "weapon_rpg" ) )
			{
				Msg( "!\n" );
				// Destroy!
			}
		}
#endif
	}

	return BaseClass::OnMoveBlocked( pResult );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CAI_BaseHumanoid::ShouldPickADeathPose( void ) 
{
	if ( IsCrouching() )
		return false;

	// Allow unique death animations - deathposes are meant to overwrite the generic ones
	if ( GetDeathActivity() != ACT_DIESIMPLE )
		return false;

	return BaseClass::ShouldPickADeathPose();
}

bool CAI_BaseHumanoid::ShouldGib( const CTakeDamageInfo &info )
{
	if ( info.GetDamageType() & (DMG_NEVERGIB|DMG_DISSOLVE) )
		return false;

	if ( ( g_pGameRules->Damage_ShouldGibCorpse( info.GetDamageType() ) && m_iHealth < GIB_HEALTH_VALUE ) || ( info.GetDamageType() & DMG_ALWAYSGIB ) )
		 return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Sufficient fire damage has been done. Zombie ignites!
//-----------------------------------------------------------------------------
void CAI_BaseHumanoid::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	BaseClass::Ignite( flFlameLifetime, bNPCOnly, flSize, bCalledByLevelDesigner );

	if( !BehaviorSelectSchedule() )
	{
		Activity activity = GetActivity();
		Activity burningActivity = activity;

		if ( activity == ACT_WALK )
		{
			burningActivity = ACT_WALK_ON_FIRE;
		}
		else if ( activity == ACT_RUN )
		{
			burningActivity = ACT_RUN_ON_FIRE;
		}
		else if ( activity == ACT_IDLE )
		{
			burningActivity = ACT_IDLE_ON_FIRE;
		}

		if( HaveSequenceForActivity(burningActivity) )
		{
			// Make sure we have a sequence for this activity (torsos don't have any, for instance) 
			// to prevent the baseNPC & baseAnimating code from throwing red level errors.
			SetActivity( burningActivity );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ConVar	sk_backstab_distance( "sk_backstab_distance", "144.0" );	// 16 feet -- orig; 360.0f
ConVar	sk_backstab_multiplier( "sk_backstab_multiplier", "5.0" );
void CAI_BaseHumanoid::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	CTakeDamageInfo newInfo = info;
	bool bSneakAttacked = false;

#ifdef HL2_DLL
	// Airboat gun will impart major force if it's about to kill him....
	if ( info.GetDamageType() & DMG_AIRBOAT )
	{
		if ( info.GetDamage() >= GetHealth() )
		{
			float flMagnitude = newInfo.GetDamageForce().Length();
			if ( (flMagnitude != 0.0f) && (flMagnitude < 400.0f * 65.0f) )
			{
				newInfo.ScaleDamageForce( 400.0f * 65.0f / flMagnitude );
			}
		}
	}
#endif

	// Always drop if its a melee attack, regardless of hitgroup
	if( ptr->hitgroup == HITGROUP_HEAD || info.GetDamageType() & (DMG_SLASH|DMG_CLUB|DMG_SHOCK) )
	{
		if ( info.GetAttacker() && !IsInAScript() )
		{
			if ( info.GetAttacker()->IsPlayer() && info.GetAttacker() != GetEnemy() )
			{
				// Shot/Thwapped in the head by a player I've never seen. In this case the player 
				// has gotten the drop on this enemy and such an attack is always lethal (at close range)
				bSneakAttacked = true;

				AIEnemiesIter_t	iter;
				for( AI_EnemyInfo_t *pMemory = GetEnemies()->GetFirst(&iter); pMemory != NULL; pMemory = GetEnemies()->GetNext(&iter) )
				{
					if ( pMemory->hEnemy == info.GetAttacker() )
					{
						bSneakAttacked = false;
						break;
					}
				}

				float flDist;
				flDist = (info.GetAttacker()->GetAbsOrigin() - GetAbsOrigin()).Length();

				if( flDist > sk_backstab_distance.GetFloat() )
				{
					bSneakAttacked = false;
				}
			}

			if ((ShouldGib(info) || info.GetDamageType() & (DMG_BUCKSHOT|DMG_AIRBOAT) && m_iHealth <= (GIB_HEALTH_VALUE-5)))
			{
				// My head just got liquidated by a huge blow - gib it and kill me
				m_bHeadGibbed = true;
				Event_Killed( info );
			}
		}
	}

	if( bSneakAttacked )
	{
		newInfo.ScaleDamage( sk_backstab_multiplier.GetFloat() );
	}

	BaseClass::TraceAttack( newInfo, vecDir, ptr );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_BaseHumanoid::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CAI_BaseHumanoid::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// If I just got blasted and I should be knocked down, get KO'd!
	// NOTE; Whether or not a npc can get knocked down depends on that npcs selectsched,
	// If a bit of logic isnt in there, that npc wont get gonk'd
	if ( m_NPCState != NPC_STATE_SCRIPT && m_NPCState != NPC_STATE_PRONE && !m_bKnockedDown )
	{
		bool bKnockdownDamage = (info.GetDamage() >= (GetMaxHealth() / 2));
		if ( bKnockdownDamage || (info.GetDamageType() & (DMG_BLAST|DMG_ALWAYSGIB) && IsHeavyDamage( info )) )
		{
			// Get blasted!
			m_bKnockedDown = true;
			SetCondition( COND_STUNNED );
		}

		// !TODO; If the last bullet did so much damage i'm in the negatives
#if 0
		if ( info.GetDamageType() & (DMG_BUCKSHOT) && m_iHealth <= GIB_HEALTH_VALUE )
		{
			// I just got a limb blown off - send it flying
			Vector vecLimbForce;
			vecLimbForce.x = random->RandomFloat( -300, 300 );
			vecLimbForce.y = random->RandomFloat( -300, 300 );
			vecLimbForce.z = random->RandomFloat( 0, 250 );
			CBaseEntity *pGib = CreateRagGib( "models/gibs/hgibs.mdl", info.GetDamagePosition(), GetAbsAngles(), vecLimbForce );

			// don't collide with this thing ever
			if ( pGib )
			{
				pGib->SetOwnerEntity( this );
			}
		}
#endif
	}

	return BaseClass::OnTakeDamage_Alive( info );
}

//-----------------------------------------------------------------------------
// TASK_RANGE_ATTACK1
//-----------------------------------------------------------------------------
void CAI_BaseHumanoid::StartTaskRangeAttack1( const Task_t *pTask )
{
	if ( ( CapabilitiesGet() & bits_CAP_USE_SHOT_REGULATOR ) == 0 )
	{
		BaseClass::StartTask( pTask );
		return;
	}

	// Can't shoot if we're in the rest interval; fail the schedule
	if ( GetShotRegulator()->IsInRestInterval() )
	{
		TaskFail( "Shot regulator in rest interval" );
		return;
	}

	if ( GetShotRegulator()->ShouldShoot() )
	{
		OnRangeAttack1();
		ResetIdealActivity( ACT_RANGE_ATTACK1 );
	}
	else
	{
		// This can happen if we start while in the middle of a burst
		// which shouldn't happen, but given the chaotic nature of our AI system,
		// does occasionally happen.
		ResetIdealActivity( ACT_IDLE_ANGRY );
	}
}


//-----------------------------------------------------------------------------
// Starting Tasks
//-----------------------------------------------------------------------------
void CAI_BaseHumanoid::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		StartTaskRangeAttack1( pTask );
		break;

	default:
		BaseClass::StartTask( pTask );
	}
}

//-----------------------------------------------------------------------------
// Running Tasks
//-----------------------------------------------------------------------------
void CAI_BaseHumanoid::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		RunTaskRangeAttack1( pTask );
		break;

	default:
		BaseClass::RunTask( pTask );
	}
}

//-----------------------------------------------------------------------------
// TASK_RANGE_ATTACK1 / TASK_RANGE_ATTACK2 / etc.
//-----------------------------------------------------------------------------
void CAI_BaseHumanoid::RunTaskRangeAttack1( const Task_t *pTask )
{
	if ( ( CapabilitiesGet() & bits_CAP_USE_SHOT_REGULATOR ) == 0 )
	{
		BaseClass::RunTask( pTask );
		return;
	}

	AutoMovement( );

	Vector vecEnemyLKP = GetEnemyLKP();

	// If our enemy was killed, but I'm not done animating, the last known position comes
	// back as the origin and makes the me face the world origin if my attack schedule
	// doesn't break when my enemy dies. (sjb)
	if( vecEnemyLKP != vec3_origin )
	{
		if ( ( pTask->iTask == TASK_RANGE_ATTACK1 || pTask->iTask == TASK_RELOAD ) && 
			 ( CapabilitiesGet() & bits_CAP_AIM_GUN ) && 
			 FInAimCone( vecEnemyLKP ) )
		{
			// Arms will aim, so leave body yaw as is
			GetMotor()->SetIdealYawAndUpdate( GetMotor()->GetIdealYaw(), AI_KEEP_YAW_SPEED );
		}
		else
		{
			GetMotor()->SetIdealYawToTargetAndUpdate( vecEnemyLKP, AI_KEEP_YAW_SPEED );
		}
	}

	if ( IsActivityFinished() )
	{
		if ( !GetEnemy() || !GetEnemy()->IsAlive() )
		{
			TaskComplete();
			return;
		}

		if ( !GetShotRegulator()->IsInRestInterval() )
		{
			if ( GetShotRegulator()->ShouldShoot() )
			{
				OnRangeAttack1();
				ResetIdealActivity( ACT_RANGE_ATTACK1 );
			}
			return;
		}
		TaskComplete();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Activity CAI_BaseHumanoid::NPC_TranslateActivity( Activity NewActivity )
{
	switch ( NewActivity )
	{
		case ACT_RUN:
		case ACT_RUN_AIM:
			if ( IsOnFire() && HaveSequenceForActivity( ACT_RUN_ON_FIRE ) )
			{
				// flail around!
				return ACT_RUN_ON_FIRE;
			}
		break;

		case ACT_WALK:
		case ACT_WALK_AIM:
			if ( IsOnFire() && HaveSequenceForActivity( ACT_WALK_ON_FIRE ) )
			{
				// flail around!
				return ACT_WALK_ON_FIRE;
			}
		break;
	}

	return BaseClass::NPC_TranslateActivity( NewActivity );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CAI_BaseHumanoid::TranslateSchedule( int scheduleType ) 
{
	switch( scheduleType )
	{
	case SCHED_RELOAD:
			if ( !CanReload() )
			{
				// Check if this is because im completely out of ammo
				if ( GetActiveWeapon() && m_iClips == 0 )
				{
					// Drop my active weapon, and pull out my secondary
					GetActiveWeapon()->m_iClip1 = 0;
					GetActiveWeapon()->m_iClip2 = 0;
					return TranslateSchedule( SCHED_DROP_WEAPON );
				}
				// Otherwise fail
				return TranslateSchedule( SCHED_FAIL );
			}

			return SCHED_RELOAD;
	break;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

// MOVED TO BASENPC
#if 0
AI_BEGIN_CUSTOM_NPC( base_humanoid, CAI_BaseHumanoid )

	DECLARE_TASK( TASK_FIND_DODGE_POSITION )
	DECLARE_TASK( TASK_DODGE )

//=========================================================
// > SCHED_DUCK_DODGE
// Roll away from an enemy, or incoming danger
//=========================================================
DEFINE_SCHEDULE
(
SCHED_DODGE,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_FIND_DODGE_POSITION	0"
 "		TASK_DODGE					0"
 "		TASK_DEFER_DODGE			10"
 ""
 "	Interrupts"
 ""
)

AI_END_CUSTOM_NPC()
#endif