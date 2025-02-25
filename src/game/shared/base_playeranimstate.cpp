//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Base anim state
//
// TODO; Need gesture support, movement should be separate from the main ACT...
//=============================================================================//

#include "cbase.h"
#include "base_playeranimstate.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
#include "filesystem.h"


#ifdef CLIENT_DLL
	#include "c_baseplayer.h"
	#include "engine/ivdebugoverlay.h"
	#include "eventlist.h"

	ConVar cl_showanimstate( "cl_showanimstate", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show the (client) animation state for the specified entity (-1 for none)." );
	ConVar showanimstate_log( "cl_showanimstate_log", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "1 to output cl_showanimstate to Msg(). 2 to store in AnimStateClient.log. 3 for both." );
#else
	#include "player.h"
	ConVar sv_showanimstate( "sv_showanimstate", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show the (server) animation state for the specified entity (-1 for none)." );
	ConVar showanimstate_log( "sv_showanimstate_log", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "1 to output sv_showanimstate to Msg(). 2 to store in AnimStateServer.log. 3 for both." );
#endif

ConVar mp_feetyawrate( 
	"mp_feetyawrate", 
	"720", 
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, 
	"How many degrees per second that we can turn our feet or upper body." );

ConVar mp_facefronttime( 
	"mp_facefronttime", 
	"0.1", 
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, 
	"After this amount of time of standing in place but aiming to one side, go ahead and move feet to face upper body." );

ConVar mp_ik( "mp_ik", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Use IK on in-place turns." );
ConVar mp_slammoveyaw( "mp_slammoveyaw", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Force movement yaw along an animation path." );

// Below this many degrees, slow down turning rate linearly
#define FADE_TURN_DEGREES	45.0f

// After this, need to start turning feet
#define MAX_TORSO_ANGLE		50.0f

// Below this amount, don't play a turning animation/perform IK
#define MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION		15.0f
#define BODYYAW_RATE		mp_feetyawrate.GetFloat()

// Pose parameters stored for debugging.
float g_flLastBodyPitch, g_flLastBodyYaw, m_flLastMoveYaw;

IPlayerAnimState* CreatePlayerAnimState( CBasePlayer *pPlayer )
{
	CBasePlayerAnimState *pState = new CBasePlayerAnimState;

	CModAnimConfig config;
	config.m_flMaxBodyYawDegrees = 45;
	config.m_LegAnimType = LEGANIM_8WAY;
	config.m_bUseAimSequences = false;

	pState->Init( pPlayer, config );
	return pState;
}

// ------------------------------------------------------------------------------------------------ //
// CBasePlayerAnimState implementation.
// ------------------------------------------------------------------------------------------------ //
CBasePlayerAnimState::CBasePlayerAnimState()
{
	m_flEyeYaw = 0.0f;
	m_flEyePitch = 0.0f;
	m_bCurrentFeetYawInitialized = false;
	m_flCurrentTorsoYaw = 0.0f;
	m_flCurrentTorsoYaw = TURN_NONE;
	m_flMaxGroundSpeed = 0.0f;
	m_flStoredCycle = 0.0f;

	m_bJumping = false;
	m_flJumpStartTime = 0.0f;	
	m_bFirstJumpFrame = false;

	m_bFiring = false;
	m_flFireStartTime = 0.0f;	

	m_flGaitYaw = 0.0f;
	m_flGoalFeetYaw = 0.0f;
	m_flCurrentFeetYaw = 0.0f;
	m_flLastYaw = 0.0f;
	m_flLastTurnTime = 0.0f;
	m_nTurningInPlace = 0;
	m_angRender.Init();
	m_vLastMovePose.Init();
	m_iCurrent8WayIdleSequence = -1;
	m_iCurrent8WayCrouchIdleSequence = -1;

	m_pOuter = NULL;
	m_eCurrentMainSequenceActivity = ACT_IDLE;
	m_flLastAnimationStateClearTime = 0.0f;
}

CBasePlayerAnimState::~CBasePlayerAnimState()
{
}


void CBasePlayerAnimState::Init( CBasePlayer *pPlayer, const CModAnimConfig &config )
{
	m_pOuter = pPlayer;
	m_AnimConfig = config;
	ClearAnimationState();
}


void CBasePlayerAnimState::Release()
{
	delete this;
}


void CBasePlayerAnimState::ClearAnimationState()
{
	ClearAnimationLayers();
	m_bJumping = false;
	m_bFiring = false;
	m_bDying = false;
	m_bCurrentFeetYawInitialized = false;
	m_flLastAnimationStateClearTime = gpGlobals->curtime;
}

float CBasePlayerAnimState::TimeSinceLastAnimationStateClear() const
{
	return gpGlobals->curtime - m_flLastAnimationStateClearTime;
}


void CBasePlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	VPROF( "CBasePlayerAnimState::Update" );

	// Get the studio header for the player.
	CStudioHdr *pStudioHdr = GetOuter()->GetModelPtr();
	if ( !pStudioHdr )
		return;

	// Some mods don't want to update the player's animation state if they're dead and ragdolled.
	if ( !ShouldUpdateAnimState() )
	{
		ClearAnimationState();
		return;
	}

	// Store these. All the calculations are based on them.
	m_flEyeYaw = AngleNormalize( eyeYaw );
	m_flEyePitch = AngleNormalize( eyePitch );
	m_angRender[YAW] = m_flEyeYaw;
	m_angRender[ROLL] = 0;
	m_angRender[PITCH] = 0;

	// Clear animation overlays because we're about to completely reconstruct them.
	ClearAnimationLayers();

	// Compute the player sequences.
	ComputeSequences( pStudioHdr );

	// Compute all the pose params.
	ComputePoseParam_BodyPitch( pStudioHdr );	// Torso elevation.
	ComputePoseParam_BodyYaw();		// Torso rotation.
	ComputePoseParam_MoveYaw( pStudioHdr );		// What direction his legs are running in.

	ComputePlaybackRate();

#ifdef CLIENT_DLL
	C_BasePlayer *targetPlayer = C_BasePlayer::GetLocalPlayer();

	if ( targetPlayer->ShouldDrawLocalPlayer() )
	{
		targetPlayer->SetPlaybackRate( 1.0f );
	}

	if ( cl_showanimstate.GetInt() == m_pOuter->entindex() )
	{
		DebugShowAnimStateFull( 5 );
	}
	else if ( cl_showanimstate.GetInt() == -2 )
	{
		if( targetPlayer && ( targetPlayer->GetObserverMode() == OBS_MODE_IN_EYE || targetPlayer->GetObserverMode() == OBS_MODE_CHASE ) )
		{
			C_BaseEntity *target = targetPlayer->GetObserverTarget();

			if( target && target->IsPlayer() )
			{
				targetPlayer = ToBasePlayer( target );
			}
		}

		if ( m_pOuter == targetPlayer )
		{
			DebugShowAnimStateFull( 6 );
		}
	}
#else
	if ( sv_showanimstate.GetInt() == m_pOuter->entindex() )
	{
		DebugShowAnimState( 20 );
	}
#endif
}

bool CBasePlayerAnimState::ShouldUpdateAnimState()
{
	// Don't update anim state if we're not visible
	if ( GetOuter()->IsEffectActive( EF_NODRAW ) )
		return false;

	// By default, don't update their animation state when they're dead because they're
	// either a ragdoll or they're not drawn.
#ifdef CLIENT_DLL
	if ( GetOuter()->IsDormant() )
		return false;
#endif

	return (GetOuter()->IsAlive() || m_bDying);
}

bool CBasePlayerAnimState::ShouldChangeSequences( void ) const
{
	return true;
}

void CBasePlayerAnimState::SetOuterPoseParameter( int iParam, float flValue )
{
	// Make sure to set all the history values too, otherwise the server can overwrite them.
	GetOuter()->SetPoseParameter( iParam, flValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : event - 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::DoAnimationEvent( int PlayerAnimEvent, int nData )
{
	//CBasePlayer *pPlayer = m_pOuter;
	switch( PlayerAnimEvent )
	{
	case PLAYER_JUMP:
	case PLAYER_SUPERJUMP:
		{
			// Jump.
			m_bJumping = true;
			m_bFirstJumpFrame = true;
			m_flJumpStartTime = gpGlobals->curtime;

			RestartMainSequence();

			break;
		}
	case PLAYER_ATTACK1:
	case PLAYER_ATTACK2:
		{
			// Weapon firing - super important! Uses a special function.
			m_bFiring = true;
			m_flFireStartTime = gpGlobals->curtime;
			break;
		}
	case PLAYER_RELOAD:
		{
			// Weapon reload - not important enough for a dedicated function, but still needed.
#ifndef CLIENT_DLL
			m_pOuter->RestartGesture( TranslateActivity( ACT_GESTURE_RELOAD ) );
#else
			
#endif
			break;
		}
	case PLAYER_DIE:
		{
			// Should be here - not supporting this yet!
			Assert( 0 );

			// Start playing the death animation
			m_bDying = true;
			RestartMainSequence();
			break;
		}
	case PLAYER_FALL:
		{
			// Hit a trigger_fall, start playing the falling death anim
			// Dont worry about dying, im gonna be gibbed at the bottom anyway
			m_bDying = true;
			RestartMainSequence();
			break;
		}
	default:
		break;
	}
}

void CBasePlayerAnimState::ClearAnimationLayers()
{
	VPROF( "CBasePlayerAnimState::ClearAnimationLayers" );
	if ( !m_pOuter )
		return;

	m_pOuter->SetNumAnimOverlays( AIMSEQUENCE_LAYER+NUM_AIMSEQUENCE_LAYERS );
	for ( int i=0; i < m_pOuter->GetNumAnimOverlays(); i++ )
	{
		m_pOuter->GetAnimOverlay( i )->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
#ifndef CLIENT_DLL
		m_pOuter->GetAnimOverlay( i )->m_fFlags = 0;
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : event - 
//-----------------------------------------------------------------------------
Activity CBasePlayerAnimState::TranslateActivity( Activity actDesired )
{
	Activity translatedActivity = actDesired;

	// Just keep the base activity if we're going to use the PREHISTORIC aim sequence system instead.
	if ( !m_AnimConfig.m_bUseAimSequences )
	{
		if ( m_pOuter->GetActiveWeapon() )
			translatedActivity = m_pOuter->GetActiveWeapon()->ActivityOverride( translatedActivity, false );
	}

	// 99% of the time this is just going to return the original activity
	return BodyYawTranslateActivity( translatedActivity );
}

void CBasePlayerAnimState::RestartMainSequence()
{
	CBaseAnimatingOverlay *pPlayer = GetOuter();

	pPlayer->m_flAnimTime = gpGlobals->curtime;
	pPlayer->SetCycle( 0 );
}

void CBasePlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	VPROF( "CBasePlayerAnimState::ComputeSequences" );

	ComputeMainSequence();		// Lower body (walk/run/idle).
	UpdateInterpolators();		// The groundspeed interpolator uses the main sequence info.

	if ( m_AnimConfig.m_bUseAimSequences )
	{
		ComputeAimSequence();		// Upper body, based on weapon type.
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CBasePlayerAnimState::CalcMainActivity()
{
	Activity idealActivity = ACT_IDLE;
	if ( m_bDying )
	{
		// NOTENOTE; NOT USED IN PLAYER.CPP
		// Dying is VERY important feedback, thus the server always handles the dying animation
		// Event_killed destroys the animstate (for caliber) anyway, rendering this unused
		// This is here for posterity only -MM
#ifdef CLIENT_DLL 
		// Client's not supposed to handle this anyway, but just in case, t-pose;
		return idealActivity;
#else
		// Use basecombatcharacter's GetDeathActivity();
		return m_pOuter->GetDeathActivity();
#endif
	}
	if ( m_bJumping )
	{
		idealActivity = ACT_JUMP;
		if ( m_bFirstJumpFrame )
			m_bFirstJumpFrame = false;

		// Don't check if he's on the ground for a sec.. sometimes the client still has the
		// on-ground flag set right when the message comes in.
		if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f )
		{
			if ( GetOuter()->GetFlags() & FL_ONGROUND )
			{
				m_bJumping = false;
				RestartMainSequence();	// Reset the animation.
			}
		}
	}
	else
	{
		if ( m_pOuter->GetFlags() & FL_DUCKING )
			idealActivity = ACT_CROUCHIDLE;

		float flSpeed = GetOuterXYSpeed();
		if ( flSpeed > MOVING_MINIMUM_SPEED )
		{
			if ( m_pOuter->GetFlags() & FL_DUCKING )
			{
				idealActivity = ACT_WALK_CROUCH;
			}
			else
			{
				if( flSpeed >= SPRINT_SPEED )
					idealActivity = ACT_SPRINT;
				else if( flSpeed >= RUN_SPEED )
					idealActivity = ACT_RUN;
				else
					idealActivity = ACT_WALK;
			}
		}
	}

//	ShowDebugInfo();

	return idealActivity;
}

//-----------------------------------------------------------------------------
const char* CBasePlayerAnimState::GetWeaponPrefix()
{
	CBaseCombatWeapon *pWeapon = m_pOuter->GetActiveWeapon();
	if ( pWeapon )
		return pWeapon->GetWpnData().szAnimationPrefix;	//Get the prefix
	else
		return "shotgun";	//Assume its underhanded shotgun style
}

int CBasePlayerAnimState::CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle )
{
#if 0
	const char *pPrefix = GetWeaponPrefix();
	if ( !pPrefix )
		return 0;

	if ( bForceIdle )
	{
		switch ( GetCurrentMainSequenceActivity() )
		{
			case ACT_CROUCHIDLE:
				return CalcSequenceIndex( "%s%s", DEFAULT_CROUCH_IDLE_NAME, pPrefix );

			default:
				return CalcSequenceIndex( "%s%s", DEFAULT_IDLE_NAME, pPrefix );
		}
	}
	else
	{
		switch ( GetCurrentMainSequenceActivity() )
		{
			case ACT_RUN:
				return CalcSequenceIndex( "%s%s", DEFAULT_RUN_NAME, pPrefix );

			case ACT_WALK:
			case ACT_RUNTOIDLE:
			case ACT_IDLETORUN:
				return CalcSequenceIndex( "%s%s", DEFAULT_WALK_NAME, pPrefix );

			case ACT_CROUCHIDLE:
				return CalcSequenceIndex( "%s%s", DEFAULT_CROUCH_IDLE_NAME, pPrefix );

			case ACT_RUN_CROUCH:
				return CalcSequenceIndex( "%s%s", DEFAULT_CROUCH_WALK_NAME, pPrefix );

			case ACT_IDLE:
			default:
				return CalcSequenceIndex( "%s%s", DEFAULT_IDLE_NAME, pPrefix );
		}
		return m_iCurrentAimSequence;
	}
#else
	const char *pWeaponPrefix = GetWeaponPrefix();
	if ( !pWeaponPrefix )
		return -1;

#ifdef HL1_DLL
	if ( strcmp( pWeaponPrefix, "pistol" ) == 0 )
		pWeaponPrefix = "onehanded";
#endif

	// Are we aiming or firing?
	const char *pAimOrShoot = "aim";
	if ( m_bFiring )
		pAimOrShoot = "shoot";

	// Are we standing or crouching?
	int iSequence = 0;
	const char *pPrefix = "ref";
	if ( m_bDying )
	{
		// While dying, only play the main sequence.. don't layer this one on top.
		*flAimSequenceWeight = 0;	
	}
	else
	{
		switch ( GetCurrentMainSequenceActivity() )
		{
			case ACT_CROUCHIDLE:
			case ACT_WALK_CROUCH:
			case ACT_RUN_CROUCH:
				pPrefix = "crouch";
				break;
		}
	}

	iSequence = CalcSequenceIndex( "%s_%s_%s", pPrefix, pAimOrShoot, pWeaponPrefix );
	
	// Check if we're done firing.
	if ( m_bFiring )
	{
		float dur = m_pOuter->SequenceDuration( iSequence );
		*flCycle = (gpGlobals->curtime - m_flFireStartTime) / dur;
		if ( *flCycle >= 1 )
		{
			*flCycle = 1;
			m_bFiring = false;
		}
	}

	return iSequence;
#endif
}


//-----------------------------------------------------------------------------
void CBasePlayerAnimState::ResetGroundSpeed( void )
{
	m_flMaxGroundSpeed = GetCurrentMaxGroundSpeed();
}

void CBasePlayerAnimState::ComputeMainSequence()
{
	VPROF( "CBasePlayerAnimState::ComputeMainSequence" );

	CBasePlayer *pPlayer = m_pOuter;

	// Have our class or the mod-specific class determine what the current activity is.
	Activity idealActivity = CalcMainActivity();

#ifdef CLIENT_DLL
	Activity oldActivity = m_eCurrentMainSequenceActivity;
#endif
	
	// Store our current activity so the aim and fire layers know what to do.
	m_eCurrentMainSequenceActivity = idealActivity;

	// Export to our outer class..
	int animDesired = SelectWeightedSequence( TranslateActivity(idealActivity) );

#if !defined( HL1_CLIENT_DLL ) && !defined ( HL1_DLL )
	if ( pPlayer->GetSequenceActivity( pPlayer->GetSequence() ) == pPlayer->GetSequenceActivity( animDesired ) )
		return;
#endif

	// The requested animation isnt available for the model, punt.
	if ( animDesired <= ACT_INVALID )
		 animDesired = ACT_IDLE;

	pPlayer->ResetSequence( animDesired );

#ifdef CLIENT_DLL
	// If we went from idle to walk, reset the interpolation history.
	// Kind of hacky putting this here.. it might belong outside the base class.
	if ( (oldActivity == ACT_CROUCHIDLE || oldActivity == ACT_IDLE) && 
		 (idealActivity == ACT_WALK || idealActivity == ACT_WALK_CROUCH) )
	{
		ResetGroundSpeed();
	}
#endif
}

//-----------------------------------------------------------------------------
void CBasePlayerAnimState::UpdateAimSequenceLayers(
	float flCycle,
	int iFirstLayer,
	bool bForceIdle,
	CSequenceTransitioner *pTransitioner,
	float flWeightScale
	)
{
	float flAimSequenceWeight = 1;
	int iAimSequence = CalcAimLayerSequence( &flCycle, &flAimSequenceWeight, bForceIdle );
	if ( iAimSequence == -1 )
		return;

	// Feed the current state of the animation parameters to the sequence transitioner.
	// It will hand back either 1 or 2 animations in the queue to set, depending on whether
	// it's transitioning or not. We just dump those into the animation layers.
	pTransitioner->CheckForSequenceChange(
		m_pOuter->GetModelPtr(),
		iAimSequence,
		false,	// don't force transitions on the same anim
		true	// yes, interpolate when transitioning
		);

	pTransitioner->UpdateCurrent(
		m_pOuter->GetModelPtr(),
		iAimSequence,
		flCycle,
		GetOuter()->GetPlaybackRate(),
		gpGlobals->curtime
		);

	CAnimationLayer *pDest0 = m_pOuter->GetAnimOverlay( iFirstLayer );
	CAnimationLayer *pDest1 = m_pOuter->GetAnimOverlay( iFirstLayer+1 );

	if ( pTransitioner->m_animationQueue.Count() == 1 )
	{
		// If only 1 animation, then blend it in fully.
		CAnimationLayer *pSource0 = &pTransitioner->m_animationQueue[0];
		*pDest0 = *pSource0;
		
		pDest0->m_flWeight = 1;
		pDest1->m_flWeight = 0;
		pDest0->m_nOrder = iFirstLayer;

#ifndef CLIENT_DLL
		pDest0->m_fFlags |= ANIM_LAYER_ACTIVE;
#endif
	}
	else if ( pTransitioner->m_animationQueue.Count() >= 2 )
	{
		// The first one should be fading out. Fade in the new one inversely.
		CAnimationLayer *pSource0 = &pTransitioner->m_animationQueue[0];
		CAnimationLayer *pSource1 = &pTransitioner->m_animationQueue[1];

		*pDest0 = *pSource0;
		*pDest1 = *pSource1;
		Assert( pDest0->m_flWeight >= 0.0f && pDest0->m_flWeight <= 1.0f );
		pDest1->m_flWeight = 1 - pDest0->m_flWeight;	// This layer just mirrors the other layer's weight (one fades in while the other fades out).

		pDest0->m_nOrder = iFirstLayer;
		pDest1->m_nOrder = iFirstLayer+1;

#ifndef CLIENT_DLL
		pDest0->m_fFlags |= ANIM_LAYER_ACTIVE;
		pDest1->m_fFlags |= ANIM_LAYER_ACTIVE;
#endif
	}
	
	pDest0->m_flWeight *= flWeightScale * flAimSequenceWeight;
	pDest0->m_flWeight = clamp( pDest0->m_flWeight, 0.0f, 1.0f );

	pDest1->m_flWeight *= flWeightScale * flAimSequenceWeight;
	pDest1->m_flWeight = clamp( pDest1->m_flWeight, 0.0f, 1.0f );

	pDest0->m_flCycle = pDest1->m_flCycle = flCycle;
}

void CBasePlayerAnimState::OptimizeLayerWeights( int iFirstLayer, int nLayers )
{
	int i;

	// Find the total weight of the blended layers, not including the idle layer (iFirstLayer)
	float totalWeight = 0.0f;
	for ( i=1; i < nLayers; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
		if ( pLayer->IsActive() && pLayer->m_flWeight > 0.0f )
		{
			totalWeight += pLayer->m_flWeight;
		}
	}

	// Set the idle layer's weight to be 1 minus the sum of other layer weights
	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer );
	if ( pLayer->IsActive() && pLayer->m_flWeight > 0.0f )
	{
		pLayer->m_flWeight = 1.0f - totalWeight;
		pLayer->m_flWeight = max(pLayer->m_flWeight, 0.0f);
	}

	// This part is just an optimization. Since we have the walk/run animations weighted on top of 
	// the idle animations, all this does is disable the idle animations if the walk/runs are at
	// full weighting, which is whenever a guy is at full speed.
	//
	// So it saves us blending a couple animation layers whenever a guy is walking or running full speed.
	int iLastOne = -1;
	for ( i=0; i < nLayers; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
		if ( pLayer->IsActive() && pLayer->m_flWeight > 0.99 )
			iLastOne = i;
	}

	if ( iLastOne != -1 )
	{
		for ( int i=iLastOne-1; i >= 0; i-- )
		{
			CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
#ifdef CLIENT_DLL 
			pLayer->m_nOrder = CBaseAnimatingOverlay::MAX_OVERLAYS;
#else
			pLayer->m_nOrder.Set( CBaseAnimatingOverlay::MAX_OVERLAYS );
			pLayer->m_fFlags = 0;
#endif
		}
	}
}

bool CBasePlayerAnimState::ShouldBlendAimSequenceToIdle()
{
	Activity act = GetCurrentMainSequenceActivity();

	return (act == ACT_RUN || act == ACT_WALK || act == ACT_RUN_CROUCH || act == ACT_WALK_CROUCH);
}

void CBasePlayerAnimState::ComputeAimSequence()
{
	VPROF( "CBasePlayerAnimState::ComputeAimSequence" );

	// Synchronize the lower and upper body cycles.
	float flCycle = m_pOuter->GetCycle();

	// Figure out the new cycle time.
	UpdateAimSequenceLayers( flCycle, AIMSEQUENCE_LAYER, true, &m_IdleSequenceTransitioner, 1 );
	
	if ( ShouldBlendAimSequenceToIdle() )
	{
		// What we do here is blend between the idle upper body animation (like where he's got the dual elites
		// held out in front of him but he's not moving) and his walk/run/crouchrun upper body animation,
		// weighting it based on how fast he's moving. That way, when he's moving slowly, his upper 
		// body doesn't jiggle all around.
		bool bIsMoving;
		float flPlaybackRate = CalcMovementPlaybackRate( &bIsMoving );
		if ( bIsMoving )
			UpdateAimSequenceLayers( flCycle, AIMSEQUENCE_LAYER+2, false, &m_SequenceTransitioner, flPlaybackRate );
	}

	OptimizeLayerWeights( AIMSEQUENCE_LAYER, NUM_AIMSEQUENCE_LAYERS );
}

//-----------------------------------------------------------------------------
int CBasePlayerAnimState::CalcSequenceIndex( const char *pBaseName, ... )
{
	char szFullName[512];
	va_list marker;
	va_start( marker, pBaseName );
	Q_vsnprintf( szFullName, sizeof( szFullName ), pBaseName, marker );
	va_end( marker );
	int iSequence = GetOuter()->LookupSequence( szFullName );
	
	// Show warnings if we can't find anything here.
	if ( iSequence == -1 )
	{
		static CUtlDict<int,int> dict;
		if ( dict.Find( szFullName ) == -1 )
		{
			dict.Insert( szFullName, 0 );
			Warning( "CalcSequenceIndex: can't find '%s'.\n", szFullName );
		}

		iSequence = 0;
	}

	return iSequence;
}

void CBasePlayerAnimState::UpdateInterpolators()
{
	VPROF( "CBasePlayerAnimState::UpdateInterpolators" );

	// First, figure out their current max speed based on their current activity.
	float flCurMaxSpeed = GetCurrentMaxGroundSpeed();
	m_flMaxGroundSpeed = flCurMaxSpeed;
}

float CBasePlayerAnimState::GetInterpolatedGroundSpeed()
{
	return m_flMaxGroundSpeed;
}

float CBasePlayerAnimState::CalcMovementPlaybackRate( bool *bIsMoving )
{
	// Determine ideal playback rate
	Vector vel;
	GetOuterAbsVelocity( vel );

	float flSpeed = vel.Length2D();
	bool bMoving = ( flSpeed > MOVING_MINIMUM_SPEED );

	*bIsMoving = false;
	float flReturnValue = 1.0f;

	if ( bMoving && CanThePlayerMove() )
	{
		float flGroundSpeed = GetInterpolatedGroundSpeed();
#ifdef INVASION_DLL
		if ( ( flGroundSpeed > 0.0f ) )
		{
			float flFactor = 1.0f;

			// HACK HACK:: Defender backward animation is animated at 0.6 times speed, so scale up animation for this class
			//  if he's running backward.

			// Not sure if we're really going to do all classes this way.
			if ( GetOuter()->IsClass( TFCLASS_DEFENDER ) ||
				 GetOuter()->IsClass( TFCLASS_MEDIC ) )
			{
				Vector facing;
				Vector moving;

				moving = vel;
				AngleVectors( GetOuter()->GetLocalAngles(), &facing );
				VectorNormalize( moving );

				float dot = moving.Dot( facing );
				if ( dot < 0.0f )
				{
					float backspeed = sv_backspeed.GetFloat();
					flFactor = 1.0f - fabs( dot ) * (1.0f - backspeed);

					if ( flFactor > 0.0f )
					{
						flFactor = 1.0f / flFactor;
					}
				}
			}

			// Note this gets set back to 1.0 if sequence changes due to ResetSequenceInfo below
			GetOuter()->SetPlaybackRate( ( flSpeed * flFactor ) / maxspeed );

			// BUG BUG:
			// This stuff really should be m_flPlaybackRate = speed / m_flGroundSpeed
		}
#endif
		// Walking is always animated at 1x
		if ( flGroundSpeed < 0.001f )
		{
			flReturnValue = 0.01;
		}
		else if ( flSpeed > RUN_SPEED )
		{
			// Note this gets set back to 1.0 if sequence changes due to ResetSequenceInfo below
			flReturnValue = flSpeed/flGroundSpeed;
			flReturnValue = clamp( flReturnValue, 0.4f, 1.8f );	// don't go nuts here.
		}
		*bIsMoving = true;
	}

	return flReturnValue;
}

bool CBasePlayerAnimState::CanThePlayerMove()
{
	return true;
}

void CBasePlayerAnimState::ComputePlaybackRate()
{
	VPROF( "CBasePlayerAnimState::ComputePlaybackRate" );
	if ( m_AnimConfig.m_LegAnimType != LEGANIM_9WAY )
	{
		// When using a 9-way blend, playback rate is always 1 and we just scale the pose params
		// to speed up or slow down the animation.
		bool bIsMoving;
		float flRate = CalcMovementPlaybackRate( &bIsMoving );
		if ( bIsMoving )
			GetOuter()->SetPlaybackRate( flRate );
		else
			GetOuter()->SetPlaybackRate( 1 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBasePlayer
//-----------------------------------------------------------------------------
CBaseAnimatingOverlay *CBasePlayerAnimState::GetOuter() const
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::EstimateYaw()
{
	Vector est_velocity;
	GetOuterAbsVelocity( est_velocity );

	float flLength = est_velocity.Length2D();
	if ( flLength > MOVING_MINIMUM_SPEED )
	{
		m_flGaitYaw = atan2( est_velocity[1], est_velocity[0] );
		m_flGaitYaw = RAD2DEG( m_flGaitYaw );
		m_flGaitYaw = AngleNormalize( m_flGaitYaw );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override for backpeddling
// Input  : dt - 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr )
{
	VPROF( "CBasePlayerAnimState::ComputePoseParam_MoveYaw" );

	//Matt: Goldsrc style animations need to not rotate the model
	if ( m_AnimConfig.m_LegAnimType == LEGANIM_GOLDSRC )
	{
#ifndef CLIENT_DLL
		//Adrian: Make the model's angle match the legs so the hitboxes match on both sides.
		GetOuter()->SetLocalAngles( QAngle( 0, m_flCurrentFeetYaw, 0 ) );
#endif
	}

	// If using goldsrc-style animations where he's moving in the direction that his feet are facing,
	// we don't use move yaw.
	if ( m_AnimConfig.m_LegAnimType != LEGANIM_9WAY && m_AnimConfig.m_LegAnimType != LEGANIM_8WAY )
		return;

	// view direction relative to movement
	float flYaw;	 

	EstimateYaw();

	float ang = m_flEyeYaw;
	if ( ang > 180.0f )
	{
		ang -= 360.0f;
	}
	else if ( ang < -180.0f )
	{
		ang += 360.0f;
	}

	// calc side to side turning
	flYaw = ang - m_flGaitYaw;
	// Invert for mapping into 8way blend
	flYaw = -flYaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;

	if (flYaw < -180)
	{
		flYaw = flYaw + 360;
	}
	else if (flYaw > 180)
	{
		flYaw = flYaw - 360;
	}

	if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY )
	{
#ifndef CLIENT_DLL
		//Adrian: Make the model's angle match the legs so the hitboxes match on both sides.
		GetOuter()->SetLocalAngles( QAngle( 0, m_flCurrentFeetYaw, 0 ) );
#endif

		int iMoveX = GetOuter()->LookupPoseParameter( pStudioHdr, "move_x" );
		int iMoveY = GetOuter()->LookupPoseParameter( pStudioHdr, "move_y" );
		if ( iMoveX < 0 || iMoveY < 0 )
			return;

		bool bIsMoving;
		float flPlaybackRate = CalcMovementPlaybackRate( &bIsMoving );

		// Setup the 9-way blend parameters based on our speed and direction.
		Vector2D vCurMovePose( 0, 0 );

		if ( bIsMoving )
		{
			if ( mp_slammoveyaw.GetBool() )
				flYaw = SnapYawTo( flYaw );

			vCurMovePose.x = cos( DEG2RAD( flYaw ) ) * flPlaybackRate;
			vCurMovePose.y = -sin( DEG2RAD( flYaw ) ) * flPlaybackRate;
		}

		GetOuter()->SetPoseParameter( pStudioHdr, iMoveX, vCurMovePose.x );
		GetOuter()->SetPoseParameter( pStudioHdr, iMoveY, vCurMovePose.y );

		m_vLastMovePose = vCurMovePose;
	}
	else
	{
		int iMoveYaw = GetOuter()->LookupPoseParameter( pStudioHdr, "move_yaw" );
		if ( iMoveYaw >= 0 )
		{
			GetOuter()->SetPoseParameter( pStudioHdr, iMoveYaw, flYaw );
			m_flLastMoveYaw = flYaw;

			// Now blend in his idle animation.
			// This makes the 8-way blend act like a 9-way blend by blending to 
			// an idle sequence as he slows down.
#if defined(CLIENT_DLL)
			bool bIsMoving;
			CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( MAIN_IDLE_SEQUENCE_LAYER );
			
			pLayer->m_flWeight = 1 - CalcMovementPlaybackRate( &bIsMoving );
			if ( !bIsMoving )
			{
				pLayer->m_flWeight = 1;
			}

			if ( ShouldChangeSequences() )
			{
				// Whenever this layer stops blending, we can choose a new idle sequence to blend to, so he 
				// doesn't always use the same idle.
				if ( pLayer->m_flWeight < 0.02f || m_iCurrent8WayIdleSequence == -1 )
				{
					m_iCurrent8WayIdleSequence = m_pOuter->SelectWeightedSequence( ACT_IDLE );
					m_iCurrent8WayCrouchIdleSequence = m_pOuter->SelectWeightedSequence( ACT_CROUCHIDLE );
				}

				if ( m_eCurrentMainSequenceActivity == ACT_CROUCHIDLE || m_eCurrentMainSequenceActivity == ACT_WALK_CROUCH )
					pLayer->m_nSequence = m_iCurrent8WayCrouchIdleSequence;
				else
					pLayer->m_nSequence = m_iCurrent8WayIdleSequence;
			}
			
			pLayer->m_flPlaybackRate = 1;
			pLayer->m_flCycle += m_pOuter->GetSequenceCycleRate( pStudioHdr, pLayer->m_nSequence ) * gpGlobals->frametime;
			pLayer->m_flCycle = fmod( pLayer->m_flCycle, 1 );
			pLayer->m_nOrder = MAIN_IDLE_SEQUENCE_LAYER;
#endif
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr )
{
	VPROF( "CBasePlayerAnimState::ComputePoseParam_BodyPitch" );

	// Get pitch from v_angle
	float flPitch = m_flEyePitch;	//GetOuter()->GetLocalAngles()[ PITCH ]
	if ( flPitch > 180.0f )
	{
		flPitch -= 360.0f;
	}
	flPitch = clamp( flPitch, -90, 90 );

	// See if we have a blender for pitch
	int pitch = GetOuter()->LookupPoseParameter( pStudioHdr, "body_pitch" );

	// Try aim pitch instead
	/*
	if ( pitch < 0 )
		pitch = GetOuter()->LookupPoseParameter( pStudioHdr, "aim_pitch" );

	// Test again
	if ( pitch < 0 )
		return;
	*/

	GetOuter()->SetPoseParameter( pStudioHdr, pitch, flPitch );
	g_flLastBodyPitch = flPitch;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : goal - 
//			maxrate - 
//			dt - 
//			current - 
// Output : int
//-----------------------------------------------------------------------------
int CBasePlayerAnimState::ConvergeAngles( float goal,float maxrate, float maxgap, float dt, float& current )
{
	int direction = TURN_NONE;

	float anglediff = goal - current;
	anglediff = AngleNormalize( anglediff );
	
	float anglediffabs = fabs( anglediff );

	float scale = 1.0f;
	if ( anglediffabs <= FADE_TURN_DEGREES )
	{
		scale = anglediffabs / FADE_TURN_DEGREES;
		// Always do at least a bit of the turn ( 1% )
		scale = clamp( scale, 0.01f, 1.0f );
	}

	float maxmove = maxrate * dt * scale;

	if ( anglediffabs > maxgap )
	{
		// gap is too big, jump
		maxmove = (anglediffabs - maxgap);
	}

	if ( anglediffabs < maxmove )
	{
		// we are close enought, just set the final value
		current = goal;
	}
	else
	{
		// adjust value up or down
		if ( anglediff > 0 )
		{
			current += maxmove;
			direction = TURN_LEFT;
		}
		else
		{
			current -= maxmove;
			direction = TURN_RIGHT;
		}
	}

	current = AngleNormalize( current );

	return direction;
}

void CBasePlayerAnimState::ComputePoseParam_BodyYaw()
{
	VPROF( "CBasePlayerAnimState::ComputePoseParam_BodyYaw" );

	// Find out which way he's running (m_flEyeYaw is the way he's looking).
	Vector vel;
	GetOuterAbsVelocity( vel );
	bool bIsMoving = vel.Length2D() > MOVING_MINIMUM_SPEED;

	// If we just initialized this guy (maybe he just came into the PVS), then immediately
	// set his feet in the right direction, otherwise they'll spin around from 0 to the 
	// right direction every time someone switches spectator targets.
	if ( !m_bCurrentFeetYawInitialized )
	{
		m_bCurrentFeetYawInitialized = true;
		m_flGoalFeetYaw = m_flCurrentFeetYaw = m_flEyeYaw;
		m_flLastTurnTime = 0.0f;
	}
	else if ( bIsMoving )
	{
		// player is moving, feet yaw = aiming yaw
		if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY || m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
		{
			// His feet point in the direction his eyes are, but they can run in any direction.
			m_flGoalFeetYaw = m_flEyeYaw;
		}
		else
		{
			m_flGoalFeetYaw = RAD2DEG( atan2( vel.y, vel.x ) );

			// If he's running backwards, flip his feet backwards.
			Vector vEyeYaw( cos( DEG2RAD( m_flEyeYaw ) ), sin( DEG2RAD( m_flEyeYaw ) ), 0 );
			Vector vFeetYaw( cos( DEG2RAD( m_flGoalFeetYaw ) ), sin( DEG2RAD( m_flGoalFeetYaw ) ), 0 );
			if ( vEyeYaw.Dot( vFeetYaw ) < -0.01 )
			{
				m_flGoalFeetYaw += 180;
			}
		}

	}
	else if ( (gpGlobals->curtime - m_flLastTurnTime) > mp_facefronttime.GetFloat() )
	{
		// player didn't move & turn for quite some time
		m_flGoalFeetYaw = m_flEyeYaw;
	}
	else
	{
		// If he's rotated his view further than the model can turn, make him face forward.
		float flDiff = AngleNormalize(  m_flGoalFeetYaw - m_flEyeYaw );

		if ( fabs(flDiff) > m_AnimConfig.m_flMaxBodyYawDegrees )
		{
			if ( flDiff  > 0 )
				m_flGoalFeetYaw -= m_AnimConfig.m_flMaxBodyYawDegrees;
			else
				m_flGoalFeetYaw += m_AnimConfig.m_flMaxBodyYawDegrees;
		}
	}

	m_flGoalFeetYaw = AngleNormalize( m_flGoalFeetYaw );

	if ( m_flCurrentFeetYaw != m_flGoalFeetYaw )
	{
		ConvergeAngles( m_flGoalFeetYaw, BODYYAW_RATE, m_AnimConfig.m_flMaxBodyYawDegrees,
			 gpGlobals->frametime, m_flCurrentFeetYaw );

		m_flLastTurnTime = gpGlobals->curtime;
		m_nTurningInPlace = 0;
	}

	float flCurrentTorsoYaw = AngleNormalize( m_flEyeYaw - m_flCurrentFeetYaw );

	SetOuterBodyYaw( flCurrentTorsoYaw );
	g_flLastBodyYaw = flCurrentTorsoYaw;
}



float CBasePlayerAnimState::SetOuterBodyYaw( float flValue )
{
	int body_yaw = GetOuter()->LookupPoseParameter( "body_yaw" );
	if ( body_yaw < 0 )
	{
		return 0;
	}

	SetOuterPoseParameter( body_yaw, flValue );
	return flValue;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CBasePlayerAnimState::BodyYawTranslateActivity( Activity activity )
{
	if ( mp_ik.GetBool() )
	{
		// Not even standing still, sigh
//		if ( activity != ACT_IDLE )
//			return activity;

		// Moving too fast
		if ( GetOuterXYSpeed() > MOVING_MINIMUM_SPEED )
			return activity;

		switch ( m_nTurningInPlace )
		{
		default:
		case TURN_NONE:
			return activity;

		case TURN_RIGHT:
			return ACT_TURN_RIGHT;
		case TURN_LEFT:
			return ACT_TURN_LEFT;
		case TURN_180:
			return ACT_TURN;
		}

		//Now see if the weapon we have has a entry
		//Activity newactivity = m_pOuter->GetActiveWeapon()->ActivityOverride( activity, NULL );
		//return newactivity;
	}

	return activity;
}

const QAngle& CBasePlayerAnimState::GetRenderAngles()
{
	return m_angRender;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : vel - 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::GetOuterAbsVelocity( Vector& vel ) const
{
#if defined( CLIENT_DLL )
	GetOuter()->EstimateAbsVelocity( vel );
#else
	vel = GetOuter()->GetAbsVelocity();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : float
//-----------------------------------------------------------------------------
float CBasePlayerAnimState::GetOuterXYSpeed() const
{
	Vector vel;
	GetOuterAbsVelocity( vel );
	return vel.Length2D();
}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::AnimStateLog( const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( str, sizeof( str ), pMsg, marker );
	va_end( marker );

	// Log it?	
	if ( showanimstate_log.GetInt() == 1 || showanimstate_log.GetInt() == 3 )
	{
		Msg( "%s", str );
	}

	if ( showanimstate_log.GetInt() > 1 )
	{
#ifdef CLIENT_DLL
		const char *fname = "AnimStateClient.log";
#else
		const char *fname = "AnimStateServer.log";
#endif
		static FileHandle_t hFile = filesystem->Open( fname, "wt" );
		filesystem->FPrintf( hFile, "%s", str );
		filesystem->Flush( hFile );
	}
}


// -----------------------------------------------------------------------------
void CBasePlayerAnimState::AnimStatePrintf( int iLine, const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( str, sizeof( str ), pMsg, marker );
	va_end( marker );

	// Show it with Con_NPrintf.
	engine->Con_NPrintf( iLine, "%s", str );

	// Log it.
	AnimStateLog( "%s\n", str );
}


// -----------------------------------------------------------------------------
void CBasePlayerAnimState::DebugShowAnimState( int iStartLine )
{
	Vector vOuterVel;
	GetOuterAbsVelocity( vOuterVel );

	int iLine = iStartLine;
	AnimStatePrintf( iLine++, "main: %s(%d), cycle: %.2f cyclerate: %.2f playbackrate: %.2f\n", 
		GetSequenceName( m_pOuter->GetModelPtr(), m_pOuter->GetSequence() ), 
		m_pOuter->GetSequence(),
		m_pOuter->GetCycle(), 
		m_pOuter->GetSequenceCycleRate(m_pOuter->GetModelPtr(), m_pOuter->GetSequence()),
		m_pOuter->GetPlaybackRate()
		);

	if ( m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( MAIN_IDLE_SEQUENCE_LAYER );

		AnimStatePrintf( iLine++, "idle: %s, weight: %.2f\n",
			GetSequenceName( m_pOuter->GetModelPtr(), pLayer->m_nSequence ), 
			(float)pLayer->m_flWeight );
	}

	for ( int i=0; i < m_pOuter->GetNumAnimOverlays()-1; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( AIMSEQUENCE_LAYER + i );
#ifdef CLIENT_DLL
		AnimStatePrintf( iLine++, "%s(%d), weight: %.2f, cycle: %.2f, order (%d), aim (%d)", 
			!pLayer->IsActive() ? "-- ": (pLayer->m_nSequence == 0 ? "-- " : GetSequenceName( m_pOuter->GetModelPtr(), pLayer->m_nSequence ) ), 
			!pLayer->IsActive() ? 0 : (int)pLayer->m_nSequence, 
			!pLayer->IsActive() ? 0 : (float)pLayer->m_flWeight, 
			!pLayer->IsActive() ? 0 : (float)pLayer->m_flCycle, 
			!pLayer->IsActive() ? 0 : (int)pLayer->m_nOrder,
			i
			);
#else
		AnimStatePrintf( iLine++, "%s(%d), flags (%d), weight: %.2f, cycle: %.2f, order (%d), aim (%d)", 
			!pLayer->IsActive() ? "-- " : ( pLayer->m_nSequence == 0 ? "-- " : GetSequenceName( m_pOuter->GetModelPtr(), pLayer->m_nSequence ) ), 
			!pLayer->IsActive() ? 0 : (int)pLayer->m_nSequence, 
			!pLayer->IsActive() ? 0 : (int)pLayer->m_fFlags,// Doesn't exist on client
			!pLayer->IsActive() ? 0 : (float)pLayer->m_flWeight, 
			!pLayer->IsActive() ? 0 : (float)pLayer->m_flCycle, 
			!pLayer->IsActive() ? 0 : (int)pLayer->m_nOrder,
			i
			);
#endif
	}

	AnimStatePrintf( iLine++, "vel: %.2f, time: %.2f, max: %.2f, animspeed: %.2f", 
		vOuterVel.Length2D(), gpGlobals->curtime, GetInterpolatedGroundSpeed(), m_pOuter->GetSequenceGroundSpeed(m_pOuter->GetSequence()) );
	
	if ( m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
	{
		AnimStatePrintf( iLine++, "ent yaw: %.2f, body_yaw: %.2f, move_yaw: %.2f, gait_yaw: %.2f, body_pitch: %.2f", 
			m_angRender[YAW], g_flLastBodyYaw, m_flLastMoveYaw, m_flGaitYaw, g_flLastBodyPitch );
	}
	else
	{
		AnimStatePrintf( iLine++, "ent yaw: %.2f, body_yaw: %.2f, body_pitch: %.2f, move_x: %.2f, move_y: %.2f", 
			m_angRender[YAW], g_flLastBodyYaw, g_flLastBodyPitch, m_vLastMovePose.x, m_vLastMovePose.y );
	}

	// Draw a red triangle on the ground for the eye yaw.
	float flBaseSize = 10;
	float flHeight = 80;
	Vector vBasePos = GetOuter()->GetAbsOrigin() + Vector( 0, 0, 3 );
	QAngle angles( 0, 0, 0 );
	angles[YAW] = m_flEyeYaw;
	Vector vForward, vRight, vUp;
	AngleVectors( angles, &vForward, &vRight, &vUp );
	debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 255, 0, 0, 255, false, 0.01 );

	// Draw a blue triangle on the ground for the body yaw.
	angles[YAW] = m_angRender[YAW];
	AngleVectors( angles, &vForward, &vRight, &vUp );
	debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 0, 0, 255, 255, false, 0.01 );

}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::DebugShowAnimStateFull( int iStartLine )
{
	AnimStateLog( "----------------- frame %d -----------------\n", gpGlobals->framecount );

	DebugShowAnimState( iStartLine );

	AnimStateLog( "--------------------------------------------\n\n" );
}

// -----------------------------------------------------------------------------
int CBasePlayerAnimState::SelectWeightedSequence( Activity activity ) 
{
	return GetOuter()->SelectWeightedSequence( activity ); 
}

