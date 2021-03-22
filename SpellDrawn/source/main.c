// Simple citro2d sprite drawing example
// Images borrowed from:
//   https://kenney.nl/assets/space-shooter-redux
#include <citro2d.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUMTHREADS 1
#define STACKSIZE (4 * 1024)

#define MAX_SPRITES   768
#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240

// Simple sprite struct
typedef struct
{
	C2D_Sprite spr;
	float dx, dy; // velocity
	int initFrame;
	float px, py; //position;


} Sprite;

static C2D_SpriteSheet spriteSheet;
static Sprite sprites[MAX_SPRITES];
static size_t numSprites = MAX_SPRITES/2;
static int frame = 0;

volatile bool runThreads = true;


//---------------------------------------------------------------------------------
static void initSprites() {
//---------------------------------------------------------------------------------
	size_t numImages = C2D_SpriteSheetCount(spriteSheet);
	srand(time(NULL));

	Sprite* sprite = &sprites[0];
	C2D_SpriteFromSheet(&sprite->spr, spriteSheet, 0);
	C2D_SpriteSetCenter(&sprite->spr, 0.5f, 0.5f);
	C2D_SpriteSetPos(&sprite->spr, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
	C2D_SpriteSetRotation(&sprite->spr, C3D_Angle(0));
	sprite->dx = 0;
	sprite->dy = 0;
	sprite->initFrame = 1;

	for (size_t i = 1; i < MAX_SPRITES; i++)
	{
		sprite = &sprites[i];

		sprite->initFrame = (8 + (rand() % 17) * 4);

		sprite->px = rand() % SCREEN_WIDTH;
		sprite->py = rand() % SCREEN_HEIGHT;
		// Random image, position, rotation and speed
		C2D_SpriteFromSheet(&sprite->spr, spriteSheet, sprite->initFrame);
		C2D_SpriteSetCenter(&sprite->spr, 0.5f, 0.5f);
		C2D_SpriteSetPos(&sprite->spr, sprite->px, sprite->py);
		C2D_SpriteSetRotation(&sprite->spr, C3D_Angle(0));
		sprite->dx = rand()*4.0f/RAND_MAX - 2.0f;
		sprite->dy = rand()*4.0f/RAND_MAX - 2.0f;
		
	}

}

//---------------------------------------------------------------------------------
static void moveSprites() {
	//---------------------------------------------------------------------------------

	Sprite* sprite = &sprites[0];

		for (size_t i = 1; i < numSprites; i++)
		{
			sprite = &sprites[i];

			

			C2D_SpriteMove(&sprite->spr, sprite->dx, sprite->dy);
			sprite->py = sprite->spr.params.pos.y;
			sprite->px = sprite->spr.params.pos.x;

			//C2D_SpriteRotateDegrees(&sprite->spr, 0);
			if (sprite->dx > 0) { C2D_SpriteSetScale(&sprite->spr, 1, 1); }
			if (sprite->dx < 0) { C2D_SpriteSetScale(&sprite->spr, -1, 1); }


			// Check for collision with the screen boundaries
			if ((sprite->spr.params.pos.x < sprite->spr.params.pos.w / 2.0f && sprite->dx < 0.0f) ||
				(sprite->spr.params.pos.x > (SCREEN_WIDTH - (sprite->spr.params.pos.w / 2.0f)) && sprite->dx > 0.0f))
				sprite->dx = -sprite->dx;

			if ((sprite->spr.params.pos.y < sprite->spr.params.pos.h / 2.0f && sprite->dy < 0.0f) ||
				(sprite->spr.params.pos.y > (SCREEN_HEIGHT - (sprite->spr.params.pos.h / 2.0f)) && sprite->dy > 0.0f))
				sprite->dy = -sprite->dy;

		}

}

void threadMain(void* arg)
{
	u64 sleepDuration = 1000000ULL * (u32)arg;
	while (runThreads)
	{
		Sprite* sprite = &sprites[0];
		C2D_SpriteFromSheet(&sprite->spr, spriteSheet, sprite->initFrame + (frame % 4));
		C2D_SpriteSetPos(&sprite->spr, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
		C2D_SpriteSetCenter(&sprite->spr, 0.5f, 0.5f);

		for (size_t i = 1; i < numSprites; i++)
		{
			sprite = &sprites[i];

			

			C2D_SpriteFromSheet(&sprite->spr, spriteSheet, sprite->initFrame + (frame % 4));
			C2D_SpriteSetPos(&sprite->spr, sprite->px, sprite->py);
			C2D_SpriteSetCenter(&sprite->spr, 0.5f, 0.5f);
		}

		frame++;
		svcSleepThread(sleepDuration);
	}
}

//---------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
//---------------------------------------------------------------------------------
	// Init libs
	romfsInit();
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	consoleInit(GFX_BOTTOM, NULL);



	// Create screens
	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

	// Load graphics
	spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
	if (!spriteSheet) svcBreak(USERBREAK_PANIC);

	// Initialize sprites
	initSprites();

	printf("\x1b[8;1HPress Up to increment sprites");
	printf("\x1b[9;1HPress Down to decrement sprites");

	Thread threads[NUMTHREADS];
	int i;
	s32 prio = 0;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	printf("Main thread prio: 0x%lx\n", prio);

	for (i = 0; i < NUMTHREADS; i++)
	{
		// The priority of these child threads must be higher (aka the value is lower) than that
		// of the main thread, otherwise there is thread starvation due to stdio being locked.
		threads[i] = threadCreate(threadMain, (void*)((i + 1) * 250), STACKSIZE, prio - 1, -2, false);
		printf("created thread %d: %p\n", i, threads[i]);
	}
	

	// Main loop
	while (aptMainLoop())
	{
		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		u32 kHeld = hidKeysHeld();
		if ((kHeld & KEY_UP) && numSprites < MAX_SPRITES)
			numSprites++;
		if ((kHeld & KEY_DOWN) && numSprites > 1)
			numSprites--;

		moveSprites();

		printf("\x1b[1;1HSprites: %zu/%u\x1b[K", numSprites, MAX_SPRITES);
		printf("\x1b[2;1HCPU:     %6.2f%%\x1b[K", C3D_GetProcessingTime() * 6.0f);
		printf("\x1b[3;1HGPU:     %6.2f%%\x1b[K", C3D_GetDrawingTime() * 6.0f);
		printf("\x1b[4;1HCmdBuf:  %6.2f%%\x1b[K", C3D_GetCmdBufUsage() * 100.0f);

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(top, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
		C2D_SceneBegin(top);
		for (size_t i = 0; i < numSprites; i++) {
			C2D_DrawSprite(&sprites[i].spr);
		}
		C3D_FrameEnd(0);
	}

	// Delete graphics
	C2D_SpriteSheetFree(spriteSheet);

	// Deinit libs
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	romfsExit();
	return 0;
}
