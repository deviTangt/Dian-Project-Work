#ifndef __VIDEO_DECODER_HEADER__
#define __VIDEO_DECODER_HEADER__

typedef struct _Frame{
    int width;
    int height;
    int linesize;
    unsigned char* data;
} Frame;

int decoder_init(const char *filename);
Frame decoder_get_frame();
void decoder_close();
double get_fps();
int get_frame_index();
int get_total_frames();

#endif // !__VIDEO_DECODER_HEADER__