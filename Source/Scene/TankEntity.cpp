/*******************************************
	TankEntity.cpp

	Tank entity template and entity classes
********************************************/

// Additional technical notes for the assignment:
// - Each tank has a team number (0 or 1), HP and other instance data - see the end of TankEntity.h
//   You will need to add other instance data suitable for the assignment requirements
// - A function GetTankUID is defined in TankAssignment.cpp and made available here, which returns
//   the UID of the tank on a given team. This can be used to get the enemy tank UID
// - Tanks have three parts: the root, the body and the turret. Each part has its own matrix, which
//   can be accessed with the Matrix function - root: Matrix(), body: Matrix(1), turret: Matrix(2)
//   However, the body and turret matrix are relative to the root's matrix - so to get the actual 
//   world matrix of the body, for example, we must multiply: Matrix(1) * Matrix()
// - Vector facing work similar to the car tag lab will be needed for the turret->enemy facing 
//   requirements for the Patrol and Aim states
// - The CMatrix4x4 function DecomposeAffineEuler allows you to extract the x,y & z rotations
//   of a matrix. This can be used on the *relative* turret matrix to help in rotating it to face
//   forwards in Evade state
// - The CShellEntity class is simply an outline. To support shell firing, you will need to add
//   member data to it and rewrite its constructor & update function. You will also need to update 
//   the CreateShell function in EntityManager.cpp to pass any additional constructor data required
// - Destroy an entity by returning false from its Update function - the entity manager wil perform
//   the destruction. Don't try to call DestroyEntity from within the Update function.
// - As entities can be destroyed, you must check that entity UIDs refer to existant entities, before
//   using their entity pointers. The return value from EntityManager.GetEntity will be NULL if the
//   entity no longer exists. Use this to avoid trying to target a tank that no longer exists etc.

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

extern int NumTanksPerTeam = 3;
extern int NumPoints = 7;
extern CVector3 FrontPatrolPoints[];
extern CVector3 BackPatrolPoints[];

// Helper function made available from TankAssignment.cpp - gets UID of tank A (team 0) or B (team 1).
// Will be needed to implement the required tank behaviour in the Update function below
extern TEntityUID GetTankUID( int team, int pos );

extern bool PointToSphere(const int radius, const CVector3 currentPos, const CVector3 target);

extern bool PointToAABB(const int xSize, const int zSize, const CVector3 currentPos, const CVector3 target);

/*-----------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------
	Tank Entity Class
-------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------*/

// Tank constructor intialises tank-specific data and passes its parameters to the base
// class constructor
CTankEntity::CTankEntity
(
	CTankTemplate*  tankTemplate,
	TEntityUID      UID,
	TUInt32         team,
	const string&   name /*=""*/,
	const CVector3& position /*= CVector3::kOrigin*/, 
	const CVector3& rotation /*= CVector3( 0.0f, 0.0f, 0.0f )*/,
	const CVector3& scale /*= CVector3( 1.0f, 1.0f, 1.0f )*/
) : CEntity( tankTemplate, UID, name, position, rotation, scale )
{
	m_TankTemplate = tankTemplate;

	// Tanks are on teams so they know who the enemy is
	m_Team = team;

	// Initialise other tank data and state
	m_Speed = 0.0f;
	m_HP = m_TankTemplate->GetMaxHP();
	m_State = Inactive;
	m_PatrolPointCounter = 0;
	if (m_Team == 0) { m_PatrolType = Front; m_TargetPoint = FrontPatrolPoints[0]; }
	else { m_PatrolType = Back; m_TargetPoint = BackPatrolPoints[0];  }
	m_Timer = 0.0f;
	m_ShellsFired = 0;
}


// Update the tank - controls its behaviour. The shell code just performs some test behaviour, it
// is to be rewritten as one of the assignment requirements
// Return false if the entity is to be destroyed
bool CTankEntity::Update( TFloat32 updateTime )
{
	// Fetch any messages
	SMessage msg;
	while (Messenger.FetchMessage( GetUID(), &msg ))
	{
		// Set state variables based on received messages
		switch (msg.type)
		{
			case Msg_Go:
				m_State = Patrol;
				break;
			case Msg_Stop:
				m_State = Inactive;
				break;
			case Msg_Hit:
				m_HP -= msg.data;
				if (m_HP <= 0)
				{ 
					return false;
				}
				break;
		}
	}

	// Tank behaviour
	// Only move if in Go state
	if (m_State == Patrol)
	{
		if (TankInTurretRange())
		{
			m_Speed = 0.0f;
			m_Timer = 1.0f;
			m_State = Aim;
		}

		if (PointToSphere(5.0f, Matrix().Position(), m_TargetPoint)) { ChangePatrolPoint(); }

		if (GetTurnAmount(m_TargetPoint, Matrix()) > 0.1)
		{
			Matrix().RotateLocalY(m_TankTemplate->GetTurnSpeed() * updateTime);
		}
		else if (GetTurnAmount(m_TargetPoint, Matrix()) < -0.1)
		{
			Matrix().RotateLocalY(-m_TankTemplate->GetTurnSpeed() * updateTime);
		}

		Matrix(2).RotateLocalY(m_TankTemplate->GetTurretTurnSpeed() * updateTime);
		if (m_Speed < m_TankTemplate->GetMaxSpeed())
		{
			m_Speed += m_TankTemplate->GetAcceleration() * updateTime;
		}
		else
		{
			m_Speed = m_TankTemplate->GetMaxSpeed();
		}
	}
	else if (m_State == Aim)
	{
		m_Timer -= updateTime;

		if (m_TargetTank)
		{
			const CVector3 targetTank = m_TargetTank->Position();

			if (GetTurnAmount(targetTank, (Matrix(2) * Matrix())) > 0)
			{
				Matrix(2).RotateLocalY(m_TankTemplate->GetTurretTurnSpeed() * 1.5f * updateTime);
			}
			else
			{
				Matrix(2).RotateLocalY(-m_TankTemplate->GetTurretTurnSpeed() * 1.5f * updateTime);
			}

			if (m_Timer < 0.0f)
			{
				CVector3 position;
				CVector3 rotation;
				CVector3 scale;
				(Matrix(2) * Matrix()).DecomposeAffineEuler(&position, &rotation, &scale);
				EntityManager.CreateShell("Shell Type 1", "Name", GetUID(), m_TankTemplate->GetShellDamage(), position, rotation, scale);
				m_ShellsFired += 1;
				m_TargetPoint.x = Random(Matrix().GetX() - 40.0f, Matrix().GetX() + 40.0f);
				m_TargetPoint.z = Random(Matrix().GetZ() - 40.0f, Matrix().GetZ() + 40.0f);
				m_State = Evade;
			}
		}
	}
	else if (m_State == Evade)
	{
		float turretTargetDir = 0.0f;
		const CVector3 turretXAxis = Normalise((Matrix(2) * Matrix()).XAxis());
		const CVector3 turretZAxis = Normalise((Matrix(2) * Matrix()).ZAxis());
		const CVector3 turretTarget = Normalise(Matrix().ZAxis());

		if (Dot(turretXAxis, turretTarget) > 0)
		{
			turretTargetDir = ACos(Dot(turretZAxis, turretTarget));
		}
		else
		{
			turretTargetDir = -ACos(Dot(turretZAxis, turretTarget));
		}

		if (turretTargetDir > 0.01)
		{
			Matrix(2).RotateLocalY(m_TankTemplate->GetTurnSpeed() * 1.5f * updateTime);
		}
		else if (turretTargetDir < -0.01)
		{
			Matrix(2).RotateLocalY(-m_TankTemplate->GetTurnSpeed() * 1.5f * updateTime);
		}

		if (GetTurnAmount(m_TargetPoint, Matrix()) > 0.1)
		{
			Matrix().RotateLocalY(m_TankTemplate->GetTurnSpeed() * updateTime);
		}
		else if (GetTurnAmount(m_TargetPoint, Matrix()) < -0.1)
		{
			Matrix().RotateLocalY(-m_TankTemplate->GetTurnSpeed() * updateTime);
		}

		if (PointToSphere(5.0f, Matrix().Position(), m_TargetPoint))
		{ 
			if (m_PatrolType == Front) { m_TargetPoint = FrontPatrolPoints[m_PatrolPointCounter]; }
			else { m_TargetPoint = BackPatrolPoints[m_PatrolPointCounter]; }
			m_State = Patrol;
		}

		if (m_Speed < m_TankTemplate->GetMaxSpeed())
		{
			m_Speed += m_TankTemplate->GetAcceleration() * updateTime;
		}
		else
		{
			m_Speed = m_TankTemplate->GetMaxSpeed();
		}
	}
	else
	{
		m_Speed = 0.0f;
	}

	// Perform movement...
	// Move along local Z axis scaled by update time
	Matrix().MoveLocalZ( m_Speed * updateTime );

	return true; // Don't destroy the entity
}

void CTankEntity::ChangePatrolPoint()
{
	if (m_PatrolType == Front)
	{
		if (m_PatrolPointCounter == NumPoints - 1)
		{
			m_PatrolPointCounter = 0;
			m_PatrolType = Back;
			m_TargetPoint = BackPatrolPoints[m_PatrolPointCounter];
		}
		else
		{
			m_PatrolPointCounter++;
			m_TargetPoint = FrontPatrolPoints[m_PatrolPointCounter];
		}
	}
	else
	{
		if (m_PatrolPointCounter == NumPoints - 1)
		{
			m_PatrolPointCounter = 0;
			m_PatrolType = Front;
			m_TargetPoint = FrontPatrolPoints[m_PatrolPointCounter];
		}
		else
		{
			m_PatrolPointCounter++;
			m_TargetPoint = BackPatrolPoints[m_PatrolPointCounter];
		}
	}
}

float CTankEntity::GetTurnAmount(CVector3 target, CMatrix4x4 matrix)
{
	target = Normalise(target - matrix.Position());

	if (Dot(Normalise(matrix.XAxis()), target) > 0)
	{
		return ACos(Dot(Normalise(matrix.ZAxis()), target));
	}
	else
	{
		return -ACos(Dot(Normalise(matrix.ZAxis()), target));
	}
}

bool CTankEntity::TankInTurretRange()
{
	//CVector3(0.0f, 0.0f, 40.0f)
	for (int i = 0; i < NumTanksPerTeam; i++)
	{
		CVector3 turretVector = (Matrix(2) * Matrix(1) * Matrix()).ZAxis();
		CEntity* enemyTank = EntityManager.GetEntity(GetTankUID(1 - m_Team, i));

		if (enemyTank)
		{
			CVector3 enemyPos = enemyTank->Position();
			float angle = acos(Dot(turretVector, enemyPos) / (turretVector.Length() * enemyPos.Length()));

			if (angle < ToRadians(15.0f))
			{
				CVector3 currentPos = Normalise(enemyPos - Matrix(2).ZAxis());

				while (currentPos.DistanceTo(enemyPos) > 5.0f)
				{
					if (PointToAABB(5.0f, 5.0f, Matrix(2).ZAxis(), CVector3(0.0f, 0.0f, 40.0f))) return false;
					else { currentPos += Normalise(enemyPos - Matrix(2).ZAxis()); }
				}

				m_TargetTank = enemyTank;
				return true;
			}
		}
	}

	return false;
}

} // namespace gen
