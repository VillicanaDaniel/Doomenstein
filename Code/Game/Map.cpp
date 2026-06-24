#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/Game.hpp"
#include "Game/Actor.hpp"
#include "Game/Tile.hpp"
#include "Game/TileDefinition.hpp"
#include "Game/MapDefinition.hpp"
#include "Game/Prop.hpp"
#include "Game/ActorHandle.hpp"
#include "Game/Weapon.hpp"
#include "Game/WeaponDefinition.hpp"
#include "Game/ActorDefinition.hpp"
#include "Game/AIController.hpp"
#include "Game/PokemonParty.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/ObjUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Math/MathUtils.hpp"

static void AppendIndexedQuad(
	std::vector<Vertex_TBN>& verts,
	std::vector<unsigned int>& indexes,
	Vec3 const& bl,
	Vec3 const& br,
	Vec3 const& tr,
	Vec3 const& tl,
	Rgba8 const& color,
	AABB2 const& uvs)
{
	unsigned int startIndex = (unsigned int)verts.size();

	Vec3 tangent = (br - bl).GetNormalized();
	Vec3 bitangent = (tl - bl).GetNormalized();
	Vec3 normal = CrossProduct3D(tangent, bitangent).GetNormalized();

	Vertex_TBN v0(bl, color, Vec2(uvs.m_mins.x, uvs.m_mins.y), tangent, bitangent, normal);
	Vertex_TBN v1(br, color, Vec2(uvs.m_maxs.x, uvs.m_mins.y), tangent, bitangent, normal);
	Vertex_TBN v2(tr, color, Vec2(uvs.m_maxs.x, uvs.m_maxs.y), tangent, bitangent, normal);
	Vertex_TBN v3(tl, color, Vec2(uvs.m_mins.x, uvs.m_maxs.y), tangent, bitangent, normal);

	verts.push_back(v0);
	verts.push_back(v1);
	verts.push_back(v2);
	verts.push_back(v3);

	indexes.push_back(startIndex + 0);
	indexes.push_back(startIndex + 1);
	indexes.push_back(startIndex + 2);
	indexes.push_back(startIndex + 0);
	indexes.push_back(startIndex + 2);
	indexes.push_back(startIndex + 3);
}

static RaycastResult3D GetCloserHit(RaycastResult3D const& a, RaycastResult3D const& b)
{
	if (!a.m_didImpact) return b;
	if (!b.m_didImpact) return a;
	return (a.m_impactDistance <= b.m_impactDistance) ? a : b;
}

static void AddSubmeshToProp(Prop* prop, std::map<std::string, std::vector<Vertex>> const& vertsByMtl, char const* mtlName, char const* texPath)
{
	auto it = vertsByMtl.find(mtlName);
	if (it == vertsByMtl.end() || it->second.empty())
	{
		return;
	}

	PropSubmesh sm;
	sm.m_verts = it->second;
	sm.m_texture = g_engine->m_render->CreateOrGetTextureFromFile(texPath);
	prop->m_submeshes.push_back(std::move(sm));
}

Map::Map(Game* game, MapDefinition const* definition)
	: m_game(game)
	, m_definition(definition)
{
	GUARANTEE_OR_DIE(m_game != nullptr, "Map::Map - game was null");
	GUARANTEE_OR_DIE(m_definition != nullptr, "Map::Map - definition was null");

	m_texture = g_engine->m_render->CreateOrGetTextureFromFile(m_definition->m_spriteSheetTexture.c_str());
	m_shader = g_engine->m_render->CreateShader(m_definition->m_shader.c_str());

	UpdatePlayerViewports();

	if (m_definition->m_name == "TestMap")
	{
		m_squirrelParty = new PokemonParty(this);
		m_squirrelParty->InitializeDefaultParty();
		m_squirrelParty->SpawnNextPokemon();
	}

	m_skyTexture = g_engine->m_render->CreateOrGetTextureFromFile("Data/Images/MountainSkybox.png");

	CreateTiles();
	CreateGeometry();
	CreateBuffers();
	CreateSkyCylinder();

	SpawnActorsFromDefinition();
}

Map::~Map()
{
	for (Player* player : m_players)
	{
		delete player;
	}
	m_players.clear();
	m_player = nullptr;

	delete m_squirrelParty;
	m_squirrelParty = nullptr;

	for (Actor* actor : m_actors)
	{
		delete actor;
	}
	m_actors.clear();

	for (Controller* controller : m_controllers)
	{
		delete controller;
	}
	m_controllers.clear();

	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;

	delete m_shader;
	m_shader = nullptr;
}


void Map::ConfigurePlayersFromLobby(bool keyboardJoined, bool controllerJoined)
{
	for (Player* player : m_players)
	{
		delete player;
	}
	m_players.clear();
	m_player = nullptr;

	if (keyboardJoined)
	{
		Player* player = new Player(this);
		player->m_playerIndex = (int)m_players.size();
		player->m_controllerIndex = -1;
		m_players.push_back(player);
	}

	if (controllerJoined)
	{
		Player* player = new Player(this);
		player->m_playerIndex = (int)m_players.size();
		player->m_controllerIndex = 0;
		m_players.push_back(player);
	}

	m_player = m_players.empty() ? nullptr : m_players[0];

	UpdatePlayerViewports();

	if (g_engine->m_audio != nullptr)
	{
		g_engine->m_audio->SetNumListeners((int)m_players.size());
	}

	if (m_player != nullptr && m_player->GetActor() == nullptr)
	{
		for (Actor* actor : m_actors)
		{
			if (actor != nullptr && actor->m_canBePossessed)
			{
				m_player->Possess(actor->m_handle);
				break;
			}
		}
	}

	if (m_players.size() > 1 && m_players[1] != nullptr && m_players[1]->GetActor() == nullptr)
	{
		Actor* firstActor = m_player != nullptr ? m_player->GetActor() : nullptr;

		SpawnInfo spawn;
		spawn.m_actor = "Marine";
		spawn.m_faction = "Marine";
		spawn.m_position = firstActor != nullptr
			? firstActor->m_position + Vec3(0.75f, 0.f, 0.f)
			: Vec3(26.f, 16.f, 0.f);
		spawn.m_orientation = firstActor != nullptr
			? firstActor->m_orientation
			: EulerAngles();

		Actor* secondActor = SpawnActor(spawn);
		if (secondActor != nullptr)
		{
			m_players[1]->Possess(secondActor->m_handle);
		}
	}
}

void Map::CreateTiles()
{
	m_tiles.clear();

	for (int z = 0; z < (int)m_definition->m_layerImagePaths.size(); ++z)
	{
		Image mapImage(m_definition->m_layerImagePaths[z].c_str());
		IntVec2 imageDims = mapImage.GetDimensions();

		if (z == 0)
		{
			m_dimensions.x = imageDims.x;
			m_dimensions.y = imageDims.y;
			m_dimensions.z = (int)m_definition->m_layerImagePaths.size();

			m_tiles.resize(m_dimensions.x * m_dimensions.y * m_dimensions.z);
		}

		for (int y = 0; y < m_dimensions.y; ++y)
		{
			for (int x = 0; x < m_dimensions.x; ++x)
			{
				Rgba8 pixelColor = mapImage.GetTexelColor(x, y);
				TileDefinition const* tileDef = TileDefinition::GetByColor(pixelColor);

				GUARANTEE_OR_DIE(tileDef != nullptr, "Could not match pixel color to TileDefinition");

				int index = GetTileIndex(x, y, z);

				m_tiles[index].m_tileDef = tileDef;
				m_tiles[index].m_tileCoords = IntVec2(x, y);
			}
		}
	}
}

void Map::CreateGeometry()
{
	m_vertexes.clear();
	m_indexes.clear();

	for (int z = 0; z < m_dimensions.z; ++z)
	{
		for (int y = 0; y < m_dimensions.y; ++y)
		{
			for (int x = 0; x < m_dimensions.x; ++x)
			{
				Tile const* tile = GetTile(x, y, z);
				if (tile == nullptr || tile->m_tileDef == nullptr)
				{
					continue;
				}

				AABB3 bounds(
					Vec3((float)x, (float)y, (float)z),
					Vec3((float)x + 1.f, (float)y + 1.f, (float)z + 1.f)
				);

				if (tile->m_tileDef->m_isSolid)
				{
					AABB2 wallUVs = m_definition->GetSpriteUVs(tile->m_tileDef->m_wallSpriteCoords);

					Tile const* above = GetTile(x, y, z + 1);
					Tile const* below = GetTile(x, y, z - 1);

					if (above == nullptr || above->m_tileDef == nullptr || !above->m_tileDef->m_isSolid)
					{
						AddGeometryForSolidTop(bounds, wallUVs);
					}

					if (below == nullptr || below->m_tileDef == nullptr || !below->m_tileDef->m_isSolid)
					{
						AddGeometryForSolidBottom(bounds, wallUVs);
					}

					Tile const* north = GetTile(x, y + 1, z);
					Tile const* south = GetTile(x, y - 1, z);
					Tile const* east = GetTile(x + 1, y, z);
					Tile const* west = GetTile(x - 1, y, z);

					if (north == nullptr || north->m_tileDef == nullptr || !north->m_tileDef->m_isSolid)
						AddGeometryForWall(bounds, wallUVs, WALL_NORTH);

					if (south == nullptr || south->m_tileDef == nullptr || !south->m_tileDef->m_isSolid)
						AddGeometryForWall(bounds, wallUVs, WALL_SOUTH);

					if (east == nullptr || east->m_tileDef == nullptr || !east->m_tileDef->m_isSolid)
						AddGeometryForWall(bounds, wallUVs, WALL_EAST);

					if (west == nullptr || west->m_tileDef == nullptr || !west->m_tileDef->m_isSolid)
						AddGeometryForWall(bounds, wallUVs, WALL_WEST);
				}
				else
				{
					if (tile->m_tileDef->m_isOpenAir)
					{
						continue;
					}

					AABB2 floorUVs = m_definition->GetSpriteUVs(tile->m_tileDef->m_floorSpriteCoords);
					AddGeometryForFloor(bounds, floorUVs);

					if (z == m_dimensions.z - 1)
					{
						AABB2 ceilingUVs = m_definition->GetSpriteUVs(tile->m_tileDef->m_ceilingSpriteCoords);
						AddGeometryForCeiling(bounds, ceilingUVs);
					}
				}
			}
		}
	}
}

void Map::AddGeometryForWall(AABB3 const& bounds, AABB2 const& uvs, int wallFace)
{
	Vec3 bl;
	Vec3 br;
	Vec3 tr;
	Vec3 tl;

	switch (wallFace)
	{
	case WALL_NORTH:
		bl = Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z);
		br = Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z);
		tr = Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z);
		tl = Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z);
		break;

	case WALL_SOUTH:
		bl = Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z);
		br = Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z);
		tr = Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z);
		tl = Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z);
		break;

	case WALL_EAST:
		bl = Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z);
		br = Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z);
		tr = Vec3(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z);
		tl = Vec3(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z);
		break;

	case WALL_WEST:
		bl = Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z);
		br = Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z);
		tr = Vec3(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z);
		tl = Vec3(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z);
		break;

	default:
		return;
	}

	AppendIndexedQuad(m_vertexes, m_indexes, br, bl, tl, tr, Rgba8::WHITE, uvs);
}

void Map::AddGeometryForFloor(AABB3 const& bounds, AABB2 const& uvs)
{
	Vec3 bl(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z);
	Vec3 br(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z);
	Vec3 tr(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z);
	Vec3 tl(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z);

	AppendIndexedQuad(m_vertexes, m_indexes, bl, br, tr, tl, Rgba8::WHITE, uvs);
}

void Map::AddGeometryForCeiling(AABB3 const& bounds, AABB2 const& uvs)
{
	Vec3 bl(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z);
	Vec3 br(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z);
	Vec3 tr(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z);
	Vec3 tl(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z);

	AppendIndexedQuad(m_vertexes, m_indexes, bl, br, tr, tl, Rgba8::WHITE, uvs);
}

void Map::AddGeometryForSolidTop(AABB3 const& bounds, AABB2 const& uvs)
{
	Vec3 bl(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z);
	Vec3 br(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z);
	Vec3 tr(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z);
	Vec3 tl(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z);

	AppendIndexedQuad(m_vertexes, m_indexes, bl, br, tr, tl, Rgba8::WHITE, uvs);
}

void Map::AddGeometryForSolidBottom(AABB3 const& bounds, AABB2 const& uvs)
{
	Vec3 bl(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z);
	Vec3 br(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z);
	Vec3 tr(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z);
	Vec3 tl(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z);

	AppendIndexedQuad(m_vertexes, m_indexes, bl, br, tr, tl, Rgba8::WHITE, uvs);
}

void Map::CreateBuffers()
{
	if (m_vertexes.empty() || m_indexes.empty())
	{
		return;
	}

	m_vertexBuffer = g_engine->m_render->CreateVertexBuffer(
		(unsigned int)(m_vertexes.size() * sizeof(Vertex_TBN)),
		sizeof(Vertex_TBN));

	m_indexBuffer = g_engine->m_render->CreateIndexBuffer(
		(unsigned int)(m_indexes.size() * sizeof(unsigned int)),
		sizeof(unsigned int));

	g_engine->m_render->CopyCPUToGPU(
		m_vertexes.data(),
		(unsigned int)(m_vertexes.size() * sizeof(Vertex_TBN)),
		m_vertexBuffer);

	g_engine->m_render->CopyCPUToGPU(
		m_indexes.data(),
		(unsigned int)(m_indexes.size() * sizeof(unsigned int)),
		m_indexBuffer);
}

bool Map::IsPositionInBounds(Vec3 const& position) const
{
	return position.x >= 0.f &&
		position.y >= 0.f &&
		position.x < (float)m_dimensions.x &&
		position.y < (float)m_dimensions.y;
}

bool Map::AreCoordsInBounds(int x, int y) const
{
	return x >= 0 && y >= 0 && x < m_dimensions.x && y < m_dimensions.y;
}

bool Map::AreCoordsInBounds(int x, int y, int z) const
{
	return x >= 0 && y >= 0 && z >= 0 &&
		x < m_dimensions.x &&
		y < m_dimensions.y &&
		z < m_dimensions.z;
}

int Map::GetTileIndex(int x, int y, int z) const
{
	return x + (y * m_dimensions.x) + (z * m_dimensions.x * m_dimensions.y);
}

Tile const* Map::GetTile(int x, int y) const
{
	return GetTile(x, y, 0);
}

Tile const* Map::GetTile(int x, int y, int z) const
{
	if (x < 0 || y < 0 || z < 0 ||
		x >= m_dimensions.x ||
		y >= m_dimensions.y ||
		z >= m_dimensions.z)
	{
		return nullptr;
	}

	return &m_tiles[GetTileIndex(x, y, z)];
}

void Map::Update()
{
	for (Player* player : m_players)
	{
		if (player != nullptr)
		{
			player->Update();
		}
	}

	for (Actor* actor : m_actors)
	{
		if (actor == nullptr)
		{
			continue;
		}

		bool controlledByPlayer = false;

		for (Player* player : m_players)
		{
			if (actor->m_controller == player)
			{
				controlledByPlayer = true;
				break;
			}
		}

		if (actor->m_controller != nullptr && !controlledByPlayer)
		{
			actor->m_controller->Update();
		}
	}

	for (Actor* actor : m_actors)
	{
		if (actor != nullptr)
		{
			actor->Update();
		}
	}

	if (m_squirrelParty != nullptr)
	{
		m_squirrelParty->Update();
	}

	UpdateDebugControlToggle();
	UpdateControlledActorMovement();

	CollideActors();
	CollideActorsWithMap();
	CheckCaveExit();
	DeleteDestroyedActors();

	for (int i = 0; i < (int)m_players.size(); ++i)
	{
		Player* player = m_players[i];
		if (player == nullptr || player->m_worldCamera == nullptr)
		{
			continue;
		}

		Mat44 cameraToWorld = player->m_worldCamera->GetCameraToWorldTransform();

		Vec3 listenerPos = player->m_worldCamera->GetPosition();
		Vec3 listenerForward = cameraToWorld.GetIBasis3D();
		Vec3 listenerUp = cameraToWorld.GetKBasis3D();

		g_engine->m_audio->UpdateListener(
			i,
			listenerPos,
			listenerForward,
			listenerUp
		);
	}
}


void Map::UpdatePlayerViewports()
{
	if (m_players.empty())
	{
		return;
	}

	if (m_players.size() == 1)
	{
		m_players[0]->m_viewport = AABB2(Vec2(0.f, 0.f), Vec2(1.f, 1.f));
		return;
	}

	m_players[0]->m_viewport = AABB2(Vec2(0.f, 0.5f), Vec2(1.f, 1.f));
	m_players[1]->m_viewport = AABB2(Vec2(0.f, 0.f), Vec2(1.f, 0.5f));
}

void Map::DeleteDestroyedActors()
{
	std::vector<Player*> playersToRespawn;

	for (int i = 0; i < (int)m_actors.size(); ++i)
	{
		Actor* actor = m_actors[i];
		if (actor == nullptr)
		{
			continue;
		}

		if (actor->m_isDead && actor->m_timeSinceDeath > actor->m_corpseLifetime)
		{
			Prop* deletedProp = dynamic_cast<Prop*>(actor);

			if (actor == m_debugProjectile)
			{
				m_debugProjectile = nullptr;
			}

			for (Player* player : m_players)
			{
				if (player != nullptr && player->GetActor() == actor)
				{
					playersToRespawn.push_back(player);
				}
			}

			delete actor;
			m_actors[i] = nullptr;

			if (m_squirrelParty != nullptr)
			{
				m_squirrelParty->NotifyActorDeleted(deletedProp);
			}
		}
	}

	for (Player* player : playersToRespawn)
	{
		RespawnPlayer(player);
	}
}

void Map::UpdateDebugControlToggle()
{
/*	InputSystem* input = g_engine->m_input;*/

// 	if (input->WasKeyJustPressed(KEYCODE_F1))
// 	{
// 		m_controlsProjectile = !m_controlsProjectile;
// 		m_player->m_disableMovement = !m_player->m_disableMovement;
// 
// 		if (m_controlsProjectile)
// 		{
// 			DebugAddMessage("Controls: Projectile", 2.f, Rgba8::WHITE, Rgba8::WHITE);
// 		}
// 		else
// 		{
// 			DebugAddMessage("Controls: Player", 2.f, Rgba8::WHITE, Rgba8::WHITE);
// 		}
// 	}

// 	if (input->WasKeyJustPressed(KEYCODE_F2))
// 	{
// 		m_debugDrawRaycasts = !m_debugDrawRaycasts;
// 	}
}

void Map::UpdateControlledActorMovement()
{
	if (m_player == nullptr || m_debugProjectile == nullptr || !m_controlsProjectile)
	{
		return;
	}

	Actor* playerActor = m_player->GetActor();
	if (playerActor == nullptr)
	{
		return;
	}

	InputSystem* input = g_engine->m_input;

	float dt = (float)m_game->m_gameClock->GetDeltaSeconds();
	float moveSpeed = input->IsKeyDown(KEYCODE_SHIFT) ? 15.f : 1.f;

	Mat44 playerToWorld = playerActor->GetModelToWorldTransform();

	Vec3 forward = playerToWorld.GetIBasis3D().GetNormalized();
	Vec3 left = playerToWorld.GetJBasis3D().GetNormalized();

	forward.z = 0.f;
	left.z = 0.f;

	if (forward.GetLengthSquared() > 0.f)
	{
		forward = forward.GetNormalized();
	}
	if (left.GetLengthSquared() > 0.f)
	{
		left = left.GetNormalized();
	}

	Vec3 moveDir = Vec3::ZERO;

	if (input->IsKeyDown('W'))
		moveDir += forward;
	if (input->IsKeyDown('S'))
		moveDir -= forward;
	if (input->IsKeyDown('A'))
		moveDir += left;
	if (input->IsKeyDown('D'))
		moveDir -= left;
	if (input->IsKeyDown('Z'))
		moveDir += Vec3(0.f, 0.f, 1.f);
	if (input->IsKeyDown('C'))
		moveDir -= Vec3(0.f, 0.f, 1.f);

	if (moveDir.GetLengthSquared() > 0.f)
	{
		moveDir = moveDir.GetNormalized();
		m_debugProjectile->m_position += moveDir * moveSpeed * dt;
	}
}

void Map::CollideActors()
{
	for (int i = 0; i < (int)m_actors.size(); ++i)
	{
		for (int j = i + 1; j < (int)m_actors.size(); ++j)
		{
			CollideActors(m_actors[i], m_actors[j]);
		}
	}
}

void Map::CollideActors(Actor* actorA, Actor* actorB)
{
	if (actorA == nullptr || actorB == nullptr)
	{
		return;
	}

	if (actorA->m_isDead || actorB->m_isDead)
	{
		return;
	}

	if (actorA->m_owner.IsValid() && actorA->m_owner == actorB->m_handle)
	{
		return;
	}

	if (actorB->m_owner.IsValid() && actorB->m_owner == actorA->m_handle)
	{
		return;
	}

	if (!actorA->m_collidesWithActors || !actorB->m_collidesWithActors)
	{
		return;
	}

	float aBottom = actorA->m_position.z;
	float aTop = actorA->m_position.z + actorA->m_physicsHeight;
	float bBottom = actorB->m_position.z;
	float bTop = actorB->m_position.z + actorB->m_physicsHeight;

	bool zOverlaps = !(aTop <= bBottom || bTop <= aBottom);
	if (!zOverlaps)
	{
		return;
	}

	Vec2 aCenter(actorA->m_position.x, actorA->m_position.y);
	Vec2 bCenter(actorB->m_position.x, actorB->m_position.y);

	if (!DoDiscsOverlap(aCenter, actorA->m_physicsRadius, bCenter, actorB->m_physicsRadius))
	{
		return;
	}

	ActorHandle sourceA = actorA->m_owner.IsValid() ? actorA->m_owner : actorA->m_handle;
	ActorHandle sourceB = actorB->m_owner.IsValid() ? actorB->m_owner : actorB->m_handle;

	if (actorA->m_dieOnCollide)
	{
		actorA->Damage(actorA->m_health, sourceB);
	}
	if (actorB->m_dieOnCollide)
	{
		actorB->Damage(actorB->m_health, sourceA);
	}

	if (actorA->m_damageOnCollide.m_max > 0.f)
	{
		float damageA = 0.5f * (actorA->m_damageOnCollide.m_min + actorA->m_damageOnCollide.m_max);
		actorB->Damage(damageA, sourceA);
	}
	if (actorB->m_damageOnCollide.m_max > 0.f)
	{
		float damageB = 0.5f * (actorB->m_damageOnCollide.m_min + actorB->m_damageOnCollide.m_max);
		actorA->Damage(damageB, sourceB);
	}

	if (actorA->m_impulseOnCollide > 0.f)
	{
		Vec3 dir = (actorB->m_position - actorA->m_position).GetNormalized();
		actorB->AddImpulse(dir * actorA->m_impulseOnCollide);
	}
	if (actorB->m_impulseOnCollide > 0.f)
	{
		Vec3 dir = (actorA->m_position - actorB->m_position).GetNormalized();
		actorA->AddImpulse(dir * actorB->m_impulseOnCollide);
	}

	if (actorA->m_isStatic && actorB->m_isStatic)
	{
		return;
	}
	else if (actorA->m_isStatic && !actorB->m_isStatic)
	{
		PushDiscOutOfFixedDisc2D(
			bCenter,
			actorB->m_physicsRadius,
			aCenter,
			actorA->m_physicsRadius
		);

		actorB->m_position.x = bCenter.x;
		actorB->m_position.y = bCenter.y;
	}
	else if (!actorA->m_isStatic && actorB->m_isStatic)
	{
		PushDiscOutOfFixedDisc2D(
			aCenter,
			actorA->m_physicsRadius,
			bCenter,
			actorB->m_physicsRadius
		);

		actorA->m_position.x = aCenter.x;
		actorA->m_position.y = aCenter.y;
	}
	else
	{
		PushDiscsOutOfEachOther2D(
			aCenter,
			actorA->m_physicsRadius,
			bCenter,
			actorB->m_physicsRadius
		);

		actorA->m_position.x = aCenter.x;
		actorA->m_position.y = aCenter.y;

		actorB->m_position.x = bCenter.x;
		actorB->m_position.y = bCenter.y;
	}
}

void Map::CollideActorsWithMap()
{
	for (Actor* actor : m_actors)
	{
		if (actor != nullptr)
		{
			CollideActorWithMap(actor);
		}
	}
}

void Map::CollideActorWithMap(Actor* actor)
{
	if (actor == nullptr || actor->m_isDead)
	{
		return;
	}

	if (!actor->m_collidesWithWorld)
	{
		return;
	}

	bool hitWorld = false;

	// Floor
	if (actor->m_position.z < 0.f)
	{
		if (!actor->m_ignoreFloorCollision)
		{
			hitWorld = true;

			if (actor->m_dieOnCollide)
			{
				actor->Damage(actor->m_health, ActorHandle::INVALID);
				return;
			}

			actor->m_position.z = 0.f;
		}
	}

	Vec2 actorCenter(actor->m_position.x, actor->m_position.y);
	float actorRadius = actor->m_physicsRadius;

	int tileX = RoundDownToInt(actor->m_position.x);
	int tileY = RoundDownToInt(actor->m_position.y);

	for (int yOffset = -1; yOffset <= 1; ++yOffset)
	{
		for (int xOffset = -1; xOffset <= 1; ++xOffset)
		{
			int neighborX = tileX + xOffset;
			int neighborY = tileY + yOffset;

			Tile const* tile = GetTile(neighborX, neighborY);
			if (tile == nullptr || tile->m_tileDef == nullptr)
			{
				continue;
			}

			if (!tile->m_tileDef->m_isSolid)
			{
				continue;
			}

			AABB2 tileBounds(
				Vec2((float)neighborX, (float)neighborY),
				Vec2((float)neighborX + 1.f, (float)neighborY + 1.f)
			);

			Vec2 nearestPoint = tileBounds.GetNearestPoint(actorCenter);
			Vec2 displacement = actorCenter - nearestPoint;
			bool overlappedBeforePush = displacement.GetLengthSquared() < (actorRadius * actorRadius);

			if (overlappedBeforePush)
			{
				hitWorld = true;

				if (actor->m_dieOnCollide)
				{
					actor->Damage(actor->m_health, ActorHandle::INVALID);
					return;
				}

				PushDiscOutOfFixedAABB2D(actorCenter, actorRadius, tileBounds);
			}
		}
	}

	actor->m_position.x = actorCenter.x;
	actor->m_position.y = actorCenter.y;

	if (hitWorld)
	{
		actor->m_velocity = Vec3::ZERO;
	}
}

Actor* Map::SpawnActor(SpawnInfo const& spawnInfo)
{
	ActorDefinition const* actorDef = ActorDefinition::GetByName(spawnInfo.m_actor);
	GUARANTEE_OR_DIE(actorDef != nullptr, "SpawnActor could not find ActorDefinition by name");

	Actor* actor = new Actor(m_game, this);

	actor->m_definition = actorDef;
	actor->m_position = spawnInfo.m_position;
	actor->m_orientation = spawnInfo.m_orientation;

	actor->m_visible = actorDef->m_visible;
	actor->m_health = actorDef->m_health;
	actor->m_corpseLifetime = actorDef->m_corpseLifetime;
	actor->m_canBePossessed = actorDef->m_canBePossessed;

	actor->m_physicsRadius = actorDef->m_physicsRadius;
	actor->m_physicsHeight = actorDef->m_physicsHeight;
	actor->m_collidesWithWorld = actorDef->m_collidesWithWorld;
	actor->m_collidesWithActors = actorDef->m_collidesWithActors;
	actor->m_dieOnCollide = actorDef->m_dieOnCollide;
	actor->m_damageOnCollide = actorDef->m_damageOnCollide;
	actor->m_impulseOnCollide = actorDef->m_impulseOnCollide;

	actor->m_simulated = actorDef->m_simulated;
	actor->m_canFly = actorDef->m_flying;
	actor->m_walkSpeed = actorDef->m_walkSpeed;
	actor->m_runSpeed = actorDef->m_runSpeed;
	actor->m_drag = actorDef->m_drag;
	actor->m_turnSpeed = actorDef->m_turnSpeed;

	actor->m_eyeHeight = actorDef->m_eyeHeight;
	actor->m_cameraFOVDegrees = actorDef->m_cameraFOVDegrees;

	actor->m_aiEnabled = actorDef->m_aiEnabled;
	actor->m_sightRadius = actorDef->m_sightRadius;
	actor->m_sightAngle = actorDef->m_sightAngle;

	actor->m_faction = spawnInfo.m_faction;
	if (actor->m_faction.empty())
	{
		switch (actorDef->m_faction)
		{
		case Faction::MARINE: actor->m_faction = "MARINE"; break;
		case Faction::DEMON: actor->m_faction = "DEMON"; break;
		default: actor->m_faction = "NEUTRAL"; break;
		}
	}

	unsigned int index = 0;
	bool reusedSlot = false;

	for (unsigned int i = 0; i < (unsigned int)m_actors.size(); ++i)
	{
		if (m_actors[i] == nullptr)
		{
			index = i;
			m_actors[i] = actor;
			reusedSlot = true;
			break;
		}
	}

	if (!reusedSlot)
	{
		GUARANTEE_OR_DIE(m_actors.size() < ActorHandle::MAX_ACTOR_INDEX, "Exceeded max actor count");
		index = (unsigned int)m_actors.size();
		m_actors.push_back(actor);
	}

	unsigned int uid = m_nextActorUID;
	++m_nextActorUID;

	if (m_nextActorUID >= ActorHandle::MAX_ACTOR_UID)
	{
		m_nextActorUID = 0;
	}

	actor->m_actorUID = uid;
	actor->m_handle = ActorHandle(uid, index);

	if (actor->m_aiEnabled)
	{
		AIController* ai = new AIController(this);
		ai->m_detectionRange = actor->m_sightRadius;
		actor->m_aiController = ai;
		ai->Possess(actor->m_handle);
	}

	for (std::string const& weaponName : actorDef->m_weaponNames)
	{
		WeaponDefinition const* weaponDef = WeaponDefinition::GetByName(weaponName);
		if (weaponDef != nullptr)
		{
			actor->m_weapons.push_back(new Weapon(weaponDef));
		}
	}

	if (!actor->m_weapons.empty())
	{
		actor->m_equippedWeaponIndex = 0;
	}

	if (spawnInfo.m_actor == "Demon")
	{
		actor->m_color = Rgba8(255, 0, 0, 255);
	}
	else if (spawnInfo.m_actor == "Marine")
	{
		actor->m_color = Rgba8(255, 255, 255, 255);
	}
	else if (spawnInfo.m_actor == "PlasmaProjectile")
	{
		actor->m_color = Rgba8(0, 0, 255, 255);
	}

	actor->PlayAnimationGroup("Walk");

	if (actorDef->m_dieOnSpawn)
	{
		actor->m_isDead = true;
		actor->m_timeSinceDeath = 0.f;
		actor->PlayAnimationGroup("Death");
	}


	return actor;
}


Actor* Map::SpawnProjectile(SpawnInfo const& spawnInfo, ActorHandle ownerHandle, Vec3 const& velocity)
{
	Actor* projectile = SpawnActor(spawnInfo);
	if (projectile == nullptr)
	{
		return nullptr;
	}

	projectile->m_owner = ownerHandle;
	projectile->m_velocity = velocity;
	projectile->m_canBePossessed = false;
	projectile->m_canFly = true;
	projectile->m_isStatic = false;
	projectile->m_canBeHitByRaycast = false;

	if (spawnInfo.m_actor == "PlasmaProjectile")
	{
		projectile->m_color = Rgba8(0, 0, 255, 255);
	}

	return projectile;
}

void Map::SpawnActorsFromDefinition()
{
	Actor* firstPossessableActor = nullptr;
	bool spawnedPlayerFromMarineSpawnPoint = false;

	for (SpawnInfo const& spawnInfo : m_definition->m_spawnInfos)
	{
		if (spawnInfo.m_actor == "SpawnPoint")
		{
			if (!spawnedPlayerFromMarineSpawnPoint &&
				(spawnInfo.m_faction == "Marine" || spawnInfo.m_faction == "MARINE"))
			{
				SpawnInfo marineSpawn = spawnInfo;
				marineSpawn.m_actor = "Marine";
				marineSpawn.m_faction = "Marine";

				Actor* marineActor = SpawnActor(marineSpawn);
				if (marineActor != nullptr)
				{
					spawnedPlayerFromMarineSpawnPoint = true;

					if (m_player != nullptr && m_player->GetActor() == nullptr)
					{
						m_player->Possess(marineActor->m_handle);
					}
				}
			}

			continue;
		}

		Actor* actor = SpawnActor(spawnInfo);

		if (actor == nullptr)
		{
			continue;
		}

		if (firstPossessableActor == nullptr && actor->m_canBePossessed)
		{
			firstPossessableActor = actor;
		}
	}

	if (m_player != nullptr && m_player->GetActor() == nullptr && firstPossessableActor != nullptr)
	{
		m_player->Possess(firstPossessableActor->m_handle);
	}
}

Actor* Map::GetActorByHandle(ActorHandle const handle) const
{
	if (!handle.IsValid())
	{
		return nullptr;
	}

	unsigned int index = handle.GetIndex();

	if (index >= m_actors.size())
	{
		return nullptr;
	}

	Actor* actor = m_actors[index];
	if (actor == nullptr)
	{
		return nullptr;
	}

	if (actor->m_actorUID != handle.GetUID())
	{
		return nullptr;
	}

	return actor;
}

void Map::DestroyActor(ActorHandle handle)
{
	Actor* actor = GetActorByHandle(handle);
	if (actor == nullptr)
	{
		return;
	}

	if (actor == m_debugProjectile)
	{
		m_debugProjectile = nullptr;
	}

	unsigned int index = handle.GetIndex();
	delete actor;
	m_actors[index] = nullptr;
}


void Map::RespawnPlayer(Player* player)
{
	if (player == nullptr)
	{
		return;
	}

	if (player->GetActor() != nullptr)
	{
		return;
	}

	for (SpawnInfo const& spawnInfo : m_definition->m_spawnInfos)
	{
		if (spawnInfo.m_actor == "SpawnPoint" &&
			(spawnInfo.m_faction == "Marine" || spawnInfo.m_faction == "MARINE"))
		{
			SpawnInfo marineSpawn = spawnInfo;
			marineSpawn.m_actor = "Marine";
			marineSpawn.m_faction = "Marine";

			Actor* marineActor = SpawnActor(marineSpawn);
			if (marineActor != nullptr)
			{
				marineActor->BecomeInvincible(1.f);
				player->Possess(marineActor->m_handle);
			}

			return;
		}
	}
}

void Map::Render(Camera const& camera, Actor const* controlledActor) const
{
	RenderSky(camera);

	g_engine->m_render->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_engine->m_render->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_engine->m_render->SetBlendMode(BlendMode::OPAQUE);
	g_engine->m_render->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_engine->m_render->BindShader(m_shader);
	g_engine->m_render->BindTexture(m_texture);
	g_engine->m_render->SetModelConstants();

	if (m_vertexBuffer != nullptr && m_indexBuffer != nullptr)
	{
		g_engine->m_render->DrawIndexedVertexBuffer(
			m_vertexBuffer,
			m_indexBuffer,
			(unsigned int)m_indexes.size()
		);
	}

	std::vector<Actor*> possessedActors;

	for (Player* player : m_players)
	{
		if (player != nullptr && player->GetActor() != nullptr)
		{
			possessedActors.push_back(player->GetActor());
		}
	}

	for (Actor* actor : m_actors)
	{
		if (actor == nullptr)
		{
			continue;
		}

		if (actor == controlledActor)
		{
			continue;
		}

		actor->Render(camera);
	}

	if (m_squirrelParty != nullptr)
	{
		m_squirrelParty->RenderGengarAttackWarning();
	}
}

RaycastResult3D Map::RaycastAll(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner) const
{
	Vec3 fwd = direction.GetNormalized();

	RaycastResult3D actorHit = RaycastWorldActors(start, fwd, distance, owner);
	RaycastResult3D gridHit = RaycastWorldXY(start, fwd, distance);
	RaycastResult3D zHit = RaycastWorldZ(start, fwd, distance);

	RaycastResult3D bestHit = GetCloserHit(actorHit, gridHit);
	bestHit = GetCloserHit(bestHit, zHit);

	return bestHit;
}

RaycastResult3D Map::RaycastWorldZ(Vec3 const& start, Vec3 const& direction, float distance) const
{
	RaycastResult3D result;
	result.m_rayStartPosition = start;
	result.m_rayDirection = direction;
	result.m_rayLength = distance;

	if (direction.z == 0.f)
	{
		return result;
	}

	float bestT = distance + 1.f;

	if (direction.z < 0.f)
	{
		float tGround = (0.f - start.z) / direction.z;

		if (tGround >= 0.f && tGround <= distance)
		{
			Vec3 hitPos = start + direction * tGround;

			int tileX = RoundDownToInt(hitPos.x);
			int tileY = RoundDownToInt(hitPos.y);

			if (AreCoordsInBounds(tileX, tileY))
			{
				result.m_didImpact = true;
				result.m_impactDistance = tGround;
				result.m_impactPosition = hitPos;
				result.m_impactNormal = Vec3(0.f, 0.f, 1.f);
				bestT = tGround;
			}
		}
	}

	for (int z = 0; z <= m_dimensions.z; ++z)
	{
		float planeZ = (float)z;
		float t = (planeZ - start.z) / direction.z;

		if (t < 0.f || t > distance || t >= bestT)
		{
			continue;
		}

		Vec3 hitPos = start + direction * t;

		int tileX = RoundDownToInt(hitPos.x);
		int tileY = RoundDownToInt(hitPos.y);

		if (!AreCoordsInBounds(tileX, tileY))
		{
			continue;
		}

		bool shouldHitPlane = false;

		Tile const* below = GetTile(tileX, tileY, z - 1);
		Tile const* above = GetTile(tileX, tileY, z);

		if (direction.z > 0.f)
		{
			if (above != nullptr &&
				above->m_tileDef != nullptr &&
				above->m_tileDef->m_isSolid)
			{
				shouldHitPlane = true;
			}
		}
		else
		{
			if (below != nullptr &&
				below->m_tileDef != nullptr &&
				!below->m_tileDef->m_isOpenAir)
			{
				shouldHitPlane = true;
			}
		}

		if (!shouldHitPlane)
		{
			continue;
		}

		result.m_didImpact = true;
		result.m_impactDistance = t;
		result.m_impactPosition = hitPos;
		result.m_impactNormal = direction.z > 0.f
			? Vec3(0.f, 0.f, -1.f)
			: Vec3(0.f, 0.f, 1.f);

		bestT = t;
	}

	return result;
}

RaycastResult3D Map::RaycastWorldXY(Vec3 const& start, Vec3 const& direction, float distance) const
{
	RaycastResult3D result;
	result.m_rayStartPosition = start;
	result.m_rayDirection = direction;
	result.m_rayLength = distance;

	Vec2 rayStart(start.x, start.y);
	Vec2 rayDir(direction.x, direction.y);

	if (rayDir.x == 0.f && rayDir.y == 0.f)
	{
		return result;
	}

	int tileX = RoundDownToInt(rayStart.x);
	int tileY = RoundDownToInt(rayStart.y);

	int stepX = 0;
	int stepY = 0;

	float tDeltaX = FLT_MAX;
	float tDeltaY = FLT_MAX;
	float tMaxX = FLT_MAX;
	float tMaxY = FLT_MAX;

	if (rayDir.x > 0.f)
	{
		stepX = 1;
		tDeltaX = 1.f / rayDir.x;
		tMaxX = ((float)(tileX + 1) - rayStart.x) / rayDir.x;
	}
	else if (rayDir.x < 0.f)
	{
		stepX = -1;
		tDeltaX = 1.f / -rayDir.x;
		tMaxX = (rayStart.x - (float)tileX) / -rayDir.x;
	}

	if (rayDir.y > 0.f)
	{
		stepY = 1;
		tDeltaY = 1.f / rayDir.y;
		tMaxY = ((float)(tileY + 1) - rayStart.y) / rayDir.y;
	}
	else if (rayDir.y < 0.f)
	{
		stepY = -1;
		tDeltaY = 1.f / -rayDir.y;
		tMaxY = (rayStart.y - (float)tileY) / -rayDir.y;
	}

	float t = 0.f;
	Vec3 impactNormal = Vec3::ZERO;

	while (t <= distance)
	{
		Tile const* tile = GetTile(tileX, tileY);
		if (tile != nullptr && tile->m_tileDef != nullptr && tile->m_tileDef->m_isSolid)
		{
			Vec3 hitPos = start + direction * t;
			if (hitPos.z >= 0.f && hitPos.z <= 1.f)
			{
				result.m_didImpact = true;
				result.m_impactDistance = t;
				result.m_impactPosition = hitPos;
				result.m_impactNormal = impactNormal;
				return result;
			}
		}

		if (tMaxX < tMaxY)
		{
			t = tMaxX;
			tMaxX += tDeltaX;
			tileX += stepX;
			impactNormal = Vec3((float)-stepX, 0.f, 0.f);
		}
		else
		{
			t = tMaxY;
			tMaxY += tDeltaY;
			tileY += stepY;
			impactNormal = Vec3(0.f, (float)-stepY, 0.f);
		}

		if (!AreCoordsInBounds(tileX, tileY))
		{
			break;
		}
	}

	return result;
}

RaycastResult3D Map::RaycastWorldActors(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner) const
{
	RaycastResult3D bestResult;
	bestResult.m_rayStartPosition = start;
	bestResult.m_rayDirection = direction;
	bestResult.m_rayLength = distance;

	float bestDist = distance + 1.f;

	for (Actor* actor : m_actors)
	{
		if (actor == nullptr || actor == owner || actor->m_isDead)
		{
			continue;
		}

		if (!actor->m_canBeHitByRaycast)
		{
			continue;
		}

		if (owner != nullptr && actor->m_handle == owner->m_owner)
		{
			continue;
		}

		Vec2 center(actor->m_position.x, actor->m_position.y);
		float minZ = actor->m_position.z;
		float maxZ = actor->m_position.z + actor->m_physicsHeight;
		float radius = actor->m_physicsRadius;

		RaycastResult3D hit = RaycastVsCylinderZ3D(
			start,
			direction,
			distance,
			center,
			minZ,
			maxZ,
			radius
		);

		if (hit.m_didImpact && hit.m_impactDistance < bestDist)
		{
			bestResult = hit;
			bestDist = hit.m_impactDistance;
		}
	}

	return bestResult;
}

void Map::DebugPossessNext()
{

	for (Player* player : m_players)
	{
		if (player == nullptr)
		{
			return;
		}
	}

	Actor* oldActor = m_player->GetActor();
	int startIndex = -1;

	if (oldActor != nullptr)
	{
		startIndex = (int)oldActor->m_handle.GetIndex();
	}

	int actorCount = (int)m_actors.size();
	for (int offset = 1; offset <= actorCount; ++offset)
	{
		int index = (startIndex + offset) % actorCount;
		Actor* nextActor = m_actors[index];

		if (nextActor == nullptr || nextActor->m_isDead)
		{
			continue;
		}

		if (!nextActor->m_canBePossessed)
		{
			continue;
		}

		m_player->Possess(nextActor->m_handle);

		if (oldActor != nullptr && oldActor != nextActor)
		{
			if (oldActor->m_aiController != nullptr)
			{
				oldActor->m_aiController->Possess(oldActor->m_handle);
			}
			else
			{
				oldActor->m_controller = nullptr;
				oldActor->OnUnpossessed();
			}
		}

		DebugAddMessage(
			Stringf("Possessed actor index: %u", nextActor->m_handle.GetIndex()),
			2.f,
			Rgba8::WHITE,
			Rgba8::WHITE
		);
		return;
	}
}

Actor* Map::GetClosestVisibleEnemy(Vec3 const& fromPosition, Vec3 const& forwardNormal, float maxDist, Actor* requester) const
{
	Actor* closestActor = nullptr;
	float closestDistSq = maxDist * maxDist;

	std::string requesterFaction = "";
	float sightAngle = 180.f;

	if (requester != nullptr)
	{
		requesterFaction = requester->m_faction;
		sightAngle = requester->m_sightAngle;
		if (sightAngle <= 0.f)
		{
			sightAngle = 180.f;
		}
	}

	float cosHalfFov = CosDegrees(sightAngle * 0.5f);

	for (Actor* actor : m_actors)
	{
		if (actor == nullptr || actor == requester || actor->m_isDead)
		{
			continue;
		}

		if (!requesterFaction.empty())
		{
			if (actor->m_faction == requesterFaction)
			{
				continue;
			}
			if (actor->m_faction == "Neutral" || actor->m_faction == "NEUTRAL")
			{
				continue;
			}
		}

		Vec3 targetPos = actor->m_position;
		targetPos.z += actor->m_eyeHeight;

		Vec3 toActor = targetPos - fromPosition;
		float distSq = toActor.GetLengthSquared();
		if (distSq <= 0.f || distSq > closestDistSq)
		{
			continue;
		}

		Vec3 dirToActor = toActor.GetNormalized();

		float dot = DotProduct3D(forwardNormal, dirToActor);
		if (dot < cosHalfFov)
		{
			continue;
		}

		float dist = sqrtf(distSq);
		RaycastResult3D gridHit = RaycastWorldXY(fromPosition, dirToActor, dist);
		RaycastResult3D zHit = RaycastWorldZ(fromPosition, dirToActor, dist);
		RaycastResult3D worldHit = GetCloserHit(gridHit, zHit);

		if (!worldHit.m_didImpact || worldHit.m_impactDistance >= dist - 0.01f)
		{
			closestActor = actor;
			closestDistSq = distSq;
		}
	}

	return closestActor;
}

Actor* Map::GetClosestVisibleActorInRay(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner /*= nullptr*/) const
{
	Vec3 fwd = direction.GetNormalized();

	RaycastResult3D gridHit = RaycastWorldXY(start, fwd, distance);
	RaycastResult3D zHit = RaycastWorldZ(start, fwd, distance);
	RaycastResult3D worldHit = GetCloserHit(gridHit, zHit);

	float maxActorDist = distance;
	if (worldHit.m_didImpact)
	{
		maxActorDist = worldHit.m_impactDistance;
	}


	Actor* bestActor = nullptr;
	float bestActorDist = maxActorDist;

	for (Actor* actor : m_actors)
	{
		if (actor == nullptr || actor == owner || actor->m_isDead || !actor->m_canBeHitByRaycast)
		{
			continue;
		}

		if (owner != nullptr && actor->m_handle == owner->m_owner)
		{
			continue;
		}

		Vec2 center(actor->m_position.x, actor->m_position.y);
		float minZ = actor->m_position.z;
		float maxZ = actor->m_position.z + actor->m_physicsHeight;
		float radius = actor->m_physicsRadius;

		RaycastResult3D hit = RaycastVsCylinderZ3D(
			start,
			fwd,
			maxActorDist,
			center,
			minZ,
			maxZ,
			radius
		);

		if (hit.m_didImpact && hit.m_impactDistance < bestActorDist)
		{
			bestActor = actor;
			bestActorDist = hit.m_impactDistance;
		}
	}

	return bestActor;
}


Actor* Map::GetClosestVisibleEnemyInSector(
	Vec3 const& fromPosition,
	Vec3 const& forwardNormal,
	float maxDist,
	float sectorDegrees,
	Actor* requester
) const
{
	Actor* closestActor = nullptr;
	float closestDistSq = maxDist * maxDist;

	std::string requesterFaction = "";
	if (requester != nullptr)
	{
		requesterFaction = requester->m_faction;
	}

	float halfSector = sectorDegrees * 0.5f;
	float cosHalfSector = CosDegrees(halfSector);

	for (Actor* actor : m_actors)
	{
		if (actor == nullptr || actor == requester || actor->m_isDead)
		{
			continue;
		}

		if (requester != nullptr)
		{
			if (actor->m_handle == requester->m_owner)
			{
				continue;
			}
		}

		if (!requesterFaction.empty())
		{
			if (actor->m_faction == requesterFaction)
			{
				continue;
			}
			if (actor->m_faction == "NEUTRAL")
			{
				continue;
			}
		}

		Vec3 targetPos = actor->m_position;
		targetPos.z += actor->m_physicsHeight * 0.5f;

		Vec3 toActor = targetPos - fromPosition;
		float distSq = toActor.GetLengthSquared();
		if (distSq <= 0.f || distSq > closestDistSq)
		{
			continue;
		}

		Vec3 dirToActor = toActor.GetNormalized();
		float dot = DotProduct3D(forwardNormal, dirToActor);
		if (dot < cosHalfSector)
		{
			continue;
		}

		closestActor = actor;
		closestDistSq = distSq;
	}

	return closestActor;
}


Player* Map::GetPlayerByActorHandle(ActorHandle const& handle) const
{
	for (Player* player : m_players)
	{
		if (player == nullptr)
		{
			continue;
		}

		Actor* actor = player->GetActor();
		if (actor != nullptr && actor->m_handle == handle)
		{
			return player;
		}
	}

	return nullptr;
}

Prop* Map::GetActivePokemon() const
{
	if (m_squirrelParty == nullptr)
	{
		return nullptr;
	}

	return m_squirrelParty->GetActivePokemon();
}

Prop* Map::SpawnPokemonOBJ(char const* objPath, char const* bodyMaterialName, char const* bodyTexturePath, char const* eyeMaterialName, char const* eyeTexturePath, Vec3 const& position, EulerAngles const& orientation)
{
	Prop* pokemon = new Prop(m_game, this);

	std::map<std::string, std::vector<Vertex>> vertsByMtl;
	bool loaded = LoadObjToMaterialVertexArrays(vertsByMtl, objPath, Rgba8::WHITE);
	GUARANTEE_OR_DIE(loaded, Stringf("Failed to load OBJ: %s", objPath));

	AddSubmeshToProp(pokemon, vertsByMtl, bodyMaterialName, bodyTexturePath);
	AddSubmeshToProp(pokemon, vertsByMtl, eyeMaterialName, eyeTexturePath);

	pokemon->m_position = position;
	pokemon->m_orientation = orientation;
	pokemon->m_color = Rgba8::WHITE;
	pokemon->m_canBePossessed = false;
	pokemon->m_visible = true;
	pokemon->m_collidesWithActors = false;
	pokemon->m_collidesWithWorld = false;
	pokemon->m_simulated = false;

	unsigned int index = (unsigned int)m_actors.size();
	unsigned int uid = m_nextActorUID++;

	if (m_nextActorUID >= ActorHandle::MAX_ACTOR_UID)
	{
		m_nextActorUID = 0;
	}

	pokemon->m_actorUID = uid;
	pokemon->m_handle = ActorHandle(uid, index);
	m_actors.push_back(pokemon);

	return pokemon;
}

void Map::CreateSkyCylinder()
{
	m_skyVerts.clear();

	int slices = 64;
	int stacks = 32;
	float radius = 250.f;

	for (int y = 0; y < stacks; ++y)
	{
		float v0 = (float)y / (float)stacks;
		float v1 = (float)(y + 1) / (float)stacks;

		float phi0 = RangeMap(v0, 0.f, 1.f, -90.f, 90.f);
		float phi1 = RangeMap(v1, 0.f, 1.f, -90.f, 90.f);

		for (int x = 0; x < slices; ++x)
		{
			float u0 = (float)x / (float)slices;
			float u1 = (float)(x + 1) / (float)slices;

			float theta0 = u0 * 360.f;
			float theta1 = u1 * 360.f;

			Vec3 p00(
				radius * CosDegrees(phi0) * CosDegrees(theta0),
				radius * CosDegrees(phi0) * SinDegrees(theta0),
				radius * SinDegrees(phi0)
			);

			Vec3 p10(
				radius * CosDegrees(phi0) * CosDegrees(theta1),
				radius * CosDegrees(phi0) * SinDegrees(theta1),
				radius * SinDegrees(phi0)
			);

			Vec3 p11(
				radius * CosDegrees(phi1) * CosDegrees(theta1),
				radius * CosDegrees(phi1) * SinDegrees(theta1),
				radius * SinDegrees(phi1)
			);

			Vec3 p01(
				radius * CosDegrees(phi1) * CosDegrees(theta0),
				radius * CosDegrees(phi1) * SinDegrees(theta0),
				radius * SinDegrees(phi1)
			);

			Vec2 uv00(u0, v0);
			Vec2 uv10(u1, v0);
			Vec2 uv11(u1, v1);
			Vec2 uv01(u0, v1);

			// inverted winding, visible from inside
			m_skyVerts.push_back(Vertex(p00, Rgba8::WHITE, uv00));
			m_skyVerts.push_back(Vertex(p11, Rgba8::WHITE, uv11));
			m_skyVerts.push_back(Vertex(p10, Rgba8::WHITE, uv10));

			m_skyVerts.push_back(Vertex(p00, Rgba8::WHITE, uv00));
			m_skyVerts.push_back(Vertex(p01, Rgba8::WHITE, uv01));
			m_skyVerts.push_back(Vertex(p11, Rgba8::WHITE, uv11));
		}
	}
}

void Map::RenderSky(Camera const& camera) const
{
	if (m_skyTexture == nullptr || m_skyVerts.empty())
	{
		return;
	}

	Vec3 camPos = camera.GetPosition();

	Mat44 model = Mat44::MakeTranslation3D(Vec3(camPos.x, camPos.y, 0.f));

	g_engine->m_render->BindShader(nullptr); // important
	g_engine->m_render->SetModelConstants(model, Rgba8::WHITE);
	g_engine->m_render->SetDepthMode(DepthMode::DISABLED);
	g_engine->m_render->SetBlendMode(BlendMode::OPAQUE);
	g_engine->m_render->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_engine->m_render->SetSamplerMode(SamplerMode::POINT_CLAMP);

	g_engine->m_render->BindTexture(m_skyTexture);
	g_engine->m_render->DrawVertexArray((int)m_skyVerts.size(), m_skyVerts.data());

	g_engine->m_render->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_engine->m_render->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_engine->m_render->SetModelConstants();
}

void Map::SetTileType(int x, int y, int z, std::string const& tileDefName)
{
	Tile* tile = const_cast<Tile*>(GetTile(x, y, z));
	if (tile == nullptr)
	{
		return;
	}

	TileDefinition const* def = TileDefinition::GetByName(tileDefName);
	tile->m_tileDef = def;
}

void Map::RebuildGeometry()
{
	CreateGeometry();

	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;

	CreateBuffers();
}

void Map::OpenCaveBehindSquirrel()
{
	SetTileType(1, 17, 0, "CaveWallLeft");
	SetTileType(1, 16, 0, "CaveWallRight");

	m_isCaveOpen = true;

	RebuildGeometry();
}

void Map::CheckCaveExit()
{
	if (!m_isCaveOpen || m_player == nullptr)
	{
		return;
	}

	Actor* playerActor = m_player->GetActor();
	if (playerActor == nullptr)
	{
		return;
	}

	Vec2 playerPos(playerActor->m_position.x, playerActor->m_position.y);

	Vec2 caveCenter(1.5f, 17.f);

	float distSq = (playerPos - caveCenter).GetLengthSquared();

	if (distSq <= 1.0f)
	{
		g_engine->m_audio->StartSound(m_game->m_titleBoom);
		m_game->PlayNewMusic(m_game->m_attractMusic);
		m_game->m_gamemode = GAMEMODE_ATTRACT;
	}
}
