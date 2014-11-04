#include "Minion.h"
#include "MinionStats.h"
#include "Map.h"
#include "Game.h"
#include "Pathfinder.h"
#include "Logger.h"

using namespace std;

Minion::Minion(Map* map, uint32 id, MinionSpawnType type, MinionSpawnPosition position, const vector<MovementVector>& constWaypoints) : Unit(map, id, "", new MinionStats(), 40, 0, 0, 1100), type(type), spawnPosition(position), constWaypoints(constWaypoints), curConstWaypoint(0) {
   switch (spawnPosition) {
   case SPAWN_BLUE_TOP:
      setSide(0);
      setPosition(907, 1715);
      break;
   case SPAWN_BLUE_BOT:
      setSide(0);
      setPosition(1533, 1321);
      break;
   case SPAWN_BLUE_MID:
      setSide(0);
      setPosition(1443, 1663);
      break;
   case SPAWN_RED_TOP:
      setSide(1);
      setPosition(14455, 13159);
      break;
   case SPAWN_RED_BOT:
      setSide(1);
      setPosition(12967, 12695);
      break;
   case SPAWN_RED_MID:
      setSide(1);
      setPosition(12433, 12623);
      break;
   }
   
   // TODO : make these data dynamic with the game elapsed time
   switch(type) {
   case MINION_TYPE_MELEE:
      stats->setCurrentHealth(475.0f + ((20.f)*(int)(map->getGameTime() / (180 * 1000000))));
      stats->setMaxHealth(475.0f + ((20.f)*(int)(map->getGameTime() / (180 * 1000000))));
      stats->setBaseAd(12.0f + ((1.f)*(int)(map->getGameTime() / (180 * 1000000))));
      stats->setRange(180.f);
      stats->setBaseAttackSpeed(1.250f);
      autoAttackDelay = 11.8f/30.f;
      setMelee(true);
      break;
   case MINION_TYPE_CASTER:
      stats->setCurrentHealth(279.0f + ((7.5f)*(int)(map->getGameTime() / (90 * 1000000))));
      stats->setMaxHealth(279.0f + ((7.5f)*(int)(map->getGameTime() / (90 * 1000000))));
      stats->setBaseAd(23.0f + ((1.f)*(int)(map->getGameTime() / (90 * 1000000))));
      stats->setRange(600.f);
      stats->setBaseAttackSpeed(0.670f);
      autoAttackDelay = 14.1f/30.f;
      autoAttackProjectileSpeed = 650.f;
      break;
   case MINION_TYPE_CANNON:
      stats->setCurrentHealth(700.0f + ((27.f)*(int)(map->getGameTime() / (180 * 1000000))));
      stats->setMaxHealth(700.0f + ((27.f)*(int)(map->getGameTime() / (180 * 1000000))));
      stats->setBaseAd(40.0f + ((3.f)*(int)(map->getGameTime() / (180 * 1000000))));
      stats->setRange(450.f);
      stats->setBaseAttackSpeed(1.0f);
      autoAttackDelay = 9.f/30.f;
      autoAttackProjectileSpeed = 1200.f;
      break;
   }
   
   std::string minionModel;
   if (position == SPAWN_BLUE_TOP ||
       position == SPAWN_BLUE_MID ||
       position == SPAWN_BLUE_BOT) {
       minionModel += "Blue_Minion_";
   } else {
       minionModel += "Red_Minion_";
   }
   
   if (type == MINION_TYPE_MELEE) {
       minionModel += "Basic";
   } else if (type == MINION_TYPE_CASTER) {
       minionModel += "Wizard"; 
   } else {
       minionModel += "MechCannon";
   }
   
   setModel(minionModel);
   
   stats->setMovementSpeed(325.f);
   stats->setBaseAttackSpeed(0.625f);
   stats->setAttackSpeedMultiplier(1.0f);
   
   vector<MovementVector> newWaypoints;
   
   if(constWaypoints.size() > 0) {
      newWaypoints = { constWaypoints[0], constWaypoints[0] };
   } else {
      newWaypoints = { MovementVector(x, y), MovementVector(x, y) };
   }
   
   setWaypoints(newWaypoints);
   setMoveOrder(MOVE_ORDER_ATTACKMOVE);
}

void Minion::update(int64 diff) {
   Unit::update(diff);

   if (isDead()) {
      return;
   }

   if (moveOrder == MOVE_ORDER_ATTACKMOVE && !unitTarget) {
      const std::map<uint32, Object*>& objects = map->getObjects();
      Unit* nextTarget = 0;
      unsigned int nextTargetPriority = 10;

      for (auto& it : objects) {
         Unit* u = dynamic_cast<Unit*> (it.second);

         if (!u || u->isDead() || u->getSide() == getSide() || distanceWith(u) > DETECT_RANGE || !getMap()->teamHasVisionOn(getSide(), u)) {
            continue;
         }

         auto priority = classifyTarget(u);
         if (priority < nextTargetPriority) {
            nextTarget = u;
            nextTargetPriority = priority;
         }
      }
      if (nextTarget) {
         setUnitTarget(nextTarget); // Set the new target and refresh waypoints
         map->getGame()->notifySetTarget(this, nextTarget);
      }
   }
   
   if(autoAttackFlag && (!unitTarget || distanceWith(unitTarget) > stats->getRange())) {
      map->getGame()->notifyStopAutoAttack(this);
      autoAttackFlag = false;
   }
   
   if(unitTarget) {
      Champion* c = dynamic_cast<Champion*>(unitTarget);
      
      if (c && !autoAttackFlag) {
         const std::map<uint32, Object*>& objects = map->getObjects();
         Unit* nextTarget = 0;
         unsigned int nextTargetPriority = 10;
         for(auto& it : objects) {
            Unit* u = dynamic_cast<Unit*>(it.second);

            if(!u || u->isDead() || u->getSide() == getSide() || distanceWith(u) > stats->getRange()) {
               continue;
            }
            
            // Find the next champion in range targeting an enemy champion who is also in range
            Champion* nextChampion = dynamic_cast<Champion*>(u);
            if (nextChampion && c != nextChampion && nextChampion->getUnitTarget() != 0) {
               Champion* target = dynamic_cast<Champion*>(nextChampion->getUnitTarget());
               if (target && nextChampion->distanceWith(target) <= nextChampion->getStats().getRange() && distanceWith(target) <= stats->getRange()) {
                  nextTarget = nextChampion; // No priority required
                  break;
               }
            }
            
            auto priority = classifyTarget(u);
            if (priority < nextTargetPriority) {
               nextTarget = u;
               nextTargetPriority = priority;
            }
         }
         if (nextTarget) {
            setUnitTarget(nextTarget); // Set the new target and refresh waypoints
            map->getGame()->notifySetTarget(this, nextTarget);
         }
      }
      
      return;
   }
   
   // Minion reached its temporary destination
   if((waypoints.size() == 1) || (curWaypoint == 2 && ++curConstWaypoint < constWaypoints.size())) {
      //CORE_INFO("Minion reached ! Going to %d;%d", constWaypoints[curConstWaypoint].x, constWaypoints[curConstWaypoint].y);
      vector<MovementVector> newWaypoints = { MovementVector(x, y), constWaypoints[curConstWaypoint] };
      setWaypoints(newWaypoints);
   }
}

void Minion::onCollision(Object * a_Collider)
{
   if (a_Collider == 0) return;

   {
      Target *t = getTarget();
      Vector2 targetpos;
      if (t)
      {
         CORE_INFO("Pathing to target. (%f,%f)->(%f,%f)", getPosition().x, getPosition().y, t->getPosition().x, t->getPosition().y);
         targetpos = t->getPosition();
      }
      else
      {
         targetpos = (a_Collider->getPosition() - getPosition()).Normalize(); // direction
         targetpos *= a_Collider->getCollisionRadius() + getCollisionRadius(); // direction with length, displacement
         targetpos = getPosition() - targetpos; // position + difference = endpos, but we need to go the exact other way
         CORE_INFO("Pathing away from minion. (%f,%f)->(%f,%f)", getPosition().x, getPosition().y, targetpos.x, targetpos.y);
      }

      Path new_path = Pathfinder::getPath(getPosition(), targetpos);
		setWaypoints(new_path.getWaypoints());
   }
}