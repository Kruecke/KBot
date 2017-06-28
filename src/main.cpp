#include "KBot.h"
#include <BWAPI.h>
#include <BWAPI/Client.h>
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, const char **argv) {
    // Connecting to server...
    while (!BWAPI::BWAPIClient.connect()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{300});
    }

    // Main loop
    while (BWAPI::BWAPIClient.isConnected()) {
        // Waiting for a game to begin...
        std::cout << "Waiting for a game to begin..." << std::endl;
        while (!BWAPI::Broodwar->isInGame())
            BWAPI::BWAPIClient.update(); // push/pull server

        // if (Broodwar->isReplay()) ... // TODO: Handle here?

        std::cout << "Game ready!" << std::endl;
        KBot::KBot bot;

        while (BWAPI::Broodwar->isInGame()) {
            for (auto &e : BWAPI::Broodwar->getEvents()) {
                switch (e.getType()) {
                case BWAPI::EventType::MatchStart:
                    bot.onStart();
                    break;
                case BWAPI::EventType::MatchEnd:
                    bot.onEnd(e.isWinner());
                    break;
                case BWAPI::EventType::MatchFrame:
                    bot.onFrame();
                    break;
                case BWAPI::EventType::MenuFrame:
                    bot.onFrame(); // TODO: skip these?
                    break;
                case BWAPI::EventType::SendText:
                    bot.onSendText(e.getText());
                    break;
                case BWAPI::EventType::ReceiveText:
                    bot.onReceiveText(e.getPlayer(), e.getText());
                    break;
                case BWAPI::EventType::PlayerLeft:
                    bot.onPlayerLeft(e.getPlayer());
                    break;
                case BWAPI::EventType::NukeDetect:
                    bot.onNukeDetect(e.getPosition());
                    break;
                case BWAPI::EventType::UnitDiscover:
                    bot.onUnitDiscover(e.getUnit());
                    break;
                case BWAPI::EventType::UnitEvade:
                    bot.onUnitEvade(e.getUnit());
                    break;
                case BWAPI::EventType::UnitShow:
                    bot.onUnitShow(e.getUnit());
                    break;
                case BWAPI::EventType::UnitHide:
                    bot.onUnitHide(e.getUnit());
                    break;
                case BWAPI::EventType::UnitCreate:
                    bot.onUnitCreate(e.getUnit());
                    break;
                case BWAPI::EventType::UnitDestroy:
                    bot.onUnitDestroy(e.getUnit());
                    break;
                case BWAPI::EventType::UnitMorph:
                    bot.onUnitMorph(e.getUnit());
                    break;
                case BWAPI::EventType::UnitRenegade:
                    bot.onUnitRenegade(e.getUnit());
                    break;
                case BWAPI::EventType::SaveGame:
                    bot.onSaveGame(e.getText());
                    break;
                case BWAPI::EventType::UnitComplete:
                    bot.onUnitComplete(e.getUnit());
                    break;
                default:
                    // FIXME: This will never happen, right?
                    assert(false);
                }
            }

            // Push/pull information to/from the server
            BWAPI::BWAPIClient.update();
        } // end while (BWAPI::Broodwar->isInGame())
        std::cout << "Game ended!" << std::endl;
    }
    std::cout << "Press ENTER to continue..." << std::endl;
    std::cin.ignore();

    return 0;
}