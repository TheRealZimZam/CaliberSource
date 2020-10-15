//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "engine/IEngineSound.h"
#include "doors.h"
#include "soundent.h"
#include "shake.h"
#include "sprite.h"
#include "func_break.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFuncMortarField : public CBaseToggle
{
	DECLARE_CLASS( CFuncMortarField, CBaseToggle );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );

	// Bmodels don't go across transitions
	virtual int	ObjectCaps( void ) { return CBaseToggle :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

/*	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];*/

	//void EXPORT FieldUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void InputTrigger ( inputdata_t &inputdata );

	DECLARE_DATADESC();

	string_t	m_iszXController;
	string_t	m_iszYController;
	float		m_flSpread;
	float		m_flDelay;
	int			m_iCount;
	int			m_fControl;
};

LINK_ENTITY_TO_CLASS( func_mortar_field, CFuncMortarField );

BEGIN_DATADESC( CFuncMortarField )
	DEFINE_KEYFIELD( CFuncMortarField, m_iszXController, FIELD_STRING, "m_iszXController" ),
	DEFINE_KEYFIELD( CFuncMortarField, m_iszYController, FIELD_STRING, "m_iszYController" ),
	DEFINE_KEYFIELD( CFuncMortarField, m_flSpread,		FIELD_FLOAT,   "m_flSpread" ),
	DEFINE_KEYFIELD( CFuncMortarField, m_iCount,		FIELD_INTEGER, "m_iCount" ),
	DEFINE_KEYFIELD( CFuncMortarField, m_fControl,		FIELD_INTEGER, "m_fControl" ),

	DEFINE_INPUTFUNC( CFuncMortarField,	FIELD_VOID,		"Trigger",		InputTrigger ),
END_DATADESC()


// Drop bombs from above
void CFuncMortarField :: Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetModel( STRING(GetModelName()) );    // set size and link into world
	SetMoveType( MOVETYPE_NONE );
	m_fEffects |= EF_NODRAW;
//	SetUse( FieldUse );
	Precache();
}


void CFuncMortarField :: Precache( void )
{
	enginesound->PrecacheSound ("weapons/mortar.wav");
	enginesound->PrecacheSound ("weapons/mortarhit.wav");
	engine->PrecacheModel( "sprites/lgtning.vmt" );
}

void CFuncMortarField::InputTrigger( inputdata_t &inputdata )
{
	Vector vecStart;
	RandomPointInBounds( Vector( 0, 0, 1 ), Vector( 1, 1, 1 ), &vecStart );

	switch( m_fControl )
	{
	case 0:	// random
		break;
	case 1: // Trigger Activator
		if (inputdata.pActivator != NULL)
		{
			vecStart.x = inputdata.pActivator->GetAbsOrigin().x;
			vecStart.y = inputdata.pActivator->GetAbsOrigin().y;
		}
		break;
	case 2: // table
		{
			CBaseEntity *pController;

			if ( m_iszXController != NULL_STRING )
			{
				pController = gEntList.FindEntityByName( NULL, STRING(m_iszXController), NULL );
				if (pController != NULL)
				{
					if ( FClassnameIs( pController, "momentary_rot_button" ) )
					{
						CMomentaryRotButton *pXController = static_cast<CMomentaryRotButton*>( pController );

						vecStart.x = GetAbsMins().x + pXController->GetPos( pXController->GetLocalAngles() ) * (EntitySpaceSize().x);
					}
					else
					{
						DevMsg( "func_mortarfield has X controller that isn't a momentary_rot_button.\n" );
					}
				}
			}
			if ( m_iszYController != NULL_STRING )
			{
				pController = gEntList.FindEntityByName( NULL, STRING(m_iszYController), NULL );
				if (pController != NULL)
				{
					if ( FClassnameIs( pController, "momentary_rot_button" ) )
					{
						CMomentaryRotButton *pYController = static_cast<CMomentaryRotButton*>( pController );

						vecStart.y = GetAbsMins().y + pYController->GetPos( pYController->GetLocalAngles() ) * (EntitySpaceSize().y);
					}
					else
					{
						DevMsg( "func_mortarfield has Y controller that isn't a momentary_rot_button.\n" );
					}
				}
			}
		}
		break;
	}

	int pitch = random->RandomInt(95,120);

	CPASAttenuationFilter filter( this, ATTN_NONE );
	enginesound->EmitSound( filter, entindex(), CHAN_VOICE, "weapons/mortar.wav", 1.0, ATTN_NONE, 0, pitch);	

	float t = 2.5;
	for (int i = 0; i < m_iCount; i++)
	{
		Vector vecSpot = vecStart;
		vecSpot.x += random->RandomFloat( -m_flSpread, m_flSpread );
		vecSpot.y += random->RandomFloat( -m_flSpread, m_flSpread );

		trace_t tr;
		UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -1 ) * MAX_TRACE_LENGTH, MASK_SOLID_BRUSHONLY, this,  COLLISION_GROUP_NONE, &tr );

		CBaseEntity *pMortar = Create( "npc_mortar", tr.endpos, QAngle( 0, 0, 0 ), inputdata.pActivator );
		pMortar->SetNextThink( gpGlobals->curtime + t );
		t += random->RandomFloat( 0.2, 0.5 );

		if (i == 0)
			CSoundEnt::InsertSound ( SOUND_DANGER, tr.endpos, 400, 0.3 );
	}
}

class CMortar : public CHL1BaseGrenade
{
	DECLARE_CLASS( CMortar, CHL1BaseGrenade );

public:
	void Spawn( void );
	void Precache( void );

	void MortarExplode( void );

	int m_spriteTexture;

	DECLARE_DATADESC();
};

BEGIN_DATADESC( CMortar )
	DEFINE_FUNCTION( CMortar, MortarExplode ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_mortar, CMortar );

void CMortar::Spawn( )
{
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );

	SetDamage( 200 );
	SetDamageRadius( GetDamage() * 2.5 );

	SetThink( MortarExplode );
	SetNextThink( TICK_NEVER_THINK );

	Precache( );
}


void CMortar::Precache( )
{
	m_spriteTexture = engine->PrecacheModel( "sprites/lgtning.vmt" );
}

void CMortar::MortarExplode( void )
{
	Vector vecStart	= GetAbsOrigin();
	Vector vecEnd	= vecStart;
	vecEnd.z += 1024;

	UTIL_Beam( vecStart, vecEnd, m_spriteTexture, 0, 0, 0, 0.5, 4.0, 4.0, 100, 0, 255, 160, 100, 128, 0 );

	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin() + Vector( 0, 0, 1024 ), GetAbsOrigin() - Vector( 0, 0, 1024 ), MASK_ALL, this, COLLISION_GROUP_NONE, &tr );

	Explode( &tr, DMG_BLAST | DMG_MISSILEDEFENSE );
	UTIL_ScreenShake( tr.endpos, 25.0, 150.0, 1.0, 750, SHAKE_START );
}
