/* Main executable and game loop. */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <math.h>
#include <string>

#define COLOR_WHITE 0xFF, 0xFF, 0xFF, 0xFF
#define COLOR_WALL_1 0x00, 0x22, 0x20, 0xFF
#define COLOR_WALL_2 0x55, 0x00, 0x00, 0xFF
#define COLOR_FLOOR 0x55, 0x55, 0x55, 0x55
#define COLOR_CEILING 0x33, 0x33, 0x33, 0xFF

#define sgn(a) (a < 0 ? -1 : a > 0 ? +1 : 0)

bool isColliding (double x, double y);

SDL_Texture* loadTexture (const char * file);
SDL_Surface* loadSurface (const char * file);
SDL_Texture* cropTexture (SDL_Texture* src, int x, int y);

struct Texture {
    SDL_Texture* texture;
    int width;
    int height;
};


// resolution (and in windowed mode, the screen size)
const int SCREEN_WIDTH = 800;		// horizontal resolution
const int SCREEN_HEIGHT = 600;		// vertical resolution

// the field of view
const double FOV = 1.30899694; 		// rad (75 deg)
const double MIN_DIST = 0.3;		// square sides / s
const double DARKEST_DIST = 6.0;	// any greater distance will not be darker
const int DARKNESS_ALPHA = 40;		// minimal lighting

// world size
const int GRID_WIDTH = 18;
const int GRID_HEIGHT = 10;

const double DEFAULT_SPEED = 3; 	// sqsides/s
const double TURN_SPEED = M_PI; 	// rad/s


void * mPixels;


// the fixed map
const int world[GRID_HEIGHT][GRID_WIDTH] =
{
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1 },
{ 1, 0, 2, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1 },
{ 1, 0, 2, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1 },
{ 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
{ 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1 },
{ 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1 },
{ 1, 0, 1, 1, 0, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
{ 1, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

//The window we'll be rendering to
SDL_Window* window = NULL;

//The surface contained by the window
SDL_Surface* gSurface = NULL;

SDL_Renderer* gRenderer = NULL;

SDL_Texture* gTexture = NULL;

SDL_Texture* gFloorTexture = NULL;
SDL_Texture* gCeilgTexture = NULL;

TTF_Font* gFont = NULL;

// a GPU accelerated wall texture
SDL_Texture* txt_walls [3];

SDL_Surface* srf_floor;
SDL_Surface* srf_ceilg;

Mix_Chunk * snd_meinleben = NULL;

// the initial player position
double px = 1.5, py = 1.5;

// the intial central ray direction
double teta = 0.0;

// the tangent y-coordinate for teta=0, one for every horizontal pixel
double scr_pts [SCREEN_WIDTH];

// correction values for distortion
double distortion [SCREEN_WIDTH];

int ticks;			// ms 
int diffTicks;		// ms

double speed = 0.0; // sqsides/s
double turn = 0.0;  // rad/s


int init ()
{
    double tan_FOV = tan (FOV / 2);
    
    // a ray will be casted for every horizontal pixel
    for (int i = 0; i < SCREEN_WIDTH; i++){
        scr_pts[i] = tan_FOV - (2 * tan_FOV * (i + 1)) / SCREEN_WIDTH;
        distortion[i] = 1.0 / sqrt(1 + scr_pts[i] * scr_pts[i]);
    }
    
    if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        return -1;
    }
    window = SDL_CreateWindow( 
            "SDL Tutorial", 
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED, 
            SCREEN_WIDTH, 
            SCREEN_HEIGHT, 
            0);
    if ( window == NULL )
    {
        printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
        return -1;
    }
    
    gSurface = SDL_GetWindowSurface( window );
    if ( gSurface == NULL )
    {
        printf( "Surface could not be created! SDL_Error: %s\n", SDL_GetError() );
        return -1;
    }

    gRenderer = SDL_GetRenderer (window);
    if ( gRenderer == NULL )
    {
        printf( "Renderer could not be created! SDL_Error: %s\n", SDL_GetError() );
        return -1;
    }
    SDL_SetRenderDrawBlendMode (gRenderer, SDL_BLENDMODE_NONE);
    
    gTexture = SDL_CreateTexture(gRenderer, gSurface->format->format,
        SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH, SCREEN_HEIGHT); 
    if ( gTexture == NULL )
    {
        printf( "Texture could not be created! SDL_Error: %s\n", SDL_GetError() );
        return -1;
    }
	
	gFloorTexture = SDL_CreateTexture (gRenderer, gSurface->format->format,
		SDL_TEXTUREACCESS_STREAMING, 1, SCREEN_HEIGHT);
	if ( gFloorTexture == NULL )
    {
        printf( "Texture could not be created! SDL_Error: %s\n", SDL_GetError() );
        return -1;
    }
	
	gCeilgTexture = SDL_CreateTexture (gRenderer, gSurface->format->format,
		SDL_TEXTUREACCESS_STREAMING, 1, SCREEN_HEIGHT);
	if ( gCeilgTexture == NULL )
    {
        printf( "Texture could not be created! SDL_Error: %s\n", SDL_GetError() );
        return -1;
    }
	
    if ( TTF_Init () < 0 )
    {
        printf( "TTF could not initialize! TTF_Error: %s\n", TTF_GetError() );
        return -1;
    }
	
	if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 2048 ) < 0 )
	{
		printf( "SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError() );
	}
	
	snd_meinleben = Mix_LoadWAV( "res/snd/mein_leben.wav" );
    
    SDL_Color color = { 0x00, 0x00, 0x00 };
    gFont = TTF_OpenFont( "res/arial.ttf", 12 );
    if ( gFont == NULL)
    {
        printf ("Could not load font! SDL error: %s\n", TTF_GetError());
    }
    
    SDL_SetWindowTitle (window, "Raycaster in C++ and SDL");
    
	SDL_Texture* allWalls = loadTexture("res/txtrs/w3d_allwalls.png");
	
	txt_walls[0] = cropTexture (allWalls, 0, 0);
	txt_walls[1] = cropTexture (allWalls, 0, 1);
	txt_walls[2] = cropTexture (allWalls, 4, 3);
	
	srf_ceilg = loadSurface ("res/txtrs/w3d_redbrick.png");
	srf_floor = loadSurface ("res/txtrs/w3d_bluewall.png");
	
    return 0;
}

SDL_Texture* cropTexture (SDL_Texture* src, int x, int y)
{
	SDL_Texture* dst = SDL_CreateTexture(gRenderer, gSurface->format->format, SDL_TEXTUREACCESS_TARGET, 64, 64);
	SDL_Rect rect = {64 * x, 64 * y, 64, 64};
	SDL_SetRenderTarget(gRenderer, dst);
	SDL_RenderCopy(gRenderer, src, &rect, NULL);
	SDL_SetRenderTarget(gRenderer, NULL);
	
	return dst;
}

SDL_Texture* loadTexture (const char * file)
{
	SDL_Surface* srfc = loadSurface(file);
	if(!srfc) {
		printf("IMG_LoadPNG_RW: %s\n", IMG_GetError());
		// handle error
	}
	
	SDL_Texture* txt = SDL_CreateTextureFromSurface(gRenderer, srfc);
	
	SDL_FreeSurface(srfc);
	
	return txt;
}

SDL_Surface* loadSurface (const char * file)
{
	SDL_RWops* rwop = SDL_RWFromFile(file, "rb");
	SDL_Surface* loaded = IMG_LoadPNG_RW(rwop);
	SDL_Surface* conv=NULL;
	if (loaded != NULL)
	{
		conv = SDL_ConvertSurface(loaded, gSurface->format, 0);
		SDL_FreeSurface(loaded);
	} 
	return conv;	
}

void renderScene ()
{

	for (int i = 0; i < SCREEN_WIDTH; i++)
	{
        double r_x = 1.0;
        double r_y = scr_pts[i]; // precalculated
		double base = sqrt(r_x * r_x + r_y * r_y);
		r_x = r_x / base;
		r_y = r_y / base;
        
        // rotate this ray with teta
        double rot_x = cos (teta) * r_x - sin (teta) * r_y;
        double rot_y = sin (teta) * r_x + cos (teta) * r_y;
        
        // step sizes
        int step_x = sgn(rot_x);
        int step_y = sgn(rot_y);
        
        // grid lines and hitpoints to calculate
        int l_vx = round(px + 0.5 * step_x);
        double l_vy = -1;
        int l_hy = round(py + 0.5 * step_y);
        double l_hx = -1;
        
        // find hitpoint
        double dist = -1;
		int txt_x = -1; // 0..63 (texture width = 64)
		
        double hit_x = 0, hit_y = 0;
		int wall_idx;
        while (dist < 0)
        {
            // calculate the hitpoints with the grid lines
            if (l_vy == -1 && step_x != 0)
                l_vy = py + (l_vx - px) * (rot_y / rot_x);
            
            if (l_hx == -1 && step_y != 0)
                l_hx = px + (l_hy - py) * (rot_x / rot_y);
            
            // determine which one "wins" (= shortest distance)
            bool vertWin;
            if (l_vy != -1 && l_hx != -1)
            {    // 2 candidates, choose closest one
                vertWin = step_x * (l_vx - px) < step_x * (l_hx - px);
            }
            else 
            {    // one candidate
                vertWin = l_vy != -1;
            }
            
            // determine array indices
            int arr_x = -1, arr_y = -1;
            if (vertWin)
            {                
                hit_x = l_vx;
                hit_y = l_vy;
				
				txt_x = 64 * (hit_y - (int) hit_y);
				if ( step_x == 1)
				{ 	// // looking from the left, mirror the texture to correct
					txt_x = 63 - txt_x;
				}
				
                l_vx += step_x;
                l_vy = -1;
                
                arr_x = step_x < 0 ? hit_x - 1: hit_x;
                arr_y = GRID_HEIGHT - hit_y;
            }
            else 
            {
                hit_x = l_hx;
                hit_y = l_hy;
                
				txt_x = 64 * (hit_x - (int) hit_x);
				if ( step_y == -1)
				{ 	// looking from above, mirror the texture to correct
					txt_x = 63 - txt_x;
				}
				
                l_hx = -1;                    
                l_hy += step_y;    
            
                arr_x = hit_x;
                arr_y = GRID_HEIGHT - (step_y < 0 ? hit_y - 1: hit_y) - 1;
            }
			
            wall_idx = world[arr_y][arr_x];     
            if (wall_idx != 0)
            {    // we've hit a block
                double dx = hit_x - px;
                double dy = hit_y - py;
                dist = sqrt (dx * dx + dy * dy);
            }
        }
        
        // correct distance and calculate height
        double corrected = dist * distortion[i];
        int height = SCREEN_HEIGHT / corrected;
		
		int y = (SCREEN_HEIGHT - height) / 2;
		int darkness = 255;
		if (corrected > DARKEST_DIST)
			darkness = DARKNESS_ALPHA;
		else if (corrected <= MIN_DIST)
			darkness = 255;
		else // interpolate
			darkness = (int) ( (corrected - MIN_DIST) * (DARKNESS_ALPHA - 255) / (DARKEST_DIST - MIN_DIST) + 255);
		
		SDL_Rect src = { txt_x, 0, 1, 64 };
		SDL_Rect dst = { i, y, 1, height };		
		
		if (wall_idx - 1 >= 3)
		{
			printf("%d\n", wall_idx);
		}
		SDL_Texture* txt = txt_walls[wall_idx - 1];
		
		SDL_SetTextureColorMod( txt, darkness, darkness, darkness );
		SDL_RenderCopy(gRenderer, txt, &src, &dst);
		
		// get floor texture pixels
		
		if (y > 0)
		{
			Uint32 floor [y];
			Uint32 ceilg [y];
			Uint32* pixsflr = (Uint32*) srf_floor->pixels;
			Uint32* pixsclg = (Uint32*) srf_ceilg->pixels;
			for (int j = y - 1 ; j >= 0; j--)
			{
				double rev_height =  SCREEN_HEIGHT - 2 * j;	
				double rev_corr = SCREEN_HEIGHT / rev_height;
				double rev_dist = rev_corr / distortion[i];
				
				double real_x = px + rot_x * rev_dist;
				double real_y = py + rot_y * rev_dist;
				
				real_x = real_x - (int) real_x;
				real_y = real_y - (int) real_y;
				if (real_x < 0) real_x += 1;
				if (real_y < 0) real_y += 1;
				int tx = (int)(real_x * 64);
				int ty = (int)(real_y * 64);
				
				int darkflr = 255;
				if (rev_corr > DARKEST_DIST)
					darkflr = DARKNESS_ALPHA;
				else if (rev_corr <= MIN_DIST)
					darkflr = 255;
				else // interpolate
					darkflr = (int) ( (rev_corr - MIN_DIST) * (DARKNESS_ALPHA - 255) / (DARKEST_DIST - MIN_DIST) + 255);
				double scale = 1.0 * darkflr / 255;
				
				Uint32 pixflr = (Uint32) pixsflr [64 * ty + tx];
				Uint32 pixclg = (Uint32) pixsclg [64 * ty + tx];
				
				Uint32 f_r = (( pixflr >> 16 ) & 0xFF ) * scale;
				Uint32 f_g = (( pixflr >> 8 ) & 0xFF ) * scale;
				Uint32 f_b = (( pixflr >> 0 ) & 0xFF ) * scale;
				floor[y - 1 - j] = (f_r << 16) + (f_g << 8) + (f_b << 0);
				
				Uint32 g_r = (( pixclg >> 16 ) & 0xFF ) * scale;
				Uint32 g_g = (( pixclg >> 8 ) & 0xFF ) * scale;
				Uint32 g_b = (( pixclg >> 0 ) & 0xFF ) * scale;
				ceilg[j] = (g_r << 16) + (g_g << 8) + (g_b << 0);
			}
			
			int pitch = 1 * sizeof(Uint32);
			SDL_Rect rect = {0, 0, 1, y};
			
			if (SDL_LockTexture(gFloorTexture, &rect, &mPixels, &pitch) != 0){
				printf("Error: %s\n", SDL_GetError() );
			}
			Uint8* pixels = (Uint8 *) mPixels;
			memcpy(pixels, floor, y * pitch);
			SDL_UnlockTexture(gFloorTexture);
			SDL_Rect dstflr = {i, y + height, 1, y};
			SDL_RenderCopy (gRenderer, gFloorTexture, &rect, &dstflr);
			
			if (SDL_LockTexture(gCeilgTexture, &rect, &mPixels, &pitch) != 0){
				printf("Error: %s\n", SDL_GetError() );
			}
			pixels = (Uint8 *) mPixels;
			memcpy(pixels, ceilg, y * pitch);
			SDL_UnlockTexture(gCeilgTexture);
			
			SDL_Rect dstclg = {i, 0, 1, y};
			SDL_RenderCopy (gRenderer, gCeilgTexture, &rect, &dstclg);
		}
		
    }
}

void updateTicks (){
    int ticksNow = SDL_GetTicks();
    diffTicks = ticksNow - ticks;
    ticks = ticksNow;
}

void renderFPS ()
{
    int fps = 0;
    if (diffTicks != 0)
    {
        fps = 1000 / diffTicks;
    }    
    
    SDL_Color color = { COLOR_WHITE };
    std::string fps_str ("FPS: ");
    std::string full = fps_str + std::to_string(fps);
    
    SDL_Surface * surface = TTF_RenderText_Blended(gFont, full.c_str(), color);
    int w = surface->w;
    int h = surface->h;
    SDL_Texture * texture = SDL_CreateTextureFromSurface( gRenderer, surface );
    SDL_FreeSurface( surface );
    SDL_Rect rect = { 0, 0, w, h };
    SDL_RenderCopy(gRenderer, texture, NULL, &rect);
}

void close ()
{
    if (gRenderer != NULL)
        SDL_DestroyRenderer(gRenderer);
    if (gSurface != NULL)
        SDL_FreeSurface(gSurface);
    if (window != NULL)
        SDL_DestroyWindow(window);
    if (gTexture != NULL)
        SDL_DestroyTexture(gTexture);
    if (gFont != NULL){
        TTF_CloseFont(gFont);
    }
    
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void handleInput (const Uint8* states)
{    
    speed = 0.0;
    turn = 0.0;
    if( states[ SDL_SCANCODE_UP ] )
    {
        speed += DEFAULT_SPEED;
    }
    if( states[ SDL_SCANCODE_DOWN ] )
    {
        speed -= DEFAULT_SPEED;
    }
    if( states[ SDL_SCANCODE_LEFT ] )
    {
        turn += TURN_SPEED;
    }
    if( states[ SDL_SCANCODE_RIGHT ] )
    {
        turn -= TURN_SPEED;
    }
    if( states[ SDL_SCANCODE_LSHIFT ] )
    {
        speed *= 2;
        turn *= 2;
    }
	
	if ( states [ SDL_SCANCODE_SPACE ] )
	{
		Mix_PlayChannel( 1, snd_meinleben, 0 );
	}
}

void updateWorld (){
    double coss = cos(teta);
	double sinn = sin(teta);    
	
	int dsgn = sgn(speed); // for when walking backwards
	double sx = sgn(coss) * dsgn; 
	double sy = sgn(sinn) * dsgn;
		
	double dt = (diffTicks / 1000.0);
	double dp = dt * speed;
	double dx = coss * dp;
	double dy = sinn * dp;
	
	double px_new = px + dx;
	double py_new = py + dy;
	
	// collision detection
	double cx_b = px_new - sx * MIN_DIST;
	double cx_f = px_new + sx * MIN_DIST;
	double cy_b = py_new - sy * MIN_DIST;
	double cy_f = py_new + sy * MIN_DIST;
	if (	!isColliding(cx_f, cy_f)
		&&	!isColliding(cx_b, cy_f)
		&&	!isColliding(cx_f, cy_b) )
	{	// direction where playing is looking at
		px = px_new;
		py = py_new;
	} 	
	else if (!isColliding(cx_f, py + MIN_DIST)
		&& 	 !isColliding(cx_f, py - MIN_DIST))
	{ // X-direction
		px = px_new;
	}
	else if (!isColliding(px + MIN_DIST, cy_f)
		&& 	 !isColliding(px - MIN_DIST, cy_f))
	{ // Y-direction
		py = py_new;
	} 
	// else: no movement possible (corner)	
	
    double diffTurn = dt * turn;
    teta += diffTurn;
}

bool isColliding (double x, double y)
{
	int arr_x = (int) x;
	int arr_y = GRID_HEIGHT - 1 - (int) y;
	
	return world [arr_y][arr_x] != 0;
}

int main( int argc, char* args[] )
{
    if (init () < 0)
        return -1;

    bool quit = false;
    
    while (!quit)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // User requests quit
            if( event.type == SDL_QUIT )
            {
                quit = true;
            } 
        }
        
        const Uint8* keyStates = SDL_GetKeyboardState( NULL );
        handleInput(keyStates);
        updateTicks();
        updateWorld();
        
                
        //Fill the surface black
        SDL_SetRenderDrawColor ( gRenderer, 0x00, 0x00, 0x00, 0xFF );
        SDL_RenderClear(gRenderer);
        
        SDL_SetRenderTarget(gRenderer, gTexture);
        SDL_RenderClear(gRenderer);
        
        renderScene();
        renderFPS();
        
        SDL_SetRenderTarget(gRenderer, NULL);

        SDL_RenderCopy(gRenderer, gTexture, NULL, NULL);
        
        // draw everything
        SDL_RenderPresent(gRenderer);
        
        SDL_Delay (1);
    }
    close();

    //Destroy window
    SDL_DestroyWindow( window );

    //Quit SDL subsystems
    SDL_Quit();

    
    return 0;
}