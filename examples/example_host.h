#ifndef EXAMPLE_HOST_H
#define EXAMPLE_HOST_H
#include "markup/mu_soft.h"
#include "markup/mu_widgets_basic.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_input.h"
typedef struct Example { MuContext ctx; MuRenderContext render; MuNode *root; } Example;
bool example_init(Example *example);
/* --snapshot path.ppm writes one offscreen frame. --window uses SDL when enabled. */
int example_run(Example *example, const char *title, int argc, char **argv);
void example_destroy(Example *example);
#endif
