/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Dune Legacy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Dune Legacy.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <players/QuantBot.h>

#include <Game.h>
#include <GameInitSettings.h>
#include <Map.h>
#include <sand.h>
#include <House.h>

#include <structures/StructureBase.h>
#include <structures/BuilderBase.h>
#include <structures/StarPort.h>
#include <structures/ConstructionYard.h>
#include <structures/RepairYard.h>
#include <structures/Palace.h>
#include <units/UnitBase.h>
#include <units/GroundUnit.h>
#include <units/MCV.h>
#include <units/Harvester.h>
#include <units/Saboteur.h>
#include <units/Devastator.h>

#include <algorithm>
#include <string>

#define AIUPDATEINTERVAL 50



/**
    TODO

 New list from May 2016
 - units should move at start
 - fix single player campaign crash
 - fix unit allocation bug - atredes only building light tanks


 == Building Placement ==


    ia) build concrete when no placement locations are available == in progress, bugs exist ==
    iii) increase favourability of being near other buildings == 50% done ==

    1. Refinerys near spice == tried but failed ==
    4. Repair yards factories, & Turrets near enemy == 50% done ==
    5. All buildings away from enemy other that silos and turrets


    == buildings ==
    i) stop repair when just on yellow (at 50%) == 50% done, still broken for some buildings as goes into yellow health ==
    ii) silo build broken == fixed ==


    building algo still leaving gaps
    increase alignment score when sides match

    == Units ==
    ii) units that get stuck in buildings should be transported to squadcenter =%80=
    vii) fix attack timer =%80=
    viii) when attack timer exceeds a certain value then all fing units are set to area guard

    2) harvester return distance bug been introduced.= in progress ==

    3) carryalls sit over units hovering bug introduced.... fix scramble units and defend + manual carryall = 50% =

    4) theres a bug in on increment and decrement units...

    5) turn off force move to rally point after attacked = 50% =
    6) reduce turret building when lacking a military = 50% =

    7) remove turrets from nuke target calculation =50%=
    8) adjust turret placement algo to include points for proximitry to base centre =50%=



    1. Harvesters deploy away from enemy
    5. fix gun turret & gun for rocket turret

    x. Improve squad management

    == New work ==
    1. Add them with some logic =50%=
    2. fix force ratio optimisation algorithm,
        need to make it based off kill / death ratio instead of just losses =50%=
    3. create a retreate mechanism = 50% = still need to add retreat timer, say 1 retreat per minute, max
        - fix rally point and ybut deploy logic


    2. Make carryalls and ornithopers easier to hit

    ====> FIX WORM CRASH GAME BUG

**/


QuantBot::QuantBot(House* associatedHouse, std::string playername, Uint32 difficulty)
: Player(associatedHouse, playername), difficulty(difficulty) {

    buildTimer = getRandomGen().rand(0,3) * 50;

    attackTimer = MILLI2CYCLES(10000);
    retreatTimer = MILLI2CYCLES(60000);

    // Different AI logic for Campaign. Assumption is if player is loading they are playing a campaign game
    if((currentGame->gameType == GAMETYPE_CAMPAIGN) || (currentGame->gameType == GAMETYPE_LOAD_SAVEGAME) || (currentGame->gameType == GAMETYPE_SKIRMISH)) {
        gameMode = CAMPAIGN;
    }else{
        gameMode = CUSTOM;
    }


    if(gameMode == CAMPAIGN){
        // Wait a while if it is a campaign game

        switch(currentGame->techLevel){
            case 6: {
                attackTimer = MILLI2CYCLES(540000);
            }break;

            case 7: {
                attackTimer = MILLI2CYCLES(600000);
            }break;

            case 8: {
                attackTimer = MILLI2CYCLES(720000);
            }break;

            default: {
                attackTimer = MILLI2CYCLES(480000);
            }

        }
    }

    initialMilitaryValue = 0;
    militaryValueLimit = 0;
    harvesterLimit = 4;
    campaignAIAttackFlag = false;
}


QuantBot::QuantBot(InputStream& stream, House* associatedHouse) : Player(stream, associatedHouse) {
    QuantBot::init();

    difficulty = stream.readUint32();
    gameMode = stream.readUint32();
    buildTimer = stream.readSint32();
    attackTimer = stream.readSint32();
    retreatTimer = stream.readSint32();

    for (Uint32 i = ItemID_FirstID; i <= Structure_LastID; i++ ){
       initialItemCount[i] = stream.readUint32();
    }
    initialMilitaryValue = stream.readSint32();
    militaryValueLimit = stream.readSint32();
    harvesterLimit = stream.readSint32();
    campaignAIAttackFlag = stream.readBool();

    squadRallyLocation.x = stream.readSint32();
    squadRallyLocation.y = stream.readSint32();
    squadRetreatLocation.x = stream.readSint32();
    squadRetreatLocation.y = stream.readSint32();

    // Need to add in a building array for when people save and load
    // So that it keeps the count of buildings that should be on the map.
    Uint32 NumPlaceLocations = stream.readUint32();
    for(Uint32 i = 0; i < NumPlaceLocations; i++) {
        Sint32 x = stream.readSint32();
        Sint32 y = stream.readSint32();

        placeLocations.push_back(Coord(x,y));
    }
}


void QuantBot::init() {
}


QuantBot::~QuantBot() {
}

void QuantBot::save(OutputStream& stream) const {
    Player::save(stream);

    stream.writeUint32(difficulty);
    stream.writeUint32(gameMode);
    stream.writeSint32(buildTimer);
    stream.writeSint32(attackTimer);
    stream.writeSint32(retreatTimer);

    for (Uint32 i = ItemID_FirstID; i <= Structure_LastID; i++ ){
        stream.writeUint32(initialItemCount[i]);
    }
    stream.writeSint32(initialMilitaryValue);
    stream.writeSint32(militaryValueLimit);
    stream.writeSint32(harvesterLimit);
    stream.writeBool(campaignAIAttackFlag);

    stream.writeSint32(squadRallyLocation.x);
    stream.writeSint32(squadRallyLocation.y);
    stream.writeSint32(squadRetreatLocation.x);
    stream.writeSint32(squadRetreatLocation.y);

    stream.writeUint32(placeLocations.size());
    std::list<Coord>::const_iterator iter;
    for(iter = placeLocations.begin(); iter != placeLocations.end(); ++iter) {
        stream.writeSint32(iter->x);
        stream.writeSint32(iter->y);
    }

}


void QuantBot::update() {
    if(getGameCylceCount() == 0) {
        // The game just started and we gather some
        // Count the items once initially

        // First count all the objects we have
        for (int i = ItemID_FirstID; i <= ItemID_LastID; i++ ) {
            initialItemCount[i] = getHouse()->getNumItems(i);
            logDebug("Initial: Item: %d  Count: %d", i, initialItemCount[i]);
        }

        if((initialItemCount[Structure_RepairYard] == 0) && gameMode == CAMPAIGN && currentGame->techLevel > 4) {
                initialItemCount[Structure_RepairYard] = 1;
                if(initialItemCount[Structure_Radar] == 0){
                    initialItemCount[Structure_Radar] = 1;
                }

                if(initialItemCount[Structure_LightFactory] == 0){
                    initialItemCount[Structure_LightFactory] = 1;
                }

                logDebug("Allow Campaign AI one Repair Yard");
        }

        // Calculate the total military value of the player
        initialMilitaryValue = 0;
        for(Uint32 i = Unit_FirstID; i <= Unit_LastID; i++){
            if(i != Unit_Carryall && i != Unit_Harvester){
                // Used for campaign mode.
                initialMilitaryValue += initialItemCount[i] * currentGame->objectData.data[i][getHouse()->getHouseID()].price;
            }
        }

        switch(gameMode) {
            case CAMPAIGN: {

                 switch(difficulty) {
                    case EASY: {
                        harvesterLimit = initialItemCount[Structure_Refinery];
                        if(currentGame->techLevel == 8){
                            militaryValueLimit = 4000;

                        } else {
                            militaryValueLimit = initialMilitaryValue;
                        }

                        logDebug("Easy Campaign  ");
                    } break;

                    case MEDIUM: {
                        harvesterLimit = 2 * initialItemCount[Structure_Refinery];
                        militaryValueLimit = lround(initialMilitaryValue * FixPt(1,2));
                        if(militaryValueLimit < 4000 && currentGame->techLevel == 8) {
                            militaryValueLimit = 4000;
                        }

                        logDebug("Medium Campaign  ");
                    } break;

                    case HARD: {
                        if(currentGame->techLevel == 8) {
                            harvesterLimit = 3;
                            initialItemCount[Structure_Refinery] = 2;
                            militaryValueLimit = 5000;
                        } else {
                            harvesterLimit = 2 * initialItemCount[Structure_Refinery];
                            militaryValueLimit = lround(initialMilitaryValue * FixPt(1,5));
                        }

                        logDebug("Hard Campaign  ");
                    } break;

                    case BRUTAL: {
                        harvesterLimit = (currentGameMap->getSizeX() * currentGameMap->getSizeY() / 512);
                        militaryValueLimit = 25000;

                        logDebug("Brutal Campaign  ");
                    } break;

                    case DEFEND: {
                        harvesterLimit = 2 * initialItemCount[Structure_Refinery];
                        militaryValueLimit = lround(initialMilitaryValue * FixPt(1,2));

                        logDebug("Defensive Campaign  ");
                    } break;
                }

            } break;

            case CUSTOM: {

                 switch(difficulty) {
                    case BRUTAL: {
                        harvesterLimit = 60;
                        militaryValueLimit = 100000;
                        //logDebug("BUILD BRUTAL SKIRM ");
                    } break;

                    case EASY: {
                        harvesterLimit = (currentGameMap->getSizeX() * currentGameMap->getSizeY() / 2048);
                        militaryValueLimit = 10000;
                        //logDebug("BUILD EASY SKIRM ");
                    } break;

                    case MEDIUM: {
                        harvesterLimit = (currentGameMap->getSizeX() * currentGameMap->getSizeY() / 1024);
                        militaryValueLimit = 25000;
                        //logDebug("BUILD MEDIUM SKIRM ");
                    } break;

                    case HARD: {
                        harvesterLimit = (currentGameMap->getSizeX() * currentGameMap->getSizeY() / 512);
                        militaryValueLimit = 50000;
                        //logDebug("BUILD HARD SKIRM ");
                    } break;

                    case DEFEND: {
                        harvesterLimit = (currentGameMap->getSizeX() * currentGameMap->getSizeY() / 1024);
                        militaryValueLimit = 25000;
                        //logDebug("BUILD MEDIUM SKIRM ");
                    } break;
                }

                // what is this useful for?
                if((currentGameMap->getSizeX() * currentGameMap->getSizeY() / 512) < harvesterLimit && difficulty != BRUTAL) {
                    harvesterLimit = currentGameMap->getSizeX() * currentGameMap->getSizeY() / 512;
                }

            } break;

        }
    }


    if((getGameCylceCount() + getHouse()->getHouseID()) % AIUPDATEINTERVAL != 0) {
        // we are not updating this AI player this cycle
        return;
    }

    // Calculate the total military value of the player
    int militaryValue = 0;
    for(Uint32 i = Unit_FirstID; i <= Unit_LastID; i++){
        if(i != Unit_Carryall && i != Unit_Harvester){
            militaryValue += getHouse()->getNumItems(i) * currentGame->objectData.data[i][getHouse()->getHouseID()].price;
        }
    }
    //logDebug("Military Value %d  Initial Military Value %d", militaryValue, initialMilitaryValue);

    checkAllUnits();

    if(buildTimer <= 0) {
        build(militaryValue);
    } else {
        buildTimer -= AIUPDATEINTERVAL;
    }

    if(attackTimer <= 0) {
        attack(militaryValue);
    } else if (attackTimer > MILLI2CYCLES(100000) ) {
        // If we have taken substantial losses then retreat
        attackTimer = MILLI2CYCLES(90000);

        if(retreatTimer < 0){
            retreatAllUnits();
        }
    } else {
        attackTimer -= AIUPDATEINTERVAL;
        retreatTimer -= AIUPDATEINTERVAL;
    }

    fflush(stdout);
}


void QuantBot::onIncrementStructures(int itemID) {
}


void QuantBot::onDecrementStructures(int itemID, const Coord& location) {
}


/// When we take losses we should hold off from attacking for longer...
void QuantBot::onDecrementUnits(int itemID) {
    if(itemID != Unit_Trooper && itemID != Unit_Infantry) {
        attackTimer += MILLI2CYCLES(currentGame->objectData.data[itemID][getHouse()->getHouseID()].price * 30 / (difficulty+1) );
        //logDebug("loss ");
    }
}


/// When we get kills we should re-attack sooner...
void QuantBot::onIncrementUnitKills(int itemID) {
    if(itemID != Unit_Trooper && itemID != Unit_Infantry) {
        attackTimer -= MILLI2CYCLES(currentGame->objectData.data[itemID][getHouse()->getHouseID()].price * 15);
        //logDebug("kill ");
    }
}

void QuantBot::onDamage(const ObjectBase* pObject, int damage, Uint32 damagerID) {
    const ObjectBase* pDamager = getObject(damagerID);

    if(pDamager == nullptr || pDamager->getOwner() == getHouse() || pObject->getItemID() == Unit_Sandworm) {
        return;
    }

    // If the human has attacked us then its time to start fighting back... unless its an attack on a special unit
    // Don't trigger with fremen or saboteur
    bool bPossiblyOwnFremen = (pObject->getOwner()->getHouseID() == HOUSE_ATREIDES) && (pObject->getItemID() == Unit_Trooper) && (currentGame->techLevel > 7);
    if(gameMode == CAMPAIGN && !pDamager->getOwner()->isAI() && !campaignAIAttackFlag && !bPossiblyOwnFremen && (pObject->getItemID() != Unit_Saboteur)) {
        campaignAIAttackFlag = true;
    } else if (pObject->isAStructure()) {
        doRepair(pObject);
        // no point scrambling to defend a missile
        if(pDamager->getItemID() != Structure_Palace){
            scrambleUnitsAndDefend(pDamager);
        }

    } else if(pObject->isAGroundUnit()){
        Coord squadCenterLocation = findSquadCenter(pObject->getOwner()->getHouseID());

        const GroundUnit* pUnit = dynamic_cast<const GroundUnit*>(pObject);

        if(pUnit == nullptr) {
            return;
        }

        if(pUnit->isawaitingPickup()) {
            return;
        }

        // Stop him dead in his tracks if he's going to rally point
        if(pUnit->wasForced()) {
            doMove2Pos(pUnit,
                       pUnit->getCenterPoint().x,
                       pUnit->getCenterPoint().y,
                       false);
        }

        if (pUnit->getItemID() == Unit_Harvester) {
            // Always keep Harvesters away from harm
            // Defend the harvester!
            scrambleUnitsAndDefend(pDamager);
            const Harvester* pHarvester = dynamic_cast<const Harvester*>(pUnit);
            doReturn(pHarvester);
        } else if (pUnit->getItemID() == Unit_Launcher
            || pUnit->getItemID() == Unit_Deviator) {
            // Always keep Launchers away from harm

            doSetAttackMode(pUnit, AREAGUARD);
            doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, true);

        } else if((pUnit->getItemID() == Unit_Quad

                // Quads don't run from trikes, infantry, other quads
                && !pDamager->isInfantry()
                && pDamager->getItemID() != Unit_RaiderTrike
                && pDamager->getItemID() != Unit_Trike
                && pDamager->getItemID() != Unit_Quad)

                // Trikes run from quads
                || ((pUnit->getItemID() == Unit_RaiderTrike
                    || pUnit->getItemID() == Unit_Trike)
                    && !pDamager->isInfantry()
                    && pDamager->getItemID() != Unit_RaiderTrike
                    && pDamager->getItemID() != Unit_Trike)) {
            // We want to use our light vehicles as raiders.
            // This means they are free to engage other light military units
            // but should run away from tanks

            doSetAttackMode(pUnit, AREAGUARD);
            doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, true);

        } else if ((pUnit->getItemID() == Unit_Tank || pUnit->getItemID() == Unit_SiegeTank)
            && pUnit->getHealth() * 100 / pUnit->getMaxHealth() < 85
            && pDamager->getItemID() != Structure_GunTurret
            && pDamager->getItemID() != Structure_RocketTurret
            && pDamager->getItemID() != Unit_RaiderTrike
            && pDamager->getItemID() != Unit_Trike
            && pDamager->getItemID() != Unit_Quad
            && pDamager->getItemID() != Unit_SonicTank
            && pDamager->getItemID() != Unit_Deviator
            && !pDamager->isInfantry()) {
            // All Tanks run from other tanks and Ornithopters when below 85% health

            doSetAttackMode(pUnit, AREAGUARD);
            doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, true);
        } else if ((pUnit->getItemID() == Unit_Tank)
                 && pDamager->getItemID() != Unit_SiegeTank
                 && pDamager->getItemID() != Structure_GunTurret
                 && pDamager->getItemID() != Structure_RocketTurret
                 && pDamager->getItemID() != Unit_RaiderTrike
                 && pDamager->getItemID() != Unit_Trike
                 && pDamager->getItemID() != Unit_Quad
                 && pDamager->getItemID() != Unit_SonicTank
                 && pDamager->getItemID() != Unit_Deviator
                 && !pDamager->isInfantry()){
            // Tanks run from Siege Tanks
            doSetAttackMode(pUnit, AREAGUARD);
            doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, true);
        }

        // If the unit is at 60% health or less and is not being forced to move anywhere
        // repair them, if they are eligible to be repaired
        if((pUnit->getHealth() * 100) / pUnit->getMaxHealth() < 60
            && !pUnit->isInfantry()
            && pUnit->isVisible()) {

            if(getHouse()->hasRepairYard()){
                doRepair(pUnit);
            } else if(gameMode == CUSTOM && pUnit->getItemID() != Unit_Devastator){
                doSetAttackMode(pUnit, RETREAT);
            }
        }
    }
}

Coord QuantBot::findMcvPlaceLocation(const MCV* pMCV) {
    int bestLocationScore = 1000;
    Coord bestLocation = Coord::Invalid();

    bestLocation = findPlaceLocation(Structure_ConstructionYard);

    if(bestLocation == Coord::Invalid()) {
        logDebug("No MCV deploy location adjacent to existing base structures was found, move to full search | ");

        // Don't place on the very edge of the map
        for(int placeLocationX = 1; placeLocationX < getMap().getSizeX() -1; placeLocationX++) {
            for(int placeLocationY = 1; placeLocationY < getMap().getSizeY() -1; placeLocationY++) {
                Coord placeLocation = Coord::Invalid();
                placeLocation.x = placeLocationX;
                placeLocation.y = placeLocationY;

                if(getMap().okayToPlaceStructure(placeLocationX, placeLocationY, 2, 2, false, nullptr)) {
                    int locationScore = lround(blockDistance(pMCV->getLocation(), placeLocation));
                    if(locationScore < bestLocationScore){
                        bestLocationScore = locationScore;
                        bestLocation.x = placeLocationX;
                        bestLocation.y = placeLocationY;
                    }
                }
            }
        }
    }

    return bestLocation;
}

Coord QuantBot::findPlaceLocation(Uint32 itemID) {

    RobustList<const StructureBase*>::const_iterator iter;

    // Will over allocate space for small maps so its not clean
    // But should allow Richard to compile
    int buildLocationScore[128][128] = {{0}};

    int bestLocationX = -1;
    int bestLocationY = -1;
    int bestLocationScore = - 10000;
    int newSizeX = getStructureSize(itemID).x;
    int newSizeY = getStructureSize(itemID).y;
    Coord bestLocation = Coord::Invalid();

    for(iter = getStructureList().begin(); iter != getStructureList().end(); ++iter) {
        const StructureBase* pStructureExisting = *iter;

        if(pStructureExisting->getOwner() == getHouse()) {

            int existingStartX = pStructureExisting->getX();
            int existingStartY = pStructureExisting->getY();

            int existingSizeX = pStructureExisting->getStructureSizeX();
            int existingSizeY = pStructureExisting->getStructureSizeY();

            int existingEndX = existingStartX + existingSizeX;
            int existingEndY = existingStartY + existingSizeY;

            squadRallyLocation = findSquadRallyLocation();

            bool existingIsBuilder = (pStructureExisting->getItemID() == Structure_HeavyFactory
                                   || pStructureExisting->getItemID() == Structure_RepairYard
                                   || pStructureExisting->getItemID() == Structure_LightFactory
                                   || pStructureExisting->getItemID() == Structure_WOR
                                   || pStructureExisting->getItemID() == Structure_Barracks
                                   || pStructureExisting->getItemID() == Structure_StarPort);

            bool sizeMatchX = (existingSizeX == newSizeX);
            bool sizeMatchY = (existingSizeY == newSizeY);


            for(int placeLocationX = existingStartX - newSizeX; placeLocationX <= existingEndX; placeLocationX++){
                for(int placeLocationY = existingStartY - newSizeY; placeLocationY <= existingEndY; placeLocationY++){
                    if(getMap().tileExists(placeLocationX,placeLocationY)){
                        if(getMap().okayToPlaceStructure(placeLocationX, placeLocationY, newSizeX, newSizeY,
                                                         false, (itemID == Structure_ConstructionYard) ? nullptr : getHouse())) {

                            int placeLocationEndX = placeLocationX + newSizeX;
                            int placeLocationEndY = placeLocationY + newSizeY;

                            bool alignedX = (placeLocationX == existingStartX && sizeMatchX);
                            bool alignedY = (placeLocationY == existingStartY && sizeMatchY);

                            // bool placeGapExists = (placeLocationEndX < existingStartX || placeLocationX > existingEndX || placeLocationEndY < existingStartY || placeLocationY > existingEndY);

                            // How many free spaces the building will have if placed
                            for(int i = placeLocationX-1; i <= placeLocationEndX; i++) {
                                for(int j = placeLocationY-1; j <= placeLocationEndY; j++) {
                                    if(getMap().tileExists(i,j) && (getMap().getSizeX() > i) && (0 <= i) && (getMap().getSizeY() > j) && (0 <= j)) {
                                            // Penalise if near edge of map
                                            if((i == 0) || (i == getMap().getSizeX() - 1) || (j == 0) || (j == getMap().getSizeY() - 1)) {
                                                buildLocationScore[placeLocationX][placeLocationY] -= 10;
                                            }

                                            if(getMap().getTile(i,j)->hasAStructure()) {
                                                // If one of our buildings is nearby favour the location
                                                // if it is someone elses building don't favour it
                                                if(getMap().getTile(i,j)->getOwner() == getHouse()->getHouseID()){
                                                    buildLocationScore[placeLocationX][placeLocationY]+=3;
                                                } else{
                                                    buildLocationScore[placeLocationX][placeLocationY]-=10;
                                                }
                                            } else if(!getMap().getTile(i,j)->isRock()){
                                                // square isn't rock, favour it
                                                buildLocationScore[placeLocationX][placeLocationY]+=1;
                                            } else if(getMap().getTile(i,j)->hasAGroundObject()){
                                                if(getMap().getTile(i,j)->getOwner() != getHouse()->getHouseID()){
                                                    // try not to build next to units which aren't yours
                                                    buildLocationScore[placeLocationX][placeLocationY]-=100;
                                                } else if(itemID != Structure_RocketTurret){
                                                    buildLocationScore[placeLocationX][placeLocationY]-=20;
                                                }
                                            }
                                    } else {
                                        // penalise if on edge of map
                                        buildLocationScore[placeLocationX][placeLocationY]-=200;
                                    }
                                }
                            }

                            //encourage structure alignment
                            if(alignedX) {
                                buildLocationScore[placeLocationX][placeLocationX] += 10;
                            }

                            if(alignedY) {
                                buildLocationScore[placeLocationX][placeLocationY] += 10;
                            }

                            // Add building specific scores
                            if(existingIsBuilder || itemID == Structure_GunTurret || itemID == Structure_RocketTurret){
                                buildLocationScore[placeLocationX][placeLocationY] -= lround(blockDistance(squadRallyLocation, Coord(placeLocationX,placeLocationY))/2);

                                buildLocationScore[placeLocationX][placeLocationY] -= lround(blockDistance(findBaseCentre(getHouse()->getHouseID()), Coord(placeLocationX,placeLocationY)));
                            }

                            // Pick this location if it has the best score
                            if (buildLocationScore[placeLocationX][placeLocationY] > bestLocationScore) {
                                bestLocationScore = buildLocationScore[placeLocationX][placeLocationY];
                                bestLocationX = placeLocationX;
                                bestLocationY = placeLocationY;
                                //logDebug("Build location for item:%d  x:%d y:%d score:%d", itemID, bestLocationX, bestLocationY, bestLocationScore);
                            }
                        }
                    }
                }
            }
        }
    }


    if (bestLocationScore != -10000){
        bestLocation = Coord(bestLocationX, bestLocationY);
    }

    return bestLocation;
}


void QuantBot::build(int militaryValue) {
    int houseID = getHouse()->getHouseID();
    auto& data = currentGame->objectData.data;

    int itemCount[Num_ItemID];
    for (int i = ItemID_FirstID; i <= ItemID_LastID; i++ ) {
        itemCount[i] = getHouse()->getNumItems(i);
    }

    int activeHeavyFactoryCount = 0;
    int activeRepairYardCount = 0;

    // Let's try just running this once...
    if(squadRallyLocation.x == 0 && squadRallyLocation.y == 0 ){
        logDebug("Set squad rally location");
        retreatAllUnits();
    }

    // Next add in the objects we are building
    RobustList<const StructureBase*>::const_iterator iter;

    for(iter = getStructureList().begin(); iter != getStructureList().end(); ++iter) {
        const StructureBase* pStructure = *iter;

        if(pStructure->getOwner() == getHouse()) {
            if(pStructure->isABuilder()) {
                const BuilderBase* pBuilder = dynamic_cast<const BuilderBase*>(pStructure);
                if(pBuilder->getProductionQueueSize() > 0){
                    itemCount[pBuilder->getCurrentProducedItem()]++;
                    if(pBuilder->getItemID() == Structure_HeavyFactory){
                        activeHeavyFactoryCount++;
                    }
                }
            } else if(pStructure->getItemID() == Structure_RepairYard) {
                const RepairYard* pRepairYard= dynamic_cast<const RepairYard*>(pStructure);
                if(!pRepairYard->isFree()) {
                    activeRepairYardCount++;
                }

            }

            // Set unit deployment position
            if(pStructure->getItemID() == Structure_Barracks
               || pStructure->getItemID() == Structure_WOR
               || pStructure->getItemID() == Structure_LightFactory
               || pStructure->getItemID() == Structure_HeavyFactory
               || pStructure->getItemID() == Structure_RepairYard
               || pStructure->getItemID() == Structure_StarPort) {
                doSetDeployPosition(pStructure, squadRallyLocation.x, squadRallyLocation.y);
            }
        }


    }

    int money = getHouse()->getCredits();

    if(militaryValue > 0 || getHouse()->getNumStructures() > 0) {
        logDebug(   " att: %d  crdt: %d  mVal: %d/%d  built: %d  kill: %d  loss: %d hvstr: %d/%d",
                    attackTimer, getHouse()->getCredits(), militaryValueLimit, militaryValue, getHouse()->getUnitBuiltValue(),
                    getHouse()->getKillValue(), getHouse()->getLossValue(), getHouse()->getNumItems(Unit_Harvester), harvesterLimit);
    }


    // Second attempt at unit prioritisation
    // This algorithm calculates damage dealt over units lost value for each unit type
    // referred to as damage loss ratio (dlr)
    // It then prioritises the build of units with a higher dlr
    FixPoint dlrTank = getHouse()->getNumItemDamageInflicted(Unit_Tank) / FixPoint((1 + getHouse()->getNumLostItems(Unit_Tank)) * data[Unit_Tank][houseID].price);
    FixPoint dlrSiege = getHouse()->getNumItemDamageInflicted(Unit_SiegeTank) / FixPoint((1 + getHouse()->getNumLostItems(Unit_SiegeTank)) * data[Unit_SiegeTank][houseID].price);
    int numSpecialUnitsDamageInflicted = getHouse()->getNumItemDamageInflicted(Unit_Devastator) + getHouse()->getNumItemDamageInflicted(Unit_SonicTank) + getHouse()->getNumItemDamageInflicted(Unit_Deviator);
    int weightedNumLostSpecialUnits = (getHouse()->getNumLostItems(Unit_Devastator) * data[Unit_Devastator][houseID].price)
                                        + (getHouse()->getNumLostItems(Unit_SonicTank) * data[Unit_SonicTank][houseID].price)
                                        + (getHouse()->getNumLostItems(Unit_Deviator) * data[Unit_Deviator][houseID].price)
                                        + 700; // middle ground 1 for special units
    FixPoint dlrSpecial = FixPoint(numSpecialUnitsDamageInflicted) / FixPoint(weightedNumLostSpecialUnits);
    FixPoint dlrLauncher = getHouse()->getNumItemDamageInflicted(Unit_Launcher) / FixPoint((1 + getHouse()->getNumLostItems(Unit_Launcher)) * data[Unit_Launcher][houseID].price);
    FixPoint dlrOrnithopter = getHouse()->getNumItemDamageInflicted(Unit_Ornithopter) / FixPoint((1 + getHouse()->getNumLostItems(Unit_Ornithopter)) * data[Unit_Ornithopter][houseID].price);

    Sint32 totalDamage =    getHouse()->getNumItemDamageInflicted(Unit_Tank)
                            + getHouse()->getNumItemDamageInflicted(Unit_SiegeTank)
                            + getHouse()->getNumItemDamageInflicted(Unit_Devastator)
                            + getHouse()->getNumItemDamageInflicted(Unit_Launcher)
                            + getHouse()->getNumItemDamageInflicted(Unit_Ornithopter);

    // Harkonnen can't build ornithopers
    if(houseID == HOUSE_HARKONNEN){
        dlrOrnithopter = 0;
    }

    // Ordos can't build Launchers
    if(houseID == HOUSE_ORDOS){
        dlrLauncher = 0;
    }

    // Sonic tanks can get into negative damage territory
    if(dlrSpecial < 0){
        dlrSpecial = 0;
    }

    FixPoint dlrTotal = dlrTank + dlrSiege + dlrSpecial + dlrLauncher + dlrOrnithopter;

    if(dlrTotal < 0){
        dlrTotal = 0;
    }

    logDebug("Dmg: %d DLR: %f", totalDamage, dlrTotal.toFloat());

    /// Calculate ratios of launcher, special and light tanks. Remainder will be tank
    FixPoint launcherPercent = dlrLauncher / dlrTotal;
    FixPoint specialPercent = dlrSpecial / dlrTotal;
    FixPoint siegePercent = dlrSiege / dlrTotal;
    FixPoint ornithopterPercent = dlrOrnithopter / dlrTotal;
    FixPoint tankPercent = dlrTank / dlrTotal;

    // If we haven't done much damage just keep all ratios at optimised defaults
    // These ratios are based on end game stats over a number of AI test runs to see
    // Which units perform. By and large launchers and siege tanks have the best damage to loss ratio
    if(totalDamage < 3000){
        switch (houseID) {
            case HOUSE_HARKONNEN:
                launcherPercent = FixPt(0,5);
                specialPercent = FixPt(0,15);
                siegePercent = FixPt(0,35);
                ornithopterPercent = FixPt(0,0);
                break;

            case HOUSE_ORDOS:
                launcherPercent = FixPt(0,0); // Don't have these
                specialPercent = FixPt(0,25);
                siegePercent = FixPt(0,75);
                ornithopterPercent = FixPt(0,05);
                break;

            default:
                launcherPercent = FixPt(0,40);
                specialPercent = FixPt(0,10);
                siegePercent = FixPt(0,35);
                ornithopterPercent = FixPt(0,15);

                break;
        }
    }

    // lets analyse damage inflicted
    logDebug("  Tank: %d/%d %f Siege: %d/%d %f Special: %d/%d %f Launch: %d/%d %f Orni: %d/%d %f",
                getHouse()->getNumItemDamageInflicted(Unit_Tank), getHouse()->getNumLostItems(Unit_Tank) * 300, tankPercent.toDouble(),
                getHouse()->getNumItemDamageInflicted(Unit_SiegeTank), getHouse()->getNumLostItems(Unit_SiegeTank) * 600, siegePercent.toDouble(),
                getHouse()->getNumItemDamageInflicted(Unit_SonicTank) + getHouse()->getNumItemDamageInflicted(Unit_Devastator) + getHouse()->getNumItemDamageInflicted(Unit_Deviator),
                getHouse()->getNumLostItems(Unit_SonicTank) * 600 + getHouse()->getNumLostItems(Unit_Devastator) * 800 + getHouse()->getNumLostItems(Unit_Deviator) * 750,
                specialPercent.toDouble(),
                getHouse()->getNumItemDamageInflicted(Unit_Launcher), getHouse()->getNumLostItems(Unit_Launcher) * 450, launcherPercent.toDouble(),
                getHouse()->getNumItemDamageInflicted(Unit_Ornithopter), getHouse()->getNumLostItems(Unit_Ornithopter) * data[Unit_Ornithopter][houseID].price, ornithopterPercent.toDouble()
            );


    // End of adaptive unit prioritisation algorithm


    for(iter = getStructureList().begin(); iter != getStructureList().end(); ++iter) {
        const StructureBase* pStructure = *iter;

        if(pStructure->getOwner() == getHouse()) {
            if((pStructure->isRepairing() == false)
               && (pStructure->getHealth() < pStructure->getMaxHealth())
                && (!getGameInitSettings().getGameOptions().concreteRequired
                     || pStructure->getItemID() == Structure_Palace) // Palace repairs for free
                && (pStructure->getItemID() != Structure_Refinery
                    && pStructure->getItemID() != Structure_Silo
                    && pStructure->getItemID() != Structure_Radar
                    && pStructure->getItemID() != Structure_WindTrap))
            {
                doRepair(pStructure);
            } else if(  (pStructure->isRepairing() == false)
                        && (pStructure->getHealth() < pStructure->getMaxHealth() * FixPt(0,45))
                        && !getGameInitSettings().getGameOptions().concreteRequired
                        && money > 1000) {
                doRepair(pStructure);
            } else if( (pStructure->isRepairing() == false) && money > 5000){
                // Repair if we are rich
                doRepair(pStructure);
            } else if(pStructure->getItemID() == Structure_RocketTurret) {
                if(!getGameInitSettings().getGameOptions().structuresDegradeOnConcrete || pStructure->hasATarget()) {
                    doRepair(pStructure);
                }
            }

            // Special weapon launch logic
            if(pStructure->getItemID() == Structure_Palace) {

                const Palace* pPalace = dynamic_cast<const Palace*>(pStructure);

                if(pPalace != nullptr){
                    if(pPalace->isSpecialWeaponReady()){

                        if(houseID != HOUSE_HARKONNEN && houseID != HOUSE_SARDAUKAR) {
                            doSpecialWeapon(pPalace);
                        } else {
                            int enemyHouseID = -1;
                            int enemyHouseBuildingCount = 0;

                            for(int i = 0; i < NUM_HOUSES; i++) {
                                if(getHouse(i) != nullptr) {
                                    if(getHouse(i)->getTeam() != getHouse()->getTeam() && getHouse(i)->getNumStructures() > enemyHouseBuildingCount) {
                                        enemyHouseBuildingCount = getHouse(i)->getNumStructures();
                                        enemyHouseID = i;
                                    }
                                }
                            }

                            if((enemyHouseID != -1) && (houseID == HOUSE_HARKONNEN || houseID == HOUSE_SARDAUKAR)) {
                                Coord target = findBaseCentre(enemyHouseID);
                                doLaunchDeathhand(pPalace, target.x, target.y);
                            }
                        }
                    }
                }
            }


            /*  First attempt of unit prioritisation
             We this algorithm prioritises units with the lowest loss ratio
             The idea is if a unit is less likely to die the AI should have
             a higher ratio of that unit in its army

             At the moment it takes in special, light tanks and launchers
             The default is siege tanks otherwise


            int launcherLosses = getHouse()->getNumLostItems(Unit_Launcher)
            * data[Unit_Devastator][houseID].price;

            int specialLosses = getHouse()->getNumLostItems(Unit_SonicTank)
            * data[Unit_SonicTank][houseID].price + getHouse()->getNumLostItems(Unit_Deviator)
            * data[Unit_Deviator][houseID].price + getHouse()->getNumLostItems(Unit_Devastator)
            * data[Unit_Devastator][houseID].price;

            int lightLosses = getHouse()->getNumLostItems(Unit_Tank)
            * data[Unit_Tank][houseID].price;

            int siegeLosses = getHouse()->getNumLostItems(Unit_SiegeTank)
            * data[Unit_SiegeTank][houseID].price;

            int ornithopterLosses = getHouse()->getNumLostItems(Unit_Ornithopter)
            * data[Unit_Ornithopter][houseID].price;

            int totalLosses = launcherLosses + specialLosses + lightLosses + siegeLosses + ornithopterLosses;



             //Effectively I'm solving a simultaneous equation
             //There's probably an easier way involving matrices but this works



            FixPoint launcherWeight = FixPoint((totalLosses - launcherLosses) + 1) / (launcherLosses+1);
            FixPoint specialWeight = FixPoint((totalLosses - specialLosses) + 1) / (specialLosses+1);
            FixPoint lightWeight = FixPoint((totalLosses - lightLosses) + 1) / (lightLosses+1);
            FixPoint siegeWeight = FixPoint((totalLosses - siegeLosses) + 1) / (siegeLosses+1);
            FixPoint ornithopterWeight = FixPoint((totalLosses - ornithopterLosses) + 1) / (ornithopterLosses+1);

            FixPoint totalWeight = launcherWeight + specialWeight + lightWeight + siegeWeight + ornithopterWeight;

            // Apply house specific logic
            if(houseID == HOUSE_HARKONNEN){
                totalWeight -= ornithopterWeight;
            }

            if(houseID == HOUSE_ATREIDES){
                totalWeight -= specialWeight;
            }


            if(houseID == HOUSE_ORDOS){
                totalWeight -= launcherWeight;
            }

            /// Calculate ratios of launcher, special and light tanks. Remainder will be tank
            FixPoint launcherPercent = launcherWeight / totalWeight;
            FixPoint specialPercent = specialWeight / totalWeight;
            FixPoint siegePercent = siegeWeight / totalWeight;
            FixPoint ornithopterPercent = ornithopterWeight / totalWeight;

            */

            // End of unit ratio optimisation algorithm

            const BuilderBase* pBuilder = dynamic_cast<const BuilderBase*>(pStructure);
            if(pBuilder != nullptr) {
                switch (pStructure->getItemID()) {

                    case Structure_LightFactory: {
                        if(!pBuilder->isUpgrading()
                           && gameMode == CAMPAIGN
                           && money > 1000
                           && (itemCount[Structure_HeavyFactory == 0] || militaryValue < militaryValueLimit * FixPt(0,30))
                           && pBuilder->getProductionQueueSize() < 1
                           && pBuilder->getBuildListSize() > 0
                           && militaryValue < militaryValueLimit) {

                            if(pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel() && getHouse()->getCredits() > 1500){
                                doUpgrade(pBuilder);
                            } else if(!getHouse()->isUnitLimitReached()) {
                                Uint32 itemID = NONE_ID;

                                if(pBuilder->isAvailableToBuild(Unit_RaiderTrike)) {
                                    itemID = Unit_RaiderTrike;
                                } else if(pBuilder->isAvailableToBuild(Unit_Quad)) {
                                    itemID = Unit_Quad;
                                } else if(pBuilder->isAvailableToBuild(Unit_Trike)) {
                                    itemID = Unit_Trike;
                                }

                                if(itemID != NONE_ID){
                                    doProduceItem(pBuilder, itemID);
                                    itemCount[itemID]++;
                                }
                            }
                        }
                    } break;

                    case Structure_WOR: {
                        if(!pBuilder->isUpgrading()
                           && gameMode == CAMPAIGN
                           && money > 1000
                           && (itemCount[Structure_HeavyFactory == 0] || militaryValue < militaryValueLimit * FixPt(0,30))
                           && pBuilder->getProductionQueueSize() < 1
                           && pBuilder->getBuildListSize() > 0
                           && !getHouse()->isUnitLimitReached()
                           && militaryValue < militaryValueLimit) {

                            doProduceItem(pBuilder, Unit_Trooper);
                            itemCount[Unit_Trooper]++;
                        }
                    } break;

                    case Structure_Barracks: {
                        if(!pBuilder->isUpgrading()
                           && gameMode == CAMPAIGN
                           && (itemCount[Structure_HeavyFactory == 0] || militaryValue < militaryValueLimit * FixPt(0,30))
                           && itemCount[Structure_WOR == 0]
                           && money > 1000
                           && pBuilder->getProductionQueueSize() < 1
                           && pBuilder->getBuildListSize() > 0
                           && !getHouse()->isUnitLimitReached()
                           && militaryValue < militaryValueLimit){

                            doProduceItem(pBuilder, Unit_Soldier);
                            itemCount[Unit_Soldier]++;
                        }
                    } break;

                    case Structure_HighTechFactory: {
                        int ornithopterValue = data[Unit_Ornithopter][houseID].price * itemCount[Unit_Ornithopter];

                        if(itemCount[Unit_Carryall] < (militaryValue + itemCount[Unit_Harvester] * 500) / 3000
                           && (pBuilder->getProductionQueueSize() < 1)
                           && money > 1000
                           && !getHouse()->isUnitLimitReached()){
                            doProduceItem(pBuilder, Unit_Carryall);
                            itemCount[Unit_Carryall]++;
                        } else if((money > 500) && (pBuilder->isUpgrading() == false) && (pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel())) {
                            if (pBuilder->getHealth() >= pBuilder->getMaxHealth()) {
                                doUpgrade(pBuilder);
                            } else {
                                doRepair(pBuilder);
                            }
                        } else if( pBuilder->isAvailableToBuild(Unit_Ornithopter)
                                && (militaryValue * ornithopterPercent > ornithopterValue)
                                && (pBuilder->getProductionQueueSize() < 1)
                                && !getHouse()->isUnitLimitReached()
                                && money > 1200){
                            // Current value and what percentage of military we want used to determine
                            // whether to build an additional unit.
                            doProduceItem(pBuilder, Unit_Ornithopter);
                            itemCount[Unit_Ornithopter]++;
                            money -= data[Unit_Ornithopter][houseID].price;
                            militaryValue += data[Unit_Ornithopter][houseID].price;
                        }
                    } break;

                    case Structure_HeavyFactory: {
                        // only if the factory isn't busy
                        if((pBuilder->isUpgrading() == false) && (pBuilder->getProductionQueueSize() < 1) && (pBuilder->getBuildListSize() > 0)) {
                            // we need a construction yard. Build an MCV if we don't have a starport
                            if(itemCount[Unit_MCV] + itemCount[Structure_ConstructionYard] + itemCount[Structure_StarPort] < 1
                                && pBuilder->isAvailableToBuild(Unit_MCV)
                                && !getHouse()->isUnitLimitReached()) {
                                doProduceItem(pBuilder, Unit_MCV);
                                itemCount[Unit_MCV]++;
                            } else if(gameMode == CUSTOM && (itemCount[Structure_ConstructionYard] + itemCount[Unit_MCV] )*3500 < getHouse()->getCredits()
                                        && pBuilder->isAvailableToBuild(Unit_MCV)
                                        && itemCount[Structure_ConstructionYard] + itemCount[Unit_MCV] < 10
                                        && !getHouse()->isUnitLimitReached()
                                        && militaryValue * 2 > militaryValueLimit){
                                // If we are really rich, like in all against Atriedes
                                doProduceItem(pBuilder, Unit_MCV);
                                itemCount[Unit_MCV]++;
                            } else if(gameMode == CUSTOM
                                        && (itemCount[Structure_ConstructionYard] + itemCount[Unit_MCV] ) * 10000 < getHouse()->getCredits()
                                        && !getHouse()->isUnitLimitReached()
                                        && pBuilder->isAvailableToBuild(Unit_MCV)){
                                // If we are kind of rich make a backup construction yard to spend the excess money
                                doProduceItem(pBuilder, Unit_MCV);
                                itemCount[Unit_MCV]++;
                            } else if(gameMode == CUSTOM
                                        && !getHouse()->isUnitLimitReached()
                                        && itemCount[Unit_Harvester] < militaryValue / 1000
                                        && itemCount[Unit_Harvester] < harvesterLimit ) {
                                // In case we get given lots of money, it will eventually run out so we need to be prepared
                                doProduceItem(pBuilder, Unit_Harvester);
                                itemCount[Unit_Harvester]++;
                            } else if(itemCount[Unit_Harvester] < harvesterLimit
                                        && !getHouse()->isUnitLimitReached()
                                        && (money < 2500 || gameMode == CAMPAIGN)) {
                                //logDebug("*Building a Harvester.",
                                //itemCount[Unit_Harvester], harvesterLimit, money);
                                doProduceItem(pBuilder, Unit_Harvester);
                                itemCount[Unit_Harvester]++;
                            } else if((money > 500) && (pBuilder->isUpgrading() == false) && (pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel())) {
                                if (pBuilder->getHealth() >= pBuilder->getMaxHealth()){
                                    doUpgrade(pBuilder);
                                } else {
                                    doRepair(pBuilder);
                                }
                            } else if(money > 1200 && militaryValue < militaryValueLimit && !getHouse()->isUnitLimitReached()) {
                                // TODO: This entire section needs to be refactored to make it more generic
                                // Limit enemy military units based on difficulty

                                // Calculate current value of units
                                int launcherValue = data[Unit_Launcher][houseID].price * itemCount[Unit_Launcher];
                                int specialValue = data[Unit_Devastator][houseID].price * itemCount[Unit_Devastator]
                                                    + data[Unit_Deviator][houseID].price * itemCount[Unit_Deviator]
                                                    + data[Unit_SonicTank][houseID].price * itemCount[Unit_SonicTank];
                                int siegeValue = data[Unit_SiegeTank][houseID].price * itemCount[Unit_SiegeTank];


                                /// Use current value and what percentage of military we want to determine
                                /// whether to build an additional unit.
                                if( pBuilder->isAvailableToBuild(Unit_Launcher) && (militaryValue * launcherPercent > launcherValue)) {
                                    doProduceItem(pBuilder, Unit_Launcher);
                                    itemCount[Unit_Launcher]++;
                                    money -= data[Unit_Launcher][houseID].price;
                                    militaryValue += data[Unit_Launcher][houseID].price;
                                } else if( pBuilder->isAvailableToBuild(Unit_Devastator) && (militaryValue * specialPercent > specialValue)) {
                                    doProduceItem(pBuilder, Unit_Devastator);
                                    itemCount[Unit_Devastator]++;
                                    money -= data[Unit_Devastator][houseID].price;
                                    militaryValue += data[Unit_Devastator][houseID].price;
                                } else if( pBuilder->isAvailableToBuild(Unit_SonicTank) && (militaryValue * specialPercent > specialValue)) {
                                    doProduceItem(pBuilder, Unit_SonicTank);
                                    itemCount[Unit_SonicTank]++;
                                    money -= data[Unit_SonicTank][houseID].price;
                                    militaryValue += data[Unit_SonicTank][houseID].price;
                                } else if( pBuilder->isAvailableToBuild(Unit_Deviator) && (militaryValue * specialPercent > specialValue)) {
                                    doProduceItem(pBuilder, Unit_Deviator);
                                    itemCount[Unit_Deviator]++;
                                    money -= data[Unit_Deviator][houseID].price;
                                    militaryValue += data[Unit_Deviator][houseID].price;
                                } else if( pBuilder->isAvailableToBuild(Unit_SiegeTank) && (militaryValue * siegePercent > siegeValue)) {
                                    doProduceItem(pBuilder, Unit_SiegeTank);
                                    itemCount[Unit_SiegeTank]++;
                                    money -= data[Unit_Tank][houseID].price;
                                    militaryValue += data[Unit_SiegeTank][houseID].price;
                                } else if(pBuilder->isAvailableToBuild(Unit_Tank)) {
                                    // Tanks for all else
                                    doProduceItem(pBuilder, Unit_Tank);
                                    itemCount[Unit_Tank]++;
                                    money -= data[Unit_Tank][houseID].price;
                                    militaryValue += data[Unit_Tank][houseID].price;
                                }
                            }
                        }

                    } break;

                    case Structure_StarPort: {
                        const StarPort* pStarPort = dynamic_cast<const StarPort*>(pBuilder);
                        if(pStarPort->okToOrder())  {
                            const Choam& choam = getHouse()->getChoam();

                            // We need a construction yard!!
                            if(pStarPort->isAvailableToBuild(Unit_MCV) && choam.getNumAvailable(Unit_MCV) > 0 && itemCount[Structure_ConstructionYard] + itemCount[Unit_MCV] < 1) {
                                doProduceItem(pBuilder, Unit_MCV);
                                itemCount[Unit_MCV]++;
                                money = money - choam.getPrice(Unit_MCV);
                            }

                            if(money >= choam.getPrice(Unit_Carryall) && itemCount[Unit_Carryall] == 0) {
                                doProduceItem(pBuilder, Unit_Carryall);
                                itemCount[Unit_Carryall]++;
                                money = money - choam.getPrice(Unit_Carryall);
                            } else if(militaryValue > (itemCount[Unit_Harvester]*200)) {
                                while (money > choam.getPrice(Unit_Harvester) && choam.getNumAvailable(Unit_Harvester) > 0 && itemCount[Unit_Harvester] < harvesterLimit){
                                    doProduceItem(pBuilder, Unit_Harvester);
                                    itemCount[Unit_Harvester]++;
                                    money = money - choam.getPrice(Unit_Harvester);
                                }

                                int itemCountUnits = itemCount[Unit_Tank] + itemCount[Unit_SiegeTank] + itemCount[Unit_Launcher] + itemCount[Unit_Quad] + itemCount[Unit_Harvester];
                                while (money > choam.getPrice(Unit_Carryall) && choam.getNumAvailable(Unit_Carryall) > 0 && itemCount[Unit_Carryall] < itemCountUnits / 5) {
                                    doProduceItem(pBuilder, Unit_Carryall);
                                    itemCount[Unit_Carryall]++;
                                    money = money - choam.getPrice(Unit_Carryall);
                                }
                            }

                            if (money > choam.getPrice(Unit_Carryall) && choam.getNumAvailable(Unit_Carryall) > 0 && itemCount[Unit_Carryall] == 0) {
                                // Get at least one Carryall
                                doProduceItem(pBuilder, Unit_Carryall);
                                itemCount[Unit_Carryall]++;
                                money = money - choam.getPrice(Unit_Carryall);
                            }

                            if(militaryValue < militaryValueLimit && itemCount[Unit_Carryall] > 0 ) {
                                while (money > choam.getPrice(Unit_SiegeTank) && choam.getNumAvailable(Unit_SiegeTank) > 0
                                       && choam.isCheap(Unit_SiegeTank) && militaryValue < militaryValueLimit) {
                                    doProduceItem(pBuilder, Unit_SiegeTank);
                                    itemCount[Unit_SiegeTank]++;
                                    money = money - choam.getPrice(Unit_SiegeTank);
                                    militaryValue += data[Unit_SiegeTank][houseID].price;
                                }

                                while (money > choam.getPrice(Unit_Tank) && choam.getNumAvailable(Unit_Tank) > 0
                                       && choam.isCheap(Unit_Tank) && militaryValue < militaryValueLimit) {
                                    doProduceItem(pBuilder, Unit_Tank);
                                    itemCount[Unit_Tank]++;
                                    money = money - choam.getPrice(Unit_Tank);
                                    militaryValue += data[Unit_Tank][houseID].price;
                                }

                                while (money > choam.getPrice(Unit_Launcher) && choam.getNumAvailable(Unit_Launcher) > 0
                                       && choam.isCheap(Unit_Launcher) && militaryValue < militaryValueLimit && militaryValue > 1000) {
                                    doProduceItem(pBuilder, Unit_Launcher);
                                    itemCount[Unit_Launcher]++;
                                    money = money - choam.getPrice(Unit_Launcher);
                                    militaryValue += data[Unit_Launcher][houseID].price;
                                }

                                while (money > choam.getPrice(Unit_Quad) && choam.getNumAvailable(Unit_Quad) > 0
                                       && choam.isCheap(Unit_Quad) && militaryValue * 10 < militaryValueLimit) {
                                    doProduceItem(pBuilder, Unit_Quad);
                                    itemCount[Unit_Quad]++;
                                    money = money - choam.getPrice(Unit_Quad);
                                    militaryValue += data[Unit_Quad][houseID].price;
                                }

                                while (money > choam.getPrice(Unit_Trike) && choam.getNumAvailable(Unit_Trike) > 0
                                       && choam.isCheap(Unit_Trike) && militaryValue * 10 < militaryValueLimit) {
                                    doProduceItem(pBuilder, Unit_Trike);
                                    itemCount[Unit_Trike]++;
                                    money = money - choam.getPrice(Unit_Trike);
                                    militaryValue += data[Unit_Trike][houseID].price;
                                }
                            }

                            doPlaceOrder(pStarPort);
                        }

                    } break;

                    case Structure_ConstructionYard: {

                        // If rocket turrets don't need power then let's build some for defense
                        int rocketTurretValue = itemCount[Structure_RocketTurret] * 250;

                        if(getGameInitSettings().getGameOptions().rocketTurretsNeedPower) {
                            rocketTurretValue = 1000000; // If rocket turrets need power we don't want to build them
                        }

                        const ConstructionYard* pConstYard = dynamic_cast<const ConstructionYard*>(pBuilder);

                        if(!pBuilder->isUpgrading() && getHouse()->getCredits() > 100 && (pBuilder->getProductionQueueSize() < 1) && pBuilder->getBuildListSize()) {

                            // Campaign Build order, iterate through the buildings, if the number that exist
                            // is less than the number that should exist, then build the one that is missing

                            if(gameMode == CAMPAIGN && difficulty != BRUTAL) {
                                //logDebug("GameMode Campaign.. ");

                                for(int i = Structure_FirstID; i <= Structure_LastID; i++){
                                    if(itemCount[i] < initialItemCount[i]
                                       && pBuilder->isAvailableToBuild(i)
                                       && findPlaceLocation(i).isValid()
                                       && !pBuilder->isUpgrading()
                                       && pBuilder->getProductionQueueSize() < 1) {

                                        logDebug("***CampAI Build itemID: %o structure count: %o, initial count: %o", i, itemCount[i], initialItemCount[i]);
                                        doProduceItem(pBuilder, i);
                                        itemCount[i]++;
                                    }
                                }

                                // If Campaign AI can't build military, let it build up its cash reserves and defenses

                                if(pStructure->getHealth() < pStructure->getMaxHealth()) {
                                    doRepair(pBuilder);
                                } else if(pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel()
                                          && !pBuilder->isUpgrading()
                                          && itemCount[Unit_Harvester] >= harvesterLimit) {

                                    doUpgrade(pBuilder);
                                    logDebug("***CampAI Upgrade builder");
                                } else if((getHouse()->getCapacity() < getHouse()->getStoredCredits() + 2000)
                                       && pBuilder->isAvailableToBuild(Structure_Silo)
                                       && findPlaceLocation(Structure_Silo).isValid()
                                       && pBuilder->getProductionQueueSize() == 0){

                                    doProduceItem(pBuilder, Structure_Silo);
                                    itemCount[Structure_Silo]++;

                                    logDebug("***CampAI Build A new Silo increasing count to: %d", itemCount[Structure_Silo]);
                                } else if (money > 3000
                                           && pBuilder->isAvailableToBuild(Structure_RocketTurret)
                                           && findPlaceLocation(Structure_RocketTurret).isValid()
                                           && pBuilder->getProductionQueueSize() == 0
                                           && (itemCount[Structure_RocketTurret] <
                                               (itemCount[Structure_Silo] + itemCount[Structure_Refinery]) * 2)){

                                    doProduceItem(pBuilder, Structure_RocketTurret);
                                    itemCount[Structure_RocketTurret]++;

                                    logDebug("***CampAI Build A new Rocket turret increasing count to: %d", itemCount[Structure_RocketTurret]);
                                }

                                buildTimer = getRandomGen().rand(0,3)*5;
                            } else {
                                // custom AI starts here:

                                Uint32 itemID = NONE_ID;

                                if(itemCount[Structure_WindTrap] == 0) {
                                    itemID = Structure_WindTrap;
                                    itemCount[Structure_WindTrap]++;
                                } else if(itemCount[Structure_Refinery] == 0 || itemCount[Structure_Refinery] < itemCount[Unit_Harvester] / 2) {
                                    itemID = Structure_Refinery;
                                    itemCount[Unit_Harvester]++;
                                    itemCount[Structure_Refinery]++;
                                } else if(itemCount[Structure_Refinery] < 6 - (money / 2000) ) {
                                    itemID = Structure_Refinery;
                                    itemCount[Unit_Harvester]++;
                                    itemCount[Structure_Refinery]++;
                                } else if(itemCount[Structure_StarPort] == 0 && pBuilder->isAvailableToBuild(Structure_StarPort) && findPlaceLocation(Structure_StarPort).isValid()) {
                                    itemID = Structure_StarPort;
                                } else if(itemCount[Unit_Harvester] < (harvesterLimit / 3) && money < 2000
                                        && ((itemCount[Structure_Refinery] < harvesterLimit / 4
                                             && itemCount[Structure_Refinery] < 8)
                                            || itemCount[Structure_HeavyFactory] > 0)) {
                                    // Focus on the economy
                                    itemID = Structure_Refinery;
                                    itemCount[Unit_Harvester]++;
                                } else if(itemCount[Unit_Harvester] < harvesterLimit / 2  && money < 1200) {
                                    itemID = Structure_Refinery;
                                    itemCount[Unit_Harvester]++;
                                    itemCount[Structure_Refinery]++;
                                } else if(itemCount[Structure_LightFactory] == 0 && pBuilder->isAvailableToBuild(Structure_LightFactory)) {
                                    itemID = Structure_LightFactory;
                                } else if(itemCount[Structure_Radar] == 0 && pBuilder->isAvailableToBuild(Structure_Radar)) {
                                    itemID = Structure_Radar;
                                } else if(itemCount[Structure_HeavyFactory] == 0) {
                                    if(pBuilder->isAvailableToBuild(Structure_HeavyFactory)) {
                                        itemID = Structure_HeavyFactory;
                                    }
                                } else if(itemCount[Structure_RepairYard] == 0 && pBuilder->isAvailableToBuild(Structure_RepairYard)) {
                                    itemID = Structure_RepairYard;
                                } else if(money < 2000 && itemCount[Unit_Harvester] < harvesterLimit) {
                                    // Focus on the economy
                                    itemID = Structure_Refinery;
                                    itemCount[Unit_Harvester]++;
                                    itemCount[Structure_Refinery]++;
                                } else if(itemCount[Structure_HighTechFactory] == 0){
                                    if(pBuilder->isAvailableToBuild(Structure_HighTechFactory)) {
                                        itemID = Structure_HighTechFactory;
                                    }
                                } else if(itemCount[Structure_IX] == 0) {
                                    // Let's trial special units
                                    if(pBuilder->isAvailableToBuild(Structure_IX)) {
                                        itemID = Structure_IX;
                                    }
                                } else if(pBuilder->isAvailableToBuild(Structure_RepairYard) && money > 500
                                        && (itemCount[Structure_RepairYard] <= activeRepairYardCount
                                            || itemCount[Structure_RepairYard] * 6000 < militaryValue)) {
                                    // If we have a lot of troops get some repair facilities
                                    itemID = Structure_RepairYard;
                                    //logDebug("Build Repair... active: %d  total: %d", activeRepairYardCount, getHouse()->getNumItems(Structure_RepairYard));

                                } else if(pBuilder->isAvailableToBuild(Structure_HeavyFactory) && money > 500
                                        && (itemCount[Structure_HeavyFactory] <= activeHeavyFactoryCount || money > itemCount[Structure_HeavyFactory]*4000)) {
                                    // If we have a lot of money get more heavy factories
                                    itemID = Structure_HeavyFactory;
                                    logDebug("Build Factory... active: %d  total: %d", activeHeavyFactoryCount, getHouse()->getNumItems(Structure_HeavyFactory));
                                } else if(itemCount[Structure_Refinery] * FixPt(3,5) < itemCount[Unit_Harvester]) {
                                    itemID = Structure_Refinery;
                                } else if (getHouse()->getStoredCredits() + 2000 > (itemCount[Structure_Refinery] + itemCount[Structure_Silo]) * 1000){
                                    // We are running out of spice storage capacity
                                    itemID = Structure_Silo;
                                } else if(money > 1200
                                        && pBuilder->isAvailableToBuild(Structure_Palace)
                                        && getGameInitSettings().getGameOptions().onlyOnePalace
                                        && itemCount[Structure_Palace] == 0) {
                                    // Let's build one palace if its available
                                    itemID = Structure_Palace;
                                } else if(money > 1200
                                        && pBuilder->getCurrentUpgradeLevel() < pBuilder->getMaxUpgradeLevel()
                                        && !getGameInitSettings().getGameOptions().rocketTurretsNeedPower) {
                                    // First off we need to upgrade the construction yard
                                    doUpgrade(pBuilder);
                                } else if(money > 1200
                                          && rocketTurretValue < militaryValueLimit * FixPt(0,10) + itemCount[Structure_Palace] * 750 + itemCount[Structure_Refinery] * 250
                                          && rocketTurretValue < militaryValue * FixPt(0,25) + itemCount[Structure_Palace] * 750 + itemCount[Structure_Refinery] * 250
                                          && pBuilder->isAvailableToBuild(Structure_RocketTurret)) {
                                    // Lets build turrets based on our military value limit, palaces and silo's
                                    itemID = Structure_RocketTurret;
                                } else if(money > militaryValueLimit - militaryValue) {
                                    // Here are our luxury items:
                                    // - Rocket Turrets
                                    // - Palaces
                                    // Need to balance saving credits with expenditure on palaces and turrets

                                    //logDebug("Build Luxury.. money: %d  mildecifict: %d", money, militaryValueLimit - militaryValue);
                                    if(pBuilder->isAvailableToBuild(Structure_Palace)
                                            && !getGameInitSettings().getGameOptions().onlyOnePalace
                                            && itemCount[Structure_Palace] * 1250 < rocketTurretValue
                                            && money > itemCount[Structure_Palace] * 500){
                                        itemID = Structure_Palace;
                                    } else if(pBuilder->isAvailableToBuild(Structure_RocketTurret) && money > rocketTurretValue) {
                                        itemID = Structure_RocketTurret;
                                    }
                                }

                                // TODO: Build concrete if we have bad building spots
                                if(pBuilder->isAvailableToBuild(itemID) && findPlaceLocation(itemID).isValid() && itemID != NONE_ID) {
                                    doProduceItem(pBuilder, itemID);
                                    itemCount[itemID]++;
                                }/*else if(pBuilder->isAvailableToBuild(Structure_Slab1) && findPlaceLocation(Structure_Slab1).isValid()){
                                    doProduceItem(pBuilder, Structure_Slab1);
                                }*/

                            }
                        }

                        if(pBuilder->isWaitingToPlace()) {
                            Coord location = findPlaceLocation(pBuilder->getCurrentProducedItem());

                            if(location.isValid()){
                                doPlaceStructure(pConstYard, location.x, location.y);
                            } else{
                                doCancelItem(pConstYard, pBuilder->getCurrentProducedItem());
                            }
                        }
                    } break;
                }
            }
        }
    }

    buildTimer = getRandomGen().rand(0,3)*5;
}


void QuantBot::scrambleUnitsAndDefend(const ObjectBase* pIntruder) {
    RobustList<const UnitBase*>::const_iterator iter;
    for(iter = getUnitList().begin(); iter != getUnitList().end(); ++iter) {
        const UnitBase* pUnit = *iter;
        if(pUnit->isRespondable() && (pUnit->getOwner() == getHouse())) {

            if(!pUnit->hasATarget() && !pUnit->wasForced()) {
                Uint32 itemID = pUnit->getItemID();
                if((itemID != Unit_Harvester) && (pUnit->getItemID() != Unit_MCV) && (pUnit->getItemID() != Unit_Carryall)
                    && (pUnit->getItemID() != Unit_Frigate) && (pUnit->getItemID() != Unit_Saboteur) && (pUnit->getItemID() != Unit_Sandworm)) {

                    doSetAttackMode(pUnit, AREAGUARD);

                    if(pUnit->getItemID() == Unit_Launcher || pUnit->getItemID() == Unit_Deviator) {
                        //doAttackObject(pUnit, pIntruder, true);
                    } else {
                        doAttackObject(pUnit, pIntruder, true);
                    }

                    if(pUnit->isVisible()
                        && blockDistance(pUnit->getLocation(), pUnit->getDestination()) >= 10
                        && pUnit->isAGroundUnit()
                        && pUnit->getHealth() / pUnit->getMaxHealth() > BADLYDAMAGEDRATIO) {

                        const GroundUnit* pGroundUnit = dynamic_cast<const GroundUnit*>(pUnit);

                        if(getGameInitSettings().getGameOptions().manualCarryallDrops && pGroundUnit->getItemID() != Unit_Deviator && pGroundUnit->getItemID() != Unit_Launcher) {
                            doRequestCarryallDrop(pGroundUnit); //do request carryall to defend unit
                        }
                    }
                }
            }
        }
    }
}


void QuantBot::attack(int militaryValue) {

    /// Logic to make Brutal AI attack more often
    /// not using this atm

    int tempLim = militaryValueLimit;
    if(tempLim > 60000) {
        tempLim = 60000;
    }

    FixPoint strength = (FixPoint(militaryValue) + 1) / (FixPoint(tempLim)) + FixPt(0,03);

    FixPoint newAttack = 15000 / strength;

    if(newAttack > 100000){
        newAttack = 100000;
    }
    // overwriting existing logic for the time being
    attackTimer = MILLI2CYCLES(10000);

    // only attack if we have 35% of maximum military power on max sized map. Required military power scales down accordingly
    if(militaryValue < militaryValueLimit * FixPt(0,35) * currentGameMap->getSizeX() * currentGameMap->getSizeY() / 16384 && militaryValue < 20000) {
        return;
    }

    // In campaign mode don't attack if  the attack trigger isn't set
    // And don't attack with less than 40% of your limit
    if((!campaignAIAttackFlag || militaryValue < militaryValueLimit * FixPt(0,80)) && gameMode == CAMPAIGN) {
        return;
    }

    if(difficulty == DEFEND){
        return;
    }

    logDebug(   "Attack: house: %d  dif: %d  mStr: %d  mLim: %d  strength: %f  attackTimer: %d",
                getHouse()->getHouseID(), difficulty, militaryValue, militaryValueLimit, strength.toFloat(), attackTimer);

    Coord squadCenterLocation = findSquadCenter(getHouse()->getHouseID());

    RobustList<const UnitBase*>::const_iterator iter;
    for(iter = getUnitList().begin(); iter != getUnitList().end(); ++iter) {
        const UnitBase *pUnit = *iter;
        if (pUnit->isRespondable()
            && (pUnit->getOwner() == getHouse())
            && pUnit->isActive()
            && (pUnit->getAttackMode() == AREAGUARD)
            && pUnit->getItemID() != Unit_Harvester
            && pUnit->getItemID() != Unit_MCV
            && pUnit->getItemID() != Unit_Carryall
            && (pUnit->getItemID() != Unit_Ornithopter || getHouse()->getNumItems(Unit_Ornithopter) > 15)
            && (pUnit->getItemID() != Unit_Deviator || getHouse()->getNumItems(Unit_Deviator) > 10)
            && pUnit->getHealth() / pUnit->getMaxHealth() > FixPt(0,6)
            // Only units within the squad should hunt, safety in numbers
            && blockDistance(pUnit->getLocation(), squadCenterLocation) < FixPoint::sqrt(getHouse()->getNumUnits()
                                                                                         - getHouse()->getNumItems(Unit_Harvester)
                                                                                         - getHouse()->getNumItems(Unit_Carryall)
                                                                                         - getHouse()->getNumItems(Unit_Ornithopter)
                                                                                         - getHouse()->getNumItems(Unit_Sandworm)
                                                                                         - getHouse()->getNumItems(Unit_MCV)) + 6)
        {
            doSetAttackMode(pUnit, HUNT);
        }
    }

}


Coord QuantBot::findSquadRallyLocation() {
    int buildingCount = 0;
    int totalX = 0;
    int totalY = 0;

    int enemyBuildingCount = 0;
    int enemyTotalX = 0;
    int enemyTotalY = 0;

    RobustList<const StructureBase*>::const_iterator currentStructure;
    for(currentStructure = getStructureList().begin(); currentStructure != getStructureList().end(); ++currentStructure) {
        const StructureBase* pCurrentStructure = *currentStructure;
        if(pCurrentStructure->getOwner()->getHouseID() == getHouse()->getHouseID()) {
            // Lets find the center of mass of our squad
            buildingCount ++;
            totalX += pCurrentStructure->getX();
            totalY += pCurrentStructure->getY();
        } else if(pCurrentStructure->getOwner()->getTeam() != getHouse()->getTeam()) {
            enemyBuildingCount ++;
            enemyTotalX += pCurrentStructure->getX();
            enemyTotalY += pCurrentStructure->getY();
        }
    }

    Coord baseCentreLocation = Coord::Invalid();
    if(enemyBuildingCount > 0 && buildingCount > 0) {
        baseCentreLocation.x = lround((totalX / buildingCount) * FixPt(0,75) + (enemyTotalX / enemyBuildingCount) * FixPt(0,25));
        baseCentreLocation.y = lround((totalY / buildingCount) * FixPt(0,75) + (enemyTotalY / enemyBuildingCount) * FixPt(0,25));
    }

    //logDebug("Squad rally location: %d, %d", baseCentreLocation.x , baseCentreLocation.y );

    return baseCentreLocation;
}

Coord QuantBot::findBaseCentre(int houseID) {
    int buildingCount = 0;
    int totalX = 0;
    int totalY = 0;

    RobustList<const StructureBase*>::const_iterator currentStructure;
    for(currentStructure = getStructureList().begin(); currentStructure != getStructureList().end(); ++currentStructure) {
        const StructureBase* pCurrentStructure = *currentStructure;
        if(pCurrentStructure->getOwner()->getHouseID() == houseID && pCurrentStructure->getStructureSizeX() != 1) {
            // Lets find the center of mass of our squad
            buildingCount++;
            totalX += pCurrentStructure->getX();
            totalY += pCurrentStructure->getY();
        }
    }

    Coord baseCentreLocation = Coord::Invalid();

    if(buildingCount > 0) {
        baseCentreLocation.x = totalX / buildingCount;
        baseCentreLocation.y = totalY / buildingCount;
    }

    return baseCentreLocation;
}


Coord QuantBot::findSquadCenter(int houseID) {
    int squadSize = 0;

    int totalX = 0;
    int totalY = 0;


    RobustList<const UnitBase*>::const_iterator currentUnit;

    for(currentUnit = getUnitList().begin(); currentUnit != getUnitList().end(); ++currentUnit) {
        const UnitBase* pCurrentUnit = *currentUnit;

        if(pCurrentUnit->getOwner()->getHouseID()  == houseID
            && pCurrentUnit->getItemID() != Unit_Carryall
            && pCurrentUnit->getItemID() != Unit_Harvester
            && pCurrentUnit->getItemID() != Unit_Frigate
            && pCurrentUnit->getItemID() != Unit_MCV

            // Stop freeman making tanks roll forward
            && !(currentGame->techLevel > 6 && pCurrentUnit->getItemID() == Unit_Trooper)
            && pCurrentUnit->getItemID() != Unit_Saboteur
            && pCurrentUnit->getItemID() != Unit_Sandworm

            // Don't let troops moving to rally point contribute
            && pCurrentUnit->getAttackMode() != RETREAT
            && pCurrentUnit->getDestination().x != squadRallyLocation.x
            && pCurrentUnit->getDestination().y != squadRallyLocation.y) {

            // Lets find the center of mass of our squad
            squadSize++;
            totalX += pCurrentUnit->getX();
            totalY += pCurrentUnit->getY();
        }

    }

    Coord squadCenterLocation = Coord::Invalid();

    if(squadSize > 0) {
        squadCenterLocation.x = totalX / squadSize;
        squadCenterLocation.y = totalY / squadSize;
    }

    return squadCenterLocation;
}

/**
    Set a rally / retreat location for all our military units.
    This should be near our base but within it
    The retreat mode causes all our military units to move
    to this squad rally location

*/
void QuantBot::retreatAllUnits() {

    // Set the new squad rally location
    squadRallyLocation = findSquadRallyLocation();

    // set attck timer down a bit
    retreatTimer = MILLI2CYCLES(90000);

    FixPoint closestDistance = FixPt_MAX;
    RobustList<StructureBase*>::const_iterator iter;
    for(iter = structureList.begin(); iter != structureList.end(); ++iter) {
        StructureBase* tempStructure = *iter;

        // if it is our building, check to see if it is closer to the squad rally point then we are
        if(tempStructure->getOwner()->getHouseID() == getHouse()->getHouseID()) {
            Coord closestStructurePoint = tempStructure->getClosestPoint(squadRallyLocation);
            FixPoint structureDistance = blockDistance(squadRallyLocation, closestStructurePoint);

            if(structureDistance < closestDistance) {
                closestDistance = structureDistance;
                squadRetreatLocation = closestStructurePoint;
            }
        }
    }

    if(getHouse()->getNumStructures() == 0){
        squadRetreatLocation.x = -1;
        squadRetreatLocation.y = -1;
    }

    // If no base exists yet, there is no retreat location
    if(squadRallyLocation.x != -1 && squadRetreatLocation.x != -1){
        RobustList<const UnitBase*>::const_iterator iter;
        for(iter = getUnitList().begin(); iter != getUnitList().end(); ++iter) {
            const UnitBase* pUnit = *iter;

            if(pUnit->getOwner() == getHouse()
               && pUnit->getItemID() != Unit_Carryall
               && pUnit->getItemID() != Unit_Sandworm
               && pUnit->getItemID() != Unit_Harvester
               && pUnit->getItemID() != Unit_MCV
               && pUnit->getItemID() != Unit_Frigate){

                doSetAttackMode(pUnit, RETREAT);
            }
        }
    }
}


/**
    In dune it is best to mass military units in one location.
    This function determines a squad leader by finding the unit with the most central location
    Amongst all of a players units.

    Rocket launchers and Ornithopters are excluded from having this role as on the
    battle field these units should always have other supporting units to work with

*/
void QuantBot::checkAllUnits() {
    Coord squadCenterLocation = findSquadCenter(getHouse()->getHouseID());

    RobustList<const UnitBase*>::const_iterator iter;
    for(iter = getUnitList().begin(); iter != getUnitList().end(); ++iter) {
        const UnitBase* pUnit = *iter;

        if(pUnit->getOwner() == getHouse()) {
            switch(pUnit->getItemID()) {
                case Unit_MCV: {
                    const MCV* pMCV = dynamic_cast<const MCV*>(pUnit);
                    if(pMCV != nullptr) {
                        //logDebug("MCV: forced: %d  moving: %d  canDeploy: %d",
                        //pMCV->wasForced(), pMCV->isMoving(), pMCV->canDeploy());

                        if (pMCV->canDeploy() && !pMCV->wasForced() && !pMCV->isMoving()) {
                            //logDebug("MCV: Deployed");
                            doDeploy(pMCV);
                        } else if(!pMCV->isMoving() && !pMCV->wasForced()) {
                            Coord pos = findMcvPlaceLocation(pMCV);
                            doMove2Pos(pMCV, pos.x, pos.y, true);
                            /*
                            if(getHouse()->getNumItems(Unit_Carryall) > 0){
                                doRequestCarryallDrop(pMCV);
                            }*/
                        }
                    }
                } break;

                case Unit_Harvester: {
                    const Harvester* pHarvester = dynamic_cast<const Harvester*>(pUnit);
                    if(getHouse()->getCredits() < 1000 && pHarvester->getAmountOfSpice() >= HARVESTERMAXSPICE/2
                       && getHouse()->getNumItems(Structure_HeavyFactory) == 0 && pHarvester != nullptr) {
                        doReturn(pHarvester);
                    }
                } break;

                case Unit_Carryall: {
                } break;

                case Unit_Frigate: {
                } break;

                case Unit_Sandworm: {
                } break;

                default: {

                    int squadRadius = lround(FixPoint::sqrt(getHouse()->getNumUnits()
                                                            - getHouse()->getNumItems(Unit_Harvester)
                                                            - getHouse()->getNumItems(Unit_Carryall)
                                                            - getHouse()->getNumItems(Unit_Ornithopter)
                                                            - getHouse()->getNumItems(Unit_Sandworm)
                                                            - getHouse()->getNumItems(Unit_MCV))) + 1;

                    if(pUnit->getOwner()->getHouseID() != pUnit->getOriginalHouseID()) {
                        // If its a devastator and its not ours, blow it up!!
                        if(pUnit->getItemID() == Unit_Devastator){
                            const Devastator* pDevastator = dynamic_cast<const Devastator*>(pUnit);
                            if(pDevastator != nullptr) {

                                doStartDevastate(pDevastator);
                                doSetAttackMode(pDevastator, HUNT);
                            }
                        } else if(pUnit->getItemID() == Unit_Ornithopter) {
                            if(pUnit->getAttackMode() != HUNT){
                                doSetAttackMode(pUnit, HUNT);
                            }
                        } else if(pUnit->getItemID() == Unit_Harvester) {
                            const Harvester* pHarvester = dynamic_cast<const Harvester*>(pUnit);
                            if(pHarvester->getAmountOfSpice() >= HARVESTERMAXSPICE/5 && pHarvester != nullptr) {
                                doReturn(pHarvester);
                            } else {
                                doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, true );
                            }
                        } else {
                            // Send deviated unit to squad centre
                            if(pUnit->getAttackMode() != AREAGUARD) {
                                doSetAttackMode(pUnit, AREAGUARD);
                            }

                            if(blockDistance(pUnit->getLocation(), squadCenterLocation) > squadRadius - 1) {
                                doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, true );
                            }
                        }
                    } else if((pUnit->getItemID() == Unit_Launcher || pUnit->getItemID() == Unit_Deviator || pUnit->getItemID() == Unit_SonicTank) && pUnit->hasATarget()) {
                        // Special logic to keep launchers away from harm
                        if(pUnit->getTarget() != nullptr){
                            if(blockDistance(pUnit->getLocation(), pUnit->getTarget()->getLocation()) <= 5 && pUnit->getTarget()->getItemID() != Unit_Ornithopter) {
                                doSetAttackMode(pUnit, AREAGUARD);
                                doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, true );
                            }
                        }
                    } else if(pUnit->getAttackMode() != HUNT && !pUnit->hasATarget() && !pUnit->wasForced()) {
                        if(pUnit->getAttackMode() == AREAGUARD && squadCenterLocation.isValid()) {
                           if(blockDistance(pUnit->getLocation(), squadCenterLocation) > squadRadius) {
                                if(!pUnit->hasATarget()){
                                    doMove2Pos(pUnit, squadCenterLocation.x, squadCenterLocation.y, false );
                                }
                            }
                        } else if (pUnit->getAttackMode() == RETREAT) {
                            if(blockDistance(pUnit->getLocation(), squadRetreatLocation) > squadRadius + 2 && !pUnit->wasForced()) {
                               if(pUnit->getHealth() < pUnit->getMaxHealth()) {
                                   doRepair(pUnit);
                               }
                               doMove2Pos(pUnit, squadRetreatLocation.x, squadRetreatLocation.y, true );
                            } else {
                                // We have finished retreating back to the rally point
                                doSetAttackMode(pUnit, AREAGUARD);
                            }
                        } else if (pUnit->getAttackMode() == GUARD
                                   && ((pUnit->getDestination().x != squadRallyLocation.x || pUnit->getDestination().y != squadRallyLocation.y)
                                       || (blockDistance(pUnit->getLocation(),squadRallyLocation) <= squadRadius))) {
                            // A newly deployed unit has reached the rally point, or has been diverted => Change it to area guard
                            doSetAttackMode(pUnit, AREAGUARD);
                        }
                    } else if (pUnit->getAttackMode() == HUNT
                             && attackTimer > MILLI2CYCLES(250000)
                             && pUnit->getItemID() != Unit_Trooper
                             && pUnit->getItemID() != Unit_Saboteur
                             && pUnit->getItemID() != Unit_Sandworm){
                        doSetAttackMode(pUnit, AREAGUARD);
                    }
                } break;
            }
        }
    }
}
