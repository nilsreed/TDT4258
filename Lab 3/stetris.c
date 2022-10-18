#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <linux/input.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <poll.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>

/* OWN DEFINES */
// Frame buffer defines
#define FB_PATH     "/dev"
#define FB_PREFIX   "fb"
#define FB_NAME     "RPi-Sense FB"
#define LIGHT_ON    0xFFFF
#define LIGHT_OFF   0x0000

// Joystick defines
#define INPUT_PATH    "/dev/input"
#define INPUT_PREFIX  "event"
#define JOYSTICK_NAME "Raspberry Pi Sense HAT Joystick"

// Game defines
#define GRID_DIM    8           // Grid dimension (8x8)
#define BOARD_BYTES GRID_DIM*GRID_DIM*sizeof(uint16_t) //Number of bytes needed to represent the LED matrix in memory
#define NO_COLOURS  6

/* END OWN DEFINES */


// The game state can be used to detect what happens on the playfield
#define GAMEOVER   0
#define ACTIVE     (1 << 0)
#define ROW_CLEAR  (1 << 1)
#define TILE_ADDED (1 << 2)

// If you extend this structure, either avoid pointers or adjust
// the game logic allocate/deallocate and reset the memory
typedef struct {
  bool occupied;
  uint16_t colour;
} tile;

typedef struct {
  unsigned int x;
  unsigned int y;
} coord;

typedef struct {
  coord const grid;                     // playfield bounds
  unsigned long const uSecTickTime;     // tick rate
  unsigned long const rowsPerLevel;     // speed up after clearing rows
  unsigned long const initNextGameTick; // initial value of nextGameTick

  unsigned int tiles; // number of tiles played
  unsigned int rows;  // number of rows cleared
  unsigned int score; // game score
  unsigned int level; // game level

  tile *rawPlayfield; // pointer to raw memory of the playfield
  tile **playfield;   // This is the play field array
  unsigned int state;
  coord activeTile;                       // current tile

  unsigned long tick;         // incremeted at tickrate, wraps at nextGameTick
                              // when reached 0, next game state calculated
  unsigned long nextGameTick; // sets when tick is wrapping back to zero
                              // lowers with increasing level, never reaches 0
} gameConfig;



gameConfig game = {
                   .grid = {8, 8},
                   .uSecTickTime = 10000,
                   .rowsPerLevel = 2,
                   .initNextGameTick = 50,
};

/* OWN GLOBALS*/
int framebuffer_fd;   // Framebuffer file descriptor, or more accurately; index into the process' table of file descriptor, contains index of framebuffer.
int joystick_fd;      // File descriptor for joystick input.
uint16_t* LED_matrix; // Pointer to LED matrix mapped into memory by mmap in initializeSenseHat()

//                              RED     GREEN   BLUE    CYAN    YELLOW  ORANGE
uint16_t colours[NO_COLOURS] = {0xF800, 0x07E0, 0x001F, 0x07FF, 0XFFE0, 0xFBE0}; 

/* END OWN GLOBALS*/

// Randomly choose a colour from the predefined colours
uint16_t colour_sel(){
  return colours[rand() % NO_COLOURS];
}
// Returns 1 if dir's name is prefixed by fb
// meaning it's a framebuffer
int fb_filter(const struct dirent* dir){
    if (!strncmp(FB_PREFIX, dir->d_name, strlen(FB_PREFIX))){
    return 1;
  }
  return 0;
}

// Returns 1 if dir's name is prefixed by event
// meaning it's an event device
int evdev_filter(const struct dirent* dir){
  if (!strncmp(INPUT_PREFIX, dir->d_name, strlen(INPUT_PREFIX))){
    return 1;
  }
  return 0;
}

int framebuffer_init(){
  struct dirent** namelist;
  struct fb_fix_screeninfo info;
  int fd = -1;
  
  // Scan /dev for framebuffer devices
  int no_devices = scandir(FB_PATH, &namelist, fb_filter, alphasort);
  
  // Iterate through all framebuffer devices found
  for (int i = 0; i < no_devices; i++){
    char fullpath[256];
    
    // Assemble full path for so it can be opened by open
    snprintf(fullpath, sizeof(fullpath), "%s/%s", FB_PATH, namelist[i]->d_name);
    // Open framebuffer device
    fd = open(fullpath, O_RDWR);
    
    // If opening failed, go on to next iteration of loop
    if (fd < 0) continue;
    
    // Get info from framebuffer device
    ioctl(fd, FBIOGET_FSCREENINFO, &info);
    
    // Check if it is the correct framebuffer
    // device by comparing identifiers. If it is,
    // we can stop iterating through the name list
    if (!strcmp(FB_NAME, info.id)){
      break;
    }
    
    // If it was the wrong framebuffer, close it and reset fd
    close(fd);
    fd = -1;
  }
  
  // Iterate through namelist to free
  // dynamically allocated memory
  for (int i = 0; i < no_devices; i++){
    free(namelist[i]);
  }
  
  return fd;
}

int joystick_init(){
  struct dirent** namelist;
  int fd = -1;
  
  // Scan /dev/input for event devices
  int no_devices = scandir(INPUT_PATH, &namelist, evdev_filter, alphasort);
  
  // Iterate through all event devices found
  for (int i = 0; i < no_devices; i++){
    char fullpath[256];
    char name[256];
    
    // Assemble full path for so it can be opened by open
    snprintf(fullpath, sizeof(fullpath), "%s/%s", INPUT_PATH, namelist[i]->d_name);
    // Open framebuffer device
    fd = open(fullpath, O_RDONLY);
    
    // If opening failed, go on to next iteration of loop
    if (fd < 0) continue;
    
    // Get name from event device
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    
    // Check if it is the correct event device
    // device by comparing identifiers. If it is,
    // we can stop iterating through the name list
    if (!strcmp(JOYSTICK_NAME, name)){
      break;
    }
    
    // If it was the wrong event device, close it and reset fd
    close(fd);
    fd = -1;
  }
  
  // Iterate through namelist to free
  // dynamically allocated memory
  for (int i = 0; i < no_devices; i++){
    free(namelist[i]);
  }
  
  return fd;
}


// This function is called on the start of your application
// Here you can initialize what ever you need for your task
// return false if something fails, else true
bool initializeSenseHat() {
  // Open the framebuffer for reading
  // Need read and write permission so that mmap will work.
  framebuffer_fd = framebuffer_init();
  if (framebuffer_fd == -1){
    fprintf(stderr, "ERROR: Failed to open framebuffer!\n");
    return false;
  }
  
  // Map framebuffer into process memory so we can write directly to a variable
  LED_matrix = mmap(NULL, BOARD_BYTES, PROT_WRITE | PROT_READ, MAP_SHARED, framebuffer_fd, 0);
  if (LED_matrix == MAP_FAILED){ // Check if mapping succeeded
    close(framebuffer_fd);
    fprintf(stderr, "ERROR: Failed to map framebuffer to memory!\n");
    return false;
  }
  
  memset(LED_matrix, 0x00, BOARD_BYTES); // Clear LED matrix
  
  // Open input event for joystick
  joystick_fd = joystick_init();
  if (joystick_fd == -1){ // Check if opening succeeded
    fprintf(stderr, "ERROR: Failed to open joystick input event!\n");
    munmap(LED_matrix, BOARD_BYTES);
    close(framebuffer_fd);
    return false;
  }
  
  // seed random number generator with number of seconds since epoch
  // to get different result every time
  srand(time(NULL));
  
  return true;
}

// This function is called when the application exits
// Here you can free up everything that you might have opened/allocated
void freeSenseHat() {
  // Clear LED matrix before unmapping it from memory
  memset(LED_matrix, 0x00, BOARD_BYTES); // Clear LED matrix
  // Unmap framebuffer from memory
  if(munmap(LED_matrix, BOARD_BYTES) == -1){
    fprintf(stderr, "ERROR: Unable to unmap framebuffer from memory!\n");
  }
  // Close frame buffer file
  close(framebuffer_fd);
  // Close input event
  close(joystick_fd);
}

// This function should return the key that corresponds to the joystick press
// KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, with the respective direction
// and KEY_ENTER, when the the joystick is pressed
// !!! when nothing was pressed you MUST return 0 !!!
int readSenseHatJoystick() {  
  struct pollfd joystick_poll = {
          .events = POLLIN,
          .fd = joystick_fd
  };
  // Parameters to poll: joystick_poll, number of file descriptors and timeout
  // Basically checks if there's any input to get from joystick input event at
  // time of calling
  if (poll(&joystick_poll, 1, 0)){
    struct input_event event_buffer[8]; // input buffer
    int bytes_read = read(joystick_fd, event_buffer, 8*sizeof(struct input_event)); // Read from event device into event_buffer
    
    if (bytes_read < (int) sizeof(struct input_event)){ // Too few bytes have been read
      fprintf(stderr, "ERROR: Too few bytes read from input event to event buffer!\n");
      return 0;
    }
    
    // Find the first key event in the buffer and return it
    // since the joystick inputs map to arrow keys and enter
    // by default
    for (int i = 0; i < bytes_read/sizeof(struct input_event); i++){
      if (event_buffer[i].type != EV_KEY) continue;
      if (event_buffer[i].value != 2) continue;
      return event_buffer[i].code;
    }
  }
  
  return 0;
}


// This function should render the gamefield on the LED matrix. It is called
// every game tick. The parameter playfieldChanged signals whether the game logic
// has changed the playfield
void renderSenseHatMatrix(bool const playfieldChanged) {
  // If the playfield has changed, we need to re-render the board
  if (playfieldChanged){
    // Iterate through pixel grid in y and x directions
    for (int yy = 0; yy < game.grid.y; yy++){
      for (int xx = 0; xx < game.grid.x; xx++){
        // If the pixel is occupied, the tile's own colour is written, if it isn't, LIGHT on is written.
        LED_matrix[yy*game.grid.y + xx] = (game.playfield[yy][xx].occupied) ? game.playfield[yy][xx].colour : LIGHT_OFF;
      }
    }
  }
}

// The game logic uses only the following functions to interact with the playfield.
// if you choose to change the playfield or the tile structure, you might need to
// adjust this game logic <> playfield interface

static inline void newTile(coord const target) {
  game.playfield[target.y][target.x].occupied = true;
  game.playfield[target.y][target.x].colour = colour_sel(); // This line has been added, to add the constant colour to tiles required in the task
}

static inline void copyTile(coord const to, coord const from) {
  memcpy((void *) &game.playfield[to.y][to.x], (void *) &game.playfield[from.y][from.x], sizeof(tile));
}

static inline void copyRow(unsigned int const to, unsigned int const from) {
  memcpy((void *) &game.playfield[to][0], (void *) &game.playfield[from][0], sizeof(tile) * game.grid.x);

}

static inline void resetTile(coord const target) {
  memset((void *) &game.playfield[target.y][target.x], 0, sizeof(tile));
}

static inline void resetRow(unsigned int const target) {
  memset((void *) &game.playfield[target][0], 0, sizeof(tile) * game.grid.x);
}

static inline bool tileOccupied(coord const target) {
  return game.playfield[target.y][target.x].occupied;
}

static inline bool rowOccupied(unsigned int const target) {
  for (unsigned int x = 0; x < game.grid.x; x++) {
    coord const checkTile = {x, target};
    if (!tileOccupied(checkTile)) {
      return false;
    }
  }
  return true;
}


static inline void resetPlayfield() {
  for (unsigned int y = 0; y < game.grid.y; y++) {
    resetRow(y);
  }
}

// Below here comes the game logic. Keep in mind: You are not allowed to change how the game works!
// that means no changes are necessary below this line! And if you choose to change something
// keep it compatible with what was provided to you!

bool addNewTile() {
  game.activeTile.y = 0;
  game.activeTile.x = (game.grid.x - 1) / 2;
  if (tileOccupied(game.activeTile))
    return false;
  newTile(game.activeTile);
  return true;
}

bool moveRight() {
  coord const newTile = {game.activeTile.x + 1, game.activeTile.y};
  if (game.activeTile.x < (game.grid.x - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}

bool moveLeft() {
  coord const newTile = {game.activeTile.x - 1, game.activeTile.y};
  if (game.activeTile.x > 0 && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool moveDown() {
  coord const newTile = {game.activeTile.x, game.activeTile.y + 1};
  if (game.activeTile.y < (game.grid.y - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool clearRow() {
  if (rowOccupied(game.grid.y - 1)) {
    for (unsigned int y = game.grid.y - 1; y > 0; y--) {
      copyRow(y, y - 1);
    }
    resetRow(0);
    return true;
  }
  return false;
}

void advanceLevel() {
  game.level++;
  switch(game.nextGameTick) {
  case 1:
    break;
  case 2 ... 10:
    game.nextGameTick--;
    break;
  case 11 ... 20:
    game.nextGameTick -= 2;
    break;
  default:
    game.nextGameTick -= 10;
  }
}

void newGame() {
  game.state = ACTIVE;
  game.tiles = 0;
  game.rows = 0;
  game.score = 0;
  game.tick = 0;
  game.level = 0;
  resetPlayfield();
}

void gameOver() {
  game.state = GAMEOVER;
  game.nextGameTick = game.initNextGameTick;
}


bool sTetris(int const key) {
  bool playfieldChanged = false;

  if (game.state & ACTIVE) {
    // Move the current tile
    if (key) {
      playfieldChanged = true;
      switch(key) {
      case KEY_LEFT:
        moveLeft();
        break;
      case KEY_RIGHT:
        moveRight();
        break;
      case KEY_DOWN:
        while (moveDown()) {};
        game.tick = 0;
        break;
      default:
        playfieldChanged = false;
      }
    }

    // If we have reached a tick to update the game
    if (game.tick == 0) {
      // We communicate the row clear and tile add over the game state
      // clear these bits if they were set before
      game.state &= ~(ROW_CLEAR | TILE_ADDED);

      playfieldChanged = true;
      // Clear row if possible
      if (clearRow()) {
        game.state |= ROW_CLEAR;
        game.rows++;
        game.score += game.level + 1;
        if ((game.rows % game.rowsPerLevel) == 0) {
          advanceLevel();
        }
      }

      // if there is no current tile or we cannot move it down,
      // add a new one. If not possible, game over.
      if (!tileOccupied(game.activeTile) || !moveDown()) {
        if (addNewTile()) {
          game.state |= TILE_ADDED;
          game.tiles++;
        } else {
          gameOver();
        }
      }
    }
  }

  // Press any key to start a new game
  if ((game.state == GAMEOVER) && key) {
    playfieldChanged = true;
    newGame();
    addNewTile();
    game.state |= TILE_ADDED;
    game.tiles++;
  }

  return playfieldChanged;
}

int readKeyboard() {
  struct pollfd pollStdin = {
       .fd = STDIN_FILENO,
       .events = POLLIN
  };
  int lkey = 0;

  if (poll(&pollStdin, 1, 0)) {
    lkey = fgetc(stdin);
    if (lkey != 27)
      goto exit;
    lkey = fgetc(stdin);
    if (lkey != 91)
      goto exit;
    lkey = fgetc(stdin);
  }
 exit:
    switch (lkey) {
      case 10: return KEY_ENTER;
      case 65: return KEY_UP;
      case 66: return KEY_DOWN;
      case 67: return KEY_RIGHT;
      case 68: return KEY_LEFT;
    }
  return 0;
}

void renderConsole(bool const playfieldChanged) {
  if (!playfieldChanged)
    return;

  // Goto beginning of console
  fprintf(stdout, "\033[%d;%dH", 0, 0);
  for (unsigned int x = 0; x < game.grid.x + 2; x ++) {
    fprintf(stdout, "-");
  }
  fprintf(stdout, "\n");
  for (unsigned int y = 0; y < game.grid.y; y++) {
    fprintf(stdout, "|");
    for (unsigned int x = 0; x < game.grid.x; x++) {
      coord const checkTile = {x, y};
      fprintf(stdout, "%c", (tileOccupied(checkTile)) ? '#' : ' ');
    }
    switch (y) {
      case 0:
        fprintf(stdout, "| Tiles: %10u\n", game.tiles);
        break;
      case 1:
        fprintf(stdout, "| Rows:  %10u\n", game.rows);
        break;
      case 2:
        fprintf(stdout, "| Score: %10u\n", game.score);
        break;
      case 4:
        fprintf(stdout, "| Level: %10u\n", game.level);
        break;
      case 7:
        fprintf(stdout, "| %17s\n", (game.state == GAMEOVER) ? "Game Over" : "");
        break;
    default:
        fprintf(stdout, "|\n");
    }
  }
  for (unsigned int x = 0; x < game.grid.x + 2; x++) {
    fprintf(stdout, "-");
  }
  fflush(stdout);
}


inline unsigned long uSecFromTimespec(struct timespec const ts) {
  return ((ts.tv_sec * 1000000) + (ts.tv_nsec / 1000));
}

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  // This sets the stdin in a special state where each
  // keyboard press is directly flushed to the stdin and additionally
  // not outputted to the stdout
  {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
  }

  // Allocate the playing field structure
  game.rawPlayfield = (tile *) malloc(game.grid.x * game.grid.y * sizeof(tile));
  game.playfield = (tile**) malloc(game.grid.y * sizeof(tile *));
  if (!game.playfield || !game.rawPlayfield) {
    fprintf(stderr, "ERROR: could not allocate playfield\n");
    return 1;
  }
  for (unsigned int y = 0; y < game.grid.y; y++) {
    game.playfield[y] = &(game.rawPlayfield[y * game.grid.x]);
  }

  // Reset playfield to make it empty
  resetPlayfield();
  // Start with gameOver
  gameOver();

  if (!initializeSenseHat()) {
    fprintf(stderr, "ERROR: could not initilize sense hat\n");
    return 1;
  };

  // Clear console, render first time
  fprintf(stdout, "\033[H\033[J");
  renderConsole(true);
  renderSenseHatMatrix(true);

  while (true) {
    struct timeval sTv, eTv;
    gettimeofday(&sTv, NULL);

    int key = readSenseHatJoystick();
    if (!key)
      key = readKeyboard();
    if (key == KEY_ENTER)
      break;

    bool playfieldChanged = sTetris(key);
    renderConsole(playfieldChanged);
    renderSenseHatMatrix(playfieldChanged);
    
    // Wait for next tick
    gettimeofday(&eTv, NULL);
    unsigned long const uSecProcessTime = ((eTv.tv_sec * 1000000) + eTv.tv_usec) - ((sTv.tv_sec * 1000000 + sTv.tv_usec));
    if (uSecProcessTime < game.uSecTickTime) {
      usleep(game.uSecTickTime - uSecProcessTime);
    }
    game.tick = (game.tick + 1) % game.nextGameTick;
  }

  freeSenseHat();
  free(game.playfield);
  free(game.rawPlayfield);

  return 0;
}
