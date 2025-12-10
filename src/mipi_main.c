#include "camera_init.h"


/**
 * @brief 主函数
 */
int main() {
    camera_t cam;
    void* yuv_data;
    MppCtx ctx;
    MppApi *mpi;
    MppBufferGroup group;
    MppEncCfg cfg;
    MPP_RET ret;

    const char* camera_device = "/dev/video11";
    const char* output_file = "capture.jpg";
    int width = 1920;
    int height = 1080;
    uint32_t pixelformat = V4L2_PIX_FMT_NV12;  // NV12格式
    
    printf("=== RK3562摄像头YUV数据采集与MPP Buffer处理示例 ===\n");
    
    // 1. 初始化摄像头
    if (camera_init(&cam, camera_device, width, height, pixelformat) != 0) {
        perror("摄像头初始化失败!\n\n");
        return -1;
    }
    
    // 2. 开始采集
    if (camera_start_capture(&cam) != 0) {
        perror("摄像头采集启动失败!\n\n");
        close(cam.fd);
        return -1;
    }
    
    // 3. 捕获一帧YUV数据
    yuv_data = capture_yuv_frame(&cam, 5000);  // 5秒超时
    if (!yuv_data) {
        perror("YUV数据捕获失败!\n\n");
        close(cam.fd);
        return -1;
    }
    // 初始化MPP JPEG编码器
    ret = mpp_create(&ctx, &mpi);
    if (ret != MPP_OK) {
        printf("   MPP创建失败: %d\n", ret);
    }else{
        printf("   MPP创建成功!   \n");
    }
    
    ret = mpp_init(ctx, MPP_CTX_ENC, MPP_VIDEO_CodingMJPEG);
    if (ret != MPP_OK) {
        printf("   MPP初始化失败: %d\n", ret);
        mpp_destroy(ctx);

    }else{
        printf("   MPP初始化成功!   \n");
    }
    // 配置编码参数
    mpp_enc_cfg_init(&cfg);
    ret = mpp_enc_cfg_init(&cfg);
    if (ret != MPP_OK) {
        printf("   ❌ 编码配置初始化失败: %d\n", ret);
        mpp_destroy(ctx);

    }
    ret = mpi->control(ctx, MPP_ENC_GET_CFG, cfg);
    mpp_enc_cfg_set_s32(cfg, "prep:width", width1);
    mpp_enc_cfg_set_s32(cfg, "prep:height", height1);
    mpp_enc_cfg_set_s32(cfg, "prep:hor_stride",width1);
    mpp_enc_cfg_set_s32(cfg, "prep:ver_stride",height1);
    mpp_enc_cfg_set_s32(cfg, "prep:format", MPP_FMT_YUV420SP);  // NV12格式
    mpp_enc_cfg_set_s32(cfg, "jpeg:q_factor", JPEG_QUALITY);    // JPEG质量
    mpp_enc_cfg_set_s32(cfg, "jpeg:qf_max", 99);    
    mpp_enc_cfg_set_s32(cfg, "jpeg:qf_min", 1);      
    mpp_enc_cfg_set_s32(cfg, "rc:mode", MPP_ENC_RC_MODE_FIXQP); 
    ret = mpi->control(ctx, MPP_ENC_SET_CFG, cfg);
    if (ret) {
        printf("   ❌ 编码器配置失败: %d\n", ret);
        mpp_enc_cfg_deinit(cfg);
        mpp_destroy(ctx);

    }
    printf("   ✅ 编码器配置成功!\n");
    // 创建MPP内存池
    ret = mpp_buffer_group_get_internal(&group, MPP_BUFFER_TYPE_ION);
    if (ret != MPP_OK) {
        printf("   MPP内存池创建失败: %d\n", ret);
        mpp_destroy(ctx);

    }else{
        printf("   MPP内存池创建成功！\n");
    }

    ret =mpp_buffer_group_limit_config(group,buffer_size,2);
    if (ret!=MPP_OK){
        printf("   MPP内存池限制失败，请重试！\n");

    }else{
        printf("   MPP内存池限制成功！\n");
    }
    //开始处理摄像头图像
    printf("   ==处理摄像头图像IMAGE...==\n" );
    MppBuffer buffer = NULL;
    MppBuffer buffer2 =NULL;
    MppPacket packet =NULL;
    MppFrame  frame =NULL;
    // 给帧分配MPP缓冲区
    ret = mpp_buffer_get(group, &buffer, buffer_size);
    if (ret != MPP_OK || !buffer) {
        printf("   MPP缓冲区分配失败");
        printf("   问题是：ret=%d,buffer=%p",ret,buffer);
    }else{
        printf("   MPP帧缓冲区分配成功!\n");
    }
    // 给packet分配缓冲区
    ret = mpp_buffer_get(group,&buffer2,buffer_size);
    if (ret != MPP_OK || !buffer2) {
        printf("   MPP_packet缓冲区分配失败\n");
        printf("   问题是：ret=%d,buffer2=%p",ret,buffer2);
    }else{
        printf("   MPP_packet缓冲区分配成功!\n");
    }
    void* mpp_ptr = mpp_buffer_get_ptr(buffer);
    if (!mpp_ptr) {
        printf("   无法获取MPP缓冲区指针\n");
        mpp_buffer_put(buffer);
    }else{
        printf("   成功获取MPP缓冲区指针\n");
        printf("   mpp_ptr的地址为：%p\n",mpp_ptr);
    }
    void *mpp_ptr2=mpp_buffer_get_ptr(buffer2);
    if (!mpp_ptr2) {
        printf("   无法获取MPP缓冲区指针\n");
        mpp_buffer_put(buffer);
    }else{
        printf("   成功获取MPP缓冲区指针\n");
        printf("   mpp_ptr2的地址为：%p\n",mpp_ptr2);
    }

    printf("   ===读取YUV数据!===\n");

    mpp_buffer_sync_begin(buffer);
    memcpy(mpp_ptr, yuv_data, YUV_SIZE);
    printf("   已成功将yuv数据放进MPPbuffer!\n");
    mpp_buffer_sync_end(buffer);
    // 准备MPP帧
    mpp_frame_init(&frame);
    mpp_frame_set_buffer(frame, buffer);
    mpp_frame_set_width(frame, width1);
    mpp_frame_set_height(frame, height1);
    mpp_frame_set_hor_stride(frame, height1);
    mpp_frame_set_ver_stride(frame,width1);
    mpp_frame_set_fmt(frame, MPP_FMT_YUV420SP);
    mpp_frame_set_eos(frame, 1);  // 设置结束标志
    printf("   准备MPP帧成功！\n");
    MppMeta meta = mpp_frame_get_meta(frame);
    ret=mpp_packet_init_with_buffer(&packet, buffer2);
    if (ret != MPP_OK) {
        printf("   ❌ 包初始化失败: %d\n", ret);
        mpp_frame_deinit(&frame);
        mpp_buffer_put(buffer);
        mpp_buffer_put(buffer2);
    }
    mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);
    mpp_packet_set_length(packet, 0);
    // JPEG编码
    //ret=mpi->encode(ctx,frame,packet);//目前暂未开发
    printf("   准备进行帧提取！\n");
    ret=mpi->encode_put_frame(ctx, frame);
    if (ret != MPP_OK) {
        printf("   获取帧失败: %d \n", ret);
        mpp_buffer_put(buffer);
        mpp_frame_deinit(&frame);
    }else{
        printf("   获取帧成功！\n\n");
        
    }
    printf("   准备进行包提取！\n");
    ret=mpi->encode_get_packet(ctx,packet);
    if (ret != MPP_OK) {
        printf("   获取包失败: %d \n", ret);
        mpp_buffer_put(buffer);
        mpp_frame_deinit(&frame);
    }else{
        printf("   获取包成功！\n\n");
        
    }
    void* jpeg_data = mpp_packet_get_data(packet);
    printf("   packet指针的位置在：%p\n",jpeg_data);
    size_t jpeg_size = mpp_packet_get_length(packet);
    printf("   此次JPEG图像大小为：%ld\n",jpeg_size);
    ret=write_data_to_file(output_file, jpeg_data, jpeg_size);
    // 保存JPEG文件
    if (!ret) {
        printf("   ✅ 保存成功\n\n\n");
    } else {
        printf("   ❌❌ 保存失败\n\n");
    }
    // 清理映射内存资源
    if (yuv_data != NULL && yuv_data != MAP_FAILED) {
    if (munmap(yuv_data, YUV_SIZE) == -1) {
        perror("munmap 失败");
    } else {
        printf("YUV 数据映射内存释放成功\n");
    }
    }
    mpp_packet_deinit(&packet);
    mpp_frame_deinit(&frame);
    mpp_buffer_put(buffer);
    mpp_buffer_put(buffer2);
    // 清理MPP资源
    ret=mpp_enc_cfg_deinit(cfg);
    if (ret!=MPP_OK)
    {
        printf("   编码配置释放失败\n");
    }else{
        printf("   编码配置释放成功！\n");
    }
    ret=mpp_buffer_group_put(group);
    if (ret!=MPP_OK)
    {
        printf("   缓冲组释放失败\n");
    }else{
        printf("   缓冲组释放成功！\n");
    }
    ret=mpp_destroy(ctx);
    if (ret!=MPP_OK)
    {
        printf("   MPP释放失败\n");
    }else{
        printf("   MPP释放成功！\n");

    }
}
