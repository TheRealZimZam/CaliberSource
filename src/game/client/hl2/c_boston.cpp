//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_ai_basenpc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_Boston : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_Boston, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

					C_Boston();
	virtual			~C_Boston();

private:
	C_Boston( const C_Boston & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_Boston, DT_NPC_Boston, CNPC_Boston)
END_RECV_TABLE()

C_Boston::C_Boston()
{
}


C_Boston::~C_Boston()
{
}


