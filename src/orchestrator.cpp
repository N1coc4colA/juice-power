#include "orchestrator.h"

#include <SDL3/SDL.h>

#include <thread>

#include "graphics/engine.h"
#include "input/engine.h"
#include "loaders/map.h"
#include "physics/engine.h"

Orchestrator *Orchestrator::m_instance = nullptr;

using ET = Input::EventType;

Orchestrator::Orchestrator()
    : m_graphicsEngine(new Graphics::Engine())
    , m_physicsEngine(new Physics::Engine())
    , m_inputEngine(new Input::Engine({
          {SDLK_Z, ET::Up},
          {SDLK_S, ET::Down},
          {SDLK_Q, ET::Left},
          {SDLK_D, ET::Right},
          {SDLK_SPACE, ET::Jump},
          {SDLK_E, ET::Attack},
      }))
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

    m_physicsEngine->setInputState(m_inputEngine->state());

    std::jthread m_physicsThread([this]() { m_physicsEngine->run(m_commands); });
    std::jthread m_inputThread([this]() { m_inputEngine->run(m_commands); });
    m_graphicsEngine->run([this]() { m_physicsEngine->prepare(); }, m_commands);
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
