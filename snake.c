#include <stdio.h>
#include <term.h>
#include <curses.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#define TERM_SUCCESS      1               // 成功码
#define MAX_LEVEL         10              // 最大等级
#define MIN_WIDTH         75              // 最小宽度
#define MIN_HEIGHT        30              // 最小高度
#define MIN_SPEED         1               // 最小宽度
#define MAX_SPEED         5               // 最小高度
#define M_LOOP            10L             // ticker毫秒数 10ms
#define WINDOW_WIDTH      windowWidth     // 窗口宽
#define WINDOW_HEIGHT     windowHeight    // 窗口高
#define WINDOW_PADDING_X  windowPaddingX  // 边距X
#define WINDOW_PADDING_Y  windowPaddingY  // 边距Y
#define WINDOW_BOLD_CHAR  '@'             // 边框字符
#define SNAKE_INIT_SPEED  1               // 蛇的初始速度
#define SNAKE_INIT_LEN    10              // 蛇的初始速度
#define SNAKE_BODY_CH     '*'             // 蛇身字符
// 方向
#define DIR_LEFT           1
#define DIR_UP             2
#define DIR_DOWN           3
#define DIR_RIGHT          4
#define DIR_CHECK_SUM      5
#define DIR_MAX_QUEUE_SIZE 3

// 定义终端错误码
#define EXIT_ERROR_TERM_GET   10
#define EXIT_ERROR_TERM_SIZE  11
// 宏定义函数
#define min(A, B)    ((A) < (B) ? (A) : (B))
#define keepAlive()  while(TRUE) usleep(100)

// 函数定义
void termSize(int *, int *);   // 获取终端尺寸
void signalReg();              // 信号处理
void signalHandle(int);        // 信号处理
void init();                   // 初始化
void colorDefine();            // 颜色处理
void drawWindow();             // 绘制窗体
void drawPanel();              // 绘制面板
void initSnake();              // 初始化蛇身
void rePaintSnake();           // 重绘蛇身
int isWall(int, int);          // 检查坐标是否墙体
void updateTime();             // 更新时间
void snakeMoveOneStep();       // 蛇移动一个MOVE
void setSpeed(int);            // 设置速度
void setTicker(long);          // 设置定时器
void ticker();                 // 一个ticker的操作
void readCh();                 // 读取输入作出响应
void setDirection(int);        // 设置蛇下一次移动方向
void swap(int *, int*);        // 交换
void end(int);                 // 结束
// 队列函数
int isQueFull();               // 队列是否已满
int isQueEmpty();              // 队列是否已空
int enqueue(int);              // 入队
int dequeue();                 // 出队


// 窗体
int windowWidth  = 100; // 窗体宽度
int windowHeight = 40; // 窗体高度

int windowPaddingX,windowPaddingY;

// 计分板
char *playerName = "anonymous"; // 玩家昵称
short level      = 1;  // 当前速度等级
int score        = 0;  // 分数
int gap; // 展示间隙

// 障碍物


// 蛇
//  每个ticker蛇执行一步操作，速度由ticker的时间间隔决定，时间间隔越小，速度越快
//  需要一个队列记录操作，蛇身的每个点记录自己上一次执行到的操作
//  吃到果实变长时插入头结点，记录最后一个操作指针为当前链表的顶端
//  若当前元素为操作链表的头节点，按照当前行进方向(头指针方向)插入一个操作，行进并更新最后一次操作指针
//  触碰检测
//  键盘监听
int speed,tickerBlock;        // 移动速度 1~5 表示 (100 / speed)个ticker
int moveTimer = 0;            // 记录当前经过了多少个ticker，达到(100 / speed)个ticker后清零重计
int curDirection = DIR_RIGHT;
int isStart = FALSE;

// 方向队列，最多存储MAX_SIZE个操作
int dirQueue[DIR_MAX_QUEUE_SIZE];
int qHead = DIR_MAX_QUEUE_SIZE;
int qTail = DIR_MAX_QUEUE_SIZE;
int qSize = 0;

struct Snake { // 定义蛇身链表结构体
    int posX;           // X坐标
    int posY;           // Y坐标
    struct Snake *next; // 前节点
} *snake;

// 果实
// @TODO 出现位置 除蛇身和墙体外的随机位置


// 入口
int main()
{
    init();

    signalReg();

    drawWindow();

    drawPanel();

    initSnake();

    setTicker(M_LOOP);

    //keepAlive();
    readCh();

    end(0);

    return TERM_SUCCESS;
}



void termSize(int *width, int *height)
{
    int errno;
    setupterm(NULL, fileno(stdout), &errno);

    if (TERM_SUCCESS != errno) {
        printf("初始化控制台参数失败，程序终止\n");
        exit(EXIT_ERROR_TERM_GET);
    }

    *width  = (int)tigetnum("cols");
    *height = (int)tigetnum("lines");
}

void signalHandle(int sigNum)
{
    switch (sigNum) {
        case SIGINT:  end(sigNum); break;
        case SIGALRM: ticker(); break;
        default: return;
    }
}

void signalReg()
{
    struct sigaction quitNewAct,quitOldAct;
    struct sigaction alrmNewAct,alrmOldAct;

    quitNewAct.sa_handler = signalHandle;
    quitNewAct.sa_flags = SA_NODEFER;
    sigaddset(&quitNewAct.sa_mask,SIGALRM);

    alrmNewAct.sa_handler = signalHandle;
    alrmNewAct.sa_flags = SA_NODEFER;

    // Ctrl + C 信号
    sigaction(SIGINT, &quitNewAct, &quitOldAct);

    // Ticker 信号
    sigaction(SIGALRM, &alrmNewAct, &alrmOldAct);
}

void setTicker(long nMsecs)
{
    struct itimerval itv;
    long nSec, nUsec;

    nSec  = nMsecs / 1000;
    nUsec = (nMsecs % 1000) * 1000L;

    itv.it_interval.tv_sec  = nSec;
    itv.it_interval.tv_usec = nUsec;
    itv.it_value.tv_sec     = nSec;
    itv.it_value.tv_usec    = nUsec;

    // 设置定时器
    setitimer(ITIMER_REAL, &itv, NULL);
}

void init()
{
    // 获取终端宽度和高度
    int termWidth, termHeight;
    termSize(&termWidth, &termHeight);

    WINDOW_WIDTH  = min(windowWidth, termWidth); // 窗体宽度
    WINDOW_HEIGHT = min(windowHeight, termHeight); // 窗体高度

    // 小于定义的最小值，报错
    if (WINDOW_WIDTH < MIN_WIDTH || WINDOW_HEIGHT < MIN_HEIGHT) {
        printf("当前终端尺寸 %dx%d，不符合要求尺寸 %dx%d, 请调整后重试\n", WINDOW_WIDTH, WINDOW_HEIGHT, MIN_WIDTH, MIN_HEIGHT);
        exit(EXIT_ERROR_TERM_SIZE);
    }

    WINDOW_PADDING_X = (termWidth - WINDOW_WIDTH) / 2; // 边距 X
    WINDOW_PADDING_Y = (termHeight - WINDOW_HEIGHT) / 2; // 边距 X

    // 计算面板显示间隙
    gap = (WINDOW_WIDTH - 41) / 2;

    system("clear");
    // 初始化curses
    initscr();
    // 清屏
    clear();
    // 允许键盘映射
    keypad(stdscr, TRUE);
    // 关闭键盘输入回显
    (void)noecho();
    // RETURN 不当做 NEW_LINE
    (void)nonl();
    // 键盘输入直接传给程序
    (void)cbreak();
    // 隐藏光标
    curs_set(0);
    // 关闭特殊字符 这里若定义，则会屏蔽^C信号
    //(void)raw();
    // 颜色定义
    colorDefine();
}

void colorDefine()
{

    if (has_colors()) {
        start_color();

        // 颜色映射
    }
}

void drawWindow()
{
    int i;
    // 申请内存
    char *line = malloc(WINDOW_WIDTH + 1);

    for (i = 0; i < WINDOW_WIDTH; i ++) {
        if ((0 == i) || ((WINDOW_WIDTH - 1) == i)) {
            *(line + i) = '#';
        } else {
            *(line + i) = WINDOW_BOLD_CHAR;
        }
    }
    *(line + i) = '\0';

    // 绘制上横线
    move(WINDOW_PADDING_Y, WINDOW_PADDING_X);
    addstr(line);

    // 绘制侧边框
    int yReachable = WINDOW_HEIGHT + WINDOW_PADDING_Y - 1;
    for (i = WINDOW_PADDING_Y + 1; i < yReachable; i ++) {
        move(i, WINDOW_PADDING_X);
        addch(WINDOW_BOLD_CHAR);

        move(i, WINDOW_WIDTH + WINDOW_PADDING_X - 1);
        addch(WINDOW_BOLD_CHAR);
    }

    // 绘制面板分割线
    move(WINDOW_PADDING_Y + 2, WINDOW_PADDING_X);
    addstr(line);

    // 绘制下横线
    move(WINDOW_HEIGHT + WINDOW_PADDING_Y - 1, WINDOW_PADDING_X);
    addstr(line);

    // 释放内存占用
    free(line);

    refresh();
}

void initSnake()
{
    // 初始化头结点
    snake = (struct Snake *) malloc(sizeof(struct Snake));

    struct Snake *cursor = snake;
    // 初始化速度
    setSpeed(SNAKE_INIT_SPEED);

    // 计算蛇头的初始位置
    cursor->posX = WINDOW_PADDING_X + SNAKE_INIT_LEN + 1;
    cursor->posY = WINDOW_PADDING_Y + 4;

    move(cursor->posY, cursor->posX);
    addch(SNAKE_BODY_CH);

    // 创建蛇身
    for (int i = 1; i <= (SNAKE_INIT_LEN - 1); i ++) {
        struct Snake *body = (struct Snake *)malloc(sizeof(struct Snake));

        body->posX   = cursor->posX - 1;
        body->posY   = cursor->posY;
        cursor->next = body;

        if ((SNAKE_INIT_LEN - 1) == i) {
            body->next = NULL;
        }

        move(body->posY, body->posX);
        addch(SNAKE_BODY_CH);

        cursor = body;
    }

    refresh();
}

void rePaintSnake()
{
    // 初始化方向
    curDirection = DIR_RIGHT;
    // 停止
    isStart = FALSE;

    // 释放当前蛇身占用内存
    struct Snake *next = snake->next;
    while (NULL != snake) {
        move(snake->posY, snake->posX);
        // 清空当前位置的像素点
        addch(' ');

        free(snake);

        snake = next;
        if (NULL != snake) {
            next  = snake->next;
        }
    }

    initSnake();
}

int isWall(int x, int y)
{
    int xMax  = WINDOW_WIDTH + WINDOW_PADDING_X - 1;
    int yMax = WINDOW_HEIGHT + WINDOW_PADDING_Y - 1;

    // 顶
    if (((WINDOW_PADDING_Y + 2) == y) && (x >= WINDOW_PADDING_X) && (x <= xMax)) {
        return TRUE;
    }

    // 左侧
    if ((WINDOW_PADDING_X == x) && (y >= (WINDOW_PADDING_Y + 2)) && (y <= yMax)) {
        return TRUE;
    }

    // 右侧
    if ((xMax == x) && (y >= (WINDOW_PADDING_Y + 2)) && (y <= yMax)) {
        return TRUE;
    }

    // 底
    if ((yMax == y) && (x >= WINDOW_PADDING_X) && (x <= xMax)) {
        return TRUE;
    }

    return FALSE;
}

void ticker()
{
    int qDir;

    updateTime();

    // 指定时间间隔移动
    if (isStart) {
        moveTimer = (moveTimer + 1) % tickerBlock;
        if (0 == moveTimer) {
            // 若队列不为空，取队列头元素作为下一次方向
            qDir = dequeue();
            if (qDir) {
                setDirection(qDir);
            }
            snakeMoveOneStep();

            score ++;

            drawPanel();
        }
    } else {
        moveTimer = 0;
    }

}

void updateTime()
{
    time_t curtime;

    time(&curtime);

    move(0,0);
    addstr(ctime(&curtime));

    refresh();
}

void drawPanel()
{
    move((WINDOW_PADDING_Y + 1), (WINDOW_PADDING_X + 2));
    printw("player: %s", playerName);

    move((WINDOW_PADDING_Y + 1), (WINDOW_PADDING_X + 2 + 18 + gap));
    printw("level: %d", level);

    move((WINDOW_PADDING_Y + 1), (WINDOW_PADDING_X + 2 + 18 + 2 * gap + 8));
    printw("score: %04d", score);
}

void snakeMoveOneStep()
{
    int prePosX,prePosY,newPosX,newPosY;
    int isBodyLoop = FALSE;
    struct Snake *cursor = snake;

    prePosX = cursor->posX;
    prePosY = cursor->posY;

    newPosX = cursor->posX;
    newPosY = cursor->posY;

    switch (curDirection) {
        case DIR_LEFT:
            newPosX -= 1;
            break;
        case DIR_RIGHT:
            newPosX += 1;
            break;
        case DIR_UP:
            newPosY -= 1;
            break;
        case DIR_DOWN:
            newPosY += 1;
            break;
        default:
            return ;
    }

    // 碰撞检测
    if (isWall(newPosX, newPosY)) {
        // 重绘蛇身
        rePaintSnake();
        return ;
    }

    cursor->posX = newPosX;
    cursor->posY = newPosY;

    move(cursor->posY, cursor->posX);
    addch(SNAKE_BODY_CH);

    cursor = cursor->next;

    while (NULL != cursor) {
        // 清空原来的位置
        move(cursor->posY, cursor->posX);
        addch(' ');

        // 计算新位置
        swap(&cursor->posX, &prePosX);
        swap(&cursor->posY, &prePosY);

        move(cursor->posY, cursor->posX);
        addch(SNAKE_BODY_CH);

        if ((newPosX == cursor->posX) && (newPosY == cursor->posY)) {
            isBodyLoop = TRUE;
        }

        cursor = cursor->next;
    }

    refresh();

    // 咬到自己的身体，触发蛇身重绘
    if (isBodyLoop) {
        rePaintSnake();
    }
}

void setSpeed(int newSpeed)
{
    if ((newSpeed > MAX_SPEED) || (newSpeed < MIN_SPEED)) {
        return ;
    }

    speed       = newSpeed;
    tickerBlock = 50 / newSpeed;
}

void end(int signum)
{
    if (0 != signum) {
        move(0, 0);
        addstr("Quit Game, ByeBye~~                  ");
        refresh();
        sleep(1);
    }

    // 移除定时器
    setitimer(ITIMER_REAL, NULL, NULL);

    // 正常应该Free掉Snake内存占用，由于链表在整个程序的生命周期有效，这里省略Free

    endwin();

    system("stty echo");
    system("clear");

    exit(TERM_SUCCESS);
}

void swap(int * num1, int* num2)
{
    int tmp = *num1;

    *num1 = *num2;
    *num2 = tmp;
}


void readCh()
{
    while(TRUE)
    {
        int c = getch();

        if ('q' == c) {
            // 手动触发信号SIGINT
            kill(getpid(), SIGINT);
            continue;
        }

        // 按下任意键开始...
        if (c > 0 && !isStart) {
            isStart = TRUE;
            continue;
        }

        int res;
        switch (c) {
            // 上
            case 'w':
            case KEY_UP:
                res = enqueue(DIR_UP);
                break;
            // 下
            case 's':
            case KEY_DOWN:
                res = enqueue(DIR_DOWN);
                break;
            // 左
            case 'a':
            case KEY_LEFT:
                res = enqueue(DIR_LEFT);
                break;
            // 右
            case 'd':
            case KEY_RIGHT:
                res = enqueue(DIR_RIGHT);
                break;
            // 暂停
            case 'p':
                isStart = FALSE;
                break;
            default:
                continue ;
        }
    }
}

void setDirection(int direction)
{
    // 禁止设置同方向
    if ((direction + curDirection) != DIR_CHECK_SUM) {
        curDirection = direction;
    }
}



// 队列函数
int enqueue(int item)
{
    if (isQueFull()) {
        // 判断队列是否已满
        return 0;
    }

    qTail = (qTail + 1) % DIR_MAX_QUEUE_SIZE;
    dirQueue[qTail] = item;
    qSize ++;
    return item;
}

int dequeue()
{
    if (isQueEmpty()) {
        // 判断队列是否已空
        return 0;
    }

    qHead = (qHead + 1) % DIR_MAX_QUEUE_SIZE;
    qSize --;
    return dirQueue[qHead];
}

int isQueFull()
{
    if (DIR_MAX_QUEUE_SIZE == qSize) {
        return TRUE;
    }

    return FALSE;
}

int isQueEmpty()
{
    if (0 == qSize) {
        return TRUE;
    }

    return FALSE;
}
