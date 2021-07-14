#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "SDL.h"
#include "SDL_ttf.h"

const int INITIAL_SCREEN_WIDTH = 640;
const int INITIAL_SCREEN_HEIGHT = 480;

const int POINT_SIZE = 10;

const float SLIDER_MAX = 2.00f;
const float SLIDER_MIN = 0.01f;

typedef struct {
    int x;
    int y;
} Point;

typedef enum {
    MOUSE_SELECTED_NONE,
    MOUSE_SELECTED_POINT,
    MOUSE_SELECTED_SLIDER,
    MOUSE_SELECTED_BACKGROUND,
} MouseSelectionState;

typedef struct {
    /* bezier points */
    Point points[4];
    /* sliders */
    float sliders_value[4];
    int sliders_x1;
    int sliders_x2;
    int sliders_y[4];
    /* background position */
    Point background_origin;
    /* selection state */
    MouseSelectionState selected;
    int selected_index;
    /* window info */
    int window_width;
    int window_height;
    /* helpful pointers */
    SDL_Renderer * renderer;
    TTF_Font * font;
    /* thread safety */
    SDL_mutex * mutex;
} RenderState;

/* TODO may want to change convention to returning true on error
   This would apply to RenderState_init and render
   Means we can change the return values in a consistent manner
*/
bool RenderState_init(RenderState * state, SDL_Renderer * renderer, TTF_Font * font) {
    assert(!state->mutex);

    int win_width = INITIAL_SCREEN_WIDTH;
    int win_height = INITIAL_SCREEN_HEIGHT;

    state->points[0] = (Point) { win_width / 4, win_height / 4 };
    state->points[1] = (Point) { win_width / 4, win_height * 3 / 4 };
    state->points[2] = (Point) { win_width * 3 / 4, win_height * 3 / 4 };
    state->points[3] = (Point) { win_width * 3 / 4, win_height / 4 };

    for (int i = 0; i < 4; i++) {
        state->sliders_value[i] = 1.00f;
    }

    // sliders_x1, sliders_x2, sliders_y to be assigned by render()

    state->background_origin = (Point) { 0, 0 };

    state->window_width = win_width;
    state->window_height = win_height;

    state->selected = MOUSE_SELECTED_NONE;

    state->renderer = renderer;
    state->font = font;

    state->mutex = SDL_CreateMutex();
    if (!state->mutex) {
        printf("Could not create mutex: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void RenderState_cleanup(RenderState * state) {
    SDL_DestroyMutex(state->mutex);
}

Point cubic_bezier(double t, Point w[4]) {
    double t2 = t * t;
    double t3 = t2 * t;
    double mt = 1 - t;
    double mt2 = mt * mt;
    double mt3 = mt2 * mt;

    return (Point) {
        mt3 * w[0].x + 3 * mt2 * t * w[1].x + 3 * mt * t2 * w[2].x + t3 * w[3].x,
        mt3 * w[0].y + 3 * mt2 * t * w[1].y + 3 * mt * t2 * w[2].y + t3 * w[3].y
    };
}

Point add_points(Point a, Point b) {
    return (Point) { a.x + b.x, a.y + b.y };
}

Point subtract_points(Point a, Point b) {
    return (Point) { a.x - b.x, a.y - b.y };
}

void draw_point(SDL_Renderer * renderer, Point p) {
    SDL_Rect point_rect = { p.x - POINT_SIZE / 2, p.y - POINT_SIZE / 2, POINT_SIZE, POINT_SIZE };
    SDL_RenderFillRect(renderer, &point_rect);
}

bool check_mouse_on_point(int x, int y, Point p) {
    return x < p.x + POINT_SIZE / 2 && x >= p.x - POINT_SIZE / 2 && y < p.y + POINT_SIZE / 2 && y >= p.y - POINT_SIZE / 2;
}

int slider_value_to_x(float value, int x1, int x2) {
    assert(SLIDER_MAX > SLIDER_MIN);
    assert(x2 > x1);
    return x1 + (value - SLIDER_MIN) * (x2 - x1) / (SLIDER_MAX - SLIDER_MIN);
}

float slider_x_to_value(int x, int x1, int x2) {
    assert(SLIDER_MAX > SLIDER_MIN);
    assert(x2 > x1);
    assert(x1 <= x && x <= x2);
    return SLIDER_MIN + (x - x1) * (SLIDER_MAX - SLIDER_MIN) / (x2 - x1);
}

void draw_line_between_points(SDL_Renderer * renderer, Point p1, Point p2) {
    SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
}

bool render(RenderState * state) {
    SDL_Renderer * const renderer = state->renderer;
    TTF_Font * const font = state->font;

    bool success = true;

    if (SDL_LockMutex(state->mutex)) {
        printf("Failed to lock mutex: %s\n", SDL_GetError());
        success = false;
        goto render_end;
    }

    // clear screen
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(renderer);

    // calculate actual point positions
    Point point_positions[4];
    for (int i = 0; i < 4; i++) {
        point_positions[i] = add_points(state->points[i], state->background_origin);
    }

    // draw bezier curve
    {
        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xFF, 0xFF);

        Point prev = cubic_bezier(0, point_positions);
        for (int i = 1; i <= 100; i += 1) {
            double t = (double) i / 100;

            Point next = cubic_bezier(t, point_positions);
            draw_line_between_points(renderer, prev, next);
            prev = next;
        }
    }


    // draw lines between start/end points and control points
    SDL_SetRenderDrawColor(renderer, 0x00, 0xAA, 0xAA, 0xFF);
    draw_line_between_points(renderer, point_positions[0], point_positions[1]);
    draw_line_between_points(renderer, point_positions[1], point_positions[2]);
    draw_line_between_points(renderer, point_positions[2], point_positions[3]);

    // draw start/end/control points
    SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);
    draw_point(renderer, point_positions[0]);
    draw_point(renderer, point_positions[3]);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, 0xFF);
    draw_point(renderer, point_positions[1]);
    draw_point(renderer, point_positions[2]);

    /* TODO all of these const values should probably be macro constants
       TODO that said, easier to leave it this way if screen height and width
       are ever to be dynamic
    */

    // draw container for sliders
    // TODO could probably be more consistent with use of const
    const int win_width = state->window_width;
    const int win_height = state->window_height;

    const int slider_box_outer_padding = 20;
    const int slider_box_width = win_width / 2 - slider_box_outer_padding;
    const int slider_box_height = win_height / 6;
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xAA);
    SDL_Rect box_rect = {
        win_width - slider_box_width - slider_box_outer_padding,
        win_height - slider_box_height - slider_box_outer_padding,
        slider_box_width,
        slider_box_height,
    };
    SDL_RenderFillRect(renderer, &box_rect);

    // draw slider text, lines and points
    const int slider_box_inner_padding = slider_box_height / 5;

    // these variables remain constant between iterations in the following loop
    int text_width;
    int text_height;
    int sliders_x1;
    int sliders_x2;

    // TODO could do first iteration outside loop to avoid doing the same stuff repeatedly
    for (int i = 0; i < 4; i++) {
        // calculating in full here to avoid integer rounding errors, subtracting 1 accounts for line height (1 pixel)
        int current_row_offset = (slider_box_height - slider_box_inner_padding * 2 - 1) * i / 3;
        int current_y = box_rect.y + slider_box_inner_padding + current_row_offset;

        /* slider text */

        char text_string[5];
        snprintf(text_string, 5, "%.2f", state->sliders_value[i]);

        SDL_Surface * text_surface = TTF_RenderText_Solid(font, text_string, (SDL_Color) { 0xFF, 0xFF, 0xFF });
        if (!text_surface) {
            printf("Failed to render text surface: %s\n", TTF_GetError());
            success = false;
            goto render_end;
        }

        SDL_Texture * text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
        if (!text_texture) {
            SDL_FreeSurface(text_surface);
            printf("Failed to create texture from rendered text: %s\n", SDL_GetError());
            success = false;
            goto render_end;
        }

        // check text size remains constant (assuming monospace font)
        assert(i == 0 || text_width == text_surface->w);
        assert(i == 0 || text_height == text_surface->h);

        text_width = text_surface->w;
        text_height = text_surface->h;
        SDL_FreeSurface(text_surface);

        int text_x = box_rect.x + box_rect.w - slider_box_inner_padding - text_width;
        int text_y = current_y - text_height / 2;

        SDL_Rect text_rect = { text_x, text_y, text_width, text_height };
        SDL_RenderCopyEx(renderer, text_texture, NULL, &text_rect, 0, NULL, SDL_FLIP_NONE);
        SDL_DestroyTexture(text_texture);

        /* slider lines */

        int line_width = slider_box_width - slider_box_inner_padding * 3 - text_width;
        /* TODO x1 and x2 can only vary if we change the screen to be resizable, but
           even then presumably not during calls to render(), so it would stay constant
           in this loop */
        int x1 = box_rect.x + slider_box_inner_padding;
        int x2 = x1 + line_width;
        // handle window resized too small
        if (x2 <= x1) x2 = x1 + 1;

        assert(i == 0 || sliders_x1 == x1);
        assert(i == 0 || sliders_x2 == x2);
        sliders_x1 = x1;
        sliders_x2 = x2;

        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderDrawLine(renderer, x1, current_y, x2, current_y);

        /* slider points*/

        int point_x = slider_value_to_x(state->sliders_value[i], x1, x2);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        draw_point(renderer, (Point) { point_x, current_y });

        /* update render state*/
        state->sliders_y[i] = current_y;
    }

    /* update render state*/
    state->sliders_x1 = sliders_x1;
    state->sliders_x2 = sliders_x2;

    // update screen
    SDL_RenderPresent(renderer);

    if (SDL_UnlockMutex(state->mutex)) {
        printf("Failed to unlock mutex: %s\n", SDL_GetError());
        success = false;
        goto render_end;
    }

render_end:

    return success;
}

void handle_window_event(SDL_Event e, RenderState * state) {
    if (e.type != SDL_WINDOWEVENT) return;

    switch (e.window.event) {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        {
            if (SDL_LockMutex(state->mutex)) {
                printf("Failed to lock mutex: %s\n", SDL_GetError());
                assert(0); // don't care about clean exit, just die
            }

            state->window_width = e.window.data1;
            state->window_height = e.window.data2;

            if (SDL_UnlockMutex(state->mutex)) {
                printf("Failed to unlock mutex: %s\n", SDL_GetError());
                assert(0); // don't care about clean exit, just die
            }

            render(state); // rerender
        } break;
    }
}

int handle_window_event_helper(void * state, SDL_Event * e) {
    handle_window_event(*e, (RenderState *) state);
    return 1;
}

// updates render state based on mouse events
void handle_mouse_event(SDL_Event e, RenderState * state) {
    if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION) {

        if (SDL_LockMutex(state->mutex)) {
            printf("Failed to lock mutex: %s\n", SDL_GetError());
            assert(0); // don't care about clean exit, just die
        }

        switch (e.type) {
            case SDL_MOUSEBUTTONDOWN:
            {
                int mouse_x = e.button.x;
                int mouse_y = e.button.y;

                /* TODO could instead handle overlapping points by finding closest
                point to cursor and applying check_mouse_on_point to just that */

                /* Bezier points */
                for (int i = 0; i < 4; i++) {
                    Point point_position = add_points(state->points[i], state->background_origin);
                    if (check_mouse_on_point(mouse_x, mouse_y, point_position)) {
                        state->selected = MOUSE_SELECTED_POINT;
                        state->selected_index = i;
                        break;
                    }
                }

                if (state->selected != MOUSE_SELECTED_NONE) break;

                /* slider points */
                int x1 = state->sliders_x1;
                int x2 = state->sliders_x2;

                for (int i = 0; i < 4; i++) {
                    int slider_x = slider_value_to_x(state->sliders_value[i], x1, x2);
                    int slider_y = state->sliders_y[i];
                    if (check_mouse_on_point(mouse_x, mouse_y, (Point) { slider_x, slider_y })) {
                        state->selected = MOUSE_SELECTED_SLIDER;
                        state->selected_index = i;
                        break;
                    }
                }

                if (state->selected != MOUSE_SELECTED_NONE) break;

                // TODO could make the slider container opaque
                state->selected = MOUSE_SELECTED_BACKGROUND;

            } break;

            case SDL_MOUSEBUTTONUP:
            {
                state->selected = MOUSE_SELECTED_NONE;
            } break;

            case SDL_MOUSEMOTION:
            {
                int mouse_x = e.motion.x;
                int mouse_y = e.motion.y;

                switch (state->selected) {
                    case MOUSE_SELECTED_POINT:
                    {
                        Point world_position = (Point) { mouse_x, mouse_y };
                        state->points[state->selected_index] = subtract_points(world_position, state->background_origin);
                    } break;

                    case MOUSE_SELECTED_SLIDER:
                    {

                        /* TODO how are we going to get x1 and x2 from our render
                           function here? Calculate what we need external to both
                           functions? Pass through render state? */
                        int x1 = state->sliders_x1;
                        int x2 = state->sliders_x2;

                        int slider_x = mouse_x;
                        if (slider_x < x1) slider_x = x1;
                        if (slider_x > x2) slider_x = x2;

                        state->sliders_value[state->selected_index] = slider_x_to_value(slider_x, x1, x2); // TODO

                    } break;

                    case MOUSE_SELECTED_BACKGROUND:
                    {
                        Point offset = { e.motion.xrel, e.motion.yrel };
                        state->background_origin = add_points(state->background_origin, offset);
                    } break;

                    default:
                        assert(state->selected == MOUSE_SELECTED_NONE); // only other option
                }
            } break;
        }

        if (SDL_UnlockMutex(state->mutex)) {
            printf("Failed to unlock mutex: %s\n", SDL_GetError());
            assert(0); // don't care about clean exit, just die
        }
    }
}

// TODO use snake case instead of camel case for functions everywhere?
// TODO also, tabs to spaces please
int main(int argc, char * argv[]) {
    SDL_Window * window = NULL;
    SDL_Renderer * renderer = NULL;
    TTF_Font * font = NULL;

    // initialise SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialise: %s\n", SDL_GetError());
        goto main_cleanup;
    }

    // set texture filtering to linear
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
        printf("Warning: Linear texture filtering not enabled");
    }

    // create window
    window = SDL_CreateWindow(
        "Beziers",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        INITIAL_SCREEN_WIDTH,
        INITIAL_SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        printf("Window could not be created: %s\n", SDL_GetError());
        goto main_cleanup;
    }

    // create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer could not be created: %s\n", SDL_GetError());
        goto main_cleanup;
    }

    if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND)) {
        printf("Warning: Renderer blend mode could not be set properly (SDL error: %s)\n", SDL_GetError());
    }

    if (TTF_Init()) {
        printf("SDL_ttf could not initialize: %s\n", TTF_GetError());
        goto main_cleanup;
    }

    const char font_path[] = "fonts/m5x7.ttf";
    font = TTF_OpenFont(font_path, 16); // ptsize = 16, 32, 48, etc.
    if (!font) {
        printf("Could not open font at path %s: %s\n", font_path, TTF_GetError());
        goto main_cleanup;
    }

    {
        bool quit = false;
        SDL_Event e;

        // initialise render state
        RenderState state = {0};
        if (!RenderState_init(&state, renderer, font)) {
            printf("Failed to intialise render state\n");
            goto main_render_cleanup;
        }

        // this method is necessary for getting resize events during resizing,
        // rather than just at the very end
        SDL_AddEventWatch(handle_window_event_helper, &state);

        while (!quit) {
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) quit = true;
                handle_mouse_event(e, &state);
            }

            if (!render(&state)) {
                printf("Failed to render frame\n");
                goto main_render_cleanup;
            }
        }

main_render_cleanup:
        RenderState_cleanup(&state);
    }


main_cleanup:
    // TODO destroy font texture
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}
