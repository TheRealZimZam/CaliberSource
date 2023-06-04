//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Lil' Firebee/Chinese Predator - Hosptials beware
//
//
// TODO; Code this thing!
// Note; "Pods" refer to disposable missile pods, like manhacks for cops
//=============================================================================//

//-----------------------------------------------------------------------------
// Heres a rundown - 
// Big ol' flying attack bot, thrice as large as the wscanner, twice of the
// cscanner, strafes enemies like a monoplane on the attack, hovers around when idle.
// Usually paried with a launcher to keep the player in the map.
// A good amount of health and high speed makes it a real threat when alone, 
// an absolute monster when paired with grunts or other deadly distractions.
// Has three attacks;
// 1. Shoot short-range lasers, its "melee" attack.
// 2. Launched homing grenades - they go up abit then come back down on
// the unfortunate basterds head.
// 3. If specified, can be given disposable missile pods for a longer range,
// direct-fire attack, rather than the slow, easily dodgable homer grenades
//
//-----------------------------------------------------------------------------

#include "cbase.h"
#include "npcevent.h"
#include "soundenvelope.h"
#include "ai_hint.h"
#include "ai_moveprobe.h"
#include "ai_squad.h"
#include "beam_shared.h"
#include "globalstate.h"
#include "soundent.h"
#include "npc_basescanner.h"
#include "gib.h"
#include "explode.h"
#include "spotlightend.h"
#include "IEffects.h"
#include "items.h"
#include "ai_route.h"
#include "player_pickup.h"
#include "hl2_player.h"
#include "grenade_homer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Spawnflags
//-----------------------------------------------------------------------------
#define	ASCANNER_NUM_BEAMS				4
#define	ASCANNER_NUM_GLOWS				3

#define	ASCANNER_MAX_SPEED			400
#define ASCANNER_MIN_GROUND_DIST	96
#define ASCANNER_HOVER_NEAR_DIST	400		//At this distance use laserbeam
#define ASCANNER_HOVER_FAR_DIST		750		//At this distance use grenades
#define	ASCANNER_BANK_RATE	35

#define	ASCANNER_MAX_ATTACK_RADIUS	5000	//Dont engage targets further than this, just observe
#define ASCANNER_TARGET_TIME  1			//How long an enemy must be our target before allowed to engage
#define ASCANNER_LASER_WARMUP_TIME  1	//Delay before laser fires
#define ASCANNER_LASER_BURST_SIZE  3
#define ASCANNER_ROCKET_WARMUP_TIME  2	//Delay before rocket pods can fire

//Scannerstate, used to control flight type
enum AScannerState_t
{
	ASCANNER_STATE_NORMAL,	//Normal idle/Combat
	ASCANNER_STATE_STRAFE,	//Aiming state - When firing grenades/rockets, go into this state for X seconds
	ASCANNER_STATE_EVASION,	//Dodging away, siren sounding, alert friends
};

class CSprite;
class CBeam;

//-----------------------------------------------------------------------------
// WScanner 
//-----------------------------------------------------------------------------
class CNPC_ADrone : public CNPC_BaseScanner
{
	DECLARE_CLASS( CNPC_ADrone, CNPC_BaseScanner );

public:
	CNPC_ADrone();

	Class_T			Classify(void);

	void			Event_Killed( const CTakeDamageInfo &info );
	int				OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual bool	ShouldGib( const CTakeDamageInfo &info );

	void			HandleAnimEvent( animevent_t *pEvent );

 	virtual int		SelectSchedule(void);
	virtual int		TranslateSchedule( int scheduleType );

	bool			FValidateHintType(CAI_Hint *pHint);

	virtual Vector	BodyTarget( const Vector &posSrc, bool bNoisy = true ) { return WorldSpaceCenter(); }

	void			SetAScannerState(AScannerState_t pNewState);
	Vector			PodPos(int nPos);
	void			DropPod(int nPod);
	void			StartAimBeam(void);

	bool			OverrideMove(float flInterval);
	bool 			OverridePathMove(float flInterval);
	
	void			MoveToTarget(float flInterval, const Vector &MoveTarget);
	void			MoveToAttack(float flInterval);
	Vector			MoveToEvade(CBaseCombatCharacter *pEnemy);
	void			MoveExecute_Alive(float flInterval);
	float			MinGroundDist(void);
	void			SetBanking(float flInterval);

	void			PlayAlarmSound(void);
	void			AdjustAntenna(float flInterval);
	virtual void	StopLoopingSounds(void);

	char *GetScannerSoundPrefix( void )
	{
		return "NPC_AScanner";
	}
	char			*GetEngineSound( void );

	void			Precache(void);
	void			RunTask( const Task_t *pTask );
	void			Spawn(void);
	void			StartTask( const Task_t *pTask );

	bool			IsValidEnemy(CBaseEntity *pEnemy);

	int				DrawDebugTextOverlays(void);
	void			OnScheduleChange();

	DEFINE_CUSTOM_AI;

	DECLARE_DATADESC();

	~CNPC_ADrone(void);

protected:
	int				m_nPods;
	AScannerState_t	m_nAScannerState;

	// ==================
	// Attack 
	// ==================
	CBeam*			m_pBeam[ASCANNER_NUM_BEAMS];
	CSprite*		m_pLaserGlow[ASCANNER_NUM_GLOWS];
	bool			m_bBankFace;
	float			m_fNextGrenadeTime;
	float			m_fNextLaserTime;
	void			LaserWarmup(void);
	void			LaserShoot(void);
	void			LaserKill(void);
	void			TossGrenade( bool bFireGrenade = false );
	float			m_fSwitchToHelpTime;

	// ==================
	// Movement variables.
	// ==================
	float			m_fNextMoveEvadeTime;
	float			m_fNextAlarmSoundTime;
	float			m_fNextFlySoundTime;
	float			m_fTargetTime;
	
	float			m_fAntennaPos;
	float			m_fAntennaTarget;

	CSprite*		m_pLightGlow;
};

ConVar	sk_ascanner_health( "sk_ascanner_health","0");
ConVar	sk_ascanner_missile_dmg( "sk_ascanner_missile_dmg","0");	//Long-range missile
ConVar	sk_ascanner_grenade_dmg( "sk_ascanner_grenade_dmg","0");	//Mid-range homer grenade
ConVar	sk_ascanner_laser_dmg( "sk_ascanner_laser_dmg","0");	//Close-range laser

//-----------------------------------------------------------------------------
// Private activities.
//-----------------------------------------------------------------------------
static int ACT_ASCANNER_DROP = 0;
static int ACT_ASCANNER_FLINCH_BACK = 0;
static int ACT_ASCANNER_FLINCH_FRONT = 0;
static int ACT_ASCANNER_FLINCH_LEFT = 0;
static int ACT_ASCANNER_FLINCH_RIGHT = 0;

//-----------------------------------------------------------------------------
// Interactions
//-----------------------------------------------------------------------------
int	g_interactionWScannerBomb		= 0;

//-----------------------------------------------------------------------------
// AScanner schedules.
//-----------------------------------------------------------------------------
enum WScannerSchedules
{
	SCHED_ASCANNER_ATTACK_HOVER = LAST_SHARED_SCHEDULE,
	SCHED_ASCANNER_PATROL,
	SCHED_ASCANNER_GET_HELP,
	SCHED_ASCANNER_CHASE_ENEMY,
	SCHED_ASCANNER_SHOOT_LASER,
};

//-----------------------------------------------------------------------------
// AScanner tasks.
//-----------------------------------------------------------------------------
enum AScannerTasks
{
	TASK_LASER_WARMUP	= LAST_SHARED_TASK,
	TASK_LASER_SHOOT,
	TASK_RAISE_ANTENNA,
	TASK_AIM_ROCKETS,
	TASK_FIRE_ROCKETS,
};

//-----------------------------------------------------------------------------
// Animation events
//------------------------------------------------------------------------
int AE_ASCANNER_SHOOT;

//-----------------------------------------------------------------------------
// Attachment points
//-----------------------------------------------------------------------------
#define ASCANNER_ATTACHMENT_LIGHT	"light"
#define ASCANNER_ATTACHMENT_SHOOT		1
#define ASCANNER_ATTACHMENT_POD_RIGHT	1
#define ASCANNER_ATTACHMENT_POD_LEFT	2
#define ASCANNER_ATTACHMENT_POD_BOTTOM		3

BEGIN_DATADESC( CNPC_ADrone )
	DEFINE_KEYFIELD( m_nPods,			FIELD_INTEGER, "NumberOfPods" ),

	DEFINE_FIELD( m_nAScannerState,			FIELD_INTEGER),
	DEFINE_ARRAY( m_pBeam,					FIELD_CLASSPTR, ASCANNER_NUM_BEAMS ),
	DEFINE_ARRAY( m_pLaserGlow,				FIELD_CLASSPTR, ASCANNER_NUM_GLOWS ),
	DEFINE_FIELD( m_bBankFace,				FIELD_BOOLEAN),
	DEFINE_FIELD( m_fNextGrenadeTime,		FIELD_TIME),
	DEFINE_FIELD( m_fNextLaserTime,			FIELD_TIME),

	DEFINE_FIELD( m_fNextMoveEvadeTime,		FIELD_TIME),
	DEFINE_FIELD( m_fNextAlarmSoundTime,	FIELD_TIME),
	DEFINE_FIELD( m_fNextFlySoundTime,		FIELD_TIME),
	DEFINE_FIELD( m_fTargetTime,			FIELD_TIME),
	DEFINE_FIELD( m_fSwitchToHelpTime,		FIELD_TIME),

	DEFINE_FIELD( m_fAntennaPos,			FIELD_FLOAT),
	DEFINE_FIELD( m_fAntennaTarget,			FIELD_FLOAT),

	DEFINE_FIELD( m_pLightGlow,				FIELD_CLASSPTR),

END_DATADESC()


LINK_ENTITY_TO_CLASS( npc_ascanner, CNPC_ADrone );
IMPLEMENT_CUSTOM_AI( npc_ascanner, CNPC_ADrone );

CNPC_ADrone::CNPC_ADrone()
{
#ifdef _DEBUG
	m_vCurrentBanking.Init();
#endif
}
//-----------------------------------------------------------------------------
// Purpose: Initialize the custom schedules
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_ADrone::InitCustomSchedules(void) 
{
	INIT_CUSTOM_AI(CNPC_ADrone);

	ADD_CUSTOM_TASK(CNPC_ADrone,	TASK_LASER_WARMUP);
	ADD_CUSTOM_TASK(CNPC_ADrone,	TASK_LASER_SHOOT);
	ADD_CUSTOM_TASK(CNPC_ADrone,	TASK_RAISE_ANTENNA);
	ADD_CUSTOM_TASK(CNPC_ADrone,	TASK_AIM_ROCKETS);
	ADD_CUSTOM_TASK(CNPC_ADrone,	TASK_FIRE_ROCKETS);

	ADD_CUSTOM_SCHEDULE(CNPC_ADrone,	SCHED_ASCANNER_ATTACK_HOVER);
	ADD_CUSTOM_SCHEDULE(CNPC_ADrone,	SCHED_ASCANNER_PATROL);
	ADD_CUSTOM_SCHEDULE(CNPC_ADrone,	SCHED_ASCANNER_GET_HELP);
	ADD_CUSTOM_SCHEDULE(CNPC_ADrone,	SCHED_ASCANNER_CHASE_ENEMY);
	ADD_CUSTOM_SCHEDULE(CNPC_ADrone,	SCHED_ASCANNER_SHOOT_LASER);

	ADD_CUSTOM_ACTIVITY(CNPC_ADrone,	ACT_ASCANNER_DROP);
	ADD_CUSTOM_ACTIVITY(CNPC_ADrone,	ACT_ASCANNER_FLINCH_BACK);
	ADD_CUSTOM_ACTIVITY(CNPC_ADrone,	ACT_ASCANNER_FLINCH_FRONT);
	ADD_CUSTOM_ACTIVITY(CNPC_ADrone,	ACT_ASCANNER_FLINCH_LEFT);
	ADD_CUSTOM_ACTIVITY(CNPC_ADrone,	ACT_ASCANNER_FLINCH_RIGHT);

	g_interactionWScannerBomb = CBaseCombatCharacter::GetInteractionID();

	AI_LOAD_SCHEDULE(CNPC_ADrone,	SCHED_ASCANNER_ATTACK_HOVER);
	AI_LOAD_SCHEDULE(CNPC_ADrone,	SCHED_ASCANNER_PATROL);
	AI_LOAD_SCHEDULE(CNPC_ADrone,	SCHED_ASCANNER_GET_HELP);
	AI_LOAD_SCHEDULE(CNPC_ADrone,	SCHED_ASCANNER_CHASE_ENEMY);
	AI_LOAD_SCHEDULE(CNPC_ADrone,	SCHED_ASCANNER_SHOOT_LASER);
}

//-----------------------------------------------------------------------------
// Purpose: Indicates this NPC's place in the relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_ADrone::Classify(void)
{
	return (CLASS_SCANNER);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_ADrone::StopLoopingSounds(void)
{
	StopSound(entindex(), "NPC_AScanner.HoverAlarm" );
	StopSound(entindex(), "NPC_AScanner.Hover" );
	StopSound(entindex(), "NPC_AScanner.Alarm" );
	StopSound(entindex(), "NPC_AScanner.LaserAim" );
	BaseClass::StopLoopingSounds();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
char *CNPC_ADrone::GetEngineSound( void )
{
	if ( m_nAScannerState == ASCANNER_STATE_EVASION )
		return( "NPC_AScanner.HoverAlarm" );
	else
		return( "NPC_AScanner.Hover" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ADrone::Precache(void)
{
	//
	// Model
	//
	engine->PrecacheModel("models/ascanner.mdl");
	engine->PrecacheModel("models/ascanner_pod.mdl");
	engine->PrecacheModel("models/Weapons/wscanner_grenade.mdl"); 
	
	engine->PrecacheModel("sprites/lgtning.vmt");	
	engine->PrecacheModel("sprites/glow01.vmt");

	UTIL_PrecacheOther("grenade_homer");

	//
	// Sounds
	//
	PrecacheScriptSound( "NPC_AScanner.Hover");
	PrecacheScriptSound( "NPC_AScanner.HoverAlarm");
	PrecacheScriptSound( "NPC_AScanner.Alarm");
	PrecacheScriptSound( "NPC_AScanner.DropPod");
	PrecacheScriptSound( "NPC_AScanner.LaserAim");
	PrecacheScriptSound( "NPC_AScanner.LaserShoot");
	PrecacheScriptSound( "NPC_AScanner.GrenadeShoot");
	PrecacheScriptSound( "NPC_AScanner.RocketAim");
	PrecacheScriptSound( "NPC_AScanner.RocketShoot");

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ADrone::Spawn(void)
{
	Precache();

	SetModel( "models/ascanner.mdl" );

	m_nAScannerState = ASCANNER_STATE_NORMAL;

	if (m_nPods != 0)
	{
		// Cap at 3 (wings, belly)
		if (m_nPods > 3)
			m_nPods = 3;

		SetBodygroup( m_nPods, true );
	}

	SetHullType( HULL_SMALL_CENTERED ); 
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	SetMoveType( MOVETYPE_FLY );	//STEP??
	SetGravity(0.001);

	m_iHealth = sk_ascanner_health.GetFloat();
	m_iMaxHealth = m_iHealth;
	SetViewOffset( Vector(0, 0, 10) );		// Position of the eyes relative to NPC's origin.
	m_flFieldOfView		= VIEW_FIELD_FULL;	// Gimble cam, no hiding unless completely out of sight
	m_NPCState			= NPC_STATE_NONE;
	SetBloodColor( DONT_BLEED );
	SetCurrentVelocity( vec3_origin );
	m_vCurrentBanking	= vec3_origin;
	SetNavType( NAV_FLY );
	AddFlag( FL_FLY );

	CapabilitiesAdd( bits_CAP_MOVE_FLY );
	CapabilitiesAdd( bits_CAP_SQUAD );
//	CapabilitiesAdd( bits_CAP_INNATE_RANGE_ATTACK1 );

	m_bBankFace					= false;

	m_vLastPatrolDir			= vec3_origin;
	m_fTargetTime				= ASCANNER_TARGET_TIME;
	m_fNextGrenadeTime			= 0;
	m_fNextLaserTime			= 0;
	m_fNextMoveEvadeTime		= 0;
	m_fNextAlarmSoundTime		= 0;
	m_fNextFlySoundTime			= 0;
	m_fSwitchToHelpTime			= 0;

	int i;
	for (i=0;i<ASCANNER_NUM_BEAMS;i++)
	{
		m_pBeam[i] = NULL;
	}
	for (i=0;i<ASCANNER_NUM_GLOWS;i++)
	{
		m_pLaserGlow[i] = NULL;
	}

	SetNoiseMod( 2, 2, 2 );

	m_fHeadYaw			= 0;
	m_fAntennaPos		= 0;
	m_fAntennaTarget	= 0;

	NPCInit();

	// Let this guy look far
	m_flDistTooFar = 99999999.0;
	SetDistLook( 4000.0 );
	m_flSpeed = ASCANNER_MAX_SPEED;

	// Keep track of the number of live WScanners
//	CNPC_ADrone::m_nNumAttackers++;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Type - 
//-----------------------------------------------------------------------------
int CNPC_ADrone::TranslateSchedule( int scheduleType ) 
{
	return BaseClass::TranslateSchedule(scheduleType);
}

void CNPC_ADrone::OnScheduleChange()
{
	m_flSpeed = ASCANNER_MAX_SPEED;

	BaseClass::OnScheduleChange();
}

//-----------------------------------------------------------------------------
bool CNPC_ADrone::ShouldGib( const CTakeDamageInfo &info )
{
	if ( info.GetDamageType() & (DMG_NEVERGIB|DMG_DISSOLVE) )
		return false;

	if ( ( g_pGameRules->Damage_ShouldGibCorpse( info.GetDamageType() ) && m_iHealth < GIB_HEALTH_VALUE ) || ( info.GetDamageType() & DMG_ALWAYSGIB ) )
		 return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_ADrone::Event_Killed( const CTakeDamageInfo &info )
{
	LaserKill();

	// If im really, really dead, explode, otherwise just fall to the ground
	if ( ShouldGib( info ) )
	{
		Gib();
		return;
	}
#if 1
	else
	{
		//TODO; convert to physobject, a really heavy one that crushes things
		ExplosionCreate(GetLocalOrigin(), GetLocalAngles(), this, random->RandomInt(5, 10), 64, true);
		UTIL_Remove(this);
	}
#else
	BaseClass::Event_Killed( info );
#endif
}

//------------------------------------------------------------------------------
// Purpose: Override to split in two when attacked
//------------------------------------------------------------------------------
int CNPC_ADrone::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	// -------------------------------------
	//  If hit by stalker beam, blow up
	// -------------------------------------
	if (info.GetAttacker()->Classify() == CLASS_STALKER)
	{
		info.SetDamage( m_iHealth+1 );
	}

	// Flinch in the direction of the attack
	float vAttackYaw = VecToYaw(g_vecAttackDir);

	float vAngleDiff = UTIL_AngleDiff( vAttackYaw, m_fHeadYaw );

	if (vAngleDiff > -45 && vAngleDiff < 45)
	{
		SetActivity((Activity)ACT_ASCANNER_FLINCH_BACK);
	}
	else if (vAngleDiff < -45 && vAngleDiff > -135)
	{
		SetActivity((Activity)ACT_ASCANNER_FLINCH_LEFT);
	}
	else if (vAngleDiff >  45 && vAngleDiff <  135)
	{
		SetActivity((Activity)ACT_ASCANNER_FLINCH_RIGHT);
	}
	else
	{
		SetActivity((Activity)ACT_ASCANNER_FLINCH_FRONT);
	}

	return BaseClass::OnTakeDamage_Alive( info );
}

//-----------------------------------------------------------------------------
// Purpose: Handles movement towards the last move target.
// Input  : flInterval - 
//-----------------------------------------------------------------------------
bool CNPC_ADrone::OverridePathMove( float flInterval )
{
	CBaseEntity *pMoveTarget = GetEnemy();

	Vector waypointDir = GetNavigator()->GetCurWaypointPos() - GetLocalOrigin();
	float flWaypointDist = VectorNormalize(waypointDir);

	// -----------------------------------------------------------------
	// Check route is blocked
	// ------------------------------------------------------------------
	Vector checkPos = GetLocalOrigin() + (waypointDir * (m_flSpeed * flInterval));

	AIMoveTrace_t moveTrace;
	GetMoveProbe()->MoveLimit( NAV_FLY, GetLocalOrigin(), checkPos, MASK_NPCSOLID|CONTENTS_WATER, pMoveTarget, &moveTrace);
	if (IsMoveBlocked(moveTrace))
	{
		TaskFail(FAIL_NO_ROUTE);
		GetNavigator()->ClearGoal();
		return true;
	}

	// --------------------------------------------------
	
	if (GetNavigator()->CurWaypointIsGoal() && 
	    IsCurSchedule(SCHED_ASCANNER_GET_HELP) && 
	    flWaypointDist < 100)
	{
		// If I'm geting help allow a wide berth
		TaskMovementComplete();
		return true;
	}

	// --------------------------------------------------
	//  Check if I've reached my goal
	// --------------------------------------------------
	
	Vector lastPatrolDir = GetNavigator()->GetCurWaypointPos() - GetLocalOrigin();
	
	if ( ProgressFlyPath( flInterval, GetEnemy(), MASK_NPCSOLID|CONTENTS_WATER,
						  !IsCurSchedule( SCHED_ASCANNER_PATROL ) ) == AINPP_COMPLETE )
	{
		if (IsCurSchedule( SCHED_ASCANNER_PATROL ))
		{
			m_vLastPatrolDir = lastPatrolDir;
			VectorNormalize(m_vLastPatrolDir);
		}
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
bool CNPC_ADrone::OverrideMove(float flInterval)
{
	// -----------------------------------------------------------------
	// If I have a route, keep it updated and move toward target
	// ------------------------------------------------------------------
	if (GetNavigator()->IsGoalActive())
	{
		if ( OverridePathMove( flInterval ) )
			return true;
	}

	// ----------------------------------------------
	//	If attacking 
	// ----------------------------------------------
	else if (	GetEnemy()			!= NULL && 
				m_nAScannerState	!= ASCANNER_STATE_EVASION)
	{
		MoveToAttack(flInterval);
	}
	// -----------------------------------------------------------------
	// If I don't have a route, just decelerate
	// -----------------------------------------------------------------
	else if (!GetNavigator()->IsGoalActive())
	{
		float	myDecay	 = 9.5;
		Decelerate( flInterval, myDecay );
		// -------------------------------------
		// If I have an enemy turn to face him
		// -------------------------------------
		if (GetEnemy())
		{
			TurnHeadToTarget(flInterval, GetEnemy()->GetLocalOrigin() );
		}
	}
	MoveExecute_Alive(flInterval);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_ADrone::MoveToTarget(float flInterval, const Vector &MoveTarget)
{
	// -------------------------------------
	// Move towards are target
	// -------------------------------------
	Vector targetDir = MoveTarget - GetLocalOrigin();
	VectorNormalize(targetDir);

	// ---------------------------------------
	//  If I have an enemy attempt to evade it
	// ---------------------------------------
	CBaseCombatCharacter *pEnemy  = GetEnemyCombatCharacterPointer();
	if (pEnemy)
	{
		targetDir = targetDir + 0.2 * MoveToEvade(pEnemy);
	}

	const float	myAccel	 = 300.0;
	const float	myDecay	 = 0.35; // decay to 35% in 1 second
	TurnHeadToTarget( flInterval, MoveTarget );
	MoveInDirection(flInterval, targetDir, myAccel, (2 * myAccel), myDecay);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
Vector CNPC_ADrone::MoveToEvade(CBaseCombatCharacter *pEnemy)
{
	// Don't evade if I just shot a grenade
	if (gpGlobals->curtime < m_fNextMoveEvadeTime)
	{
		return vec3_origin;
	}
	// -----------------------------------------
	//  Keep out of enemy's shooting position
	// -----------------------------------------
	Vector vEnemyFacing = pEnemy->BodyDirection2D( );
	Vector	vEnemyDir   = pEnemy->EyePosition() - GetLocalOrigin();
	VectorNormalize(vEnemyDir);
	float  fDotPr		= DotProduct(vEnemyFacing,vEnemyDir);

	if (fDotPr < -0.95)
	{
		Vector vDirUp(0,0,1);
		Vector vDir;
		CrossProduct( vEnemyFacing, vDirUp, vDir);

		Vector crossProduct;
		CrossProduct(vEnemyFacing, vEnemyDir, crossProduct);
		if (crossProduct.y < 0)
		{
			vDir = vDir * -1;
		}
		return (2*vDir);
	}
	else if (fDotPr < -0.9)
	{
		Vector vDirUp(0,0,1);
		Vector vDir;
		CrossProduct( vEnemyFacing, vDirUp, vDir);

		Vector crossProduct;
		CrossProduct(vEnemyFacing, vEnemyDir, crossProduct);
		if (crossProduct.y < 0)
		{
			vDir = vDir * -1;
		}
		return (vDir);
	}
	else if (fDotPr < -0.85)
	{
		Vector vDirUp(0,0,1);
		Vector vDir;
		CrossProduct( vEnemyFacing, vDirUp, vDir);

		Vector crossProduct;
		CrossProduct(vEnemyFacing, vEnemyDir, crossProduct);
		if (random->RandomInt(0,1))
		{
			vDir = vDir * -1;
		}
		return (vDir);
	}
	return vec3_origin;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_ADrone::MoveToAttack(float flInterval)
{
	CBaseCombatCharacter* pEnemy  = GetEnemyCombatCharacterPointer();

	if (!pEnemy)
	{
		return;
	}

	Vector vFlyDirection = vec3_origin;

	// -----------------------------------------
	//  Keep within range of enemy
	// -----------------------------------------
	Vector	vEnemyDir  = pEnemy->EyePosition() - GetLocalOrigin();
	float	fEnemyDist = VectorNormalize(vEnemyDir);
	if (fEnemyDist < ASCANNER_HOVER_NEAR_DIST)
	{
		vFlyDirection = -vEnemyDir;
	}
	else if (fEnemyDist > ASCANNER_HOVER_FAR_DIST)
	{
		vFlyDirection = vEnemyDir;
	}
	else if (gpGlobals->curtime > m_fNextGrenadeTime)
	{
		if ( m_nAScannerState != ASCANNER_STATE_STRAFE )
		{
			TossGrenade();
		}
	}

	// -------------------------------------
	//	Add some evasion
	// -------------------------------------
	vFlyDirection = vFlyDirection + MoveToEvade(pEnemy);

	// -------------------------------------
	// Set net velocity 
	// -------------------------------------
	float myAccel = 300.0;
	float myDecay = 0.35; // decay to 35% in 1 second
	MoveInDirection( flInterval, vFlyDirection, myAccel, 2 * myAccel, myDecay);
		
	TurnHeadToTarget( flInterval, GetEnemy()->GetLocalOrigin() );
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_ADrone::PlayAlarmSound(void)
{
	if (gpGlobals->curtime > m_fNextAlarmSoundTime)
	{
//		if (!GetHintNode())
//			return;

		m_pLightGlow = CSprite::SpriteCreate( "sprites/glow01.vmt", GetLocalOrigin(), FALSE );
		m_pLightGlow->SetTransparency( kRenderGlow, 255, 200, 200, 0, kRenderFxNoDissipation );
		m_pLightGlow->SetAttachment( this, 2 );
		m_pLightGlow->SetBrightness( 255 );
		m_pLightGlow->SetColor(255,0,0);
		m_pLightGlow->SetScale( 0.7 );
		m_pLightGlow->Expand( 0, 10 );
		m_pLightGlow->AnimateForTime( 1, 0.2 );
		m_pLightGlow = NULL;

		EmitSound( "NPC_AScanner.Alarm" );

		// Alarm sound and light frequency more often as wscanner nears join point
		Vector vHintPos;
		GetHintNode()->GetPosition(this,&vHintPos);

		float fHintDist		= (vHintPos - GetLocalOrigin()).Length();
		float fDelayTime	= fHintDist/2500;

		if (fDelayTime > 3.0) fDelayTime = 3.0;
		if (fDelayTime < 1.0) fDelayTime = 1.0;
		m_fNextAlarmSoundTime	= gpGlobals->curtime + fDelayTime;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_ADrone::AdjustAntenna(float flInterval)
{	
	if (m_fAntennaPos != m_fAntennaTarget)
	{
		// Make frame rate independent
		float timeToUse = flInterval;
		float iRate	= 0.8;
		while (timeToUse > 0)
		{
			m_fAntennaPos = (iRate * m_fAntennaPos) + (1-iRate)*m_fAntennaTarget;
			timeToUse =- 0.1;
		}
		SetBoneController( 1, m_fAntennaPos );
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
float CNPC_ADrone::MinGroundDist(void)
{
	return ASCANNER_MIN_GROUND_DIST;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_ADrone::SetBanking(float flInterval)
{
	// Make frame rate independent
	float iRate = 0.5;
	float timeToUse = flInterval;
	while (timeToUse > 0)
	{
		Vector vFlyDirection	= GetCurrentVelocity();
		VectorNormalize(vFlyDirection);
		
		// If I'm shooting a grenade don't bank
		if (GetActivity() == ACT_RANGE_ATTACK1)
			vFlyDirection		= vec3_origin;
		else
			vFlyDirection		= vFlyDirection * ASCANNER_BANK_RATE;

		Vector vBankDir;
		if (m_bBankFace && GetEnemy()!=NULL)
		{
			// Bank to face enemy
			Vector vRawDir = GetEnemy()->EyePosition() - GetLocalOrigin();
			VectorNormalize( vRawDir );
			vRawDir *= 50; 

			// Account for head rotation
			vBankDir.x = vRawDir.z * -cos(m_fHeadYaw*M_PI/180);
			vBankDir.z = vRawDir.z *  sin(m_fHeadYaw*M_PI/180);
		}
		else
		{
			// Bank based on fly direction
			vBankDir = vFlyDirection;
		}

		m_vCurrentBanking.x		= (iRate * m_vCurrentBanking.x) + (1 - iRate)*(vBankDir.x);
		m_vCurrentBanking.z		= (iRate * m_vCurrentBanking.z) + (1 - iRate)*(vBankDir.z);
		timeToUse =- 0.1;
	}
	SetLocalAngles( QAngle( m_vCurrentBanking.x, 0, m_vCurrentBanking.z ) );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_ADrone::MoveExecute_Alive(float flInterval)
{
	// -------------------------------------------
	//  Avoid obstacles
	// -------------------------------------------
	SetCurrentVelocity( GetCurrentVelocity() + VelocityToAvoidObstacles(flInterval) );

	// ----------------------------------------------------------------------------------------
	// Add time-coherent noise to the current velocity so that it never looks bolted in place.
	// ----------------------------------------------------------------------------------------
	float noiseScale = 7.0;
	if ((CBaseEntity*)GetEnemy() && (GetLocalOrigin() - GetEnemy()->GetLocalOrigin()).Length2D() < 80.0) 
	{
		// Less noise when I'm close to enemy (attacking)
		noiseScale = 2.0;
	}

	AddNoiseToVelocity( noiseScale );
	
	// ---------------------
	//  Limit overall speed
	// ---------------------
	LimitSpeed( 300 );

	SetBanking(flInterval);

	// -------------------------------------------------------
	// If travelling on a path and getting help play an alarm
	// -------------------------------------------------------
	if (m_nAScannerState == ASCANNER_STATE_EVASION &&
		GetNavigator()->IsGoalActive()			)
	{
		PlayAlarmSound();
	}
	PlayFlySound();
	AdjustAntenna(flInterval);
	FlyMove( GetCurrentVelocity() * flInterval, MASK_NPCSOLID );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_ADrone::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_LASER_WARMUP:
		{
			// -------------------
			//  Adjust beams
			// -------------------
			int i;
			float fScale = (1-(m_flWaitFinished - gpGlobals->curtime)/ASCANNER_LASER_WARMUP_TIME);
			for (i=0;i<ASCANNER_NUM_BEAMS-1;i++)
			{
				if (m_pBeam[i])
				{
					m_pBeam[i]->SetBrightness(255*fScale);
					m_pBeam[i]->SetColor( 255, 255*fScale, 255*fScale );
				}
			}

			// -------------------
			//  Adjust glow
			// -------------------
			for (i=0;i<ASCANNER_NUM_GLOWS-1;i++)
			{
				if (m_pLaserGlow[i])
				{
					m_pLaserGlow[i]->SetColor( 255, 255*fScale, 255*fScale );
					m_pLaserGlow[i]->SetBrightness( 100 + (155*fScale) );
				}
			}

			if ( gpGlobals->curtime >= m_flWaitFinished )
			{
				TaskComplete();
			}
			break;
		}
		case TASK_AIM_ROCKETS:
		{
			if ( GetActivity() != ACT_ASCANNER_DROP )
			{
				SetAScannerState(ASCANNER_STATE_STRAFE);
				TaskComplete();
			}
			else
			{
				// Set lasersight and start beeping

			}
			break;
		}
		// If my enemy has moved significantly, update my path
		case TASK_WAIT_FOR_MOVEMENT:
		{
			CBaseEntity *pEnemy = GetEnemy();
			if (pEnemy														 &&
				IsCurSchedule(SCHED_ASCANNER_CHASE_ENEMY) && 
				GetNavigator()->IsGoalActive()												 )
			{
				Vector flEnemyLKP = GetEnemyLKP();
				if ((GetNavigator()->GetGoalPos() - pEnemy->EyePosition()).Length() > 40 )
				{
					GetNavigator()->UpdateGoalPos(pEnemy->EyePosition());
				}
				// If final position is enemy, exit my schedule (will go to attack hover)
				if (GetNavigator()->IsGoalActive() && 
					GetNavigator()->GetCurWaypointPos() == pEnemy->EyePosition())
				{
					TaskComplete();
					GetNavigator()->ClearGoal();		// Stop moving
					break;
				}
			}				
			else if (IsCurSchedule(SCHED_ASCANNER_GET_HELP) && GetHintNode())
			{
				Vector vHintPos;
				GetHintNode()->GetPosition(this,&vHintPos);

				float fHintDist = (vHintPos - GetLocalOrigin()).Length();
				if (fHintDist < 100)
				{
					TaskComplete();
					GetNavigator()->ClearGoal();		// Stop moving
					break;
				}
			}
			BaseClass::RunTask(pTask);
			break;
		}
		default:
		{
			BaseClass::RunTask(pTask);
		}
	}
}

//------------------------------------------------------------------------------
bool CNPC_ADrone::IsValidEnemy( CBaseEntity *pEnemy )
{
	// AScanner can see forever, so in order to be fair,
	// enemies must be seen for a certain amount of time before
	// we can engage/let our squadmates know
	if ( gpGlobals->curtime < m_fTargetTime )
		return false;

	if ((GetAbsOrigin() - pEnemy->GetAbsOrigin()).Length() > ASCANNER_MAX_ATTACK_RADIUS)
		return false;

	return BaseClass::IsValidEnemy(pEnemy);
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CNPC_ADrone::SetAScannerState(AScannerState_t pNewState)
{
	m_nAScannerState = pNewState;
}

//------------------------------------------------------------------------------
// Purpose : Get pod split positions
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector CNPC_ADrone::PodPos(int nPos)
{
	// Only 4 attachment points are available, so use shooting attachment 
	// points to determine split position and offset from model center
	if (m_nPods == 3)
	{
		Vector vAttachPos;
		QAngle vAttachAng;
		GetAttachment( nPos, vAttachPos, vAttachAng );
		Vector vDir			= 2*(vAttachPos - GetLocalOrigin());
		vAttachPos			= GetLocalOrigin() + vDir;
		return vAttachPos;
	}
	else
	{
		Vector vForward,vRight,vUp;
		AngleVectors( GetLocalAngles(), &vForward, &vRight, &vUp );
		if (nPos == ASCANNER_ATTACHMENT_POD_RIGHT)
		{
			return GetLocalOrigin() - vRight * 35;
		}
		else if (nPos == ASCANNER_ATTACHMENT_POD_LEFT)
		{
			return GetLocalOrigin() + vRight * 35;
		}
		else
		{
			return GetLocalOrigin() + vUp * 35;
		}
	}
}

//------------------------------------------------------------------------------
// Purpose : Drop missile pod
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CNPC_ADrone::DropPod( int nPod )
{
	Vector vLeftAttach  = PodPos(ASCANNER_ATTACHMENT_POD_LEFT);
	Vector vRightAttach	= PodPos(ASCANNER_ATTACHMENT_POD_RIGHT);
	Vector vTopAttach	= PodPos(ASCANNER_ATTACHMENT_POD_BOTTOM);

	//TODO;

	EmitSound( "NPC_AScanner.DropPod" );
}

//-----------------------------------------------------------------------------
// Purpose: Gets the appropriate next schedule based on current condition
//			bits.
//-----------------------------------------------------------------------------
int CNPC_ADrone::SelectSchedule(void)
{
	// Kill laser if its still on
	LaserKill();
	m_fAntennaTarget = 0;

	switch ( m_NPCState )
	{
		case NPC_STATE_IDLE:
		{
			//Fly around
			return SCHED_ASCANNER_PATROL;
			break;
		}
		case NPC_STATE_DEAD:
		case NPC_STATE_SCRIPT:
		{
			return BaseClass::SelectSchedule();
			break;
		}
		default:
		{
			// ------------------------------------------------
			//  If I'm locking on:
			// ------------------------------------------------
			if (m_nAScannerState == ASCANNER_STATE_STRAFE)
			{
				// ----------------------------------------------------------
				//  If I have an enemy
				// ----------------------------------------------------------
				if ( m_nPods > 1 && GetEnemy() != NULL && GetEnemy()->IsAlive() )
				{
					// --------------------------------------------
					//  Track the enemy
					// --------------------------------------------
					//!!TODO;
				}
				//!else
				//!{
					SetAScannerState(ASCANNER_STATE_NORMAL);
					return SCHED_ASCANNER_PATROL;
				//!}
			}
			// --------------------------------------------------
			//  If I'm getting help
			// --------------------------------------------------
			else if (m_nAScannerState == ASCANNER_STATE_EVASION)
			{
				// If enemy is dead, go back to patrol
				if ( GetEnemy() == NULL		|| 
					!GetEnemy()->IsAlive()	)
				{
					SetAScannerState(ASCANNER_STATE_NORMAL);
					return SCHED_ASCANNER_PATROL;
				}
				return SCHED_ASCANNER_GET_HELP;
			}
			// --------------------------------------------------
			//  If I'm attacking		
			// --------------------------------------------------
			else
			{
				// --------------------------------------------------
				//  Otherwise hover and attack
				// --------------------------------------------------
				if (GetEnemy() != NULL )
				{
					if (HasCondition(COND_SEE_ENEMY))
					{
						// --------------------------------------------
						//  Attack attack attack!
						// --------------------------------------------
						float fEnemyDist = (GetEnemy()->EyePosition() - GetLocalOrigin()).Length();
						if (gpGlobals->curtime > m_fNextLaserTime && fEnemyDist < 1000)
						{
							return SCHED_ASCANNER_SHOOT_LASER;
						}
						else
						{
							return SCHED_ASCANNER_ATTACK_HOVER;
						}
					}
					else
					{
						return SCHED_ASCANNER_CHASE_ENEMY;
					}
				}
				else
				{
					return SCHED_ASCANNER_PATROL;
				}
			}
			return SCHED_FAIL;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CNPC_ADrone::StartTask( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
		// Create
		case TASK_LASER_WARMUP:
		{
			LaserWarmup();
			// set a future time that tells us when the warmup is over.
			m_flWaitFinished = gpGlobals->curtime + ASCANNER_LASER_WARMUP_TIME;
			break;
		}
		case TASK_LASER_SHOOT:
		{
			LaserShoot();
			TaskComplete();
			break;
		}
		case TASK_AIM_ROCKETS:
		{
			// Start beams a-going
			StartAimBeam();
			// set a future time that tells us when the warmup is over.
			m_flWaitFinished = gpGlobals->curtime + ASCANNER_ROCKET_WARMUP_TIME;
			break;
		}
		case TASK_FIRE_ROCKETS:
		{
			// Change my model
			SetBodygroup( m_nPods, true );
			SetHullSizeNormal();

			// Play drop activity
			SetActivity((Activity)ACT_ASCANNER_DROP);
			DropPod( m_nPods );
			TaskComplete();
			break;
		}
		case TASK_RAISE_ANTENNA:
		{
			m_fAntennaTarget = 24;
			TaskComplete();
			break;
		}
		// Override so can find hint nodes that are much further away
		case TASK_FIND_HINTNODE:
		{
			if (!GetHintNode())
			{
				SetHintNode( CAI_HintManager::FindHint( this, HINT_NONE, pTask->flTaskData, 5000 ) );
			}
			if ( GetHintNode() )
			{
				TaskComplete();
			}
			else
			{
				// No hint node run from enemy
				SetSchedule( SCHED_RUN_FROM_ENEMY );
			}
			break;
		}
		default:
		{
				
			BaseClass::StartTask(pTask);
		}
	}
}

//------------------------------------------------------------------------------
// Purpose : Beam that slowly homes in on enemy position
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CNPC_ADrone::StartAimBeam(void)
{
	// --------------------------
	//  Blue beams between pods
	// --------------------------
	if (m_nPods == 3)
	{
		m_pBeam[0] = CBeam::BeamCreate( "sprites/lgtning.vmt", 2.0 );
		m_pBeam[0]->SetColor( 50, 50, 255 ); 
		m_pBeam[0]->SetBrightness( 0 );
		m_pBeam[0]->SetNoise( 100 );
		m_pBeam[0]->SetWidth( 3.0 );
		m_pBeam[0]->EntsInit( this, this );
		m_pBeam[0]->SetStartAttachment( 1 );
		m_pBeam[0]->SetEndAttachment( 2 );	

		m_pBeam[1] = CBeam::BeamCreate( "sprites/lgtning.vmt", 2.0 );
		m_pBeam[1]->SetColor( 50, 50, 255 ); 
		m_pBeam[1]->SetBrightness( 0 );
		m_pBeam[1]->SetNoise( 16 );
		m_pBeam[1]->SetWidth( 3.0 );
		m_pBeam[1]->EntsInit( this, this );
		m_pBeam[1]->SetStartAttachment( 2 );
		m_pBeam[1]->SetEndAttachment( 3 );

		m_pBeam[2] = CBeam::BeamCreate( "sprites/lgtning.vmt", 2.0 );
		m_pBeam[2]->SetColor( 50, 50, 255 ); 
		m_pBeam[2]->SetBrightness( 0 );
		m_pBeam[2]->SetNoise( 16 );
		m_pBeam[2]->SetWidth( 3.0 );
		m_pBeam[2]->EntsInit( this, this );
		m_pBeam[2]->SetStartAttachment( 1 );
		m_pBeam[2]->SetEndAttachment( 2 );	

		m_pBeam[3] = CBeam::BeamCreate( "sprites/lgtning.vmt", 2.0 );
		m_pBeam[3]->SetColor( 50, 50, 255 ); 
		m_pBeam[3]->SetBrightness( 0 );
		m_pBeam[3]->SetWidth( 3.0 );
		m_pBeam[3]->SetNoise( 16 );
		m_pBeam[3]->EntsInit( this, this );
		m_pBeam[3]->SetStartAttachment( 2 );
		m_pBeam[3]->SetEndAttachment( 3 );
	}
	else	
	{
		m_pBeam[0] = CBeam::BeamCreate( "sprites/lgtning.vmt", 2.0 );
		m_pBeam[0]->SetColor( 50, 50, 255 ); 
		m_pBeam[0]->SetBrightness( 0 );
		m_pBeam[0]->SetNoise( 100 );
		m_pBeam[0]->SetWidth( 3.0 );
		m_pBeam[0]->EntsInit( this, this );
		m_pBeam[0]->SetStartAttachment( 2 );
		m_pBeam[0]->SetEndAttachment( 3 );

		m_pBeam[1] = CBeam::BeamCreate( "sprites/lgtning.vmt", 2.0 );
		m_pBeam[1]->SetColor( 50, 50, 255 ); 
		m_pBeam[1]->SetBrightness( 0 );
		m_pBeam[1]->SetNoise( 16 );
		m_pBeam[1]->SetWidth( 3.0 );
		m_pBeam[1]->EntsInit( this, this );
		m_pBeam[1]->SetStartAttachment( 2 );
		m_pBeam[1]->SetEndAttachment( 3 );
	}

	EmitSound( "NPC_AScanner.LaserAim" );
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CNPC_ADrone::LaserWarmup(void)
{
	// -------------------
	//  Converging beams
	// -------------------
	int i;
	for (i=0;i<ASCANNER_NUM_BEAMS-1;i++)
	{
		m_pBeam[i] = CBeam::BeamCreate( "sprites/lgtning.vmt", 2.0 );
		m_pBeam[i]->SetColor( 255, 0, 0 ); 
		m_pBeam[i]->SetBrightness( 100 );
		m_pBeam[i]->SetNoise( 8 );
		m_pBeam[i]->EntsInit( this, this );
		m_pBeam[i]->SetStartAttachment( i+1 );
		m_pBeam[i]->SetEndAttachment( 4 );
	}

	// -------------
	//  Glow
	// -------------
	for (i=0;i<ASCANNER_NUM_GLOWS-1;i++)
	{
		m_pLaserGlow[i] = CSprite::SpriteCreate( "sprites/glow01.vmt", GetLocalOrigin(), FALSE );
		m_pLaserGlow[i]->SetAttachment( this, 4 );
		m_pLaserGlow[i]->SetTransparency( kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation );
		m_pLaserGlow[i]->SetBrightness( 100 );
		m_pLaserGlow[i]->SetScale( 0.3 );
	}

	EmitSound( "NPC_AScanner.LaserWarmUp" );

	// Bank to face player
	m_bBankFace = true;

	// Don't shoot laser again for a while
	m_fNextLaserTime = gpGlobals->curtime + 10;

	// After firing sit still for a second to make easier to hit
	SetCurrentVelocity( vec3_origin );
	m_fNextMoveEvadeTime = gpGlobals->curtime + ASCANNER_LASER_WARMUP_TIME;

}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CNPC_ADrone::LaserShoot(void)
{
	if (GetEnemy() != NULL)
	{
		Vector shootPos = Weapon_ShootPosition();
		Vector vTargetDir = (GetEnemy()->EyePosition() - shootPos);
		vTargetDir.z -= 12;  // A little below the eyes

		trace_t tr;
		AI_TraceLine ( shootPos, vTargetDir * 2048, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		CBaseEntity *pEntity = tr.m_pEnt;
		if (pEntity != NULL && m_takedamage)
		{
			ClearMultiDamage();
			CTakeDamageInfo info( this, this, sk_ascanner_laser_dmg.GetFloat(), DMG_SHOCK );
			CalculateMeleeDamageForce( &info, vTargetDir, tr.endpos );
			pEntity->DispatchTraceAttack( info, vTargetDir, &tr );
			ApplyMultiDamage();
		}

		// -------------
		//  Shoot beam
		// -------------
		int nBeamNum = ASCANNER_NUM_BEAMS-1;
		m_pBeam[nBeamNum] = CBeam::BeamCreate( "sprites/lgtning.vmt", 2.0 );
		m_pBeam[nBeamNum]->SetColor( 255, 255, 255 );
		m_pBeam[nBeamNum]->PointEntInit( tr.endpos, this );
		m_pBeam[nBeamNum]->SetEndAttachment( 4 );  
		m_pBeam[nBeamNum]->SetBrightness( 255 );
		m_pBeam[nBeamNum]->SetNoise( 0 );
		

		// -------------
		//  Impact Glow
		// -------------
		if (pEntity && pEntity->IsPlayer())
		{
			int nGlowNum = ASCANNER_NUM_GLOWS-1;
			m_pLaserGlow[nGlowNum] = CSprite::SpriteCreate( "sprites/glow01.vmt", tr.endpos, true );
			m_pLaserGlow[nGlowNum]->SetTransparency( kRenderGlow, 255, 200, 200, 0, kRenderFxNoDissipation );
			m_pLaserGlow[nGlowNum]->SetBrightness( 255 );
			m_pLaserGlow[nGlowNum]->SetScale( 0.8 );
			m_pLaserGlow[nGlowNum]->Expand( 10, 100 );
			m_pLaserGlow[nGlowNum] = NULL;
		}

		EmitSound( "NPC_AScanner.LaserShoot" );
	}

	for (int i=0;i<ASCANNER_NUM_BEAMS;i++)
	{
		if (m_pBeam[i])
		{
			// Let beam kill itself
			m_pBeam[i]->LiveForTime(0.2);
			m_pBeam[i] = NULL;
		}
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CNPC_ADrone::LaserKill(void)
{
	// -------------------------------
	//  Kill beams if not set to die
	// ------------------------------
	int i;
	for (i=0;i<ASCANNER_NUM_BEAMS;i++)
	{
		if (m_pBeam[i])
		{
			UTIL_Remove(m_pBeam[i]);
			m_pBeam[i] = NULL;
		}
	}

	// ---------------------
	//  Kill laser
	// ---------------------
	for (i=0;i<ASCANNER_NUM_GLOWS;i++)
	{
		if (m_pLaserGlow[i])
		{
			UTIL_Remove( m_pLaserGlow[i] );
			m_pLaserGlow[i] = NULL;
		}
	}

	// No longer bank to face player
	m_bBankFace = false;
 
	// Kill charge sound in case still going
	StopSound(entindex(), "NPC_AScanner.LaserWarmUp" );
}

//-----------------------------------------------------------------------------
// Purpose: Catches the monster-specific messages that occur when tagged
//			animation frames are played.
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CNPC_ADrone::HandleAnimEvent( animevent_t *pEvent )
{
	if( pEvent->event == AE_ASCANNER_SHOOT )
	{
		// Just callback to toss grenade
		TossGrenade( true );
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CNPC_ADrone::TossGrenade( bool bFireGrenade )
{
	if ( bFireGrenade )
	{
		if (GetEnemy())
		{
			Vector vShootPos;
			QAngle vShootAng;
			GetAttachment( ASCANNER_ATTACHMENT_SHOOT, vShootPos, vShootAng );
				
			EmitSound( "NPC_AScanner.GrenadeShoot" );

			CGrenadeHomer *pGrenade = CGrenadeHomer::CreateGrenadeHomer( MAKE_STRING("models/Weapons/wscanner_grenade.mdl"), MAKE_STRING(""),  GetAbsOrigin(), vec3_angle, edict() );
			pGrenade->Spawn();
			pGrenade->SetSpin(1,10);
			pGrenade->SetHoming(0.05,1,0.5,1,0.5);
			pGrenade->SetDamage(sk_ascanner_grenade_dmg.GetFloat());
			pGrenade->SetDamageRadius(200);
			pGrenade->Launch(this,GetEnemy(),800,600,GetGravity(),2);

			if (GetEnemy()->GetFlags() & FL_NPC)
			{
				GetEnemy()->MyNPCPointer()->HandleInteraction( g_interactionWScannerBomb, pGrenade, this );
			}
		}
	}
	else
	{
		// Play grenate toss activity (event launches actual grenade)
		SetActivity((Activity)ACT_RANGE_ATTACK1);

		m_fNextGrenadeTime = gpGlobals->curtime + random->RandomFloat(1.8,2.2); 

		// After firing sit still for a second to make easier to hit
		SetCurrentVelocity( vec3_origin );
		m_fNextMoveEvadeTime = gpGlobals->curtime + 1.2;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells use whether or not the NPC cares about a given type of hint node.
// Input  : sHint - 
// Output : TRUE if the NPC is interested in this hint type, FALSE if not.
//-----------------------------------------------------------------------------
bool CNPC_ADrone::FValidateHintType(CAI_Hint *pHint)
{
//	return(pHint->HintType() == (HINT_WASTE_SCANNER_SPAWN);

	// Waste scanner spawn isnt used right now, so we gotta be compatible
	// with what the other scanners use
	switch( pHint->HintType() )
	{
	case HINT_WASTE_SCANNER_SPAWN:
	case HINT_WORLD_WINDOW:
	case HINT_ROLLER_PATROL_POINT:
		return true;
		break;

	default:
		return false;
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CNPC_ADrone::DrawDebugTextOverlays(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		// Print state
		char tempstr[512];
		Q_snprintf(tempstr,sizeof(tempstr),"WS State: ");
		switch (m_nAScannerState)
		{
			case ASCANNER_STATE_NORMAL:				strcat(tempstr,"Normal");		break;
			case ASCANNER_STATE_STRAFE:				strcat(tempstr,"Strafing");		break;
			case ASCANNER_STATE_EVASION:		strcat(tempstr,"Evading");	break;
		}
		NDebugOverlay::EntityText(entindex(),text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
CNPC_ADrone::~CNPC_ADrone(void)
{
	// Decrease count of scanners that are on the attack
//	CNPC_ADrone::m_nNumAttackers--;
}


//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

//=========================================================
// > SCHED_ASCANNER_ATTACK_HOVER
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_ASCANNER_ATTACK_HOVER,

	"	Tasks"
	"		TASK_WAIT				5"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
);

//=========================================================
// > SCHED_ASCANNER_SHOOT_LASER
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_ASCANNER_SHOOT_LASER,

	"	Tasks"
	"		TASK_LASER_WARMUP		0"
	"		TASK_LASER_SHOOT		0"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
);

//=========================================================
// > SCHED_ASCANNER_PATROL
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_ASCANNER_PATROL,

	"	Tasks"
	"		TASK_SET_TOLERANCE_DISTANCE		48"
	"		TASK_SET_ROUTE_SEARCH_TIME		5"	// Spend 5 seconds trying to build a path if stuck
	"		TASK_GET_PATH_TO_RANDOM_NODE	2000"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_GIVE_WAY"
	"		COND_NEW_ENEMY"
	"		COND_SEE_ENEMY"
	"		COND_SEE_FEAR"
	"		COND_HEAR_COMBAT"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_PLAYER"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_PROVOKED"
);

//=========================================================
// > SCHED_ASCANNER_CHASE_ENEMY
//
//  Different interrupts than normal chase enemy.  
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_ASCANNER_CHASE_ENEMY,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_ASCANNER_PATROL"
	"		TASK_SET_TOLERANCE_DISTANCE			120"
	"		TASK_GET_PATH_TO_ENEMY				0"
	"		TASK_RUN_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
//	"		COND_CAN_RANGE_ATTACK1"
);

//=========================================================
// > SCHED_ASCANNER_GET_HELP
//
// Fly towards nearest hint OR ally
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_ASCANNER_GET_HELP,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ASCANNER_GET_HELP"
	"		TASK_WAIT_PVS					0"
	"		TASK_FIND_HINTNODE				0"
	"		TASK_GET_PATH_TO_HINTNODE		0"
	"		TASK_SET_TOLERANCE_DISTANCE		0"
	"		TASK_RAISE_ANTENNA				0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_STOP_MOVING				0"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
);
