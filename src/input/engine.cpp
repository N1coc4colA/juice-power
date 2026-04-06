#include "src/input/engine.h"

#include <SDL3/SDL.h>

#include <iostream>
#include <ranges>

#include <imgui/backends/imgui_impl_sdl3.h>

#include "src/states.h"

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
    for (const auto &key : corresps | std::views::keys) {
        m_registeredKeys.insert(key);
    }
}

void Engine::run(std::atomic<uint64_t> &commands)
{
    SDL_Event event{};

    while (!(commands & Stop)) {
        // Handle events on queue
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
            case SDL_EVENT_KEY_DOWN: {
                std::cout << event.key.key << '\n';
                m_state.*m_eventToEntry[m_keyToEvent[event.key.key]] = {.state = true, .hold = event.key.repeat};
                break;
            }
            case SDL_EVENT_KEY_UP: {
                m_state.*m_eventToEntry[m_keyToEvent[event.key.key]] = {.state = false, .hold = event.key.repeat};
                break;
            }
            case SDL_EVENT_QUIT: {
                // close the window when user alt-f4s or clicks the X button
                commands |= Stop;
                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED: {
                commands |= PauseRendering;
                break;
            }
            case SDL_EVENT_WINDOW_RESTORED: {
                commands &= ~PauseRendering;
                break;
            }
            default: break;
            }

            ImGui_ImplSDL3_ProcessEvent(&event);
        }
    }
}

} // namespace Input
