#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <threads.h>

#include "video_decoder.h"

#define COMMAND_STACK_MAX 100  // 读取一个形如 -r 3 8 指令的参数长度的最大长度（示例中长度为3）
#define busying 2
#define true 1
#define false 0
#define second *1000000  // 一秒的clock（）时间度量（严格来说比一秒略长）
#define THRD_NUM 200
#define LL unsigned long long int
#define uint8 unsigned char
#define BUFFER_MAX 200

#define GRAY_HANDLE 1
#define MAX_POOLING 2
#define AVERAGE_POOLING 3

typedef char* string;   // 字符串

struct{         // 接受命令行参数的执行容器栈  一次至多存储即处理一条以'-'开头的指令
    string s[COMMAND_STACK_MAX];
    int len;
}command_stack;

typedef struct{     // 颜色值
    unsigned char R,G,B;
}color;

/*          
cd TSP/
./src/main --help 356 -r 34 -b -v 4235 --file --color -r
./main.out -h -v
*/

char *play_file_path = NULL;    // 播放视频的路径

clock_t last_time = 0;          // 上一次更新视频帧的时刻
clock_t current_time = 0;       // 当前时刻（以clock（）记录为准）
int video_play_fps = 12;        // 视频播放帧率（1s内）
int skip_frame = 1;             // 视频播放帧时每次省略的帧数（而不必每帧都打印）（可以减少视频闪的程度）

int frame_width = 0, frame_height = 0, frame_linesize = 0, total_frame = 0;     // 源视频帧宽高，data一行元素数，总帧
int new_frame_width = 0, new_frame_height = 0, new_frame_linesize = 0, new_total_frame = 0; // 修改尺寸后的视频帧宽高，data一行元素数，总帧
int cur_pos = 0;            // 当前位置
double frame_fps = 5;       // 源视频帧率

int pixel_width = 5,pixel_height = 10;      // 修改尺寸后一个像素格在原视频中对应多长的宽高的值      
                                            // 参考：bad apple 3 8   // dragon 5 10
                                            // 由于终端打印的字体高宽比是12 ： 6,所以pixel_width ：pixel_height尽量维持在这个范围附近

color color0;           // 颜色值记录，打印像素格时使用
int pooling = MAX_POOLING;
int is_gray = true;

thrd_t threads[THRD_NUM];
int threads_busy[THRD_NUM] = {0};

typedef struct{
    uint8 *buffer;
    int is_done;
    int index;
}Buffer_saving;
Buffer_saving buffer_save[BUFFER_MAX + 10] = {0};
int buffer_cur_collect[100] = {0};
int buffer_cur_print = 0;
int is_frame_saved[100] = {0};

typedef struct{
    int thrd_index;
    Frame frame;
    int handle_frame_index;
    int cur_collect_thrd;
}Collect_data;



void command_stack_handle();        // 处理指令栈中的所有参数

int frame_cal(int row,int colmn);       // 计算原视频中某行某列的像素格在data中对应的下标（会返回R对应位置）

int gray_cal(color *c);         // 灰度值计算

void frame_show(Frame *f);          // 打印一帧

color max_pooling(Frame *f,int w,int h,int row,int colmn);      // 最大堆积池算法

color average_pooling(Frame *f,int w,int h,int row,int colmn);  // 平均堆积池算法

int frame_buffer_collect(void *ptr);

int frame_buffer_print(void *ptr);

// 

int main(int argc,char *argv[]){        
    printf("arguments : %d\n",argc);    
    puts("The arguments are :");
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
    frame_width = f.width, frame_height = f.height;
    total_frame = get_total_frames();
    frame_fps = get_fps();
    frame_linesize = f.linesize;
    printf("%.4f   %d     %d\n",get_fps(),get_frame_index(),get_total_frames());
    printf("%d  %d  %d\n",f.width,f.height,f.linesize);
    frame_show(&f);
    

#if 1
    for (int i = 100;i <= 70;i ++){
        f = decoder_get_frame();                        // 解码一帧，获取对应数据
        Collect_data collect_data = {i,f,get_frame_index(),i};
        thrd_create(&threads[i],frame_buffer_collect,&collect_data);    
        for (int i = 0;i < skip_frame;i ++) f = decoder_get_frame();  // 略读，忽略一些帧
        thrd_join(threads[i],NULL);
    }
/*              // 下面是对应指令，可直接使用来执行代码
gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 -r -f bad_apple.mp4 -r 3 8 -c m g

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 --file "dragon.mp4" -r 5 10 -c a g

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 --file "dragon.mp4" -r 4 10 -c a

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 --file "shi.mp4" -r 10 20 -c a 

gcc src/main.c -I include/ -lpthread -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 --file "ctj.mp4" -r 10 20 -c a
g
*/
    while (true){
        current_time = clock();
        
        if (current_time > last_time + 1 second / video_play_fps){  // 控制帧率
            last_time = current_time;                       // 更新时间
            printf("\x1b[H\x1b[2J");                        //清屏
            

            for (int i = 1;i <= 3;i ++){
                if (threads_busy[i] == false){
                    f = decoder_get_frame();                        // 解码一帧，获取对应数据
                    threads_busy[i] = true;
                    Collect_data collect_data = {i,f,get_frame_index(),i};
                    thrd_create(&threads[i],frame_buffer_collect,&collect_data);    
                    for (int i = 0;i < skip_frame;i ++) f = decoder_get_frame();  // 略读，忽略一些帧
                }
            }
            //for (int i = 1;i <= 3;i ++) thrd_join(threads[i],NULL);;
            //frame_show(&f);
            thrd_create(&threads[0],frame_buffer_print,NULL);
            thrd_join(threads[0],NULL);
            printf("print index: %d\t",buffer_save[buffer_cur_print - 1].index);
            printf("status: %d\t",buffer_save[buffer_cur_print - 1].is_done);
            printf("time: %d\n",(int)current_time);
        }
        

#if 1   // 终止条件判定
        if (get_frame_index() >= get_total_frames() - 10) {     // 视频播放完成，退出
            printf("The video has been completely played.\n");
            break;
        }
        //printf("%d\t%d\n",current_time,clock());
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
    puts("finish\n");
    return 0;
}

/*
gcc src/main.c -I include/ -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 -r 
*/

/*
g++ dian.cpp -I include/ -L lib/ -o dian.out
./dian.out -h -v 599 -r
g++ -c dian.cpp
ar rcs libtst.a dian.o
*/

void command_stack_handle(){            // 处理指令栈中的所有参数
    if (command_stack.len == 0) return;
    if (command_stack.s[0][0] != '-') printf("Invalid command was received.\n");    // 参数不合法
    else {
        printf("command< ");
        for (int i = 0;i < command_stack.len;i ++) printf("%s ",command_stack.s[i]);
        printf(">:\t");                 // 打印对应命令，便于查看效果

        if (strcmp(command_stack.s[0],"-h") == 0 || strcmp(command_stack.s[0],"--help") == 0){
            printf("help:");
            if (command_stack.len >= 2) printf("  Unkown arugment was passed after -h or --help");  // 参数过多
        }
        else if (strcmp(command_stack.s[0],"-v") == 0 || strcmp(command_stack.s[0],"--version") == 0){
            printf("\"dian-player v1.2\"");
            if (command_stack.len >= 2) printf("  Unkown arugment was passed after -v or --version");  // 参数过多
        }
        
        else if (strcmp(command_stack.s[0],"-c") == 0 || strcmp(command_stack.s[0],"--color") == 0){
            if (command_stack.len >= 1) {
                pooling = MAX_POOLING;
                is_gray = false;
            }
            else if (command_stack.len >= 2 && strcmp(command_stack.s[1],"g") == 0) is_gray == true;
            else if (command_stack.len >= 2 && strcmp(command_stack.s[1],"m") == 0) pooling = MAX_POOLING;
            else if (command_stack.len >= 2 && strcmp(command_stack.s[1],"a") == 0) pooling = AVERAGE_POOLING;
            if (command_stack.len >= 3 && strcmp(command_stack.s[2],"g") == 0) is_gray == true;
            else if (command_stack.len >= 3 && strcmp(command_stack.s[2],"m") == 0) pooling = MAX_POOLING;
            else if (command_stack.len >= 3 && strcmp(command_stack.s[2],"a") == 0) pooling = AVERAGE_POOLING;
        }
        else if (strcmp(command_stack.s[0],"-r") == 0 || strcmp(command_stack.s[0],"--resize") == 0){
            if (command_stack.len == 1) 
                printf("No argument after -r or --resize.\n");
            else if (command_stack.len == 2){
                pixel_width = (int)strtol(command_stack.s[1],NULL,0);
                pixel_height = 10;
                printf("Resize successfully. The pixel_height has been set defaultly : 10.\n");
            }
            else if (command_stack.len == 3) {
                pixel_width = (int)strtol(command_stack.s[1],NULL,0),
                pixel_height = (int)strtol(command_stack.s[2],NULL,0);
                printf("Resize successfully.\n");
                //printf("w:%d  h:%d\n",(int)strtol(command_stack.s[1],NULL,0),(int)strtol(command_stack.s[2],NULL,0));
            }
            else if (command_stack.len >= 4) 
                printf("Surplus arguments (more than three) has been input after -r or --resize.\n");  // 参数过多
            // bad apple 3 8   // dragon 5 10
        }
        else if (strcmp(command_stack.s[0],"-f") == 0 || strcmp(command_stack.s[0],"--file") == 0){
            if (command_stack.len == 1) 
                printf("No argument after -f or --file.\n");    // 参数缺失
            if (command_stack.len >= 2)
                play_file_path = command_stack.s[1];
            if (command_stack.len >= 3) 
                printf("Surplus arguments (more than three) has been input after -f or --file.\n"); // 参数过多
        }

        else {
            printf("Wrong arugment, no such commands: %s",command_stack.s[0]);   // 参数未知
        }
    }
    puts("");
    command_stack.len = 0;      // 模拟清空指令栈
}

int frame_cal(int row,int colmn){       // 计算原视频中某行某列的像素格在data中对应的下标（会返回R对应位置）
    return frame_linesize * row + colmn * 3;
}

int gray_cal(color *c){         // 灰度值计算
    switch(2){
        case 1: return (int)(c -> R * 0.3 + c -> G * 0.59 + c -> B * 0.11);
        case 2: return (c -> R * 30 + c -> G * 59 + c -> B * 11) / 100;  
        case 3: return (c -> R * 28 + c -> G * 151 + c -> B * 77) >> 8;
        case 4: return (c -> R + c -> G + c -> B) / 3;
        case 5: return c -> G;
    }
    /*
    1.浮点算法：Gray=R*0.3+G*0.59+B*0.11 
    2.整数方法：Gray=(R*30+G*59+B*11)/100   
    3.移位方法：Gray =(R*28+G*151+B*77)>>8 
    4.平均值法：Gray=（R+G+B）/3  
    5.仅取绿色：Gray=G      
    */
}


void frame_show(Frame *f){          // 打印一帧
    int pos_R = 0,pos_G = 1,pos_B = 2;      // 记录R,G,B值位置
    new_frame_width = (int)ceil(f -> width / pixel_width);
    new_frame_height = (int)ceil(f -> height / pixel_height);
    new_frame_linesize = new_frame_width * 3;               // 计算修改尺寸后的一帧的长宽，data一行的长
    for (int i = 0;i < new_frame_height;i ++){      // 遍历行
        for (int j = 0;j < new_frame_width;j ++){       // 遍历列
            if (pooling == MAX_POOLING)
                color0 = max_pooling(f,pixel_width,pixel_height,i,j);
            else if (pooling == AVERAGE_POOLING)
                color0 = average_pooling(f,pixel_width,pixel_height,i,j);
            printf("\x1b[38;2;%d;%d;%dm█",color0.R,color0.G,color0.B);   // 打印像素块
        }
        puts("");
    }
    printf("\033[0m");
    //puts("\n");
}

color max_pooling(Frame *f,int w,int h,int row,int colmn){      // 最大堆积池算法
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

color average_pooling(Frame *f,int w,int h,int row,int colmn){      // 平均堆积池算法
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


int frame_buffer_collect(void *ptr){
    Collect_data cur_data = *(Collect_data *)ptr;
    if ((buffer_cur_collect[cur_data.cur_collect_thrd] + 10) % BUFFER_MAX == buffer_cur_print) {
        threads_busy[cur_data.thrd_index] = false;
        return thrd_error;
    }
    while (buffer_save[buffer_cur_collect[cur_data.cur_collect_thrd]].is_done != false){
        if ((buffer_cur_collect[cur_data.cur_collect_thrd] + 10) % BUFFER_MAX == buffer_cur_print) {
            threads_busy[cur_data.thrd_index] = false;
            return thrd_error;
        }
        buffer_cur_collect[cur_data.cur_collect_thrd] = (buffer_cur_collect[cur_data.cur_collect_thrd] + 1) % BUFFER_MAX;
    }
    uint8 *buffer_new = (uint8 *)malloc(sizeof(uint8) * new_frame_height * new_frame_width * 3 + 100);
    color color2;

    buffer_save[buffer_cur_collect[cur_data.cur_collect_thrd]].index = cur_data.handle_frame_index;
    for (int i = 0;i < new_frame_height;i ++){
        for (int j = 0;j < new_frame_width;j ++){
            if (pooling == MAX_POOLING)
                color2 = max_pooling(&cur_data.frame,pixel_width,pixel_height,i,j);
            else if (pooling == AVERAGE_POOLING)
                color2 = average_pooling(&cur_data.frame,pixel_width,pixel_height,i,j);
            buffer_new[i * new_frame_linesize + j * 3 + 0] = color2.R;
            buffer_new[i * new_frame_linesize + j * 3 + 1] = color2.G;
            buffer_new[i * new_frame_linesize + j * 3 + 2] = color2.B;
        }
    }
    buffer_save[buffer_cur_collect[cur_data.cur_collect_thrd]].buffer = buffer_new;
    buffer_save[buffer_cur_collect[cur_data.cur_collect_thrd]].is_done = true;
    //printf("\tbuffer_save [%d] has been bulit successfully.\n",buffer_cur_collect[cur_data.cur_collect_thrd]);
    buffer_cur_collect[cur_data.cur_collect_thrd] = (buffer_cur_collect[cur_data.cur_collect_thrd] + 1) % BUFFER_MAX;
    
    threads_busy[cur_data.thrd_index] = false;
    return thrd_success;
}

int frame_buffer_print(void *ptr){
    if (buffer_save[buffer_cur_print].is_done == false || buffer_save[buffer_cur_print].buffer == NULL) return thrd_error;
    int pos = 0;
    //printf("\tbuffer_print_index:%d\n",buffer_cur_print);
    for (int i = 0;i < new_frame_height;i ++){
        for (int j = 0;j < new_frame_width;j ++){
            printf("\x1b[38;2;%d;%d;%dm█",buffer_save[buffer_cur_print].buffer[pos],
                buffer_save[buffer_cur_print].buffer[pos + 1],buffer_save[buffer_cur_print].buffer[pos + 2]);
            pos += 3;
        }
        puts("");
    }
    free(buffer_save[buffer_cur_print].buffer);
    buffer_save[buffer_cur_print].is_done = false;
    buffer_cur_print = (buffer_cur_print + 1) % BUFFER_MAX;
        
    return thrd_success;
}

/*
gcc src/main.c -I include/ -L lib/ -lvideodecoder -lavformat -lavcodec -lavutil -lswscale -o main.out
./main.out -h -v 599 -r
sudo vi /usr/share/vte/termcap/xterm
*/



