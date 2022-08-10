//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#if !defined( HL2_PLAYERANIMSTATE_H )
#define HL2_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif


#include "convar.h"
#include "multiplayer_animstate.h"
#include "iplayeranimstate.h"

#if defined( CLIENT_DLL )
class C_BaseHLPlayer;
#define CHL2_Player C_BaseHLPlayer
#else
class CHL2_Player;
#endif

// ------------------------------------------------------------------------------------------------ //
// CPlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //
class CHL2PlayerAnimState : public CMultiPlayerAnimState
{
public:
	
	DECLARE_CLASS( CHL2PlayerAnimState, CMultiPlayerAnimState );

	CHL2PlayerAnimState();
	CHL2PlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData );
	~CHL2PlayerAnimState();

	void Init( CHL2_Player *pPlayer );
	CHL2_Player *GetHL2Player( void )							{ return m_pHL2Player; }

	virtual void ClearAnimationState();
	virtual Activity TranslateActivity( Activity actDesired );
	virtual void Update( float eyeYaw, float eyePitch );

	void	DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

	bool	HandleMoving( Activity &idealActivity );
	bool	HandleJumping( Activity &idealActivity );
	bool	HandleSwimming( Activity &idealActivity );

	virtual float GetCurrentMaxGroundSpeed();
private:
	//Tony; temp till 9way!
	bool						SetupPoseParameters( CStudioHdr *pStudioHdr );
	virtual void				EstimateYaw( void );
	virtual void				ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr );
	virtual void				ComputePoseParam_AimPitch( CStudioHdr *pStudioHdr );
	virtual void				ComputePoseParam_AimYaw( CStudioHdr *pStudioHdr );
	
	CHL2_Player   *m_pHL2Player;
	bool		m_bInAirWalk;
	float		m_flHoldDeployedPoseUntilTime;
};

//!CHL2PlayerAnimState *CreatePlayerAnimState( CHL2_Player *pPlayer );



#endif // HL2MP_PLAYERANIMSTATE_H
