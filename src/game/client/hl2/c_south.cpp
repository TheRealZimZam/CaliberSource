//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: This probably doesnt need to exist??
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
class C_South : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_South, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

					C_South();
	virtual			~C_South();

private:
	C_South( const C_South & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_South, DT_NPC_South, CNPC_South)
END_RECV_TABLE()

C_South::C_South()
{
}


C_South::~C_South()
{
}


