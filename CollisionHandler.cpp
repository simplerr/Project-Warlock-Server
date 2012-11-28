#include "CollisionHandler.h"
#include "Player.h"
#include "Projectile.h"
#include "Skills.h"

void CollisionHandler::HandleCollision(Player* pPlayer, Projectile* pProjetile)
{
	if(pProjetile->GetSkillType() == SKILL_FIREBALL)
	{
		// Add a "impulse" to the player.
		XMFLOAT3 dir = pProjetile->GetDirection();
		float impulse = 0.1f;
		pPlayer->SetVelocity(dir * impulse);

		// Damage the player.
		pPlayer->SetHealth(pPlayer->GetHealth() - 10.0f);
	}
}