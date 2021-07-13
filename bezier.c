#include "SDL.h"
#include "SDL_ttf.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

const int POINT_SIZE = 10;

typedef struct {
	int x;
	int y;
} Point;

typedef enum {
	MOUSE_SELECTED_NONE,
	MOUSE_SELECTED_POINT,
	MOUSE_SELECTED_SLIDER,
} MouseSelectionState;

typedef struct {
	Point points[4];
	float sliders[4];
	MouseSelectionState selected;
	int selected_index;
} RenderState;


Point cubicBezier(double t, Point w[4]) {
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

void drawPoint(Point p, SDL_Renderer * renderer) {
	SDL_Rect point_rect = { p.x - POINT_SIZE / 2, p.y - POINT_SIZE / 2, POINT_SIZE, POINT_SIZE };
	SDL_RenderFillRect(renderer, &point_rect);
}

bool checkMouseOnPoint(int x, int y, Point p) {
	return x < p.x + POINT_SIZE / 2 && x >= p.x - POINT_SIZE / 2 && y < p.y + POINT_SIZE / 2 && y >= p.y - POINT_SIZE / 2;
}

// updates render state based on mouse events
void handleMouseUpdates(RenderState * state, SDL_Event e) {
	if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION) {
		int mouse_x, mouse_y;
		SDL_GetMouseState(&mouse_x, &mouse_y);

		switch (e.type) {
			case SDL_MOUSEBUTTONDOWN:
			{
				// TODO this might cause issues with overlapping points?
				// TODO could instead find closest point and see if it overlaps
				for (int i = 0; i < 4; i++) {
					if (checkMouseOnPoint(mouse_x, mouse_y, state->points[i])) {
						state->selected = MOUSE_SELECTED_POINT;
						state->selected_index = i;
						break;
					}
				}

				// TODO handle click on sliders, but avoid handling event twice
				for (int i = 0; i < 4; i++) {
					// TODO
				}
			} break;

			case SDL_MOUSEBUTTONUP:
			{
				state->selected = false;
			} break;

			case SDL_MOUSEMOTION:
			{
				switch (state->selected) {
					case MOUSE_SELECTED_POINT:
						state->points[state->selected_index] = (Point) { mouse_x, mouse_y };
						break;
					case MOUSE_SELECTED_SLIDER:
						state->sliders[state->selected_index] = 0; // TODO
						break;
					default:
						assert(state->selected == MOUSE_SELECTED_NONE);
				}
			} break;
		}
	}
}

void drawLineBetweenPoints(SDL_Renderer * renderer, Point p1, Point p2) {
	SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
}

bool render(RenderState * state, SDL_Renderer * renderer, TTF_Font * font) {
	// clear screen
	SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
	SDL_RenderClear(renderer);

	// draw bezier curve
	{
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xFF, 0xFF);

		Point prev = cubicBezier(0, state->points);
		for (int i = 1; i <= 100; i += 1) {
			double t = (double) i / 100;

			Point next = cubicBezier(t, state->points);
			drawLineBetweenPoints(renderer, prev, next);
			prev = next;
		}
	}

	// draw lines between start/end points and control points
	SDL_SetRenderDrawColor(renderer, 0x00, 0xAA, 0xAA, 0xFF);
	drawLineBetweenPoints(renderer, state->points[0], state->points[1]);
	drawLineBetweenPoints(renderer, state->points[1], state->points[2]);
	drawLineBetweenPoints(renderer, state->points[2], state->points[3]);

	// draw start/end/control points
	SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);
	drawPoint(state->points[0], renderer);
	drawPoint(state->points[3], renderer);
	SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, 0xFF);
	drawPoint(state->points[1], renderer);
	drawPoint(state->points[2], renderer);

	/* TODO all of these const values should probably be macro constants
	   TODO that said, easier to leave it this way if screen height and width
	   are ever to be dynamic
	*/

	// draw container for sliders
	const int slider_box_outer_padding = 20;
	const int slider_box_width = SCREEN_WIDTH / 2 - slider_box_outer_padding;
	const int slider_box_height = SCREEN_HEIGHT / 6;
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xAA);
	SDL_Rect box_rect = {
		SCREEN_WIDTH - slider_box_width - slider_box_outer_padding,
		SCREEN_HEIGHT - slider_box_height - slider_box_outer_padding,
		slider_box_width,
		slider_box_height,
	};
	SDL_RenderFillRect(renderer, &box_rect);

	// draw slider lines, text, and points
	const int slider_box_inner_padding = slider_box_height / 5;
	SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
	for (int i = 0; i < 4; i++) {
		// calculating in full here to avoid integer rounding errors, subtracting 1 accounts for line height (1 pixel)
		int current_row_offset = (slider_box_height - slider_box_inner_padding * 2 - 1) * i / 3;
		int current_y = box_rect.y + slider_box_inner_padding + current_row_offset;

		/* slider lines */
		int line_width = (slider_box_width - slider_box_inner_padding * 2) * 3/4;
		int x1 = box_rect.x + slider_box_inner_padding;
		int x2 = x1 + line_width;

		SDL_RenderDrawLine(renderer, x1, current_y, x2, current_y);

		/* slider text */
		// TODO set text
		SDL_Surface * text_surface = TTF_RenderText_Solid(font, "1.00", (SDL_Color) { 0xFF, 0xFF, 0xFF });
		if (!text_surface) {
			printf("Failed to render text surface: %s\n", TTF_GetError());
			return false;
		}

		SDL_Texture * text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
		if (!text_texture) {
			printf("Failed to create texture from rendered text: %s\n", SDL_GetError());
			return false;
		}

		int text_width = text_surface->w;
		int text_height = text_surface->h;
		SDL_FreeSurface(text_surface);

		int text_x = box_rect.x + box_rect.w - slider_box_inner_padding - text_width;
		int text_y = current_y - text_height / 2;

		SDL_Rect text_rect = { text_x, text_y, text_width, text_height };
		SDL_RenderCopyEx(renderer, text_texture, NULL, &text_rect, 0, NULL, SDL_FLIP_NONE);
		SDL_DestroyTexture(text_texture);

		/* slider points*/
		// TODO
	}

	// update screen
	SDL_RenderPresent(renderer);

	return true;
}

// TODO use snake case instead of camel case for local variables everywhere?
int main(int argc, char * argv[]) {
	SDL_Window * window = NULL;
	SDL_Renderer * renderer = NULL;
	TTF_Font * font = NULL;

	// initialise SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("SDL could not initialise: %s\n", SDL_GetError());
		goto cleanup;
	}

	// set texture filtering to linear
	if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
		printf("Warning: Linear texture filtering not enabled");
	}

	// create window
	window = SDL_CreateWindow("Beziers", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	if (!window) {
		printf("Window could not be created: %s\n", SDL_GetError());
		goto cleanup;
	}

	// create renderer
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		printf("Renderer could not be created: %s\n", SDL_GetError());
		goto cleanup;
	}

	if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND)) {
		printf("Warning: Renderer blend mode could not be set properly (SDL error: %s)\n", SDL_GetError());
	}

	if (TTF_Init()) {
		printf("SDL_ttf could not initialize: %s\n", TTF_GetError());
		goto cleanup;
	}

	const char font_path[] = "fonts/m5x7.ttf";
	font = TTF_OpenFont(font_path, 16); // ptsize = 16, 32, 48, etc.
	if (!font) {
		printf("Could not open font at path %s: %s\n", font_path, TTF_GetError());
		goto cleanup;
	}

	{
		bool quit = false;
		SDL_Event e;

		// initialise state
		RenderState state = {0};
		state.points[0] = (Point) { SCREEN_WIDTH / 4, SCREEN_HEIGHT / 4 };
		state.points[1] = (Point) { SCREEN_WIDTH / 4, SCREEN_HEIGHT * 3 / 4 };
		state.points[2] = (Point) { SCREEN_WIDTH * 3 / 4, SCREEN_HEIGHT * 3 / 4 };
		state.points[3] = (Point) { SCREEN_WIDTH * 3 / 4, SCREEN_HEIGHT / 4 };

		while (!quit) {
			while (SDL_PollEvent(&e)) {
				if (e.type == SDL_QUIT) quit = true;
			}

			handleMouseUpdates(&state, e);
			render(&state, renderer, font); // TODO check for error
		}
	}


cleanup:
	// TODO destroy font texture
	TTF_CloseFont(font);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	TTF_Quit();
	SDL_Quit();

	return 0;
}
