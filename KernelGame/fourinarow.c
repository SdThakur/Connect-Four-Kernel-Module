#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#define DEVICE_NAME "fourinarow"
#define CLASS_NAME "fourinarow"
#define BOARD_SIZE 8
#define MAX_RESPONSE_SIZE 256

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Satya Thakur");
MODULE_DESCRIPTION("Connect Four Game Kernel Module");
MODULE_VERSION("1.0");

// Device attributes
static int major_number;
static struct class *fourinarow_class = NULL;
static struct device *fourinarow_device = NULL;
static struct cdev fourinarow_cdev;

// Device structure
typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    char current_player;
    char user_color;
    int game_active;
    char response[MAX_RESPONSE_SIZE];
    int response_len;
    struct mutex lock;
} game_state_t;

static game_state_t game_state;

// Permission attribute
static ssize_t permissions_show(struct device *dev, 
                              struct device_attribute *attr, 
                              char *buf)
{
    return sprintf(buf, "0666\n");
}
static DEVICE_ATTR(permissions, 0444, permissions_show, NULL);

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);

// File operations structure
static struct file_operations fops = {
    .open = device_open,
    .read = device_read,
    .write = device_write,
    .release = device_release,
};

static void init_board(void) {
    int i, j;
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            game_state.board[i][j] = '0';
        }
    }
}

/// Reset the game state
static void reset_game(char user_color) {
    mutex_lock(&game_state.lock);
    init_board();
    game_state.game_active = 1;
    game_state.user_color = user_color;
    game_state.current_player = 'Y';
    snprintf(game_state.response, MAX_RESPONSE_SIZE, "OK\n");
    game_state.response_len = strlen(game_state.response);
    mutex_unlock(&game_state.lock);
}

/// Check if the column is valid (A-H or a-h)
static int is_valid_column(char col) {
    return (col >= 'A' && col <= 'H') || (col >= 'a' && col <= 'h');
}

/// Convert column character to index (0-7)
static int column_to_index(char col) {
    if (col >= 'a' && col <= 'h') {
        return col - 'a';
    }
    return col - 'A';
}

/// Convert index to column character (A-H)
static int find_empty_row(int col) {
    int row;
    for (row = 0; row < BOARD_SIZE; row++) {
        if (game_state.board[row][col] == '0') {
            return row;
        }
    }
    return -1;
}

/// Check if the board is full
static int is_board_full(void) {
    int col;
    for (col = 0; col < BOARD_SIZE; col++) {
        if (game_state.board[BOARD_SIZE-1][col] == '0') {
            return 0;
        }
    }
    return 1;
}

/// Check if the current player has won
static int check_win(int row, int col, char player) {
    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    int i, j, k, count;
    int new_row, new_col;

    for (i = 0; i < 4; i++) {
        count = 1;
        
        for (j = 1; j < 4; j++) {
            new_row = row + j * directions[i][0];
            new_col = col + j * directions[i][1];
            
            if (new_row < 0 || new_row >= BOARD_SIZE || 
                new_col < 0 || new_col >= BOARD_SIZE || 
                game_state.board[new_row][new_col] != player) {
                break;
            }
            count++;
        }
        
        for (k = 1; k < 4; k++) {
            new_row = row - k * directions[i][0];
            new_col = col - k * directions[i][1];
            
            if (new_row < 0 || new_row >= BOARD_SIZE || 
                new_col < 0 || new_col >= BOARD_SIZE || 
                game_state.board[new_row][new_col] != player) {
                break;
            }
            count++;
        }
        
        if (count >= 4) {
            return 1;
        }
    }
    return 0;
}

/// Process the move and check for win or tie
static int process_move(int col, char player) {
    int row = find_empty_row(col);
    if (row == -1) {
        return -1;
    }
    
    game_state.board[row][col] = player;
    
    if (check_win(row, col, player)) {
        return 1;
    }
    
    if (is_board_full()) {
        return 2;
    }
    
    return 0;
}

/// Handle the "DROPC" command
static void handle_dropc(char col) {
    int col_index, result;
    
    mutex_lock(&game_state.lock);
    
    if (!game_state.game_active) {
        snprintf(game_state.response, MAX_RESPONSE_SIZE, "NOGAME\n");
        game_state.response_len = strlen(game_state.response);
        mutex_unlock(&game_state.lock);
        return;
    }
    
    if (game_state.current_player != game_state.user_color) {
        snprintf(game_state.response, MAX_RESPONSE_SIZE, "OOT\n");
        game_state.response_len = strlen(game_state.response);
        mutex_unlock(&game_state.lock);
        return;
    }
    
    if (!is_valid_column(col)) {
        snprintf(game_state.response, MAX_RESPONSE_SIZE, "INVALID\n");
        game_state.response_len = strlen(game_state.response);
        mutex_unlock(&game_state.lock);
        return;
    }
    
    col_index = column_to_index(col);
    result = process_move(col_index, game_state.current_player);
    
    switch (result) {
        case -1:
            // Column is full
            snprintf(game_state.response, MAX_RESPONSE_SIZE, "FULL\n");
            break;
        case 0:
            // move successful
            snprintf(game_state.response, MAX_RESPONSE_SIZE, "OK\n");
            game_state.current_player = (game_state.current_player == 'Y') ? 'R' : 'Y';
            break;
        case 1:
            // Player wins
            snprintf(game_state.response, MAX_RESPONSE_SIZE, "WIN\n");
            game_state.game_active = 0;
            break;
        case 2:
            // Tie
            snprintf(game_state.response, MAX_RESPONSE_SIZE, "TIE\n");
            game_state.game_active = 0;
            break;
        default:
            snprintf(game_state.response, MAX_RESPONSE_SIZE, "ERROR\n");
    }
    
    game_state.response_len = strlen(game_state.response);
    mutex_unlock(&game_state.lock);
}

/// Handle the "CTURN" command
static void handle_cturn(void) {
    int valid_cols[BOARD_SIZE];
    int num_valid = 0;
    int col;
    unsigned int rand;
    int result;
    
    mutex_lock(&game_state.lock);
    
    // Check if the game is active
    if (!game_state.game_active) {
        snprintf(game_state.response, MAX_RESPONSE_SIZE, "NOGAME\n");
        game_state.response_len = strlen(game_state.response);
        mutex_unlock(&game_state.lock);
        return;
    }
    
    // Check if it's the user's turn
    if (game_state.current_player == game_state.user_color) {
        snprintf(game_state.response, MAX_RESPONSE_SIZE, "OOT\n");
        game_state.response_len = strlen(game_state.response);
        mutex_unlock(&game_state.lock);
        return;
    }
    
    for (col = 0; col < BOARD_SIZE; col++) {
        if (find_empty_row(col) != -1) {
            valid_cols[num_valid++] = col;
        }
    }
    
    if (num_valid == 0) {
        snprintf(game_state.response, MAX_RESPONSE_SIZE, "TIE\n");
        game_state.game_active = 0;
        game_state.response_len = strlen(game_state.response);
        mutex_unlock(&game_state.lock);
        return;
    }
    
    get_random_bytes(&rand, sizeof(rand));
    col = valid_cols[rand % num_valid];
    
    result = process_move(col, game_state.current_player);
    
    switch (result) {
        case 0:
            // move successful
            snprintf(game_state.response, MAX_RESPONSE_SIZE, "OK\n");
            game_state.current_player = (game_state.current_player == 'Y') ? 'R' : 'Y';
            break;
        case 1:
            // Player wins
            snprintf(game_state.response, MAX_RESPONSE_SIZE, "LOSE\n");
            game_state.game_active = 0;
            break;
        case 2:
            // Tie
            snprintf(game_state.response, MAX_RESPONSE_SIZE, "TIE\n");
            game_state.game_active = 0;
            break;
        default:
            snprintf(game_state.response, MAX_RESPONSE_SIZE, "ERROR\n");
    }
    
    game_state.response_len = strlen(game_state.response);
    mutex_unlock(&game_state.lock);
}

/// Handle the "BOARD" command
static void handle_board(void) {
    int i, j;
    char *ptr = game_state.response;
    
    mutex_lock(&game_state.lock);
    
    for (i = BOARD_SIZE - 1; i >= 0; i--) {
        for (j = 0; j < BOARD_SIZE; j++) {
            *ptr++ = game_state.board[i][j];
        }
        *ptr++ = '\n';
    }
    *ptr = '\0';
    game_state.response_len = ptr - game_state.response;
    
    mutex_unlock(&game_state.lock);
}

/// Execute the command based on the input string
static void execute_command(const char *cmd) {
    char color;
    
    if (strncmp(cmd, "RESET", 5) == 0) {
        color = cmd[6];
        if (color == 'Y' || color == 'R') {
            reset_game(color);
        } else {
            snprintf(game_state.response, MAX_RESPONSE_SIZE, "INVALID\n");
        }
    } 
    else if (strncmp(cmd, "BOARD", 5) == 0) {
        handle_board();
    }
    else if (strncmp(cmd, "DROPC", 5) == 0) {
        char col = cmd[6];
        handle_dropc(col);
    }
    else if (strncmp(cmd, "CTURN", 5) == 0) {
        handle_cturn();
    }
    else {
        snprintf(game_state.response, MAX_RESPONSE_SIZE, "UNKNOWN\n");
    }
    
    game_state.response_len = strlen(game_state.response);
}

static int device_open(struct inode *inode, struct file *file) {
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    return 0;
}

/// Read from the device
static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset) {
    ssize_t bytes_read = 0;
    
    mutex_lock(&game_state.lock);
    
    // Check if the game is active
    if (*offset >= game_state.response_len) {
        mutex_unlock(&game_state.lock);
        return 0;
    }
    
    // Check if the buffer is large enough
    bytes_read = min(length, (size_t)(game_state.response_len - *offset));
    if (copy_to_user(buffer, game_state.response + *offset, bytes_read)) {
        mutex_unlock(&game_state.lock);
        return -EFAULT;
    }
    
    *offset += bytes_read;
    mutex_unlock(&game_state.lock);
    return bytes_read;
}

//  Write to the device
static ssize_t device_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset) {
    char *cmd = kmalloc(length + 1, GFP_KERNEL);
    
    if (!cmd) {
        return -ENOMEM;
    }
    
    if (copy_from_user(cmd, buffer, length)) {
        kfree(cmd);
        return -EFAULT;
    }
    
    cmd[length] = '\0';
    
    if (cmd[length - 1] == '\n') {
        cmd[length - 1] = '\0';
    }
    
    execute_command(cmd);
    kfree(cmd);
    
    return length;
}

static int __init fourinarow_init(void) {
    int retval;
    
    printk(KERN_INFO "FourInARow: Initializing...\n");
    
    mutex_init(&game_state.lock);
    init_board();
    game_state.game_active = 0;
    
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Failed to register device\n");
        return major_number;
    }
    
    fourinarow_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(fourinarow_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create class\n");
        return PTR_ERR(fourinarow_class);
    }
    
    fourinarow_device = device_create(fourinarow_class, NULL, 
                                    MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(fourinarow_device)) {
        class_destroy(fourinarow_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create device\n");
        return PTR_ERR(fourinarow_device);
    }
    
    // Set permissions
    retval = device_create_file(fourinarow_device, &dev_attr_permissions);
    if (retval) {
        printk(KERN_WARNING "Failed to create permissions attribute\n");
    }
    
    // Alternative permission setting
    if (sysfs_chmod_file(&fourinarow_device->kobj, 
                        &dev_attr_permissions.attr, 
                        0666)) {
        printk(KERN_WARNING "Failed to set device permissions\n");
    }
    
    cdev_init(&fourinarow_cdev, &fops);
    if (cdev_add(&fourinarow_cdev, MKDEV(major_number, 0), 1) < 0) {
        device_destroy(fourinarow_class, MKDEV(major_number, 0));
        class_destroy(fourinarow_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to add cdev\n");
        return -1;
    }
    
    printk(KERN_INFO "FourInARow: Initialized successfully\n");
    return 0;
}

static void __exit fourinarow_exit(void) {
    device_remove_file(fourinarow_device, &dev_attr_permissions);
    cdev_del(&fourinarow_cdev);
    device_destroy(fourinarow_class, MKDEV(major_number, 0));
    class_destroy(fourinarow_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "FourInARow: Module unloaded\n");
}

module_init(fourinarow_init);
module_exit(fourinarow_exit);
