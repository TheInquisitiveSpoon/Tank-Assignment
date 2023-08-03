/*******************************************
	TankAssignment.cpp

	Shell scene and game functions
********************************************/

#include <sstream>
#include <string>
using namespace std;

#include <d3d10.h>
#include <d3dx10.h>

#include "Defines.h"
#include "CVector3.h"
#include "Camera.h"
#include "Light.h"
#include "EntityManager.h"
#include "Messenger.h"
#include "TankAssignment.h"

namespace gen
{

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Control speed
const float CameraRotSpeed = 2.0f;
float CameraMoveSpeed = 80.0f;

// Amount of time to pass before calculating new average update time
const float UpdateTimePeriod = 1.0f;


//-----------------------------------------------------------------------------
// Global system variables
//-----------------------------------------------------------------------------

// Get reference to global DirectX variables from another source file
extern ID3D10Device*           g_pd3dDevice;
extern IDXGISwapChain*         SwapChain;
extern ID3D10DepthStencilView* DepthStencilView;
extern ID3D10RenderTargetView* BackBufferRenderTarget;
extern ID3DX10Font*            OSDFont;

// Actual viewport dimensions (fullscreen or windowed)
extern TUInt32 ViewportWidth;
extern TUInt32 ViewportHeight;

// Current mouse position
extern TUInt32 MouseX;
extern TUInt32 MouseY;

// Messenger class for sending messages to and between entities
extern CMessenger Messenger;


//-----------------------------------------------------------------------------
// Global game/scene variables
//-----------------------------------------------------------------------------

// Entity manager
CEntityManager EntityManager;

// Tank UIDs
const int NumTanksPerTeam = 3;
TEntityUID ATanks[NumTanksPerTeam];
TEntityUID BTanks[NumTanksPerTeam];
CEntity* selectedTank;

// Patrol Points
const int NumPoints = 7;
CVector3 FrontPatrolPoints[NumPoints];
CVector3 BackPatrolPoints[NumPoints];

// Cameras
enum ECamera
{
	Main,
	TankA1,
	TankA2,
	TankA3,
	TankB1,
	TankB2,
	TankB3
};

ECamera currentCam = Main;
CCamera* MainCamera;
CCamera* TankA1Cam;
CCamera* TankA2Cam;
CCamera* TankA3Cam;
CCamera* TankB1Cam;
CCamera* TankB2Cam;
CCamera* TankB3Cam;

// Other scene elements
const int NumLights = 2;
CLight*  Lights[NumLights];
SColourRGBA AmbientLight;
bool ShowExtraUI = true;
CVector3 mousePos;

// Sum of recent update times and number of times in the sum - used to calculate
// average over a given time period
float SumUpdateTimes = 0.0f;
int NumUpdateTimes = 0;
float AverageUpdateTime = -1.0f; // Invalid value at first


//-----------------------------------------------------------------------------
// Scene management
//-----------------------------------------------------------------------------

// Creates the scene geometry
bool SceneSetup()
{
	//////////////////////////////////////////////
	// Prepare render methods
	InitInput();
	InitialiseMethods();


	//////////////////////////////////////////
	// Create scenery templates and entities

	// Create scenery templates - loads the meshes
	// Template type, template name, mesh name
	EntityManager.CreateTemplate("Scenery", "Skybox", "Skybox.x");
	EntityManager.CreateTemplate("Scenery", "Floor", "Floor.x");
	EntityManager.CreateTemplate("Scenery", "Building", "Building.x");
	EntityManager.CreateTemplate("Scenery", "Tree", "Tree1.x");

	// Creates scenery entities
	// Type (template name), entity name, position, rotation, scale
	EntityManager.CreateEntity("Skybox", "Skybox", CVector3(0.0f, -10000.0f, 0.0f), CVector3::kZero, CVector3(10, 10, 10));
	EntityManager.CreateEntity("Floor", "Floor");
	EntityManager.CreateEntity("Building", "Building", CVector3(0.0f, 0.0f, 40.0f));
	for (int tree = 0; tree < 100; ++tree)
	{
		// Some random trees
		EntityManager.CreateEntity( "Tree", "Tree",
			                        CVector3(Random(-200.0f, 30.0f), 0.0f, Random(40.0f, 150.0f)),
			                        CVector3(0.0f, Random(0.0f, 2.0f * kfPi), 0.0f) );
	}


	/////////////////////////////////
	// Create tank templates

	// Template type, template name, mesh name, top speed, acceleration, tank turn speed, turret
	// turn speed, max HP and shell damage. These latter settings are for advanced requirements only
	EntityManager.CreateTankTemplate("Tank", "Rogue Scout", "HoverTank02.x",
		24.0f, 2.2f, 2.0f, kfPi / 3, 100, 20);
	EntityManager.CreateTankTemplate("Tank", "Oberon MkII", "HoverTank07.x",
		18.0f, 1.6f, 1.3f, kfPi / 4, 120, 35);

	// Template for tank shell
	EntityManager.CreateTemplate("Projectile", "Shell Type 1", "Bullet.x");

	/////////////////////////////
	// Create Patrol Points

	FrontPatrolPoints[0] = CVector3(-70.0f, 0.0f, 35.0f);
	FrontPatrolPoints[1] = CVector3(-50.0f, 0.0f, -10.0f);
	FrontPatrolPoints[2] = CVector3(-30.0f, 0.0f, 20.0f);
	FrontPatrolPoints[3] = CVector3(0.0f, 0.0f, -25.0f);
	FrontPatrolPoints[4] = CVector3(30.0f, 0.0f, 20.0f);
	FrontPatrolPoints[5] = CVector3(50.0f, 0.0f, -10.0f);
	FrontPatrolPoints[6] = CVector3(70.0f, 0.0f, 35.0f);

	BackPatrolPoints[0] = CVector3(70.0f, 0.0f, 45.0f);
	BackPatrolPoints[1] = CVector3(50.0f, 0.0f, 90.0f);
	BackPatrolPoints[2] = CVector3(30.0f, 0.0f, 60.0f);
	BackPatrolPoints[3] = CVector3(0.0f, 0.0f, 105.0f);
	BackPatrolPoints[4] = CVector3(-30.0f, 0.0f, 60.0f);
	BackPatrolPoints[5] = CVector3(-50.0f, 0.0f, 90.0f);
	BackPatrolPoints[6] = CVector3(-70.0f, 0.0f, 45.0f);

	////////////////////////////////
	// Create tank entities

	// Type (template name), team number, tank name, position, rotation
	ATanks[0] = EntityManager.CreateTank("Rogue Scout", 0, "A-1", CVector3(-70.0f, 0.0f, 45.0f),
		CVector3(0.0f, ToRadians(180.0f), 0.0f));
	ATanks[1] = EntityManager.CreateTank("Rogue Scout", 0, "A-2", CVector3(-70.0f, 0.0f, 55.0f),
		CVector3(0.0f, ToRadians(180.0f), 0.0f));
	ATanks[2] = EntityManager.CreateTank("Rogue Scout", 0, "A-3", CVector3(-70.0f, 0.0f, 65.0f),
		CVector3(0.0f, ToRadians(180.0f), 0.0f));
	BTanks[0] = EntityManager.CreateTank("Oberon MkII", 1, "B-1", CVector3(70.0f, 0.0f, 35.0f),
		CVector3(0.0f, ToRadians(0.0f), 0.0f));
	BTanks[1] = EntityManager.CreateTank("Oberon MkII", 1, "B-2", CVector3(70.0f, 0.0f, 25.0f),
		CVector3(0.0f, ToRadians(0.0f), 0.0f));
	BTanks[2] = EntityManager.CreateTank("Oberon MkII", 1, "B-3", CVector3(70.0f, 0.0f, 15.0f),
		CVector3(0.0f, ToRadians(0.0f), 0.0f));

	/////////////////////////////
	// Camera / light setup

	// Set camera position and clip planes
	MainCamera = new CCamera(CVector3(0.0f, 30.0f, -100.0f), CVector3(ToRadians(15.0f), 0, 0));
	MainCamera->SetNearFarClip(1.0f, 20000.0f);

	TankA1Cam = new CCamera(CVector3(-70.0f, 5.0f, 35.0f), CVector3(ToRadians(15.0f), ToRadians(0.0f), 0.0f));
	TankA2Cam = new CCamera(CVector3(-70.0f, 5.0f, 45.0f), CVector3(ToRadians(15.0f), ToRadians(0.0f), 0.0f));
	TankA3Cam = new CCamera(CVector3(-70.0f, 5.0f, 55.0f), CVector3(ToRadians(15.0f), ToRadians(0.0f), 0.0f));

	TankB1Cam = new CCamera(CVector3(70.0f, 5.0f, 45.0f), CVector3(ToRadians(15.0f), ToRadians(0.0f), 0.0f));
	TankB2Cam = new CCamera(CVector3(70.0f, 5.0f, 35.0f), CVector3(ToRadians(15.0f), ToRadians(0.0f), 0.0f));
	TankB3Cam = new CCamera(CVector3(70.0f, 5.0f, 25.0f), CVector3(ToRadians(15.0f), ToRadians(0.0f), 0.0f));

	// Sunlight and light in building
	Lights[0] = new CLight(CVector3(-5000.0f, 4000.0f, -10000.0f), SColourRGBA(1.0f, 0.9f, 0.6f), 15000.0f);
	Lights[1] = new CLight(CVector3(6.0f, 7.5f, 40.0f), SColourRGBA(1.0f, 0.0f, 0.0f), 1.0f);

	// Ambient light level
	AmbientLight = SColourRGBA(0.6f, 0.6f, 0.6f, 1.0f);

	return true;
}


// Release everything in the scene
void SceneShutdown()
{
	// Release render methods
	ReleaseMethods();

	// Release lights
	for (int light = NumLights - 1; light >= 0; --light)
	{
		delete Lights[light];
	}

	// Release cameras
	delete MainCamera;
	delete TankA1Cam;
	delete TankA2Cam;
	delete TankA3Cam;
	delete TankB1Cam;
	delete TankB2Cam;
	delete TankB3Cam;

	// Destroy all entities
	EntityManager.DestroyAllEntities();
	EntityManager.DestroyAllTemplates();
}


//-----------------------------------------------------------------------------
// Game Helper functions
//-----------------------------------------------------------------------------

// Get UID of tank A (team 0) or B (team 1)
TEntityUID GetTankUID(int team, int pos)
{
	if (team == 0) { return ATanks[pos]; }
	else { return BTanks[pos]; }
}

int GetNumTanksPerTeam()
{
	return NumTanksPerTeam;
}

bool PointToSphere(const int radius, const CVector3 currentPos, const CVector3 target)
{
	const float distance = Sqrt((currentPos.x - target.x) * (currentPos.x - target.x) +
		(currentPos.z - target.z) * (currentPos.z - target.z));

	return distance < radius;
}

bool PointToAABB(const int xSize, const int zSize, const CVector3 currentPos, const CVector3 target)
{
	return (currentPos.x >= (target.x - xSize) && currentPos.x <= (target.x + xSize)) &&
		(currentPos.z >= (target.z - zSize) && currentPos.z <= (target.z + zSize));
}

CCamera* GetCamera()
{
	switch (currentCam)
	{
		case Main:
			return MainCamera;
		case TankA1:
			return TankA1Cam;
		case TankA2:
			return TankA2Cam;
		case TankA3:
			return TankA3Cam;
		case TankB1:
			return TankB1Cam;
		case TankB2:
			return TankB2Cam;
		case TankB3:
			return TankB3Cam;
	}
}

void MoveChaseCameras(float updateTime)
{
	CEntity* A1 = EntityManager.GetEntity(ATanks[0]);
	CEntity* A2 = EntityManager.GetEntity(ATanks[1]);
	CEntity* A3 = EntityManager.GetEntity(ATanks[2]);
	CEntity* B1 = EntityManager.GetEntity(BTanks[0]);
	CEntity* B2 = EntityManager.GetEntity(BTanks[1]);
	CEntity* B3 = EntityManager.GetEntity(BTanks[2]);

	if (A1) TankA1Cam->Chase(A1->Matrix());
	if (A2) TankA2Cam->Chase(A2->Matrix());
	if (A3) TankA3Cam->Chase(A3->Matrix());
	if (B1) TankB1Cam->Chase(B1->Matrix());
	if (B2) TankB2Cam->Chase(B2->Matrix());
	if (B3) TankB3Cam->Chase(B3->Matrix());
}


//-----------------------------------------------------------------------------
// Game loop functions
//-----------------------------------------------------------------------------

// Draw one frame of the scene
void RenderScene( float updateTime )
{
	// Setup the viewport - defines which part of the back-buffer we will render to (usually all of it)
	D3D10_VIEWPORT vp;
	vp.Width  = ViewportWidth;
	vp.Height = ViewportHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pd3dDevice->RSSetViewports( 1, &vp );

	// Select the back buffer and depth buffer to use for rendering
	g_pd3dDevice->OMSetRenderTargets( 1, &BackBufferRenderTarget, DepthStencilView );
	
	// Clear previous frame from back buffer and depth buffer
	g_pd3dDevice->ClearRenderTargetView( BackBufferRenderTarget, &AmbientLight.r );
	g_pd3dDevice->ClearDepthStencilView( DepthStencilView, D3D10_CLEAR_DEPTH, 1.0f, 0 );

	// Update camera aspect ratio based on viewport size - for better results when changing window size
	GetCamera()->SetAspect( static_cast<TFloat32>(ViewportWidth) / ViewportHeight );

	// Set camera and light data in shaders
	GetCamera()->CalculateMatrices();
	SetCamera(GetCamera());
	SetAmbientLight(AmbientLight);
	SetLights(&Lights[0]);

	// Render entities and draw on-screen text
	EntityManager.RenderAllEntities();
	RenderSceneText( updateTime );

    // Present the backbuffer contents to the display
	SwapChain->Present( 0, 0 );
}


// Render a single text string at the given position in the given colour, may optionally centre it
void RenderText( const string& text, int X, int Y, float r, float g, float b, bool centre = false )
{
	RECT rect;
	if (!centre)
	{
		SetRect( &rect, X, Y, 0, 0 );
		OSDFont->DrawText( NULL, text.c_str(), -1, &rect, DT_NOCLIP, D3DXCOLOR( r, g, b, 1.0f ) );
	}
	else
	{
		SetRect( &rect, X - 100, Y, X + 100, 0 );
		OSDFont->DrawText( NULL, text.c_str(), -1, &rect, DT_CENTER | DT_NOCLIP, D3DXCOLOR( r, g, b, 1.0f ) );
	}
}

// Render on-screen text each frame
void RenderSceneText( float updateTime )
{
	// Accumulate update times to calculate the average over a given period
	SumUpdateTimes += updateTime;
	++NumUpdateTimes;
	if (SumUpdateTimes >= UpdateTimePeriod)
	{
		AverageUpdateTime = SumUpdateTimes / NumUpdateTimes;
		SumUpdateTimes = 0.0f;
		NumUpdateTimes = 0;
	}

	// Write FPS text string
	stringstream outText;
	if (AverageUpdateTime >= 0.0f)
	{
		outText << "Frame Time: " << AverageUpdateTime * 1000.0f << "ms" << endl << "FPS:" << 1.0f / AverageUpdateTime;
		RenderText( outText.str(), 2, 2, 0.0f, 0.0f, 0.0f);
		RenderText( outText.str(), 0, 0, 1.0f, 1.0f, 0.0f );
		outText.str("");

		mousePos = GetCamera()->WorldPtFromPixel(MouseX, MouseY, ViewportWidth, ViewportHeight);
		outText << mousePos.x << ", " << mousePos.y << ", " << mousePos.z;
		RenderText(outText.str(), 2, 40, 1.0f, 0.0f, 0.0f, false);
		outText.str("");
	}

	for (int i = 0; i < NumTanksPerTeam; i++)
	{
		CEntity* entity = EntityManager.GetEntity(ATanks[i]);

		if (entity)
		{
			CTankEntity* tankEntity = static_cast<CTankEntity*>(entity);
			CVector3 tankPos = tankEntity->Position();
			
			int x;
			int y;

			if (GetCamera()->PixelFromWorldPt(tankPos, ViewportWidth, ViewportHeight, &x, &y))
			{
				const int textSpacer = 10.0f;

				outText << tankEntity->GetName().c_str() << " " << tankEntity->Template()->GetName().c_str() << " ";
				RenderText(outText.str(), x, y + textSpacer, 0.0f, 0.0f, 1.0f, true);
				outText.str("");

				if (entity == selectedTank)
				{
					outText << "SELECTED";
					RenderText(outText.str(), x, y, 1.0f, 0.0f, 0.0f, true);
					outText.str("");
				}

				if (ShowExtraUI)
				{
					switch (tankEntity->GetState())
					{
					case 0:
						outText << "Inactive";
						RenderText(outText.str(), x, y + (2 * textSpacer), 1.0f, 1.0f, 1.0f, true);
						break;

					case 1:
						outText << "Patrol";
						RenderText(outText.str(), x, y + (2 * textSpacer), 0.0f, 1.0f, 0.0f, true);
						break;
					case 2:
						outText << "Aim";
						RenderText(outText.str(), x, y + (2 * textSpacer), 1.0f, 0.0f, 0.0f, true);
						break;
					case 3:
						outText << "Evade";
						RenderText(outText.str(), x, y + (2 * textSpacer), 0.0f, 0.0f, 0.0f, true);
						break;
					}
					outText.str("");

					outText << "HP: " << tankEntity->GetHP();
					RenderText(outText.str(), x, y - (4 * textSpacer), 0.0f, 0.0f, 0.0f, true);
					outText.str("");

					outText << "Fired: " << tankEntity->GetShellsFired();
					RenderText(outText.str(), x, y + (3 * textSpacer), 0.0f, 0.0f, 0.0f, true);
					outText.str("");
				}
			}
		}
	}

	for (int i = 0; i < NumTanksPerTeam; i++)
	{
		CEntity* entity = EntityManager.GetEntity(BTanks[i]);

		if (entity)
		{
			CTankEntity* tankEntity = static_cast<CTankEntity*>(entity);
			CVector3 tankPos = tankEntity->Position();

			int x;
			int y;

			if (GetCamera()->PixelFromWorldPt(tankPos, ViewportWidth, ViewportHeight, &x, &y))
			{
				const int textSpacer = 10.0f;

				outText << tankEntity->GetName().c_str() << " " << tankEntity->Template()->GetName().c_str() << " ";
				RenderText(outText.str(), x, y + textSpacer, 0.0f, 0.0f, 1.0f, true);
				outText.str("");

				if (entity == selectedTank)
				{
					outText << "SELECTED";
					RenderText(outText.str(), x, y, 1.0f, 0.0f, 0.0f, true);
					outText.str("");
				}

				if (ShowExtraUI)
				{
					switch (tankEntity->GetState())
					{
					case 0:
						outText << "Inactive";
						RenderText(outText.str(), x, y + (2 * textSpacer), 1.0f, 1.0f, 1.0f, true);
						break;

					case 1:
						outText << "Patrol";
						RenderText(outText.str(), x, y + (2 * textSpacer), 0.0f, 1.0f, 0.0f, true);
						break;
					case 2:
						outText << "Aim";
						RenderText(outText.str(), x, y + (2 * textSpacer), 1.0f, 0.0f, 0.0f, true);
						break;
					case 3:
						outText << "Evade";
						RenderText(outText.str(), x, y + (2 * textSpacer), 0.0f, 0.0f, 0.0f, true);
						break;
					}
					outText.str("");

					outText << "HP: " << tankEntity->GetHP();
					RenderText(outText.str(), x, y - (4 * textSpacer), 0.0f, 0.0f, 0.0f, true);
					outText.str("");

					outText << "Fired: " << tankEntity->GetShellsFired();
					RenderText(outText.str(), x, y + (3 * textSpacer), 0.0f, 0.0f, 0.0f, true);
					outText.str("");
				}
			}
		}
	}
}


// Update the scene between rendering
void UpdateScene( float updateTime )
{
	// Call all entity update functions
	EntityManager.UpdateAllEntities( updateTime );
	MoveChaseCameras(updateTime);

	// Set camera speeds
	// Key F1 used for full screen toggle
	if (KeyHit(Key_F2)) CameraMoveSpeed = 5.0f;
	if (KeyHit(Key_F3)) CameraMoveSpeed = 40.0f;

	if (KeyHit(Key_1))
	{
		for (int i = 0; i < NumTanksPerTeam; i++)
		{
			CEntity* entity = EntityManager.GetEntity(ATanks[i]);
			if (entity)
			{
				SMessage msg;
				msg.type = Msg_Go;
				msg.from = SystemUID;
				Messenger.SendMessageA(entity->GetUID(), msg);
			}
		}

		for (int i = 0; i < NumTanksPerTeam; i++)
		{
			CEntity* entity = EntityManager.GetEntity(BTanks[i]);
			if (entity)
			{
				SMessage msg;
				msg.type = Msg_Go;
				msg.from = SystemUID;
				Messenger.SendMessageA(entity->GetUID(), msg);
			}
		}
	}

	if (KeyHit(Key_2))
	{
		for (int i = 0; i < NumTanksPerTeam; i++)
		{
			CEntity* entity = EntityManager.GetEntity(ATanks[i]);
			if (entity)
			{
				SMessage msg;
				msg.type = Msg_Stop;
				msg.from = SystemUID;
				Messenger.SendMessageA(entity->GetUID(), msg);
			}
		}

		for (int i = 0; i < NumTanksPerTeam; i++)
		{
			CEntity* entity = EntityManager.GetEntity(BTanks[i]);
			if (entity)
			{
				SMessage msg;
				msg.type = Msg_Stop;
				msg.from = SystemUID;
				Messenger.SendMessageA(entity->GetUID(), msg);
			}
		}
	}

	if (KeyHit(Key_3)) currentCam = Main;
	if (KeyHit(Key_4)) currentCam = TankA1;
	if (KeyHit(Key_5)) currentCam = TankA2;
	if (KeyHit(Key_6)) currentCam = TankA3;
	if (KeyHit(Key_7)) currentCam = TankB1;
	if (KeyHit(Key_8)) currentCam = TankB2;
	if (KeyHit(Key_9)) currentCam = TankB3;

	if (KeyHit(Key_0))
	{
		if (ShowExtraUI) { ShowExtraUI = false; }
		else { ShowExtraUI = true; }
	}

	if (KeyHit(Mouse_LButton))
	{
		for (int i = 0; i < NumTanksPerTeam; i++)
		{
			CEntity* entity = EntityManager.GetEntity(ATanks[i]);

			if (entity)
			{
				int x;
				int y;
				CVector2 mousePos = CVector2(MouseX, MouseY);
				GetCamera()->PixelFromWorldPt(entity->Position(), ViewportWidth, ViewportHeight, &x, &y);
				if (mousePos.DistanceTo(CVector2(x, y)) < 30.0f)
				{
					selectedTank = entity;
				}
			}
		}

		for (int i = 0; i < NumTanksPerTeam; i++)
		{
			CEntity* entity = EntityManager.GetEntity(BTanks[i]);
			if (entity)
			{
				int x;
				int y;
				CVector2 mousePos = CVector2(MouseX, MouseY);
				GetCamera()->PixelFromWorldPt(entity->Position(), ViewportWidth, ViewportHeight, &x, &y);
				if (mousePos.DistanceTo(CVector2(x, y)) < 30.0f)
				{
					selectedTank = entity;
				}
			}
		}
	}

	if (KeyHit(Mouse_RButton))
	{
		CVector3 movePos;
		movePos = GetCamera()->WorldPtFromPixel(MouseX, MouseY, ViewportWidth, ViewportHeight) - GetCamera()->Position();
		movePos.y = 0.0f;
		movePos *= 100;
		mousePos = movePos;
		selectedTank->Matrix().SetPosition(movePos);
	}

	// Move the camera
	if (currentCam == Main)
	{
		MainCamera->Control(Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D,
			CameraMoveSpeed * updateTime, CameraRotSpeed * updateTime);
	}
}


} // namespace gen
