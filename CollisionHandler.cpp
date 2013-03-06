#include "CollisionHandler.h"
#include "Player.h"
#include "Projectile.h"
#include "Skills.h"
#include "World.h"
#include "Server.h"
#include "ItemLoaderXML.h"
#include "ServerArena.h"
#include "FreezeEffect.h"

void CollisionHandler::HandleCollision(Player* pPlayer, Projectile* pProjectile, ServerArena* pArena, ItemLoaderXML* pItemLoader, float baseImpulse)
{
	if(pProjectile->GetSkillType() == SKILL_FIREBALL)
	{
		// Add a "impulse" to the player.
		XMFLOAT3 dir = pProjectile->GetDirection();
		float impulse = baseImpulse;
		pPlayer->SetVelocity(dir * impulse);

		// Get item data.
		Item* item = pItemLoader->GetItem(ItemKey(pProjectile->GetSkillType(), pProjectile->GetSkillLevel()));

		// Damage the player.
		pPlayer->TakeDamage(item->GetAttributes().damage);
		pPlayer->SetLastHitter((Player*)pProjectile->GetWorld()->GetObjectById(pProjectile->GetOwner()));

		// Dead? [NOTE] A bit of a hack.
		if(pPlayer->GetCurrentHealth() <= 0) 
			pArena->PlayerEliminated(pPlayer, (Player*)pProjectile->GetWorld()->GetObjectById(pProjectile->GetOwner()));
	}
	else if(pProjectile->GetSkillType() == SKILL_FROSTNOVA)
	{
		// Add a "impulse" to the player.
		XMFLOAT3 dir = pProjectile->GetDirection();
		float impulse = baseImpulse;
		pPlayer->SetVelocity(dir * impulse);

		// Get item data.
		Item* item = pItemLoader->GetItem(ItemKey(pProjectile->GetSkillType(), pProjectile->GetSkillLevel()));

		// Damage the player.
		pPlayer->TakeDamage(item->GetAttributes().damage);
		pPlayer->SetLastHitter((Player*)pProjectile->GetWorld()->GetObjectById(pProjectile->GetOwner()));
		pPlayer->AddStatusEffect(new FreezeEffect());

		// Dead? [NOTE] A bit of a hack.
		if(pPlayer->GetCurrentHealth() <= 0) 
			pArena->PlayerEliminated(pPlayer, (Player*)pProjectile->GetWorld()->GetObjectById(pProjectile->GetOwner()));
	}
}