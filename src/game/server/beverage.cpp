//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Beverage Dispenser - Now an actual fleshed out entity, to take
// the load out of the maps and reduce entcounts - If this entity is parented to a brush, 
// it will attempt to change the texture frame of the brush according to this code;
/*
	0 - Default OFF
	1 - Default ON
	2 - Dispensing
	3 - Failed to Dispense
	4 - Finished Dispensing
	5 - Broken
*/
// This entity should be placed in the dispensing slot of the parented machine, as all
// items/effects are created at or relative to this entity's origin.
//=============================================================================//

#include "cbase.h"
#include "beverage.h"
#include "effects.h"
#include "decals.h"
#include "gib.h"
#include "entitylist.h"
#include "model_types.h"
#include "player.h"
#include "ndebugoverlay.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "filesystem.h"
#include "engine/IEngineSound.h"
#include "te_effect_dispatch.h"
#include "Sprite.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=========================================================
// Beverage Dispenser
// overloaded m_iHealth, is now how many cans remain in the machine.
//=========================================================
void CEnvBeverage::Precache ( void )
{
//	PrecacheModel( "models/can.mdl" );
	if ( FClassnameIs( this, "env_snacks" ) )
		UTIL_PrecacheOther("item_candybar");
	else
		UTIL_PrecacheOther("item_sodacan");

	PrecacheScriptSound( "VendingMachine.Dispensing" );
	PrecacheScriptSound( "VendingMachine.FinishDispense" );
	PrecacheScriptSound( "VendingMachine.Deny" );
	PrecacheScriptSound( "VendingMachine.SlotFull" );
}

BEGIN_DATADESC( CEnvBeverage )

	// Function Pointers
	DEFINE_FUNCTION( DispenserWait ),
	DEFINE_FUNCTION( DispenserReset ),

	DEFINE_FIELD( m_CanInDispenser, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nBeverageType, FIELD_INTEGER ),

	// Inputs
	DEFINE_OUTPUT(m_OnBreak, "OnBreak"),
	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Break", InputBreak ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_beverage, CEnvBeverage );
LINK_ENTITY_TO_CLASS( env_snacks, CEnvBeverage );

void CEnvBeverage::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
	m_takedamage = false;

	m_CanInDispenser = false;
	m_bAskedToRemove = false;
	m_flDispenseTime = 0;

	SetDispenserState( ON );

	// If we've been spawned with 0 cans, assume LD wants us broken
	if ( m_iHealth == 0 )
		Break(false);
}

bool CEnvBeverage::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "beveragetype"))
		m_nBeverageType = atoi(szValue);
	else if (FStrEq(szKeyName, "playspeech"))
		m_bUseSpeech = atoi(szValue);
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}

void CEnvBeverage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// no more cans while one is waiting in the dispenser, or if I'm out of cans.
	if ( m_CanInDispenser )
	{
		// Always beep, but only say the line once
		EmitSound( "VendingMachine.Deny" );
		if ( !m_bAskedToRemove )
		{
			EmitSound( "VendingMachine.SlotFull" );
			m_bAskedToRemove = true;
		}

		SetDispenserState( DISPENSE_FAILED );

		SetThink( &CEnvBeverage::DispenserReset );
		SetNextThink( gpGlobals->curtime + 1.0f );
		return;
	}
	else if ( m_iHealth <= 0 )
	{
		// Turn me OFF
		SetDispenserState( OFF );

		EmitSound( "VendingMachine.Deny" );
		return;
	}

	// TODO; If a player has activated me, check monies
#if 0
	if ( pActivator->IsPlayer() && !g_pGameRules->IsMultiplayer() )
	{
		// Only do so for HL2 players
		CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(pActivator);
		if ( pPlayer )
		{
			if ( pPlayer->GetCash() <= 1 )
			{
				// Just say no
				EmitSound( "VendingMachine.Deny" );
				return;
			}
			else
			{
				// Take a cash away
				pPlayer->SetCash( (pPlayer->GetCash()-1) );
			}
		}
	}
#endif

	// If everything is ready and nothing's gonna break my stride, start the machine
	EmitSound( "VendingMachine.Dispensing" );

	SetDispenserState( DISPENSING );

	SetThink( &CEnvBeverage::DispenserWait );
	SetNextThink( gpGlobals->curtime + 1.5f );	//m_flDispenseTime
}

void CEnvBeverage::DispenserWait( void )
{
	// Change the graphic of my parent
//	if ( !m_CanInDispenser )
//	{
//		if ( GetMoveParent() )
//			GetMoveParent()->SetTextureFrameIndex( DISPENSING );
//
		Dispense();
//		SetNextThink( gpGlobals->curtime + 0.3f );	//0.2
//	}
}

void CEnvBeverage::Dispense( void )
{
	if ( FClassnameIs( this, "env_snacks" ) )
		CBaseEntity::Create( "item_candybar", GetLocalOrigin(), GetLocalAngles(), this );
	else
	{
		CBaseAnimating *pCan = (CBaseAnimating *)CBaseEntity::Create( "item_sodacan", GetLocalOrigin(), GetLocalAngles(), this );
		if ( m_nBeverageType == 6 )
		{
			// random
			pCan->m_nSkin = random->RandomInt( 0, 5 );
		}
		else
		{
			pCan->m_nSkin = m_nBeverageType;
		}
		m_CanInDispenser = true;
	}

	m_iHealth -= 1;
	m_bAskedToRemove = false;

	// Thank you, come again!
	EmitSound( "VendingMachine.FinishDispense" );

	SetDispenserState( DISPENSE_FINISHED );

	SetThink( &CEnvBeverage::DispenserReset );
	SetNextThink( gpGlobals->curtime + 1.0f );	//m_flDispenseTime
}

void CEnvBeverage::DispenserReset( void )
{
	// Wipe out back to defaults, and ready me for a use
	if ( m_iHealth <= 0 )
		SetDispenserState( OFF );	// Turn me OFF
	else
		SetDispenserState( ON );	// Keep me ON

	m_bAskedToRemove = false;

	//SetUse( &CEnvBeverage::Use );
	SetThink( NULL );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Breaks the dispenser, throw out a bunch of cans/candybars
//-----------------------------------------------------------------------------
void CEnvBeverage::Break( bool bDoEffect )
{
	if ( bDoEffect )
	{
		//!!TODO; Spew out your guts, spawn some gibs with sodacan model
	}

	// Remove thyself after a couple seconds
	m_OnBreak.FireOutput( this, this );

	SetDispenserState( BROKEN );

	SetUse( NULL );
	SetThink( &CEnvBeverage::SUB_Remove );
	SetNextThink( gpGlobals->curtime + random->RandomFloat( 1.0f, 2.0f ) );
}

//-----------------------------------------------------------------------------
// Inputs
void CEnvBeverage::InputActivate( inputdata_t &inputdata )
{
	Use( inputdata.pActivator, inputdata.pCaller, USE_ON, 0 );
}

void CEnvBeverage::InputBreak( inputdata_t &inputdata )
{
	Break( true );
}

//=========================================================
// Soda can
// TODO; Make this an actual item?
//=========================================================
ConVar sk_soda( "sk_soda", "1" );

BEGIN_DATADESC( CItemSoda )

	// Function Pointers
	DEFINE_FUNCTION( CanDrop ),
	DEFINE_FUNCTION( CanThink ),
	DEFINE_FUNCTION( CanTouch ),
	
	DEFINE_FIELD( m_flCanWarmTime, FIELD_TIME ),
	DEFINE_FIELD( m_bWarm, FIELD_BOOLEAN ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( item_sodacan, CItemSoda );

void CItemSoda::Precache( void )
{
	PrecacheModel( "models/can.mdl" );

	PrecacheScriptSound( "ItemSoda.Bounce" );
	PrecacheScriptSound( "ItemSoda.Open" );
	PrecacheScriptSound( "ItemSoda.OpenWarm" );
	PrecacheScriptSound( "Player.Drink" );
}

void CItemSoda::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_FLYGRAVITY );

	SetModel ( "models/can.mdl" );
	UTIL_SetSize( this, Vector ( 0, 0, 0 ), Vector ( 0, 0, 0 ) );

	m_flCanWarmTime = gpGlobals->curtime + 60.0f;	//Gets warm and gross after this amount of time
	m_bWarm = false;

	SetThink (&CItemSoda::CanDrop);
	SetNextThink( gpGlobals->curtime + 0.5f );
}

void CItemSoda::CanDrop( void )
{
	EmitSound( "ItemSoda.Bounce" );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER );
	UTIL_SetSize( this, Vector ( -8, -8, 0 ), Vector ( 8, 8, 8 ) );

	SetThink( &CItemSoda::CanThink );	//&CItemSoda::CanThink
	SetTouch( &CItemSoda::CanTouch );
}

void CItemSoda::CanThink( void )
{
	if ( gpGlobals->curtime >= m_flCanWarmTime )
	{
		m_bWarm = true;
		SetThink( NULL );
		SetNextThink( gpGlobals->curtime + 0.2f );
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.8f );	//0.5f
	}
}

void CItemSoda::CanTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
		return;

	Use( pOther, NULL, USE_ON, 0 ); // only the first param is used
}

void CItemSoda::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// UNDONE; Actually no, if a npc wants to use it through a script thats fine
//	if ( !pActivator->IsPlayer() )
//		return;

	if ( !m_bWarm )
	{
		EmitSound( "ItemSoda.Open" );
		pActivator->TakeHealth( sk_soda.GetFloat(), DMG_GENERIC );	// awesome, cool, refreshing beverage - a bit of health
	}
	else
	{
		EmitSound( "ItemSoda.OpenWarm" );
		if ( pActivator->GetHealth() > 2 )
			pActivator->TakeDamage( CTakeDamageInfo( this, this, 1, DMG_POISON ) );	// warm tarty liquid of gross despair - negative health
	}

	// spoit sound here
	if ( pActivator->IsPlayer() )
	{
		CPASAttenuationFilter filter( pActivator, "Player.Drink" );
		EmitSound( filter, pActivator->entindex(), "Player.Drink" );
	}

	if ( GetOwnerEntity() )
	{
		// tell the machine the can was taken
		CEnvBeverage *bev = (CEnvBeverage *)GetOwnerEntity();
		bev->m_CanInDispenser = false;
	}

	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
	SetTouch ( NULL );
	SetThink ( &CItemSoda::SUB_Remove );
	SetNextThink( gpGlobals->curtime );
}

