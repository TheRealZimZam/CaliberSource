//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Contains the splash effect
//
// TODO; Perf, need to limit how many particles are spewing from this effect at once
//=============================================================================//
#include "cbase.h"
#include "IViewRender.h"
#include "ClientEffectPrecacheSystem.h"
#include "c_splash.h"
#include "c_tracer.h"
#include "iefx.h"
#include "decals.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SPLASH_MAX_FALL_DIST 1000
#define SPLASH_SOUND_DELAY	 0.4

// Expose to the particle app.
EXPOSE_PROTOTYPE_EFFECT(Splash, C_Splash);

// Datatable.. this can have all the smoketrail parameters when we need it to.
IMPLEMENT_CLIENTCLASS_DT(C_Splash, DT_Splash, Splash)
	RecvPropFloat(RECVINFO(m_flSpawnRate)),
	RecvPropVector(RECVINFO(m_vStartColor)),
	RecvPropVector(RECVINFO(m_vEndColor)),
	RecvPropFloat(RECVINFO(m_flParticleLifetime)),
	RecvPropFloat(RECVINFO(m_flStopEmitTime)),
	RecvPropFloat(RECVINFO(m_flSpeed)),
	RecvPropFloat(RECVINFO(m_flSpeedRange)),
	RecvPropFloat(RECVINFO(m_flWidthMin)),
	RecvPropFloat(RECVINFO(m_flWidthMax)),
	RecvPropFloat(RECVINFO(m_flNoise)),
	RecvPropInt(RECVINFO(m_bSplashSound)),
	RecvPropInt(RECVINFO(m_bEmit)),
	RecvPropInt(RECVINFO(m_nNumDecals)),
END_RECV_TABLE()

// FIXME: These all have to be moved out of this old system and into the new to leverage art assets!
CLIENTEFFECT_REGISTER_BEGIN( PrecacheLeakEffect )
CLIENTEFFECT_MATERIAL( "particle/particle_smokegrenade" )
CLIENTEFFECT_REGISTER_END()

// ------------------------------------------------------------------------- //
// ParticleMovieExplosion
// ------------------------------------------------------------------------- //
C_Splash::C_Splash()
{
	m_pParticleMgr = NULL;
	m_MaterialHandle = INVALID_MATERIAL_HANDLE;
	
	m_flSpawnRate = 10;
	m_tParticleSpawn.Init(10);
	m_vStartColor.Init(0.6, 0.6, 0.7);
	m_vEndColor.Init(0,0,0);
	m_flParticleLifetime = 5;
	//m_flStartEmitTime = gpGlobals->curtime;
	m_flStopEmitTime	= 0;	// No end time
	m_flSpeed		= 20;
	m_flSpeedRange	= 10;
	m_flWidthMin	= 2;
	m_flWidthMax	= 8;
	m_flNoise		= 0.1;
	m_Pos.Init();

	m_bSplashSound = true;
	m_bEmit = true;
	m_nNumDecals = 1;

	//m_pLeakEmitter = NULL;
}

void C_Splash::SetPos(const Vector &pos, bool bInitial)
{
	m_Pos = pos;
}

void C_Splash::SetEmit(bool bEmit)
{
	m_bEmit = bEmit;
}

void C_Splash::SetSpawnRate(float rate)
{
	m_flSpawnRate = rate;
	m_tParticleSpawn.Init(rate);
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void C_Splash::InitParticleCollisions(void)
{
	float flMinSpeed = m_flSpeed - m_flSpeedRange;
	float flMaxSpeed = m_flSpeed + m_flSpeedRange;

	Vector vForward;
	AngleVectors(GetAbsAngles(),&vForward);
	m_ParticleCollision.Setup( GetAbsOrigin(), &vForward, m_flNoise, flMinSpeed, flMaxSpeed, 500, 0.5 );
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void C_Splash::OnDataChanged(DataUpdateType_t updateType)
{
	C_BaseEntity::OnDataChanged(updateType);

	if(updateType == DATA_UPDATE_CREATED)
	{
		SetPos(GetAbsOrigin(), true);
		Start(ParticleMgr(), NULL);
	}
	InitParticleCollisions();
}

bool C_Splash::ShouldDraw()
{
	return true;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void C_Splash::Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs)
{
	if(!pParticleMgr->AddEffect( &m_ParticleEffect, this ))
		return;

	m_tParticleSpawn.Init(m_flSpawnRate);
	m_pParticleMgr = pParticleMgr;

	if ( !m_pParticleMgr )
	{
		Assert( false );
		return;
	}

	//m_pLeakEmitter = CSimpleEmitter::Create("LeakEffect");

	m_Pos				= GetAbsOrigin();
	m_flStartEmitTime	= gpGlobals->curtime;
	// Cap leaktime at 20
	if (m_flStopEmitTime > gpGlobals->curtime+20)
		m_flStopEmitTime = gpGlobals->curtime+20;

	// Trace down to see where particles should disappear
	trace_t tr;
	UTIL_TraceLine( m_Pos+Vector(0,0,1), m_Pos+Vector(0,0,-SPLASH_MAX_FALL_DIST), 
		MASK_SOLID_BRUSHONLY|CONTENTS_WATER, NULL, COLLISION_GROUP_NONE, &tr );
	m_flSplashEndZ = tr.endpos.z;

	m_MaterialHandle = m_ParticleEffect.FindOrAddMaterial("particle/particle_smokegrenade");

	m_nDecalsRemaining	= m_nNumDecals;
	m_flNextSoundTime	= 0;

	InitParticleCollisions();

#if 0
	ClientEntityList().AddNonNetworkableEntity(	this );
	CollisionProp()->CreatePartitionHandle();
	AddToLeafSystem();
#endif
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void C_Splash::PlaySplashSound(Vector vPos)
{
	if ( !m_bSplashSound )
		return;

	// TODO; We need to check if its a looping sound here
	if (gpGlobals->curtime > m_flNextSoundTime)
	{
		CLocalPlayerFilter filter;
		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName =  "Splash.SplashSound";
		ep.m_flVolume = 1.0f;
		ep.m_SoundLevel = SNDLVL_NORM;
		ep.m_pOrigin = &vPos;
		C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );

		m_flNextSoundTime = gpGlobals->curtime + SPLASH_SOUND_DELAY;
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void C_Splash::Update(float fTimeDelta)
{
	if(!m_pParticleMgr)
		return;

	m_Pos = GetAbsOrigin();

	// Add new particles.
	SimpleParticle	*pParticle;

	if(m_bEmit)
	{
		if (m_flStopEmitTime == 0	|| 
			m_flStopEmitTime > gpGlobals->curtime )
		{
			float tempDelta = fTimeDelta;
			while(m_tParticleSpawn.NextEvent(tempDelta))
			{	
				//Make a new particle
				pParticle = (SimpleParticle*)m_ParticleEffect.AddParticle( sizeof(StandardParticle_t), m_MaterialHandle );	//m_ParticleEffect.AddParticle
				if (pParticle)
				{
					Vector vForward;
					AngleVectors(GetAbsAngles(),&vForward);
					vForward.x += random->RandomFloat( -m_flNoise, m_flNoise );
					vForward.y += random->RandomFloat( -m_flNoise, m_flNoise );
					vForward.z += random->RandomFloat( -m_flNoise, m_flNoise );

					Vector vNewPos	=  GetAbsOrigin();
					Vector vNewVel	=  vForward*(m_flSpeed + random->RandomFloat(-m_flSpeedRange,m_flSpeedRange));
					pParticle->m_Pos			= vNewPos;
					pParticle->m_vecVelocity	= vNewVel;
					pParticle->m_flLifetime		= 0;
					pParticle->m_flDieTime		= m_flParticleLifetime;
				}
				else
				{
					break;
				}
			}
		}
	}
	
	// Decay amount of leak over time
	if (m_flStopEmitTime != 0)
	{
		float lifetimePercent = 1-((gpGlobals->curtime - m_flStartEmitTime) / (m_flStopEmitTime - m_flStartEmitTime));
		m_tParticleSpawn.ResetRate(m_flSpawnRate*lifetimePercent);
	}
	else
	{
		m_tParticleSpawn.ResetRate(m_flSpawnRate);
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
/*
bool C_Splash::SimulateAndRender(Particle *pParticle, ParticleDraw *pDraw, float &sortKey)
{
	SimpleParticle* pSimpleParticle = (SimpleParticle*)pParticle;

	//Should this particle die?
	pSimpleParticle->m_flLifetime += pDraw->GetTimeDelta();
	if ( pSimpleParticle->m_flLifetime >= m_flParticleLifetime )
		return false;

	// Calculate color
	float lifetimePercent = pSimpleParticle->m_flLifetime / m_flParticleLifetime;
	float color[3];
	color[0] = m_vStartColor[0] + (m_vEndColor[0] - m_vStartColor[0]) * lifetimePercent;
	color[1] = m_vStartColor[1] + (m_vEndColor[1] - m_vStartColor[1]) * lifetimePercent;
	color[2] = m_vStartColor[2] + (m_vEndColor[2] - m_vStartColor[2]) * lifetimePercent;
	color[3] = 1.0;

	float scale = random->RandomFloat( 0.02, 0.08 );

	// NOTE: We need to do everything in screen space
	Vector  delta;
	Vector	start;
	TransformParticle(ParticleMgr()->GetModelView(), pSimpleParticle->m_Pos, start);

	Vector3DMultiply( CurrentWorldToViewMatrix(), pSimpleParticle->m_vecVelocity, delta );

	delta[0] *= scale;
	delta[1] *= scale;
	delta[2] *= scale;

	// See c_tracer.* for this method
	Tracer_Draw( pDraw, start, delta, random->RandomInt( m_flWidthMin, m_flWidthMax ), color );

	//Simulate the movement with collision
	const float	timeDelta = pDraw->GetTimeDelta();
	trace_t trace;
	
	if (m_ParticleCollision.MoveParticle( pSimpleParticle->m_Pos, pSimpleParticle->m_vecVelocity, NULL, timeDelta, &trace ))
	{
		// If particle hits horizontal surface kill it soon
		if (DotProduct(trace.plane.normal, CurrentViewUp())>0.8)
		{
			pSimpleParticle->m_flLifetime = m_flParticleLifetime-0.2;
		}

		// Drop a decal if any remaining
		if (m_nDecalsRemaining>0)
		{
			C_BaseEntity *ent = cl_entitylist->GetEnt( 0 );
			if ( ent )
			{
				int index = decalsystem->GetDecalIndexForName( "Splash" );
				if ( index >= 0 )
				{
					color32 rgbaColor = {255*m_vStartColor[0],255*m_vStartColor[1],255*m_vStartColor[2],150};
					effects->DecalColorShoot( index, 0, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), trace.endpos, 0, 0, rgbaColor);
				}
				m_nDecalsRemaining--;
			}
		}
		PlaySplashSound(trace.endpos);
	}
	return true;
}
*/

void C_Splash::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	float timeDelta = pIterator->GetTimeDelta();
	SimpleParticle *pParticle = (SimpleParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		//Should this particle die?
		pParticle->m_flLifetime += timeDelta;
		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
		{
			pIterator->RemoveParticle( pParticle );
		}
		else
		{
			//Simulate the movement with collision
			trace_t trace;
			
			if (m_ParticleCollision.MoveParticle( pParticle->m_Pos, pParticle->m_vecVelocity, NULL, timeDelta, &trace ))
			{
				// If particle hits horizontal surface kill it soon
				if (DotProduct(trace.plane.normal, CurrentViewUp())>0.8)
				{
					pParticle->m_flLifetime = m_flParticleLifetime-0.2;
				}

				// Drop a decal if any remaining
				if (m_nDecalsRemaining>0)
				{
					C_BaseEntity *ent = cl_entitylist->GetEnt( 0 );
					if ( ent )
					{
						int index = decalsystem->GetDecalIndexForName( "Splash" );
						if ( index >= 0 )
						{
							color32 rgbaColor = {255*m_vStartColor[0],255*m_vStartColor[1],255*m_vStartColor[2],150};
							effects->DecalColorShoot( index, 0, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), trace.endpos, 0, 0, rgbaColor);
						}
						m_nDecalsRemaining--;
					}
				}
				PlaySplashSound(trace.endpos);
			}
		}

		pParticle = (SimpleParticle*)pIterator->GetNext();
	}
}


void C_Splash::RenderParticles( CParticleRenderIterator *pIterator )
{
	const SimpleParticle *pParticle = (const SimpleParticle *)pIterator->GetFirst();
	while ( pParticle )
	{
		// Calculate color
		float lifetimePercent = pParticle->m_flLifetime / m_flParticleLifetime;
		float color[3];
		color[0] = m_vStartColor[0] + (m_vEndColor[0] - m_vStartColor[0]) * lifetimePercent;
		color[1] = m_vStartColor[1] + (m_vEndColor[1] - m_vStartColor[1]) * lifetimePercent;
		color[2] = m_vStartColor[2] + (m_vEndColor[2] - m_vStartColor[2]) * lifetimePercent;

		float scale = random->RandomFloat( 0.02, 0.08 );

		// NOTE: We need to do everything in screen space
		Vector  delta;
		Vector	start;
		TransformParticle(ParticleMgr()->GetModelView(), pParticle->m_Pos, start);
		float sortKey = start.z;

		float alpha = 1.0f * GetAlphaDistanceFade(start, 10, 100);
		color[3] = clamp(alpha, 0.4f, 1.0f);

		Vector3DMultiply( CurrentWorldToViewMatrix(), pParticle->m_vecVelocity, delta );

		delta[0] *= scale;
		delta[1] *= scale;
		delta[2] *= scale;

		// See c_tracer.* for this method
		Tracer_Draw( pIterator->GetParticleDraw(), start, delta, random->RandomInt( m_flWidthMin, m_flWidthMax ), color );

		pParticle = (const SimpleParticle *)pIterator->GetNext( sortKey );
	}
}

void C_Splash::Think(void)
{
	Release();
	//BaseClass::Think();
}

C_Splash::~C_Splash()
{
	if(m_pParticleMgr)
	{
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
#if 0
		ClientThinkList()->RemoveThinkable( GetClientHandle() );

		// Remove from the client entity list.
		ClientEntityList().RemoveEntity( GetClientHandle() );

		partition->Remove( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS, CollisionProp()->GetPartitionHandle() );

		RemoveFromLeafSystem();
#endif
	}
}
