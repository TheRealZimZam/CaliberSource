//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// New version of multi_manager. Fires an OnTrigger output when triggered, and can be
// disabled to prevent forwarding outputs multiple times.
//
// Useful as an intermediary between one entity and another for turning on or
// off an I/O connection, or as a container for holding a set of outputs that
// can be triggered from multiple places.
//
//=============================================================================

#include "cbase.h"
#include "logic_playerproxy.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "player.h"
#include "filters.h"

#if defined HL2_DLL
#include "hl2_player.h"
#endif // HL2_DLL

#if defined ( PORTAL2 )
#include "portal_player.h"
#include "weapon_portalgun.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the player to the world and vice versa
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( logic_playerproxy, CLogicPlayerProxy);

BEGIN_DATADESC( CLogicPlayerProxy )

DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),

// Base
DEFINE_OUTPUT( m_RequestedPlayerHealth,		"PlayerHealth" ),
DEFINE_OUTPUT( m_PlayerDied,				"PlayerDied" ),

DEFINE_OUTPUT( m_OnJump,					"OnJump" ),
DEFINE_OUTPUT( m_OnDuck,					"OnDuck" ),
DEFINE_OUTPUT( m_OnUnDuck,					"OnUnDuck" ),

DEFINE_OUTPUT( m_PlayerHasAmmo,				"PlayerHasAmmo" ),
DEFINE_OUTPUT( m_PlayerHasNoAmmo,			"PlayerHasNoAmmo" ),

// HL2 / Episodic
#if defined HL2_DLL
DEFINE_OUTPUT( m_OnFlashlightOn,			"OnFlashlightOn" ),
DEFINE_OUTPUT( m_OnFlashlightOff,			"OnFlashlightOff" ),

DEFINE_INPUTFUNC( FIELD_VOID,				"SetFlashlightSlowDrain",	InputSetFlashlightSlowDrain ),
DEFINE_INPUTFUNC( FIELD_VOID,				"SetFlashlightNormalDrain",	InputSetFlashlightNormalDrain ),
DEFINE_INPUTFUNC( FIELD_VOID,				"LowerWeapon", InputLowerWeapon ),
DEFINE_INPUTFUNC( FIELD_STRING,				"SetLocatorTargetEntity", InputSetLocatorTargetEntity ),
DEFINE_INPUTFUNC( FIELD_VOID,				"RequestPlayerHealth",	InputRequestPlayerHealth ),
DEFINE_INPUTFUNC( FIELD_INTEGER,			"SetPlayerHealth",	InputSetPlayerHealth ),
DEFINE_INPUTFUNC( FIELD_VOID,				"RequestAmmoState", InputRequestAmmoState ),
DEFINE_INPUTFUNC( FIELD_VOID,				"EnableCappedPhysicsDamage", InputEnableCappedPhysicsDamage ),
DEFINE_INPUTFUNC( FIELD_VOID,				"DisableCappedPhysicsDamage", InputDisableCappedPhysicsDamage ),
#endif // HL2_DLL

// Portal 2
#ifdef PORTAL2
DEFINE_OUTPUT( m_OnStartSlowingTime,		"OnStartSlowingTime" ),
DEFINE_OUTPUT( m_OnStopSlowingTime,			"OnStopSlowingTime" ),

DEFINE_OUTPUT( m_OnPrimaryPortalPlaced,		"OnPrimaryPortalPlaced" ),
DEFINE_OUTPUT( m_OnSecondaryPortalPlaced,	"OnSecondaryPortalPlaced" ),

DEFINE_OUTPUT( m_OnCoopPing,				"OnCoopPing" ),

DEFINE_INPUTFUNC( FIELD_VOID,				"AddPotatosToPortalgun", InputAddPotatosToPortalgun ),
DEFINE_INPUTFUNC( FIELD_VOID,				"RemovePotatosFromPortalgun", InputRemovePotatosFromPortalgun ),

DEFINE_INPUTFUNC( FIELD_BOOLEAN,			"SetDropEnabled", InputSetDropEnabled ),
DEFINE_INPUTFUNC( FIELD_VOID,				"ForceVMGrabController", InputForceVMGrabController ),
DEFINE_INPUTFUNC( FIELD_VOID,				"ForcePhysicsGrabController", InputForcePhysicsGrabController ),
DEFINE_INPUTFUNC( FIELD_VOID,				"ResetGrabControllerBehavior", InputResetGrabControllerBehavior ),
DEFINE_INPUTFUNC( FIELD_VOID,				"PaintPlayerWithPortalPaint", InputPaintPlayerWithPortalPaint ),
DEFINE_INPUTFUNC( FIELD_FLOAT,				"SetMotionBlurAmount", InputSetMotionBlurAmount ),
#endif // PORTAL2

END_DATADESC()


//-----------------------------------------------------------------------------
void CLogicPlayerProxy::Activate( void )
{
	BaseClass::Activate();

	if ( m_hPlayer == NULL )
	{
		if ( GameRules()->IsMultiplayer() )
		{
			Assert( 0 );
			Warning( "You shouldnt use logic_playerproxy in multiplayer!\n" );
		}

		m_hPlayer = UTIL_GetLocalPlayer();
	}
}

// For MP inputs/outputs
CBasePlayer *CLogicPlayerProxy::GetPlayersFromProxy( void )
{
	for (int i = 1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			
		if ( !pPlayer )
			continue;

		if ( !pPlayer->IsConnected() )
			continue;

		return pPlayer;
	}

	return NULL;
}

bool CLogicPlayerProxy::PassesDamageFilter( const CTakeDamageInfo &info )
{
	if (m_hDamageFilter)
	{
		CBaseFilter *pFilter = (CBaseFilter *)(m_hDamageFilter.Get());
		return pFilter->PassesDamageFilter(info);
	}

	return true;
}

//-----------------------------------------------------------------------------
void CLogicPlayerProxy::InputSetPlayerHealth( inputdata_t &inputdata )
{
	if ( m_hPlayer == NULL )
		return;

	m_hPlayer->SetHealth( inputdata.value.Int() );
}

void CLogicPlayerProxy::InputRequestPlayerHealth( inputdata_t &inputdata )
{
	if ( m_hPlayer == NULL )
		return;

	m_RequestedPlayerHealth.Set( m_hPlayer->GetHealth(), inputdata.pActivator, inputdata.pCaller );
}

void CLogicPlayerProxy::InputRequestAmmoState( inputdata_t &inputdata )
{
	if( m_hPlayer == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( m_hPlayer );

	for ( int i = 0 ; i < pPlayer->WeaponCount(); ++i )
	{
		CBaseCombatWeapon* pCheck = pPlayer->GetWeapon( i );

		if ( pCheck )
		{
			if ( pCheck->HasAnyAmmo() && (pCheck->UsesPrimaryAmmo() || pCheck->UsesSecondaryAmmo()))
			{
				m_PlayerHasAmmo.FireOutput( this, this, 0 );
				return;
			}
		}
	}

	m_PlayerHasNoAmmo.FireOutput( this, this, 0 );
}

//-----------------------------------------------------------------------------
// HL2 Inputs
//-----------------------------------------------------------------------------
#if defined HL2_DLL

extern ConVar hl2_darkness_flashlight_factor;

void CLogicPlayerProxy::InputSetFlashlightSlowDrain( inputdata_t &inputdata )
{
	if( m_hPlayer == NULL )
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if( pPlayer )
		pPlayer->SetFlashlightPowerDrainScale( hl2_darkness_flashlight_factor.GetFloat() );
}

void CLogicPlayerProxy::InputSetFlashlightNormalDrain( inputdata_t &inputdata )
{
	if( m_hPlayer == NULL )
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if( pPlayer )
		pPlayer->SetFlashlightPowerDrainScale( 1.0f );
}

void CLogicPlayerProxy::InputLowerWeapon( inputdata_t &inputdata )
{
	if( m_hPlayer == NULL )
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	pPlayer->Weapon_Lower();
}

void CLogicPlayerProxy::InputSetLocatorTargetEntity( inputdata_t &inputdata )
{
	if( m_hPlayer == NULL )
		return;

	CBaseEntity *pTarget = NULL; // assume no target
	string_t iszTarget = MAKE_STRING( inputdata.value.String() );

	if( iszTarget != NULL_STRING )
	{
		pTarget = gEntList.FindEntityByName( NULL, iszTarget );
	}

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	pPlayer->SetLocatorTargetEntity(pTarget);
}

void CLogicPlayerProxy::InputEnableCappedPhysicsDamage( inputdata_t &inputdata )
{
	if ( m_hPlayer == NULL )
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	pPlayer->EnableCappedPhysicsDamage();
}

void CLogicPlayerProxy::InputDisableCappedPhysicsDamage( inputdata_t &inputdata )
{
	if ( m_hPlayer == NULL )
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	pPlayer->DisableCappedPhysicsDamage();
}

#endif // HL2_EPISODIC

//-----------------------------------------------------------------------------
// Portal Inputs
//-----------------------------------------------------------------------------
#if defined ( PORTAL2 )
CPortal_Player* GetPortalPlayerFromProxy()
{
	if ( GameRules()->IsMultiplayer() )
	{
		Assert( 0 );
		Warning( "Can't use logic player proxy in multiplayer!\n" );
		return NULL;
	}

	return (CPortal_Player*)UTIL_PlayerByIndex( 1 );
}

void CLogicPlayerProxy::InputAddPotatosToPortalgun( inputdata_t &inputdata )
{
	CPortal_Player* pPlayer = GetPortalPlayerFromProxy();
	if ( pPlayer == NULL )
		return;

	CWeaponPortalgun *pPortalgun = (CWeaponPortalgun*)pPlayer->GetActiveWeapon();
	
	Assert( pPortalgun );
	if ( !pPortalgun )
		return;

	pPortalgun->SetPotatosOnPortalgun( true );
}

void CLogicPlayerProxy::InputRemovePotatosFromPortalgun( inputdata_t &inputdata )
{
	CPortal_Player* pPlayer = GetPortalPlayerFromProxy();
	if ( pPlayer == NULL )
		return;

	CWeaponPortalgun *pPortalgun = (CWeaponPortalgun*)pPlayer->GetActiveWeapon();
	
	Assert( pPortalgun );
	if ( !pPortalgun )
		return;

	pPortalgun->SetPotatosOnPortalgun( false );
}

void CLogicPlayerProxy::InputSetDropEnabled( inputdata_t &inputdata )
{
	CPortal_Player* pPlayer = GetPortalPlayerFromProxy();
	if( pPlayer == NULL )
	{
		return;
	}
	pPlayer->SetDropEnabled( inputdata.value.Bool() );
}

void SetForcedGrabController( ForcedGrabControllerType type )
{
	CPortal_Player* pPlayer = GetPortalPlayerFromProxy();
	if( pPlayer == NULL )
	{
		return;
	}

	pPlayer->SetForcedGrabControllerType( type );
}

void CLogicPlayerProxy::InputForceVMGrabController( inputdata_t &inputdata )
{
	SetForcedGrabController( FORCE_GRAB_CONTROLLER_VM );
}

void CLogicPlayerProxy::InputForcePhysicsGrabController( inputdata_t &inputdata )
{
	SetForcedGrabController( FORCE_GRAB_CONTROLLER_PHYSICS );
}


void CLogicPlayerProxy::InputResetGrabControllerBehavior( inputdata_t &inputdata )
{
	SetForcedGrabController( FORCE_GRAB_CONTROLLER_DEFAULT );
}

void CLogicPlayerProxy::InputPaintPlayerWithPortalPaint( inputdata_t &/*inputdata*/ )
{
	CPortal_Player* pPlayer = GetPortalPlayerFromProxy();
	if( pPlayer != NULL )
		pPlayer->Paint( PORTAL_POWER, vec3_origin );
}

void CLogicPlayerProxy::InputSetMotionBlurAmount( inputdata_t &inputdata )
{
	if ( GameRules() && GameRules()->IsMultiplayer() == false )
	{
		CPortal_Player* pPlayer = GetPortalPlayerFromProxy();
		if( pPlayer != NULL )
		{
			pPlayer->SetMotionBlurAmount( inputdata.value.Float() );
		}
	}
}
#endif

