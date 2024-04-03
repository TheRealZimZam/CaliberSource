//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef HL1_ENTS_H
#define HL1_ENTS_H
#ifdef _WIN32
#pragma once
#endif

class CHL1Gib : public CBaseEntity
{
	DECLARE_CLASS( CHL1Gib, CBaseEntity );

public:
	void Spawn( const char *szGibModel );
	void BounceGibTouch ( CBaseEntity *pOther );
	void StickyGibTouch ( CBaseEntity *pOther );
	void WaitTillLand( void );
	void LimitVelocity( void );

	virtual int	ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }
	static	void SpawnHeadGib( CBaseEntity *pVictim );
	static	void SpawnRandomGibs( CBaseEntity *pVictim, int cGibs, int human );
	static  void SpawnStickyGibs( CBaseEntity *pVictim, Vector vecOrigin, int cGibs );

	int		m_bloodColor;
	int		m_cBloodDecals;
	int		m_material;
	float	m_lifeTime;

	DECLARE_DATADESC();
};


#endif // HL1_ENTS_H 
