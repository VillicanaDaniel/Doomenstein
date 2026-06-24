#pragma once

#include "Game/Tile.hpp"
#include "Engine/Math/RaycastResult.hpp"
#include "Engine/Core/Vertex_TBN.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/Vec3.hpp"
#include <vector>
#include <string>

class Game;
class Player;
class Actor;
class Prop;
class Texture;
class Shader;
class VertexBuffer;
class IndexBuffer;
class MapDefinition;
class Controller;
class Camera;
struct EulerAngles;
class PokemonParty;
struct Vertex;
struct ActorHandle;
struct SpawnInfo;

enum WallFace
{
	WALL_NORTH = 0,
	WALL_SOUTH,
	WALL_EAST,
	WALL_WEST
};

class Map
{
public:
	Map(Game* game, MapDefinition const* definition);
	virtual ~Map();

	void ConfigurePlayersFromLobby(bool keyboardJoined, bool controllerJoined);
	void CreateTiles();
	void CreateGeometry();
	void AddGeometryForWall(AABB3 const& bounds, AABB2 const& uvs, int wallFace);
	void AddGeometryForFloor(AABB3 const& bounds, AABB2 const& uvs);
	void AddGeometryForCeiling(AABB3 const& bounds, AABB2 const& uvs);
	void AddGeometryForSolidTop(AABB3 const& bounds, AABB2 const& uvs);
	void AddGeometryForSolidBottom(AABB3 const& bounds, AABB2 const& uvs);
	void CreateBuffers();

	void UpdateDebugControlToggle();
	void UpdateControlledActorMovement();

	bool IsPositionInBounds(Vec3 const& position) const;
	bool AreCoordsInBounds(int x, int y) const;
	bool AreCoordsInBounds(int x, int y, int z) const;
	int GetTileIndex(int x, int y, int z) const;
	Tile const* GetTile(int x, int y) const;
	Tile const* GetTile(int x, int y, int z) const;

	virtual void Update();
	void UpdatePlayerViewports();
	void CollideActors();
	void CollideActors(Actor* actorA, Actor* actorB);
	void CollideActorsWithMap();
	void CollideActorWithMap(Actor* actor);
	void DeleteDestroyedActors();

	Actor* SpawnActor(SpawnInfo const& spawnInfo);
	Actor* SpawnProjectile(SpawnInfo const& spawnInfo, ActorHandle ownerHandle, Vec3 const& velocity);
	void SpawnActorsFromDefinition();
	Actor* GetActorByHandle(ActorHandle const handle) const;
	void DestroyActor(ActorHandle handle);
	void RespawnPlayer(Player* player);

	virtual void Render(Camera const& camera, Actor const* controlledActor) const;

	RaycastResult3D RaycastAll(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner = nullptr) const;
	RaycastResult3D RaycastWorldXY(Vec3 const& start, Vec3 const& direction, float distance) const;
	RaycastResult3D RaycastWorldZ(Vec3 const& start, Vec3 const& direction, float distance) const;
	RaycastResult3D RaycastWorldActors(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner = nullptr) const;

	void DebugPossessNext();
	Actor* GetClosestVisibleEnemy(Vec3 const& fromPosition, Vec3 const& forwardNormal, float maxDist, Actor* requester = nullptr) const;
	Actor* GetClosestVisibleActorInRay(Vec3 const& start, Vec3 const& direction, float distance, Actor* owner = nullptr) const;
	Actor* GetClosestVisibleEnemyInSector(Vec3 const& fromPosition, Vec3 const& forwardNormal, float maxDist, float sectorDegrees, Actor* requester = nullptr) const;
	Player* GetPlayerByActorHandle(ActorHandle const& handle) const;
	Prop* GetActivePokemon() const;

	//Simplify loading in pokemon
	Prop* SpawnPokemonOBJ(
		char const* objPath,
		char const* bodyMaterialName,
		char const* bodyTexturePath,
		char const* eyeMaterialName,
		char const* eyeTexturePath,
		Vec3 const& position,
		EulerAngles const& orientation
	);

	//Skybox 
	void CreateSkyCylinder();
	void RenderSky(Camera const& camera) const;

	//Replace tiles while game is running
	void SetTileType(int x, int y, int z, std::string const& tileDefName);
	void RebuildGeometry();
	void OpenCaveBehindSquirrel();

	void CheckCaveExit();

	bool m_isCaveOpen;

public:
	Game* m_game = nullptr;

	Player* m_player = nullptr;              // temporary primary player
	std::vector<Player*> m_players;

	bool m_controlsProjectile = false;
	bool m_debugDrawRaycasts = true;
	Actor* m_debugProjectile = nullptr;

	std::vector<Tile> m_tiles;
	IntVec3 m_dimensions;


	//Pokemon party
	PokemonParty* m_squirrelParty = nullptr;

	//Skybox
	std::vector<Vertex> m_skyVerts;
	Texture* m_skyTexture = nullptr;

protected:
	MapDefinition const* m_definition = nullptr;

	std::vector<Vertex_TBN> m_vertexes;
	std::vector<unsigned int> m_indexes;

	Texture* m_texture = nullptr;
	Shader* m_shader = nullptr;
	VertexBuffer* m_vertexBuffer = nullptr;
	IndexBuffer* m_indexBuffer = nullptr;

	std::vector<Actor*> m_actors;
	std::vector<Controller*> m_controllers;
	unsigned int m_nextActorUID = 0;
};