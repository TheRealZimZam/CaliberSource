//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: MP Animstate - adds gesture support for more versatile animation
//
//=============================================================================//
#include "cbase.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
#include "multiplayer_animstate.h"
#include "activitylist.h"

#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#include "engine/ivdebugoverlay.h"
#include "filesystem.h"
#include "eventlist.h"
ConVar anim_showmainactivity( "anim_showmainactivity", "0", FCVAR_CHEAT, "Show the idle, walk, run, and/or sprint activities." );
#else
#include "player.h"
#endif

extern ConVar mp_feetyawrate;
extern ConVar mp_facefronttime;

#define MOVING_MINIMUM_SPEED	0.5f

ConVar anim_showstate( "anim_showstate", "-1", FCVAR_CHEAT | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Show the (client) animation state for the specified entity (-1 for none)." );
ConVar anim_showstatelog( "anim_showstatelog", "0", FCVAR_CHEAT | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "1 to output anim_showstate to Msg(). 2 to store in AnimState.log. 3 for both." );
ConVar mp_showgestureslots( "mp_showgestureslots", "-1", FCVAR_CHEAT | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Show multiplayer client/server gesture slot information for the specified player index (-1 for no one)." );


CMultiPlayerAnimState* CreateMultiPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData )
{
	MDLCACHE_CRITICAL_SECTION();

	// Create animation state for this player.
	CMultiPlayerAnimState *pRet = new CMultiPlayerAnimState( pPlayer, movementData );
	pRet->Init( pPlayer, movementData );

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			&movementData - 
//-----------------------------------------------------------------------------
CMultiPlayerAnimState::CMultiPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData )
#ifdef CLIENT_DLL
	: m_iv_flMaxGroundSpeed( "CMultiPlayerAnimState::m_iv_flMaxGroundSpeed" )
#endif
{
	// Pose parameters.
	m_bPoseParameterInit = false;
	m_PoseParameterData.Init();
	m_DebugAnimData.Init();

	m_pPlayer = NULL;
	m_angRender.Init();

	m_bCurrentFeetYawInitialized = false;
	m_flLastAnimationStateClearTime = 0.0f;

	m_flEyeYaw = 0.0f;
	m_flEyePitch = 0.0f;
	m_flGoalFeetYaw = 0.0f;
	m_flCurrentFeetYaw = 0.0f;
	m_flLastAimTurnTime = 0.0f;

	// Jumping.
	m_bJumping = false;
	m_flJumpStartTime = 0.0f;	
	m_bFirstJumpFrame = false;

	// Swimming
	m_bInSwim = false;
	m_bFirstSwimFrame = true;

	// Dying
	m_bDying = false;
	m_bFirstDyingFrame = true;

	m_eCurrentMainSequenceActivity = ACT_INVALID;	
	m_nSpecificMainSequence = -1;

	// Weapon data.
	m_hActiveWeapon = NULL;

	// Ground speed interpolators.
#ifdef CLIENT_DLL
	m_iv_flMaxGroundSpeed.Setup( &m_flMaxGroundSpeed, LATCH_ANIMATION_VAR | INTERPOLATE_LINEAR_ONLY );
	m_flLastGroundSpeedUpdateTime = 0.0f;
#endif

	m_flMaxGroundSpeed = 0.0f;

	m_bForceAimYaw = false;

	// UNDONE; No, let the entity using this decide when to turn it on
	//Init( pPlayer, movementData );

	//!!!TODO; Broken! Crashing on startup!
	//!!!InitGestureSlots();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CMultiPlayerAnimState::~CMultiPlayerAnimState()
{
	ShutdownGestureSlots();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			&movementData - 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::Init( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData )
{
	// Get the player this animation data works on.
	m_pPlayer = pPlayer;

	CModAnimConfig config;
	config.m_flMaxBodyYawDegrees = 60;
	config.m_LegAnimType = LEGANIM_8WAY;
	config.m_bUseAimSequences = false;

	// Copy the movement data.
	memcpy( &m_MovementData, &movementData, sizeof( MultiPlayerMovementData_t ) );
	BaseClass::Init( pPlayer, config );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ClearAnimationState()
{
	// Reset state.
	//!!!TODO; Crashing!
	//!!!ResetGestureSlots();
	ClearAnimationLayers();

	m_bDying = false;

	BaseClass::ClearAnimationState();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eyeYaw - 
//			eyePitch - 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	// Profile the animation update.
	VPROF( "CMultiPlayerAnimState::Update" );

	// Get the studio header for the player.
	CStudioHdr *pStudioHdr = GetPlayer()->GetModelPtr();
	if ( !pStudioHdr )
		return;

	// Check to see if we should be updating the animation state - dead, ragdolled?
	if ( !ShouldUpdateAnimState() )
	{
		ClearAnimationState();
		return;
	}

	// Store the eye angles.
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
	if ( SetupPoseParameters( pStudioHdr ) )
	{
		ComputePoseParam_AimPitch( pStudioHdr );
	//	ComputePoseParam_AimYaw( pStudioHdr );
	}

	ComputePoseParam_BodyYaw();		// Torso rotation.
	ComputePoseParam_MoveYaw( pStudioHdr );		// What direction his legs are running in.

#ifdef CLIENT_DLL
	if ( C_BasePlayer::ShouldDrawLocalPlayer() )
	{
		GetPlayer()->SetPlaybackRate( 1.0f );
	}
#endif

	if( mp_showgestureslots.GetInt() == GetPlayer()->entindex() )
	{
		DebugGestureInfo();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMultiPlayerAnimState::SetupPoseParameters( CStudioHdr *pStudioHdr )
{
	// Check to see if this has already been done.
	if ( m_bPoseParameterInit )
		return true;

	// Save off the pose parameter indices.
	if ( !pStudioHdr )
		return false;

	if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY )
	{
		// Look for the movement blenders.
		m_PoseParameterData.m_iMoveX = GetPlayer()->LookupPoseParameter( pStudioHdr, "move_x" );
		m_PoseParameterData.m_iMoveY = GetPlayer()->LookupPoseParameter( pStudioHdr, "move_y" );
		if ( ( m_PoseParameterData.m_iMoveX < 0 ) || ( m_PoseParameterData.m_iMoveY < 0 ) )
			return false;
	}

	// Look for the aim pitch blender.
	m_PoseParameterData.m_iAimPitch = GetPlayer()->LookupPoseParameter( pStudioHdr, "aim_pitch" );
	if ( m_PoseParameterData.m_iAimPitch < 0 )
		return false;

	// Look for aim yaw blender.
	m_PoseParameterData.m_iAimYaw = GetPlayer()->LookupPoseParameter( pStudioHdr, "aim_yaw" );
	if ( m_PoseParameterData.m_iAimYaw < 0 )
		return false;

	m_bPoseParameterInit = true;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ComputePoseParam_AimPitch( CStudioHdr *pStudioHdr )
{
	// Get the view pitch.
	float flAimPitch = m_flEyePitch;
	if ( flAimPitch > 180.0f )
	{
		flAimPitch -= 360.0f;
	}
	flAimPitch = clamp( flAimPitch, -90, 90 );

	// Set the aim pitch pose parameter and save.
	GetPlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iAimPitch, -flAimPitch );
	m_DebugAnimData.m_flAimPitch = flAimPitch;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : const QAngle&
//-----------------------------------------------------------------------------
const QAngle& CMultiPlayerAnimState::GetRenderAngles()
{
#if defined( PORTAL ) && defined( CLIENT_DLL )
	C_Portal_Player *pPlayer = (C_Portal_Player *)GetBasePlayer();
	
	if( pPlayer )
	{		
		if( pPlayer->GetOriginInterpolator().GetInterpolatedTime( pPlayer->GetEffectiveInterpolationCurTime( gpGlobals->curtime ) ) < pPlayer->m_fLatestServerTeleport )
		{
			m_angRender_InterpHistory = TransformAnglesToWorldSpace( m_angRender, pPlayer->m_matLatestServerTeleportationInverseMatrix.As3x4() );
			return m_angRender_InterpHistory;
		}
	}
#endif

	return m_angRender;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : event - 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::DoAnimationEvent( int PlayerAnimEvent_mp, int nData )
{
	switch( PlayerAnimEvent_mp )
	{
	case PLAYERANIMEVENT_ATTACK_PRIMARY:
#ifdef HL1_DLL
	case PLAYERANIMEVENT_FIRE_GUN:
#endif
		{
			// Weapon primary fire.
			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_GESTURE_RANGE_ATTACK1 );
			break;
		}
	case PLAYERANIMEVENT_ATTACK_SECONDARY:
		{
			// Weapon secondary fire.
			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_GESTURE_RANGE_ATTACK2 );
			break;
		}
	case PLAYERANIMEVENT_ATTACK_GRENADE:
		{
			// Grenade throw.
			RestartGesture( GESTURE_SLOT_GRENADE, ACT_MELEE_ATTACK_SWING_GESTURE );
			break;
		}
	case PLAYERANIMEVENT_RELOAD:
		{
			// Weapon reload.
			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_GESTURE_RELOAD );
			break;
		}
	case PLAYERANIMEVENT_RELOAD_LOOP:
		{
			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_RELOAD_START );
			break;
		}
	case PLAYERANIMEVENT_RELOAD_END:
		{
			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_RELOAD_FINISH );
			break;
		}
	case PLAYERANIMEVENT_JUMP:
		{
			// Jump.
			m_bJumping = true;
			m_bFirstJumpFrame = true;
			m_flJumpStartTime = gpGlobals->curtime;

			RestartMainSequence();
			break;
		}
	case PLAYERANIMEVENT_DIE:
		{
			// Start playing the death animation
			m_bDying = true;

			RestartMainSequence();
			break;
		}
	case PLAYERANIMEVENT_SPAWN:
		{
			// Player has respawned. Clear flags.
			ClearAnimationState();
			break;
		}

	case PLAYERANIMEVENT_SNAP_YAW:
		m_PoseParameterData.m_flLastAimTurnTime = 0.0f;
		break;

	case PLAYERANIMEVENT_CUSTOM:
		{
			Activity iIdealActivity = TranslateActivity( (Activity)nData );
			m_nSpecificMainSequence = GetPlayer()->SelectWeightedSequence( iIdealActivity );
			RestartMainSequence();
		}
		break;

	case PLAYERANIMEVENT_CUSTOM_GESTURE:
		// Weapon primary fire.
		RestartGesture( GESTURE_SLOT_CUSTOM, (Activity)nData );
		break;

	case PLAYERANIMEVENT_CUSTOM_SEQUENCE:
		m_nSpecificMainSequence = nData;
		RestartMainSequence();
		break;

	case PLAYERANIMEVENT_CUSTOM_GESTURE_SEQUENCE:
		// Weapon primary fire.
//		RestartGestureSequence( nData, false );
		break;

	case PLAYERANIMEVENT_FLINCH_CHEST:
		PlayFlinchGesture( ACT_GESTURE_FLINCH_CHEST );
		break;
	case PLAYERANIMEVENT_FLINCH_HEAD:
		PlayFlinchGesture( ACT_GESTURE_FLINCH_HEAD );
		break;
	case PLAYERANIMEVENT_FLINCH_LEFTARM:
		PlayFlinchGesture( ACT_GESTURE_FLINCH_LEFTARM );
		break;
	case PLAYERANIMEVENT_FLINCH_RIGHTARM:
		PlayFlinchGesture( ACT_GESTURE_FLINCH_RIGHTARM );
		break;
	case PLAYERANIMEVENT_FLINCH_LEFTLEG:
		PlayFlinchGesture( ACT_GESTURE_FLINCH_LEFTLEG );
		break;
	case PLAYERANIMEVENT_FLINCH_RIGHTLEG:
		PlayFlinchGesture( ACT_GESTURE_FLINCH_RIGHTLEG );
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ShowDebugInfo( void )
{
	if ( anim_showstate.GetInt() == GetPlayer()->entindex() )
	{
		DebugShowAnimStateForPlayer( GetPlayer()->IsServer() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Cancel the current gesture and restart the main sequence.
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::RestartMainSequence( void )
{
	CBaseAnimatingOverlay *pPlayer = GetPlayer();
	if ( pPlayer )
	{
		pPlayer->m_flAnimTime = gpGlobals->curtime;
		pPlayer->SetCycle( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiPlayerAnimState::HandleJumping( Activity &idealActivity )
{
	if ( m_bJumping )
	{
		if ( m_bFirstJumpFrame )
		{
			m_bFirstJumpFrame = false;
			RestartMainSequence();	// Reset the animation.
		}

		// Check to see if we hit water and stop jumping animation.
		if ( GetPlayer()->GetWaterLevel() >= WL_Waist )
		{
			m_bJumping = false;
			RestartMainSequence();
		}
		// Don't check if he's on the ground for a sec.. sometimes the client still has the
		// on-ground flag set right when the message comes in.
		else if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f )
		{
			if ( GetPlayer()->GetFlags() & FL_ONGROUND )
			{
				m_bJumping = false;
				RestartMainSequence();
			}
		}
	}
	if ( m_bJumping )
	{
		idealActivity = ACT_JUMP;
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiPlayerAnimState::HandleSwimming( Activity &idealActivity )
{
	if ( GetPlayer()->GetWaterLevel() >= WL_Waist )
	{
		if ( m_bFirstSwimFrame )
		{
			// Reset the animation.
			RestartMainSequence();	
			m_bFirstSwimFrame = false;
		}

		if ( GetOuterXYSpeed() > MOVING_MINIMUM_SPEED )
		{
			idealActivity = ACT_SWIM;
		}
		else
		{
			idealActivity = ACT_HOVER;
		}
		m_bInSwim = true;
		return true;
	}
	else
	{
		m_bInSwim = false;

		if ( !m_bFirstSwimFrame )
		{
			m_bFirstSwimFrame = true;
		}
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiPlayerAnimState::HandleDying( Activity &idealActivity )
{
	if ( m_bDying )
	{
		if ( m_bFirstDyingFrame )
		{
			// Reset the animation.
			RestartMainSequence();	
			m_bFirstDyingFrame = false;
		}

		idealActivity = ACT_DIESIMPLE;
		return true;
	}
	else
	{
		if ( !m_bFirstDyingFrame )
		{
			m_bFirstDyingFrame = true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiPlayerAnimState::HandleMoving( Activity &idealActivity )
{
	// In TF we run all the time now.
	float flSpeed = GetOuterXYSpeed();
	if ( flSpeed > MOVING_MINIMUM_SPEED )
	{
		if ( GetPlayer()->GetFlags() & FL_DUCKING )
		{
#if defined( TF_DLL )
			idealActivity = ACT_RUN_CROUCH;
#else
			if( flSpeed >= GetRunSpeed() )
				idealActivity = ACT_RUN_CROUCH;
			else
				idealActivity = ACT_WALK_CROUCH;
#endif
		}
		else
		{
#if defined( TF_DLL )
			idealActivity = ACT_RUN;
#else
			if( flSpeed >= GetRunSpeed() )
				idealActivity = ACT_RUN;
			else
				idealActivity = ACT_WALK;
#endif
		}

		// No sprint anims in TF
#if !defined( TF_DLL )
		if( flSpeed >= GetSprintSpeed() )
			idealActivity = ACT_SPRINT;
#endif
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CMultiPlayerAnimState::CalcMainActivity()
{
	Activity idealActivity = ACT_IDLE;
	if ( GetPlayer()->GetFlags() & FL_DUCKING )
		idealActivity = ACT_CROUCHIDLE;

	if ( HandleJumping( idealActivity ) || 
		HandleSwimming( idealActivity ) || 
		HandleDying( idealActivity ) )
	{
		// intentionally blank
	}
	else
	{
		HandleMoving( idealActivity );
	}

	ShowDebugInfo();

	// Client specific.
#ifdef CLIENT_DLL
	if ( anim_showmainactivity.GetBool() )
	{
		DebugShowActivity( idealActivity );
	}
#endif

	return idealActivity;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actDesired - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CMultiPlayerAnimState::TranslateActivity( Activity actDesired )
{
	return BaseClass::TranslateActivity(actDesired);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : float
//-----------------------------------------------------------------------------
float CMultiPlayerAnimState::GetCurrentMaxGroundSpeed()
{
	if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY )
	{
		CStudioHdr *pStudioHdr = GetPlayer()->GetModelPtr();

		if ( pStudioHdr == NULL )
			return 1.0f;

		float prevX = GetPlayer()->GetPoseParameter( m_PoseParameterData.m_iMoveX );
		float prevY = GetPlayer()->GetPoseParameter( m_PoseParameterData.m_iMoveY );

		float d = sqrt( prevX * prevX + prevY * prevY );
		float newX, newY;
		if ( d == 0.0 )
		{
			newX = 1.0;
			newY = 0.0;
		}
		else
		{
			newX = prevX / d;
			newY = prevY / d;
		}

		GetPlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveX, newX );
		GetPlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, newY );

		float speed = GetPlayer()->GetSequenceGroundSpeed( GetPlayer()->GetSequence() );

		GetPlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveX, prevX );
		GetPlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, prevY );

		return speed;
	}
	else
	{
		return BaseClass::GetCurrentMaxGroundSpeed();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CMultiPlayerAnimState::GetInterpolatedGroundSpeed( void )
{
	return m_flMaxGroundSpeed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pStudioHdr - 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	VPROF( "CMultiPlayerAnimState::ComputeSequences" );

	//!!!TODO; Crashing!
	//!!!ComputeGestureSequence( pStudioHdr );
	BaseClass::ComputeSequences( pStudioHdr );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ComputeMainSequence()
{
	VPROF( "CMultiPlayerAnimState::ComputeMainSequence" );

	CBaseAnimatingOverlay *pPlayer = GetPlayer();

	// Have our class or the mod-specific class determine what the current activity is.
	Activity idealActivity = CalcMainActivity();

#ifdef CLIENT_DLL
	Activity oldActivity = m_eCurrentMainSequenceActivity;
#endif
	
	// Store our current activity so the aim and fire layers know what to do.
	m_eCurrentMainSequenceActivity = idealActivity;

	// Hook to force playback of a specific requested full-body sequence
	if ( m_nSpecificMainSequence >= 0 )
	{
		if ( pPlayer->GetSequence() != m_nSpecificMainSequence )
		{
			pPlayer->ResetSequence( m_nSpecificMainSequence );
			ResetGroundSpeed();
			return;
		}
		 
		if ( !pPlayer->IsSequenceFinished() )
			return;

		m_nSpecificMainSequence = -1;
		RestartMainSequence();
		ResetGroundSpeed();
	}

	// Export to our outer class..
	int animDesired = SelectWeightedSequence( TranslateActivity( idealActivity ) );
	if ( pPlayer->GetSequenceActivity( pPlayer->GetSequence() ) == pPlayer->GetSequenceActivity( animDesired ) )
		return;

	if ( animDesired < 0 )
		 animDesired = 0;

	pPlayer->ResetSequence( animDesired );

#ifdef CLIENT_DLL
	// If we went from idle to walk, reset the interpolation history.
	// Kind of hacky putting this here.. it might belong outside the base class.
	if ( (oldActivity == ACT_CROUCHIDLE || oldActivity == ACT_IDLE || oldActivity == ACT_ARM || oldActivity == ACT_COVER_LOW ) && 
		 (idealActivity == ACT_WALK || idealActivity == ACT_WALK_CROUCH ) )
	{
		ResetGroundSpeed();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ResetGroundSpeed( void )
{
		m_flMaxGroundSpeed = GetCurrentMaxGroundSpeed();
#ifdef CLIENT_DLL
		m_iv_flMaxGroundSpeed.Reset();
		m_iv_flMaxGroundSpeed.NoteChanged( gpGlobals->curtime, 0, false );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::UpdateInterpolators()
{
	VPROF( "CMultiPlayerAnimState::UpdateInterpolators" );

	// First, figure out their current max speed based on their current activity.
	float flCurMaxSpeed = GetCurrentMaxGroundSpeed();

/*
#ifdef CLIENT_DLL
	float flGroundSpeedInterval = 0.1;

	// Only update this 10x/sec so it has an interval to interpolate over.
	if ( gpGlobals->curtime - m_flLastGroundSpeedUpdateTime >= flGroundSpeedInterval )
	{
		m_flLastGroundSpeedUpdateTime = gpGlobals->curtime;

		m_flMaxGroundSpeed = flCurMaxSpeed;
		m_iv_flMaxGroundSpeed.NoteChanged( gpGlobals->curtime, flGroundSpeedInterval, false );
	}

	m_iv_flMaxGroundSpeed.Interpolate( gpGlobals->curtime, flGroundSpeedInterval );
#else
*/
	m_flMaxGroundSpeed = flCurMaxSpeed;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ComputeFireSequence( void )
{
#ifdef CLIENT_DLL
	//UpdateLayerSequenceGeneric( pStudioHdr, FIRESEQUENCE_LAYER, m_bFiring, m_flFireCycle, m_iFireSequence, false );
#else
	// Server doesn't bother with different fire sequences.
#endif
}

void CMultiPlayerAnimState::UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd, float flWeight /* = 1.0 */ )
{
	if ( !bEnabled )
		return;

	if( flCurCycle > 1.0 )
		flCurCycle = 1.0;

	// Increment the fire sequence's cycle.
	flCurCycle += GetPlayer()->GetSequenceCycleRate( pStudioHdr, iSequence ) * gpGlobals->frametime;
	if ( flCurCycle > 1 )
	{
		if ( bWaitAtEnd )
		{
			flCurCycle = 1;
		}
		else
		{
			// Not firing anymore.
			bEnabled = false;
			iSequence = 0;
			return;
		}
	}

	CAnimationLayer *pLayer = GetPlayer()->GetAnimOverlay( iLayer );

	pLayer->m_flCycle = flCurCycle;
	pLayer->m_nSequence = iSequence;

	pLayer->m_flPlaybackRate = 1.0;
	pLayer->m_flWeight = flWeight;
	pLayer->m_nOrder = iLayer;
}

//=============================================================================
//
// Multiplayer gesture code.
//

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::PlayFlinchGesture( Activity iActivity )
{
	if ( !IsGestureSlotActive( GESTURE_SLOT_FLINCH ) )
	{
		// See if we have the custom flinch. If not, revert to chest
		if ( iActivity != ACT_GESTURE_FLINCH_CHEST && GetPlayer()->SelectWeightedSequence( iActivity ) == -1 )
		{
			RestartGesture( GESTURE_SLOT_FLINCH, ACT_GESTURE_FLINCH_CHEST );
		}
		else
		{
			RestartGesture( GESTURE_SLOT_FLINCH, iActivity );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMultiPlayerAnimState::InitGestureSlots( void )
{
	// Get the base player.
	CBasePlayer *pPlayer = GetPlayer();
	if( pPlayer )
	{
		// Set the number of animation overlays we will use.
		pPlayer->SetNumAnimOverlays( GESTURE_SLOT_COUNT );
	}

	// Setup the number of gesture slots. 
	m_aGestureSlots.AddMultipleToTail( GESTURE_SLOT_COUNT );
	for ( int iGesture = 0; iGesture < GESTURE_SLOT_COUNT; ++iGesture )
	{
		m_aGestureSlots[iGesture].m_pAnimLayer = pPlayer->GetAnimOverlay( iGesture );
		if ( !m_aGestureSlots[iGesture].m_pAnimLayer )
			return false;

		ResetGestureSlot( iGesture );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ShutdownGestureSlots( void )
{
	// Clean up the gesture slots.
	m_aGestureSlots.Purge();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ResetGestureSlots( void )
{
	// Clear out all the gesture slots.
	for ( int iGesture = 0; iGesture < GESTURE_SLOT_COUNT; ++iGesture )
	{
		ResetGestureSlot( iGesture );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ResetGestureSlot( int iGestureSlot )
{
	// Sanity Check
	Assert( iGestureSlot >= 0 && iGestureSlot < GESTURE_SLOT_COUNT );

	GestureSlot_t *pGestureSlot = &m_aGestureSlots[iGestureSlot];
	if ( pGestureSlot )
	{
#ifdef CLIENT_DLL
		// briefly set to 1.0 so we catch the events, before we reset the slot
		pGestureSlot->m_pAnimLayer->m_flCycle = 1.0;

		RunGestureSlotAnimEventsToCompletion( pGestureSlot );
#endif

		pGestureSlot->m_iGestureSlot = GESTURE_SLOT_INVALID;
		pGestureSlot->m_iActivity = ACT_INVALID;
		pGestureSlot->m_bAutoKill = false;
		pGestureSlot->m_bActive = false;
		if ( pGestureSlot->m_pAnimLayer )
		{
			pGestureSlot->m_pAnimLayer->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
#ifdef CLIENT_DLL
			pGestureSlot->m_pAnimLayer->Reset();
#endif
		}
	}
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::RunGestureSlotAnimEventsToCompletion( GestureSlot_t *pGesture )
{
	CBasePlayer *pPlayer = GetPlayer();
	if( !pPlayer )
		return;

	// Get the studio header for the player.
	CStudioHdr *pStudioHdr = pPlayer->GetModelPtr();
	if ( !pStudioHdr )
		return;

	// Do all the anim events between previous cycle and 1.0, inclusive
	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( pGesture->m_pAnimLayer->m_nSequence );
	if ( seqdesc.numevents > 0 )
	{
		mstudioevent_t *pevent = seqdesc.pEvent( 0 );

		for (int i = 0; i < (int)seqdesc.numevents; i++)
		{
			if ( pevent[i].type & AE_TYPE_NEWEVENTSYSTEM )
			{
				if ( !( pevent[i].type & AE_TYPE_CLIENT ) )
					continue;
			}
			else if ( pevent[i].event < 5000 ) //Adrian - Support the old event system
				continue;

			if ( pevent[i].cycle > pGesture->m_pAnimLayer->m_flPrevCycle &&
				pevent[i].cycle <= pGesture->m_pAnimLayer->m_flCycle )
			{
				pPlayer->FireEvent( pPlayer->GetAbsOrigin(), pPlayer->GetAbsAngles(), pevent[ i ].event, pevent[ i ].pszOptions() );
			}
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMultiPlayerAnimState::IsGestureSlotActive( int iGestureSlot )
{
	// Sanity Check
	Assert( iGestureSlot >= 0 && iGestureSlot < GESTURE_SLOT_COUNT );
	return m_aGestureSlots[iGestureSlot].m_bActive;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMultiPlayerAnimState::IsGestureSlotPlaying( int iGestureSlot, Activity iGestureActivity )
{
	// Sanity Check
	Assert( iGestureSlot >= 0 && iGestureSlot < GESTURE_SLOT_COUNT );

	// Check to see if the slot is active.
	if ( !IsGestureSlotActive( iGestureSlot ) )
		return false;

	return ( m_aGestureSlots[iGestureSlot].m_iActivity == iGestureActivity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::RestartGesture( int iGestureSlot, Activity iGestureActivity, bool bAutoKill )
{
#if 0	//FIXME!!
	// Sanity Check
	Assert( iGestureSlot >= 0 && iGestureSlot < GESTURE_SLOT_COUNT );
	
	if ( !IsGestureSlotPlaying( iGestureSlot, iGestureActivity ) )
	{
#ifdef CLIENT_DLL
		if ( IsGestureSlotActive( iGestureSlot ) )
		{
			GestureSlot_t *pGesture = &m_aGestureSlots[iGestureSlot];
			if ( pGesture && pGesture->m_pAnimLayer )
			{
				pGesture->m_pAnimLayer->m_flCycle = 1.0; // run until the end
				RunGestureSlotAnimEventsToCompletion( &m_aGestureSlots[iGestureSlot] );
			}
		}
#endif

		Activity iIdealGestureActivity = TranslateActivity( iGestureActivity );
		AddToGestureSlot( iGestureSlot, iIdealGestureActivity, bAutoKill );
		return;
	}

	// Reset the cycle = restart the gesture.
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flCycle = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flPrevCycle = 0.0f;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::AddToGestureSlot( int iGestureSlot, Activity iGestureActivity, bool bAutoKill )
{
	// Sanity Check
	Assert( iGestureSlot >= 0 && iGestureSlot < GESTURE_SLOT_COUNT );

	CBasePlayer *pPlayer = GetPlayer();
	if ( !pPlayer )
		return;

	// Make sure we have a valid animation layer to fill out.
	if ( !m_aGestureSlots[iGestureSlot].m_pAnimLayer )
		return;

	// Get the sequence.
	int iGestureSequence = pPlayer->SelectWeightedSequence( iGestureActivity );
	if ( iGestureSequence <= 0 )
		return;

#ifdef CLIENT_DLL 

	// Setup the gesture.
	m_aGestureSlots[iGestureSlot].m_iGestureSlot = iGestureSlot;
	m_aGestureSlots[iGestureSlot].m_iActivity = iGestureActivity;
	m_aGestureSlots[iGestureSlot].m_bAutoKill = bAutoKill;
	m_aGestureSlots[iGestureSlot].m_bActive = true;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nSequence = iGestureSequence;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nOrder = iGestureSlot;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flWeight = 1.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flPlaybackRate = 1.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flCycle = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flPrevCycle = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flLayerAnimtime = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flLayerFadeOuttime = 0.0f;

	pPlayer->m_flOverlayPrevEventCycle[iGestureSlot] = -1.0;

#else

	// Setup the gesture.
	m_aGestureSlots[iGestureSlot].m_iGestureSlot = iGestureSlot;
	m_aGestureSlots[iGestureSlot].m_iActivity = iGestureActivity;
	m_aGestureSlots[iGestureSlot].m_bAutoKill = bAutoKill;
	m_aGestureSlots[iGestureSlot].m_bActive = true;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nActivity = iGestureActivity;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nOrder = iGestureSlot;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nPriority = 0;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flCycle = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flPrevCycle = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flPlaybackRate = 1.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nActivity = iGestureActivity;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nSequence = iGestureSequence;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flWeight = 1.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flBlendIn = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flBlendOut = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_bSequenceFinished = false;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flLastEventCheck = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flLastEventCheck = gpGlobals->curtime;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_bLooping = false;//( ( GetSequenceFlags( GetModelPtr(), iGestureSequence ) & STUDIO_LOOPING ) != 0);
	if ( bAutoKill )
	{
		m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_fFlags |= ANIM_LAYER_AUTOKILL;
	}
	else
	{
		m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_fFlags &= ~ANIM_LAYER_AUTOKILL;
	}
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_fFlags |= ANIM_LAYER_ACTIVE;

#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::AddVCDSequenceToGestureSlot( int iGestureSlot, int iGestureSequence, bool bAutoKill )
{
	// Sanity Check
	Assert( iGestureSlot >= 0 && iGestureSlot < GESTURE_SLOT_COUNT );

	CBasePlayer *pPlayer = GetPlayer();
	if ( !pPlayer )
		return;

	// Make sure we have a valid animation layer to fill out.
	if ( !m_aGestureSlots[iGestureSlot].m_pAnimLayer )
		return;

	// Set the activity.
	Activity iGestureActivity = ACT_GESTURE_USE;

#ifdef CLIENT_DLL 

	// Setup the gesture.
	m_aGestureSlots[iGestureSlot].m_iGestureSlot = iGestureSlot;
	m_aGestureSlots[iGestureSlot].m_iActivity = iGestureActivity;
	m_aGestureSlots[iGestureSlot].m_bAutoKill = bAutoKill;
	m_aGestureSlots[iGestureSlot].m_bActive = true;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nSequence = iGestureSequence;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nOrder = iGestureSlot;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flWeight = 1.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flPlaybackRate = 1.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flCycle = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flPrevCycle = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flLayerAnimtime = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flLayerFadeOuttime = 0.0f;

	pPlayer->m_flOverlayPrevEventCycle[iGestureSlot] = -1.0;

#else

	// Setup the gesture.
	m_aGestureSlots[iGestureSlot].m_iGestureSlot = iGestureSlot;
	m_aGestureSlots[iGestureSlot].m_iActivity = iGestureActivity;
	m_aGestureSlots[iGestureSlot].m_bAutoKill = bAutoKill;
	m_aGestureSlots[iGestureSlot].m_bActive = true;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nActivity = iGestureActivity;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nOrder = iGestureSlot;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nPriority = 0;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flCycle = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flPrevCycle = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flPlaybackRate = 1.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nActivity = iGestureActivity;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_nSequence = iGestureSequence;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flWeight = 1.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flBlendIn = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flBlendOut = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_bSequenceFinished = false;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flLastEventCheck = 0.0f;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_flLastEventCheck = gpGlobals->curtime;
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_bLooping = false;//( ( GetSequenceFlags( GetModelPtr(), iGestureSequence ) & STUDIO_LOOPING ) != 0);
	if ( bAutoKill )
	{
		m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_fFlags |= ANIM_LAYER_AUTOKILL;
	}
	else
	{
		m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_fFlags &= ~ANIM_LAYER_AUTOKILL;
	}
	m_aGestureSlots[iGestureSlot].m_pAnimLayer->m_fFlags |= ANIM_LAYER_ACTIVE;

#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pStudioHdr - 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::ComputeGestureSequence( CStudioHdr *pStudioHdr )
{
	// Update all active gesture layers.
	for ( int iGesture = 0; iGesture < GESTURE_SLOT_COUNT; ++iGesture )
	{
		if ( !m_aGestureSlots[iGesture].m_bActive )
			continue;

		UpdateGestureLayer( pStudioHdr, &m_aGestureSlots[iGesture] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::UpdateGestureLayer( CStudioHdr *pStudioHdr, GestureSlot_t *pGesture )
{
	// Sanity check.
	if ( !pStudioHdr || !pGesture )
		return;

	CBasePlayer *pPlayer = GetPlayer();
	if( !pPlayer )
		return;

#ifdef CLIENT_DLL 

	// Get the current cycle.
	float flCycle = pGesture->m_pAnimLayer->m_flCycle;
	flCycle += pPlayer->GetSequenceCycleRate( pStudioHdr, pGesture->m_pAnimLayer->m_nSequence ) * gpGlobals->frametime;

	pGesture->m_pAnimLayer->m_flPrevCycle =	pGesture->m_pAnimLayer->m_flCycle;
	pGesture->m_pAnimLayer->m_flCycle = flCycle;

	if( flCycle > 1.0f )
	{
		RunGestureSlotAnimEventsToCompletion( pGesture );

		if ( pGesture->m_bAutoKill )
		{
			ResetGestureSlot( pGesture->m_iGestureSlot );
			return;
		}
		else
		{
			pGesture->m_pAnimLayer->m_flCycle = 1.0f;
		}
	}

#else

	if ( pGesture->m_iActivity != ACT_INVALID && pGesture->m_pAnimLayer->m_nActivity == ACT_INVALID )
	{
		ResetGestureSlot( pGesture->m_iGestureSlot );
	}

#endif
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Anim_StateLog( const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( str, sizeof( str ), pMsg, marker );
	va_end( marker );

	// Log it?	
	if ( anim_showstatelog.GetInt() == 1 || anim_showstatelog.GetInt() == 3 )
	{
		Msg( "%s", str );
	}

	if ( anim_showstatelog.GetInt() > 1 )
	{
//		static FileHandle_t hFile = filesystem->Open( "AnimState.log", "wt" );
//		filesystem->FPrintf( hFile, "%s", str );
//		filesystem->Flush( hFile );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Anim_StatePrintf( int iLine, const char *pMsg, ... )
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
	Anim_StateLog( "%s\n", str );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::DebugShowAnimStateForPlayer( bool bIsServer )
{
	// Get the player's velocity.
	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );

	// Start animation state logging.
	int iLine = 5;
	if ( bIsServer )
	{
		iLine = 12;
	}
//	Anim_StateLog( "-------------%s: frame %d -----------------\n", bIsServer ? "Server" : "Client", gpGlobals->framecount );
	Anim_StatePrintf( iLine++, "-------------%s: frame %d -----------------\n", bIsServer ? "Server" : "Client", gpGlobals->framecount );

	// Write out the main sequence and its data.
	Anim_StatePrintf( iLine++, "Main: %s, Cycle: %.2f\n", GetSequenceName( GetPlayer()->GetModelPtr(), GetPlayer()->GetSequence() ), GetPlayer()->GetCycle() );

#if 0
	if ( m_bPlayingGesture )
	{
		Anim_StatePrintf( iLine++, "Gesture: %s, Cycle: %.2f\n",
			GetSequenceName( GetPlayer()->GetModelPtr(), m_iGestureSequence ),
			m_flGestureCycle );
	}
#endif

	// Write out the layers and their data.
	for ( int iAnim = 0; iAnim < GetPlayer()->GetNumAnimOverlays(); ++iAnim )
	{
#ifdef CLIENT_DLL
		C_AnimationLayer *pLayer = GetPlayer()->GetAnimOverlay( iAnim );
		if ( pLayer && ( pLayer->m_nOrder != CBaseAnimatingOverlay::MAX_OVERLAYS ) )
		{
			Anim_StatePrintf( iLine++, "Layer %s: Weight: %.2f, Cycle: %.2f", GetSequenceName( GetPlayer()->GetModelPtr(), pLayer->m_nSequence ), pLayer->m_flWeight, pLayer->m_flCycle );
		}
#else
		CAnimationLayer *pLayer = GetPlayer()->GetAnimOverlay( iAnim );
		if ( pLayer && ( pLayer->m_nOrder != CBaseAnimatingOverlay::MAX_OVERLAYS ) )
		{
			Anim_StatePrintf( iLine++, "Layer %s: Weight: %.2f, Cycle: %.2f", GetSequenceName( GetPlayer()->GetModelPtr(), pLayer->m_nSequence ), pLayer->m_flWeight, pLayer->m_flCycle );
		}
#endif
	}

	// Write out the speed data.
	Anim_StatePrintf( iLine++, "Time: %.2f, Speed: %.2f, MaxSpeed: %.2f", gpGlobals->curtime, vecVelocity.Length2D(), GetCurrentMaxGroundSpeed() );

	// Write out the 9-way blend data.
	Anim_StatePrintf( iLine++, "EntityYaw: %.2f, AimYaw: %.2f, AimPitch: %.2f, MoveX: %.2f, MoveY: %.2f", m_angRender[YAW], m_DebugAnimData.m_flAimYaw, m_DebugAnimData.m_flAimPitch, m_DebugAnimData.m_vecMoveYaw.x, m_DebugAnimData.m_vecMoveYaw.y );

//	Anim_StateLog( "--------------------------------------------\n\n" );
	Anim_StatePrintf( iLine++, "--------------------------------------------\n\n" );

	DebugShowEyeYaw();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::DebugShowEyeYaw( void )
{
#ifdef _NDEBUG

	float flBaseSize = 10;
	float flHeight = 80;

	Vector vecPos = GetOuter()->GetAbsOrigin() + Vector( 0.0f, 0.0f, 3.0f );
	QAngle angles( 0.0f, 0.0f, 0.0f );

	angles[YAW] = m_flEyeYaw;

	Vector vecForward, vecRight, vecUp;
	AngleVectors( angles, &vecForward, &vecRight, &vecUp );

	// Draw a red triangle on the ground for the eye yaw.
	debugoverlay->AddTriangleOverlay( ( vecPos + vecRight * flBaseSize / 2.0f ), 
		( vecPos - vecRight * flBaseSize / 2.0f ), 
		( vecPos + vecForward * flHeight, 255, 0, 0, 255, false, 0.01f );

#endif
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::DebugShowActivity( Activity activity )
{
#ifdef _DEBUG

	const char *pszActivity = "other";

	switch( activity )
	{
	case ACT_IDLE:
		{
			pszActivity = "idle";
			break;
		}
	case ACT_SPRINT:
		{
			pszActivity = "sprint";
			break;
		}
	case ACT_WALK:
		{
			pszActivity = "walk";
			break;
		}
	case ACT_RUN:
		{
			pszActivity = "run";
			break;
		}
	}

	Msg( "Activity: %s\n", pszActivity );

#endif
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iStartLine - 
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::DebugShowAnimState( int iStartLine )
{
	Vector vOuterVel;
	GetOuterAbsVelocity( vOuterVel );

	Anim_StateLog( "----------------- frame %d -----------------\n", gpGlobals->framecount );

	int iLine = iStartLine;
	Anim_StatePrintf( iLine++, "main: %s, cycle: %.2f\n", GetSequenceName( GetPlayer()->GetModelPtr(), GetPlayer()->GetSequence() ), GetPlayer()->GetCycle() );

#if defined( CLIENT_DLL )
	for ( int i=0; i < GetPlayer()->GetNumAnimOverlays()-1; i++ )
	{
		C_AnimationLayer *pLayer = GetPlayer()->GetAnimOverlay( i /*i+1?*/ );
		Anim_StatePrintf( iLine++, "%s, weight: %.2f, cycle: %.2f, aim (%d)", 
			pLayer->m_nOrder == CBaseAnimatingOverlay::MAX_OVERLAYS ? "--" : GetSequenceName( GetPlayer()->GetModelPtr(), pLayer->m_nSequence ), 
			pLayer->m_nOrder == CBaseAnimatingOverlay::MAX_OVERLAYS ? -1 :(float)pLayer->m_flWeight, 
			pLayer->m_nOrder == CBaseAnimatingOverlay::MAX_OVERLAYS ? -1 :(float)pLayer->m_flCycle, 
			i
			);
	}
#endif

	Anim_StatePrintf( iLine++, "vel: %.2f, time: %.2f, max: %.2f", 
		vOuterVel.Length2D(), gpGlobals->curtime, GetInterpolatedGroundSpeed() );
		
//	AnimStatePrintf( iLine++, "ent yaw: %.2f, body_yaw: %.2f, body_pitch: %.2f, move_x: %.2f, move_y: %.2f", 
//		m_angRender[YAW], g_flLastBodyYaw, g_flLastBodyPitch, m_vLastMovePose.x, m_vLastMovePose.y );

	Anim_StateLog( "--------------------------------------------\n\n" );

	// Draw a red triangle on the ground for the eye yaw.
	float flBaseSize = 10;
	float flHeight = 80;
	Vector vBasePos = GetPlayer()->GetAbsOrigin() + Vector( 0, 0, 3 );
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

// Debug!
const char *s_aGestureSlotNames[GESTURE_SLOT_COUNT] =
{
	"Attack and Reload",
	"Grenade",
	"Jump",
	"Flinch",
	"VCD",
	"Custom"
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::DebugGestureInfo( void )
{
	CBasePlayer *pPlayer = GetPlayer();
	if ( !pPlayer )
		return;

	int iLine = ( pPlayer->IsServer() ? 12 : ( 14 + GESTURE_SLOT_COUNT ) );

	Anim_StatePrintf( iLine++, "%s\n", ( pPlayer->IsServer() ? "Server" : "Client" ) );

	for ( int iGesture = 0; iGesture < GESTURE_SLOT_COUNT; ++iGesture )
	{
		GestureSlot_t *pGesture = &m_aGestureSlots[iGesture];
		if ( pGesture )
		{
			if( pGesture->m_bActive )
			{
				Anim_StatePrintf( iLine++, "Gesture Slot %d(%s): %s %s(A:%s, C:%f P:%f)\n", 
					iGesture, 
					s_aGestureSlotNames[iGesture],
					ActivityList_NameForIndex( pGesture->m_iActivity ),
					GetSequenceName( pPlayer->GetModelPtr(), pGesture->m_pAnimLayer->m_nSequence ),
					( pGesture->m_bAutoKill ? "true" : "false" ),
					pGesture->m_pAnimLayer->m_flCycle, pGesture->m_pAnimLayer->m_flPlaybackRate );
			}
			else
			{
				Anim_StatePrintf( iLine++, "Gesture Slot %d(%s): NOT ACTIVE!\n", iGesture, s_aGestureSlotNames[iGesture] );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: New Model, init the pose parameters
//-----------------------------------------------------------------------------
void CMultiPlayerAnimState::OnNewModel( void )
{
	m_bPoseParameterInit = false;
	ClearAnimationState();
}
