//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: MP Animstate - adds gesture support for more versatile animation
//
//=============================================================================//
#ifndef MULTIPLAYER_ANIMSTATE_H
#define MULTIPLAYER_ANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#include "iplayeranimstate.h"
#include "base_playeranimstate.h"
#include "basecombatweapon_shared.h"

#if defined( CLIENT_DLL )
class C_BasePlayer;
#define CPlayer C_BasePlayer
#else
class CBasePlayer;
#endif

enum PlayerAnimEvent_t
{
	// Old redefs
	// DO NOT CHANGE ORDER - REFER TO SHAREDDEFS.H!!!
	// MUST ALLIGN WITH PLAYER_ANIM ENUM!!!
	PLAYERANIMEVENT_IDLE,
	PLAYERANIMEVENT_WALK,
	PLAYERANIMEVENT_JUMP,
	PLAYERANIMEVENT_SUPERJUMP,
	PLAYERANIMEVENT_DIE,
	PLAYERANIMEVENT_ATTACK_PRIMARY,
	PLAYERANIMEVENT_ATTACK_SECONDARY,
	PLAYERANIMEVENT_IN_VEHICLE,
	PLAYERANIMEVENT_RELOAD,
	PLAYERANIMEVENT_ATTACK_PRE,
	PLAYERANIMEVENT_ATTACK_POST,
	PLAYERANIMEVENT_SPAWN,

	// Multiplayer only anims
	PLAYERANIMEVENT_ATTACK_GRENADE,
	PLAYERANIMEVENT_RELOAD_LOOP,
	PLAYERANIMEVENT_RELOAD_END,
	PLAYERANIMEVENT_FLINCH_CHEST,
	PLAYERANIMEVENT_FLINCH_HEAD,
	PLAYERANIMEVENT_FLINCH_LEFTARM,
	PLAYERANIMEVENT_FLINCH_RIGHTARM,
	PLAYERANIMEVENT_FLINCH_LEFTLEG,
	PLAYERANIMEVENT_FLINCH_RIGHTLEG,

	// Cancel.
	PLAYERANIMEVENT_CANCEL,

	// Snap to current yaw exactly
	PLAYERANIMEVENT_SNAP_YAW,

	PLAYERANIMEVENT_CUSTOM,				// Used to play specific activities
	PLAYERANIMEVENT_CUSTOM_GESTURE,
	PLAYERANIMEVENT_CUSTOM_SEQUENCE,	// Used to play specific sequences
	PLAYERANIMEVENT_CUSTOM_GESTURE_SEQUENCE,

	// TF Specific. Here until there's a derived game solution to this.
	PLAYERANIMEVENT_GRENADE1_DRAW,
	PLAYERANIMEVENT_GRENADE2_DRAW,
	PLAYERANIMEVENT_GRENADE1_THROW,
	PLAYERANIMEVENT_GRENADE2_THROW,
	PLAYERANIMEVENT_VOICE_COMMAND_GESTURE,

	// Tony; some SDK ones now too.
	PLAYERANIMEVENT_STAND_TO_PRONE,
	PLAYERANIMEVENT_CROUCH_TO_PRONE,
	PLAYERANIMEVENT_PRONE_TO_STAND,
	PLAYERANIMEVENT_PRONE_TO_CROUCH,

	PLAYERANIMEVENT_COUNT
};

// Gesture Slots.
enum
{
	GESTURE_SLOT_ATTACK_AND_RELOAD,
	GESTURE_SLOT_GRENADE,
	GESTURE_SLOT_JUMP,
	GESTURE_SLOT_FLINCH,
	GESTURE_SLOT_VCD,
	GESTURE_SLOT_CUSTOM,

	GESTURE_SLOT_COUNT,
};

#define GESTURE_SLOT_INVALID	-1

struct GestureSlot_t
{
	int					m_iGestureSlot;
	Activity			m_iActivity;
	bool				m_bAutoKill;
	bool				m_bActive;
	CAnimationLayer		*m_pAnimLayer;
};

inline bool IsCustomPlayerAnimEvent( PlayerAnimEvent_t event )
{
	return ( event == PLAYERANIMEVENT_CUSTOM ) || ( event == PLAYERANIMEVENT_CUSTOM_GESTURE ) ||
		( event == PLAYERANIMEVENT_CUSTOM_SEQUENCE ) || ( event == PLAYERANIMEVENT_CUSTOM_GESTURE_SEQUENCE );
}

struct MultiPlayerPoseData_t
{
	int			m_iMoveX;
	int			m_iMoveY;
	int			m_iAimYaw;
	int			m_iAimPitch;
	int			m_iBodyHeight;

	float		m_flLastAimTurnTime;

	void Init()
	{
		m_iMoveX = 0;
		m_iMoveY = 0;
		m_iAimYaw = 0;
		m_iAimPitch = 0;
		m_iBodyHeight = 0;
		m_flLastAimTurnTime = 0.0f;
	}
};

struct DebugPlayerAnimData_t
{
	float		m_flSpeed;
	float		m_flAimPitch;
	float		m_flAimYaw;
	float		m_flBodyHeight;
	Vector2D	m_vecMoveYaw;

	void Init()
	{
		m_flSpeed = 0.0f;
		m_flAimPitch = 0.0f;
		m_flAimYaw = 0.0f;
		m_flBodyHeight = 0.0f;
		m_vecMoveYaw.Init();
	}
};

struct MultiPlayerMovementData_t
{
	// Set speeds to -1 if they are not used.
	float		m_flWalkSpeed;
	float		m_flRunSpeed;
	float		m_flSprintSpeed;
};

//=============================================================================
//
// Multi-Player Animation State
//
class CMultiPlayerAnimState : public CBasePlayerAnimState
{
public:

	DECLARE_CLASS( CMultiPlayerAnimState, CBasePlayerAnimState );

	// Creation/Destruction
	CMultiPlayerAnimState() {}
	CMultiPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData );
	~CMultiPlayerAnimState();

	virtual void Init( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData ); 

	// This is called by both the client and the server in the same way to trigger events for
	// players firing, jumping, throwing grenades, etc.
	virtual void ClearAnimationState();
	virtual void DoAnimationEvent( int PlayerAnimEvent_mp, int nData = 0 );
	virtual Activity CalcMainActivity();	
	virtual void Update( float eyeYaw, float eyePitch );

	const QAngle &GetRenderAngles();

	virtual Activity TranslateActivity( Activity actDesired );

	virtual float GetRunSpeed() { return RUN_SPEED; }
	virtual void SetRunSpeed( float flSpeed ) { m_MovementData.m_flRunSpeed = flSpeed; }

	virtual float GetWalkSpeed() { return m_MovementData.m_flWalkSpeed; }
	virtual void SetWalkSpeed( float flSpeed ) { m_MovementData.m_flWalkSpeed = flSpeed; }

	virtual float GetSprintSpeed() { return SPRINT_SPEED; }
	virtual void SetSprintSpeed( float flSpeed ) { m_MovementData.m_flSprintSpeed = flSpeed; }

	// Debug
	virtual void ShowDebugInfo( void );
	virtual void DebugShowAnimState( int iStartLine );

	Activity GetCurrentMainActivity( void ) { return m_eCurrentMainSequenceActivity; }

	void OnNewModel( void );

	// Gestures.
	void ResetGestureSlots( void );
	void ResetGestureSlot( int iGestureSlot );
	void AddVCDSequenceToGestureSlot( int iGestureSlot, int iGestureSequence, bool bAutoKill = true );

	// Feet.
	bool	m_bForceAimYaw;

protected:

	CBasePlayer *GetPlayer( void )				{ return m_pPlayer; }

	// Allow inheriting classes to override SelectWeightedSequence
	virtual int SelectWeightedSequence( Activity activity ) { return GetPlayer()->SelectWeightedSequence( activity ); }
	virtual void RestartMainSequence();

	virtual bool HandleJumping( Activity &idealActivity );
	virtual bool HandleMoving( Activity &idealActivity );
	virtual bool HandleSwimming( Activity &idealActivity );
	virtual bool HandleDying( Activity &idealActivity );

	// Gesture Slots
	CUtlVector<GestureSlot_t>		m_aGestureSlots;
	bool	InitGestureSlots( void );
	void	ShutdownGestureSlots( void );
	bool	IsGestureSlotActive( int iGestureSlot );
	bool	IsGestureSlotPlaying( int iGestureSlot, Activity iGestureActivity );
	void	AddToGestureSlot( int iGestureSlot, Activity iGestureActivity, bool bAutoKill );
	void	RestartGesture( int iGestureSlot, Activity iGestureActivity, bool bAutoKill = true );
	void	ComputeGestureSequence( CStudioHdr *pStudioHdr );
	void	UpdateGestureLayer( CStudioHdr *pStudioHdr, GestureSlot_t *pGesture );
	void	DebugGestureInfo( void );

#ifdef CLIENT_DLL
	void	RunGestureSlotAnimEventsToCompletion( GestureSlot_t *pGesture );
#endif

	virtual void PlayFlinchGesture( Activity iActivity );

	// Pose paramters.
	bool				SetupPoseParameters( CStudioHdr *pStudioHdr );
	virtual void		ComputePoseParam_AimPitch( CStudioHdr *pStudioHdr );
	//virtual void		ComputePoseParam_AimYaw( CStudioHdr *pStudioHdr );
	//void				ConvergeYawAngles( float flGoalYaw, float flYawRate, float flDeltaTime, float &flCurrentYaw );

	void UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd, float flWeight = 1.0 );

	virtual float GetCurrentMaxGroundSpeed();
	virtual void ComputeSequences( CStudioHdr *pStudioHdr );
	void ComputeMainSequence();
	void UpdateInterpolators();
	void ResetGroundSpeed( void );
	float GetInterpolatedGroundSpeed( void );

	void ComputeFireSequence();
	void ComputeDeployedSequence();

	void				DebugShowAnimStateForPlayer( bool bIsServer );
	void				DebugShowEyeYaw( void );

// Client specific.
#ifdef CLIENT_DLL

	// Debug.
	void				DebugShowActivity( Activity activity );

#endif

protected:

	CBasePlayer	*m_pPlayer;

	QAngle				m_angRender;

	// Pose parameters.
	bool						m_bPoseParameterInit;
	MultiPlayerPoseData_t		m_PoseParameterData;

	bool						m_bCurrentFeetYawInitialized;
	float						m_flLastAnimationStateClearTime;
	
	float m_flEyeYaw;
	float m_flEyePitch;
	float m_flGoalFeetYaw;
	float m_flCurrentFeetYaw;
	float m_flLastAimTurnTime;

	MultiPlayerMovementData_t	m_MovementData;

	// Swimming.
	bool	m_bInSwim;
	bool	m_bFirstSwimFrame;

	// Dying.
	bool	m_bFirstDyingFrame;

	// Last activity we've used on the lower body. Used to determine if animations should restart.
	Activity m_eCurrentMainSequenceActivity;	

	// Specific full-body sequence to play
	int		m_nSpecificMainSequence;

	// Weapon data.
	CHandle<CBaseCombatWeapon>	m_hActiveWeapon;

	// Ground speed interpolators.
#ifdef CLIENT_DLL
	float m_flLastGroundSpeedUpdateTime;
	CInterpolatedVar<float> m_iv_flMaxGroundSpeed;
#endif
	float m_flMaxGroundSpeed;

	//Tony; moved debuganim data to a private block and made the 2 sdk animstates friendly. I override the base classes
	//but want complete functionality.
private:
	friend class CSDKPlayerAnimState;
	friend class CHL2MPPlayerAnimState;
	DebugPlayerAnimData_t		m_DebugAnimData;

};

CMultiPlayerAnimState *CreateMultiPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData );

// If this is set, then the game code needs to make sure to send player animation events
// to the local player if he's the one being watched.
extern ConVar cl_showanimstate;

#endif // DOD_PLAYERANIMSTATE_H