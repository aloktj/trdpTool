#include "trdp/config.hpp"
#include "trdp/logging.hpp"
#include "trdp/md.hpp"
#include "trdp/pd.hpp"
#include "trdp/session.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

using namespace trdp;

namespace
{
std::atomic_bool running{true};

void handleSignal(int)
{
    running.store(false);
}

void repl(PdEngine &pd, MdEngine &md)
{
    std::string line;
    std::cout << "Type 'help' for commands" << std::endl;
    while (running.load() && std::getline(std::cin, line))
    {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd == "quit" || cmd == "exit")
        {
            running.store(false);
            break;
        }
        else if (cmd == "help")
        {
            std::cout << "Commands:\n"
                      << "  list-pd-pub\n  list-pd-sub\n  set-pd-value <index> <element> <value>\n  clear-pd-pub <index>\n"
                      << "  list-md\n  set-md-value <name> <element> <value>\n  clear-md <name>\n  send-md <name>\n"
                      << std::endl;
        }
        else if (cmd == "list-pd-pub")
        {
            pd.listPublish(std::cout);
        }
        else if (cmd == "list-pd-sub")
        {
            pd.listSubscribe(std::cout);
        }
        else if (cmd == "set-pd-value")
        {
            std::size_t idx;
            std::string element, value;
            if (iss >> idx >> element >> value)
            {
                if (!pd.setPublishValue(idx, element, value))
                {
                    std::cout << "Failed to set value" << std::endl;
                }
            }
            else
            {
                std::cout << "Usage: set-pd-value <index> <element> <value>" << std::endl;
            }
        }
        else if (cmd == "clear-pd-pub")
        {
            std::size_t idx;
            if (iss >> idx)
            {
                pd.clearPublish(idx);
            }
        }
        else if (cmd == "list-md")
        {
            md.listTemplates(std::cout);
        }
        else if (cmd == "set-md-value")
        {
            std::string name, element, value;
            if (iss >> name >> element >> value)
            {
                if (!md.setTemplateValue(name, element, value))
                {
                    std::cout << "Failed to update MD template" << std::endl;
                }
            }
            else
            {
                std::cout << "Usage: set-md-value <name> <element> <value>" << std::endl;
            }
        }
        else if (cmd == "clear-md")
        {
            std::string name;
            if (iss >> name)
            {
                md.clearTemplate(name);
            }
        }
        else if (cmd == "send-md")
        {
            std::string name;
            if (iss >> name)
            {
                if (!md.sendTemplate(name, std::cout))
                {
                    std::cout << "Unknown template" << std::endl;
                }
            }
        }
        else if (!cmd.empty())
        {
            std::cout << "Unknown command: " << cmd << std::endl;
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    std::signal(SIGINT, handleSignal);

    const std::string deviceFile = (argc > 1) ? argv[1] : "apps/trdp-sim/example-device.xml";

    XmlConfigLoader loader;
    auto config = loader.loadFromDeviceConfig(deviceFile, "", "", "");
    if (!config)
    {
        error("Failed to load configuration");
        return 1;
    }

    TrdpSession session;
    session.init();
    session.open();

    PdEngine pd(*config);
    MdEngine md(*config);

    std::thread worker([&]() { session.runLoop(running); });
    repl(pd, md);
    running.store(false);
    worker.join();
    session.close();

    return 0;
}

