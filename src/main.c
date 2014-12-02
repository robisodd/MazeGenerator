#include "pebble.h"
#define UPDATE_MS 50 // Refresh rate in ms
#define UPDATE_MAP_MS 5 // Update map rate in ms
#define MAPSIZE 18   // Size of map: 20x20
#define ZOOM 8       // Number of pixels per map square
#define MAP_X 0      // Upper left corner of map
#define MAP_Y 10     // Upper left corner of map

typedef struct PlayerVar {
  int32_t x;                  // Player's X Position
  int32_t y;                  // Player's Y Position
  int32_t facing;             // Player Direction Facing (from 0 - TRIG_MAX_ANGLE)
} PlayerVar;
static PlayerVar player;
static Window *window;
static GRect window_frame;
static Layer *graphics_layer;
//static AppTimer *timer;

// ------------------------------------------------------------------------ //
//  Map Functions
// ------------------------------------------------------------------------ //
static int8_t map[MAPSIZE * MAPSIZE];  // int8 means cells can be from -127 to 128

/*
typedef struct GPoint {
  int16_t x; //! The x-coordinate.
  int16_t y; //! The y-coordinate.
} GPoint;
#define GPoint(x, y) ((GPoint){(x), (y)})

0xabccdddd  b=start cc=WhereFrom  dddd = To
 cc c d dddd
 00 0 1 0001 East  / Right
 01 1 2 0010 South / Down
 10 2 4 0100 West  / Left
 11 3 8 1000 North / Up
 NV     NWSE
negative(1) / vertical(1)

  Cursor XY = Start XY
  Begin
  If No tries left (cell=15) then {
   if Cursor XY = StartXY then exit
   go to previous cell
  }
  Random direction from remaining options
  See if you can go that direction
  flag current cell that you've tried that direction
  If yes, update the cell going to with where youve been, update cursor position and Loop To Begin
  If not, then Loop To Begin  
*/
int32_t startx=0; int32_t starty=5;
int32_t cursorx = 0, cursory = 5, next=1;

void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  for (int16_t i=0; i<MAPSIZE*MAPSIZE; i++) map[i] = 0; // Fill map with 0s
  startx=MAPSIZE/2;starty=0;
  cursorx=startx;cursory=starty;
}
void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  for (int16_t i=0; i<MAPSIZE*MAPSIZE; i++) map[i] = 0; // Fill map with 0s
  startx=rand()%MAPSIZE;starty=rand()%MAPSIZE;
  cursorx=startx;cursory=starty;
}
void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  for (int16_t i=0; i<MAPSIZE*MAPSIZE; i++) map[i] = 0; // Fill map with 0s
  startx=0;starty=1;
  cursorx=startx;cursory=starty;
}


void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
}



void GenerateMap() {
  int32_t x=0, y=0; 
  int8_t try=0;
  
  //while(true) {
    int32_t current = cursory * MAPSIZE + cursorx;
    if((map[current] & 15) == 15) {  // If No Tries Left
      if(cursory==starty && cursorx==startx) {map[current]=1; return;}  // If back at the start, then we're done.
      switch(map[current] >> 4) { // Else go back to the previous cell:  NOTE: If the 1st two bits are used, need to "&3" mask this
       case 0: cursorx++; break;
       case 1: cursory++; break;
       case 2: cursorx--; break;
       case 3: cursory--; break;
      }
      map[current]=next; next=1;
    } else {
      do try = rand()%4; while (map[current] & (1<<try));  // Pick Random Directions until that direction hasn't been tried
      map[current] |= (1<<try); // turn on bit in this cell saying this path has been tried
      // below is just: x=0, y=0; if(try=0)x=1; if(try=1)y=1; if(try=2)x=-1; if(try=3)y=-1;
      y=(try&1); x=y^1; if(try&2){y=(~y)+1; x=(~x)+1;} //  y = try's 1st bit, x=y with 1st bit xor'd (toggled).  Then "Two's Complement Negation" if try's 2nd bit=1
      
      // Move if spot is blank and every spot around it is blank (except where it came from)
      if((cursory+y)>0 && (cursory+y)<MAPSIZE-1 && (cursorx+x)>0 && (cursorx+x)<MAPSIZE-1) // Make sure not moving to or over boundary
        if(map[(cursory+y) * MAPSIZE + cursorx + x]==0)                                    // Make sure not moving to a dug spot
          if((map[(cursory+y-1) * MAPSIZE + cursorx+x]==0 || try==1))                        // Nothing above (unless came from above)
            if((map[(cursory+y+1) * MAPSIZE + cursorx+x]==0 || try==3))                     // nothing below (unless came from below)
              if((map[(cursory+y) * MAPSIZE + cursorx+x - 1]==0 || try==0))                  // nothing to the left (unless came from left)
                if((map[(cursory+y) * MAPSIZE + cursorx + x + 1]==0 || try==2)) {           // nothing to the right (unless came from right)
                  next=2;          
                  cursorx += x; cursory += y;                                              // All's good!  Let's move
                  map[cursory * MAPSIZE + cursorx] |= ((try+2)%4) << 4; //record in new cell where ya came from -- the (try+2)%4 is because when you move west, you came from east
                }
    }
  //} //End While True
}

int8_t getmap(int16_t x, int16_t y) {
  if (x<0 || x>=MAPSIZE || y<0 || y>=MAPSIZE) return -1;
  return map[(int)y * MAPSIZE + (int)x];
}


void setmap(int16_t x, int16_t y, int8_t value) {
  if ((x >= 0) && (x < MAPSIZE) && (y >= 0) && (y < MAPSIZE))
    map[y * MAPSIZE + x] = value;
}

// ------------------------------------------------------------------------ //
//  Drawing Functions
// ------------------------------------------------------------------------ //
static void graphics_layer_update(Layer *me, GContext *ctx) {
  /*
  static char text[40];  //Buffer to hold text
  GRect textframe = GRect(0, 123, 143, 20);  // Text Box Position and Size
  snprintf(text, sizeof(text), " x:%d y:%d", (int)(cursorx), (int)(cursory));  // What text to draw
  graphics_context_set_fill_color(ctx, 0); graphics_fill_rect(ctx, textframe, 0, GCornerNone);  //Black Filled Rectangle
  graphics_context_set_stroke_color(ctx, 1); graphics_draw_rect(ctx, textframe);                //White Rectangle Border
  graphics_context_set_text_color(ctx, 1);  // Text Color
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_14), textframe, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);  //Write Text
*/
  
  for (int16_t x = 0; x < MAPSIZE; x++)
    for (int16_t y = 0; y < MAPSIZE; y++) {
      if(getmap(x,y)) graphics_context_set_fill_color(ctx, 0); else graphics_context_set_fill_color(ctx, 1); // Black if map cell = 0, else white
      graphics_fill_rect(ctx, GRect((x*ZOOM)+MAP_X, (y*ZOOM)+MAP_Y, ZOOM, ZOOM), 0, GCornerNone);
      if(getmap(x,y)==2) {
        //graphics_context_set_stroke_color(ctx, 1);
        //graphics_draw_pixel(ctx, GPoint((x*ZOOM)+MAP_X+2, (y*ZOOM)+MAP_Y+2));
        graphics_context_set_fill_color(ctx, 1);
        graphics_fill_rect(ctx, GRect((x*ZOOM)+MAP_X+3, (y*ZOOM)+MAP_Y+3, 2, 2), 0, GCornerNone); // Special Dot
      } 
      
        

      graphics_context_set_fill_color(ctx, (time_ms(NULL, NULL) % 250)>125?0:1);  // Flashing dot
      //graphics_fill_rect(ctx, GRect((player.x*ZOOM)+MAP_X+1, (player.y*ZOOM)+MAP_Y+1, ZOOM-2, ZOOM-2), 0, GCornerNone); // Full Square Player
      graphics_fill_rect(ctx, GRect((cursorx*ZOOM)+MAP_X+1, (cursory*ZOOM)+MAP_Y+1, ZOOM-2, ZOOM-2), 0, GCornerNone); // Smaller Square Cursor
    }
}

// ------------------------------------------------------------------------ //
//  Timer Functions
// ------------------------------------------------------------------------ //
static void timer_callback(void *data) {
  layer_mark_dirty(graphics_layer);
  app_timer_register(UPDATE_MS, timer_callback, NULL); // timer = 
}

static void update_map(void *data) {
  GenerateMap();
  app_timer_register(UPDATE_MAP_MS, update_map, NULL);
}

// ------------------------------------------------------------------------ //
//  Main Functions
// ------------------------------------------------------------------------ //
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect frame = window_frame = layer_get_frame(window_layer);

  graphics_layer = layer_create(frame);
  layer_set_update_proc(graphics_layer, graphics_layer_update);
  layer_add_child(window_layer, graphics_layer);

}

static void window_unload(Window *window) {
  layer_destroy(graphics_layer);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(window, true /* Animated */);
  window_set_click_config_provider(window, click_config_provider);
  window_set_background_color(window, 0);
  
  srand(time(NULL));  // Seed randomizer
  for (int16_t i=0; i<MAPSIZE*MAPSIZE; i++) map[i] = 0; // Fill map with 0s
  
  player = (PlayerVar){.x=5, .y=2, .facing=10000};
  app_timer_register(UPDATE_MS, timer_callback, NULL); // was timer=app_timer_register...
  update_map(NULL);
}

static void deinit(void) {
  accel_data_service_unsubscribe();
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
