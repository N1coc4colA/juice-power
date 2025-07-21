#include "orchestrator.h"

#include "graphics/engine.h"
#include "loaders/map.h"
#include "physics/engine.h"

Orchestrator *Orchestrator::m_instance = nullptr;

Orchestrator::Orchestrator()
    : m_graphicsEngine(new Graphics::Engine())
    , m_physicsEngine(new Physics::Engine())
{
    assert(m_instance == nullptr);

    m_instance = this;

    init();
}

Orchestrator &Orchestrator::get()
{
    return *m_instance;
}

void Orchestrator::init()
{
    m_graphicsEngine->init();
}

void Orchestrator::run()
{
    m_commands = 0;

    std::jthread m_physicsThread([this]() { m_physicsEngine->run(&m_commands); });
    m_graphicsEngine->run([this]() { m_physicsEngine->prepare(); }, &m_commands);
}

void Orchestrator::cleanup()
{
    m_graphicsEngine->cleanup();
}

void Orchestrator::setScene(World::Scene &scene)
{
    m_graphicsEngine->setScene(scene);
    m_physicsEngine->setScene(scene);
}

Loaders::Status Orchestrator::loadMap(World::Scene &scene, const std::string &path)
{
    Loaders::Map mapLoader(path);
    return mapLoader.load(*m_graphicsEngine, scene);
}
