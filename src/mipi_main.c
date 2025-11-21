#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <rockchip/rk_mpi.h>

// 摄像头设备结构体
typedef struct {
    int fd;                         // 摄像头文件描述符
    struct v4l2_format fmt;         // 视频格式
    struct v4l2_buffer buf;         // 缓冲区信息
    struct v4l2_requestbuffers req; // 缓冲区请求
    void** buffers;                 // 映射的缓冲区指针数组
    unsigned int n_buffers;        // 缓冲区数量
    unsigned int buf_size;          // 每个缓冲区大小
} camera_t;

// MPP编码器结构体
typedef struct {
    MppCtx ctx;                     // MPP上下文
    MppApi* mpi;                    // MPP接口
    MppEncPrepCfg prep_cfg;         // 预处理配置
    MppEncCodecCfg codec_cfg;       // 编解码配置
} mpp_encoder_t;

/**
 * @brief 初始化摄像头并设置YUV格式
 * @param cam 摄像头结构体指针
 * @param device 摄像头设备路径
 * @param width 图像宽度
 * @param height 图像高度
 * @param pixelformat 像素格式(V4L2_PIX_FMT_YUYV等)
 * @return 成功返回0，失败返回-1
 */
int camera_init(camera_t* cam, const char* device, int width, int height, uint32_t pixelformat) {
    struct v4l2_capability cap;
    
    printf("正在初始化摄像头: %s\n", device);
    
    // 1. 打开摄像头设备
    cam->fd = open(device, O_RDWR | O_NONBLOCK);
    if (cam->fd < 0) {
        perror("无法打开摄像头设备");
        return -1;
    } 
    
    // 2. 查询设备能力
    if (ioctl(cam->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("无法查询设备能力");
        close(cam->fd);
        return -1;
    }
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        printf("错误: 设备不支持视频采集\n");
        close(cam->fd);
        return -1;
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("错误: 设备不支持流式IO\n");
        close(cam->fd);
        return -1;
    }
    
    printf("摄像头支持能力: 0x%x\n", cap.capabilities);
    
    // 3. 设置图像格式
    memset(&cam->fmt, 0, sizeof(cam->fmt));
    cam->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam->fmt.fmt.pix.width = width;
    cam->fmt.fmt.pix.height = height;
    cam->fmt.fmt.pix.pixelformat = pixelformat;
    cam->fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (ioctl(cam->fd, VIDIOC_S_FMT, &cam->fmt) < 0) {
        perror("无法设置视频格式");
        close(cam->fd);
        return -1;
    }
    
    // 检查实际设置的格式
    if (ioctl(cam->fd, VIDIOC_G_FMT, &cam->fmt) < 0) {
        perror("无法获取视频格式");
        close(cam->fd);
        return -1;
    }
    
    printf("实际设置格式: %dx%d, pixelformat: 0x%08x\n", 
           cam->fmt.fmt.pix.width, cam->fmt.fmt.pix.height, 
           cam->fmt.fmt.pix.pixelformat);
    
    // 4. 请求缓冲区
    memset(&cam->req, 0, sizeof(cam->req));
    cam->req.count = 4;  // 请求4个缓冲区
    cam->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam->req.memory = V4L2_MEMORY_MMAP;  // 内存映射方式
    
    if (ioctl(cam->fd, VIDIOC_REQBUFS, &cam->req) < 0) {
        perror("无法请求缓冲区");
        close(cam->fd);
        return -1;
    }
    
    if (cam->req.count < 2) {
        printf("错误: 缓冲区数量不足: %d\n", cam->req.count);
        close(cam->fd);
        return -1;
    }
    
    cam->n_buffers = cam->req.count;
    cam->buffers = calloc(cam->n_buffers, sizeof(void*));
    if (!cam->buffers) {
        printf("错误: 无法分配缓冲区指针数组\n");
        close(cam->fd);
        return -1;
    }
    
    // 5. 映射缓冲区到用户空间
    for (unsigned int i = 0; i < cam->n_buffers; i++) {
        memset(&cam->buf, 0, sizeof(cam->buf));
        cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cam->buf.memory = V4L2_MEMORY_MMAP;
        cam->buf.index = i;
        
        if (ioctl(cam->fd, VIDIOC_QUERYBUF, &cam->buf) < 0) {
            perror("无法查询缓冲区信息");
            close(cam->fd);
            return -1;
        }
        
        cam->buffers[i] = mmap(NULL, cam->buf.length, 
                              PROT_READ | PROT_WRITE, 
                              MAP_SHARED, 
                              cam->fd, cam->buf.m.offset);
        
        if (cam->buffers[i] == MAP_FAILED) {
            perror("无法映射缓冲区");
            close(cam->fd);
            return -1;
        }
        
        cam->buf_size = cam->buf.length;
        printf("缓冲区 %d: 地址=%p, 大小=%u\n", i, cam->buffers[i], cam->buf_size);
    }
    
    printf("摄像头初始化成功\n");
    return 0;
}
/**
 * @brief 开始摄像头采集
 * @param cam 摄像头结构体指针
 * @return 成功返回0，失败返回-1
 */
int camera_start_capture(camera_t* cam) {
    // 将所有缓冲区加入队列
    for (unsigned int i = 0; i < cam->n_buffers; i++) {
        memset(&cam->buf, 0, sizeof(cam->buf));
        cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cam->buf.memory = V4L2_MEMORY_MMAP;
        cam->buf.index = i;
        
        if (ioctl(cam->fd, VIDIOC_QBUF, &cam->buf) < 0) {
            perror("无法将缓冲区加入队列");
            return -1;
        }
    }
    
    // 开始采集
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(cam->fd, VIDIOC_STREAMON, &type) < 0) {
        perror("无法开始采集流");
        return -1;
    }
    
    printf("摄像头采集已启动\n");
    return 0;
}

/**
 * @brief 从摄像头捕获一帧YUV数据
 * @param cam 摄像头结构体指针
 * @param timeout_ms 超时时间(毫秒)
 * @return 成功返回YUV数据指针，失败返回NULL
 */
void* capture_yuv_frame(camera_t* cam, int timeout_ms) {
    fd_set fds;
    struct timeval tv;
    int ret;
    
    FD_ZERO(&fds);
    FD_SET(cam->fd, &fds);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    // 等待数据就绪
    ret = select(cam->fd + 1, &fds, NULL, NULL, &tv);
    if (ret <= 0) {
        if (ret == 0) {
            printf("采集超时\n");
        } else {
            perror("select错误");
        }
        return NULL;
    }
    
    // 从队列中取出缓冲区
    memset(&cam->buf, 0, sizeof(cam->buf));
    cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam->buf.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl(cam->fd, VIDIOC_DQBUF, &cam->buf) < 0) {
        perror("无法从队列取出缓冲区");
        return NULL;
    }
    
    if (cam->buf.index >= cam->n_buffers) {
        printf("错误: 缓冲区索引越界: %d\n", cam->buf.index);
        return NULL;
    }
    
    printf("捕获到一帧: 缓冲区索引=%d, 大小=%u\n", cam->buf.index, cam->buf.bytesused);
    return cam->buffers[cam->buf.index];
}

/**
 * @brief 将缓冲区重新加入队列
 * @param cam 摄像头结构体指针
 * @return 成功返回0，失败返回-1
 */
int requeue_buffer(camera_t* cam) {
    if (ioctl(cam->fd, VIDIOC_QBUF, &cam->buf) < 0) {
        perror("无法重新将缓冲区加入队列");
        return -1;
    }
    return 0;
}

/**
 * @brief 将YUV数据放入MPP Buffer（Internal模式关键函数）
 * @param encoder MPP编码器指针
 * @param yuv_data YUV数据指针
 * @param yuv_size YUV数据大小
 * @param width 图像宽度
 * @param height 图像高度
 * @param format YUV格式(MPP_FMT_YUV422_YUYV等)
 * @return 成功返回MPP帧指针，失败返回NULL
 */
MppFrame put_yuv_data_to_mpp_buffer(mpp_encoder_t* encoder, void* yuv_data, 
                                   size_t yuv_size, int width, int height, 
                                   MppFrameFormat format) {
    MPP_RET ret = MPP_OK;
    MppFrame frame = NULL;
    MppBuffer buffer = NULL;
    MppBufferInfo info;

    printf("开始将YUV数据放入MPP Buffer...\n");

    // 1. 创建MPP帧
    ret = mpp_frame_init(&frame);
    if (ret != MPP_OK) {
        printf("错误: 无法初始化MPP帧, ret=%d\n", ret);
        return NULL;
    }
    // 2. 设置MPP帧参数
    mpp_frame_set_width(frame, width);
    mpp_frame_set_height(frame, height);
    mpp_frame_set_hor_stride(frame, width);   // 水平 stride
    mpp_frame_set_ver_stride(frame, height);   // 垂直 stride
    mpp_frame_set_fmt(frame, format);         // YUV格式
    mpp_frame_set_eos(frame, 1);              // 结束帧标记
    //MppMeta meta = mpp_frame_get_meta(frame);//放进encode函数中
    // 3. 创建MPP Buffer（INTERNAL模式关键步骤）
    memset(&info, 0, sizeof(info));
    info.size = yuv_size;
    info.ptr = yuv_data;      // 关键：直接使用摄像头YUV数据指针
    info.fd = -1;             // 不使用文件描述符
    info.type = MPP_BUFFER_TYPE_ION;  // ION内存类型
    ret=mpp_buffer_import(&buffer,&info);
    // 5. 将MPP Buffer附加到MPP帧
    mpp_frame_set_buffer(frame, buffer);
    
    // 6. 设置时间戳（可选）
    struct timeval tv;
    gettimeofday(&tv, NULL);
    mpp_frame_set_pts(frame, (RK_S64)tv.tv_sec * 1000000 + tv.tv_usec);
    
    printf("YUV数据成功放入MPP Buffer: 大小=%zu, 格式=%d\n", yuv_size, format);
    
    // 注意：buffer会在frame被销毁时自动释放
    // 不需要手动调用mpp_buffer_put(buffer)
    
    return frame;
}
/**
 * @brief 使用MPP编码器编码YUV帧为JPEG
 * @param encoder MPP编码器指针
 * @param frame 包含YUV数据的MPP帧
 * @param output_file 输出JPEG文件名
 * @return 成功返回0，失败返回-1
 */
int encode_frame_to_jpeg(mpp_encoder_t* encoder, MppFrame frame, const char* output_file) {
    MPP_RET ret = MPP_OK;
    MppPacket packet = NULL;
    FILE* fp = NULL;
    
    printf("开始JPEG编码...\n");
    
    // 1. 将帧送入编码器
    ret = encoder->mpi->encode_put_frame(encoder->ctx, frame);
    if (ret != MPP_OK) {
        printf("错误: 无法将帧送入编码器, ret=%d\n", ret);
        return -1;
    }
    
    // 2. 获取编码后的数据包
    ret = encoder->mpi->encode_get_packet(encoder->ctx, &packet);
    if (ret != MPP_OK) {
        printf("错误: 无法获取编码数据包, ret=%d\n", ret);
        return -1;
    }
    
    if (packet == NULL) {
        printf("错误: 编码数据包为空\n");
        return -1;
    }
    
    // 3. 获取JPEG数据
    void* jpeg_data = mpp_packet_get_data(packet);
    size_t jpeg_size = mpp_packet_get_length(packet);
    
    if (jpeg_data == NULL || jpeg_size == 0) {
        printf("错误: JPEG数据无效\n");
        mpp_packet_deinit(&packet);
        return -1;
    }
    
    // 4. 保存JPEG文件
    fp = fopen(output_file, "wb");
    if (!fp) {
        perror("无法打开输出文件");
        mpp_packet_deinit(&packet);
        return -1;
    }
    
    size_t written = fwrite(jpeg_data, 1, jpeg_size, fp);
    fclose(fp);
    
    if (written != jpeg_size) {
        printf("错误: 文件写入不完整: %zu/%zu\n", written, jpeg_size);
        mpp_packet_deinit(&packet);
        return -1;
    }
    
    printf("JPEG编码成功: %s, 大小: %zu 字节\n", output_file, jpeg_size);
    
    // 5. 清理数据包
    mpp_packet_deinit(&packet);
    
    return 0;
}

/**
 * @brief 主函数示例
 */
int main() {
    camera_t cam;
    mpp_encoder_t encoder;
    void* yuv_data;
    MppFrame frame;
    
    const char* camera_device = "/dev/video0";
    const char* output_file = "capture.jpg";
    int width = 1920;
    int height = 1080;
    uint32_t pixelformat = V4L2_PIX_FMT_YUYV;  // YUV422格式
    
    printf("=== RK3562摄像头YUV数据采集与MPP Buffer处理示例 ===\n");
    
    // 1. 初始化摄像头
    if (camera_init(&cam, camera_device, width, height, pixelformat) != 0) {
        printf("摄像头初始化失败!\n");
        return -1;
    }
    
    // 2. 开始采集
    if (camera_start_capture(&cam) != 0) {
        printf("摄像头采集启动失败!\n");
        close(cam.fd);
        return -1;
    }
    
    // 3. 捕获一帧YUV数据
    yuv_data = capture_yuv_frame(&cam, 5000);  // 5秒超时
    if (!yuv_data) {
        printf("YUV数据捕获失败!\n");
        close(cam.fd);
        return -1;
    }
    
    // 4. 将YUV数据放入MPP Buffer（关键步骤）
    MppFrameFormat mpp_format;
    switch (pixelformat) {
        case V4L2_PIX_FMT_YUYV:
            mpp_format = MPP_FMT_YUV422_YUYV;
            break;
        case V4L2_PIX_FMT_NV12:
            mpp_format = MPP_FMT_YUV420SP;
            break;
        case V4L2_PIX_FMT_YUV420:
            mpp_format = MPP_FMT_YUV420P;
            break;
        default:
            mpp_format = MPP_FMT_YUV422_YUYV;
    }
    
    frame = put_yuv_data_to_mpp_buffer(&encoder, yuv_data, cam.buf.bytesused, 
                                      width, height, mpp_format);
    if (!frame) {
        printf("YUV数据放入MPP Buffer失败!\n");
        requeue_buffer(&cam);
        close(cam.fd);
        return -1;
    }
    
    // 5. 编码为JPEG（这里假设encoder已经初始化）
    // if (encode_frame_to_jpeg(&encoder, frame, output_file) != 0) {
    //     printf("JPEG编码失败!\n");
    // }
    
    // 6. 清理资源
    mpp_frame_deinit(&frame);
    requeue_buffer(&cam);
    close(cam.fd);
    
    printf("处理完成!\n");
    return 0;
}