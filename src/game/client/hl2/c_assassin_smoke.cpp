//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Smoke generator for the assassin
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "particlemgr.h"
#include "particle_prototype.h"
#include "particle_util.h"
#include "baseparticleentity.h"

// ------------------------------------------------------------------------- //
// Definitions
// ------------------------------------------------------------------------- //

#define NUM_ASS_EXPLOSION_PARTICLES	300
#define ASS_DUST_RADIUS				340
#define ASS_DUST_LIFETIME			3
#define ASS_DUST_SPEED				350
#define ASS_DUST_STARTSIZE			30
#define ASS_DUST_ENDSIZE			45


static Vector g_AssassinSmokeColor(0.3, 0.3, 0.3);


// ------------------------------------------------------------------------- //
// Classes
// ------------------------------------------------------------------------- //
class AssassinSmokeEmitter
{
public:
	Vector		m_Pos;
	Vector		m_Velocity;
	float		m_Lifetime;
	TimedEvent	m_ParticleSpawn;
};


class C_AssassinSmoke : public C_BaseParticleEntity, public IPrototypeAppEffect
{
public:
	DECLARE_CLASS( C_AssassinSmoke, C_BaseParticleEntity );
	DECLARE_CLIENTCLASS();

					C_AssassinSmoke();
					~C_AssassinSmoke();

// C_BaseEntity.
public:
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual	bool	ShouldDraw();


// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual bool	SimulateAndRender(Particle *pParticle, ParticleDraw *pDraw, float &sortKey);


public:
	CParticleMgr		*m_pParticleMgr;
	PMaterialHandle	m_MaterialHandle;

private:
	C_AssassinSmoke( const C_AssassinSmoke & );
};

// Expose to the particle app.
EXPOSE_PROTOTYPE_EFFECT(AssassinSmoke, C_AssassinSmoke);

IMPLEMENT_CLIENTCLASS_DT(C_AssassinSmoke, DT_AssassinSmoke, CAssassinSmoke)
END_RECV_TABLE()




// ------------------------------------------------------------------------- //
// Helpers.
// ------------------------------------------------------------------------- //


// ------------------------------------------------------------------------- //
// C_AssassinSmoke
// ------------------------------------------------------------------------- //
C_AssassinSmoke::C_AssassinSmoke()
{
	m_pParticleMgr = NULL;
	m_MaterialHandle = INVALID_MATERIAL_HANDLE;
}


C_AssassinSmoke::~C_AssassinSmoke()
{
}


void C_AssassinSmoke::OnDataChanged(DataUpdateType_t updateType)
{
	C_BaseEntity::OnDataChanged(updateType);

	if(updateType == DATA_UPDATE_CREATED)
	{
		Start(ParticleMgr(), NULL);
	}
}


bool C_AssassinSmoke::ShouldDraw()
{
	return true;
}


void C_AssassinSmoke::Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs)
{
	m_pParticleMgr = pParticleMgr;
	if(!pParticleMgr->AddEffect(&m_ParticleEffect, this))
		return;

	m_MaterialHandle = m_ParticleEffect.FindOrAddMaterial("particle/particle_noisesphere");
	

	// Center of explosion.
	Vector vCenter = GetAbsOrigin(); // HACKHACK.. when the engine bug is fixed, use origin.
	Vector velocity;

	int i;

	for( i = 0 ; i < 75 ; i++ )
	{
		velocity.z = random->RandomFloat( 50, 100 );
		velocity.x = random->RandomFloat( -100, 100 );
		velocity.y = random->RandomFloat( -100, 100 );

		StandardParticle_t *pParticle = (StandardParticle_t*)m_ParticleEffect.AddParticle( 
			sizeof(StandardParticle_t), m_MaterialHandle );

		if(pParticle)
		{
			pParticle->m_Pos = vCenter;
			pParticle->m_Velocity = velocity;
		}
		else
		{
			break;
		}
	}
}


void C_AssassinSmoke::Update(float fTimeDelta)
{
	if(!m_pParticleMgr)
		return;
}


bool C_AssassinSmoke::SimulateAndRender(Particle *pBaseParticle, ParticleDraw *pDraw, float &sortKey)
{
	StandardParticle_t* pParticle = (StandardParticle_t*)pBaseParticle;

	// Update its lifetime.
	pParticle->m_Lifetime += pDraw->GetTimeDelta();
	if(pParticle->m_Lifetime > ASS_DUST_LIFETIME)
	{
		return false;
	}

	// Draw.
	float lifetimePercent = pParticle->m_Lifetime / ASS_DUST_LIFETIME;
	
	//Vector color = g_AssassinSmokeColor;

	Vector color;
	float  component;

	component = random->RandomFloat( 0.3, 0.35 );

	color.x = component;
	color.y = component;
	color.z = component;
	
	Vector tPos;
	TransformParticle(m_pParticleMgr->GetModelView(), pParticle->m_Pos, tPos);
	sortKey = tPos.z;
	
	RenderParticle_ColorSize(
		pDraw,
		tPos,
		color,
		0.6f * sin(3.14159 * lifetimePercent),
		ASS_DUST_STARTSIZE + (ASS_DUST_ENDSIZE - ASS_DUST_STARTSIZE) * lifetimePercent);

	// Move it (this comes after rendering to make it clear that moving the particle here won't change
	// its rendering for this frame since m_TransformedPos has already been set).
	pParticle->m_Pos = pParticle->m_Pos + pParticle->m_Velocity * pDraw->GetTimeDelta();

	// Dampen velocity.
	static float dampenAmount = 0.95f;
	pParticle->m_Velocity = pParticle->m_Velocity * dampenAmount;
	return true;
}
