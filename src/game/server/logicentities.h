//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LOGICENTITIES_H
#define LOGICENTITIES_H
#ifdef _WIN32
#pragma once
#endif

//
// CLogicalEntitySprite
// Assigns a sprite to any entities that use this class
//
class CLogicalEntitySprite : public CLogicalEntity
{
	DECLARE_CLASS( CLogicalEntitySprite, CLogicalEntity );

public:
	CLogicalEntitySprite( void );
	void Precache( void );
	void Spawn( void );
	void Think( void );

	DECLARE_DATADESC();
private:
	string_t m_iszSprite;
};



#endif // LOGICENTITIES_H
