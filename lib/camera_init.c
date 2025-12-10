#include "camera_init.h"

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
    } else{
        printf("打开成功！\n");
    }
    
    // 2. 查询设备能力
    if (ioctl(cam->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("无法查询设备能力");
        close(cam->fd);
        return -1;
    }else{
        printf("查询设备能力成功！\n");
    }
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        printf("错误: 设备不支持视频采集\n");
        close(cam->fd);
        return -1;
    }else{
        printf("设备支持多平面采集！\n");
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("错误: 设备不支持流式IO\n");
        close(cam->fd);
        return -1;
    }else{
        printf("设备支持流式IO！\n");
    }
    
    printf("摄像头支持能力: 0x%x\n", cap.capabilities);
    
    // 3. 设置图像格式
    memset(&cam->fmt, 0, sizeof(cam->fmt));
    cam->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    cam->fmt.fmt.pix_mp.width = width;                    // 使用 pix_mp
    cam->fmt.fmt.pix_mp.height = height;                  // 使用 pix_mp
    cam->fmt.fmt.pix_mp.pixelformat = pixelformat;        // 使用 pix_mp
    cam->fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    cam->fmt.fmt.pix_mp.num_planes = 1;
    cam->fmt.fmt.pix_mp.plane_fmt[0].sizeimage = width * height*3/2;     // Y平面大小
    cam->fmt.fmt.pix_mp.plane_fmt[0].bytesperline = width;          // Y平面步长

        
    if (ioctl(cam->fd, VIDIOC_S_FMT, &cam->fmt) < 0) {
        perror("无法设置视频格式");
        close(cam->fd);
        return -1;
    }else{
        printf("设置视频格式成功！\n");
    }

    // 设置格式后，立即获取驱动实际设置的格式进行验证
    struct v4l2_format actual_fmt;
    memset(&actual_fmt, 0, sizeof(actual_fmt));
    actual_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (ioctl(cam->fd, VIDIOC_G_FMT, &actual_fmt) == 0) {
        printf("驱动实际设置的格式信息：\n");
        printf("  像素格式: 0x%x (预期: 0x%x)\n", actual_fmt.fmt.pix_mp.pixelformat, pixelformat);
        printf("  分辨率: %dx%d (预期: %dx%d)\n", actual_fmt.fmt.pix_mp.width, actual_fmt.fmt.pix_mp.height, width, height);
        printf("  平面数量: %d\n", actual_fmt.fmt.pix_mp.num_planes);
        
        for (int i = 0; i < actual_fmt.fmt.pix_mp.num_planes; i++) {
            printf("  平面[%d] - 步长: %u, 图像大小: %u\n", 
                i, 
                actual_fmt.fmt.pix_mp.plane_fmt[i].bytesperline,
                actual_fmt.fmt.pix_mp.plane_fmt[i].sizeimage);
        }

        // 检查关键参数是否被驱动修改
        if (actual_fmt.fmt.pix_mp.pixelformat != pixelformat) {
            printf("警告：驱动修改了像素格式！\n");
        }
        if (actual_fmt.fmt.pix_mp.num_planes < cam->fmt.fmt.pix_mp.num_planes) {
            printf("严重警告：驱动返回的平面数量为 %d，可能不支持多平面NV12！\n", actual_fmt.fmt.pix_mp.num_planes);
        }
    } else {
        perror("无法获取实际视频格式");
    }
    

    // 4. 请求缓冲区
    memset(&cam->req, 0, sizeof(cam->req));
    cam->req.count = 4;  // 请求4个缓冲区
    cam->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    cam->req.memory = V4L2_MEMORY_MMAP;  // 内存映射方式
    
    if (ioctl(cam->fd, VIDIOC_REQBUFS, &cam->req) < 0) {
        perror("无法请求缓冲区");
        close(cam->fd);
        return -1;
    }else{
        printf("请求缓冲区成功！\n");
    }
    
    if (cam->req.count < 2) {
        printf("错误: 缓冲区数量不足: %d\n", cam->req.count);
        close(cam->fd);
        return -1;
    }else{
        printf("请求四个缓冲区成功！\n");
    }
    
    cam->n_buffers = cam->req.count;
    // 5. 映射缓冲区到用户空间

    for (unsigned int i = 0; i < cam->n_buffers; i++) {
        struct v4l2_plane planes[1];  // 只分配一个平面
        memset(planes, 0, sizeof(planes)); // 关键：清空数组
        memset(&cam->buf, 0, sizeof(cam->buf));
        cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        cam->buf.memory = V4L2_MEMORY_MMAP;
        cam->buf.index = i;
        cam->buf.length = 1;        // 平面数量（只分配一个平面）
        cam->buf.m.planes = planes; // 指向平面数组
        if (ioctl(cam->fd, VIDIOC_QUERYBUF, &cam->buf) < 0) {
            perror("无法查询缓冲区信息");
            close(cam->fd);
            return -1;
        }else{
            printf("查询缓冲区信息成功...\n");
            printf("  平面0: 偏移=%u, 长度=%u\n", 
                   cam->buf.m.planes[0].m.mem_offset, 
                   cam->buf.m.planes[0].length);
        }
            cam->buffers[i][0] = mmap(
                NULL, // 让系统自动选择映射起始地址
                cam->buf.m.planes[0].length, // 该平面的长度
                PROT_READ | PROT_WRITE, // 映射区域可读可写
                MAP_SHARED, // 对映射区域的修改会同步到设备
                cam->fd, // 摄像头设备的文件描述符
                cam->buf.m.planes[0].m.mem_offset // 该平面在缓冲区中的偏移量
            );

            // 正确的错误检查：判断返回值是否为 MAP_FAILED
            if (cam->buffers[i][0] == MAP_FAILED) {
                perror("无法映射缓冲区");
                close(cam->fd);
                return -1;
            } else {
                printf("缓冲区[%d]  映射成功，地址：%p\n\n", i,  cam->buffers[i][0]);
            }

    }
    
    printf("====摄像头初始化成功====\n\n\n");
    return 0;
}
/**
 * @brief 开始摄像头采集
 * @param cam 摄像头结构体指针
 * @return 成功返回0，失败返回-1
 */
int camera_start_capture(camera_t* cam) {
    // 将所有缓冲区加入队列
    printf("准备将缓冲区加入队列...\n");
    for (unsigned int i = 0; i < cam->n_buffers; i++) {
        struct v4l2_plane planes[1]; // 只赋予一个平面
        memset(&cam->buf, 0, sizeof(cam->buf));
        memset(planes, 0, sizeof(planes)); // 清空平面数组
        cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        cam->buf.memory = V4L2_MEMORY_MMAP;
        cam->buf.index = i;
        cam->buf.length = 1; // 明确指定平面数量
        cam->buf.m.planes = planes; // 关联平面信息数组
        if (ioctl(cam->fd, VIDIOC_QBUF, &cam->buf) < 0) {
            perror("无法将缓冲区加入队列");
            return -1;
        }
    }

    // 开始采集
    printf("缓冲区加入队列成功！\n准备开始采集流\n");
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(cam->fd, VIDIOC_STREAMON, &type) < 0) {
        perror("无法开始采集流");
        return -1;
    }
    
    printf("====摄像头采集已启动====\n\n\n");
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
    struct v4l2_plane planes[1];
    memset(&cam->buf, 0, sizeof(cam->buf));
    memset(planes, 0, sizeof(planes)); // 清空平面数组
    cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    cam->buf.memory = V4L2_MEMORY_MMAP;
    cam->buf.length = 1; // 关键修正：明确告知驱动我们期望1个平面
    cam->buf.m.planes = planes; // 关键修正：关联平面信息数组
    if (ioctl(cam->fd, VIDIOC_DQBUF, &cam->buf) < 0) {
        perror("无法从队列取出缓冲区");
        return NULL;
    }
    
    if (cam->buf.index >= cam->n_buffers) {
        printf("错误: 缓冲区索引越界: %d\n", cam->buf.index);
        return NULL;
    }
    
    printf("捕获到一帧: 缓冲区索引=%d, 大小=%u\n", \
            cam->buf.index, cam->buf.m.planes[0].bytesused);
    printf("===YUV数据采集成功！！===\n\n");
    return cam->buffers[cam->buf.index][0];
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
 * @brief 将数据写入文件
 * @param cam 摄像头结构体指针
 * @return 成功返回0，失败返回-1
 */
int write_data_to_file(const char* filename, const void* data, size_t size) {
    printf("   正在将JPEG格式写入文件...\n");
    
    // 1. 检查输入参数
    if (!filename || !data || size == 0) {
        printf("     ❌ 无效的输入参数: filename=%p, data=%p, size=%zu\n", 
               filename, data, size);
        return -1;
    }
    
    // 2. 检查数据有效性
    printf("     检查数据有效性...\n");
    if (size >= 2) {
        unsigned char* jpeg_data = (unsigned char*)data;
        if (jpeg_data[0] == 0xFF && jpeg_data[1] == 0xD8) {
            printf("     ✅ 有效的JPEG文件头 (FF D8)\n");
        } else {
            printf("     ❌ 无效的JPEG文件头: %02X %02X\n", jpeg_data[0], jpeg_data[1]);
            printf("     可能不是有效的JPEG数据\n");
        }  
    }
    
    // 3. 打开文件 
    printf("     打开文件: %s\n", filename);
    FILE* file = fopen(filename, "wb");  // "wb" = 二进制写模式
    if (!file) {
        printf("     ❌ 无法创建文件: %s (errno: %d - %s)\n", 
               filename, errno, strerror(errno));
        return -1;
    }
    printf("     ✅ 文件打开成功\n");
    
    // 4. 写入数据 (使用fwrite代替write)
    printf("     准备写入: %zu 字节\n", size);
    size_t bytes_written = fwrite(data, 1, size, file);
    
    // 5. 检查写入结果
    if (bytes_written != size) {
        // 写入失败或不完整
        if (ferror(file)) {
            printf("     ❌ 写入失败: %s\n", strerror(errno));
        } else {
            printf("     ❌ 写入不完整: %zu/%zu 字节\n", bytes_written, size);
        }
        fclose(file);
        return -1;
    }
    
    printf("     ✅ 数据写入成功: %zu 字节\n", bytes_written);
    
    // 6. 刷新缓冲区确保数据写入磁盘
    if (fflush(file) != 0) {
        printf("     ⚠️  缓冲区刷新警告: %s\n", strerror(errno));
    } else {
        printf("     ✅ 缓冲区刷新成功\n");
    }
    
    // 7. 关闭文件 (使用fclose代替close)
    if (fclose(file) != 0) {
        printf("     ⚠️  文件关闭警告: %s\n", strerror(errno));
        return -1;
    }
    
    printf("     ✅ 文件关闭成功\n");
    
    // 8. 验证文件实际大小
    FILE* verify_file = fopen(filename, "rb");
    if (verify_file) {
        fseek(verify_file, 0, SEEK_END);
        long file_size = ftell(verify_file);
        fclose(verify_file);
        
        printf("     ✅ 文件验证成功: 实际大小 %ld 字节\n", file_size);
        if (file_size != size) {
            printf("     ⚠️  大小不匹配: 期望 %zu, 实际 %ld\n", size, file_size);
            return -1;
        }
    }
    
    printf("     ✅ 文件保存完成: %s\n", filename);
    return 0;
}
