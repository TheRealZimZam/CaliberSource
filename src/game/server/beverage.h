//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Beverage Dispenser
//
// $NoKeywords: $
//=============================================================================//

#ifndef BEVERAGE_H
#define BEVERAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"

class CEnvBeverage : public CBaseEntity
{
public:
	DECLARE_CLASS( CEnvBeverage, CBaseEntity );

	enum DispenserState { OFF, ON, DISPENSING, DISPENSE_FAILED, DISPENSE_FINISHED, BROKEN };

	void	Spawn( void );
	void	Precache( void );
	bool	KeyValue( const char *szKeyName, const char *szValue );

	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	DispenserWait( void );
	void	Dispense( void );
	void	DispenserReset( void );

	void	SetDispenseTime( float time ) {m_flDispenseTime = time;}
	// SetDispenserState: Gotta check if we have a parent every time its called, which is often, sadly
	void	SetDispenserState( int state ) { if(GetMoveParent()){GetMoveParent()->SetTextureFrameIndex( state );} }

	// For breaking
	void	Break( bool bDoEffect = true );

	// Input handlers.
	void	InputActivate( inputdata_t &inputdata );
	void	InputBreak( inputdata_t &inputdata );

	DECLARE_DATADESC();

public:
	bool	m_CanInDispenser;
	int		m_nBeverageType;
private:
//	bool	m_bOldBeverage;	//This is an old env_beverage - dont do any of the new stuff, as the mapper has already addressed it...
	bool	m_bAskedToRemove;
	bool	m_bUseSpeech;
	float	m_flDispenseTime;
	COutputEvent m_OnBreak;
};

//-----------------------------------------------------------------------------
class CItemSoda : public CBaseAnimating
{
public:
	DECLARE_CLASS( CItemSoda, CBaseAnimating );

	void	Spawn( void );
	void	Precache( void );
	void	CanDrop( void );
	void	CanThink( void );
	void	CanTouch( CBaseEntity *pOther );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();

private:
	float	m_flCanWarmTime;
	bool	m_bWarm;

};
#endif // DISPENSER_H
