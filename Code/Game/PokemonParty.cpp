#include "Game/PokemonParty.hpp"
#include "Game/Game.hpp"
#include "Game/Map.hpp"
#include "Game/Prop.hpp"
#include "Game/Player.hpp"
#include "Game/Actor.hpp"
#include "Game/MapDefinition.hpp"
#include "Game/Weapon.hpp"
#include "Game/WeaponDefinition.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

PokemonParty::PokemonParty(Map* map)
	: m_map(map)
{
}


void PokemonParty::Update()
{
	if (m_map == nullptr || m_activePokemon == nullptr || m_activePokemon->m_isDead)
	{
		return;
	}

	if (m_map->m_player == nullptr)
	{
		return;
	}

	Actor* playerActor = m_map->m_player->GetActor();
	if (playerActor == nullptr || playerActor->m_isDead)
	{
		return;
	}

	if (m_activePokemon->m_displayName == "Porygon-Z")
	{
		UpdatePorygonDiveAttack(playerActor);
	}

	if (m_activePokemon->m_displayName == "Gengar" || m_activePokemon->m_displayName == "Mega Gengar")
	{
		UpdateGengarAttack(playerActor);
	}
	if (m_gengarAttackState != GengarAttackState::None)
	{
		return;
	}

	UpdateActivePokemonMovement();
	UpdatePokemonAttack();
}

void PokemonParty::UpdatePokemonAttack()
{
	if (m_porygonDiveActive)
	{
		return;
	}


	float dt = (float)m_map->m_game->m_gameClock->GetDeltaSeconds();
	Actor* playerActor = m_map->m_player->GetActor();
	if (playerActor == nullptr || playerActor->m_isDead)
	{
		return;
	}
	if (playerActor->IsInvincible())
	{
		return;
	}

	Vec3 fireStart = m_activePokemon->m_position + Vec3(0.f, 0.f, m_activePokemon->m_physicsHeight * 0.5f);
	Vec3 targetPos = playerActor->m_position + Vec3(0.f, 0.f, playerActor->m_physicsHeight * 0.5f);

	Vec3 toPlayer = targetPos - fireStart;

	if (toPlayer.GetLengthSquared() <= 0.f)
	{
		return;
	}

	Vec3 dir = toPlayer.GetNormalized();

	float aimYaw = Atan2Degrees(dir.y, dir.x);

	float horizontalLength = sqrtf((dir.x * dir.x) + (dir.y * dir.y));
	float aimPitch = Atan2Degrees(dir.z, horizontalLength);

	m_activePokemon->m_orientation.m_yawDegrees = aimYaw + 90.f;

	Weapon* weapon = m_activePokemon->GetEquippedWeapon();
	if (weapon != nullptr)
	{
		weapon->Update(dt);
	}

	EulerAngles visualOrientation = m_activePokemon->m_orientation;

	m_activePokemon->m_orientation.m_yawDegrees = aimYaw;
	m_activePokemon->m_orientation.m_pitchDegrees = -aimPitch;

	if (m_activePokemon->m_displayName == "Gengar" || m_activePokemon->m_displayName == "Mega Gengar")
	{
		if (weapon == nullptr || !weapon->CanFire())
		{
			m_activePokemon->m_orientation = visualOrientation;
			return;
		}

		weapon->Fire(m_activePokemon);

		int projectileCount = 40;
		float speed = 5.f;
		float spawnRadius = m_activePokemon->m_physicsRadius + 2.0f;

		if (m_activePokemon->m_displayName == "Mega Gengar")
		{
			speed = 10.f;
		}

		float angleOffset = g_rng->RollRandomFloatInRange(0.f, 360.f);

		for (int i = 0; i < projectileCount; ++i)
		{
			float angle = angleOffset + ((float)i/ (float)projectileCount) * 360.f;

			Vec3 direction(
				CosDegrees(angle),
				SinDegrees(angle),
				0.f
			);

			SpawnInfo spawn;
			spawn.m_actor = "Shadow Ball";
			spawn.m_faction = m_activePokemon->m_faction;
			spawn.m_position =
				m_activePokemon->m_position +
				Vec3(0.f, 0.f, 0.5f) +
				direction * spawnRadius;

			spawn.m_orientation = EulerAngles(angle, 0.f, 0.f);

			Actor* projectile = m_map->SpawnProjectile(
				spawn,
				m_activePokemon->m_handle,
				direction * speed
			);

			if (projectile != nullptr)
			{
				projectile->m_owner = m_activePokemon->m_handle;
				projectile->m_canBeHitByRaycast = false;
			}
		}
	}
	else if (weapon != nullptr)
	{
		weapon->Fire(m_activePokemon);
	}

	m_activePokemon->m_orientation = visualOrientation;
}


void PokemonParty::UpdateActivePokemonMovement()
{
	if (m_porygonDiveActive)
	{
		return;
	}

	if (m_activePokemon == nullptr)
	{
		return;
	}
	float dt = (float)m_map->m_game->m_gameClock->GetDeltaSeconds();

	// Pachirisu moves along y
	if (m_activePokemon->m_displayName == "Pachirisu")
	{
		float minX = 13.f;
		float maxX = 20.f;
		float speed = 4.5f;

		m_activePokemon->m_position.y += m_chatotMoveDir * speed * dt;

		if (m_activePokemon->m_position.y >= maxX)
		{
			m_activePokemon->m_position.y = maxX;
			m_chatotMoveDir = -1.f;
		}
		else if (m_activePokemon->m_position.y <= minX)
		{
			m_activePokemon->m_position.x = minX;
			m_chatotMoveDir = 1.f;
		}

		m_activePokemon->m_position.x = 7.f;
		m_activePokemon->m_position.z = 0.f;
		return;
	}

	//Kricketune moves along y,z
	if (m_activePokemon->m_displayName == "Kricketune")
	{
		m_movementTimer -= dt;

		if (m_movementTimer <= 0.f ||
			(m_porygonTargetPos - m_activePokemon->m_position).GetLengthSquared() < 0.05f)
		{
			m_porygonTargetPos = Vec3(
				7.f,
				g_rng->RollRandomFloatInRange(13.f, 20.f),
				g_rng->RollRandomFloatInRange(0.25f, 5.f)
			);

			m_movementTimer = g_rng->RollRandomFloatInRange(0.5f, 1.0f);
		}

		Vec3 toTarget = m_porygonTargetPos - m_activePokemon->m_position;

		if (toTarget.GetLengthSquared() > 0.001f)
		{
			Vec3 dir = toTarget.GetNormalized();
			float speed = 2.25f;
			m_activePokemon->m_position += dir * speed * dt;
		}
		return;
	}

	//Alakazam
	if (m_activePokemon->m_displayName == "Alakazam")
	{
		m_movementTimer -= dt;

		if (m_movementTimer <= 0.f)
		{
			m_porygonTargetPos = Vec3(
				g_rng->RollRandomFloatInRange(7.f, 20.f),
				g_rng->RollRandomFloatInRange(13.f, 20.f),
				g_rng->RollRandomFloatInRange(0.25f, 5.f)
			);

			m_movementTimer = g_rng->RollRandomFloatInRange(1.0f, 2.0f);
			g_engine->m_audio->StartSound(m_map->m_game->m_teleportSound, false, 0.25f);
		}

		m_activePokemon->m_position = m_porygonTargetPos;
		return;
	}

	//Porygon moves along x, y and z
	if (m_activePokemon->m_displayName == "Porygon-Z")
	{
		m_movementTimer -= dt;

		if (m_movementTimer <= 0.f ||
			(m_porygonTargetPos - m_activePokemon->m_position).GetLengthSquared() < 0.05f)
		{
			m_porygonTargetPos = Vec3(
				g_rng->RollRandomFloatInRange(7.f, 20.f),
				g_rng->RollRandomFloatInRange(13.f, 20.f),
				g_rng->RollRandomFloatInRange(0.25f, 5.f)
			);

			m_movementTimer = g_rng->RollRandomFloatInRange(0.4f, 0.6f);
		}

		Vec3 toTarget = m_porygonTargetPos - m_activePokemon->m_position;

		if (toTarget.GetLengthSquared() > 0.001f)
		{
			Vec3 dir = toTarget.GetNormalized();
			float speed = 3.25f;
			m_activePokemon->m_position += dir * speed * dt;
		}
		return;
	}
}

void PokemonParty::InitializeDefaultParty()
{
	m_party.clear();
	m_activeIndex = -1;
	m_activePokemon = nullptr;

	m_party.push_back({
		"Emolga",
		"Data/Objects/Emolga.obj",
		"mat_body_mat_id",
		"Data/Images/Pii_EMONGA_body.png",
		"TEX_ANIM_EYE_mat_id",
		"Data/Images/Pii_EMONGA_eye.png",
		"PokemonWeapon",
		20.f,
		Vec3(7.f, 16.f, 0.f),
		EulerAngles(90.f, 0.f, 0.f)
		});

	m_party.push_back({
		"Pachirisu",
		"Data/Objects/Pachirisu_M.obj",
		"pokemon_2leg_PIKATYUU_lambert3_mat_id",
		"Data/Images/Pii_PATIRISU_1.png",
		"TEX_ANIM_EYE_mat_id",
		"Data/Images/Pii_PATIRISU_eye.png",
		"PokemonWeapon",
		70.f,
		Vec3(7.f, 16.f, 0.f),
		EulerAngles(90.f, 0.f, 0.f)
		});

	m_party.push_back({
		"Kricketune",
		"Data/Objects/Kricketune_M.obj",
		"mat_body_mat_id",
		"Data/Images/Pii_KOROTOKKU_body.png",
		"TEX_ANIM_EYE_mat_id",
		"Data/Images/Pii_KOROTOKKU_eye.png",
		"MusicWeapon2",
		125.f,
		Vec3(7.f, 16.f, 0.f),
		EulerAngles(90.f, 0.f, 0.f)
		});

	m_party.push_back({
		"Alakazam",
		"Data/Objects/Alakazam_Normalm.obj",
		"mat_skin_mat_id",
		"Data/Images/Pii_HUUDHIN_skin.png",
		"TEX_ANIM_EYE_mat_id",
		"Data/Images/Pii_HUUDHIN_eye.png",
		"AlakazamWeapon",
		150.f,
		Vec3(7.f, 16.f, 0.f),
		EulerAngles(90.f, 0.f, 0.f)
		});

	m_party.push_back({
		"Porygon-Z",
		"Data/Objects/Porygon-Z.obj",
		"mat_body_mat_id",
		"Data/Images/Pii_PORIGONZ_body.png",
		"TEX_ANIM_EYE_mat_id",
		"Data/Images/Pii_PORIGONZ_eye.png",
		"PorygonWeapon",
		150.f,
		Vec3(7.f, 16.f, 0.f),
		EulerAngles(90.f, 0.f, 0.f)
		});

	m_party.push_back({
		"Gengar",
		"Data/Objects/Gengar_Normal.obj",
		"mat_kao1_mat_id",
		"Data/Images/Pii_GENGAA_kao.png",
		"TEX_ANIM_EYE_mat_id",
		"Data/Images/Pii_GENGAA_eye.png",
		"GengarWeapon",
		190.f,
		Vec3(7.f, 16.f, 0.f),
		EulerAngles(90.f, 0.f, 0.f)
		});

	m_party.push_back({
		"Mega Gengar",
		"Data/Objects/Gengar_Mega.obj",
		"mat_kao1_mat_id",
		"Data/Images/Pii_GENGAA_MEGA_body.png",
		"TEX_ANIM_EYE_mat_id",
		"Data/Images/Pii_GENGAA_MEGA_eye.png",
		"GengarWeapon",
		250.f,
		Vec3(7.f, 16.f, 0.f),
		EulerAngles(90.f, 0.f, 0.f)
		});

}

void PokemonParty::SpawnNextPokemon()
{
	if (m_map == nullptr)
	{
		return;
	}

	++m_activeIndex;

	int pokemonLeft = (int)m_party.size() - m_activeIndex;

	if (pokemonLeft <= 0)
	{
		m_activePokemon = nullptr;

		SpawnInfo squirrelSpawn;
		squirrelSpawn.m_actor = "Squirrel3D";
		squirrelSpawn.m_faction = "Trainer";
		squirrelSpawn.m_position = Vec3(7.f, 16.f, 0.0f);
		squirrelSpawn.m_orientation = EulerAngles(0.f, 0.f, 0.f);

		m_map->SpawnActor(squirrelSpawn);

		m_map->OpenCaveBehindSquirrel();

		DialogueRequest request;
		request.m_lines.push_back("Mega Gengar \"fainted\"");
		request.m_lines.push_back("You defeated Guildhall Professor Squirrel");
		request.m_lines.push_back("Squirrel: I can't believe it...");
		request.m_lines.push_back("Squirrel: You just shot all of my Pokemon...");
		request.m_lines.push_back("Squirrel: What's wrong with you?");
		request.m_lines.push_back("Squirrel: Well, there's nothing left for you here.");
		request.m_lines.push_back("Squirrel: Go ahead and leave, the exit is right behind me");
		request.m_changeMapAfterDialogue = false;

		m_map->m_game->PlayNewMusic(m_map->m_game->m_victoryMusic);
		m_map->m_game->StartDialogue(request);

		return;
	}

	DialogueRequest request;

	if (pokemonLeft == 6)
	{
		request.m_lines.push_back("Emolga \"fainted\"");
		request.m_lines.push_back("Squirrel: Is-");
		request.m_lines.push_back("Squirrel: Is that a GUN!?");
	}
	else if (pokemonLeft == 5)
	{
		request.m_lines.push_back("Pachirisu \"fainted\"");
		request.m_lines.push_back("Squirrel: No matter! Me and my Pokemon fight as one!");
	}
	else if (pokemonLeft == 4)
	{
		request.m_lines.push_back("Kricketune \"fainted\"");
		request.m_lines.push_back("Squirrel: We're barely getting started!");
	}
	else if (pokemonLeft == 3)
	{
		request.m_lines.push_back("Alakazam \"fainted\"");
		request.m_lines.push_back("Squirrel: Only two left... but I can still win!");
	}
	else if (pokemonLeft == 2)
	{
		request.m_lines.push_back("Porygon-Z \"fainted\"");
		request.m_lines.push_back("Squirrel: One left...");
		m_map->m_game->PlayNewMusic(m_map->m_game->m_lastPokemonMusic);
	}
	else if (pokemonLeft == 1)
	{
		request.m_lines.push_back("Squirrel: I can't lose...");
		request.m_lines.push_back("Squirrel: I won't lose!");
		request.m_lines.push_back("Squirrel: Give me all you got Butler!");
		m_map->m_game->PlayNewMusic(m_map->m_game->m_lastPokemonMusic);
	}

	if (!request.m_lines.empty())
	{
		request.m_changeMapAfterDialogue = false;
		m_map->m_game->StartDialogue(request);
	}

	PokemonSpawnInfo const& info = m_party[m_activeIndex];

	Vec3 spawnPosition = info.m_position;

	Actor* playerActor = nullptr;
	if (m_map->m_player != nullptr)
	{
		playerActor = m_map->m_player->GetActor();
	}

	if (playerActor != nullptr)
	{
		Vec2 playerXY(playerActor->m_position.x, playerActor->m_position.y);
		Vec2 spawnXY(spawnPosition.x, spawnPosition.y);

		float distSq = (playerXY - spawnXY).GetLengthSquared();
		float minDist = 5.f;

		if (distSq < minDist * minDist)
		{
			spawnPosition = Vec3(25.5f, 16.f, 0.f);
		}
	}

	m_activePokemon = m_map->SpawnPokemonOBJ(
		info.m_objPath,
		info.m_bodyMaterialName,
		info.m_bodyTexturePath,
		info.m_eyeMaterialName,
		info.m_eyeTexturePath,
		spawnPosition,
		info.m_orientation
	);

	if (m_activePokemon != nullptr)
	{
		m_activePokemon->m_displayName = info.m_name;

		m_movementTimer = 0.f;
		m_porygonTargetPos = m_activePokemon->m_position;
		m_chatotMoveDir = 1.f;

		m_activePokemon->m_maxHealth = info.m_maxHealth;
		m_activePokemon->m_health = m_activePokemon->m_maxHealth;
		m_activePokemon->m_corpseLifetime = 0.5f;
		m_activePokemon->m_canBePossessed = false;
		m_activePokemon->m_visible = true;

		m_activePokemon->m_physicsRadius = 0.4f;
		m_activePokemon->m_physicsHeight = 0.75f;

		m_activePokemon->m_collidesWithWorld = true;
		m_activePokemon->m_collidesWithActors = true;
		m_activePokemon->m_simulated = false;

		if (m_activePokemon->m_displayName == "Porygon-Z")
		{
			m_porygonDiveActive = false;
			m_porygonDiveTimer = 2.5f;
			m_porygonDiveCooldown = 3.5f;
		}
		if (m_activePokemon->m_displayName == "Gengar" || m_activePokemon->m_displayName == "Mega Gengar")
		{
			m_porygonDiveActive = false;
			m_porygonDiveTimer = 2.5f;
			m_porygonDiveCooldown = 3.5f;

			m_gengarAttackState = GengarAttackState::None;
			m_gengarAttackType = GengarAttackType::None;
			m_gengarAttackTimer = 1.5f;
			m_gengarStateTimer = 0.f;
			m_gengarShotTimer = 0.f;
			m_gengarNextRingIndex = 0;
			m_gengarRingPositions.clear();
		}

		WeaponDefinition const* weaponDef = nullptr;

		if (info.m_weaponName != nullptr)
		{
			weaponDef = WeaponDefinition::GetByName(info.m_weaponName);
		}

		if (weaponDef != nullptr)
		{
			m_activePokemon->m_weapons.push_back(new Weapon(weaponDef));
			m_activePokemon->m_equippedWeaponIndex = 0;
		}
	}
}

void PokemonParty::NotifyActorDeleted(Prop* prop)
{
	if (prop == nullptr)
	{
		return;
	}

	if (prop == m_activePokemon)
	{
		m_gengarAttackState = GengarAttackState::None;
		m_gengarAttackType = GengarAttackType::None;
		m_gengarRingPositions.clear();

		m_activePokemon = nullptr;
		SpawnNextPokemon();
	}
}

void PokemonParty::UpdatePorygonDiveAttack(Actor* playerActor)
{
	if (m_activePokemon == nullptr || playerActor == nullptr)
	{
		return;
	}

	float dt = (float)m_map->m_game->m_gameClock->GetDeltaSeconds();
	m_porygonDiveTimer -= dt;

	if (!m_porygonDiveActive && m_porygonDiveTimer <= 0.f)
	{
		m_porygonDiveActive = true;
		m_porygonDiveTarget = Vec3(m_activePokemon->m_position.x, m_activePokemon->m_position.y, 0.f);
		m_porygonDiveTarget.z = 0.15f;
	}

	if (m_porygonDiveActive)
	{
		Vec3 toTarget = m_porygonDiveTarget - m_activePokemon->m_position;

		if (toTarget.GetLengthSquared() > 0.1f)
		{
			Vec3 dir = toTarget.GetNormalized();
			m_activePokemon->m_position += dir * 8.f * dt;
		}
		else
		{
			SpawnPorygonProjectileCircle();

			m_porygonDiveActive = false;
			m_porygonDiveTimer = m_porygonDiveCooldown;

			m_porygonTargetPos = Vec3(7.f, 16.f, 3.f);
		}
	}
}

void PokemonParty::SpawnPorygonProjectileCircle()
{
	if (m_map == nullptr || m_activePokemon == nullptr)
	{
		return;
	}

	//Shockwave at center 
	int projectileCount = 24;
	float speed = 8.f;
	float spawnRadius = m_activePokemon->m_physicsRadius + 0.75f;

	for (int i = 0; i < projectileCount; ++i)
	{
		float angle = ((float)i / (float)projectileCount) * 360.f;

		Vec3 dir(
			CosDegrees(angle),
			SinDegrees(angle),
			0.f
		);

		SpawnInfo spawn;
		spawn.m_actor = "Electro Ball 2";
		spawn.m_faction = m_activePokemon->m_faction;
		spawn.m_position =
			m_activePokemon->m_position +
			Vec3(0.f, 0.f, 0.05f) +
			dir * spawnRadius;

		spawn.m_orientation = EulerAngles(angle, 0.f, 0.f);

		Actor* projectile = m_map->SpawnProjectile(
			spawn,
			m_activePokemon->m_handle,
			dir * speed
		);

		if (projectile != nullptr)
		{
			projectile->m_owner = m_activePokemon->m_handle;
			projectile->m_canBeHitByRaycast = false;
		}
	}

	projectileCount = 32;
	speed = 8.f;
	spawnRadius = m_activePokemon->m_physicsRadius + 1.5f;

	for (int i = 0; i < projectileCount; ++i)
	{
		float angle = ((float)i / (float)projectileCount) * 360.f;

		Vec3 dir(
			CosDegrees(angle),
			SinDegrees(angle),
			0.f
		);

		SpawnInfo spawn;
		spawn.m_actor = "Electro Ball 2";
		spawn.m_faction = m_activePokemon->m_faction;
		spawn.m_position =
			m_activePokemon->m_position +
			Vec3(0.f, 0.f, 0.05f) +
			dir * spawnRadius;

		spawn.m_orientation = EulerAngles(angle, 0.f, 0.f);

		Actor* projectile = m_map->SpawnProjectile(
			spawn,
			m_activePokemon->m_handle,
			dir * speed
		);

		if (projectile != nullptr)
		{
			projectile->m_owner = m_activePokemon->m_handle;
			projectile->m_canBeHitByRaycast = false;
		}
	}
}

void PokemonParty::UpdateGengarAttack(Actor* playerActor)
{
	if (m_activePokemon == nullptr || playerActor == nullptr)
	{
		return;
	}

	float dt = (float)m_map->m_game->m_gameClock->GetDeltaSeconds();

	if (m_gengarAttackState == GengarAttackState::None)
	{
		m_gengarAttackTimer -= dt;

		if (m_gengarAttackTimer <= 0.f)
		{
			if (m_activePokemon->m_displayName == "Mega Gengar")
			{
				int roll = g_rng->RollRandomIntInRange(0, 1);

				if (roll == 0)
				{
					m_gengarAttackType = GengarAttackType::RingAttack;
				}
				else
				{
					m_gengarAttackType = GengarAttackType::ShadowBallStorm;
				}
			}
			else
			{
				m_gengarAttackType = GengarAttackType::RingAttack;
			}

			StartGengarAttack(playerActor);
		}

		return;
	}

	if (m_gengarAttackState == GengarAttackState::Sinking)
	{
		m_gengarStateTimer -= dt;

		float t = 1.f - (m_gengarStateTimer / 0.6f);
		t = GetClamped(t, 0.f, 1.f);

		if (m_gengarAttackType == GengarAttackType::RingAttack)
		{
			m_activePokemon->m_position.z =
				Interpolate(m_gengarSinkStartPos.z, -1.0f, t);
		}
		else if (m_gengarAttackType == GengarAttackType::ShadowBallStorm)
		{
			m_activePokemon->m_position.z =
				Interpolate(m_gengarSinkStartPos.z, 5.0f, t);
		}

		if (m_gengarStateTimer <= 0.f)
		{
			if (m_gengarAttackType == GengarAttackType::RingAttack)
			{
				SpawnGengarRings(playerActor);
				m_gengarAttackState = GengarAttackState::Warning;
				m_gengarStateTimer = 1.0f;
			}
			else
			{
				m_gengarAttackState = GengarAttackState::Warning;
				m_gengarStateTimer = 0.5f;
			}
		}

		return;
	}

	if (m_gengarAttackState == GengarAttackState::Warning)
	{
		m_gengarStateTimer -= dt;

		if (m_gengarStateTimer <= 0.f)
		{
			m_gengarAttackState = GengarAttackState::SpawningProjectiles;

			if (m_gengarAttackType == GengarAttackType::RingAttack)
			{
				m_gengarStateTimer = 0.08f;
				m_gengarNextRingIndex = 0;
			}
			else
			{
				m_gengarStateTimer = m_gengarStormDuration;
				m_gengarShotTimer = 0.f;
			}
		}

		return;
	}

	if (m_gengarAttackState == GengarAttackState::SpawningProjectiles)
	{
		m_gengarStateTimer -= dt;

		if (m_gengarAttackType == GengarAttackType::RingAttack)
		{
			if (m_gengarStateTimer <= 0.f)
			{
				if (m_gengarNextRingIndex < (int)m_gengarRingPositions.size())
				{
					SpawnGengarShadowBall(m_gengarRingPositions[m_gengarNextRingIndex]);
					m_gengarNextRingIndex++;
					m_gengarStateTimer = 0.025f;
				}
				else
				{
					m_gengarRiseTargetPos = Vec3(
						g_rng->RollRandomFloatInRange(7.f, 20.f),
						g_rng->RollRandomFloatInRange(13.f, 20.f),
						0.f
					);

					m_activePokemon->m_position = m_gengarRiseTargetPos;
					m_activePokemon->m_position.z = -1.f;

					m_gengarAttackState = GengarAttackState::Rising;
					m_gengarStateTimer = 0.6f;
				}
			}
		}
		else if (m_gengarAttackType == GengarAttackType::ShadowBallStorm)
		{
			m_gengarShotTimer -= dt;

			if (m_gengarShotTimer <= 0.f)
			{
				for (int i = 0; i < 64; ++i)
				{
					SpawnShadowBallStorm(playerActor);
				}

				m_gengarShotTimer = 0.08f;
			}

			if (m_gengarStateTimer <= 0.f)
			{
				m_gengarAttackState = GengarAttackState::Rising;
				m_gengarStateTimer = 0.6f;
			}
		}

		return;
	}

	if (m_gengarAttackState == GengarAttackState::Rising)
	{
		m_gengarStateTimer -= dt;

		float t = 1.f - (m_gengarStateTimer / 0.6f);
		t = GetClamped(t, 0.f, 1.f);

		if (m_gengarAttackType == GengarAttackType::RingAttack)
		{
			m_activePokemon->m_position.z = Interpolate(-1.0f, 0.f, t);
		}
		else
		{
			m_activePokemon->m_position.z = Interpolate(5.0f, m_gengarSinkStartPos.z, t);
		}

		if (m_gengarStateTimer <= 0.f)
		{
			m_activePokemon->m_position.z = 0.f;
			m_activePokemon->m_ignoreFloorCollision = false;
			m_activePokemon->m_collidesWithActors = true;

			m_gengarAttackState = GengarAttackState::None;
			m_gengarAttackType = GengarAttackType::None;
			m_gengarAttackTimer = 2.0f;
			m_gengarRingPositions.clear();
		}

		return;
	}
}

void PokemonParty::StartGengarAttack([[maybe_unused]]Actor* playerActor)
{
	m_gengarSinkStartPos = m_activePokemon->m_position;

	m_gengarAttackState = GengarAttackState::Sinking;
	m_gengarStateTimer = 0.6f;

	m_activePokemon->m_velocity = Vec3::ZERO;
	m_activePokemon->m_acceleration = Vec3::ZERO;

	if (m_gengarAttackType == GengarAttackType::RingAttack)
	{
		m_activePokemon->m_ignoreFloorCollision = true;
		m_activePokemon->m_collidesWithActors = false;
	}
}

void PokemonParty::SpawnGengarRings([[maybe_unused]]Actor* playerActor)
{
	m_gengarRingPositions.clear();

	int ringCount = 32;
	float minDistBetweenRings = m_gengarRingRadius * 2.25f;

	for (int i = 0; i < ringCount; ++i)
	{
		for (int attempt = 0; attempt < 100; ++attempt)
		{
			Vec3 pos;
			pos.x = g_rng->RollRandomFloatInRange(4.f, 28.f);
			pos.y = g_rng->RollRandomFloatInRange(4.f, 28.f);
			pos.z = 0.02f;

			bool overlapsExistingRing = false;

			for (Vec3 const& existingPos : m_gengarRingPositions)
			{
				Vec2 a(pos.x, pos.y);
				Vec2 b(existingPos.x, existingPos.y);

				if ((a - b).GetLengthSquared() < minDistBetweenRings * minDistBetweenRings)
				{
					overlapsExistingRing = true;
					break;
				}
			}

			if (!overlapsExistingRing)
			{
				m_gengarRingPositions.push_back(pos);
				break;
			}
		}
	}
}

void PokemonParty::SpawnGengarShadowBall(Vec3 const& ringPos)
{
	if (m_activePokemon == nullptr || m_map == nullptr)
	{
		return;
	}

	int numShadowBallRings = 6;
	float minRadius = 0.35f;

	for (int i = 0; i < numShadowBallRings; ++i)
	{
		float ringFraction = (float)i / (float)(numShadowBallRings - 1);

		float curvedFraction = SmoothStep3(ringFraction);

		float radius = RangeMap(
			curvedFraction,
			0.f, 1.f,
			m_gengarRingRadius,
			minRadius
		);

		float projectileSpacing = 0.85f;
		float circumference = 2.f * 3.14159265359f * radius;

		int ballsThisRing = (int)(circumference / projectileSpacing);

		ballsThisRing = (int)GetClamped(
			(float)ballsThisRing,
			3.f,
			(float)m_shadowBallsPerRing
		);

		for (int j = 0; j < ballsThisRing; ++j)
		{
			float angle = ((float)j / (float)ballsThisRing) * 360.f;

			angle += (float)i * 360.f / ((float)ballsThisRing * 2.f);
			angle += (float)i * 23.f;

			Vec3 offset(
				CosDegrees(angle),
				SinDegrees(angle),
				0.f);

			SpawnInfo spawn;
			spawn.m_actor = "Shadow Ball 2";
			spawn.m_faction = m_activePokemon->m_faction;
			spawn.m_position = ringPos + offset * radius;
			spawn.m_position.z = 0.05f;
			spawn.m_orientation = EulerAngles(0.f, -90.f, 0.f);

			Actor* projectile = m_map->SpawnProjectile(
				spawn,
				m_activePokemon->m_handle,
				Vec3(0.f, 0.f, 8.f)
			);

			if (projectile != nullptr)
			{
				projectile->m_owner = m_activePokemon->m_handle;
				projectile->m_canBeHitByRaycast = false;
				projectile->m_destroyAboveZ = 8.f;
				projectile->m_collidesWithWorld = false;
			}
		}
	}
}

void PokemonParty::RenderGengarAttackWarning() const
{
	if (m_gengarAttackState == GengarAttackState::None)
	{
		return;
	}

	if (m_gengarRingPositions.empty())
	{
		return;
	}

	std::vector<Vertex> verts;

	for (Vec3 const& pos : m_gengarRingPositions)
	{
		Vec2 center(pos.x, pos.y);

		AddVertsForRing2D(
			verts,
			center,
			m_gengarRingRadius - 0.16f,
			m_gengarRingRadius,
			Rgba8(120, 0, 200, 180),
			64
		);

		AddVertsForRing2D(
			verts,
			center,
			m_gengarRingRadius * 0.65f - 0.8f,
			m_gengarRingRadius * 0.65f,
			Rgba8(255, 0, 255, 120),
			64
		);
	}

	g_engine->m_render->BindTexture(nullptr);
	g_engine->m_render->DrawVertexArray((int)verts.size(), verts.data());
}

void PokemonParty::SpawnShadowBallStorm(Actor* playerActor)
{
	if (m_map == nullptr || m_activePokemon == nullptr)
	{
		return;
	}

	Vec3 spawnPos = m_activePokemon->m_position;
	spawnPos.z += m_activePokemon->m_physicsHeight + 0.5f;

	Vec3 dir;

	bool aimAtPlayer = playerActor != nullptr && g_rng->RollRandomFloatZeroToOne() < 0.65f;

	if (aimAtPlayer)
	{
		Vec3 targetPos = playerActor->m_position;
		targetPos.z += playerActor->m_physicsHeight * 0.5f;

		dir = (targetPos - spawnPos).GetNormalized();

		dir.x += g_rng->RollRandomFloatInRange(-0.10f, 0.10f);
		dir.y += g_rng->RollRandomFloatInRange(-0.10f, 0.10f);
		dir.z += g_rng->RollRandomFloatInRange(-0.05f, 0.05f);
	}
	else
	{
		float yaw = g_rng->RollRandomFloatInRange(0.f, 360.f);
		float pitch = g_rng->RollRandomFloatInRange(-35.f, 10.f);

		dir = Vec3(
			CosDegrees(yaw) * CosDegrees(pitch),
			SinDegrees(yaw) * CosDegrees(pitch),
			SinDegrees(pitch)
		);
	}

	dir = dir.GetNormalized();

	SpawnInfo spawn;
	spawn.m_actor = "Electro Ball 2";
	spawn.m_faction = m_activePokemon->m_faction;
	spawn.m_position = spawnPos + dir * (m_activePokemon->m_physicsRadius + 1.0f);
	spawn.m_orientation = EulerAngles(Atan2Degrees(dir.y, dir.x), 0.f, 0.f);

	Actor* projectile = m_map->SpawnProjectile(
		spawn,
		m_activePokemon->m_handle,
		dir * g_rng->RollRandomFloatInRange(10.f, 20.f)
	);

	if (projectile != nullptr)
	{
		projectile->m_owner = m_activePokemon->m_handle;
		projectile->m_canBeHitByRaycast = false;
		projectile->m_collidesWithWorld = true;
	}
}