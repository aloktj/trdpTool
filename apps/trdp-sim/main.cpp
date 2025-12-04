#include "trdp/config.hpp"
#include "trdp/logging.hpp"
#include "trdp/md.hpp"
#include "trdp/pd.hpp"
#include "trdp/session.hpp"
#include "trdp/tau.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace trdp;

namespace
{
std::atomic_bool running{true};
std::mutex engineMutex;

std::string toHex(const std::vector<uint8_t> &data)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const auto byte : data)
    {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::string urlDecode(const std::string &value)
{
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '%' && i + 2 < value.size())
        {
            const std::string hex = value.substr(i + 1, 2);
            char decoded = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            result.push_back(decoded);
            i += 2;
        }
        else if (value[i] == '+')
        {
            result.push_back(' ');
        }
        else
        {
            result.push_back(value[i]);
        }
    }
    return result;
}

std::string renderValuesJson(const ElementValues &values)
{
    std::ostringstream oss;
    oss << "[";
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        const auto &val = values[i];
        if (i > 0)
        {
            oss << ",";
        }
        oss << "{\"name\":\"" << val.element.name << "\",";
        oss << "\"type\":\"" << toString(val.element.type) << "\",";
        oss << "\"size\":" << val.rawValue.size() << ",";
        oss << "\"locked\":" << (val.locked ? "true" : "false") << ",";
        oss << "\"bytes\":\"" << toHex(val.rawValue) << "\"}";
    }
    oss << "]";
    return oss.str();
}

std::string renderPublishJson(const PdEngine &pd)
{
    std::ostringstream oss;
    oss << "{\"publish\":[";
    const auto &publish = pd.publishTelegrams();
    for (std::size_t i = 0; i < publish.size(); ++i)
    {
        const auto &pub = publish[i];
        if (i > 0)
        {
            oss << ",";
        }
        oss << "{\"index\":" << i << ",\"comId\":" << pub.comId << ",\"datasetId\":" << pub.datasetId << ",";
        oss << "\"destination\":\"" << pub.destinationIp << "\",\"cycleTimeMs\":" << pub.cycleTimeMs << ",";
        oss << "\"values\":" << renderValuesJson(pub.values) << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string renderMdJson(const MdEngine &md)
{
    std::ostringstream oss;
    oss << "{\"templates\":[";
    const auto &templates = md.templates();
    for (std::size_t i = 0; i < templates.size(); ++i)
    {
        const auto &tpl = templates[i];
        if (i > 0)
        {
            oss << ",";
        }
        oss << "{\"name\":\"" << tpl.name << "\",\"comId\":" << tpl.comId << ",\"datasetId\":"
            << tpl.datasetId << ",";
        oss << "\"destination\":\"" << tpl.destinationIp << ":" << tpl.destinationPort << "\",";
        oss << "\"direction\":" << static_cast<int>(tpl.direction) << ",";
        oss << "\"values\":" << renderValuesJson(tpl.values) << "}";
    }
    oss << "]}";
    return oss.str();
}

std::optional<std::string> parseJsonString(const std::string &body, const std::string &key)
{
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(body, match, re) && match.size() > 1)
    {
        return match[1].str();
    }
    return std::nullopt;
}

std::optional<bool> parseJsonBool(const std::string &body, const std::string &key)
{
    std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (std::regex_search(body, match, re) && match.size() > 1)
    {
        return match[1].str() == "true";
    }
    return std::nullopt;
}

struct HttpRequest
{
    std::string method;
    std::string path;
    std::string body;
};

class SimpleHttpServer
{
public:
    SimpleHttpServer(PdEngine &pd, MdEngine &md, TrdpConfig &config, std::atomic_bool &running)
        : pd_(pd), md_(md), config_(config), running_(running)
    {
    }

    bool start(uint16_t port)
    {
        port_ = port;
        serverThread_ = std::thread([this]() { this->serve(); });
        return true;
    }

    void stop()
    {
        running_.store(false);
        if (serverThread_.joinable())
        {
            serverThread_.join();
        }
    }

private:
    void serve()
    {
        int listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0)
        {
            error("Failed to create HTTP socket");
            return;
        }

        int opt = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port_);

        if (bind(listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
        {
            error("Failed to bind HTTP server socket");
            close(listenFd);
            return;
        }

        if (listen(listenFd, 5) < 0)
        {
            error("Failed to listen on HTTP server socket");
            close(listenFd);
            return;
        }

        info("HTTP server listening on port " + std::to_string(port_));

        while (running_.load())
        {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(listenFd, &readSet);
            timeval timeout{1, 0};
            const int ready = select(listenFd + 1, &readSet, nullptr, nullptr, &timeout);
            if (ready <= 0)
            {
                continue;
            }

            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            int clientFd = accept(listenFd, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
            if (clientFd < 0)
            {
                continue;
            }

            handleClient(clientFd);
            close(clientFd);
        }

        close(listenFd);
    }

    bool readRequest(int clientFd, HttpRequest &out) const
    {
        std::string buffer;
        buffer.reserve(4096);
        char temp[1024];
        ssize_t received = 0;
        std::size_t headerEnd = std::string::npos;

        while (headerEnd == std::string::npos)
        {
            received = recv(clientFd, temp, sizeof(temp), 0);
            if (received <= 0)
            {
                return false;
            }
            buffer.append(temp, static_cast<std::size_t>(received));
            headerEnd = buffer.find("\r\n\r\n");
            if (buffer.size() > 16384)
            {
                return false;
            }
        }

        const std::size_t bodyStart = headerEnd + 4;
        std::size_t contentLength = 0;
        std::istringstream headers(buffer.substr(0, headerEnd));
        std::string requestLine;
        std::getline(headers, requestLine);
        if (requestLine.back() == '\r')
        {
            requestLine.pop_back();
        }
        std::istringstream requestLineStream(requestLine);
        requestLineStream >> out.method >> out.path;

        std::string headerLine;
        while (std::getline(headers, headerLine))
        {
            if (!headerLine.empty() && headerLine.back() == '\r')
            {
                headerLine.pop_back();
            }
            const auto pos = headerLine.find(":");
            if (pos == std::string::npos)
            {
                continue;
            }
            const std::string key = headerLine.substr(0, pos);
            const std::string value = headerLine.substr(pos + 1);
            if (key == "Content-Length")
            {
                contentLength = static_cast<std::size_t>(std::stoul(value));
            }
        }

        while (buffer.size() - bodyStart < contentLength)
        {
            received = recv(clientFd, temp, sizeof(temp), 0);
            if (received <= 0)
            {
                break;
            }
            buffer.append(temp, static_cast<std::size_t>(received));
        }

        out.body = buffer.substr(bodyStart, contentLength);
        return true;
    }

    void sendResponse(int clientFd,
                      int status,
                      const std::string &statusText,
                      const std::string &body,
                      const std::string &contentType = "application/json") const
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status << " " << statusText << "\r\n";
        oss << "Content-Type: " << contentType << "\r\n";
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Connection: close\r\n\r\n";
        oss << body;
        const auto response = oss.str();
        send(clientFd, response.c_str(), response.size(), 0);
    }

    void handleClient(int clientFd)
    {
        HttpRequest req;
        if (!readRequest(clientFd, req))
        {
            return;
        }

        if (req.path == "/" && req.method == "GET")
        {
            const std::string body =
                "<html><body><h1>TRDP Simulator HTTP</h1><p>Use the JSON API under /api to list PD publish telegrams, MD "
                "templates, and mutate element values.\n";
            const std::string links =
                "<ul><li>GET /api/pd/publish</li><li>GET /api/md/templates</li><li>POST /api/pd/publish/{index}/value</li>"
                "<li>POST /api/md/templates/{name}/value</li><li>POST /api/pd/publish/{index}/lock</li>"
                "<li>POST /api/md/templates/{name}/lock</li><li>GET /api/pd/publish/{index}/payload</li>"
                "<li>GET /api/md/templates/{name}/payload</li></ul></body></html>";
            sendResponse(clientFd, 200, "OK", body + links, "text/html");
            return;
        }

        if (req.path == "/api/pd/publish" && req.method == "GET")
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            sendResponse(clientFd, 200, "OK", renderPublishJson(pd_));
            return;
        }

        if (req.path == "/api/md/templates" && req.method == "GET")
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            sendResponse(clientFd, 200, "OK", renderMdJson(md_));
            return;
        }

        const auto parts = splitPath(req.path);
        if (parts.size() >= 4 && parts[0] == "api" && parts[1] == "pd" && parts[2] == "publish")
        {
            handlePdRoute(parts, req, clientFd);
            return;
        }
        if (parts.size() >= 4 && parts[0] == "api" && parts[1] == "md" && parts[2] == "templates")
        {
            handleMdRoute(parts, req, clientFd);
            return;
        }

        sendResponse(clientFd, 404, "Not Found", "{}\n");
    }

    std::vector<std::string> splitPath(const std::string &path) const
    {
        std::vector<std::string> parts;
        std::stringstream ss(path);
        std::string item;
        while (std::getline(ss, item, '/'))
        {
            if (!item.empty())
            {
                parts.push_back(item);
            }
        }
        return parts;
    }

    void handlePdRoute(const std::vector<std::string> &parts, const HttpRequest &req, int clientFd)
    {
        const std::size_t index = static_cast<std::size_t>(std::stoul(parts[3]));
        if (parts.size() == 5 && parts[4] == "payload" && req.method == "GET")
        {
            std::vector<uint8_t> payload;
            {
                std::lock_guard<std::mutex> lock(engineMutex);
                if (!pd_.buildPublishPayload(index, payload))
                {
                    sendResponse(clientFd, 404, "Not Found", "{\"error\":\"Unknown publish index\"}\n");
                    return;
                }
            }
            sendResponse(clientFd, 200, "OK", "{\"payload\":\"" + toHex(payload) + "\"}\n");
            return;
        }

        if (req.method == "POST")
        {
            if (parts.size() == 5 && parts[4] == "clear")
            {
                std::lock_guard<std::mutex> lock(engineMutex);
                const bool ok = pd_.clearPublish(index);
                sendResponse(clientFd, ok ? 200 : 400, ok ? "OK" : "Bad Request", ok ? "{\"cleared\":true}\n" :
                                                                                    "{\"error\":\"Unable to clear publish\"}\n");
                return;
            }

            const auto element = parseJsonString(req.body, "element");
            if (!element)
            {
                sendResponse(clientFd, 400, "Bad Request", "{\"error\":\"Missing element\"}\n");
                return;
            }

            if (parts.size() == 5 && parts[4] == "value")
            {
                const auto value = parseJsonString(req.body, "value");
                if (!value)
                {
                    sendResponse(clientFd, 400, "Bad Request", "{\"error\":\"Missing value\"}\n");
                    return;
                }
                std::lock_guard<std::mutex> lock(engineMutex);
                const bool ok = pd_.setPublishValue(index, *element, *value);
                sendResponse(clientFd, ok ? 200 : 400, ok ? "OK" : "Bad Request", ok ? "{\"updated\":true}\n" :
                                                                                    "{\"error\":\"Failed to set value\"}\n");
                return;
            }

            if (parts.size() == 5 && parts[4] == "lock")
            {
                const auto locked = parseJsonBool(req.body, "locked");
                if (!locked)
                {
                    sendResponse(clientFd, 400, "Bad Request", "{\"error\":\"Missing locked flag\"}\n");
                    return;
                }
                std::lock_guard<std::mutex> lock(engineMutex);
                const bool ok = pd_.setPublishLock(index, *element, *locked);
                sendResponse(clientFd,
                             ok ? 200 : 400,
                             ok ? "OK" : "Bad Request",
                             ok ? "{\"locked\":" + std::string(*locked ? "true" : "false") + "}\n" :
                                   "{\"error\":\"Failed to update lock\"}\n");
                return;
            }
        }

        sendResponse(clientFd, 404, "Not Found", "{}\n");
    }

    void handleMdRoute(const std::vector<std::string> &parts, const HttpRequest &req, int clientFd)
    {
        const std::string name = urlDecode(parts[3]);
        if (parts.size() == 5 && parts[4] == "payload" && req.method == "GET")
        {
            std::vector<uint8_t> payload;
            {
                std::lock_guard<std::mutex> lock(engineMutex);
                const auto tpl = std::find_if(md_.templates().begin(), md_.templates().end(),
                                               [&](const auto &t) { return t.name == name; });
                if (tpl == md_.templates().end())
                {
                    sendResponse(clientFd, 404, "Not Found", "{\"error\":\"Unknown template\"}\n");
                    return;
                }
                const DatasetDef *dataset = md_.datasets().find(tpl->datasetId);
                if (dataset)
                {
                    packDatasetToPayload(*dataset, tpl->values, payload);
                }
                if (config_.tauMarshaller && config_.tauMarshaller->valid())
                {
                    std::vector<uint8_t> marshalled;
                    if (config_.tauMarshaller->marshall(tpl->comId, payload, marshalled))
                    {
                        payload = std::move(marshalled);
                    }
                }
            }
            sendResponse(clientFd, 200, "OK", "{\"payload\":\"" + toHex(payload) + "\"}\n");
            return;
        }

        if (req.method == "POST")
        {
            if (parts.size() == 5 && parts[4] == "clear")
            {
                std::lock_guard<std::mutex> lock(engineMutex);
                const bool ok = md_.clearTemplate(name);
                sendResponse(clientFd, ok ? 200 : 400, ok ? "OK" : "Bad Request", ok ? "{\"cleared\":true}\n" :
                                                                                    "{\"error\":\"Unable to clear template\"}\n");
                return;
            }

            const auto element = parseJsonString(req.body, "element");
            if (!element)
            {
                sendResponse(clientFd, 400, "Bad Request", "{\"error\":\"Missing element\"}\n");
                return;
            }

            if (parts.size() == 5 && parts[4] == "value")
            {
                const auto value = parseJsonString(req.body, "value");
                if (!value)
                {
                    sendResponse(clientFd, 400, "Bad Request", "{\"error\":\"Missing value\"}\n");
                    return;
                }
                std::lock_guard<std::mutex> lock(engineMutex);
                const bool ok = md_.setTemplateValue(name, *element, *value);
                sendResponse(clientFd, ok ? 200 : 400, ok ? "OK" : "Bad Request", ok ? "{\"updated\":true}\n" :
                                                                                    "{\"error\":\"Failed to set value\"}\n");
                return;
            }

            if (parts.size() == 5 && parts[4] == "lock")
            {
                const auto locked = parseJsonBool(req.body, "locked");
                if (!locked)
                {
                    sendResponse(clientFd, 400, "Bad Request", "{\"error\":\"Missing locked flag\"}\n");
                    return;
                }
                std::lock_guard<std::mutex> lock(engineMutex);
                const bool ok = md_.setTemplateLock(name, *element, *locked);
                sendResponse(clientFd,
                             ok ? 200 : 400,
                             ok ? "OK" : "Bad Request",
                             ok ? "{\"locked\":" + std::string(*locked ? "true" : "false") + "}\n" :
                                   "{\"error\":\"Failed to update lock\"}\n");
                return;
            }
        }

        sendResponse(clientFd, 404, "Not Found", "{}\n");
    }

    PdEngine &pd_;
    MdEngine &md_;
    TrdpConfig &config_;
    std::atomic_bool &running_;
    std::thread serverThread_;
    uint16_t port_{8080};
};

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
            std::lock_guard<std::mutex> lock(engineMutex);
            pd.listPublish(std::cout);
        }
        else if (cmd == "list-pd-sub")
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            pd.listSubscribe(std::cout);
        }
        else if (cmd == "set-pd-value")
        {
            std::size_t idx;
            std::string element, value;
            if (iss >> idx >> element >> value)
            {
                std::lock_guard<std::mutex> lock(engineMutex);
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
                std::lock_guard<std::mutex> lock(engineMutex);
                pd.clearPublish(idx);
            }
        }
        else if (cmd == "list-md")
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            md.listTemplates(std::cout);
        }
        else if (cmd == "set-md-value")
        {
            std::string name, element, value;
            if (iss >> name >> element >> value)
            {
                std::lock_guard<std::mutex> lock(engineMutex);
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
                std::lock_guard<std::mutex> lock(engineMutex);
                md.clearTemplate(name);
            }
        }
        else if (cmd == "send-md")
        {
            std::string name;
            if (iss >> name)
            {
                std::lock_guard<std::mutex> lock(engineMutex);
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

    SimpleHttpServer http(pd, md, *config, running);
    http.start(8080);

    std::thread worker([&]() { session.runLoop(running); });
    repl(pd, md);
    running.store(false);
    http.stop();
    worker.join();
    session.close();

    return 0;
}

