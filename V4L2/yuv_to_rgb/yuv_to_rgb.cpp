#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <signal.h>
#include <ctime>
#include<iostream>
using namespace std;



int fd_fb;
char* pfb;
int fd_video;
char yuyv[640*480*2];
char rgb[640*480*3];
struct buffer* buffers;//start, length

// 定义一个结构体来映射每个缓冲帧
struct buffer
{
void* start;
unsigned int length;
};

// 信号函数 输入ctrl+c后执行资源回收
void mysignal(int signo)  
{
printf("success\n");
int i;
for (i = 0; i < 4; i++)
{
munmap(buffers[i].start, buffers[i].length);  // 解除映射
}
free(buffers);
munmap(pfb, 1920*1080*4);
close(fd_video);
close(fd_fb);
exit(0);
}

// 图像处理
void process_image(struct buffer bb)
{
int i, j;
int y0, u0, y1, v0;
int r, g, b;
memcpy(yuyv, bb.start, bb.length);
//保存原始yuyv数据

//把yuyv转为rgb数据
for (i = 0; i < 640*480/2 ; i++)
{
y0 = yuyv[i*4];
y1 = yuyv[i*4+2];
u0 = yuyv[i*4+1];
v0 = yuyv[i*4+3];

//第i*2个点
b = (1.164383 * (y0 - 16) + 2.017232*(u0 - 128));
if (b > 255) b = 255;
if (b < 0) b = 0;
g= 1.164383 * (y0 - 16) - 0.391762*(u0 - 128) - 0.812968*(v0 - 128);
if (g > 255) g = 255;
if (g < 0) g = 0;
r= 1.164383 * (y0 - 16) + 1.596027*(v0 - 128);
if (r > 255) r = 255;
if (r < 0) r = 0;
rgb[i*3*2 + 0] = b;
rgb[i*3*2 + 1] = g;
rgb[i*3*2 + 2] = r;

//第i*2+1个点
b = (1.164383 * (y1 - 16) + 2.017232*(u0 - 128));
if (b > 255) b = 255;
if (b < 0) b = 0;
g= 1.164383 * (y1 - 16) - 0.391762*(u0 - 128) - 0.812968*(v0 - 128);
if (g > 255) g = 255;
if (g < 0) g = 0;
r= 1.164383 * (y1 - 16) + 1.596027*(v0 - 128);
if (r > 255) r = 255;
if (r < 0) r = 0;
rgb[i*3*2 + 3] = b;
rgb[i*3*2 + 4] = g;
rgb[i*3*2 + 5] = r;
}

for ( i = 0; i < 480; i++)
{
for(j=0; j<80; j++)
{
*(pfb+(i*1920+j)*4 + 0) = 0x00;
*(pfb+(i*1920+j)*4 + 1) = 0x00;
*(pfb+(i*1920+j)*4 + 2) = 0x00;
}
for (j = 80; j < 720; j++)//????????640
{
*(pfb+(i*1920+j)*4 + 0) = rgb[(i*640+j-80)*3 + 0];
*(pfb+(i*1920+j)*4 + 1) = rgb[(i*640+j-80)*3 + 1];
*(pfb+(i*1920+j)*4 + 2) = rgb[(i*640+j-80)*3 + 2];
}
for(j=720; j<1920; j++)
{
*(pfb+(i*1920+j)*4 + 0) = 0x00;
*(pfb+(i*1920+j)*4 + 1) = 0x00;
*(pfb+(i*1920+j)*4 + 2) = 0x00;
}
}
}

int main()
{
    //1.打开摄像头设备 O_RDWR可读可写
    fd_video = open("/dev/video10", O_RDWR);
    if (fd_video < 0)
    {
        perror("video10 open");
        return fd_video;
    }

    //2.打开屏幕设备
    fd_fb = open("/dev/fb0", O_RDWR);
    if (fd_fb < 0)
    {
        perror("fb open");
        return fd_fb;
    }

    // 3.查看设备功能
    struct v4l2_capability cap;
   
	if (ioctl(fd_video, VIDIOC_QUERYCAP, &cap) < 0){
		perror("requre VIDIOC_QUERYCAP fialed! \n");
		return -1;
	}
    cout << "Driver Name: " << cap.driver << endl;
    cout << "Card Name: " << cap.card << endl;
    cout << "Bus info: " << cap.bus_info << endl;
    printf("Driver Version:%u.%u.%u\n",cap.capabilities);
    cout << endl;

    //  4.查询设备支持哪种视频格式
    struct v4l2_fmtdesc fmt;       //查询设备格式所用结构体
    memset(&(fmt), 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.index = 0;
    int ret;
    // VIDIOC_ENUM_FMT: 获取当前驱动支持的视频格式
    ret = ioctl(fd_video,VIDIOC_ENUM_FMT,&fmt);
    printf("显示视频所支持的格式format:%s\n",fmt.description);//成功可显示视频所支持的格式
    cout << endl;

    // 5.设置视频帧的格式
    struct v4l2_format s_fmt;
    memset(&(fmt), 0, sizeof(fmt));
    s_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    s_fmt.fmt.pix.width = 1920;      
    s_fmt.fmt.pix.height = 1080;
    s_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  // 采样类型
    //s_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;//4 

    // 设置图片格式，VIDIOC_S_FMT:设置当前驱动的频捕获格式 
    if(ioctl(fd_video,VIDIOC_S_FMT,&s_fmt) != 0)
    {
        printf("set format error\n");
        return -1;
    }

    // 得到图片格式
    if(ioctl(fd_video, VIDIOC_G_FMT, &s_fmt) == -1){
		perror("set format failed!");
		return -1;
	}
    cout << "实际得到的图片格式如下："<< endl;
	printf("fmt.type:\t\t%d\n",s_fmt.type);
    // %c表示输出单个字符
	printf("pix.pixelformat:\t%c%c%c%c\n", \
			s_fmt.fmt.pix.pixelformat & 0xFF,\
			(s_fmt.fmt.pix.pixelformat >> 8) & 0xFF, \
			(s_fmt.fmt.pix.pixelformat >> 16) & 0xFF,\
			(s_fmt.fmt.pix.pixelformat >> 24) & 0xFF);
	printf("pix.width:\t\t%d\n",s_fmt.fmt.pix.width);
	printf("pix.height:\t\t%d\n",s_fmt.fmt.pix.height);
	printf("pix.field:\t\t%d\n",s_fmt.fmt.pix.field);

    // 将显示器设备的文件描述符映射进内存，让程序能直接访问设备内存
    //  PROT_WRITE|PROT_READ ： 可读可写
    pfb = (char *)mmap(NULL, 1920*1080*4, PROT_WRITE|PROT_READ,MAP_SHARED, fd_fb, 0);
    if (pfb == NULL)
    {
        perror ("mmap");
        return -1;
    }  

    //此处作用为申请缓冲区
    struct v4l2_requestbuffers req;
    memset(&(fmt), 0, sizeof(fmt));
    req.count=4;    	//申请一个拥有四个缓冲帧的缓冲区
    req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory=V4L2_MEMORY_MMAP;
    //  VIDIOC_REQBUFS：分配内存
    ioctl(fd_video,VIDIOC_REQBUFS,&req);


    //申请4个struct buffer空间
    buffers = (struct buffer*)calloc (req.count, sizeof (struct buffer));
    if (!buffers)
    {
        perror ("Out of memory");
        exit (EXIT_FAILURE);
    }
    unsigned int n_buffers;
    for (n_buffers = 0; n_buffers < req.count; n_buffers++)
    {
         struct v4l2_buffer buf;  // 驱动中的一帧图像缓存
         memset(&buf,0,sizeof(buf));
         buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_MMAP;
         buf.index = n_buffers;
         // 把VIDIOC_REQBUFS中分配的数据缓存转换成物理地址
         if (-1 == ioctl (fd_video, VIDIOC_QUERYBUF, &buf))
            exit(-1);
         buffers[n_buffers].length = buf.length;
         buffers[n_buffers].start = mmap (NULL,
         buf.length,PROT_READ | PROT_WRITE ,MAP_SHARED,fd_video, buf.m.offset);
         // 失败时，mmap()返回MAP_FAILED[其值为(void *)-1]
         if (MAP_FAILED == buffers[n_buffers].start)
         exit(-1);
    }

    enum v4l2_buf_type type;
    int i;
    for (i = 0; i < 4; ++i)
    {
    struct v4l2_buffer buf;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    // VIDIOC_QBUF: 把数据放入缓存队列
    ioctl (fd_video, VIDIOC_QBUF, &buf);
    }
    signal(SIGINT,mysignal);
    while(1)
    {
        //开始捕获图像
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        // 开始视频显示函数
        ioctl (fd_video, VIDIOC_STREAMON, &type);

        struct v4l2_buffer buf;
        memset(&(buf), 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
         //取出图像数据,把数据从缓存中读取出来
        ioctl (fd_video, VIDIOC_DQBUF, &buf);
        //在LCD屏幕上显示图像
        process_image (buffers[buf.index]); //YUYV
        
        //告知buf可以使用
        ioctl (fd_video,VIDIOC_QBUF,&buf);
    }
}