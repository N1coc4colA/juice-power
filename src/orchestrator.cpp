#include "src/orchestrator.h"

#include <SDL3/SDL.h>

#include <thread>

#include "src/graphics/engine.h"
#include "src/input/engine.h"
#include "src/loaders/map.h"
#include "src/physics/engine.h"

/// @brief Singleton storage for the process-wide orchestrator instance.
Orchestrator *Orchestrator::m_instance = nullptr;

/// @brief Short alias for input event type values used in key map setup.
using ET = Input::EventType;

Orchestrator::Orchestrator()
    : m_graphicsEngine(std::make_shared<Graphics::Engine>())
    , m_physicsEngine(std::make_shared<Physics::Engine>())
    , m_inputEngine(std::make_shared<Input::Engine>(std::unordered_map<uint32_t, Input::EventType>{
          {SDLK_Z, ET::Up}, {SDLK_S, ET::Down}, {SDLK_Q, ET::Left}, {SDLK_D, ET::Right}, {SDLK_SPACE, ET::Jump}, {SDLK_E, ET::Attack}}))
{
    assert(m_instance == nullptr);

    m_instance = this;

    init();
}

Orchestrator::~Orchestrator() = default;

auto Orchestrator::get() -> Orchestrator &
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

    std::jthread m_physicsThread([this]() -> void { m_physicsEngine->run(m_commands); });
    std::jthread m_inputThread([this]() -> void { m_inputEngine->run(m_commands); });
    m_graphicsEngine->run([this]() -> void { m_physicsEngine->prepare(); }, m_commands);
}

void Orchestrator::cleanup()
{
    m_graphicsEngine->cleanup();
}

void Orchestrator::setScene(const std::shared_ptr<World::Scene> &scene)
{
    m_graphicsEngine->setScene(scene);
    m_physicsEngine->setScene(scene);
}

auto Orchestrator::loadMap(const std::shared_ptr<World::Scene> &scene, const std::string &path) const -> std::tuple<Loaders::Status, std::string>
{
    Loaders::Map mapLoader(path);
    return mapLoader.load2(m_graphicsEngine, scene);
}
