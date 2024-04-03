//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: Map Parameters - Sets special options on a per-level basis
//
//=============================================================================

#ifndef MAPINFO_H
#define MAPINFO_H
#ifdef _WIN32
#pragma once
#endif


#include "baseentity.h"


class CMapInfo : public CPointEntity
{
public :
	CMapInfo();
	virtual ~CMapInfo();

	bool KeyValue( const char *szKeyName, const char *szValue );
	void Spawn();

public:
//	float m_flBombRadius;
	bool m_bKillstreaks;	//TEST
	bool m_bTaunts;	//TEST
};


// The info_map_parameters entity in this map (only one is allowed for).
extern CMapInfo *g_pMapInfo;


#endif // MAPINFO_H
