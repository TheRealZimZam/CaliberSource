//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Concussive blast
//
// Todo's; Code this thing;
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "soundent.h"
#include "mathlib/mathlib.h" 
#include "grenade_concussive.h"
#include "game.h"
#include "ndebugoverlay.h"
#include "soundent.h"
#include "hl2_shareddefs.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define STUN_GRENADE_GRACE_TIME_AFTER_PICKUP 1.5f
#define STUN_GRENADE_WARN_TIME 1.5f

ConVar sk_stun_radius			( "sk_stun_radius", "300");

#define STUNGRENADE_MODEL "models/Weapons/w_grenade.mdl"

//Concussive explosion entity
IMPLEMENT_SERVERCLASS_ST( CTEConcussiveExplosion, DT_TEConcussiveExplosion )
	SendPropVector( SENDINFO(m_vecNormal), -1, SPROP_COORD ),
	SendPropFloat( SENDINFO(m_flScale), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO(m_nRadius), 32, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_nMagnitude), 32, SPROP_UNSIGNED ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTEConcussiveExplosion::CTEConcussiveExplosion( const char *name ) : BaseClass( name )
{
	m_nRadius		= 0;
	m_nMagnitude	= 0;
	m_flScale		= 0.0f;

	m_vecNormal.Init();
}

CTEConcussiveExplosion::~CTEConcussiveExplosion( void )
{
}

// Singleton to fire TEExplosion objects
static CTEConcussiveExplosion g_TEConcussiveExplosion( "ConcussiveExplosion" );

void TE_ConcussiveExplosion( IRecipientFilter& filter, float delay,
	const Vector* pos, float scale, int radius, int magnitude, const Vector* normal )
{
	g_TEConcussiveExplosion.m_vecOrigin		= *pos;
	g_TEConcussiveExplosion.m_flScale			= scale;
	g_TEConcussiveExplosion.m_nRadius			= radius;
	g_TEConcussiveExplosion.m_nMagnitude		= magnitude;

	if ( normal )
		g_TEConcussiveExplosion.m_vecNormal	= *normal;
	else 
		g_TEConcussiveExplosion.m_vecNormal	= Vector(0,0,1);

	// Send it over the wire
	g_TEConcussiveExplosion.Create( filter, delay );
}

//Temp ent for the blast
LINK_ENTITY_TO_CLASS( concussiveblast, CConcussiveBlast );

BEGIN_DATADESC( CConcussiveBlast )
//	DEFINE_FIELD( m_spriteTexture,	FIELD_INTEGER ),
END_DATADESC()

void CConcussiveBlast::Explode( float magnitude )
{
	//Create a concussive explosion
	CPASFilter filter( GetAbsOrigin() );

	Vector vecForward;
	AngleVectors( GetAbsAngles(), &vecForward );
	TE_ConcussiveExplosion( filter, 0.0,
		&GetAbsOrigin(),//position
		1.0f,	//scale
		256*magnitude,	//radius
		175*magnitude,	//magnitude
		&vecForward );	//normal
		
	int	colorRamp = random->RandomInt( 128, 255 );

	//Shockring
	CBroadcastRecipientFilter filter2;
		te->BeamRingPoint( filter2, 0, 
		GetAbsOrigin(),	//origin
		16,			//start radius
		300*magnitude,		//end radius
		m_spriteTexture, //texture
		0,			//halo index
		0,			//start frame
		2,			//framerate
		0.3f,		//life
		128,		//width
		16,			//spread
		0,			//amplitude
		colorRamp,	//r
		colorRamp,	//g
		255,		//g
		24,			//a
		128			//speed
		);

	//Do the radius damage
	//RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), 200, DMG_BLAST|DMG_DISSOLVE ), GetAbsOrigin(), 256, CLASS_NONE, NULL );

	UTIL_Remove( this );
}

void CreateConcussiveBlast( const Vector &origin, const Vector &surfaceNormal, CBaseEntity *pOwner, float magnitude )
{
	QAngle angles;
	VectorAngles( surfaceNormal, angles );
	CConcussiveBlast *pBlast = (CConcussiveBlast*)CBaseEntity::Create( "concussiveblast", origin, angles, pOwner );
	if ( pBlast )
	{
		pBlast->Explode( magnitude );
	}
}

//-----------------------------------------------------------------------------
// Purpose: CGrenadeConcussive
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CGrenadeConcussive )

	DEFINE_FIELD( m_inSolid, FIELD_BOOLEAN),

	// Function pointers
//	DEFINE_THINKFUNC( BeamThink ),
//	DEFINE_ENTITYFUNC( GrenadeConcussiveTouch ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( grenade_concussive, CGrenadeConcussive );

CGrenadeConcussive::CGrenadeConcussive(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeConcussive::Precache( void )
{
	PrecacheModel( STUNGRENADE_MODEL );

	UTIL_PrecacheOther( "concussiveblast" );

	PrecacheScriptSound( "GrenadeConcussive.Detonate" );
	PrecacheScriptSound( "GrenadeConcussive.Ringing" );
	PrecacheScriptSound( "GrenadeConcussive.StopSounds" );	//Shouldnt be needed, but just in case

	PrecacheModel( "sprites/lgtning.vmt" );	//For electric beams that shoot out before det

	BaseClass::Precache();
}

void CGrenadeConcussive::Spawn( void )
{
	Precache( );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );
	SetCollisionGroup( COLLISION_GROUP_WEAPON );

	SetModel( STUNGRENADE_MODEL );
	UTIL_SetSize(this, Vector( -4, -4, -2), Vector(4, 4, 2));

	// 5 dmg = full effects, peter out with less damage
	if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() )
		m_flDamage		= 6;
	else
		m_flDamage		= 5;

	m_DmgRadius		= sk_stun_radius.GetFloat();
	m_takedamage	= DAMAGE_YES;
	m_iHealth		= 5;

	SetFriction( 0.8 );  // Give a little bounce so can flatten
	SetSequence( 1 );

	CreateVPhysics();

	AddSolidFlags( FSOLID_NOT_STANDABLE );

	BaseClass::Spawn();
}


//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeConcussive::Event_Killed( const CTakeDamageInfo &info )
{
	Detonate();
}

void CGrenadeConcussive::Detonate(void)
{
	m_takedamage	= DAMAGE_NO;	

	// Now do the fisvisible to see if the target gets rung or not
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityInSphere( pEntity, GetAbsOrigin(), m_DmgRadius ) ) != NULL )
	{
		CBaseCombatCharacter *pBCC = ToBaseCombatCharacter( pEntity );
		if( pBCC )
		{
			if ( pBCC->IsPlayer() )
			{
				// If its a player, check body direction
				pBCC->TakeDamage( CTakeDamageInfo( this, this, m_flDamage, DMG_SONIC ) );
			}
			else
			{
				pBCC->TakeDamage( CTakeDamageInfo( this, this, m_flDamage, DMG_SONIC ) );
			}
		}
		else
		{
			pEntity->TakeDamage( CTakeDamageInfo( this, GetOwnerEntity(), m_flDamage, DMG_CLUB ) );
		}
	}

	EmitSound( "GrenadeConcussive.Detonate" );

	CreateConcussiveBlast( GetAbsOrigin(), 0, GetOwnerEntity(), (m_DmgRadius*0.03) );

	RemoveEffects( EF_NODRAW );
	SetAbsVelocity( vec3_origin );
	SetNextThink( gpGlobals->curtime + 0.2 );

	UTIL_Remove( this );
}
