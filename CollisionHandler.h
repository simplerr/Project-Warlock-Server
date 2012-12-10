#pragma once

class Player;
class Projectile;
class Server;
class ItemLoaderXML;

class CollisionHandler
{
public:
	CollisionHandler() {};
	~CollisionHandler() {};

	void HandleCollision(Player* pPlayer, Projectile* pProjetile, Server* pServer, ItemLoaderXML* pItemLoader, float baseImpulse);
private:
};