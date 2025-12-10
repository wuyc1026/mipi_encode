#ifndef _CAMERA_INIT_H
#define _CAMERA_INIT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/vfs.h>   
#include <sys/statvfs.h>
#include <rockchip/mpp_buffer.h>
#include <linux/videodev2.h>
#include <rockchip/rk_mpi.h>
#define _POSIX_C_SOURCE 200809L  // 启用POSIX.1-2008特性
#define _XOPEN_SOURCE 700        // 启用X/Open 7特性

#define width1  1920
#define height1  1080
#define buffer_size   YUV_SIZE+1920*4
#define YUV_SIZE      (width1 * height1 * 3 / 2)  // NV12格式
#define JPEG_QUALITY  80

typedef struct {
    int fd;                         // 摄像头文件描述符
    struct v4l2_format fmt;         // 视频格式
    struct v4l2_buffer buf;         // 缓冲区信息
    struct v4l2_requestbuffers req; // 缓冲区请求
    void* buffers[4][2];                 // 映射的缓冲区指针数组
    unsigned int n_buffers;        // 缓冲区数量
    unsigned int buf_size;          // 每个缓冲区大小
} camera_t;

int camera_init(camera_t* cam, const char* device, int width, int height, uint32_t pixelformat);
int camera_start_capture(camera_t* cam);
void* capture_yuv_frame(camera_t* cam, int timeout_ms);
int requeue_buffer(camera_t* cam);
int write_data_to_file(const char* filename, const void* data, size_t size);
#endif