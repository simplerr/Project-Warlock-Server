#pragma once

class Player;
class Projectile;
class ServerArena;
class ItemLoaderXML;

class CollisionHandler
{
public:
	CollisionHandler() {};
	~CollisionHandler() {};

	void HandleCollision(Player* pPlayer, Projectile* pProjetile, ServerArena* pArena, ItemLoaderXML* pItemLoader, float baseImpulse);
private:
};