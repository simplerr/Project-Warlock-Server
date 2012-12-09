#include "CollisionHandler.h"
#include "Player.h"
#include "Projectile.h"
#include "Skills.h"

void CollisionHandler::HandleCollision(Player* pPlayer, Projectile* pProjetile, float baseImpulse)
{
	if(pProjetile->GetSkillType() == SKILL_FIREBALL)
	{
		// Add a "impulse" to the player.
		XMFLOAT3 dir = pProjetile->GetDirection();
		float impulse = baseImpulse;
		pPlayer->SetVelocity(dir * impulse);

		// Damage the player.
		pPlayer->SetHealth(pPlayer->GetHealth() - 30.0f);
	}
}