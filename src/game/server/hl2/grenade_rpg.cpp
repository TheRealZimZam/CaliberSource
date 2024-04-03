//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//			Primary attack: fire missile
//			Secondary attack: toggle guidance
// TODO; Attach the rocketflare [CTempEnts::RocketFlare( const Vector& pos )]
// CMissile and APCMissile needs to be moved out of here... Alot of other classes use it,
// and having the weapon_rpg tacked on is useless bloat
//=============================================================================//

#include "cbase.h"
#include "grenade_rpg.h"
#include "weapon_rpg.h"
#include "basehlcombatweapon_shared.h"
#include "basecombatcharacter.h"
#include "movie_explosion.h"
#include "soundent.h"
#include "player.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "explode.h"
#include "util.h"
#include "in_buttons.h"
#include "shake.h"
#include "AI_BaseNPC.h"
#include "AI_Squad.h"
#include "te_effect_dispatch.h"
#include "triggers.h"
#include "smoke_trail.h"
#include "collisionutils.h"
#include "hl2_shareddefs.h"
#include "rumble_shared.h"
#include "gamestats.h"

#ifdef PORTAL
	#include "portal_util_shared.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	RPG_SPEED			sk_rpg_speed.GetInt()
ConVar sk_rpg_speed( "sk_rpg_speed","1500");
#define	RPG_HOMING_SPEED	sk_rpg_homing_speed.GetFloat()
ConVar sk_rpg_homing_speed( "sk_rpg_homing_speed","0.05");	//0.125
#define	RPG_LIFETIME		sk_rpg_engine_lifetime.GetFloat()
ConVar sk_rpg_engine_lifetime( "sk_rpg_engine_lifetime","10");	// Engine shuts down at this time
#define	RPG_MAX_LIFETIME	sk_rpg_max_lifetime.GetFloat()
ConVar sk_rpg_max_lifetime( "sk_rpg_max_lifetime","20");	// Explodes after this amount of time

extern ConVar    sk_plr_dmg_rpg_round;
extern ConVar    sk_npc_dmg_rpg_round;
static ConVar sk_apc_missile_damage("sk_apc_missile_damage", "15");
ConVar rpg_missle_use_custom_detonators( "rpg_missle_use_custom_detonators", "1" );

#define APC_MISSILE_DAMAGE	sk_apc_missile_damage.GetFloat()

//-----------------------------------------------------------------------------
// CMissile
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CMissile )
	DEFINE_FIELD( m_hOwner,					FIELD_EHANDLE ),
	DEFINE_FIELD( m_hRocketTrail,			FIELD_EHANDLE ),
	DEFINE_FIELD( m_hRocketFlare,			FIELD_EHANDLE ),
	DEFINE_FIELD( m_flMarkDeadTime,			FIELD_TIME ),
	DEFINE_FIELD( m_flGracePeriodEndsAt,	FIELD_TIME ),
	DEFINE_FIELD( m_flEngineLifetime,		FIELD_TIME ),
	DEFINE_FIELD( m_flMaxLifetime,			FIELD_TIME ),
	DEFINE_FIELD( m_flDamage,				FIELD_FLOAT ),
	DEFINE_FIELD( m_bCreateDangerSounds,	FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bGuidingDisabled,		FIELD_BOOLEAN ),
	
	// Function Pointers
	DEFINE_FUNCTION( MissileTouch ),
	DEFINE_FUNCTION( AccelerateThink ),
	DEFINE_FUNCTION( AugerThink ),
	DEFINE_FUNCTION( IgniteThink ),
	DEFINE_FUNCTION( SeekThink ),

END_DATADESC()
LINK_ENTITY_TO_CLASS( rpg_missile, CMissile );
LINK_ENTITY_TO_CLASS( missile, CMissile );

#define	RPG_FLARE_SPRITE	"sprites/animglow01.vmt"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMissile::CMissile()
{
	m_hRocketTrail = NULL;
	m_hRocketFlare = NULL;
	m_bCreateDangerSounds = false;	//In a perfect world, this would be true
}

CMissile::~CMissile()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CMissile::Precache( void )
{
	PrecacheModel( "models/weapons/w_missile.mdl" );
	PrecacheModel( "models/weapons/w_missile_launch.mdl" );
	PrecacheModel( "models/weapons/w_missile_closed.mdl" );
	PrecacheModel( RPG_FLARE_SPRITE );
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CMissile::Spawn( void )
{
	Precache();

	SetSolid( SOLID_BBOX );
	SetModel("models/weapons/w_missile_launch.mdl");
	UTIL_SetSize( this, -Vector(4,4,4), Vector(4,4,4) );

	SetTouch( &CMissile::MissileTouch );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetThink( &CMissile::IgniteThink );
	SetNextThink( gpGlobals->curtime + 0.3f );

	if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() )
		SetDamage( sk_plr_dmg_rpg_round.GetFloat() );
	else
		SetDamage( sk_npc_dmg_rpg_round.GetFloat() );

	SetGravity( UTIL_ScaleForGravity( 700 ) );	// missiles without thrust need to plumet hard

	m_takedamage = DAMAGE_YES;
	m_iHealth = m_iMaxHealth = 100;
	m_bloodColor = DONT_BLEED;
	m_flGracePeriodEndsAt = 0;
	m_flEngineLifetime = gpGlobals->curtime + RPG_LIFETIME;
	m_flMaxLifetime = gpGlobals->curtime + RPG_MAX_LIFETIME;

	AddFlag( FL_OBJECT );
}


//---------------------------------------------------------
//---------------------------------------------------------
void CMissile::Event_Killed( const CTakeDamageInfo &info )
{
	m_takedamage = DAMAGE_NO;

	ShotDown();
}

unsigned int CMissile::PhysicsSolidMaskForEntity( void ) const
{ 
	return BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX;
}

//---------------------------------------------------------
//---------------------------------------------------------
int CMissile::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if ( ( info.GetDamageType() & (DMG_MISSILEDEFENSE | DMG_AIRBOAT) ) == false )
		return 0;

	bool bIsDamaged;
	if( m_iHealth <= AugerHealth() )
	{
		// This missile is already damaged (i.e., already running AugerThink)
		bIsDamaged = true;
	}
	else
	{
		// This missile isn't damaged enough to wobble in flight yet
		bIsDamaged = false;
	}
	
	int nRetVal = BaseClass::OnTakeDamage_Alive( info );

	if( !bIsDamaged )
	{
		if ( m_iHealth <= AugerHealth() )
		{
			ShotDown();
		}
	}

	return nRetVal;
}


//-----------------------------------------------------------------------------
// Purpose: Stops any kind of tracking and shoots dumb
//-----------------------------------------------------------------------------
void CMissile::KillEngine( void )
{
	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetThink( &CMissile::AugerThink );
	SetNextThink( gpGlobals->curtime );

	// Let the RPG start reloading immediately
	if ( m_hOwner != NULL )
	{
		m_hOwner->NotifyRocketDied();
		m_hOwner = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMissile::SetGracePeriod( float flGracePeriod )
{
	m_flGracePeriodEndsAt = gpGlobals->curtime + flGracePeriod;

	// Go non-solid until the grace period ends
	AddSolidFlags( FSOLID_NOT_SOLID );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CMissile::AccelerateThink( void )
{
	Vector vecForward;

	// !!!UNDONE - make this work exactly the same as HL1 RPG, lest we have looping sound bugs again!
	StopSound( "Missile.Ignite" );
	EmitSound( "Missile.Accelerate" );

	if ( m_hRocketFlare != NULL )
		AddEffects( EF_BRIGHTLIGHT );
	else
		AddEffects( EF_DIMLIGHT );

	AngleVectors( GetLocalAngles(), &vecForward );
	SetAbsVelocity( vecForward * RPG_SPEED );

	SetThink( &CMissile::SeekThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

#define AUGER_YDEVIANCE 20.0f
#define AUGER_XDEVIANCEUP 8.0f
#define AUGER_XDEVIANCEDOWN 1.0f

//---------------------------------------------------------
//---------------------------------------------------------
void CMissile::AugerThink( void )
{
	StopSound( "Missile.Accelerate" );

	// If we've augered long enough, then just explode
	if ( m_flMaxLifetime < gpGlobals->curtime )
	{
		Explode();
		return;
	}

	if ( m_flMarkDeadTime < gpGlobals->curtime )
	{
		m_lifeState = LIFE_DYING;
	}

	QAngle angles = GetLocalAngles();

	angles.y += random->RandomFloat( -AUGER_YDEVIANCE, AUGER_YDEVIANCE );
	angles.x += random->RandomFloat( -AUGER_XDEVIANCEDOWN, AUGER_XDEVIANCEUP );

	SetLocalAngles( angles );

//	Vector vecForward;
//	AngleVectors( GetLocalAngles(), &vecForward );

	// Start slowing down, but not too much
	if ( GetAbsVelocity().Length() > 100 )
		SetAbsVelocity( GetAbsVelocity() * 0.95f );

	SetNextThink( gpGlobals->curtime + 0.05f );
}

//-----------------------------------------------------------------------------
// Purpose: Causes the missile to spiral to the ground and explode, due to damage
//-----------------------------------------------------------------------------
void CMissile::ShotDown( void )
{
	CEffectData	data;
	data.m_vOrigin = GetAbsOrigin();

	DispatchEffect( "RPGShotDown", data );

	if ( m_hRocketTrail != NULL )
	{
		m_hRocketTrail->m_bDamaged = true;
	}

	SetThink( &CMissile::AugerThink );
	SetNextThink( gpGlobals->curtime );
	m_flMaxLifetime = gpGlobals->curtime + 1.5f;
	m_flMarkDeadTime = gpGlobals->curtime + 0.75;

	// Let the RPG start reloading immediately
	if ( m_hOwner != NULL )
	{
		m_hOwner->NotifyRocketDied();
		m_hOwner = NULL;
	}
}


//-----------------------------------------------------------------------------
// The actual explosion 
//-----------------------------------------------------------------------------
void CMissile::DoExplosion( void )
{
	// Explode
	ExplosionCreate( GetAbsOrigin(), GetAbsAngles(), GetOwnerEntity(), GetDamage(), CMissile::EXPLOSION_RADIUS, 
		SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NOSMOKE, 0.0f, this);

	// Create a ripple of going below water
	if ( GetWaterLevel() != 0 )
	{
		CEffectData data;
		data.m_vOrigin = WorldSpaceCenter();
		data.m_flMagnitude = 128;
		data.m_flScale = 128;
		data.m_fFlags = 0;
		DispatchEffect( "WaterSurfaceExplosion", data );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMissile::Explode( void )
{
	// Don't explode against the skybox. Just pretend that 
	// the missile flies off into the distance.
	Vector forward;

	GetVectors( &forward, NULL, NULL );

	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + forward * 16, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	m_takedamage = DAMAGE_NO;
	SetSolid( SOLID_NONE );
	if( tr.fraction == 1.0 || !(tr.surface.flags & SURF_SKY) )
	{
		DoExplosion();
	}

	if( m_hRocketTrail )
	{
		m_hRocketTrail->SetLifetime(0.1f);
		m_hRocketTrail = NULL;
	}

	if( m_hRocketFlare )
	{
		UTIL_Remove( m_hRocketFlare );
		m_hRocketFlare = NULL;
	}

	if ( m_hOwner != NULL )
	{
		m_hOwner->NotifyRocketDied();
		m_hOwner = NULL;
	}

	StopSound( "Missile.Ignite" );
	StopSound( "Missile.Accelerate" );
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CMissile::MissileTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	
	// Don't touch triggers (but DO hit weapons)
	if ( pOther->IsSolidFlagSet(FSOLID_TRIGGER|FSOLID_VOLUME_CONTENTS) && pOther->GetCollisionGroup() != COLLISION_GROUP_WEAPON )
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ( ( pOther->m_takedamage == DAMAGE_NO ) || ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY ) )
			return;
	}

	//TODO: Play a special effect on the target to denote that it pierced and did damage
	Explode();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMissile::CreateSmokeTrail( void )
{
	if ( m_hRocketTrail )
		return;

	// Smoke trail.
	if ( (m_hRocketTrail = RocketTrail::CreateRocketTrail()) != NULL )
	{
		m_hRocketTrail->m_Opacity = 0.2f;
		m_hRocketTrail->m_SpawnRate = 50;
		m_hRocketTrail->m_ParticleLifetime = 0.5f;
		m_hRocketTrail->m_StartColor.Init( 0.65f, 0.65f , 0.65f );
		m_hRocketTrail->m_EndColor.Init( 0.0, 0.0, 0.0 );
		m_hRocketTrail->m_StartSize = 8;
		m_hRocketTrail->m_EndSize = 32;
		m_hRocketTrail->m_SpawnRadius = 4;
		m_hRocketTrail->m_MinSpeed = 2;
		m_hRocketTrail->m_MaxSpeed = 16;
		
		m_hRocketTrail->SetLifetime( 999 );
		m_hRocketTrail->FollowEntity( this, "0" );
	}
}

void CMissile::CreateFlare( void )
{
	if ( m_hRocketFlare )
		return;

	// Light isn't on.
	m_hRocketFlare = CSprite::SpriteCreate( RPG_FLARE_SPRITE, GetLocalOrigin(), true );
	CSprite *pSprite = (CSprite *)m_hRocketFlare.Get();

	if( m_hRocketFlare != NULL )
	{
		pSprite->SetParent( this );
//!		pSprite->SetAttachment( this, 0 );	//TODO; Attach at the nose
		pSprite->SetTransparency( kRenderGlow, m_clrRender->r, m_clrRender->g, m_clrRender->b, 200, kRenderFxNoDissipation );
		pSprite->SetScale( 0.75 );
		pSprite->m_flSpriteFramerate = random->RandomInt(15,20);
		pSprite->m_flFrame = random->RandomInt(0,1);

		pSprite->FollowEntity( this );
		pSprite->TurnOn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMissile::IgniteThink( void )
{
	SetMoveType( MOVETYPE_FLY );
	SetModel("models/weapons/w_missile.mdl");
	UTIL_SetSize( this, vec3_origin, vec3_origin );
 	RemoveSolidFlags( FSOLID_NOT_SOLID );

	Vector vecForward;

	EmitSound( "Missile.Ignite" );

	AngleVectors( GetLocalAngles(), &vecForward );
	SetAbsVelocity( vecForward * RPG_SPEED/4 );

	SetThink( &CMissile::AccelerateThink );
	SetNextThink( gpGlobals->curtime + 0.2f );

	//TODO; Only behind the missle
	if ( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwnerEntity() );

		if ( pPlayer )
		{
			color32 white = { 255,225,205,64 };
			UTIL_ScreenFade( pPlayer, white, 0.1f, 0.0f, FFADE_IN );

			pPlayer->RumbleEffect( RUMBLE_RPG_MISSILE, 0, RUMBLE_FLAG_RESTART );
		}
	}

	CreateSmokeTrail();
	if ( HasSpawnFlags( SF_ROCKET_FLARE ) )
		CreateFlare();
}


//-----------------------------------------------------------------------------
// Gets the shooting position 
//-----------------------------------------------------------------------------
void CMissile::GetShootPosition( CLaserDot *pLaserDot, Vector *pShootPosition )
{
	if ( pLaserDot->GetOwnerEntity() != NULL )
	{
		//FIXME: Do we care this isn't exactly the muzzle position?
		*pShootPosition = pLaserDot->GetOwnerEntity()->WorldSpaceCenter();
	}
	else
	{
		*pShootPosition = pLaserDot->GetChasePosition();
	}
}

	
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMissile::ComputeActualDotPosition( CLaserDot *pLaserDot, Vector *pActualDotPosition, float *pHomingSpeed )
{
	*pHomingSpeed = RPG_HOMING_SPEED;
	if ( pLaserDot->GetTargetEntity() )
	{
		*pActualDotPosition = pLaserDot->GetChasePosition();
		return;
	}

	Vector vLaserStart;
	GetShootPosition( pLaserDot, &vLaserStart );

	//Get the laser's vector
	Vector vLaserDir;
	VectorSubtract( pLaserDot->GetChasePosition(), vLaserStart, vLaserDir );
	
	//Find the length of the current laser
	float flLaserLength = VectorNormalize( vLaserDir );
	
	//Find the length from the missile to the laser's owner
	float flMissileLength = GetAbsOrigin().DistTo( vLaserStart );

	//Find the length from the missile to the laser's position
	Vector vecTargetToMissile;
	VectorSubtract( GetAbsOrigin(), pLaserDot->GetChasePosition(), vecTargetToMissile ); 
	float flTargetLength = VectorNormalize( vecTargetToMissile );

	// See if we should chase the line segment nearest us
	if ( ( flMissileLength < flLaserLength ) || ( flTargetLength <= 512.0f ) )
	{
		*pActualDotPosition = UTIL_PointOnLineNearestPoint( vLaserStart, pLaserDot->GetChasePosition(), GetAbsOrigin() );
		*pActualDotPosition += ( vLaserDir * 256.0f );
	}
	else
	{
		// Otherwise chase the dot
		*pActualDotPosition = pLaserDot->GetChasePosition();
	}

//	NDebugOverlay::Line( pLaserDot->GetChasePosition(), vLaserStart, 0, 255, 0, true, 0.05f );
//	NDebugOverlay::Line( GetAbsOrigin(), *pActualDotPosition, 255, 0, 0, true, 0.05f );
//	NDebugOverlay::Cross3D( *pActualDotPosition, -Vector(4,4,4), Vector(4,4,4), 255, 0, 0, true, 0.05f );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMissile::SeekThink( void )
{
	// If we have a grace period, go solid when it ends
	if ( m_flGracePeriodEndsAt )
	{
		if ( m_flGracePeriodEndsAt < gpGlobals->curtime )
		{
			RemoveSolidFlags( FSOLID_NOT_SOLID );
			m_flGracePeriodEndsAt = 0;
		}
	}

	if ( rpg_missle_use_custom_detonators.GetBool() )
	{
		for ( int i = gm_CustomDetonators.Count() - 1; i >=0; --i )
		{
			CustomDetonator_t &detonator = gm_CustomDetonators[i];
			if ( !detonator.hEntity )
			{
				gm_CustomDetonators.FastRemove( i );
			}
			else
			{
				const Vector &vPos = detonator.hEntity->CollisionProp()->WorldSpaceCenter();
				if ( detonator.halfHeight > 0 )
				{
					if ( fabsf( vPos.z - GetAbsOrigin().z ) < detonator.halfHeight )
					{
						if ( ( GetAbsOrigin().AsVector2D() - vPos.AsVector2D() ).LengthSqr() < detonator.radiusSq )
						{
							Explode();
							return;
						}
					}
				}
				else
				{
					if ( ( GetAbsOrigin() - vPos ).LengthSqr() < detonator.radiusSq )
					{
						Explode();
						return;
					}
				}
			}
		}
	}

	if( GetAbsVelocity() == vec3_origin )
	{
		// Strange circumstances have brought this missile to halt. Just blow it up.
		Explode();
		return;
	}

	if ( m_bGuidingDisabled == false )
	{
		CBaseEntity	*pBestDot	= NULL;
		float		flBestDist	= MAX_TRACE_LENGTH;
		float		dotDist;

		//Search for all dots relevant to us
		for( CLaserDot *pEnt = GetLaserDotList(); pEnt != NULL; pEnt = pEnt->m_pNext )
		{
			if ( !pEnt->IsOn() )
				continue;

			if ( pEnt->GetOwnerEntity() != GetOwnerEntity() )
				continue;

			dotDist = (GetAbsOrigin() - pEnt->GetAbsOrigin()).Length();

			//Find closest
			if ( dotDist < flBestDist )
			{
				pBestDot	= pEnt;
				flBestDist	= dotDist;
			}
		}

		//If we have a dot target
		if ( pBestDot == NULL )
		{
			//Think as soon as possible
			SetNextThink( gpGlobals->curtime );
			return;
		}

		CLaserDot *pLaserDot = (CLaserDot *)pBestDot;
		Vector	targetPos;

		float flHomingSpeed; 
		Vector vecLaserDotPosition;
		ComputeActualDotPosition( pLaserDot, &targetPos, &flHomingSpeed );

		if ( IsSimulatingOnAlternateTicks() )
			flHomingSpeed *= 2;

		Vector	vTargetDir;
		VectorSubtract( targetPos, GetAbsOrigin(), vTargetDir );
		float flDist = VectorNormalize( vTargetDir );

		if( pLaserDot->GetTargetEntity() != NULL && flDist <= 240.0f )
		{
			// Prevent the missile circling the Strider like a Halo in ep1_c17_06. If the missile gets within 20
			// feet of a Strider, tighten up the turn speed of the missile so it can break the halo and strike. (sjb 4/27/2006)
			if( pLaserDot->GetTargetEntity()->ClassMatches( "npc_strider" ) )
			{
				flHomingSpeed *= 1.5f;
			}
		}

		Vector	vDir	= GetAbsVelocity();
		float	flSpeed	= VectorNormalize( vDir );
		Vector	vNewVelocity = vDir;
		if ( gpGlobals->frametime > 0.0f )
		{
			if ( flSpeed != 0 )
			{
				vNewVelocity = ( flHomingSpeed * vTargetDir ) + ( ( 1 - flHomingSpeed ) * vDir );

				// This computation may happen to cancel itself out exactly. If so, slam to targetdir.
				if ( VectorNormalize( vNewVelocity ) < 1e-3 )
				{
					vNewVelocity = (flDist != 0) ? vTargetDir : vDir;
				}
			}
			else
			{
				vNewVelocity = vTargetDir;
			}
		}

		QAngle	finalAngles;
		VectorAngles( vNewVelocity, finalAngles );
		SetAbsAngles( finalAngles );

		vNewVelocity *= flSpeed;

		if ( gpGlobals->curtime < m_flEngineLifetime )
			SetAbsVelocity( vNewVelocity );
		else
			KillEngine();
	}
	else
	{
		if ( gpGlobals->curtime < m_flEngineLifetime )
		{
			Vector vDir = GetAbsVelocity();
			VectorNormalize( vDir );
			SetAbsVelocity( vDir * RPG_SPEED );
		}
		else
			KillEngine();
	}

	if ( m_bCreateDangerSounds == true )
	{
		trace_t tr;
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + GetAbsVelocity() * 0.5, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		CSoundEnt::InsertSound( SOUND_DANGER, tr.endpos, 100, 0.2, this, SOUNDENT_CHANNEL_REPEATED_DANGER );
	}

	// Think as soon as possible
	SetNextThink( gpGlobals->curtime );
}


//-----------------------------------------------------------------------------
// Purpose: 
//
// Input  : &vecOrigin - 
//			&vecAngles - 
//			NULL - 
//
// Output : CMissile
//-----------------------------------------------------------------------------
CMissile *CMissile::Create( const Vector &vecOrigin, const QAngle &vecAngles, edict_t *pentOwner = NULL )
{
	//CMissile *pMissile = (CMissile *)CreateEntityByName("rpg_missile" );
	CMissile *pMissile = (CMissile *) CBaseEntity::Create( "rpg_missile", vecOrigin, vecAngles, CBaseEntity::Instance( pentOwner ) );
	pMissile->SetOwnerEntity( Instance( pentOwner ) );
	pMissile->Spawn();
	pMissile->AddEffects( EF_NOSHADOW );
	
	Vector vecForward;
	AngleVectors( vecAngles, &vecForward );

	pMissile->SetAbsVelocity( vecForward * 300 + Vector( 0,0, 128 ) );

	return pMissile;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CUtlVector<CMissile::CustomDetonator_t> CMissile::gm_CustomDetonators;

void CMissile::AddCustomDetonator( CBaseEntity *pEntity, float radius, float height )
{
	int i = gm_CustomDetonators.AddToTail();
	gm_CustomDetonators[i].hEntity = pEntity;
	gm_CustomDetonators[i].radiusSq = Square( radius );
	gm_CustomDetonators[i].halfHeight = height * 0.5f;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMissile::RemoveCustomDetonator( CBaseEntity *pEntity )
{
	for ( int i = 0; i < gm_CustomDetonators.Count(); i++ )
	{
		if ( gm_CustomDetonators[i].hEntity == pEntity )
		{
			gm_CustomDetonators.FastRemove( i );
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// This entity is used to create little force boxes that the helicopter
// should avoid. 
//-----------------------------------------------------------------------------
class CInfoAPCMissileHint : public CBaseEntity
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CInfoAPCMissileHint, CBaseEntity );

	virtual void Spawn( );
	virtual void Activate();
	virtual void UpdateOnRemove();

	static CBaseEntity *FindAimTarget( CBaseEntity *pMissile, const char *pTargetName, 
		const Vector &vecCurrentTargetPos, const Vector &vecCurrentTargetVel );

private:
	EHANDLE	m_hTarget;

	typedef CHandle<CInfoAPCMissileHint> APCMissileHintHandle_t;	
	static CUtlVector< APCMissileHintHandle_t > s_APCMissileHints; 
};


//-----------------------------------------------------------------------------
//
// This entity is used to create little force boxes that the helicopters should avoid. 
//
//-----------------------------------------------------------------------------
CUtlVector< CInfoAPCMissileHint::APCMissileHintHandle_t > CInfoAPCMissileHint::s_APCMissileHints; 

LINK_ENTITY_TO_CLASS( info_apc_missile_hint, CInfoAPCMissileHint );

BEGIN_DATADESC( CInfoAPCMissileHint )
	DEFINE_FIELD( m_hTarget, FIELD_EHANDLE ),
END_DATADESC()


//-----------------------------------------------------------------------------
// Spawn, remove
//-----------------------------------------------------------------------------
void CInfoAPCMissileHint::Spawn( )
{
	SetModel( STRING( GetModelName() ) );
	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW );
}

void CInfoAPCMissileHint::Activate( )
{
	BaseClass::Activate();

	m_hTarget = gEntList.FindEntityByName( NULL, m_target );
	if ( m_hTarget == NULL )
	{
		DevWarning( "%s: Could not find target '%s'!\n", GetClassname(), STRING( m_target ) );
	}
	else
	{
		s_APCMissileHints.AddToTail( this );
	}
}

void CInfoAPCMissileHint::UpdateOnRemove( )
{
	s_APCMissileHints.FindAndRemove( this );
	BaseClass::UpdateOnRemove();
}


//-----------------------------------------------------------------------------
// Where are how should we avoid?
//-----------------------------------------------------------------------------
#define HINT_PREDICTION_TIME 3.0f

CBaseEntity *CInfoAPCMissileHint::FindAimTarget( CBaseEntity *pMissile, const char *pTargetName, 
	const Vector &vecCurrentEnemyPos, const Vector &vecCurrentEnemyVel )
{
	if ( !pTargetName )
		return NULL;

	float flOOSpeed = pMissile->GetAbsVelocity().Length();
	if ( flOOSpeed != 0.0f )
	{
		flOOSpeed = 1.0f / flOOSpeed;
	}

	for ( int i = s_APCMissileHints.Count(); --i >= 0; )
	{
		CInfoAPCMissileHint *pHint = s_APCMissileHints[i];
		if ( !pHint->NameMatches( pTargetName ) )
			continue;

		if ( !pHint->m_hTarget )
			continue;

		Vector vecMissileToHint, vecMissileToEnemy;
		VectorSubtract( pHint->m_hTarget->WorldSpaceCenter(), pMissile->GetAbsOrigin(), vecMissileToHint );
		VectorSubtract( vecCurrentEnemyPos, pMissile->GetAbsOrigin(), vecMissileToEnemy );
		float flDistMissileToHint = VectorNormalize( vecMissileToHint );
		VectorNormalize( vecMissileToEnemy );
		if ( DotProduct( vecMissileToHint, vecMissileToEnemy ) < 0.866f )
			continue;

		// Determine when the target will be inside the volume.
		// Project at most 3 seconds in advance
		Vector vecRayDelta;
		VectorMultiply( vecCurrentEnemyVel, HINT_PREDICTION_TIME, vecRayDelta );

		BoxTraceInfo_t trace;
		if ( !IntersectRayWithOBB( vecCurrentEnemyPos, vecRayDelta, pHint->CollisionProp()->CollisionToWorldTransform(),
			pHint->CollisionProp()->OBBMins(), pHint->CollisionProp()->OBBMaxs(), 0.0f, &trace ))
		{
			continue;
		}

		// Determine the amount of time it would take the missile to reach the target
		// If we can reach the target within the time it takes for the enemy to reach the 
		float tSqr = flDistMissileToHint * flOOSpeed / HINT_PREDICTION_TIME;
		if ( (tSqr < (trace.t1 * trace.t1)) || (tSqr > (trace.t2 * trace.t2)) )
			continue;

		return pHint->m_hTarget;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// a list of missiles to search quickly
//-----------------------------------------------------------------------------
CEntityClassList<CAPCMissile> g_APCMissileList;
CAPCMissile *CEntityClassList<CAPCMissile>::m_pClassList = NULL;
CAPCMissile *GetAPCMissileList()
{
	return g_APCMissileList.m_pClassList;
}

//-----------------------------------------------------------------------------
// Finds apc missiles in cone
//-----------------------------------------------------------------------------
CAPCMissile *FindAPCMissileInCone( const Vector &vecOrigin, const Vector &vecDirection, float flAngle )
{
	float flCosAngle = cos( DEG2RAD( flAngle ) );
	for( CAPCMissile *pEnt = GetAPCMissileList(); pEnt != NULL; pEnt = pEnt->m_pNext )
	{
		if ( !pEnt->IsSolid() )
			continue;

		Vector vecDelta;
		VectorSubtract( pEnt->GetAbsOrigin(), vecOrigin, vecDelta );
		VectorNormalize( vecDelta );
		float flDot = DotProduct( vecDelta, vecDirection );
		if ( flDot > flCosAngle )
			return pEnt;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//
// Specialized version of the missile
//
//-----------------------------------------------------------------------------
#define MAX_HOMING_DISTANCE 2250.0f
#define MIN_HOMING_DISTANCE 1250.0f
#define MAX_NEAR_HOMING_DISTANCE 1750.0f
#define MIN_NEAR_HOMING_DISTANCE 1000.0f
#define DOWNWARD_BLEND_TIME_START 0.2f
#define MIN_HEIGHT_DIFFERENCE	250.0f
#define MAX_HEIGHT_DIFFERENCE	550.0f
#define CORRECTION_TIME		0.2f
#define	APC_LAUNCH_HOMING_SPEED	0.1f
#define	APC_HOMING_SPEED	0.025f
#define HOMING_SPEED_ACCEL	0.01f

BEGIN_DATADESC( CAPCMissile )

	DEFINE_FIELD( m_flReachedTargetTime,	FIELD_TIME ),
	DEFINE_FIELD( m_flIgnitionTime,			FIELD_TIME ),
	DEFINE_FIELD( m_bGuidingDisabled,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hSpecificTarget,		FIELD_EHANDLE ),
	DEFINE_FIELD( m_strHint,				FIELD_STRING ),
	DEFINE_FIELD( m_flLastHomingSpeed,		FIELD_FLOAT ),
//	DEFINE_FIELD( m_pNext,					FIELD_CLASSPTR ),

	DEFINE_THINKFUNC( BeginSeekThink ),
	DEFINE_THINKFUNC( AugerStartThink ),
	DEFINE_THINKFUNC( ExplodeThink ),
	DEFINE_THINKFUNC( APCSeekThink ),

	DEFINE_FUNCTION( APCMissileTouch ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( apc_missile, CAPCMissile );

CAPCMissile *CAPCMissile::Create( const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, CBaseEntity *pOwner )
{
	CAPCMissile *pMissile = (CAPCMissile *)CBaseEntity::Create( "apc_missile", vecOrigin, vecAngles, pOwner );
	pMissile->SetOwnerEntity( pOwner );
	pMissile->AddSpawnFlags( SF_ROCKET_FLARE );
	pMissile->Spawn();
	pMissile->SetAbsVelocity( vecVelocity );
	pMissile->AddFlag( FL_NOTARGET );
	pMissile->SetRenderColor( 200, 150, 0 );
	pMissile->AddEffects( EF_NOSHADOW );
	return pMissile;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CAPCMissile::CAPCMissile()
{
	g_APCMissileList.Insert( this );
}

CAPCMissile::~CAPCMissile()
{
	g_APCMissileList.Remove( this );
}


//-----------------------------------------------------------------------------
// Shared initialization code
//-----------------------------------------------------------------------------
void CAPCMissile::Init()
{
	SetMoveType( MOVETYPE_FLY );
	SetModel("models/weapons/w_missile.mdl");
	UTIL_SetSize( this, vec3_origin, vec3_origin );
	CreateSmokeTrail();
	CreateFlare();
	SetTouch( &CAPCMissile::APCMissileTouch );
	m_flLastHomingSpeed = APC_HOMING_SPEED;
	CreateDangerSounds( true );

	if( g_pGameRules->GetAutoAimMode() == AUTOAIM_ON_CONSOLE )
	{
		AddFlag( FL_AIMTARGET );
	}
}


//-----------------------------------------------------------------------------
// For hitting a specific target
//-----------------------------------------------------------------------------
void CAPCMissile::AimAtSpecificTarget( CBaseEntity *pTarget )
{
	m_hSpecificTarget = pTarget;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CAPCMissile::APCMissileTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() && !pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;

	Explode();
}


//-----------------------------------------------------------------------------
// Specialized version of the missile
//-----------------------------------------------------------------------------
void CAPCMissile::IgniteDelay( void )
{
	m_flIgnitionTime = gpGlobals->curtime + 0.3f;

	SetThink( &CAPCMissile::BeginSeekThink );
	SetNextThink( m_flIgnitionTime );
	Init();
	AddSolidFlags( FSOLID_NOT_SOLID );
}

void CAPCMissile::AugerDelay( float flDelay )
{
	m_flIgnitionTime = gpGlobals->curtime;
	SetThink( &CAPCMissile::AugerStartThink );
	SetNextThink( gpGlobals->curtime + flDelay );
	Init();
	DisableGuiding();
}

void CAPCMissile::AugerStartThink()
{
	if ( m_hRocketTrail != NULL )
	{
		m_hRocketTrail->m_bDamaged = true;
	}
	SetMaxLifetime( gpGlobals->curtime + random->RandomFloat( 1.0f, 2.0f ) );
	SetThink( &CAPCMissile::AugerThink );
	SetNextThink( gpGlobals->curtime );
}

void CAPCMissile::ExplodeDelay( float flDelay )
{
	m_flIgnitionTime = gpGlobals->curtime;
	SetThink( &CAPCMissile::ExplodeThink );
	SetNextThink( gpGlobals->curtime + flDelay );
	Init();
	DisableGuiding();
}


void CAPCMissile::BeginSeekThink( void )
{
 	RemoveSolidFlags( FSOLID_NOT_SOLID );
	SetThink( &CAPCMissile::APCSeekThink );
	SetNextThink( gpGlobals->curtime );
}

void CAPCMissile::APCSeekThink( void )
{
	BaseClass::SeekThink();

	bool bFoundDot = false;

	//If we can't find a dot to follow around then just send me wherever I'm facing so I can blow up in peace.
	for( CLaserDot *pEnt = GetLaserDotList(); pEnt != NULL; pEnt = pEnt->m_pNext )
	{
		if ( !pEnt->IsOn() )
			continue;

		if ( pEnt->GetOwnerEntity() != GetOwnerEntity() )
			continue;

		bFoundDot = true;
	}

	if ( bFoundDot == false )
	{
		Vector	vDir	= GetAbsVelocity();
		VectorNormalize ( vDir );

		SetAbsVelocity( vDir * 800 );

		SetThink( NULL );
	}
}

void CAPCMissile::ExplodeThink()
{
	DoExplosion();
}

//-----------------------------------------------------------------------------
// Health lost at which augering starts
//-----------------------------------------------------------------------------
int CAPCMissile::AugerHealth()
{
	return m_iMaxHealth - 25;
}

	
//-----------------------------------------------------------------------------
// Health lost at which augering starts
//-----------------------------------------------------------------------------
void CAPCMissile::DisableGuiding()
{
	GuidingDisabled( true );
}

	
//-----------------------------------------------------------------------------
// Guidance hints
//-----------------------------------------------------------------------------
void CAPCMissile::SetGuidanceHint( const char *pHintName )
{
	m_strHint = MAKE_STRING( pHintName );
}


//-----------------------------------------------------------------------------
// The actual explosion 
//-----------------------------------------------------------------------------
void CAPCMissile::DoExplosion( void )
{
	if ( GetWaterLevel() != 0 )
	{
		CEffectData data;
		data.m_vOrigin = WorldSpaceCenter();
		data.m_flMagnitude = 128;
		data.m_flScale = 128;
		data.m_fFlags = 0;
		DispatchEffect( "WaterSurfaceExplosion", data );
	}
	else
	{
#ifdef HL2_EPISODIC
		ExplosionCreate( GetAbsOrigin(), GetAbsAngles(), this, APC_MISSILE_DAMAGE, 100, true, 20000 );
#else
		ExplosionCreate( GetAbsOrigin(), GetAbsAngles(), GetOwnerEntity(), APC_MISSILE_DAMAGE, 100, true, 20000 );
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAPCMissile::ComputeLeadingPosition( const Vector &vecShootPosition, CBaseEntity *pTarget, Vector *pLeadPosition )
{
	Vector vecTarget = pTarget->BodyTarget( vecShootPosition, false );
	float flShotSpeed = GetAbsVelocity().Length();
	if ( flShotSpeed == 0 )
	{
		*pLeadPosition = vecTarget;
		return;
	}

	Vector vecVelocity = pTarget->GetSmoothedVelocity();
	vecVelocity.z = 0.0f;
	float flTargetSpeed = VectorNormalize( vecVelocity );
	Vector vecDelta;
	VectorSubtract( vecShootPosition, vecTarget, vecDelta );
	float flTargetToShooter = VectorNormalize( vecDelta );
	float flCosTheta = DotProduct( vecDelta, vecVelocity );

	// Law of cosines... z^2 = x^2 + y^2 - 2xy cos Theta
	// where z = flShooterToPredictedTargetPosition = flShotSpeed * predicted time
	// x = flTargetSpeed * predicted time
	// y = flTargetToShooter
	// solve for predicted time using at^2 + bt + c = 0, t = (-b +/- sqrt( b^2 - 4ac )) / 2a
	float a = flTargetSpeed * flTargetSpeed - flShotSpeed * flShotSpeed;
	float b = -2.0f * flTargetToShooter * flCosTheta * flTargetSpeed;
	float c = flTargetToShooter * flTargetToShooter;
	
	float flDiscrim = b*b - 4*a*c;
	if (flDiscrim < 0)
	{
		*pLeadPosition = vecTarget;
		return;
	}

	flDiscrim = sqrt(flDiscrim);
	float t = (-b + flDiscrim) / (2.0f * a);
	float t2 = (-b - flDiscrim) / (2.0f * a);
	if ( t < t2 )
	{
		t = t2;
	}

	if ( t <= 0.0f )
	{
		*pLeadPosition = vecTarget;
		return;
	}

	VectorMA( vecTarget, flTargetSpeed * t, vecVelocity, *pLeadPosition );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAPCMissile::ComputeActualDotPosition( CLaserDot *pLaserDot, Vector *pActualDotPosition, float *pHomingSpeed )
{
/*
	if ( m_bGuidingDisabled )
	{
		*pActualDotPosition = GetAbsOrigin();
		*pHomingSpeed = 0.0f;
		m_flLastHomingSpeed = *pHomingSpeed;
		return;
	}
*/

	if ( ( m_strHint != NULL_STRING ) && (!m_hSpecificTarget) )
	{
		Vector vecOrigin, vecVelocity;
		CBaseEntity *pTarget = pLaserDot->GetTargetEntity();
		if ( pTarget )
		{
			vecOrigin = pTarget->BodyTarget( GetAbsOrigin(), false );
			vecVelocity	= pTarget->GetSmoothedVelocity();
		}
		else
		{
			vecOrigin = pLaserDot->GetChasePosition();
			vecVelocity = vec3_origin;
		}

		m_hSpecificTarget = CInfoAPCMissileHint::FindAimTarget( this, STRING( m_strHint ), vecOrigin, vecVelocity );
	}

	CBaseEntity *pLaserTarget = m_hSpecificTarget ? m_hSpecificTarget : pLaserDot->GetTargetEntity();
	if ( !pLaserTarget )
	{
		BaseClass::ComputeActualDotPosition( pLaserDot, pActualDotPosition, pHomingSpeed );
		m_flLastHomingSpeed = *pHomingSpeed;
		return;
	}
	
	if ( pLaserTarget->ClassMatches( "npc_bullseye" ) )
	{
		if ( m_flLastHomingSpeed != RPG_HOMING_SPEED )
		{
			if (m_flLastHomingSpeed > RPG_HOMING_SPEED)
			{
				m_flLastHomingSpeed -= HOMING_SPEED_ACCEL * UTIL_GetSimulationInterval();
				if ( m_flLastHomingSpeed < RPG_HOMING_SPEED )
				{
					m_flLastHomingSpeed = RPG_HOMING_SPEED;
				}
			}
			else
			{
				m_flLastHomingSpeed += HOMING_SPEED_ACCEL * UTIL_GetSimulationInterval();
				if ( m_flLastHomingSpeed > RPG_HOMING_SPEED )
				{
					m_flLastHomingSpeed = RPG_HOMING_SPEED;
				}
			}
		}
		*pHomingSpeed = m_flLastHomingSpeed;
		*pActualDotPosition = pLaserTarget->WorldSpaceCenter();
		return;
	}

	Vector vLaserStart;
	GetShootPosition( pLaserDot, &vLaserStart );
	*pHomingSpeed = APC_LAUNCH_HOMING_SPEED;

	//Get the laser's vector
	Vector vecTargetPosition = pLaserTarget->BodyTarget( GetAbsOrigin(), false );

	// Compute leading position
	Vector vecLeadPosition;
	ComputeLeadingPosition( GetAbsOrigin(), pLaserTarget, &vecLeadPosition );

	Vector vecTargetToMissile, vecTargetToShooter;
	VectorSubtract( GetAbsOrigin(), vecTargetPosition, vecTargetToMissile ); 
	VectorSubtract( vLaserStart, vecTargetPosition, vecTargetToShooter );

	*pActualDotPosition = vecLeadPosition;

	float flMinHomingDistance = MIN_HOMING_DISTANCE;
	float flMaxHomingDistance = MAX_HOMING_DISTANCE;
	float flBlendTime = gpGlobals->curtime - m_flIgnitionTime;
	if ( flBlendTime > DOWNWARD_BLEND_TIME_START )
	{
		if ( m_flReachedTargetTime != 0.0f )
		{
			*pHomingSpeed = APC_HOMING_SPEED;
			float flDeltaTime = clamp( gpGlobals->curtime - m_flReachedTargetTime, 0.0f, CORRECTION_TIME );
			*pHomingSpeed = SimpleSplineRemapVal( flDeltaTime, 0.0f, CORRECTION_TIME, 0.2f, *pHomingSpeed );
			flMinHomingDistance = SimpleSplineRemapVal( flDeltaTime, 0.0f, CORRECTION_TIME, MIN_NEAR_HOMING_DISTANCE, flMinHomingDistance );
			flMaxHomingDistance = SimpleSplineRemapVal( flDeltaTime, 0.0f, CORRECTION_TIME, MAX_NEAR_HOMING_DISTANCE, flMaxHomingDistance );
		}
		else
		{
			flMinHomingDistance = MIN_NEAR_HOMING_DISTANCE;
			flMaxHomingDistance = MAX_NEAR_HOMING_DISTANCE;
			Vector vecDelta;
			VectorSubtract( GetAbsOrigin(), *pActualDotPosition, vecDelta );
			if ( vecDelta.z > MIN_HEIGHT_DIFFERENCE )
			{
				float flClampedHeight = clamp( vecDelta.z, MIN_HEIGHT_DIFFERENCE, MAX_HEIGHT_DIFFERENCE );
				float flHeightAdjustFactor = SimpleSplineRemapVal( flClampedHeight, MIN_HEIGHT_DIFFERENCE, MAX_HEIGHT_DIFFERENCE, 0.0f, 1.0f );

				vecDelta.z = 0.0f;
				float flDist = VectorNormalize( vecDelta );

				float flForwardOffset = 2000.0f;
				if ( flDist > flForwardOffset )
				{
					Vector vecNewPosition;
					VectorMA( GetAbsOrigin(), -flForwardOffset, vecDelta, vecNewPosition );
					vecNewPosition.z = pActualDotPosition->z;

					VectorLerp( *pActualDotPosition, vecNewPosition, flHeightAdjustFactor, *pActualDotPosition );
				}
			}
			else
			{
				m_flReachedTargetTime = gpGlobals->curtime;
			}
		}

		// Allows for players right at the edge of rocket range to be threatened
		if ( flBlendTime > 0.6f )
		{
			float flTargetLength = GetAbsOrigin().DistTo( pLaserTarget->WorldSpaceCenter() );
			flTargetLength = clamp( flTargetLength, flMinHomingDistance, flMaxHomingDistance ); 
			*pHomingSpeed = SimpleSplineRemapVal( flTargetLength, flMaxHomingDistance, flMinHomingDistance, *pHomingSpeed, 0.01f );
		}
	}

	float flDot = DotProduct2D( vecTargetToShooter.AsVector2D(), vecTargetToMissile.AsVector2D() );
	if ( ( flDot < 0 ) )	//|| m_bGuidingDisabled
	{
		*pHomingSpeed = 0.0f;
	}

	m_flLastHomingSpeed = *pHomingSpeed;

//	NDebugOverlay::Line( vecLeadPosition, GetAbsOrigin(), 0, 255, 0, true, 0.05f );
//	NDebugOverlay::Line( GetAbsOrigin(), *pActualDotPosition, 255, 0, 0, true, 0.05f );
//	NDebugOverlay::Cross3D( *pActualDotPosition, -Vector(4,4,4), Vector(4,4,4), 255, 0, 0, true, 0.05f );
}
