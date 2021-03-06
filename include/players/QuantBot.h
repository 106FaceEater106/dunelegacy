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

#ifndef QuantBot_H
#define QuantBot_H

#include <players/Player.h>
#include <units/MCV.h>

#include <DataTypes.h>

class QuantBot : public Player
{
public:



    typedef enum {
        EASY = 0,
        MEDIUM = 1,
        HARD = 2,
        BRUTAL = 3,
        DEFEND = 4
    } enum_difficulty;

    typedef enum {
        CUSTOM = 4,
        CAMPAIGN = 5
    } enum_gameMode;

    void init();
    ~QuantBot();
    virtual void save(OutputStream& stream) const;

    virtual void update();

    virtual void onIncrementStructures(int itemID);
    virtual void onDecrementStructures(int itemID, const Coord& location);
    virtual void onDecrementUnits(int itemID);
    virtual void onIncrementUnitKills(int itemID);
    virtual void onDamage(const ObjectBase* pObject, int damage, Uint32 damagerID);

    static Player* create(House* associatedHouse, std::string playername, Uint32 difficulty) {
        return new QuantBot(associatedHouse, playername, difficulty);
    }

    static Player* load(InputStream& stream, House* associatedHouse) {
        return new QuantBot(stream, associatedHouse);
    }

private:
    QuantBot(House* associatedHouse, std::string playername, Uint32 difficulty);
    QuantBot(InputStream& stream, House* associatedHouse);


    Uint32  difficulty;     ///< difficulty level
    Uint32  gameMode;       ///< game mode (custom or campaign)
    Sint32  buildTimer;     ///< When to build the next structure/unit
    Sint32  attackTimer;    ///< When to build the next structure/unit
    Sint32  retreatTimer;   ///< When you last retreated>

    int initialItemCount[Num_ItemID];
    int initialMilitaryValue;
    int militaryValueLimit;
    int harvesterLimit;
    bool campaignAIAttackFlag;
    Coord squadRallyLocation;
    Coord squadRetreatLocation;

    void scrambleUnitsAndDefend(const ObjectBase* pIntruder);


    Coord findMcvPlaceLocation(const MCV* pMCV);
    Coord findPlaceLocation(Uint32 itemID);
    Coord findSquadCenter(int houseID);
    Coord findBaseCentre(int houseID);
    Coord findSquadRallyLocation();

    std::list<Coord> placeLocations;    ///< Where to place structures

    void checkAllUnits();
    void retreatAllUnits();
    void build(int militaryValue);
    void attack(int militaryValue);

};

#endif //QuantBot_H
