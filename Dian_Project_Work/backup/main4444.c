#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

// 多线程相关
#include <threads.h> 
// 获取终端输入相关
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>

#include "video_decoder.h"

#define LL unsigned long long int
#define uint8 unsigned char
#define string char*    // 字符串

#define COMMAND_STACK_MAX 100  // 读取一个形如 -r 3 8 指令的参数长度的最大长度（示例中长度为3）
#define true 1
#define false 0
#define second *1959170     // clock（）一秒的初略时间度量
#define THRD_NUM 200        // 线程最大数量
#define BUFFER_MAX 300      // 帧缓存最大上限
#define THRD_PRINT 0        // 专门负责打印的线程
#define THRD_GETKEY 90      // 专门负责获取终端输入的线程

#define GRAY_HANDLE 1       // 灰度图处理
#define MAX_POOLING 2       // 堆积池：最大池化
#define AVERAGE_POOLING 3   // 堆积池：平均池化



#if 0           // 下面是对应指令，可直接使用输入终端来执行代码
gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 -r -f bad_apple.mp4 -r 3 8 -c m
                                                    // 彩图打印

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 -r -f bad_apple.mp4 -r 3 8 -c g -c g p -fps 15.0 
                                                    // 字符图打印

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 --file "dragon.mp4" -r 5 10 -c a g

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 --file "dragon.mp4" -r 4 8 -c g p
g
#endif


#if 1  // 数据结构部分
struct{         // 接受命令行参数的执行容器栈  一次至多存储即处理一条以'-'开头的指令
    string s[COMMAND_STACK_MAX];    // 命令栈
    int len;                        
}command_stack;

typedef struct{     // 颜色值
    unsigned char R,G,B;
}color;

string punct_collection = "   ...:::---===+++\"\"^^**##@@"; //10    // 字符图打印模式下灰度对应的字符
int punct_collection_length = 26;


char *play_file_path = NULL;    // 播放视频的路径

clock_t last_time = 0;          // 上一次更新视频帧的时刻
clock_t current_time = 0;       // 当前时刻（以clock（）记录为准）
double video_play_fps = 10.0;        // 视频播放帧率（1s内）
int skip_frame = 2;             // 视频播放帧时每次省略的帧数（而不必每帧都打印）（可以减少视频闪的程度）

int frame_width = 0, frame_height = 0, frame_linesize = 0, total_frame = 0;     // 源视频帧宽高，data一行元素数，总帧
int new_frame_width = 0, new_frame_height = 0, new_frame_linesize = 0, new_total_frame = 0; // 修改尺寸后的视频帧宽高，data一行元素数，总帧
int cur_pos = 0;            // 当前位置
double frame_fps = 5.0;       // 原视频帧率

int pixel_width = 5,pixel_height = 10;      // 修改尺寸后一个像素格在原视频中对应多长的宽高的值      
                                            // 参考：bad apple 3 8   // dragon 5 10
                                            // 由于终端打印的字体高宽比是12 ： 6,所以pixel_width ：pixel_height尽量维持在这个范围附近

color color0;           // 颜色值记录，打印像素格时使用
int pooling = MAX_POOLING;      // 堆积池使用方式
int is_gray = true;             // 是否灰度图打印
int is_punct = false;           // 是否字符图打印

thrd_t threads[THRD_NUM];               // 线程数组
int threads_busy[THRD_NUM] = {0};       // 对应线程是否繁忙


typedef struct{     // 帧缓冲保存
    uint8 *buffer;  // 帧像素数据
    int is_done;    // 对应帧是否
    int index;      // 帧的序列
}Buffer_saving;
Buffer_saving buffer_save[BUFFER_MAX + 10] = {0};       // 帧缓冲保存（打印帧时使用该容器中缓冲进行打印）

int buffer_cur_collect[100] = {0};      // 对应线程当前处理的帧序列
int buffer_cur_print = 0;               // 当前打印的帧
int buffer_update_velocity = 3;         // 一次进行帧缓冲储存处理的线程数量


typedef struct{     // 执行多线程保存帧缓冲时的传参容器
    int cur_thrd_index;         // 当前处理帧缓冲保存的线程索引
    Frame frame;            // 帧数据
    int handle_frame_index; // 当前处理的帧的序列
}Collect_data;


static struct termios ori_attr, cur_attr;   //获取终端输入相关 
int tty_set_flag;          
char input_char = 0;    // 终端输入的字符
int key_status = 0;     // 当前按下的按键
int is_play_paused = false; // 是否播放停止

#endif


#if 1   // 函数部分
void command_stack_handle();        // 处理指令栈中的所有参数

int frame_cal(int row,int colmn);       // 计算原视频中某行某列的像素格在data中对应的下标（会返回R对应位置）

int gray_cal(color *c);         // 灰度值计算

void frame_show(Frame *f);          // 打印一帧

color resize_max_pooling(Frame *f,int w,int h,int row,int colmn);      // 最大堆积池算法

color resize_average_pooling(Frame *f,int w,int h,int row,int colmn);  // 平均堆积池算法

int frame_buffer_collect(void *ptr);        // 帧缓存收集

int frame_buffer_print(void *ptr);          // 帧缓存打印


static __inline int tty_reset(void);     //    

static __inline int tty_set(void);      // 别人的轮子，原理还没搞清楚

static __inline int kbhit(void);        // 

int get_key_console(void *ptr);         // 从终端获取用户输入

void key_state_handle(int key);         // 对用户输入进行处理
#endif


int main(int argc,char *argv[]){        
    printf("arguments : %d\n",argc);    
    puts("The arguments are :");        // 打印命令行参数
    for (int i = 0;i < argc;i ++) printf("%d: %s%6s",i,argv[i]," ");
    puts("\n");

    for (int i = 1;i < argc;i ++){      // 将命令行参数一一加入指令栈中
        if (argv[i][0] == '-'){     // 遇到'-'开头指令，则处理指令栈中已有参数，后清空，接着加入新指令 eg&: --resize -file
            command_stack_handle();
            command_stack.s[command_stack.len ++] = argv[i];    // 记录长度与指令
        }
        else command_stack.s[command_stack.len ++] = argv[i];   // 否则直接将参数加入指令栈 eg&: 36 "ooo"
        if (i == argc - 1) command_stack_handle();
    }
    puts("proceed\033[1;31m Hello \x1b[0mworld!!!\n");


    Frame f = {0,0,0,NULL};         // 帧的记录
    if (play_file_path != NULL) decoder_init(play_file_path);      
    else decoder_init("bad_apple.mp4");     // 解码对应视频
    //decoder_init("dragon.mp4");

    f = decoder_get_frame();
    frame_width = f.width, frame_height = f.height;     // 初始化视频数据
    total_frame = get_total_frames();
    frame_fps = get_fps();
    frame_linesize = f.linesize;
    printf("%.4f   %d     %d\n",get_fps(),get_frame_index(),get_total_frames());
    printf("%d  %d  %d\n",f.width,f.height,f.linesize);
    frame_show(&f);  // 打印一帧

    tty_set_flag = tty_set();   // 设置终端输入相关
    

#if 0
          // 下面是对应指令，可直接使用来执行代码
gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 -r -f bad_apple.mp4 -r 3 8 -c g -c g p -fps 15.0

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 --file "dragon.mp4" -r 5 10 -c a g

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 --file "dragon.mp4" -r 4 8 -c g p
g
#endif

#if 1
    while (true){
        current_time = clock();
        
        if (current_time > last_time + (int)(1.0 second / video_play_fps)){  // 控制帧率
            last_time = current_time;                       // 更新时间
            if (is_play_paused == false) printf("\x1b[H\x1b[2J");                        //清屏

            // 多线程储存帧缓冲（存储速率远远大于打印速率）
            for (int i = 1;i <= buffer_update_velocity && get_frame_index() <= get_total_frames() - 5 ;i ++){
                if (threads_busy[i] == false){  // 繁忙中的线程跳过
                    f = decoder_get_frame();                        // 解码一帧，获取对应数据
                    threads_busy[i] = true;     // 对应线程置繁忙状态，防止重复调用相同线程
                    Collect_data collect_data = {i,f,get_frame_index()};    // 传参
                    thrd_create(&threads[i],frame_buffer_collect,&collect_data);   // 创建线程工作 
                    for (int i = 0;i < skip_frame;i ++) f = decoder_get_frame();  // 略读，忽略一些帧
                }
            }

            //frame_show(&f);
            if (is_play_paused == false && 1) { // 打印帧缓冲
                printf("current index: %d / %d\t",buffer_save[buffer_cur_print - 1].index,total_frame);
                printf("time: %d  (%.2fs)\t",(int)current_time,1.0 * current_time / (1 second));
                printf("Video play speed: %.2f\tSource fps equal:%.2f\n",video_play_fps,video_play_fps * (1 + skip_frame));
                thrd_create(&threads[THRD_PRINT],frame_buffer_print,NULL);  // 创建打印线程
                thrd_join(threads[THRD_PRINT],NULL); // 打印线程结束后才继续执行之后语句
                printf("current index: %d / %d\t",buffer_save[buffer_cur_print - 1].index,total_frame);
                printf("time: %d  (%.2fs)\t",(int)current_time,1.0 * current_time / (1 second));
                printf("Video play speed: %.2f\tSource fps equal:%.2f\n",video_play_fps,video_play_fps * (1 + skip_frame));
            }
        }

        if (threads_busy[THRD_GETKEY] == 0){    // 启动获取用户输入相关的线程
            threads_busy[THRD_GETKEY] = 1;
            thrd_create(&threads[THRD_GETKEY],get_key_console,NULL);
        }
        key_state_handle(key_status);   // 处理终端用户输入
        key_status = 0;
        

#if 1   // 终止条件判定
        if (buffer_save[buffer_cur_print - 1].index >= get_total_frames() - 10) {     // 视频播放完成，退出
            printf("The video has been completely played.\n");
            break;
        }
        if (current_time >= 300 second) {        // 时间限制
            printf("Time over limit.\n");
            break;
        }
        if ( buffer_save[buffer_cur_print - 1].index >= 30000) {        // 帧限制
            printf("Frame over limit.\n");
            break;
        }
#endif
    }
    
#endif
    tty_reset();    // 重置终端输入相关设置
    puts("finish\n");   // 完成视频播放
    return 0;
}


#if 0           // 下面是对应指令，可直接使用输入终端来执行代码
gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 -r -f bad_apple.mp4 -r 3 8 -c m
                                                    // 彩图打印

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 -r -f bad_apple.mp4 -r 3 8 -c g -c g p -fps 15.0 
                                                    // 字符图打印

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 --file "dragon.mp4" -r 5 10 -c a g

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 --file "dragon.mp4" -r 4 8 -c g p
g
#endif



void command_stack_handle(){            // 处理指令栈中的所有参数
    if (command_stack.len == 0) return;
    if (command_stack.s[0][0] != '-') printf("Invalid command was received.\n");    // 参数不合法
    else {
        printf("command< ");
        for (int i = 0;i < command_stack.len;i ++) printf("%s ",command_stack.s[i]);
        printf(">:\t");                 // 打印对应命令，便于查看效果

        if (strcmp(command_stack.s[0],"-h") == 0 || strcmp(command_stack.s[0],"--help") == 0){
            printf("\thelp [-h] [--help]\n");
            printf("\t\t\tversion [-v] [--version]\n");
            printf("\t\t\tcolor [-c] [--color] g|p|m|a [g|p|m|a]\n");
            printf("\t\t\tresize [-r] [--resize] int [int]\n");
            printf("\t\t\tsetfilepath [-f] [--file] path_name\n");
            printf("\t\t\tsetfps [-fps] [--fps] float|int\n");
            if (command_stack.len >= 2) printf("Unkown arugment was passed after -h or --help\n");  // 参数过多
        }
        else if (strcmp(command_stack.s[0],"-v") == 0 || strcmp(command_stack.s[0],"--version") == 0){
            printf("\"dian-player v1.2\"");
            if (command_stack.len >= 2) printf("\tUnkown arugment was passed after -v or --version");  // 参数过多
            puts("");
        }
        
        else if (strcmp(command_stack.s[0],"-c") == 0 || strcmp(command_stack.s[0],"--color") == 0){
            if (command_stack.len >= 1) {
                pooling = AVERAGE_POOLING;
                is_gray = false;
            }
            else if (command_stack.len >= 2 && strcmp(command_stack.s[1],"g") == 0) is_gray = true;
            else if (command_stack.len >= 3 && strcmp(command_stack.s[1],"p") == 0) is_punct = true;
            else if (command_stack.len >= 2 && strcmp(command_stack.s[1],"m") == 0) pooling = MAX_POOLING;
            else if (command_stack.len >= 2 && strcmp(command_stack.s[1],"a") == 0) pooling = AVERAGE_POOLING;
            if (command_stack.len >= 3 && strcmp(command_stack.s[2],"g") == 0)      is_gray = true;
            else if (command_stack.len >= 3 && strcmp(command_stack.s[2],"p") == 0) is_punct = true;
            else if (command_stack.len >= 3 && strcmp(command_stack.s[2],"m") == 0) pooling = MAX_POOLING;
            else if (command_stack.len >= 3 && strcmp(command_stack.s[2],"a") == 0) pooling = AVERAGE_POOLING;

            if (is_punct) is_gray = true;
            if (!is_gray) is_punct = false;     
            printf("The modified video print mode:\tpooling--:");
            if (pooling == MAX_POOLING) printf("MAX  "); 
            else printf("AVERAGE  ");
            if (is_punct) printf("Punct");
            else if (is_gray) printf("Gray");
            puts("");
        }

        else if (strcmp(command_stack.s[0],"-r") == 0 || strcmp(command_stack.s[0],"--resize") == 0){
            if (command_stack.len == 1) 
                printf("No argument after -r or --resize.");
            else if (command_stack.len == 2){
                pixel_width = (int)strtol(command_stack.s[1],NULL,0);
                pixel_height = 10;
                printf("Resize successfully.\tpixel width: %d  The pixel_height has been set defaultly : 10.",pixel_width);
            }
            else if (command_stack.len >= 3) {
                pixel_width = (int)strtol(command_stack.s[1],NULL,0),
                pixel_height = (int)strtol(command_stack.s[2],NULL,0);
                printf("Resize successfully.\tpixel width: %d  pixel height: %d.",pixel_width,pixel_height);
                //printf("w:%d  h:%d\n",(int)strtol(command_stack.s[1],NULL,0),(int)strtol(command_stack.s[2],NULL,0));
            }
            else if (command_stack.len >= 4) 
                printf("\tSurplus arguments (more than three) has been input after -r or --resize.");  // 参数过多
            // bad apple 3 8   // dragon 5 10
            puts("");
        }

        else if (strcmp(command_stack.s[0],"-f") == 0 || strcmp(command_stack.s[0],"--file") == 0){
            if (command_stack.len == 1) 
                printf("No argument after -f or --file.");    // 参数缺失
            if (command_stack.len >= 2){
                play_file_path = command_stack.s[1];
                printf("Path assigned.");
            }
            if (command_stack.len >= 3) 
                printf("\tSurplus arguments (more than three) has been input after -f or --file."); // 参数过多
            puts("");
        }

        else if (strcmp(command_stack.s[0],"-fps") == 0 || strcmp(command_stack.s[0],"--fps") == 0){
            if (command_stack.len == 1) 
                printf("No argument after -fps or --fps.");    // 参数缺失
            if (command_stack.len >= 2){
                double fps1 = strtod(command_stack.s[1],NULL);
                double fps2 = 1.0 * strtol(command_stack.s[1],NULL,0);
                video_play_fps = fps1 > fps2 ? fps1 : fps2;
                if (video_play_fps < 0.25) video_play_fps = 0.30;
                if (video_play_fps > 650.0) video_play_fps = 600.0;

                if (video_play_fps <= 15) buffer_update_velocity = 3;
                else if (video_play_fps <= 30) buffer_update_velocity = 6;
                else if (video_play_fps <= 50) buffer_update_velocity = 10;
                else if (video_play_fps <= 100) buffer_update_velocity = 20;
                else if (video_play_fps <= 200) buffer_update_velocity = 40;
                else buffer_update_velocity = 60;

                printf("Video fps modefied: %.2f",video_play_fps);
            }
            if (command_stack.len >= 3) 
                printf("Surplus arguments (more than three) has been input after -fps or --fps."); // 参数过多
            puts("");
        }

        else if (strcmp(command_stack.s[0],"-s") == 0 || strcmp(command_stack.s[0],"-skip") == 0){
            if (command_stack.len == 1) 
                printf("No argument after -s or --skip.");    // 参数缺失
            if (command_stack.len >= 2){
                skip_frame = strtol(command_stack.s[1],NULL,0);
                printf("Skip frame modefied: %d",skip_frame);
            }
            if (command_stack.len >= 3) 
            printf("Surplus arguments (more than three) has been input after -s or --skip."); // 参数过多
            puts("");
        }

        else {
            printf("Wrong arugment, no such commands: %s\n",command_stack.s[0]);   // 参数未知
        }
    }
    command_stack.len = 0;      // 模拟清空指令栈
}


int frame_cal(int row,int colmn){       // 计算原视频中某行某列的像素格在data中对应的下标（会返回R对应位置）
    return frame_linesize * row + colmn * 3;
}


int gray_cal(color *c){         // 灰度值计算
    switch(6){
        case 1: return (int)(c -> R * 0.3 + c -> G * 0.59 + c -> B * 0.11);
        case 2: return (c -> R * 30 + c -> G * 59 + c -> B * 11) / 100;  
        case 3: return (c -> R * 28 + c -> G * 151 + c -> B * 77) >> 8;
        case 4: return (c -> R + c -> G + c -> B) / 3;
        case 5: return c -> G;
        case 6: return (c -> R * 2126 + c -> G * 7152 + c -> B * 722) / 10000;  
    }
    /*
    1.浮点算法：Gray=R*0.3+G*0.59+B*0.11 
    2.整数方法：Gray=(R*30+G*59+B*11)/100   
    3.移位方法：Gray =(R*28+G*151+B*77)>>8 
    4.平均值法：Gray=（R+G+B）/3  
    5.仅取绿色：Gray=G      
    */
}


char gray_to_punct(int x){      // 灰度值转换为字符集中标点
    return punct_collection[x * punct_collection_length / 256];
}


void frame_show(Frame *f){          // 打印一帧
    int pos_R = 0,pos_G = 1,pos_B = 2;      // 记录R,G,B值位置
    new_frame_width = (int)ceil(f -> width / pixel_width);
    new_frame_height = (int)ceil(f -> height / pixel_height);
    new_frame_linesize = new_frame_width * 3;               // 计算修改尺寸后的一帧的长宽，data一行的长
    for (int i = 0;i < new_frame_height;i ++){      // 遍历行
        for (int j = 0;j < new_frame_width;j ++){       // 遍历列
            if (pooling == MAX_POOLING)
                color0 = resize_max_pooling(f,pixel_width,pixel_height,i,j);
            else if (pooling == AVERAGE_POOLING)
                color0 = resize_average_pooling(f,pixel_width,pixel_height,i,j);
            printf("\x1b[38;2;%d;%d;%dm█",color0.R,color0.G,color0.B);   // 打印像素块
        }
        puts("");
    }
    printf("\033[0m"); 
}


color resize_max_pooling(Frame *f,int w,int h,int row,int colmn){      // 最大堆积池算法
    color c = {0,0,0};
    for (int i = row * h;i < row * h + h;i += 1){
        for (int j = colmn * w;j < colmn * w + w;j += 1){
            if (f -> data[frame_cal(i,j) + 0] > c.R) c.R = f -> data[frame_cal(i,j) + 0];
            if (f -> data[frame_cal(i,j) + 1] > c.G) c.G = f -> data[frame_cal(i,j) + 1];
            if (f -> data[frame_cal(i,j) + 2] > c.B) c.B = f -> data[frame_cal(i,j) + 2];
            //printf("i:%d j:%d\t",i,j);
        }
    }
    if (is_gray) c.R = c.G = c.B = gray_cal(&c);
    return c;
}


color resize_average_pooling(Frame *f,int w,int h,int row,int colmn){      // 平均堆积池算法
    int x = 0,y = 0,z = 0;
    color c = {0,0,0};
    for (int i = row * h;i < row * h + h;i += 1){
        for (int j = colmn * w;j < colmn * w + w;j += 1){
            x += f -> data[frame_cal(i,j) + 0];
            y += f -> data[frame_cal(i,j) + 1];
            z += f -> data[frame_cal(i,j) + 2];
        }
    }
    c.R = x / (w * h), c.G = y / (w * h), c.B = z / (w * h);
    //printf("R:%d G:%d B:%d\n",c.R,c.G,c.B);
    if (is_gray) c.R = c.G = c.B = gray_cal(&c);
    return c;
}


int frame_buffer_collect(void *ptr){        // 帧缓存收集
    Collect_data cur_data = *(Collect_data *)ptr;
    if (cur_data.frame.data == NULL || (buffer_cur_collect[cur_data.cur_thrd_index] + 10) % BUFFER_MAX == buffer_cur_print) {
        threads_busy[cur_data.cur_thrd_index] = false;
        return thrd_error;      // 若传参为空或所有缓存区（非严格）均满，退出线程
    }
    
    while (buffer_save[buffer_cur_collect[cur_data.cur_thrd_index]].is_done != false){  
                                        // 使当前线程欲收集的缓存区指向尚未收集过缓存的缓存区
        if ((buffer_cur_collect[cur_data.cur_thrd_index] + 10) % BUFFER_MAX == buffer_cur_print) {
            threads_busy[cur_data.cur_thrd_index] = false;
            return thrd_error;      // 所有缓存区（非严格）均满，退出线程
        }
        buffer_cur_collect[cur_data.cur_thrd_index] = (buffer_cur_collect[cur_data.cur_thrd_index] + 1) % BUFFER_MAX;
    }

    uint8 *buffer_new = (uint8 *)malloc(sizeof(uint8) * new_frame_height * new_frame_width * 3 + 100);
                                    // 为缓存区分配待存储空间
    color color2;

    for (int i = 0;i < new_frame_height;i ++){      // 保存缓存信息
        for (int j = 0;j < new_frame_width;j ++){
            if (pooling == MAX_POOLING)
                color2 = resize_max_pooling(&cur_data.frame,pixel_width,pixel_height,i,j);
            else if (pooling == AVERAGE_POOLING)
                color2 = resize_average_pooling(&cur_data.frame,pixel_width,pixel_height,i,j);
            buffer_new[i * new_frame_linesize + j * 3 + 0] = color2.R;
            buffer_new[i * new_frame_linesize + j * 3 + 1] = color2.G;
            buffer_new[i * new_frame_linesize + j * 3 + 2] = color2.B;
        }
    }

    // 更新缓冲相关信息
    buffer_save[buffer_cur_collect[cur_data.cur_thrd_index]].index = cur_data.handle_frame_index;
    buffer_save[buffer_cur_collect[cur_data.cur_thrd_index]].buffer = buffer_new;
    buffer_save[buffer_cur_collect[cur_data.cur_thrd_index]].is_done = true;

    threads_busy[cur_data.cur_thrd_index] = false;  // 更新线程状态
    buffer_cur_collect[cur_data.cur_thrd_index] = (buffer_cur_collect[cur_data.cur_thrd_index] + 1) % BUFFER_MAX;
    //printf("\tbuffer_save [%d] has been bulit successfully.\n",buffer_cur_collect[cur_data.cur_thrd_index]);
    
    return thrd_success;
}


int frame_buffer_print(void *ptr){              // 帧缓存打印
    if (buffer_save[buffer_cur_print].is_done == false 
                || buffer_save[buffer_cur_print].buffer == NULL) 
        return thrd_error;      // 保证所打印缓存已被保存完全且非空

    int pos = 0;    // 缓冲位置指标
    for (int i = 0;i < new_frame_height;i ++){
        for (int j = 0;j < new_frame_width;j ++){
            if (is_punct) printf("%c",gray_to_punct(buffer_save[buffer_cur_print].buffer[pos]));
            else        // 打印像素块/字符
                printf("\x1b[38;2;%d;%d;%dm█",buffer_save[buffer_cur_print].buffer[pos],
                                        buffer_save[buffer_cur_print].buffer[pos + 1],
                                        buffer_save[buffer_cur_print].buffer[pos + 2]);
            pos += 3;
        }
        puts("");
    }

    free(buffer_save[buffer_cur_print].buffer);     // 释放缓存区
    buffer_save[buffer_cur_print].is_done = false;  // 更新缓存去状态
    buffer_cur_print = (buffer_cur_print + 1) % BUFFER_MAX;     // 更新缓存区打印索引
        
    return thrd_success;
}


static __inline int tty_reset(void){        // 
        if (tcsetattr(STDIN_FILENO, TCSANOW, &ori_attr) != 0)
                return -1;

        return 0;
}


static __inline int tty_set(void){          // 别人的轮子
        if ( tcgetattr(STDIN_FILENO, &ori_attr) )
                return -1;
       
        memcpy(&cur_attr, &ori_attr, sizeof(cur_attr) );
        cur_attr.c_lflag &= ~ICANON;
//        cur_attr.c_lflag |= ECHO;
        cur_attr.c_lflag &= ~ECHO;
        cur_attr.c_cc[VMIN] = 1;
        cur_attr.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &cur_attr) != 0)
                return -1;

        return 0;
}


static __inline int kbhit(void) {           //              
        fd_set rfds;
        struct timeval tv;
        int retval;

        /* Watch stdin (fd 0) to see when it has input. */
        FD_ZERO(&rfds);
        FD_SET(0, &rfds);
        /* Wait up to five seconds. */
        tv.tv_sec  = 0;
        tv.tv_usec = 0;

        retval = select(1, &rfds, NULL, NULL, &tv);
        /* Don't rely on the value of tv now! */

        if (retval == -1) {
                perror("select()");
                return 0;
        } else if (retval)
                return 1;
        /* FD_ISSET(0, &rfds) will be true. */
        else
                return 0;
        return 0;
}


int get_key_console(void *ptr){     // 从终端获取用户输入
    input_char = 0;
    while (!kbhit());

    input_char = fgetc(stdin);
    if (input_char) fprintf(stdout,"%c pressed\n", input_char);
    key_status = input_char;
        
    threads_busy[THRD_GETKEY] = 0;  // 终端输入线程繁忙置否
    return thrd_success;
}


void key_state_handle(int key){         // 对用户输入进行处理
    switch (key){
        case 'h':       // 帮助
        case 'H': {
            printf("The following instructions are available:\n");
            printf("'h' or 'H': guide help.\n");
            printf("'a' or 'A': slow down the video play speed.\n");
            printf("'d' or 'D': slow down the video play speed.\n");
            printf("' ' or 'p' or 'P: pause/resume the video.\n");
            printf("'g' or 'G': shit the gray/punct print-mode (this way the print-mode originally should also be gray/punct mode).\n");
        }  break;

        case 'a':       // 视频减速
        case 'A': {
            video_play_fps *= 0.7;
            if (video_play_fps < 0.25) video_play_fps = 0.30;
            printf("The video has been slowed.\t");
            printf("Current video play speed: %.2f\t(Min:0.30 / Max:600.00)\n",video_play_fps);

            if (video_play_fps <= 15) buffer_update_velocity = 3;
            else if (video_play_fps <= 30) buffer_update_velocity = 6;
            else if (video_play_fps <= 50) buffer_update_velocity = 10;
            else if (video_play_fps <= 100) buffer_update_velocity = 20;
            else if (video_play_fps <= 200) buffer_update_velocity = 40;
            else buffer_update_velocity = 60;
        }  break;
        case 'd':       // 视频加速
        case 'D': {
            video_play_fps *= 1.4;
            if (video_play_fps > 650.0) video_play_fps = 600.0;
            printf("The video has been accelerated.\t");
            printf("Current video play speed: %.2f\t(Min:1.00 / Max:600.00)\n",video_play_fps);

            if (video_play_fps <= 15) buffer_update_velocity = 3;
            else if (video_play_fps <= 30) buffer_update_velocity = 6;
            else if (video_play_fps <= 50) buffer_update_velocity = 10;
            else if (video_play_fps <= 100) buffer_update_velocity = 20;
            else if (video_play_fps <= 200) buffer_update_velocity = 40;
            else buffer_update_velocity = 60;
        }  break;

        case ' ':       // 视频暂停/恢复播放
        case 'p':
        case 'P': {
            is_play_paused ^= true;
            if (is_play_paused) printf("The video has been paused.\n");
            else printf("Resume play video.\n");
        }  break;

        case 'g':       // 灰度图打印与字符图打印模式切换
        case 'G': {
            if (is_gray == true) {
                is_punct ^= true;
                if (is_punct) printf("Play mode has been shifted into punct.\n");
                else printf("Play mode has been shifted into gray.\n");
            }
            else printf("Gray mode not started.");
        }  break;

        default: ;
    }
}



/*
gcc src/main.c -I include/ -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 -r
sudo vi /usr/share/vte/termcap/xterm
*/



