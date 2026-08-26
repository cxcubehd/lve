#include "app.hh"

#include <assert.h>
#include <ostream>

auto App::renderer() -> Renderer*
{
  assert(renderer_ != nullptr);
  return renderer_;
}
