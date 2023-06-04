//====== Copyright � 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: Mortar Field
//
// TODO; This could use a re-write, the current way works fine but is limited.
// The field itself needs a think function, for sounds and other niceities.
//=============================================================================

#include "cbase.h"
#include "triggers.h"
#include "ndebugoverlay.h"
#include "EntityList.h"
#include "engine/IEngineSound.h"
#include "soundent.h"
#include "basegrenade_shared.h"
#include "shake.h"
#include "buttons.h"
#include "hl2_shareddefs.h"
#include "sprite.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/*************************************
	the "LaBuznik" mortar device   
**************************************/

class CFuncMortarField : public CBaseToggle
{
	DECLARE_CLASS( CFuncMortarField, CBaseToggle );
public:
	void Spawn( void );
	void Precache( void );
	//void KeyValue( KeyValueData *pkvd );

	// Bmodels don't go across transitions
	virtual int	ObjectCaps( void ) { return CBaseToggle :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

/*	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];*/

	void EXPORT FieldUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void InputTrigger( inputdata_t &inputdata );

	DECLARE_DATADESC();

	string_t	m_iszXController;
	string_t	m_iszYController;
	float		m_flSpread;
	float		m_flDelay;
	int			m_iCount;
	int			m_fControl;
	COutputEvent m_OnFire;
};

LINK_ENTITY_TO_CLASS( func_mortar_field, CFuncMortarField );

BEGIN_DATADESC( CFuncMortarField )
	DEFINE_KEYFIELD( m_iszXController,	FIELD_STRING, "m_iszXController" ),
	DEFINE_KEYFIELD( m_iszYController,	FIELD_STRING, "m_iszYController" ),
	DEFINE_KEYFIELD( m_flSpread,		FIELD_FLOAT,   "m_flSpread" ),
	DEFINE_KEYFIELD( m_flDelay,			FIELD_FLOAT,   "m_flDelay" ),
	DEFINE_KEYFIELD( m_iCount,			FIELD_INTEGER, "m_iCount" ),
	DEFINE_KEYFIELD(  m_fControl,		FIELD_INTEGER, "m_fControl" ),

	DEFINE_INPUTFUNC( FIELD_VOID,		"Trigger",		InputTrigger ),
	DEFINE_OUTPUT( m_OnFire,			"OnFire" ),
END_DATADESC()


// Drop bombs from above
void CFuncMortarField :: Spawn( void )
{
	Precache();

	SetSolid( SOLID_NONE );
	SetModel( STRING(GetModelName()) );    // set size and link into world
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
	SetUse( &CFuncMortarField::FieldUse );

	if(m_iCount == NULL)
		m_iCount = 4;
}

void CFuncMortarField :: Precache( void )
{
	enginesound->PrecacheSound("weapons/mortar/mortar_inflight.wav");
	PrecacheScriptSound("Weapon_Mortar.Incoming");
	engine->PrecacheModel( "sprites/lgtning.vmt" );
}

void CFuncMortarField::FieldUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecStart;
/*
	vecStart.x = random->RandomFloat( this->mins.x, this->maxs.x );
	vecStart.y = random->RandomFloat( this->mins.y, this->maxs.y );
	vecStart.z = this->maxs.z;
*/
	CollisionProp()->RandomPointInBounds( Vector( 0, 0, 1 ), Vector( 1, 1, 1 ), &vecStart );

	switch( m_fControl )
	{
	case 0:	// random
		break;
	case 1: // Trigger Activator
		if (pActivator != NULL)
		{
			vecStart.x = pActivator->GetAbsOrigin().x;
			vecStart.y = pActivator->GetAbsOrigin().y;
		}
		break;
	case 2: // table
		{
			CBaseEntity *pController;

			if ( m_iszXController != NULL_STRING )
			{
				pController = gEntList.FindEntityByName( NULL, STRING(m_iszXController) );
				if (pController != NULL)
				{
					if ( FClassnameIs( pController, "momentary_rot_button" ) )
					{
						CMomentaryRotButton *pXController = static_cast<CMomentaryRotButton*>( pController );

//						vecStart.x = GetAbsMins().x + pXController->GetPos( pXController->GetLocalAngles() ) * (EntitySpaceSize().x);

						Vector vecNormalizedPos( pXController->GetPos( pXController->GetLocalAngles() ), 0.0f, 0.0f );
						Vector vecWorldSpace;
						CollisionProp()->NormalizedToWorldSpace( vecNormalizedPos, &vecWorldSpace );
						vecStart.x = vecWorldSpace.x;
					}
					else
					{
						DevMsg( "func_mortarfield has X controller that isn't a momentary_rot_button.\n" );
					}
				}
			}
			if ( m_iszYController != NULL_STRING )
			{
				pController = gEntList.FindEntityByName( NULL, STRING(m_iszYController) );
				if (pController != NULL)
				{
					if ( FClassnameIs( pController, "momentary_rot_button" ) )
					{
						CMomentaryRotButton *pYController = static_cast<CMomentaryRotButton*>( pController );

						//vecStart.y = GetAbsMins().y + pYController->GetPos( pYController->GetLocalAngles() ) * (EntitySpaceSize().y);

						Vector vecNormalizedPos( 0.0f, pYController->GetPos( pYController->GetLocalAngles() ), 0.0f );
						Vector vecWorldSpace;
						CollisionProp()->NormalizedToWorldSpace( vecNormalizedPos, &vecWorldSpace );
						vecStart.y = vecWorldSpace.y;
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

	float t = m_flDelay;

	// Make sure we actually have time to play the incoming sound
	if ( t >= 1 )
	{
		// Play a incoming sound
		CPASAttenuationFilter filter( this, ATTN_NONE );
		EmitSound_t ep;
		//ep.m_nPitch = random->RandomInt(95,124);
		//ep.m_nChannel = CHAN_WEAPON;
		//ep.m_SoundLevel = SNDLVL_NONE;
		ep.m_pSoundName = "Weapon_Mortar.Incoming";	//weapons/mortar/mortar_shell_incoming1.wav
		EmitSound( filter, entindex(), ep );
	}

	// Spawn the mortars
	for (int i = 0; i < m_iCount; i++)
	{
		Vector vecSpot = vecStart;
		vecSpot.x += random->RandomFloat( -m_flSpread, m_flSpread );
		vecSpot.y += random->RandomFloat( -m_flSpread, m_flSpread );

		trace_t tr;
		UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -1 ) * MAX_TRACE_LENGTH, MASK_SOLID_BRUSHONLY, this,  COLLISION_GROUP_NONE, &tr );

		CBaseEntity *pMortar = Create( "grenade_mortar", tr.endpos, QAngle( 0, 0, 0 ), pActivator );
		pMortar->SetNextThink( gpGlobals->curtime + t );
		t += random->RandomFloat( 0.2, 0.6 );

		if (i == 0)
			CSoundEnt::InsertSound(SOUND_DANGER, tr.endpos, 400, 0.3 );
	}

	m_OnFire.FireOutput(pActivator, this);
}

void CFuncMortarField::InputTrigger( inputdata_t &inputdata )
{
	FieldUse(inputdata.pActivator, inputdata.pCaller, USE_ON, 0);
}

//-----------------------------------------------------------------------------
// TODO; For now logic is done in mortar itself
#if 0
class CMortarEffects : public CPointEntity
{
	DECLARE_CLASS( CMortarEffects, CPointEntity );
public:

	DECLARE_DATADESC();

private:
	int m_bInSkybox;
};

BEGIN_DATADESC( CMortarEffects )
	DEFINE_FIELD( m_bInSkybox, FIELD_BOOLEAN ),
END_DATADESC()
#endif

LINK_ENTITY_TO_CLASS( info_offmap_mortar, CPointEntity );

//-----------------------------------------------------------------------------
ConVar sk_mortar_dmg( "sk_mortar_dmg", "200");

//extern short	g_sModelIndexFireball;		// (in combatweapon.cpp) holds the index for the fireball 

//class CBaseGrenadeContact;
class CMortar : public CBaseGrenade
{
	DECLARE_CLASS( CMortar, CBaseGrenade);

public:
	void Precache( void );
	void Spawn( void );

	void MortarExplode( void );
	//void ShootTimed( EVARS *pevOwner, Vector vecStart, float time );

	int m_spriteTexture;
	int m_flashTexture;

	DECLARE_DATADESC();
};

BEGIN_DATADESC( CMortar )
	DEFINE_THINKFUNC( MortarExplode ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( monster_mortar, CMortar );
LINK_ENTITY_TO_CLASS( grenade_mortar, CMortar );

void CMortar::Precache( )
{
	m_spriteTexture = engine->PrecacheModel( "sprites/lgtning.vmt" );
	m_flashTexture = engine->PrecacheModel( "sprites/bexplo.vmt" );
	PrecacheScriptSound( "Weapon_Mortar.Impact" );
}

void CMortar::Spawn( )
{
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );

	SetDamage( sk_mortar_dmg.GetFloat() );
	SetDamageRadius( GetDamage() * 2.5 );

	SetThink( &CMortar::MortarExplode );
	SetNextThink( TICK_NEVER_THINK );

	Precache( );

	// Create some flashes at a info_offmap_mortar, if we can
	CBaseEntity *pFlashPoint = gEntList.FindEntityByClassname( NULL, "info_offmap_mortar" );
	if( pFlashPoint != NULL )
	{
		pFlashPoint->DetectInSkybox();
		bool bInSkybox = pFlashPoint->IsEFlagSet(EFL_IN_SKYBOX);

		Vector vecOrigin = pFlashPoint->GetAbsOrigin();
		// This is in the skybox, gotta keep the numbers small
		float iFlashSpread = bInSkybox ? 16.0 : 160.0;	//Assuming skybox scale of 32
		vecOrigin.x += random->RandomFloat( -iFlashSpread, iFlashSpread );
		vecOrigin.y += random->RandomFloat( -iFlashSpread, iFlashSpread );

		// TEMPTEMP; just using a explosion for now, could use some proper effects
		CPASFilter filter( vecOrigin );
		te->Explosion( filter, random->RandomFloat(0.0,1.0),
			&vecOrigin,
			m_flashTexture,
			bInSkybox ? 12 : 400,
			40,
			TE_EXPLFLAG_NOPARTICLES|TE_EXPLFLAG_NOFIREBALLSMOKE|TE_EXPLFLAG_NOSOUND,
			0,
			0 );
	}
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

#if 1
	EmitSound( "Weapon_Mortar.Impact" );
	CSoundEnt::InsertSound(SOUND_COMBAT, tr.endpos, 400, 0.3 );

	//ExplodeModel( pev->origin, 400, g_sModelIndexShrapnel, 30 );
#endif
}

#if 0
void CMortar::ShootTimed( EVARS *pevOwner, Vector vecStart, float time )
{
	CMortar *pMortar = GetClassPtr( (CMortar *)NULL );
	pMortar->Spawn();

	TraceResult tr;
	UTIL_TraceLine( vecStart, vecStart + Vector( 0, 0, -1 ) * 4096, ignore_monsters, ENT(pMortar->pev), &tr );

	SetNextThink( gpGlobals->curtime + time );

	UTIL_SetOrigin( pMortar->pev, tr.vecEndPos );
}
#endif


#ifdef HL2_DLL
//-----------------------------------------------------------------------
// Purpose:	Lightning field (mortarfield but with no boom) 
// TODO; This could be vastly and improved and customized,
// maybe in the future...
//-----------------------------------------------------------------------
class CFuncLightningField : public CFuncMortarField
{
	DECLARE_CLASS( CFuncLightningField, CFuncMortarField );
public:
	void Spawn( void );
	void Precache( void );

	DECLARE_DATADESC();

	void EXPORT FieldUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int	m_beamIndex;
};

LINK_ENTITY_TO_CLASS( func_lightning_field, CFuncMortarField );

BEGIN_DATADESC( CFuncLightningField )
	DEFINE_FIELD( m_beamIndex, FIELD_INTEGER ),
END_DATADESC()

// Drop bolts from above
void CFuncLightningField :: Spawn( void )
{
	Precache();

	SetSolid( SOLID_NONE );
	SetModel( STRING(GetModelName()) );    // set size and link into world
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
	SetUse( &CFuncLightningField::FieldUse );

	if(m_iCount == NULL)
		m_iCount = 1;
}

void CFuncLightningField::Precache( void )
{
	m_beamIndex = PrecacheModel( "sprites/lgtning.vmt" );
	BaseClass::Precache();
}

void CFuncLightningField::FieldUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecStart;
/*
	vecStart.x = random->RandomFloat( this->mins.x, this->maxs.x );
	vecStart.y = random->RandomFloat( this->mins.y, this->maxs.y );
	vecStart.z = this->maxs.z;
*/
	CollisionProp()->RandomPointInBounds( Vector( 0, 0, 1 ), Vector( 1, 1, 1 ), &vecStart );

	switch( m_fControl )
	{
	case 0:	// random
		break;
	case 1: // Trigger Activator
		if (pActivator != NULL)
		{
			vecStart.x = pActivator->GetAbsOrigin().x;
			vecStart.y = pActivator->GetAbsOrigin().y;
		}
		break;
	}

	// Spawn the beams
	for (int i = 0; i < m_iCount; i++)
	{
		Vector vecSpot = vecStart;
		vecSpot.x += random->RandomFloat( -m_flSpread, m_flSpread );
		vecSpot.y += random->RandomFloat( -m_flSpread, m_flSpread );

		trace_t tr;
		UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -1 ) * MAX_TRACE_LENGTH, MASK_SOLID_BRUSHONLY, this,  COLLISION_GROUP_NONE, &tr );

		UTIL_Beam(  vecStart,
					tr.endpos,
					m_beamIndex,
					0,			//halo index
					0,			//frame start
					2.0f,		//framerate
					random->RandomFloat( 0.05, 0.2 ),	//life
					4.0,		// width
					4.0,		// endwidth
					100,		// fadelength,
					1,			// noise
					100,		// red
					160,		// green
					255,		// blue,
					255,		// bright
					0			// speed
					);
	}

	m_OnFire.FireOutput(pActivator, this);
}
#endif