#include "KBot.h"

#include <cassert>
#include <iostream>
#include <random>
#include "utils.h"

namespace KBot {

    using namespace BWAPI;

    KBot::KBot() : m_map(BWEM::Map::Instance()), m_manager(*this), m_general(*this) {}

    // Called only once at the beginning of a game.
    void KBot::onStart() {
        if (Broodwar->isReplay()) return;

        // This bot is written for Terran, so make sure we are indeed Terran!
        if (Broodwar->self()->getRace() != Races::Terran) {
            Broodwar << "Wrong race selected! KBot has to be Terran." << std::endl;
            Broodwar->leaveGame();
        }

        // This bot is written for one-on-one melee games.
        if (Broodwar->getGameType() != GameTypes::Melee) {
            Broodwar << "Unexpected game type. KBot might not work correctly." << std::endl;
            // Unfortunate, but no reason to leave.
        }

        // Set the command optimization level so that common commands can be grouped.
        Broodwar->setCommandOptimizationLevel(2);

        // BWEM map initialization
        m_map.Initialize();
        m_map.EnableAutomaticPathAnalysis();
        const bool r = m_map.FindBasesForStartingLocations();
        assert(r);

        // Create initial base
        m_manager.createBase(Broodwar->self()->getStartLocation());

        // Ready to go. Good luck, have fun!
        Broodwar->sendText("gl hf");
    }

    // Called once at the end of a game.
    void KBot::onEnd(bool isWinner) {}

    // Called once for every execution of a logical frame in Broodwar.
    void KBot::onFrame() {
        // Return if the game is a replay or is paused
        if (Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self())
            return;

        // Display some debug information
        Broodwar->drawTextScreen(2, 0, "FPS: %d, APM: %d", Broodwar->getFPS(), Broodwar->getAPM());
        Broodwar->drawTextScreen(2, 10, "Enemy position count: %d", m_enemyPositions.size());

        if (!m_enemyPositions.empty()) {
            const auto enemyPosition = m_enemyPositions.front();
            Broodwar->drawTextScreen(2, 20, "Next enemy position: (%d, %d)", enemyPosition.x, enemyPosition.y);
        }
        else
            Broodwar->drawTextScreen(2, 20, "Next enemy position: Unknown");

        Broodwar->drawTextScreen(200, 0, "Under construction:");
        for (std::size_t i = 0; i < m_underConstruction.size(); ++i) {
            if (!m_underConstruction[i]->exists()) {
                // TODO: Might need a change if this is used somewhere else as well.
                m_underConstruction.erase(m_underConstruction.begin() + i--);
                continue;
            }
            const auto type = m_underConstruction[i]->getType();
            const int progress = 100 - (100 * m_underConstruction[i]->getRemainingBuildTime() / type.buildTime());
            Broodwar->drawTextScreen(200, 10 * (i + 1), " - %s (%d %%)", type.c_str(), progress);
        }

#ifndef _DEBUG
        // Draw map (to slow for debug mode)
        //BWEM::utils::drawMap(m_map);
#endif // !_DEBUG

        // Update enemy positions
        while (!m_enemyPositions.empty() && Broodwar->isVisible(m_enemyPositions.front()) && Broodwar->getUnitsOnTile(m_enemyPositions.front(), Filter::IsEnemy).empty())
            m_enemyPositions.erase(m_enemyPositions.begin());

        // Update manager
        m_manager.update();

        // Update general
        m_general.update();

        // Prevent spamming by only running our onFrame once every number of latency frames.
        // Latency frames are the number of frames before commands are processed.
        if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
            return;
    }

    // Called when the user attempts to send a text message.
    void KBot::onSendText(std::string text) {}

    // Called when the client receives a message from another Player.
    void KBot::onReceiveText(BWAPI::Player player, std::string text) {}

    // Called when a Player leaves the game.
    void KBot::onPlayerLeft(BWAPI::Player player) {}

    // Called when a Nuke has been launched somewhere on the map.
    void KBot::onNukeDetect(BWAPI::Position target) {}

    // Called when a Unit becomes accessible.
    void KBot::onUnitDiscover(BWAPI::Unit unit) {}

    // Called when a Unit becomes inaccessible.
    void KBot::onUnitEvade(BWAPI::Unit unit) {}

    // Called when a previously invisible unit becomes visible.
    void KBot::onUnitShow(BWAPI::Unit unit) {
        // Update enemy positions
        if (Broodwar->self()->isEnemy(unit->getPlayer()) && unit->getType().isBuilding()) {
            const auto myPosition = Broodwar->self()->getStartLocation();
            TilePosition position(unit->getPosition());
            if (std::find(m_enemyPositions.begin(), m_enemyPositions.end(), position) == m_enemyPositions.end()) {
                const auto it = std::lower_bound(m_enemyPositions.begin(), m_enemyPositions.end(), position,
                    [&](TilePosition a, TilePosition b) { return distance(myPosition, a) < distance(myPosition, b); });
                m_enemyPositions.insert(it, position);
            }
        }
    }

    // Called just as a visible unit is becoming invisible.
    void KBot::onUnitHide(BWAPI::Unit unit) {}

    // Called when any unit is created.
    void KBot::onUnitCreate(BWAPI::Unit unit) {
        assert(unit->exists());

        if (unit->getPlayer() == Broodwar->self())
            m_underConstruction.push_back(unit);
    }

    // Called when a unit is removed from the game either through death or other means.
    void KBot::onUnitDestroy(BWAPI::Unit unit) {
        assert(!unit->exists());

        if (unit->getPlayer() == Broodwar->self())
            // Dispatch
            if (unit->getType().isBuilding() || unit->getType().isWorker())
                m_manager.onUnitDestroy(unit);
            else
                m_general.onUnitDestroy(unit);

        // Update BWEM information
        if (unit->getType().isMineralField())
            m_map.OnMineralDestroyed(unit);
        else if (unit->getType().isSpecialBuilding())
            m_map.OnStaticBuildingDestroyed(unit);
    }

    // Called when a unit changes its UnitType.
    void KBot::onUnitMorph(BWAPI::Unit unit) {
        // For example, when a Drone transforms into a Hatchery, a Siege Tank uses Siege Mode, or a Vespene Geyser receives a Refinery.
    }

    // Called when a unit changes ownership.
    void KBot::onUnitRenegade(BWAPI::Unit unit) {
        // This occurs when the Protoss ability Mind Control is used, or if a unit changes ownership in Use Map Settings.
    }

    // Called when the state of the Broodwar game is saved to file.
    void KBot::onSaveGame(std::string gameName) {
        Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
    }

    // Called when the state of a unit changes from incomplete to complete.
    void KBot::onUnitComplete(BWAPI::Unit unit) {
        assert(unit->exists());

        if (unit->getPlayer() == Broodwar->self()) {
            const auto it = std::find(m_underConstruction.begin(), m_underConstruction.end(), unit);
            assert(it != m_underConstruction.end());
            m_underConstruction.erase(it);

            // Dispatch
            if (unit->getType().isBuilding() || unit->getType().isWorker())
                m_manager.transferOwnership(unit);
            else
                m_general.transferOwnership(unit);
        }
    }

    TilePosition KBot::getNextBasePosition() const {
        std::vector<TilePosition> positions;
        for (const auto &area : m_map.Areas()) {
            for (const auto &base : area.Bases())
                if (std::all_of(m_manager.getBases().begin(), m_manager.getBases().end(),
                    [&base](const Base &b) { return b.getPosition() != base.Location(); }))
                    positions.push_back(base.Location());
        }

        // Order position by and distance to own starting base.
        std::sort(positions.begin(), positions.end(), [this](TilePosition a, TilePosition b) {
            const auto startPosition = manager().getBases().front().getPosition();
            return distance(startPosition, a) < distance(startPosition, b);
        });

        return positions.front();
    }

    TilePosition KBot::getNextEnemyPosition() const {
        if (!m_enemyPositions.empty())
            return m_enemyPositions.front();
        else {
            // TODO: Update to multiple own bases concept?
            // Positions to scout
            auto positions = m_map.StartingLocations();
            if (std::all_of(positions.begin(), positions.end(), [](const TilePosition &p) { return Broodwar->isExplored(p); })) {
                positions.clear();
                for (const auto &area : m_map.Areas())
                    for (const auto &base : area.Bases())
                        positions.push_back(base.Location());
            }

            // Always exclude our own base.
            const auto myPosition = Broodwar->self()->getStartLocation();
            const auto it = std::find(positions.begin(), positions.end(), myPosition);
            assert(it != positions.end());
            positions.erase(it);

            // Order positions by isExplored and distance to own base.
            std::sort(positions.begin(), positions.end(), [&](TilePosition a, TilePosition b) {
                return distance(myPosition, a) < distance(myPosition, b);
            });
            std::stable_sort(positions.begin(), positions.end(), [](TilePosition a, TilePosition b) {
                return Broodwar->isExplored(a) < Broodwar->isExplored(b);
            });
            if (!Broodwar->isExplored(positions.front()))
                return positions.front();

            // If all positions are already explored, return a random position.
            static std::default_random_engine generator;
            std::uniform_int_distribution<int> dist(0, positions.size() - 1);
            return positions[dist(generator)];
        }
    }

} // namespace
