//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: CTF Flag.
//
// TODO; Code this thing!
//=============================================================================//
#ifndef ITEM_FLAG_H
#define ITEM_FLAG_H
#ifdef _WIN32
#pragma once
#endif

#include "item.h"

#ifdef CLIENT_DLL
#define CCaptureFlag C_CaptureFlag
#endif

//=============================================================================
//
// CTF Flag defines.
//
#define SF_FLAG_USEONLY		0x001


#define FLAG_RESET_TIME		60.0f

//=============================================================================
//
// CTF Flag class.
//
class CCaptureFlag : public CTFItem
{
public:

	DECLARE_CLASS( CCaptureFlag, CTFItem );
	DECLARE_NETWORKCLASS();

	CCaptureFlag();

	unsigned int	GetItemID( void );

	void			Precache( void );
	void			Spawn( void );

	void			FlagTouch( CBaseEntity *pOther );

	bool			IsDisabled( void );
	void			SetDisabled( bool bDisabled );

	CBaseEntity		*GetPrevOwner( void ) { return m_hPrevOwner.Get(); }

// Game DLL Functions
#ifdef GAME_DLL
	virtual void	Activate( void );

	// Input handlers
	void			InputEnable( inputdata_t &inputdata );
	void			InputDisable( inputdata_t &inputdata );
	void			InputRoundActivate( inputdata_t &inputdata );

	void			Think( void );
	
	void			SetFlagStatus( int iStatus );
	void			ResetFlagReturnTime( void ) { m_flResetTime = 0; }
	void			SetFlagReturnIn( float flTime )
	{
		m_flResetTime = gpGlobals->curtime + flTime;
		m_flMaxResetTime = flTime;
	}

	void			ResetFlagNeutralTime( void ) { m_flNeutralTime = 0; }
	void			SetFlagNeutralIn( float flTime )
	{ 
		m_flNeutralTime = gpGlobals->curtime + flTime;
		m_flMaxResetTime = flTime;
	}
	bool			IsCaptured( void ){ return m_bCaptured; }

	int				UpdateTransmitState();

#else // CLIENT DLL Functions

	virtual char	*GetIDString( void ) { return "entity_capture_flag"; };

	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	CNewParticleEffect	*m_pGlowTrailEffect;
	CNewParticleEffect	*m_pPaperTrailEffect;

	void			ManageTrailEffects( void );
	virtual void	Simulate( void );

	float			GetMaxResetTime() { return m_flMaxResetTime; }
	float			GetReturnProgress( void );

#endif

	void			Capture( CTFPlayer *pPlayer, int nCapturePoint );
	virtual void	PickUp( CTFPlayer *pPlayer, bool bInvisible );
	virtual void	Drop( CTFPlayer *pPlayer, bool bVisible, bool bThrown = false, bool bMessage = true );

	bool			IsDropped( void );
	bool			IsHome( void );
	bool			IsStolen( void );

private:

	void			Reset( void );
	void			ResetMessage( void );

private:

	CNetworkVar( bool,	m_bDisabled );	// Enabled/Disabled?

	CNetworkVar( int,	m_nFlagStatus );
	CNetworkVar( float,	m_flResetTime );		// Time until the flag is placed back at spawn.
	CNetworkVar( float, m_flMaxResetTime );		// Time the flag takes to return in the current mode
	CNetworkVar( float, m_flNeutralTime );	// Time until the flag becomes neutral (used for the invade gametype)
	CNetworkHandle( CBaseEntity, m_hPrevOwner );

	int				m_iOriginalTeam;
	float			m_flOwnerPickupTime;

	EHANDLE		m_hReturnIcon;

#ifdef GAME_DLL
	Vector			m_vecResetPos;		// The position the flag should respawn (reset) at.
	QAngle			m_vecResetAng;		// The angle the flag should respawn (reset) at.

	COutputEvent	m_outputOnReturn;	// Fired when the flag is returned via timer.
	COutputEvent	m_outputOnPickUp;	// Fired when the flag is picked up.
	COutputEvent	m_outputOnDrop;		// Fired when the flag is dropped.
	COutputEvent	m_outputOnCapture;	// Fired when the flag is captured.

	bool			m_bAllowOwnerPickup;


	bool			m_bCaptured;
#else

	IMaterial	*m_pReturnProgressMaterial_Empty;		// For labels above players' heads.
	IMaterial	*m_pReturnProgressMaterial_Full;		

	int			m_nOldFlagStatus;

#endif

	DECLARE_DATADESC();
};

#endif // ITEM_FLAG_H
