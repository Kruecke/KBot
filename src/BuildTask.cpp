#include "BuildTask.h"

#include "Manager.h"
#include <cassert>

namespace KBot {

    using namespace BWAPI;

    BuildTask::BuildTask(Manager &manager,
        const UnitType &toBuild, const BuildPriority priority,
        const TilePosition &position, const bool exactPosition)
        : m_manager(&manager), m_toBuild(toBuild), m_priority(priority),
        m_position(position), m_exactPosition(exactPosition) {
    }

    void BuildTask::update() {
        switch (m_state) {
            case State::initialize:
                // TODO: Make checks and stuff...
                m_state = State::acquireResources; // go to next state
                break;
            case State::acquireResources:
                if (m_manager->acquireResources(m_toBuild.mineralPrice(), m_toBuild.gasPrice()))
                    m_state = State::acquireWorker; // go to next state
                break;
            case State::acquireWorker:
                if (m_worker = m_manager->acquireWorker(m_toBuild.whatBuilds().first, Position(m_position))) {
                    // Go to next state
                    if (m_toBuild.isBuilding())
                        m_state = State::moveToPosition;
                    else
                        m_state = State::startBuild;
                }
                break;
            case State::moveToPosition:
            {
                if (!m_allocatedBuildPosition) {
                    m_buildPosition = m_exactPosition ? m_position :
                        Broodwar->getBuildLocation(m_toBuild, m_position);
                    m_allocatedBuildPosition = true;
                }
                assert(m_worker);
                const Position position(m_buildPosition);
                if (m_worker->getOrder() != Orders::Move || m_worker->getOrderTargetPosition() != position)
                    m_worker->move(position);
                if (m_worker->getDistance(position) < 20)
                    m_state = State::startBuild; // go to next state
                break;
            }
            case State::startBuild:
                // FIXME: Non-Buildings...
                if (Broodwar->canBuildHere(m_buildPosition, m_toBuild, m_worker)) {
                    if (m_worker->build(m_toBuild, m_buildPosition))
                        m_state = State::waitForUnit; // go to next state
                } else {
                    m_allocatedBuildPosition = false;
                    m_state = State::moveToPosition; // go back and try again
                }
                break;
            case State::waitForUnit:
                if (m_buildingUnit) {
                    m_manager->releaseResources(m_toBuild.mineralPrice(), m_toBuild.gasPrice());
                    m_state = State::building; // go to next state
                }
            case State::building:
            case State::finalize:
                // TODO...
                break;
            default:
                throw std::logic_error("Unknown BuildTask::State!");
        }
    }

    bool BuildTask::onUnitCreated(const Unit &unit) {
        if (unit->getType() == m_toBuild) {
            m_buildingUnit = unit;
            return true;
        }
        return false;
    }

    void BuildTask::onUnitDestroyed(const Unit &unit) {
        // TODO
    }

    std::string BuildTask::toString() const {
        switch (m_state) {
            case State::initialize:
                return m_toBuild.getName() + ": Initialization";
            case State::acquireResources:
                return m_toBuild.getName() + ": Acquiring resources...";
            case State::acquireWorker:
                return m_toBuild.getName() + ": Acquiring worker...";
            case State::moveToPosition:
                return m_toBuild.getName() + ": Moving to position...";
            case State::startBuild:
            case State::waitForUnit:
                return m_toBuild.getName() + ": Start building...";
            case State::building:
            {
                const int progress = 100 - (100 * m_buildingUnit->getRemainingBuildTime() / m_toBuild.buildTime());
                return m_toBuild.getName() + " (" + std::to_string(progress) + " %)";
            }
            case State::finalize:
                return m_toBuild.getName() + ": Finalization";
            default:
                throw std::logic_error("Unknown BuildTask::State!");
        }
    }

} // namespace