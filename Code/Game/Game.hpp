#pragma once
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Vertex.hpp"

class Actor;
class Entity;
class Camera;
class Player;
class Clock;
class Prop;
class Map;
class Texture;
struct Vec3;

enum GameMode
{
	GAMEMODE_ATTRACT,
	GAMEMODE_LOBBY,
	GAMEMODE_PLAYING, 
	COUNT,
};

enum DialogueState
{
	DIALOGUE_NONE,
	DIALOGUE_TYPING,
	DIALOGUE_WAITING_FOR_CONFIRM,
	DIALOGUE_TRANSITION
};

struct DialogueRequest
{
	std::vector<std::string> m_lines;
	std::string m_nextMapName = "";
	bool m_changeMapAfterDialogue = false;
};

class Game
{
public:
	Game();
	~Game();

	void InitializeDevConsole();

	void Update();
	void UpdateLobby();
	void StartGameFromLobby();


	void Render() const;
	void RenderAttract() const;
	void RenderPlaying() const;
	void RenderLobby() const;
	void DrawScreenText(std::string const& text, float y, Rgba8 const& color) const;

	void LoadDefinitions();
	void InitializeCameras();
	void InitializeSounds();
	
	void DeleteGarbage();

	void UpdateFromKeyboard();
	void UpdateFromController();

	bool IsInAttractMode();
	static void BuildUnitCubeVerts(Prop& cube);

	void UpdateLightingFromKeyboard();
	void DebugInputHandling();
	void RenderBillboardedText(std::string const& text, Vec3 const& worldPosition, float cellHeight, Rgba8 const& tint) const;

	void StartTrainerBattle();
	void KillCurrentPokemon();

public:
	GameMode m_gamemode = GAMEMODE_ATTRACT;
	Map* m_currentMap = nullptr;
	/*Player* m_player = nullptr;*/

	Clock* GetClock() const;
	Clock* m_gameClock = nullptr;

	Camera* m_worldCamera;
	Camera* m_attractCamera;
	Camera* m_screenCamera;

	std::string m_defaultMapName = "TestMap";

	//Dialogue system that I hope works
	DialogueState m_dialogueState = DIALOGUE_NONE;
	DialogueRequest m_activeDialogue;
	int m_dialogueLineIndex = 0;
	float m_dialogueTimer = 0.f;
	float m_transitionTimer = 0.f;

	void StartDialogue(DialogueRequest const& request);
	void UpdateDialogue();
	void RenderDialogue() const;
	bool IsDialogueActive() const;

	//Pokemon Stuff
	void RenderPokemonHealthBar() const;

	//Makes it easy to stop old music without having to remember what is being played all the time
	void PlayNewMusic(SoundID music);

	SoundID m_attractMusic = MISSING_SOUND_ID;
	SoundID m_gameMusic = MISSING_SOUND_ID;
	SoundID m_overworldMusic = MISSING_SOUND_ID;
	SoundID m_battleMusic = MISSING_SOUND_ID;
	SoundID m_lastPokemonMusic = MISSING_SOUND_ID;
	SoundID m_victoryMusic = MISSING_SOUND_ID;

	SoundID m_titleBoom = MISSING_SOUND_ID;
	SoundID m_buttonClickSound = MISSING_SOUND_ID;
	SoundID m_teleportSound = MISSING_SOUND_ID;

protected:
	std::vector<Entity*> m_entities;
	std::vector<Actor*> m_actors;
	Prop* m_pulsingCube = nullptr;
	Prop* m_riolu = nullptr;

	Vec3  m_sunDirection = Vec3(0.f, 0.f, -1.f);
	float m_sunIntensity = 0.35f;
	float m_ambientIntensity = 0.65f;

	bool m_keyboardPlayerJoined = false;
	bool m_controllerPlayerJoined = false;

	Texture* m_attractScreenTexture = nullptr;

	SoundID m_currentMusic = MISSING_SOUND_ID;
	SoundPlaybackID m_currentMusicPlayback = MISSING_SOUND_ID;
};