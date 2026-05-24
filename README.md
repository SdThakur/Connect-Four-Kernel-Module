Project 3 - Connect Four Kernel Module

Contact Name : Satya Thakur

This is a Linux kernel module that implements a Connect Four game as a character device driver.

## Features
- Creates a character device `/dev/fourinarow`
- Supports player vs computer gameplay
- Implements all game rules and win conditions
- Handles commands: RESET, BOARD, DROPC, CTURN
- Provides proper error handling and responses

   ## Key Features
   
   1. **Character Device Creation**: The module creates `/dev/fourinarow` with proper permissions.
   
   2. **Game State Management**: Maintains an 8x8 board with '0' (empty), 'R' (Red), and 'Y' (Yellow) markers.
   
   3. **Command Handling**:
      - `RESET`: Initializes a new game and sets the player's color
      - `BOARD`: Returns the current board state
      - `DROPC`: Processes player moves with validation
      - `CTURN`: Handles computer moves
   
   4. **Game Logic**:
      - Win detection (horizontal, vertical, diagonal)
      - Tie detection
      - Turn management
      - Column validation
   
   5. **Computer Opponent**: Implements a basic AI that makes random valid moves.
   
   6. **Error Handling**: Properly responds to invalid commands, out-of-turn moves, etc.
   
   7. **Thread Safety**: Uses mutex locks to protect shared game state.

Usage
Interact with the game by writing commands to the device file and reading responses:

Commands: 
   RESET [R|Y]: Start a new game (R = play as Red, Y = play as Yellow)
   
   BOARD: Get the current board state
   
   DROPC [A-H]: Drop a chip in the specified column
   
   CTURN: Let the computer take its turn

## Building and Loading
1. Compile the module:
   make 

3. Load the module
   sudo insmod fourinarow.ko

3. Verify if its created 
   ls -l /dev/fourinarow

4. First reset the board by using this **echo "RESET Y" > /dev/fourinarow** cat /dev/fourinarow  # Should return "OK"
   echo "DROPC B" > /dev/fourinarow     #Player turns to drop into column B
      if you get permission declined use : sudo chmod 666 /dev/fourinarow to change permission
   cat /dev/fourinarow     # Should return "OK"
   echo "CTURN" > /dev/fourinarow      # Computer Moves
   echo "BOARD" > /dev/fourinarow    #to view board 
   cat /dev/fourinarow

      **WE HAVE TO USE "ECHO" TO DROP EITHER IN USER TURNS OR THE COMPUTER**

5. When done unload the module
   sudo rmmod fourinarow

