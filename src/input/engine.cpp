#include "engine.h"

#include <SDL3/SDL.h>

#include <imgui/backends/imgui_impl_sdl3.h>

#include "../states.h"

#include <iostream>

namespace Input {

Engine::Engine(const std::unordered_map<uint32_t, EventType> &corresps)
    : m_registeredKeys(corresps.size())
    , m_keyToEvent(corresps)
    , m_eventToEntry({{EventType::Up, &State::up},
                      {EventType::Down, &State::down},
                      {EventType::Left, &State::left},
                      {EventType::Right, &State::right},
                      {EventType::Jump, &State::jump},
                      {EventType::Attack, &State::attack}})
{
    for (const auto &pair : corresps) {
        m_registeredKeys.insert(pair.first);
    }
}

void Engine::run(std::atomic<uint64_t> &commands)
{
    SDL_Event e{};

    while (!(commands & CommandStates::Stop)) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            switch (e.type) {
            case SDL_EVENT_KEY_DOWN: {
                std::cout << e.key.key << std::endl;
                m_state.*(m_eventToEntry[m_keyToEvent[e.key.key]]) = {true, e.key.repeat};
                break;
            }
            case SDL_EVENT_KEY_UP: {
                m_state.*(m_eventToEntry[m_keyToEvent[e.key.key]]) = {false, e.key.repeat};
                break;
            }
            case SDL_EVENT_QUIT: {
                // close the window when user alt-f4s or clicks the X button
                commands |= CommandStates::Stop;
                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED: {
                commands |= CommandStates::PauseRendering;
                break;
            }
            case SDL_EVENT_WINDOW_RESTORED: {
                commands &= ~CommandStates::PauseRendering;
                break;
            }
            }

            ImGui_ImplSDL3_ProcessEvent(&e);
        }
    }
}

} // namespace Input
