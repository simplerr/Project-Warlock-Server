#pragma once

class Player;
class Projectile;

class CollisionHandler
{
public:
	CollisionHandler() {};
	~CollisionHandler() {};

	void HandleCollision(Player* pPlayer, Projectile* pProjetile);
private:
};