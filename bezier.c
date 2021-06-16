#include "SDL.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

const int POINT_SIZE = 10;

typedef struct {
	int x;
	int y;
} Point;

typedef struct {
	Point points[4];
	bool selected;
	int selectedIndex;
} PointState;


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
	SDL_Rect pointRect = { p.x - POINT_SIZE / 2, p.y - POINT_SIZE / 2, POINT_SIZE, POINT_SIZE };
	SDL_RenderFillRect(renderer, &pointRect);
}

bool checkMouseOnPoint(int x, int y, Point p) {
	return x < p.x + POINT_SIZE / 2 && x >= p.x - POINT_SIZE / 2 && y < p.y + POINT_SIZE / 2 && y >= p.y - POINT_SIZE / 2;
}

void handleMouseUpdates(PointState * state, SDL_Event e) {
	if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION) {
		int x, y;
		SDL_GetMouseState(&x, &y);

		switch (e.type) {
			case SDL_MOUSEBUTTONDOWN:
			{
				// TODO this might cause issues with overlapping points?
				// TODO could instead find closest point and see if it overlaps
				for (int i = 0; i < 4; i++) {
					if (checkMouseOnPoint(x, y, state->points[i])) {
						state->selected = true;
						state->selectedIndex = i;
						break;
					}
				}
			} break;

			case SDL_MOUSEBUTTONUP:
			{
				state->selected = false;
			} break;

			case SDL_MOUSEMOTION:
			{
				if (state->selected) {
					state->points[state->selectedIndex] = (Point) { x, y };
				}
			} break;
		}
	}
}

void drawLineBetweenPoints(SDL_Renderer * renderer, Point p1, Point p2) {
	SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
}

void render(PointState * state, SDL_Renderer * renderer) {
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

	// update screen
	SDL_RenderPresent(renderer);
}

int main(int argc, char * argv[]) {
	SDL_Window * window = NULL;
	SDL_Renderer * renderer = NULL;

	// initialise SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("SDL could not initialise: %s\n", SDL_GetError());
		goto cleanup;
	}

	// TODO understand better
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

	{
		bool quit = false;
		SDL_Event e;

		// initialise state
		PointState state = {0};
		state.points[0] = (Point) { SCREEN_WIDTH / 4, SCREEN_HEIGHT / 4 };
		state.points[1] = (Point) { SCREEN_WIDTH / 4, SCREEN_HEIGHT * 3 / 4 };
		state.points[2] = (Point) { SCREEN_WIDTH * 3 / 4, SCREEN_HEIGHT * 3 / 4 };
		state.points[3] = (Point) { SCREEN_WIDTH * 3 / 4, SCREEN_HEIGHT / 4 };

		while (!quit) {
			while (SDL_PollEvent(&e)) {
				if (e.type == SDL_QUIT) quit = true;
			}

			handleMouseUpdates(&state, e);
			render(&state, renderer);
		}
	}


cleanup:
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}