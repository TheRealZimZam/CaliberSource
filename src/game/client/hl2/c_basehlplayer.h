//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#if !defined( C_BASEHLPLAYER_H )
#define C_BASEHLPLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseplayer.h"
#include "c_hl2_playerlocaldata.h"
#include "multiplayer_animstate.h"

#define SINGLEPLAYER_ANIMSTATE 1	//Use a animstate instead of the old, setanimation system

class C_BaseHLPlayer : public C_BasePlayer
{
public:
	DECLARE_CLASS( C_BaseHLPlayer, C_BasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

						C_BaseHLPlayer();
						~C_BaseHLPlayer();

	virtual void		OnDataChanged( DataUpdateType_t updateType );
	virtual void		PostDataUpdate( DataUpdateType_t updateType );

	virtual bool		ShouldPredict( void );

	virtual void		Spawn( void );

	void				Weapon_DropPrimary( void );
	virtual float		GetDefaultAnimSpeed( void );
		
	float				GetFOV();
	void				Zoom( float FOVOffset, float time );
	float				GetZoom( void );
	bool				IsZoomed( void )	{ return m_HL2Local.m_bZooming; }

	//Tony; minor cosmetic really, fix confusion by simply renaming this one; everything calls IsSprinting(), and this isn't really even used.
	bool				IsSprintActive( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_SPRINT; }
	bool				IsFlashlightActive( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_FLASHLIGHT; }
	bool				IsBreatherActive( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_BREATHER; }

	virtual bool		ShouldDraw( void );
	virtual int			DrawModel( int flags );
	virtual const		QAngle& GetRenderAngles();
	virtual const		QAngle& EyeAngles( void );

	LadderMove_t		*GetLadderMove() { return &m_HL2Local.m_LadderMove; }
	virtual void		ExitLadder();
	bool				IsSprinting() const { return m_fIsSprinting; }
	
	// Input handling
	virtual bool	CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	void			PerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd );
	void			PerformClientSideNPCSpeedModifiers( float flFrameTime, CUserCmd *pCmd );
	
	virtual void	UpdateClientSideAnimation();
	void			DoAnimationEvent( int PlayerAnimEvent_t, int nData = 0 );

	bool				IsWeaponLowered( void ) { return m_HL2Local.m_bWeaponLowered; }

public:

	C_HL2PlayerLocalData		m_HL2Local;
	EHANDLE				m_hClosestNPC;
	float				m_flSpeedModTime;
	bool				m_fIsSprinting;

private:
	C_BaseHLPlayer( const C_BaseHLPlayer & ); // not defined, not accessible

#if SINGLEPLAYER_ANIMSTATE
	IPlayerAnimState	*m_PlayerAnimState;
#else
	CMultiPlayerAnimState	*m_PlayerAnimState;
#endif

	QAngle	m_angEyeAngles;
	CInterpolatedVar< QAngle >	m_iv_angEyeAngles;

	int m_iSpawnInterpCounter;
	float m_fLastPredFreeze;

	bool				TestMove( const Vector &pos, float fVertDist, float radius, const Vector &objPos, const Vector &objDir );

	float				m_flZoomStart;
	float				m_flZoomEnd;
	float				m_flZoomRate;
	float				m_flZoomStartTime;

	float				m_flSpeedMod;
	float				m_flExitSpeedMod;


friend class CHL2GameMovement;
};


#endif
