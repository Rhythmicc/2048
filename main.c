#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>

#define SIZE (4)
typedef int board_t[SIZE][SIZE];
unsigned sc = 0;
struct hash_elem {
    int hash;
    int depth;
    double value;
};

enum input {
    LEFT = 0,
    RIGHT = 1,
    UP = 2,
    DOWN = 3,
    QUIT = 4
};

int value_weight[16];
int value_real[16];
int depth_map[] = {6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4};
const char *move_text[] = {"NONE", "Left", "Right", "Up", "Down", "Game Over"};
static const board_t cell_weight = {
        {17, 13, 11, 10},
        {13, 10,  9,  9},
        {11,  9,  8,  8},
        {10,  9,  8,  8}
};

static const board_t primes = {
        {22189, 28813, 37633, 43201},
        {47629, 60493, 63949, 65713},
        {69313, 73009, 76801, 84673},
        {106033, 108301, 112909, 115249}
};

void init() {
    int i;
    int cur_weight = 1;
    int cur_real = 2;
    for (i = 1; i < 16; i++) {
        value_weight[i] = cur_weight;
        value_real[i] = cur_real;
        cur_weight *= 3;
        cur_real *= 2;
    }
}

long gettime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 * 1000 + tv.tv_usec;
}

void getColor(unsigned value, char *color, size_t length) {
    uint8_t original[] = {8,255,1,255,2,255,3,255,4,255,5,255,6,255,7,0,9,0,10,0,11,0,12,0,13,0,14,0,255,0,255,0};
    uint8_t *background = original+0;
    uint8_t *foreground = original+1;
    if (value > 0) while (value--) {
            if (background+2<original+sizeof(original)) {
                background+=2;
                foreground+=2;
            }
        }
    snprintf(color,length,"\033[38;5;%d;48;5;%dm",*foreground,*background);
}

void drawBoard(board_t board, int AI) {
    int x,y;
    char color[40], reset[] = "\033[m";
    printf("\033[H");

    printf("Suggest: %9s\n", move_text[AI]);
    printf("%17d pts\n\n",sc);

    for (x=0;x<SIZE;x++) {
        for (y=0;y<SIZE;y++) {
            getColor(board[x][y],color,40);
            printf("%s",color);
            printf("       ");
            printf("%s",reset);
        }
        printf("\n");
        for (y=0;y<SIZE;y++) {
            getColor(board[x][y],color,40);
            printf("%s",color);
            if (board[x][y]!=0) {
                char s[8];
                snprintf(s,8,"%u",value_real[board[x][y]]);
                uint8_t t = 7-strlen(s);
                printf("%*s%s%*s",t-t/2,"",s,t/2,"");
            } else printf("   ·   ");
            printf("%s",reset);
        }
        printf("\n");
        for (y=0;y<SIZE;y++) {
            getColor(board[x][y],color,40);
            printf("%s",color);
            printf("       ");
            printf("%s",reset);
        }
        printf("\n");
    }
    printf("\n");
    printf("        a,w,d,s or q        \n");
    printf("\033[A");
}

int board_count_zero(const board_t b) {
    int cnt = 0;
    int i, j;
    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            if (b[i][j] == 0)
                cnt++;
        }
    }
    return cnt;
}

void board_clear(board_t b) {
    int i, j;
    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            b[i][j] = 0;
        }
    }
}

int board_hash(board_t b) {
    int i, j;
    int hash = 0;
    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            hash += b[i][j] * primes[i][j];
        }
    }
    return hash;
}

void addRandom(board_t board) {
    static bool initialized = false;
    uint8_t x,y;
    uint8_t r,len=0;
    uint8_t n,list[SIZE*SIZE][2];

    if (!initialized) {
        srand(time(NULL));
        initialized = true;
    }

    for (x=0;x<SIZE;x++)
        for (y=0;y<SIZE;y++)
            if (board[x][y]==0) {
                list[len][0]=x;
                list[len][1]=y;
                len++;
            }
    if (len>0) {
        r = rand()%len;
        x = list[r][0];
        y = list[r][1];
        n = (rand()%10)/9+1;
        board[x][y]=n;
    }
}

long stat_time[16];
long stat_count[16];
void stat(int depth, long time) {
    stat_count[depth]++;
    stat_time[depth] += time;
}

#define movefunc(src_cell, combine_cell, nocombine_cell)\
{\
    int i, j = 0;\
    int moved = 0;\
    int score = 0;\
    for (i = 0; i < SIZE; i++) {\
        int last = 0;\
        int j2 = 0;\
        for (j = 0; j < SIZE; j++) {\
            int v = src_cell;\
            if (v == 0) {\
                continue;\
            }\
            if (v == last) {\
                last = 0;\
                combine_cell = v + 1;\
                score += value_real[v + 1];\
            } else {\
                if (j2 < j)\
                    moved = 1;\
                last = v;\
                nocombine_cell = v;\
                j2++;\
            }\
        }\
    }\
    return score ? score : moved;\
}

#define REVERSE(i) (SIZE - 1 - (i))

int move_left(board_t src, board_t dst) {
    movefunc(src[i][j], dst[i][j2 - 1], dst[i][j2])
}


int move_right(board_t src, board_t dst) {
    movefunc(src[i][REVERSE(j)], dst[i][REVERSE(j2 - 1)], dst[i][REVERSE(j2)])
}

int move_up(board_t src, board_t dst) {
    movefunc(src[j][i], dst[j2 - 1][i], dst[j2][i])
}

int move_down(board_t src, board_t dst) {
    movefunc(src[REVERSE(j)][i], dst[REVERSE(j2 - 1)][i], dst[REVERSE(j2)][i])
}

double value(board_t b, int depth, int *choice, double max);

int imm_value(board_t b) {
    int i, j;
    int result = 0;
    for (i = 0; i < SIZE; i++) for (j = 0; j < SIZE; j++) result += value_weight[b[i][j]] * cell_weight[i][j];
    return result;
}

#define HASH_SIZE (35317)
struct hash_elem vcache[HASH_SIZE];
void cache_board_value(board_t b, int depth, double value) {
    int hash = board_hash(b);
    int index = hash % HASH_SIZE;
    vcache[index].hash = hash;
    vcache[index].value = value;
    vcache[index].depth = depth;
}

int qcnt;
int qmiss;
double query_board_value(board_t b, int depth) {
    int hash = board_hash(b);
    int index = hash % HASH_SIZE;
    qcnt++;
    if (vcache[index].hash == hash && vcache[index].depth >= depth) return vcache[index].value;
    qmiss++;
    return -1;
}

/* Generate 2/4 at every posible position, return the average value score
 * b        : the board
 * depth    : depth of the recursive search
 * max      : current maximum value score
 * sampled  : sample rate, 0 means no sample
 */
double rnd_value(board_t b, int depth, double max, int sampled) {
    int i, j;
    int cnt = 0;
    double sum = 0;
    static int scnt = 0;
    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            if (b[i][j])continue;
            if (sampled) {
                scnt++;
                if(scnt % sampled)continue;
            }
            cnt += 9;
            b[i][j] = 1;
            sum += 9 * value(b, depth, NULL, max);
            if (depth >= 5) {
                cnt += 1;
                b[i][j] = 2;
                sum += value(b, depth, NULL, max);
            }
            b[i][j] = 0;
        }
    }
    return sum / cnt;
}

double value(board_t b, int depth, int *choice, double max) {
    int estimate = imm_value(b);
    if (estimate < max * 0.7)depth--;
    if (depth <= 0)
        return estimate;
    int next_depth = depth - 1;
    if (depth > 3) {
        int zeros = board_count_zero(b);
        if (next_depth > depth_map[zeros])
            next_depth--;
    }

    int i;
    int moved[4];
    double maxv = 0;
    board_t tmp[4] = {0};
    int my_choice = QUIT;
    if (!choice) {
        double v = query_board_value(b, depth);
        if (v >= 0)return v;
    }
    moved[LEFT] = move_left(b, tmp[LEFT]);
    moved[RIGHT] = move_right(b, tmp[RIGHT]);
    moved[UP] = move_up(b, tmp[UP]);
    moved[DOWN] = move_down(b, tmp[DOWN]);
    if (depth > 2)
        for (i = 0; i < 4; i++) {
            int v = imm_value(tmp[0]);
            max = v > max ? v : max;
        }
    for (i = 0; i < 4; i++) {
        if (!moved[i])continue;
        int sample = 0;
        double v = rnd_value(tmp[i], next_depth, max, sample);
        if (v > maxv) {
            my_choice = i;
            maxv = v;
            max = maxv;
        }
    }
    if (choice)*choice = my_choice;
    cache_board_value(b, depth, maxv);
    return maxv;
}

static int get_AI_input(board_t b) {
    int choice;
    int zeros = board_count_zero(b);

    long start = gettime();
    value(b, depth_map[zeros], &choice, 0);
    long timeval = gettime() - start;
    stat(depth_map[zeros], timeval);
    return choice;
}

static int get_keyboard_input() {
    char c;
    while(1){
        c = (char)getchar();
        switch(c) {
            case 'w': return UP;
            case 'a': return LEFT;
            case 's': return DOWN;
            case 'd': return RIGHT;
            default : return QUIT;
        }
    }
}

int auto_play = 0;
int suggestion = 0;

void setBufferedInput(bool enable) {
    static bool enabled = true;
    static struct termios old;
    struct termios new;

    if (enable && !enabled) {
        tcsetattr(STDIN_FILENO,TCSANOW,&old);
        enabled = true;
    } else if (!enable && enabled) {
        tcgetattr(STDIN_FILENO,&new);
        old = new;
        new.c_lflag &=(~ICANON & ~ECHO);
        tcsetattr(STDIN_FILENO,TCSANOW,&new);
        enabled = false;
    }
}

void signal_callback_handler(int signum) {
    printf("         TERMINATED         \n");
    setBufferedInput(true);
    printf("\033[?25h\033[m");
    exit(signum);
}

int main(int argc, char *argv[]) {
    switch (argv[1][1]) {
        case 'a':
            auto_play = 1;
            break;
        case 's':
            suggestion = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-a] [-s]\r\n", argv[0]);
            fprintf(stderr, "-a:  Let AI play the game\r\n");
            fprintf(stderr, "-s:  Display AI suggestion\r\n");
            exit(EXIT_FAILURE);
    }
    init();
    srandom(time(NULL));
    printf("\033[?25l\033[2J");
    signal(SIGINT, signal_callback_handler);
    board_t a = {0};
    board_t b = {0};
    board_t *cur;
    board_t *next;

    int input;
    int AI_input = -1;

    cur = &a;
    next = &b;

    addRandom(*cur);
    addRandom(*cur);
    setBufferedInput(false);
    while (1) {
        if (auto_play || suggestion) AI_input = get_AI_input(*cur);
        drawBoard(*cur, AI_input + 1);
        if (auto_play)input = AI_input;
        else input = get_keyboard_input();
        int moved = 0;
        bool succ = true;
        switch(input) {
            case UP:
                moved = move_up(*cur, *next);
                break;
            case LEFT:
                moved = move_left(*cur, *next);
                break;
            case DOWN:
                moved = move_down(*cur, *next);
                break;
            case RIGHT:
                moved = move_right(*cur, *next);
                break;
            default:
                succ = false;
                break;
        }
        if(!succ)break;
        if (!moved)continue;
        if (moved != 1)sc += moved;

        drawBoard(*next, AI_input + 1);
        usleep(150000);
        addRandom(*next);

        board_t *temp = cur;
        cur = next;
        next = temp;
        board_clear(*next);
    }
    setBufferedInput(true);
    printf("\033[?25h\033[m");
    return EXIT_SUCCESS;
}
