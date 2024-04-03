//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Grenade used by the city scanner
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "grenade_homer.h"
#include "weapon_ar2.h"
#include "soundent.h"
#include "decals.h"
#include "shake.h"
#include "smoke_trail.h"
#include "fire.h"
#include "ar2_explosion.h"
#include "mathlib/mathlib.h" 
#include "game.h"
#include "gamerules.h"		
#include "ndebugoverlay.h"
#include "hl2_shareddefs.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	HOMER_TRAIL0_LIFE	0.2	//Exhaust
#define	HOMER_TRAIL1_LIFE	2.0	//Smoketrail
#define	HOMER_TRAIL2_LIFE	0.1	//Fire
#define	BUBBLE_WARNING		1	//Should the danger be issued once, or over a period in a cone?

extern short	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

ConVar    homer_grenade_obstacle_avoidance( "homer_grenade_obstacle_avoidance","0" );

ConVar    sk_dmg_homer_grenade( "sk_dmg_homer_grenade","0" );
ConVar	  sk_homer_grenade_radius( "sk_homer_grenade_radius","0" );

BEGIN_DATADESC( CGrenadeHomer )

	DEFINE_ARRAY( m_hRocketTrail,				FIELD_EHANDLE, 3 ),
	DEFINE_FIELD( m_sFlySound,					FIELD_STRING),
//	DEFINE_FIELD( m_flNextFlySoundTime,			FIELD_TIME),
	DEFINE_FIELD( m_flNextWarnTime,				FIELD_TIME),

	DEFINE_FIELD( m_flHomingStrength,			FIELD_FLOAT),
	DEFINE_FIELD( m_flHomingDelay,				FIELD_FLOAT),
	DEFINE_FIELD( m_flHomingRampUp,				FIELD_FLOAT),
	DEFINE_FIELD( m_flHomingDuration,			FIELD_FLOAT),
	DEFINE_FIELD( m_flHomingRampDown,			FIELD_FLOAT),
	DEFINE_FIELD( m_flHomingSpeed,				FIELD_FLOAT),
	DEFINE_FIELD( m_flSpinMagnitude,			FIELD_FLOAT),
	DEFINE_FIELD( m_flSpinSpeed,				FIELD_FLOAT),
	DEFINE_FIELD( m_nRocketTrailType,			FIELD_INTEGER),
	DEFINE_FIELD( m_nRocketType,				FIELD_INTEGER),

//	DEFINE_FIELD( m_spriteTexture,				FIELD_INTEGER),

	DEFINE_FIELD( m_flHomingLaunchTime,			FIELD_TIME),
	DEFINE_FIELD( m_flHomingStartTime,			FIELD_TIME ),
	DEFINE_FIELD( m_flHomingEndTime,			FIELD_TIME ),
	DEFINE_FIELD( m_flSpinOffset,				FIELD_FLOAT),
	
	DEFINE_FIELD( m_hTarget,					FIELD_EHANDLE),

	// Function pointers
	DEFINE_THINKFUNC( AimThink ),
	DEFINE_ENTITYFUNC( GrenadeHomerTouch ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( grenade_homer, CGrenadeHomer );

///------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
CGrenadeHomer* CGrenadeHomer::CreateGrenadeHomer( string_t sModelName, string_t sFlySound, const Vector &vecOrigin, const QAngle &vecAngles, edict_t *pentOwner )
{
	CGrenadeHomer *pGrenade = (CGrenadeHomer*)CreateEntityByName( "grenade_homer" );
	if ( !pGrenade )
	{
		Warning( "NULL Ent in Create!\n" );
		return NULL;
	}

	if ( pGrenade->edict() )
	{
		pGrenade->m_sFlySound	= sFlySound;
		pGrenade->SetOwnerEntity( Instance( pentOwner ) );
		pGrenade->SetLocalOrigin( vecOrigin );
		pGrenade->SetLocalAngles( vecAngles );
		pGrenade->SetModel( STRING(sModelName) );
	}
	return pGrenade;
}


void CGrenadeHomer::Precache( void )
{
	m_spriteTexture = PrecacheModel( "sprites/lgtning.vmt" );

	PrecacheScriptSound( "GrenadeHomer.StopSounds" );
	if ( m_sFlySound != NULL_STRING )
		PrecacheScriptSound( STRING(m_sFlySound) );
}


//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeHomer::Spawn( void )
{
	Precache( );

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLY );

	UTIL_SetSize(this, Vector(0, 0, 0), Vector(0, 0, 0));

	SetDamage(sk_dmg_homer_grenade.GetFloat());
	SetDamageRadius(sk_homer_grenade_radius.GetFloat());
	m_takedamage	= DAMAGE_YES;
	m_iHealth		= 1;

	SetGravity( 1.0 );
	SetFriction( 0.8 );
	SetSequence( 1 );

	m_flHomingStrength	= 0;
	m_flHomingDelay		= 0;
	m_flHomingDuration	= 0;

	SetCollisionGroup( HL2COLLISION_GROUP_HOMING_MISSILE ); 
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeHomer::SetSpin(float flSpinMagnitude, float flSpinSpeed)
{
	m_flSpinMagnitude	= flSpinMagnitude;
	m_flSpinSpeed		= flSpinSpeed;
	m_flSpinOffset		= random->RandomInt(-m_flSpinSpeed,m_flSpinSpeed);
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeHomer::SetHoming(float flStrength, float flDelay, float flRampUp, float flDuration, float flRampDown)
{
	m_flHomingStrength	= flStrength;
	m_flHomingDelay		= flDelay;
	m_flHomingRampUp	= flRampUp;
	m_flHomingDuration	= flDuration;
	m_flHomingRampDown	= flRampDown;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeHomer::StartRocketTrail(bool bTrail1, bool bTrail2, bool bTrail3)
{
	//!!!TODO
	// TODO; This is destroying the entlimit!!!

	// Exhaust
	// Always show this at any distance - the other two can
	// be safely turned off if needed for perf - MM
	RocketTrail *pRocketTrail =  RocketTrail::CreateRocketTrail();

	if(bTrail1 && pRocketTrail)
	{
		pRocketTrail->m_SpawnRate = 60;
		pRocketTrail->m_ParticleLifetime = HOMER_TRAIL0_LIFE;
		if ( m_nRocketTrailType == HOMER_SMOKE_TRAIL_ALIEN )
			pRocketTrail->m_StartColor.Init(0.2, 0.0, 0.5);
		else
			pRocketTrail->m_StartColor.Init(0.5, 0.5, 0.2);

		pRocketTrail->m_Opacity = 0.35f;
		pRocketTrail->m_EndColor.Init(0.6,0.6,0.5);
		pRocketTrail->m_StartSize = 8;
		pRocketTrail->m_EndSize = 16;
		pRocketTrail->m_SpawnRadius = 3;
		pRocketTrail->m_MinSpeed = 10;
		pRocketTrail->m_MaxSpeed = 20;
		pRocketTrail->SetLifetime(120);
		pRocketTrail->FollowEntity(this);

		m_hRocketTrail[0] = pRocketTrail;
	}

	// Smoketrail
	pRocketTrail = RocketTrail::CreateRocketTrail();
	if(bTrail2 && pRocketTrail)
	{
		pRocketTrail->m_SpawnRate = 50;
		pRocketTrail->m_ParticleLifetime = HOMER_TRAIL1_LIFE;
		if ( m_nRocketTrailType == HOMER_SMOKE_TRAIL_ALIEN )
			pRocketTrail->m_StartColor.Init(0.1, 0.0, 0.1);
		else
			pRocketTrail->m_StartColor.Init(0.1, 0.1, 0.1);

		pRocketTrail->m_Opacity = 0.6f;
		pRocketTrail->m_EndColor.Init(0.5,0.5,0.5);
		pRocketTrail->m_StartSize = 8;
		pRocketTrail->m_EndSize = 20;
		pRocketTrail->m_SpawnRadius = 2;
		pRocketTrail->m_MinSpeed = 15;
		pRocketTrail->m_MaxSpeed = 25;
		pRocketTrail->SetLifetime(120);
		pRocketTrail->FollowEntity(this);

		m_hRocketTrail[1] = pRocketTrail;
	}

#if 0
	// Fire
	pRocketTrail = RocketTrail::CreateRocketTrail();
	if(bTrail3 && pRocketTrail)
	{
		pRocketTrail->m_SpawnRate = 80;
		pRocketTrail->m_ParticleLifetime = HOMER_TRAIL2_LIFE;
		if ( m_nRocketTrailType == HOMER_SMOKE_TRAIL_ALIEN )
			pRocketTrail->m_StartColor.Init(0.0, 0.5, 0.5);
		else
			pRocketTrail->m_StartColor.Init(0.75, 0.75, 0.5);

		pRocketTrail->m_Opacity = 0.7f;
		pRocketTrail->m_EndColor.Init(0.4,0.4,0.4);
		pRocketTrail->m_StartSize = 3;
		pRocketTrail->m_EndSize = 6;
		pRocketTrail->m_SpawnRadius = 1;
		pRocketTrail->m_MinSpeed = 2;
		pRocketTrail->m_MaxSpeed = 10;
		pRocketTrail->SetLifetime(120);
		pRocketTrail->FollowEntity(this);

		m_hRocketTrail[2] = pRocketTrail;
	}
#endif
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeHomer::UpdateRocketTrail(float fScale)
{
	if (m_hRocketTrail[0] == NULL)
	{
		StartRocketTrail(true, false, false);
	}

	// 0 is always the exhaust trail (important!!)
	if (m_hRocketTrail[0]) 
		m_hRocketTrail[0]->m_ParticleLifetime = fScale*HOMER_TRAIL0_LIFE + HOMER_TRAIL0_LIFE;

	// In SP, if im far away from the player, dont show the extra trails
	if (!g_pGameRules->IsMultiplayer())
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);
		float flPlayerDist = pPlayer->GetAbsOrigin().DistToSqr(GetAbsOrigin());
		if ( pPlayer && flPlayerDist > 4000 )	//TODO; Magic Number//
		{
			// Stop the smoketrail at 6k, extra fire at 4k
			StopCosmeticTrails( (flPlayerDist > 6000) ? true : false, true );
			return;
		}
		else
		{
			if (m_hRocketTrail[1] == NULL)
				StartRocketTrail(false, true, false);

			if (m_hRocketTrail[2] == NULL)
				StartRocketTrail(false, false, true);
		}
	}
	
	if (m_hRocketTrail[1])
		m_hRocketTrail[1]->m_ParticleLifetime = fScale*HOMER_TRAIL1_LIFE;

	if (m_hRocketTrail[2]) 
		m_hRocketTrail[2]->m_ParticleLifetime = fScale*HOMER_TRAIL2_LIFE;
}

void CGrenadeHomer::StopRocketTrail()
{
	// Stop all the trails
	for (int i=0;i<3;i++)
	{
		if(m_hRocketTrail[i])
		{
			m_hRocketTrail[i]->SetEmit(false);
			UTIL_Remove( m_hRocketTrail[i] );
			m_hRocketTrail[i] = NULL;
		}
	}	
}

void CGrenadeHomer::StopCosmeticTrails(bool bTrail1, bool bTrail2)
{
	// Stop the cosmetic trails
	if (m_hRocketTrail[1])
	{
		m_hRocketTrail[1]->SetEmit(false);
		UTIL_Remove( m_hRocketTrail[1] );
		m_hRocketTrail[1] = NULL;
	}

	if (m_hRocketTrail[2])
	{
		m_hRocketTrail[2]->SetEmit(false);
		UTIL_Remove( m_hRocketTrail[2] );
		m_hRocketTrail[2] = NULL;
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeHomer::Launch( CBaseEntity*		pOwner, 
							CBaseEntity*		pTarget, 
							const Vector&		vInitVelocity,
							float				flHomingSpeed,
							float				flGravity,
							int					nRocketTrailType)
{
	SetOwnerEntity( pOwner );
	m_hTarget					= pTarget;
	SetAbsVelocity( vInitVelocity );
	m_flHomingSpeed				= flHomingSpeed;
	SetGravity( flGravity );
	m_nRocketTrailType			= nRocketTrailType;

	// ----------------------------
	// Initialize homing parameters
	// ----------------------------
	m_flHomingLaunchTime	= gpGlobals->curtime;

	// -------------
	// Smoke trail.
	// -------------
	if ( (m_nRocketTrailType == HOMER_SMOKE_TRAIL_ON) || (m_nRocketTrailType == HOMER_SMOKE_TRAIL_ALIEN) )
	{
		// TODO; Get the current amount of entities
		// Close to the Entlimit? Dont start the cosmetic trails...
		StartRocketTrail();
	}

	PlayFlySound();

	SetUse( &CGrenadeHomer::DetonateUse );
	SetTouch( &CGrenadeHomer::GrenadeHomerTouch );
	SetThink( &CGrenadeHomer::AimThink );
	AimThink();
	SetNextThink( gpGlobals->curtime + 0.1f );

#ifndef BUBBLE_WARNING
	// Issue danger!
	if ( pTarget )
	{
		// Figure out how long it'll take for me to reach the target.
		float flDist = ( pTarget->WorldSpaceCenter() - WorldSpaceCenter() ).Length();
		float flTime = max( 0.5, flDist / GetAbsVelocity().Length() );

		CSoundEnt::InsertSound( SOUND_DANGER, m_hTarget->GetAbsOrigin(), 300, flTime, pOwner );
	}
#endif
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeHomer::Event_Killed( const CTakeDamageInfo &info )
{
	Detonate( );
}

void CGrenadeHomer::GrenadeHomerTouch( CBaseEntity *pOther )
{
	Assert( pOther );

	// Don't take damage from other homing grenades so can shoot in vollies
	if (FClassnameIs( pOther, "grenade_homer") || !pOther->IsSolid() )
	{
		return;
	}

	// AP rocket? Take full direct damage
	if (m_nRocketType == ARMORPIERCING && pOther->m_takedamage)
		pOther->TakeDamage( CTakeDamageInfo( this, GetOwnerEntity(), GetDamage(), (DMG_BLAST|DMG_DIRECT) ) );

	// ----------------------------------
	// If I hit the sky, don't explode
	// ----------------------------------
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + GetAbsVelocity(),  MASK_SOLID_BRUSHONLY, 
		this, COLLISION_GROUP_NONE, &tr);

	Detonate( (tr.surface.flags & SURF_SKY ? true : false) );
}

void CGrenadeHomer::Detonate( bool bToSpace )
{
	StopRocketTrail();
	StopSound(entindex(), CHAN_BODY, STRING(m_sFlySound));

	m_takedamage = DAMAGE_NO;	

	if ( !bToSpace )
	{
		if (m_nRocketType != INCENDIARY)
		{
			CPASFilter filter( GetAbsOrigin() );
			te->Explosion( filter, 0.0,
				&GetAbsOrigin(), 
				g_sModelIndexFireball,
				2.0, 
				40,
				TE_EXPLFLAG_NONE,
				GetDamageRadius(),
				GetDamage());
		}

	//	int magnitude = 1.0;
	//	int	colorRamp = random->RandomInt( 128, 255 );
		if ( m_nRocketTrailType == HOMER_SMOKE_TRAIL_ALIEN )
		{
			// Add a shockring
			CBroadcastRecipientFilter filter3;
			te->BeamRingPoint( filter3, 0, 
				GetAbsOrigin(),	//origin
				16,			//start radius
				1000,		//end radius
				m_spriteTexture, //texture
				0,			//halo index
				0,			//start frame
				2,			//framerate
				0.3f,		//life
				128,		//width
				16,			//spread
				0,			//amplitude
				100,		//r
				0,			//g
				200,		//b
				50,			//a
				128			//speed
				);

			// Add a shockring
			CBroadcastRecipientFilter filter4;
			te->BeamRingPoint( filter4, 0, 
				GetAbsOrigin(),	//origin
				16,			//start radius
				500,		//end radius
				m_spriteTexture, //texture
				0,			//halo index
				0,			//start frame
				2,			//framerate
				0.3f,		//life
				128,		//width
				16,			//spread
				0,			//amplitude
				200,		//r
				0,			//g
				100,		//b
				50,			//a
				128			//speed
				);
		}

		Vector vecForward = GetAbsVelocity();
		VectorNormalize(vecForward);
		trace_t		tr;
		UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60*vecForward,  MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, & tr);
		UTIL_DecalTrace( &tr, "Scorch" );

		switch(m_nRocketType)
		{
			case EXPLOSIVE:
				UTIL_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );
				RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), GetDamage(), DMG_BLAST ), GetAbsOrigin(), GetDamageRadius(), CLASS_NONE, NULL );
			break;

			case EXPLOSIVEINCENDIARY:
				RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), GetDamage(), DMG_BURN ), GetAbsOrigin(), GetDamageRadius(), CLASS_NONE, NULL );
			case INCENDIARY:
			int i;
			for( i = 0 ; i < (GetDamageRadius()/10) ; i++ )
			{
				FireSystem_StartFire( GetAbsOrigin(), (16.0f + GetDamage()), 0.2f, 10.0f, (SF_FIRE_START_ON|SF_FIRE_SMOKELESS|SF_FIRE_NO_GLOW), (CBaseEntity*) this, FIRE_NATURAL );
			}
			break;

			case ARMORPIERCING:
				// Touch takes care of the majority damage, just do half blast damage
				RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), (GetDamage()/2), DMG_BLAST ), GetAbsOrigin(), GetDamageRadius(), CLASS_NONE, NULL );
			break;
		}
	}

	CPASAttenuationFilter filter2( this, "GrenadeHomer.StopSounds" );
	EmitSound( filter2, entindex(), "GrenadeHomer.StopSounds" );
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CGrenadeHomer::PlayFlySound(void)
{
//	if (gpGlobals->curtime > m_flNextFlySoundTime)
//	{
		CPASAttenuationFilter filter( this, 0.8 );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = STRING(m_sFlySound);
		ep.m_flVolume = 1.0f;
		ep.m_SoundLevel = SNDLVL_NORM;
		ep.m_nPitch = 100;

		EmitSound( filter, entindex(), ep );

		//m_flNextFlySoundTime	= gpGlobals->curtime + 1.0;
//	}
}

//------------------------------------------------------------------------------
// Purpose : Move toward targetmap 
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeHomer::AimThink( void )
{ 
	// Blow up the missile if we have an explicit detonate time that
	// has been reached
	if (m_flDetonateTime != 0 &&
		gpGlobals->curtime > m_flDetonateTime)
	{
		Detonate();
		return;
	}

	Vector		vTargetPos	= vec3_origin;
	float		flCurHomingStrength = 0;

	// ------------------------------------------------
	//  If I'm homing
	// ------------------------------------------------
	if (m_hTarget != NULL)
	{
		// Only target the eyes if its a living thing
		if ( m_hTarget->MyCombatCharacterPointer() )
			vTargetPos = m_hTarget->EyePosition();
		else
			vTargetPos = m_hTarget->GetAbsOrigin();

		Vector vTargetDir = vTargetPos - GetAbsOrigin();
		VectorNormalize(vTargetDir);

		//!!TODO; Broken as hell, it is a cool zig-zag effect but it fucks the targeting,
		//!! If it can be fixed properly, re-enable
		if ( homer_grenade_obstacle_avoidance.GetBool() )
		{
			// --------------------------------------------------
			//  If my target is far away do some primitive
			//  obstacle avoidance
			// --------------------------------------------------
			if ((vTargetPos - GetAbsOrigin()).Length() > 600)
			{
				Vector  vTravelDir	= GetAbsVelocity();
				VectorNormalize(vTravelDir);
				vTravelDir *= 50;

				trace_t tr;
				UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vTravelDir, MASK_SHOT, m_hTarget, COLLISION_GROUP_NONE, &tr );
				if (tr.fraction < 1.0)	//!=
				{
					// Head off in normal 
					float dotPr			=  DotProduct(vTravelDir,tr.plane.normal);
					Vector vBounce		=  -dotPr * tr.plane.normal;
					vBounce.z			=  0;
					VectorNormalize(vBounce);
					vTargetDir			+= vBounce;
					VectorNormalize(vTargetDir);
					// DEBUG TOOL
					//NDebugOverlay::Line(GetOrigin(), GetOrigin()+vTravelDir, 255,0,0, true, 20);
					//NDebugOverlay::Line(GetOrigin(), GetOrigin()+(12*tr.plane.normal), 0,0,255, true, 20);
					//NDebugOverlay::Line(GetOrigin(), GetOrigin()+(vTargetDir), 0,255,0, true, 20);
				}
			}
		}

		float	flTargetSpeed					= GetAbsVelocity().Length();
		float	flHomingRampUpStartTime			= m_flHomingLaunchTime		+ m_flHomingDelay;
		float	flHomingSustainStartTime		= flHomingRampUpStartTime	+ m_flHomingRampUp;
		float	flHomingRampDownStartTime		= flHomingSustainStartTime	+ m_flHomingDuration;
		float	flHomingEndHomingTime			= flHomingRampDownStartTime + m_flHomingRampDown;
		// ---------
		// Delay
		// ---------
		if		(gpGlobals->curtime < flHomingRampUpStartTime)
		{
			flCurHomingStrength = 0;
			flTargetSpeed		= 0;
		}
		// ----------
		//  Ramp Up
		// ----------
		else if (gpGlobals->curtime < flHomingSustainStartTime)
		{
			float flAge			= gpGlobals->curtime - flHomingRampUpStartTime;
			flCurHomingStrength = m_flHomingStrength * (flAge/m_flHomingRampUp);
			flTargetSpeed		= flCurHomingStrength * m_flHomingSpeed;
		}
		// ----------
		//  Sustain
		// ----------
		else if (gpGlobals->curtime < flHomingRampDownStartTime)
		{
			flCurHomingStrength = m_flHomingStrength;
			flTargetSpeed		= m_flHomingSpeed;
		}
		// -----------
		//  Ramp Down
		// -----------
		else if (gpGlobals->curtime < flHomingEndHomingTime)
		{
			float flAge			= gpGlobals->curtime - flHomingRampDownStartTime;
			flCurHomingStrength = m_flHomingStrength * (1-(flAge/m_flHomingRampDown));
			flTargetSpeed		= m_flHomingSpeed;
		}
		// ---------------
		//  Set Homing
		// ---------------
		if (flCurHomingStrength > 0)
		{
			// -------------
			// Smoke trail.
			// -------------
			if (m_nRocketTrailType == HOMER_SMOKE_TRAIL_ON_HOMING)
			{
				UpdateRocketTrail(flCurHomingStrength);
			}

			// Extract speed and direction
			Vector	vCurDir		= GetAbsVelocity();	//GetAbsVelocity
			float flCurSpeed = VectorNormalize(vCurDir);
			flTargetSpeed = max(flTargetSpeed, flCurSpeed);

			// Add in homing direction
			Vector vecNewVelocity = GetAbsVelocity();
			float flTimeToUse = gpGlobals->frametime;
			while (flTimeToUse > 0)
			{
				vecNewVelocity = (flCurHomingStrength * vTargetDir) + ((1 - flCurHomingStrength) * vCurDir);
				flTimeToUse =- 0.1;
			}
			VectorNormalize(vecNewVelocity);
			vecNewVelocity *= flTargetSpeed;
			SetAbsVelocity( vecNewVelocity );
		}
	}

	// ----------------------------------------------------------------------------------------
	// Add time-coherent noise to the current velocity 
	// ----------------------------------------------------------------------------------------
	Vector vecImpulse( 0, 0, 0 );
	if (m_flSpinMagnitude > 0)
	{
		vecImpulse.x += m_flSpinMagnitude*sin(m_flSpinSpeed * gpGlobals->curtime + m_flSpinOffset);
		vecImpulse.y += m_flSpinMagnitude*cos(m_flSpinSpeed * gpGlobals->curtime + m_flSpinOffset);
		vecImpulse.z -= m_flSpinMagnitude*cos(m_flSpinSpeed * gpGlobals->curtime + m_flSpinOffset);
	}

	// Add in gravity
	vecImpulse.z -= GetGravity() * sv_gravity.GetFloat() * gpGlobals->frametime;
	ApplyAbsVelocityImpulse( vecImpulse );

	QAngle angles;
	VectorAngles( GetAbsVelocity(), angles );
	SetLocalAngles( angles );

#if BUBBLE_WARNING
	if( gpGlobals->curtime > m_flNextWarnTime )
	{
		// Make a bubble of warning sound in front of me.
		const float WARN_INTERVAL = 0.25f;
		float flSpeed = GetAbsVelocity().Length();
		Vector vecWarnLocation;

		// warn a little bit ahead of us, please.
		vecWarnLocation = GetAbsOrigin() + GetAbsVelocity() * 0.75;

		// Make a bubble of warning ahead of the missile.
		CSoundEnt::InsertSound ( SOUND_DANGER, vecWarnLocation, flSpeed * WARN_INTERVAL, 0.5 );

#if _DEBUG
		Vector vecRight, vecForward;

		AngleVectors( GetAbsAngles(), &vecForward, &vecRight, NULL );

		NDebugOverlay::Line( vecWarnLocation, vecWarnLocation + vecForward * flSpeed * WARN_INTERVAL * 0.5, 255,255,0, true, 10);
		NDebugOverlay::Line( vecWarnLocation, vecWarnLocation - vecForward * flSpeed * WARN_INTERVAL * 0.5, 255,255,0, true, 10);

		NDebugOverlay::Line( vecWarnLocation, vecWarnLocation + vecRight * flSpeed * WARN_INTERVAL * 0.5, 255,255,0, true, 10);
		NDebugOverlay::Line( vecWarnLocation, vecWarnLocation - vecRight * flSpeed * WARN_INTERVAL * 0.5, 255,255,0, true, 10);
#endif
		m_flNextWarnTime = gpGlobals->curtime + WARN_INTERVAL;
	}
#endif // BUBBLE

	SetNextThink( gpGlobals->curtime + 0.1f );
}

//------------------------------------------------------------------------------
// Purpose : 
// Input   :
// Output  :
//------------------------------------------------------------------------------
int CGrenadeHomer::OnTakeDamage( const CTakeDamageInfo &info )
{
	// Don't take damage from other homing grenades so can shoot in vollies
	if (FClassnameIs( info.GetInflictor(), "grenade_homer"))
	{
		return 0;
	}
	return BaseClass::OnTakeDamage( info );
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
CGrenadeHomer::CGrenadeHomer(void)
{
	for (int i=0;i<3;i++)
	{
		m_hRocketTrail[i]  = NULL;
	}
}
