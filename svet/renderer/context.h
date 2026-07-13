#pragma once

struct SDL_Window;

namespace svet::renderer {

using LContext = struct LContextT *;

struct RendererSpecification {
  bool allowValidation;
};
LContext init(SDL_Window *window, const RendererSpecification &spec);
void shutdown(LContext context);

} // namespace svet::renderer
