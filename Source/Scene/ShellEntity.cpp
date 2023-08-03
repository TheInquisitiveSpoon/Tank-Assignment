/*******************************************
	ShellEntity.cpp

	Shell entity class
********************************************/

#include "ShellEntity.h"
#include "TankEntity.h"
#include "EntityManager.h"
#include "Messenger.h"

namespace gen
{

// Reference to entity manager from TankAssignment.cpp, allows look up of entities by name, UID etc.
// Can then access other entity's data. See the CEntityManager.h file for functions. Example:
//    CVector3 targetPos = EntityManager.GetEntity( targetUID )->GetMatrix().Position();
extern CEntityManager EntityManager;

// Messenger class for sending messages to and between entities
extern CMessenger Messenger;

// Helper function made available from TankAssignment.cpp - gets UID of tank A (team 0) or B (team 1).
// Will be needed to implement the required shell behaviour in the Update function below
extern TEntityUID GetTankUID( int team, int pos );

extern int GetNumTanksPerTeam();

extern bool PointToSphere(const int radius, const CVector3 currentPos, const CVector3 target);
extern bool PointToAABB(const int xSize, const int zSize, const CVector3 currentPos, const CVector3 target);

/*-----------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------
	Shell Entity Class
-------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------*/

// Shell constructor intialises shell-specific data and passes its parameters to the base
// class constructor
CShellEntity::CShellEntity
(
	CEntityTemplate* entityTemplate,
	TEntityUID       UID,
	const string&    name /*=""*/,
	const TEntityUID parent,
	const TInt32	 damage,
	const CVector3&  position /*= CVector3::kOrigin*/, 
	const CVector3&  rotation /*= CVector3( 0.0f, 0.0f, 0.0f )*/,
	const CVector3&  scale /*= CVector3( 1.0f, 1.0f, 1.0f )*/
) : CEntity( entityTemplate, UID, name, position, rotation, scale )
{
	// Initialise any shell data you add
	m_Timer = 3.0f;
	m_ParentTank = parent;
	m_ShellDamage = damage;
}


// Update the shell - controls its behaviour. The shell code is empty, it needs to be written as
// one of the assignment requirements
// Return false if the entity is to be destroyed
bool CShellEntity::Update( TFloat32 updateTime )
{
	m_Timer -= updateTime;

	if (m_Timer < 0.0f)
	{
		return false;
	}

	if (PointToAABB(5.0f, 5.0f, Matrix().Position(), CVector3(0.0f, 0.0f, 40.0f))) return false;

	for (int i = 0; i < GetNumTanksPerTeam(); i++)
	{
		CEntity* tank = EntityManager.GetEntity(GetTankUID(0, i));

		if (tank)
		{
			if (tank->GetUID() != m_ParentTank)
			{
				if (PointToSphere(1.0f, Matrix().Position(), tank->Matrix().Position()))
				{
					SMessage msg;
					msg.type = Msg_Hit;
					msg.from = GetUID();
					msg.data = m_ShellDamage;
					Messenger.SendMessageA(tank->GetUID(), msg);
					return false;
				}
			}
		}
	}

	for (int i = 0; i < GetNumTanksPerTeam(); i++)
	{
		CEntity* tank = EntityManager.GetEntity(GetTankUID(1, i));

		if (tank)
		{
			if (tank && tank->GetUID() != m_ParentTank)
			{
				if (PointToSphere(1.0f, Matrix().Position(), tank->Matrix().Position()))
				{
					SMessage msg;
					msg.type = Msg_Hit;
					msg.from = GetUID();
					msg.data = m_ShellDamage;
					Messenger.SendMessageA(tank->GetUID(), msg);
					return false;
				}
			}
		}
	}

	Matrix().MoveLocalZ(100.0f * updateTime);
	return true; // Placeholder
}


} // namespace gen
