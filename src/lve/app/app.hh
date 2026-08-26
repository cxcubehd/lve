#include "lve/render/render.hh"

class App
{
  protected:
  Renderer* renderer_;

  public:
  auto renderer() -> Renderer*;
};
