
#ifndef KIWMI_TEXT_BUFFER_H
#define KIWMI_TEXT_BUFFER_H
#include "pango/pango-font.h"
#include <wlr/types/wlr_scene.h>

struct text_node {
    int width;
    int max_width;
    int height;
    int baseline;
    bool pango_markup;
    float color[4];

    PangoFontDescription *font_description;

    struct wlr_scene_node *node;
};

struct text_node *text_node_create(
    struct wlr_scene_tree *parent,
    PangoFontDescription *font_description,
    const char *text,
    const float *color,
    bool pango_markup);
void text_node_set_color(struct text_node *node, const float *color);
void text_node_set_text(struct text_node *node, const char *text);
void text_node_set_max_width(struct text_node *node, int max_width);

#endif
