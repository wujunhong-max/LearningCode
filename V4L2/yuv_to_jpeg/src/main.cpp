#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>
#include <stdlib.h>
#include "yuv_to_jpeg.h"
 
int image_height;
int image_width;
unsigned char *image_buffer[4];
int uvc_video_fd;
 
/*
函数功能: 摄像头设备初始化
*/
int VideoDeviceInit(char *DEVICE_NAME)
{
	/*1. 打开摄像头设备*/
	uvc_video_fd=open(DEVICE_NAME,O_RDWR);
	if(uvc_video_fd<0)
	{
		printf("%s 摄像头设备打开失败!\n",DEVICE_NAME);
		return -1;
	}
	else
	{
		printf("摄像头打开成功.\n");
	}
	
	/*2 设置摄像头的属性*/
	struct v4l2_format format;
	/*2.1 查询当前摄像头支持的格式*/
	//当前视频设备支持的视频图像格式
	struct v4l2_fmtdesc fmt;
	memset(&fmt,0,sizeof(fmt));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	printf("当前摄像头支持输出的图像格式如下:\n");
	while(ioctl(uvc_video_fd,VIDIOC_ENUM_FMT,&fmt) == 0)
	{
		fmt.index ++ ;
		printf("<'%c%c%c%c'--'%s'>\n",
				fmt.pixelformat & 0xff,(fmt.pixelformat >> 8)&0xff,
				(fmt.pixelformat >> 16) & 0xff,(fmt.pixelformat >> 24)&0xff,
				fmt.description);
	}
 
	/*2.2 设置摄像头输出的宽度高度与颜色格式(V4L2_PIX_FMT_YUYV)*/
	memset(&format,0,sizeof(struct v4l2_format));
	format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*表示视频捕获设备*/
	format.fmt.pix.width=2000;  /*预设的宽度*/
	format.fmt.pix.height=2000; /*预设的高度*/
	format.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV; /*预设的格式*/
	format.fmt.pix.field=V4L2_FIELD_ANY; /*系统自动设置: 帧属性*/
	if(ioctl(uvc_video_fd,VIDIOC_S_FMT,&format)) /*设置摄像头的属性*/
	{
		printf("摄像头格式设置失败!\n");
		return -2;
	}
	image_width=format.fmt.pix.width;
	image_height=format.fmt.pix.height;
	printf("摄像头实际输出的图像尺寸:x=%d,y=%d\n",format.fmt.pix.width,format.fmt.pix.height);
	if(format.fmt.pix.pixelformat==V4L2_PIX_FMT_YUYV)
	{
		printf("当前摄像头支持YUV格式图像输出!\n");
	}
	else
	{
		printf("当前摄像头不支持YUV格式图像输出!\n");
		return -3;
	}
	
	/*2.3 设置摄像头采集的帧率*/
	struct v4l2_streamparm streamparm;
	streamparm.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*表示视频捕获设备*/
	streamparm.parm.capture.timeperframe.numerator=1;
	streamparm.parm.capture.timeperframe.denominator=30;
	printf("设置当前摄像头采集帧率: %d秒%d帧\n",streamparm.parm.capture.timeperframe.numerator,streamparm.parm.capture.timeperframe.denominator);
	if(ioctl(uvc_video_fd,VIDIOC_S_PARM,&streamparm)) /*设置摄像头的帧率*/
	{
		printf("设置摄像头采集的帧率失败!\n");
		return -3;
	}
	if(ioctl(uvc_video_fd,VIDIOC_S_PARM,&streamparm)) /*获取摄像头的帧率*/
	{
		printf("获取摄像头采集的帧率失败!\n");
		return -3;
	}
	printf("当前摄像头实际采集帧率: %d秒%d帧\n",streamparm.parm.capture.timeperframe.numerator,streamparm.parm.capture.timeperframe.denominator);
	
	/*3. 请求缓冲区: 申请摄像头数据采集的缓冲区*/
	struct v4l2_requestbuffers req_buff;
	memset(&req_buff,0,sizeof(struct v4l2_requestbuffers));
	req_buff.count=4; /*预设要申请4个缓冲区*/
	req_buff.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
	req_buff.memory=V4L2_MEMORY_MMAP; /*支持mmap内存映射*/
	if(ioctl(uvc_video_fd,VIDIOC_REQBUFS,&req_buff)) /*申请缓冲区*/
	{
		printf("申请摄像头数据采集的缓冲区失败!\n");
		return -4;
	}
	printf("摄像头缓冲区申请的数量: %d\n",req_buff.count);
 
	/*4. 获取缓冲区的详细信息: 地址,编号*/
	struct v4l2_buffer buff_info;
	memset(&buff_info,0,sizeof(struct v4l2_buffer));
	int i;
	for(i=0;i<req_buff.count;i++)
	{
		buff_info.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
		buff_info.memory=V4L2_MEMORY_MMAP; /*支持mmap内存映射*/
		if(ioctl(uvc_video_fd,VIDIOC_QUERYBUF,&buff_info)) /*获取缓冲区的详细信息*/
		{
			printf("获取缓冲区的详细信息失败!\n");
			return -5;
		}
		/*根据摄像头申请缓冲区信息: 使用mmap函数将内核的地址映射到进程空间*/
		image_buffer[i]=(unsigned char*)mmap(NULL,buff_info.length,PROT_READ|PROT_WRITE,MAP_SHARED,uvc_video_fd,buff_info.m.offset); 
		if(image_buffer[i]==NULL)
		{
			printf("缓冲区映射失败!\n");
			return -6;
		}
	}
 
	/*5. 将缓冲区放入采集队列*/
	memset(&buff_info,0,sizeof(struct v4l2_buffer));
	for(i=0;i<req_buff.count;i++)
	{
		buff_info.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
		buff_info.index=i; /*缓冲区的节点编号*/
		buff_info.memory=V4L2_MEMORY_MMAP; /*支持mmap内存映射*/
		if(ioctl(uvc_video_fd,VIDIOC_QBUF,&buff_info)) /*根据节点编号将缓冲区放入队列*/
		{
			printf("根据节点编号将缓冲区放入队列失败!\n");
			return -7;
		}
	}
 
	/*6. 启动摄像头数据采集*/
	int Type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(uvc_video_fd,VIDIOC_STREAMON,&Type))
	{
		printf("启动摄像头数据采集失败!\n");
		return -8;
	}
	return 0;
}
 
int main(int argc,char **argv)
{
	if(argc!=2)
	{
		printf("./app </dev/videoX>\n");
		return 0;
	}
	int err;
	char jpg_file_name[100]; /*存放JPG图片名称*/
	int jpg_cnt=0;
	FILE *jpg_file;
	int jpg_size;
	
	/*1. 初始化摄像头设备*/
	err=VideoDeviceInit(argv[1]);
	printf("VideoDeviceInit=%d\n",err);
 
	/*2. 循环读取摄像头采集的数据*/
	struct pollfd fds;
	fds.fd=uvc_video_fd;
	fds.events=POLLIN;
 
	/*3. 申请存放JPG的数据空间*/
	unsigned char *jpg_p=(unsigned char*)calloc(1,image_height*image_width*3);
	
	struct v4l2_buffer video_buffer;
	while(1)
	{
		 /*(1)等待摄像头采集数据*/
		 poll(&fds,1,-1);
		 /*(2)取出队列里采集完毕的缓冲区*/
		 video_buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
		 video_buffer.memory=V4L2_MEMORY_MMAP;
		 ioctl(uvc_video_fd,VIDIOC_DQBUF,&video_buffer);
         /*(3)处理图像数据*/
	     printf("image_buffer[%d]=%X\n",video_buffer.index,image_buffer[video_buffer.index]);
    
		 /*YUV数据转JPEG格式*/
		 jpg_size=yuv_to_jpeg(image_width,image_height,image_height*image_width*3,image_buffer[video_buffer.index],jpg_p,80);
		 sprintf(jpg_file_name,"%d.jpg",jpg_cnt++);
		 printf("图片名称:%s,字节大小:%d\n",jpg_file_name,jpg_size);
		 
		 jpg_file=fopen(jpg_file_name,"wb");
		 fwrite(jpg_p,1,jpg_size,jpg_file);
		 fclose(jpg_file);
		 
		 /*(4)将缓冲区再放入队列*/
		 ioctl(uvc_video_fd,VIDIOC_QBUF,&video_buffer);
	}
	return 0;
}