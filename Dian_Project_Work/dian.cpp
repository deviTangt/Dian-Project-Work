#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <string.h>
#include <ctype.h>
#include <vector>

#include "video_decoder.h"

using namespace std;


/*
echo -e "\033[30m Hello \x1b[0mworld!!!"
echo -e "\x1b[0m Hello world!!!"
echo -e "\033[31m Hello world!!!"
echo -e "\033[1;31m Hello \x1b[0mworld!!!"
echo -e "\033[33m Hello world!!!"
echo -e "\033[34m Hello world!!!"
echo -e "\033[35m Hello world!!!"
echo -e "\033[36m Hello world!!!"
echo -e "\033[37m Hello world!!!"




*/






/*  
cd TSP/
./dian --help 356 -r 34 -b -v 4235 --file --color -r
g++ dian.cpp -I include/ -L lib/ -l tst -o dian.out
./dian.out -h -v
*/

/*
g++ -c *.cpp
ar rcs libtest.a *.o
g++ src/dian.cpp -I include/ -L lib/ -l videodecoder -o dian.out
./math.out
g++ dian.cpp -o dian -L. -lvideodecoder -lavformat -lavcodec -lavutil -lswscale
g++ src/dian.cpp -I include/ -L lib/ -l videodecoder -o dian.out

g++ main.cpp -o main -L. -lvideodecoder -lavformat -lavcodec -lavutil -lswscale
*/
vector<string> command_stack;

void command_stack_handle();

int main(int argc,char *argv[]){
    #if 0
    printf("arguments : %d\n",argc);
    puts("The arguments are :");
    for (int i = 0;i < argc;i ++) printf("%d: %s%6s",i,argv[i]," ");
    puts("\n");

    for (int i = 1;i < argc;i ++){
        if (argv[i][0] == '-'){
            command_stack_handle();
            command_stack.push_back(argv[i]);
        }
        else command_stack.push_back(argv[i]);
        if (i == argc - 1) command_stack_handle();
    }
    #endif
    puts("proceed\n");

    Frame f;
    decoder_init("bad_apple.mp4");
    //f = decoder_get_frame();
    //printf("%.4f   %d     %d\n",get_fps(),get_frame_index(),get_total_frames());


    puts("finish\n");
    return 0;
}

/*
g++ dian.cpp -I include/ -L lib/ -o dian.out
./dian.out -h -v 599 -r
g++ -c dian.cpp
ar rcs libtst.a dian.o
*/

void command_stack_handle(){
    if (command_stack.empty()) return;
    if (command_stack[0][0] != '-') printf("Invalid command was received.\n");
    else {
        printf("command< ");
        for (int i = 0;i < command_stack.size();i ++) printf("%s ",command_stack[i].c_str());
        printf(">:\t");

        if (command_stack[0] == "-h" || command_stack[0] == "--help"){
            printf("help:");
            if (command_stack.size() >= 2) printf("  Unkown arugment was passed after -h or --help");
        }
        else if (command_stack[0] == "-v" || command_stack[0] == "--version"){
            printf("\"dian-player v1.2\"");
            if (command_stack.size() >= 2) printf("  Unkown arugment was passed after -v or --version");
        }
        
        else if (command_stack[0] == "-c" || command_stack[0] == "--color"){
            printf("color");
        }
        else if (command_stack[0] == "-r" || command_stack[0] == "--resize"){
            printf("resize");
        }
        else if (command_stack[0] == "-f" || command_stack[0] == "--file"){
            printf("file");
        }

        else {
            printf("Wrong arugment, no such commands: %s",command_stack[0].c_str());
        }
    }
    puts("");
    command_stack.resize(0);
}


































