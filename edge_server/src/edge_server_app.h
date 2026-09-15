#pragma once

#include <memory>

namespace edge_server
{

class EdgeServerContext;

class EdgeServerApp
{
public:
    EdgeServerApp();

    bool init(const std::string& cfgPath);
private:
    void regiestHandler();
    void setSubscribe();
private:
    std::shared_ptr<EdgeServerContext> m_edgeServerContext;
};

}