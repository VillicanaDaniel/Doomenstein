#include "Game/Game.hpp"
#include "Game/App.hpp"
#include "Game/Entity.hpp"
#include "Game/Player.hpp"
#include "Game/Prop.hpp"
#include "Game/Actor.hpp"
#include "Game/Map.hpp"
#include "Game/Overworld.hpp"
#include "Game/TileDefinition.hpp"
#include "Game/MapDefinition.hpp"
#include "Game/ActorDefinition.hpp"
#include "Game/WeaponDefinition.hpp"
#include "Game/Weapon.hpp"
#include "Game/PokemonParty.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/ObjUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/SimpleTriangleFont.hpp"
#include "Engine/Audio/AudioSystem.hpp"

Game::Game()
{
	g_engine->m_render->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_engine->m_render->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);	
	g_engine->m_render->SetSamplerMode(SamplerMode::MIRROR);

	m_gameClock = new Clock(g_theApp->m_systemClock);

	LoadDefinitions();
	InitializeCameras();
	InitializeSounds();

	m_attractScreenTexture = g_engine->m_render->CreateOrGetTextureFromFile("Data/Images/AttractScreen.png");

	m_currentMap = nullptr;

	InitializeDevConsole();
}

Game::~Game()
{
	for (Entity* entity : m_entities)
	{
		delete entity;
	}
	m_entities.clear();

	delete m_currentMap;
	m_currentMap = nullptr;

	delete m_worldCamera;
	m_worldCamera = nullptr;

	delete m_attractCamera;
	m_attractCamera = nullptr;

	delete m_screenCamera;
	m_screenCamera = nullptr;

	delete m_gameClock;
	m_gameClock = nullptr;
}

void Game::InitializeDevConsole()
{
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "Type Help for a list of commands");
	g_engine->m_devConsole->AddLine(Rgba8(255, 255, 255, 255), "Controls");
	g_engine->m_devConsole->AddLine(Rgba8(255, 255, 255, 255), "FIRST PERSON");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "Mouse / Right Stick - Look");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "WASD / Left Stick   - Move");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "Shift / A Button    - Sprint");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "LMB / Right Trigger - Fire");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "Left/Right Arrow    - Prev/Next Weapon");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "1 / 2               - Select Weapon");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "F                   - Toggle Camera Mode");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "N                   - Possess Next Actor");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "P                   - Pause");
	g_engine->m_devConsole->AddLine(Rgba8(255, 0, 255, 255), "****NOTE**** - THIS IS TO HELP YOU TEST THE WHOLE PROJECT BUTLER");
	g_engine->m_devConsole->AddLine(Rgba8(0, 0, 255, 255), "K                 - Delete Current Pokemon");
	g_engine->m_devConsole->AddLine(Rgba8(0, 0, 255, 255), "9 / 0                 - Change Sensitivity");


	g_engine->m_devConsole->AddLine(Rgba8(255, 255, 255, 255), "FREE FLY");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "Mouse / Right Stick - Look");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "WASD / Left Stick   - Move");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "Z/C or Shoulders    - Move Up/Down");
	g_engine->m_devConsole->AddLine(Rgba8(0, 255, 255, 255), "Shift / A Button    - Speed Up");
}

void Game::Update()
{
	UpdateLightingFromKeyboard();

	if (m_gamemode == GAMEMODE_ATTRACT)
	{
		UpdateFromKeyboard();
		return;
	}

	if (m_gamemode == GAMEMODE_LOBBY)
	{
		UpdateLobby();
		return;
	}

	if (m_gamemode == GAMEMODE_PLAYING)
	{
		UpdateDialogue();

		if (!IsDialogueActive())
		{
			DebugInputHandling();

			if (m_currentMap != nullptr)
			{
				m_currentMap->Update();
			}
		}

		UpdateFromKeyboard();
		KillCurrentPokemon();
	}
}

void Game::UpdateLobby()
{
	InputSystem* input = g_engine->m_input;
	XboxController const& controller = input->GetController(0);

	if (input->WasKeyJustPressed(KEYCODE_SPACE))
	{
		if (!m_keyboardPlayerJoined)
		{
			m_keyboardPlayerJoined = true;
			g_engine->m_audio->StartSound(m_buttonClickSound, false, 1.f);
		}
		else
		{
			StartGameFromLobby();
		}
	}

	if (controller.WasButtonJustPressed(XboxButtonID::START))
	{
		if (!m_controllerPlayerJoined)
		{
			m_controllerPlayerJoined = true;
			g_engine->m_audio->StartSound(m_buttonClickSound, false, 1.f);
		}
		else
		{
			StartGameFromLobby();
		}
	}

	if (input->WasKeyJustPressed(KEYCODE_ESC))
	{
		if (m_keyboardPlayerJoined)
		{
			m_keyboardPlayerJoined = false;
			g_engine->m_audio->StartSound(m_buttonClickSound, false, 1.f);
		}
		else
		{
			m_gamemode = GAMEMODE_ATTRACT;
		}
	}

	if (controller.WasButtonJustPressed(XboxButtonID::B))
	{
		if (m_controllerPlayerJoined)
		{
			m_controllerPlayerJoined = false;
			g_engine->m_audio->StartSound(m_buttonClickSound, false, 1.f);
		}
		else
		{
			m_gamemode = GAMEMODE_ATTRACT;
		}
	}
}

void Game::StartGameFromLobby()
{
	if (!m_keyboardPlayerJoined && !m_controllerPlayerJoined)
	{
		return;
	}

	if (m_currentMap != nullptr)
	{
		delete m_currentMap;
		m_currentMap = nullptr;
	}

	MapDefinition const* mapDef = MapDefinition::GetByName("EmptyMap");
	m_currentMap = new Overworld(this, mapDef);

	if (m_currentMap != nullptr)
	{
		m_currentMap->ConfigurePlayersFromLobby(
			m_keyboardPlayerJoined,
			m_controllerPlayerJoined
		);
	}

	PlayNewMusic(m_overworldMusic);
	m_gamemode = GAMEMODE_PLAYING;
}

void Game::Render() const
{
	if (m_gamemode == GAMEMODE_ATTRACT)
	{
		RenderAttract();
		return;
	}

	if (m_gamemode == GAMEMODE_LOBBY)
	{
		RenderLobby();
		return;
	}

	RenderPlaying();
}

void Game::RenderAttract() const
{
	BitmapFont* font = g_engine->m_render->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");
	g_engine->m_render->BeginCamera(*m_screenCamera);
	g_engine->m_render->ClearScreen(Rgba8(120, 120, 120, 250));

	IntVec2 dims = g_engine->m_window->GetClientDimensions();

	AABB2 screenBounds(
		Vec2(0.f, 0.f),
		Vec2((float)dims.x, (float)dims.y)
	);

	std::vector<Vertex> verts;
	AddVertsForAABB2(verts, screenBounds, Rgba8::WHITE);

	g_engine->m_render->SetBlendMode(BlendMode::ALPHA);
	g_engine->m_render->BindTexture(m_attractScreenTexture);
	g_engine->m_render->DrawVertexArray((int)verts.size(), verts.data());


	std::vector<Vertex> titleVerts;
	float centerY = (float)dims.y * 0.55f;

	AABB2 textBox(
		Vec2(0.f, centerY - 40.f),
		Vec2((float)dims.x, centerY + 40.f)
	);

	g_engine->m_render->BindTexture(&font->GetTexture());
	g_engine->m_render->DrawVertexArray(titleVerts);

	g_engine->m_render->BindTexture(nullptr);

	Rgba8 white = Rgba8(255, 255, 255, 255);
	Rgba8 green = Rgba8(0, 255, 0, 255);
	Rgba8 yellow = Rgba8(255, 255, 0, 255);

	DrawScreenText("Press ESC to exit", 25.f, white);
	DrawScreenText("Press SPACE to join with keyboard or START to join with controller", 75.f, white);

	g_engine->m_render->EndCamera(*m_screenCamera);
}

void Game::RenderLobby() const
{
	g_engine->m_render->BeginCamera(*m_screenCamera);
	g_engine->m_render->ClearScreen(Rgba8(60, 60, 60, 255));
	g_engine->m_render->SetBlendMode(BlendMode::ALPHA);

	IntVec2 dims = g_engine->m_window->GetClientDimensions();
	float centerY = (float)dims.y * 0.55f;

	Rgba8 white = Rgba8(255, 255, 255, 255);
	Rgba8 green = Rgba8(0, 255, 0, 255);
	Rgba8 yellow = Rgba8(255, 255, 0, 255);

	DrawScreenText("LOBBY", centerY + 120.f, yellow);

	if (m_keyboardPlayerJoined)
	{
		DrawScreenText("Mouse and Keyboard: JOINED", centerY + 50.f, green);
	}
	else
	{
		DrawScreenText("Press SPACE to join with mouse and keyboard", centerY + 50.f, white);
	}

	if (m_controllerPlayerJoined)
	{
		DrawScreenText("Controller: JOINED", centerY, green);
	}
	else
	{
		DrawScreenText("Press START to join with controller", centerY, white);
	}

	DrawScreenText("Press SPACE or START again to begin", centerY - 80.f, white);
	DrawScreenText("Press ESCAPE or BACK to leave", centerY - 130.f, white);

	g_engine->m_render->EndCamera(*m_screenCamera);
}

void Game::RenderPlaying() const
{
	g_engine->m_render->ClearScreen(Rgba8(120, 180, 210, 250));

	if (m_currentMap == nullptr)
	{
		return;
	}
	
	Vec3 normalizedSunDir = m_sunDirection.GetNormalized();

	g_engine->m_render->SetLightConstants(
		normalizedSunDir,
		GetClamped(m_sunIntensity, 0.f, 1.f),
		GetClamped(m_ambientIntensity, 0.f, 1.f)
	);

	for (Player* player : m_currentMap->m_players)
	{
		if (player == nullptr)
		{
			continue;
		}

		Actor* actor = player->GetActor();

		g_engine->m_render->BeginCamera(*player->m_worldCamera, player->m_viewport);

		Actor* actorToHide = player->GetActor();

		if (dynamic_cast<Overworld*>(m_currentMap) != nullptr)
		{
			actorToHide = nullptr;
		}

		m_currentMap->Render(*player->m_worldCamera, actorToHide);

		g_engine->m_render->SetModelConstants();
		DebugRenderWorld(*player->m_worldCamera);
		g_engine->m_render->EndCamera(*player->m_worldCamera);

		g_engine->m_render->BeginCamera(*player->m_screenCamera, player->m_viewport);
		g_engine->m_render->SetBlendMode(BlendMode::ALPHA);
		g_engine->m_render->SetDepthMode(DepthMode::DISABLED);
		g_engine->m_render->SetLightConstants(Vec3(0.f, 0.f, -1.f), 0.f, 1.f);

		if (actor != nullptr && !player->m_isFreeFlyMode)
		{
			Weapon* weapon = actor->GetEquippedWeapon();
			if (weapon != nullptr)
			{
				IntVec2 dims = g_engine->m_window->GetClientDimensions();

				AABB2 screenBounds(
					Vec2(0.f, 0.f),
					Vec2((float)dims.x, (float)dims.y)
				);

				weapon->RenderHUD(screenBounds);
				player->RenderHUDText(screenBounds);
				player->RenderDeathOverlay(screenBounds);
			}
		}

		Overworld const* overworld = dynamic_cast<Overworld const*>(m_currentMap);
		if (overworld != nullptr)
		{
			overworld->RenderSnow(*player->m_screenCamera);
		}

		RenderPokemonHealthBar();
		RenderDialogue();
	}

	g_engine->m_render->BeginCamera(*m_screenCamera);
	g_engine->m_render->SetBlendMode(BlendMode::ALPHA);
	g_engine->m_render->SetDepthMode(DepthMode::DISABLED);
	g_engine->m_render->SetLightConstants(Vec3(0.f, 0.f, -1.f), 0.f, 1.f);

	if (g_engine->m_devConsole->IsOpen())
	{
		AABB2 consoleBounds(
			Vec2(0.f, 0.f),
			Vec2((float)g_engine->m_window->GetClientDimensions().x,
				(float)g_engine->m_window->GetClientDimensions().y)
		);

		g_engine->m_devConsole->Render(consoleBounds);
	}

	DebugRenderScreen(*m_screenCamera);

	g_engine->m_render->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_engine->m_render->EndCamera(*m_screenCamera);
}

void Game::DrawScreenText(std::string const& text, float y, Rgba8 const& color) const
{
	BitmapFont* font = g_engine->m_render->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");
	if (font == nullptr)
	{
		return;
	}

	IntVec2 dims = g_engine->m_window->GetClientDimensions();

	std::vector<Vertex> verts;

	AABB2 textBox(
		Vec2(0.f, y - 20.f),
		Vec2((float)dims.x, y + 20.f)
	);

	font->AddVertsForTextInBox2D(
		verts,
		text,
		textBox,
		32.f,
		color,
		0.7f,
		Vec2(0.5f, 0.5f)
	);

	g_engine->m_render->BindTexture(&font->GetTexture());
	g_engine->m_render->DrawVertexArray(verts);
}

void Game::RenderBillboardedText(std::string const& text, Vec3 const& worldPosition, float cellHeight, Rgba8 const& tint) const
{
	BitmapFont* font = g_engine->m_render->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");
	std::vector<Vertex> textVerts;
	font->AddVertsForText3DAtOriginXForward(
		textVerts,
		cellHeight,
		text,
		tint,
		1.0f,
		Vec2(0.5f, 0.5f),
		999
	);

	Mat44 billboardTransform = GetBillboardTransform(
		BillboardType::WORLD_UP_FACING,
		m_worldCamera->GetCameraToWorldTransform(),
		worldPosition,
		Vec2(1.f, 1.f)
	);

	TransformVertexArray3D(textVerts, billboardTransform);

	g_engine->m_render->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_engine->m_render->BindTexture(&font->GetTexture());
	g_engine->m_render->DrawVertexArray((int)textVerts.size(), textVerts.data());
	g_engine->m_render->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
}

void Game::StartTrainerBattle()
{
	if (m_currentMap != nullptr)
	{
		delete m_currentMap;
		m_currentMap = nullptr;
	}

	MapDefinition const* mapDef = MapDefinition::GetByName("TestMap");
	GUARANTEE_OR_DIE(mapDef != nullptr, "Could not find TestMap MapDefinition");

	m_currentMap = new Map(this, mapDef);

	if (m_currentMap != nullptr)
	{
		PlayNewMusic(m_battleMusic);
		m_currentMap->ConfigurePlayersFromLobby(
			m_keyboardPlayerJoined,
			m_controllerPlayerJoined
		);
	}
}

void Game::KillCurrentPokemon()
{
	if (g_engine->m_input->WasKeyJustPressed('K'))
	{
		if (m_currentMap != nullptr &&
			m_currentMap->m_squirrelParty != nullptr)
		{
			Prop* pokemon = m_currentMap->m_squirrelParty->GetActivePokemon();

			if (pokemon != nullptr && !pokemon->m_isDead)
			{
				pokemon->Damage(99999.f, ActorHandle());
			}
		}
	}
}

void Game::LoadDefinitions()
{
	TileDefinition::InitializeTileDefs("Data/Definitions/TileDefinitions.xml");
	MapDefinition::InitializeMapDefs("Data/Definitions/MapDefinitions.xml");
	WeaponDefinition::InitializeWeaponDefs("Data/Definitions/WeaponDefinitions.xml");

	ActorDefinition::ClearDefinitions();
	ActorDefinition::LoadActorDefsFromXml("Data/Definitions/ProjectileActorDefinitions.xml");
	ActorDefinition::LoadActorDefsFromXml("Data/Definitions/ActorDefinitions.xml");
}

void Game::InitializeCameras()
{
	m_worldCamera = new Camera();
	float aspect =
		(float)g_engine->m_window->GetClientDimensions().x /
		(float)g_engine->m_window->GetClientDimensions().y;

	m_worldCamera->SetPerspectiveView(
		aspect,
		60.0f,
		0.1f,
		300.0f
	);

	m_worldCamera->SetPositionAndOrientation(
		Vec3(0.f, 0.f, 0.f),
		EulerAngles(0.f, 0.f, 0.f)
	);

	m_attractCamera = new Camera();

	m_attractCamera->SetPerspectiveView(
		aspect,
		60.0f,
		0.1f,
		100.0f
	);

	m_attractCamera->SetOrthographicView(Vec2(-2.f, -1.f), Vec2(2.f, 1.f));
	m_attractCamera->SetPositionAndOrientation(Vec3(0.f, 0.5f, 0.5f), EulerAngles(0.f, 180.f, 0.f));

	Mat44 cameraToRender = Mat44(Vec3(0, 0, 1), Vec3(-1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 0));
	m_worldCamera->SetCameraToRenderTransform(cameraToRender);

	m_screenCamera = new Camera();
	m_screenCamera->SetOrthographicView(
		Vec2(0.f, 0.f),
		Vec2((float)g_engine->m_window->GetClientDimensions().x,
			(float)g_engine->m_window->GetClientDimensions().y)
	);
}

void Game::InitializeSounds()
{
	m_attractMusic = g_engine->m_audio->CreateOrGetSound("Data/Audio/Music/titleTheme.mp3", false);
	m_gameMusic = g_engine->m_audio->CreateOrGetSound("Data/Audio/Music/E1M1_AtDoomsGate.mp2", false);
	m_buttonClickSound = g_engine->m_audio->CreateOrGetSound("Data/Audio/Click.mp3", false);
	m_overworldMusic = g_engine->m_audio->CreateOrGetSound("Data/Audio/Music/Blizzard.mp3", false);
	m_battleMusic = g_engine->m_audio->CreateOrGetSound("Data/Audio/Music/BattleTheme.mp3", false);
	m_victoryMusic = g_engine->m_audio->CreateOrGetSound("Data/Audio/Music/Victory.mp3", false);
	m_lastPokemonMusic = g_engine->m_audio->CreateOrGetSound("Data/Audio/Music/lastPokemon.mp3", false);
	m_titleBoom = g_engine->m_audio->CreateOrGetSound("Data/Audio/undertale.mp3", false);
	m_teleportSound = g_engine->m_audio->CreateOrGetSound("Data/Audio/teleport.mp3", false);

	m_currentMusic = m_attractMusic;
	m_currentMusicPlayback = g_engine->m_audio->StartSound(m_attractMusic, true, 0.5f);
}

void Game::DeleteGarbage()
{
}

void Game::UpdateFromKeyboard()
{
	InputSystem* input = g_engine->m_input;

	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_SPACE) && m_gamemode == GAMEMODE_ATTRACT)
	{
		m_gamemode = GAMEMODE_LOBBY;

		m_keyboardPlayerJoined = true;
		m_controllerPlayerJoined = false;

		if (m_buttonClickSound != MISSING_SOUND_ID)
		{
			g_engine->m_audio->StartSound(m_buttonClickSound, false, 1.f);
		}
	}

	if (g_engine->m_input->GetController(0).WasButtonJustPressed(XboxButtonID::START) && m_gamemode == GAMEMODE_ATTRACT)
	{
		m_gamemode = GAMEMODE_LOBBY;

		m_keyboardPlayerJoined = false;
		m_controllerPlayerJoined = true;

		if (m_buttonClickSound != MISSING_SOUND_ID)
		{
			g_engine->m_audio->StartSound(m_buttonClickSound, false, 1.f);
		}
	}

	if (input->WasKeyJustPressed(KEYCODE_ESC) && m_gamemode == GAMEMODE_PLAYING)
	{
		if (m_currentMusic != m_attractMusic)
		{
			if (m_currentMusicPlayback != MISSING_SOUND_ID)
			{
				g_engine->m_audio->StopSound(m_currentMusicPlayback);
			}

			m_currentMusic = m_attractMusic;
			m_currentMusicPlayback = g_engine->m_audio->StartSound(m_attractMusic, true, 0.5f);
		}
		g_engine->m_audio->StartSound(m_buttonClickSound, false, 1.f);

		m_gamemode = GAMEMODE_ATTRACT;
	}

	if (g_engine->m_input->WasKeyJustPressed(KEYCODE_LEFT_MOUSE))
	{
		if (m_currentMap != nullptr && m_currentMap->m_player != nullptr)
		{
			Vec3 rayStart = m_worldCamera->GetPosition();
			Vec3 rayDir = m_worldCamera->GetCameraToWorldTransform().GetIBasis3D().GetNormalized();

			Actor* playerActor = m_currentMap->m_player->GetActor();

			m_currentMap->RaycastAll(rayStart, rayDir, 10.f, playerActor);
		}
	}
}

void Game::UpdateFromController()
{
}

bool Game::IsInAttractMode()
{
	return m_gamemode == GAMEMODE_ATTRACT;
}

void Game::BuildUnitCubeVerts(Prop& cube)
{
	cube.m_vertexes.clear();

	float s = 0.5f;

	Vec3 p000 = Vec3(-s, -s, -s);
	Vec3 p001 = Vec3(-s, -s, s);
	Vec3 p010 = Vec3(-s, s, -s);
	Vec3 p011 = Vec3(-s, s, s);
	Vec3 p100 = Vec3(s, -s, -s);
	Vec3 p101 = Vec3(s, -s, s);
	Vec3 p110 = Vec3(s, s, -s);
	Vec3 p111 = Vec3(s, s, s);

	AddVertsForQuad3D(cube.m_vertexes, p000, p010, p011, p001, Rgba8(0, 255, 255));
	AddVertsForQuad3D(cube.m_vertexes, p101, p111, p110, p100, Rgba8(255, 0, 0));
	AddVertsForQuad3D(cube.m_vertexes, p011, p010, p110, p111, Rgba8(0, 255, 0));
	AddVertsForQuad3D(cube.m_vertexes, p000, p001, p101, p100, Rgba8(255, 0, 255));
	AddVertsForQuad3D(cube.m_vertexes, p001, p011, p111, p101, Rgba8(0, 0, 255));
	AddVertsForQuad3D(cube.m_vertexes, p100, p110, p010, p000, Rgba8(255, 255, 0));
}

void Game::UpdateLightingFromKeyboard()
{
	InputSystem* input = g_engine->m_input;

	if (input->WasKeyJustPressed(KEYCODE_F2))
	{
		m_sunDirection.x -= 1.f;
		DebugAddMessage(Stringf("Sun X: %.2f", m_sunDirection.x), 2.f, Rgba8::WHITE, Rgba8::WHITE);
	}

	if (input->WasKeyJustPressed(KEYCODE_F3))
	{
		m_sunDirection.x += 1.f;
		DebugAddMessage(Stringf("Sun X: %.2f", m_sunDirection.x), 2.f, Rgba8::WHITE, Rgba8::WHITE);
	}

	if (input->WasKeyJustPressed(KEYCODE_F4))
	{
		m_sunDirection.y -= 1.f;
		DebugAddMessage(Stringf("Sun Y: %.2f", m_sunDirection.y), 2.f, Rgba8::WHITE, Rgba8::WHITE);
	}

	if (input->WasKeyJustPressed(KEYCODE_F5))
	{
		m_sunDirection.y += 1.f;
		DebugAddMessage(Stringf("Sun Y: %.2f", m_sunDirection.y), 2.f, Rgba8::WHITE, Rgba8::WHITE);
	}

	if (input->WasKeyJustPressed(KEYCODE_F6))
	{
		m_sunIntensity -= 0.05f;
		m_sunIntensity = GetClamped(m_sunIntensity, 0.f, 1.f);
		DebugAddMessage(Stringf("Sun Intensity: %.2f", m_sunIntensity), 2.f, Rgba8::WHITE, Rgba8::WHITE);
	}

	if (input->WasKeyJustPressed(KEYCODE_F7))
	{
		m_sunIntensity += 0.05f;
		m_sunIntensity = GetClamped(m_sunIntensity, 0.f, 1.f);
		DebugAddMessage(Stringf("Sun Intensity: %.2f", m_sunIntensity), 2.f, Rgba8::WHITE, Rgba8::WHITE);
	}

	if (input->WasKeyJustPressed(KEYCODE_F8))
	{
		m_ambientIntensity -= 0.05f;
		m_ambientIntensity = GetClamped(m_ambientIntensity, 0.f, 1.f);
		DebugAddMessage(Stringf("Ambient Intensity: %.2f", m_ambientIntensity), 2.f, Rgba8::WHITE, Rgba8::WHITE);
	}

	if (input->WasKeyJustPressed(KEYCODE_F9))
	{
		m_ambientIntensity += 0.05f;
		m_ambientIntensity = GetClamped(m_ambientIntensity, 0.f, 1.f);
		DebugAddMessage(Stringf("Ambient Intensity: %.2f", m_ambientIntensity), 2.f, Rgba8::WHITE, Rgba8::WHITE);
	}

	m_sunDirection.z = -1.f;
}

void Game::DebugInputHandling()
{
	InputSystem* input = g_engine->m_input;

	Actor* playerActor = m_currentMap->m_player->GetActor();

	if (m_currentMap->m_player && input->WasKeyJustPressed('1'))
	{
// 		Mat44 playerMatrix = playerActor->GetModelToWorldTransform();
// 		Vec3 start = playerActor->m_position;
// 		Vec3 forward = playerMatrix.GetIBasis3D().GetNormalized();
// 		Vec3 end = start + forward * 20.f;
// 
// 		DebugAddWorldArrow(
// 			start,
// 			end,
// 			0.0625f,
// 			10.f,
// 			Rgba8(255, 255, 0, 255),
// 			Rgba8(255, 255, 0, 255),
// 			DebugRenderMode::X_RAY
// 		);
		DebugAddMessage(
			Stringf("Weapons: %i  Equipped: %i",
				(int)playerActor->m_weapons.size(),
				playerActor->m_equippedWeaponIndex),
			1.f,
			Rgba8::WHITE,
			Rgba8::WHITE
		);
	}
// 
// 	if (m_currentMap->m_player && input->IsKeyDown('2'))
// 	{
// 		Vec3 spherePos = playerActor->m_position;
// 		spherePos.z = 0.f;
// 
// 		DebugAddWorldSphere(
// 			spherePos,
// 			0.5f,
// 			60.f,
// 			Rgba8(150, 75, 0, 255),
// 			Rgba8(150, 75, 0, 255),
// 			DebugRenderMode::USE_DEPTH
// 		);
// 	}
// 
// 	if (m_currentMap->m_player && input->WasKeyJustPressed('3'))
// 	{
// 		Mat44 playerMat = playerActor->GetModelToWorldTransform();
// 		Vec3 forward = playerMat.GetIBasis3D().GetNormalized();
// 		Vec3 center = playerActor->m_position + forward * 2.f;
// 
// 		DebugAddWorldWireSphere(
// 			center,
// 			1.f,
// 			5.f,
// 			Rgba8(0, 255, 0, 255),
// 			Rgba8(255, 0, 0, 255),
// 			DebugRenderMode::USE_DEPTH
// 		);
// 	}
// 
// 	if (m_currentMap->m_player && input->WasKeyJustPressed('4'))
// 	{
// 		DebugAddBasis(
// 			playerActor->GetModelToWorldTransform(),
// 			20.f,
// 			1.f,
// 			0.05f,
// 			1.f,
// 			1.f,
// 			DebugRenderMode::USE_DEPTH
// 		);
// 	}
// 
// 	if (m_currentMap->m_player && input->WasKeyJustPressed('5'))
// 	{
// 		std::string text = Stringf(
// 			"Pos:(%.2f, %.2f, %.2f)\nOri:(%.2f, %.2f, %.2f)",
// 			playerActor->m_position.x,
// 			playerActor->m_position.y,
// 			playerActor->m_position.z,
// 			playerActor->m_orientation.m_yawDegrees,
// 			playerActor->m_orientation.m_pitchDegrees,
// 			playerActor->m_orientation.m_rollDegrees
// 		);
// 
// 		Mat44 playerMat = playerActor->GetModelToWorldTransform();
// 		Vec3 forward = playerMat.GetIBasis3D().GetNormalized();
// 
// 		Vec3 textPos = playerActor->m_position + forward * 1.5f;
// 
// 		DebugAddWorldBillboardText(
// 			text,
// 			textPos,
// 			0.125f,
// 			Vec2(0.5f, 0.5f),
// 			10.f,
// 			Rgba8(255, 255, 255, 255),
// 			Rgba8(255, 0, 0, 255),
// 			DebugRenderMode::USE_DEPTH
// 		);
// 	}
// 
// 	if (m_currentMap->m_player && input->WasKeyJustPressed('6'))
// 	{
// 		Vec3 start = playerActor->m_position;
// 		Vec3 end = start + Vec3(0.f, 0.f, 1.f);
// 
// 		DebugAddWorldWireCylinder(
// 			start,
// 			end,
// 			0.5f,
// 			10.f,
// 			Rgba8(255, 255, 255, 255),
// 			Rgba8(255, 0, 0, 255),
// 			DebugRenderMode::USE_DEPTH
// 		);
// 	}
// 
// 	if (input->WasKeyJustPressed('7'))
// 	{
// 		Vec3 camForward = m_worldCamera->GetCameraToWorldTransform().GetIBasis3D();
// 		Vec3 camLeft = m_worldCamera->GetCameraToWorldTransform().GetJBasis3D();
// 		Vec3 camUp = m_worldCamera->GetCameraToWorldTransform().GetKBasis3D();
// 
// 		std::string msg = Stringf(
// 			"Cam I:(%.2f, %.2f, %.2f) J:(%.2f, %.2f, %.2f) K:(%.2f, %.2f, %.2f)",
// 			camForward.x, camForward.y, camForward.z,
// 			camLeft.x, camLeft.y, camLeft.z,
// 			camUp.x, camUp.y, camUp.z
// 		);
// 
// 		DebugAddMessage(
// 			msg,
// 			5.f,
// 			Rgba8(255, 255, 255, 255),
// 			Rgba8(255, 255, 255, 255)
// 		);
// 	}
}

Clock* Game::GetClock() const
{
	return m_gameClock;
}

void Game::StartDialogue(DialogueRequest const& request)
{
	m_activeDialogue = request;
	m_dialogueLineIndex = 0;
	m_dialogueTimer = 0.f;
	m_transitionTimer = 0.f;
	m_dialogueState = DIALOGUE_WAITING_FOR_CONFIRM;
}

bool Game::IsDialogueActive() const
{
	return m_dialogueState != DIALOGUE_NONE;
}

void Game::RenderPokemonHealthBar() const
{
	if (m_currentMap == nullptr)
	{
		return;
	}

	Prop* activePokemon = m_currentMap->GetActivePokemon();
	if (activePokemon == nullptr || activePokemon->m_isDead)
	{
		return;
	}

	IntVec2 dims = g_engine->m_window->GetClientDimensions();

	float healthFraction = activePokemon->m_health / activePokemon->m_maxHealth;
	healthFraction = GetClamped(healthFraction, 0.f, 1.f);

	float baseHealth = 100.f;
	float baseBarWidth = 420.f;

	float healthScale = activePokemon->m_maxHealth / baseHealth;
	healthScale = GetClamped(healthScale, 0.25f, 2.5f);

	float barWidth = baseBarWidth * healthScale;
	float barHeight = 40.f;
	float centerX = (float)dims.x * 0.5f;
	float topY = (float)dims.y - 50.f;

	AABB2 backBar(
		Vec2(centerX - barWidth * 0.5f, topY - barHeight),
		Vec2(centerX + barWidth * 0.5f, topY)
	);

	AABB2 healthBar = backBar;
	healthBar.m_maxs.x = backBar.m_mins.x + backBar.GetDimensions().x * healthFraction;

	std::vector<Vertex> verts;
	AddVertsForAABB2(verts, backBar, Rgba8(30, 30, 30, 220));
	AddVertsForAABB2(verts, healthBar, Rgba8(250, 0, 60, 255));

	g_engine->m_render->BindTexture(nullptr);
	g_engine->m_render->DrawVertexArray(verts);

	BitmapFont* font = g_engine->m_render->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");

	std::vector<Vertex> textVerts;

	AABB2 nameBox(
		Vec2(backBar.m_mins.x - 220.f, backBar.m_mins.y - 4.f),
		Vec2(backBar.m_mins.x - 20.f, backBar.m_maxs.y + 4.f)
	);

	font->AddVertsForTextInBox2D(
		textVerts,
		activePokemon->m_displayName,
		nameBox,
		28.f,
		Rgba8::WHITE,
		0.7f,
		Vec2(1.f, 0.5f)
	);

	g_engine->m_render->BindTexture(&font->GetTexture());
	g_engine->m_render->DrawVertexArray(textVerts);
}

void Game::PlayNewMusic(SoundID music)
{
	if (m_currentMusic == music)
	{
		return;
	}

	if (m_currentMusicPlayback != MISSING_SOUND_ID)
	{
		g_engine->m_audio->StopSound(m_currentMusicPlayback);
	}

	m_currentMusic = music;
	m_currentMusicPlayback = g_engine->m_audio->StartSound(music, true, 0.5f);
}

void Game::UpdateDialogue()
{
	if (m_dialogueState == DIALOGUE_NONE)
	{
		return;
	}

	InputSystem* input = g_engine->m_input;

	if (m_dialogueState == DIALOGUE_WAITING_FOR_CONFIRM)
	{
		if (input->WasKeyJustPressed(KEYCODE_SPACE))
		{
			++m_dialogueLineIndex;

			if (m_dialogueLineIndex >= (int)m_activeDialogue.m_lines.size())
			{
				if (m_activeDialogue.m_changeMapAfterDialogue)
				{
					m_dialogueState = DIALOGUE_TRANSITION;
					m_transitionTimer = 0.f;
				}
				else
				{
					m_dialogueState = DIALOGUE_NONE;
				}
			}
		}
	}
	else if (m_dialogueState == DIALOGUE_TRANSITION)
	{
		float dt = (float)m_gameClock->GetDeltaSeconds();
		m_transitionTimer += dt;

		if (m_transitionTimer >= 1.f)
		{
			std::string mapName = m_activeDialogue.m_nextMapName;
			m_dialogueState = DIALOGUE_NONE;

			StartTrainerBattle();
		}
	}
}

void Game::RenderDialogue() const
{
	if (m_dialogueState == DIALOGUE_NONE)
	{
		return;
	}

	IntVec2 dims = g_engine->m_window->GetClientDimensions();

	AABB2 box(
		Vec2(80.f, 60.f),
		Vec2((float)dims.x - 80.f, 220.f)
	);

	std::vector<Vertex> verts;
	AddVertsForAABB2(verts, box, Rgba8(0, 0, 0, 220));
	g_engine->m_render->BindTexture(nullptr);
	g_engine->m_render->DrawVertexArray(verts);

	if (m_dialogueLineIndex < (int)m_activeDialogue.m_lines.size())
	{
		BitmapFont* font = g_engine->m_render->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");

		std::vector<Vertex> textVerts;
		font->AddVertsForTextInBox2D(
			textVerts,
			m_activeDialogue.m_lines[m_dialogueLineIndex],
			box,
			28.f,
			Rgba8::WHITE,
			0.7f,
			Vec2(0.05f, 0.5f)
		);

		g_engine->m_render->BindTexture(&font->GetTexture());
		g_engine->m_render->DrawVertexArray(textVerts);
	}

	if (m_dialogueState == DIALOGUE_TRANSITION)
	{
		float alpha = GetClamped(m_transitionTimer / 1.f, 0.f, 1.f);

		std::vector<Vertex> fadeVerts;
		AddVertsForAABB2(
			fadeVerts,
			AABB2(Vec2(0.f, 0.f), Vec2((float)dims.x, (float)dims.y)),
			Rgba8(0, 0, 0, (unsigned char)(alpha * 255.f))
		);

		g_engine->m_render->BindTexture(nullptr);
		g_engine->m_render->DrawVertexArray(fadeVerts);
	}
}
