#include "camera_test.h"
#include "./minuitwrp/minui.h"
#include "test_case.h"
#define VIDEO_DEV_NAME   "/dev/video0"
#define PMEM_DEV_NAME    "/dev/pmem_cam"
#define DISP_DEV_NAME    "/dev/graphics/fb1"
#define ION_DEVICE          "/dev/ion"

#define FBIOSET_ENABLE			0x5019	


#define CAM_OVERLAY_BUF_NEW  1
#define RK29_CAM_VERSION_CODE_1 KERNEL_VERSION(0, 0, 1)
#define RK29_CAM_VERSION_CODE_2 KERNEL_VERSION(0, 0, 2)

static void *m_v4l2Buffer[4];
static int v4l2Buffer_phy_addr = 0;
static int iCamFd, iPmemFd, iDispFd =-1;
static int preview_w,preview_h;

static char videodevice[20] ={0};
static struct v4l2_capability mCamDriverCapability;
static unsigned int pix_format;

static void* vaddr = NULL;
static volatile int isstoped = 0;
static int hasstoped = 1;
enum {
	FD_INIT = -1,
};

static int iIonFd = -1;
struct ion_allocation_data ionAllocData;
struct ion_fd_data fd_data;
struct ion_handle_data handle_data;
#define RK30_PLAT 1
#define RK29_PLAT 0
static int is_rk30_plat = RK30_PLAT;
#define  FB_NONSTAND ((is_rk30_plat == RK29_PLAT)?0x2:0x20)
static int cam_id = 0;

static int camera_x=0,camera_y=0,camera_w=0,camera_h=0,camera_num=0;
static struct testcase_info *tc_info = NULL;
//added by zyl 2013/09/16
#define CAMERA_IS_UVC_CAMERA()  (strcmp((char*)&mCamDriverCapability.driver[0],"uvcvideo") == 0)
char *mCamDriverV4l2Buffer[16];
unsigned int mCamDriverV4l2BufferLen;
struct pmem_region sub;

typedef char bool;

#define true 1

#define false 0





int cameraFormatConvert(int v4l2_fmt_src, int v4l2_fmt_dst, const char *android_fmt_dst, 
                            char *srcbuf, char *dstbuf,int srcphy,int dstphy,
                            int src_w, int src_h, int srcbuf_w,
                            int dst_w, int dst_h, int dstbuf_w,
                            bool mirror);




int Camera_Click_Event(int x,int y)
{	
	struct list_head *pos;
	int x_start,x_end;
	int y_start,y_end;
	int err;

	if(tc_info == NULL)
		return -1;		

	if(camera_num < 2)
		return -1;
	
	get_camera_size();

	x_start = camera_x;
	x_end = x_start + camera_w;
	y_start = camera_y;
	y_end = y_start + camera_h;

	if( (x >= x_start) && (x <= x_end) && (y >= y_start) && (y <= y_end))
	{
		
		printf("Camera_Click_Event : change \r\n");	
		stopCameraTest();
		startCameraTest();
	}
		
	return 0;
	
}


int CameraCreate(void)
{
    int err,size;
	struct v4l2_format format;

    if (iCamFd == 0) {
        iCamFd = open(videodevice, O_RDWR|O_CLOEXEC);
       
        if (iCamFd < 0) {
            printf(" Could not open the camera  device:%s\n",videodevice);
    		err = -1;
            goto exit;
        }

        memset(&mCamDriverCapability, 0, sizeof(struct v4l2_capability));
        err = ioctl(iCamFd, VIDIOC_QUERYCAP, &mCamDriverCapability);
        if (err < 0) {
        	printf("Error opening device unable to query device.\n");
    	    goto exit;
        } 
		if(strstr((char*)&mCamDriverCapability, "front") != NULL){
			printf("it is a front camera! \n");
			}
		else if(strstr((char*)&mCamDriverCapability, "back") != NULL){
			printf("it is a back camera! \n"); 
		}
		//modified by zyl 2013/09/16
		else if (CAMERA_IS_UVC_CAMERA()){
			printf("it is a usb camera! \n");

			if(camera_w>=640 && camera_h>=480)
			{
				camera_w = 640;
				camera_h = 480;
			}
			else
			{
				camera_w = 320;
				camera_h = 240; 	
			}
			#if 0
			struct v4l2_frmsizeenum fsize;				  
			memset(&fsize, 0, sizeof(fsize));		  
			fsize.index = 0;	   
			fsize.pixel_format = V4L2_PIX_FMT_YUYV;

			while ((err = ioctl(iCamFd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0) {
			if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { 				
					printf("(%dx%d)\n",fsize.discrete.width, fsize.discrete.height);
				}
				else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {

					break;
				} else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
					
					break;
				}
				fsize.index++;

			}
			#endif
			
		}
        if (mCamDriverCapability.version == RK29_CAM_VERSION_CODE_1) {
            pix_format = V4L2_PIX_FMT_YUV420;
            printf("Current camera driver version: 0.0.1 \n");    
        } else 
        { 
            pix_format = V4L2_PIX_FMT_NV12;
            printf("Current camera driver version: %d.%d.%d \n",                
                (mCamDriverCapability.version>>16) & 0xff,(mCamDriverCapability.version>>8) & 0xff,
                mCamDriverCapability.version & 0xff); 
        }
        if (CAMERA_IS_UVC_CAMERA()){
			pix_format = V4L2_PIX_FMT_YUYV;
			printf("pix_format is set to V4L2_PIX_FMT_YUYV\n");
		}
    }
    if(access("/sys/module/rk29_camera_oneframe", O_RDWR) >=0 ){
        is_rk30_plat =  RK29_PLAT;
        printf("it is rk29 platform!\n");
    }else if(access("/sys/module/rk30_camera_oneframe", O_RDWR) >=0){
        printf("it is rk30 platform!\n");
    }else{
        printf("default as rk30 platform\n");
    }
    if(v4l2Buffer_phy_addr !=0)
		goto suc_alloc;
    if(access(PMEM_DEV_NAME, O_RDWR) < 0) {
            iIonFd = open(ION_DEVICE, O_RDONLY|O_CLOEXEC);

            if(iIonFd < 0 ) {
                printf("%s: Failed to open ion device - %s",
                        __FUNCTION__, strerror(errno));
                iIonFd = -1;
        		err = -1;
                goto exit1;
            }
            ionAllocData.len = 0x300000;
            ionAllocData.align = 4*1024;
            ionAllocData.flags = 1 << 0;
              err = ioctl(iIonFd, ION_IOC_ALLOC, &ionAllocData);
            if(err) {
                printf("%s: ION_IOC_ALLOC failed to alloc 0x%x bytes with error - %s", 
        			__FUNCTION__, ionAllocData.len, strerror(errno));
                
        		err = -errno;
                goto exit2;
            }

            fd_data.handle = ionAllocData.handle;
            handle_data.handle = ionAllocData.handle;

            err = ioctl(iIonFd, ION_IOC_MAP, &fd_data);
            if(err) {
                printf("%s: ION_IOC_MAP failed with error - %s",
                        __FUNCTION__, strerror(errno));
                ioctl(iIonFd, ION_IOC_FREE, &handle_data);
        		err = -errno;
               goto exit2;
            }
            m_v4l2Buffer[0] = mmap(0, ionAllocData.len, PROT_READ|PROT_WRITE,
                    MAP_SHARED, fd_data.fd, 0);
            if(m_v4l2Buffer[0] == MAP_FAILED) {
                printf("%s: Failed to map the allocated memory: %s",
                        __FUNCTION__, strerror(errno));
                err = -errno;
                ioctl(iIonFd, ION_IOC_FREE, &handle_data);
                goto exit2;
            }
        	err = ioctl(fd_data.fd, PMEM_GET_PHYS, &sub);
        	if (err < 0) {
            	printf(" PMEM_GET_PHY_ADDR failed, limp mode\n");
                ioctl(iIonFd, ION_IOC_FREE, &handle_data);
                goto exit2;
        	}
              err = ioctl(iIonFd, ION_IOC_FREE, &handle_data);
              
        	if(err){
        		printf("%s: ION_IOC_FREE failed with error - %s",
                        __FUNCTION__, strerror(errno));
        		err = -errno;
        	}else
            	printf("%s: Successfully allocated 0x%x bytes, mIonFd=%d, SharedFd=%d",
            			__FUNCTION__,ionAllocData.len, iIonFd, fd_data.fd);
        }
	else{
            iPmemFd = open(PMEM_DEV_NAME, O_RDWR|O_CLOEXEC, 0);
            if (iPmemFd < 0) {
            	printf(" Could not open pmem device(%s)\n",PMEM_DEV_NAME);
        		err = -1;
                goto exit1;
            }

        	size = sub.len = 0x300000; 
        	m_v4l2Buffer[0] =(unsigned char *) mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, iPmemFd, 0);
        	if (m_v4l2Buffer[0] == MAP_FAILED) {
            	printf(" m_v4l2Buffer[0] mmap failed\n");
        		err = -1;
                goto exit2;
        	}
        	err = ioctl(iPmemFd, PMEM_GET_PHYS, &sub);
        	if (err < 0) {
            	printf(" PMEM_GET_PHY_ADDR failed, limp mode\n");
                goto exit3;
        	}
        }
    memset(m_v4l2Buffer[0], 0x00, size);
	v4l2Buffer_phy_addr = sub.offset;
suc_alloc:   
          err = ioctl(iCamFd, VIDIOC_QUERYCAP, &mCamDriverCapability);
        if (err < 0) {
        	printf("Error opening device unable to query device.\n");
    	    goto exit;
        }  
    return 0;

exit3:
	munmap(m_v4l2Buffer[0], size);
exit2:
    if(iPmemFd > 0){
    	close(iPmemFd);
    	iPmemFd = -1;
        }
    if(iIonFd > 0){
    	close(iIonFd);
    	iIonFd = -1;
        }
exit1:
exit:
    return err;
}

int CameraStart(int phy_addr, int buffer_count, int w, int h)
{
    int err,i;
    int nSizeBytes;
    struct v4l2_format format;
    enum v4l2_buf_type type;
    struct v4l2_requestbuffers creqbuf;
		
	//buffer_count = 2;
	if( phy_addr == 0 || buffer_count == 0  ) {
    	printf(" Video Buf is NULL\n");
		goto  fail_bufalloc;
    }

	/* Set preview format */
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = w;
	format.fmt.pix.height = h;
	format.fmt.pix.pixelformat = pix_format;
	format.fmt.pix.field = V4L2_FIELD_NONE;	
	err = ioctl(iCamFd, VIDIOC_S_FMT, &format);
	if ( err < 0 ){
		printf(" Failed to set VIDIOC_S_FMT\n");
		goto exit1;
	}

	preview_w = format.fmt.pix.width;
	preview_h = format.fmt.pix.height;	
	//zyl 2013/09/16
	if(CAMERA_IS_UVC_CAMERA())
	{
		#if 0
		struct v4l2_streamparm setfps;			
		
		memset(&setfps, 0, sizeof(struct v4l2_streamparm));
		setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		setfps.parm.capture.timeperframe.numerator=1;
		setfps.parm.capture.timeperframe.denominator=params.getPreviewFrameRate();
		err = ioctl(iCamFd, VIDIOC_S_PARM, &setfps); 
		if (err != 0) {
			LOGE ("%s(%d): Set framerate(%d fps) failed",__FUNCTION__,__LINE__,params.getPreviewFrameRate());
			return err;
		} else {
			LOGD ("%s(%d): Set framerate(%d fps) success",__FUNCTION__,__LINE__,params.getPreviewFrameRate());
		}
		#endif

		creqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	    creqbuf.memory = V4L2_MEMORY_MMAP;
	    creqbuf.count  =  buffer_count /*- 1*/ ; //We will use the last buffer for snapshots.	

		if (ioctl(iCamFd, VIDIOC_REQBUFS, &creqbuf) < 0) {
			printf("%s VIDIOC_REQBUFS Failed\n",__FUNCTION__);
			goto fail_reqbufs;
		}
		printf("creqbuf.count = %d\n",creqbuf.count);

		for (i = 0; i < (int)creqbuf.count; i++) {
		
			struct v4l2_buffer buffer;
			buffer.type = creqbuf.type;
			buffer.memory = creqbuf.memory;
			buffer.index = i;
		
			if (ioctl(iCamFd, VIDIOC_QUERYBUF, &buffer) < 0) {
				printf("%s VIDIOC_QUERYBUF Failed\n",__FUNCTION__);
				goto fail_loop;
			}
		
		
		m_v4l2Buffer[i] =(void*)((int)m_v4l2Buffer[0] + i*buffer.length);

			if (buffer.memory == V4L2_MEMORY_MMAP) {
					mCamDriverV4l2Buffer[i] = (char*)mmap(0 /* start anywhere */ ,
										buffer.length, PROT_READ, MAP_SHARED, iCamFd,
										buffer.m.offset);
					if (mCamDriverV4l2Buffer[i] == MAP_FAILED) {
						 printf("%s CameraStart mmap Failed\n",__FUNCTION__);
						 goto fail_loop;
					} 
				}
			mCamDriverV4l2BufferLen = buffer.length;
			err = ioctl(iCamFd, VIDIOC_QBUF, &buffer);
			if (err < 0) {
				printf("%s CameraStart VIDIOC_QBUF Failed\n",__FUNCTION__);
				goto fail_loop;
			}			
			
		}

		
	}
	else 
	{
		creqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	    creqbuf.memory = V4L2_MEMORY_OVERLAY;
	    creqbuf.count  =  buffer_count /*- 1 */; //We will use the last buffer for snapshots.

	
	    if (ioctl(iCamFd, VIDIOC_REQBUFS, &creqbuf) < 0) {
	        printf("%s VIDIOC_REQBUFS Failed\n",__FUNCTION__);
	        goto fail_reqbufs;
	    }
		printf("creqbuf.count = %d\n",creqbuf.count);
		struct v4l2_buffer buffer;
	    for (i = 0; i < (int)creqbuf.count; i++) {

	       // struct v4l2_buffer buffer;
	        buffer.type = creqbuf.type;
	        buffer.memory = creqbuf.memory;
	        buffer.index = i;

	        if (ioctl(iCamFd, VIDIOC_QUERYBUF, &buffer) < 0) {
	            printf("%s VIDIOC_QUERYBUF Failed\n",__FUNCTION__);
	            goto fail_loop;
	        }

	        #if CAM_OVERLAY_BUF_NEW
	        buffer.m.offset = phy_addr + i*buffer.length;
	        #else
	        buffer.m.offset = phy_addr;
	        #endif

	        m_v4l2Buffer[i] =(void*)((int)m_v4l2Buffer[0] + i*buffer.length);
		//	memset(m_v4l2Buffer[i],0x0,buffer.length);
	        err = ioctl(iCamFd, VIDIOC_QBUF, &buffer);
	        if (err < 0) {
	            printf("%s CameraStart VIDIOC_QBUF Failed\n",__FUNCTION__);
	            goto fail_loop;
	        }
	    }

		//m_v4l2Buffer[i] = (void*)((int)m_v4l2Buffer[0] + i*buffer.length);
		
	}
	
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    err = ioctl(iCamFd, VIDIOC_STREAMON, &type);
    if ( err < 0) {
        printf("%s VIDIOC_STREAMON Failed\n",__FUNCTION__);
        goto fail_loop;
    }

    return 0;

fail_bufalloc:
fail_loop:
fail_reqbufs:

exit1:
    close(iCamFd);
	iCamFd = -1;
exit:
    return -1;
}



int DispCreate(int corx ,int cory,int preview_w,int preview_h )
{
	int err = 0;
	struct fb_var_screeninfo var;
	unsigned int panelsize[2];
	int x_phy,y_phy,w_phy,h_phy;
	int x_visual,y_visual,w_visual,h_visual;
	struct fb_fix_screeninfo finfo;
	struct color_key_cfg clr_key_cfg;

	int data[2];
	if(iDispFd !=-1)
		goto exit;
	iDispFd = open(DISP_DEV_NAME,O_RDWR, 0);
	if (iDispFd < 0) {
		printf("%s Could not open display device\n",__FUNCTION__);
		err = -1;
		goto exit;
	}
	if(ioctl(iDispFd, 0x5001, panelsize) < 0)
	{
		printf("%s Failed to get panel size\n",__FUNCTION__);
		err = -1;
		goto exit1;
	}
	if(panelsize[0] == 0 || panelsize[1] ==0)
	{
		panelsize[0] = preview_w;
		panelsize[1] = preview_h;
	}
	#if 0
	data[0] = v4l2Buffer_phy_addr;
	data[1] = (int)(data[0] + preview_w *preview_h);
	if (ioctl(iDispFd, 0x5002, data) == -1) 
	{
		printf("%s ioctl fb1 queuebuf fail!\n",__FUNCTION__);
		err = -1;
		goto exit;
	}
	#endif
	if (ioctl(iDispFd, FBIOGET_VSCREENINFO, &var) == -1) {
		printf("%s ioctl fb1 FBIOPUT_VSCREENINFO fail!\n",__FUNCTION__);
		err = -1;
		goto exit;
	}
	//printf("preview_w = %d,preview_h =%d,panelsize[1] = %d,panelsize[0] = %d\n",preview_w,preview_h,panelsize[1],panelsize[0]);
	//var.xres_virtual = preview_w;	//win0 memery x size
	//var.yres_virtual = preview_h;	 //win0 memery y size
	var.xoffset = 0;   //win0 start x in memery
	var.yoffset = 0;   //win0 start y in memery
	var.nonstd = ((cory<<20)&0xfff00000) + ((corx<<8)&0xfff00) +FB_NONSTAND; //win0 ypos & xpos & format (ypos<<20 + xpos<<8 + format)
	var.grayscale = ((preview_h<<20)&0xfff00000) + (( preview_w<<8)&0xfff00) + 0;	//win0 xsize & ysize
	var.xres = preview_w;	 //win0 show x size
	var.yres = preview_h;	 //win0 show y size
	var.bits_per_pixel = 16;
	var.activate = FB_ACTIVATE_FORCE;
	if (ioctl(iDispFd, FBIOPUT_VSCREENINFO, &var) == -1) {
		printf("%s ioctl fb1 FBIOPUT_VSCREENINFO fail!\n",__FUNCTION__);
		err = -1;
		goto exit;
	}
	
	clr_key_cfg.win0_color_key_cfg = 0;		//win0 color key disable
	clr_key_cfg.win1_color_key_cfg = 0x01000000; 	// win1 color key enable
	clr_key_cfg.win2_color_key_cfg = 0;  
	if (ioctl(iDispFd,RK_FBIOPUT_COLOR_KEY_CFG, &clr_key_cfg) == -1) {
                printf("%s set fb color key failed!\n",__FUNCTION__);
                err = -1;
        }

	return 0;
exit1:
	if (iDispFd > 0)
	{
		close(iDispFd);
		iDispFd = -1;
	}
exit:
	return err;
}
int TaskStop(void)
{
	struct v4l2_requestbuffers creqbuf;
    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    isstoped =1;
    while(!hasstoped){
    	sleep(1);
    	}
    if (ioctl(iCamFd, VIDIOC_STREAMOFF, &creqbuf.type) == -1) {
        printf("%s VIDIOC_STREAMOFF Failed\n", __FUNCTION__);
   //     return -1;
    }
	if (iDispFd > 0) {
		int disable = 0;
		printf("Close disp\n");
		ioctl(iDispFd, FBIOSET_ENABLE,&disable);
		close(iDispFd);
		iDispFd = -1;
	}
	if (iCamFd > 0) {
		close(iCamFd);
		iCamFd = 0;
	}
	printf("\n%s: stop ok!\n",__func__);
	return 0;
}
int TaskRuning(int fps_total,int corx,int cory)
{
	int err,fps;
	int data[2];
	struct v4l2_buffer cfilledbuffer1;
	int i ;
	struct fb_var_screeninfo var ;
	int fb_offset = 0;
	cfilledbuffer1.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	struct pmem_region tmpsub;

	//added by zyl 2013/09/16
	if(CAMERA_IS_UVC_CAMERA())
		cfilledbuffer1.memory = V4L2_MEMORY_MMAP;		
	else
		cfilledbuffer1.memory = V4L2_MEMORY_OVERLAY;
	cfilledbuffer1.reserved = 0;
	fps = 0;
	while (!isstoped)
	{
		if (ioctl(iCamFd, VIDIOC_DQBUF, &cfilledbuffer1) < 0)
		{
			printf("%s VIDIOC_DQBUF Failed!!! \n",__FUNCTION__);
			err = -1;
			goto exit;
		}		
		if (iDispFd > 0) 
		{

			if(CAMERA_IS_UVC_CAMERA())
			{
                cameraFormatConvert(pix_format,V4L2_PIX_FMT_YUV420,NULL,
                	(char*)mCamDriverV4l2Buffer[cfilledbuffer1.index],m_v4l2Buffer[cfilledbuffer1.index], 
                	0,0,
                	preview_w, preview_h,preview_w,
                	preview_w, preview_h,preview_w,
                	false);	
				#if 1
                //flush
                static struct ion_cacheop_data cache_data;
                cache_data.handle = handle_data.handle;
                cache_data.virt = m_v4l2Buffer[0];
				cache_data.type = 0;
                err = ioctl(iIonFd, ION_CUSTOM_CACHE_OP, &cache_data);
				//printf("%d: err=%d\n",__LINE__,err);
				if(err){
					printf("flush errr \n");
					memcpy(&tmpsub,&sub,sizeof(sub));
					//tmpsub.len = preview_w*preview_h*3/2;
					tmpsub.offset = m_v4l2Buffer[0];
					ioctl(fd_data.fd,ION_PMEM_CACHE_FLUSH,&tmpsub);
				}
				//err = 0;
				#endif
				data[0] = (int)v4l2Buffer_phy_addr+cfilledbuffer1.index*cfilledbuffer1.length;  
				data[1] = (int)(data[0] + preview_w *preview_h);
				//printf("v4l2Buffer_phy_addr = %p\n",v4l2Buffer_phy_addr);
			}
			else
			{
				#if 0
				memcpy(m_v4l2Buffer[4],m_v4l2Buffer[cfilledbuffer1.index],cfilledbuffer1.length);


				static struct ion_cacheop_data cache_data;
				cache_data.handle = handle_data.handle;
				cache_data.virt = m_v4l2Buffer[0];
				cache_data.type = 0;
				err = ioctl(iIonFd, ION_CUSTOM_CACHE_OP, &cache_data);
				printf("%d: err=%d\n",__LINE__,err);
				if(err){
					printf("flush errr \n");
					memcpy(&tmpsub,&sub,sizeof(sub));
					//tmpsub.len = preview_w*preview_h*3/2;
					tmpsub.offset = m_v4l2Buffer[0];
					ioctl(fd_data.fd,ION_PMEM_CACHE_FLUSH,&tmpsub);
				}

				#endif
			
#if CAM_OVERLAY_BUF_NEW
				data[0] =(int)cfilledbuffer1.m.offset /*v4l2Buffer_phy_addr+cfilledbuffer1.length*4*/;  
#else
				data[0] = (int)cfilledbuffer1.m.offset + cfilledbuffer1.index * cfilledbuffer1.length;
#endif
				data[1] = (int)(data[0] + preview_w *preview_h);
			}
			
			//  		for(i = 0;i < 100;i++){
			// 			printf("0x%x ",*((char*)(m_v4l2Buffer[cfilledbuffer1.index])+i));
			//  			}
			//printf("y_addr = 0x%x,length = %d\n",data[0],cfilledbuffer1.length);
			if (ioctl(iDispFd, 0x5002, data) == -1) {
			       printf("%s ioctl fb1 queuebuf fail!\n",__FUNCTION__);
			       err = -1;
				goto exit;
			}
			if (ioctl(iDispFd, FBIOGET_VSCREENINFO, &var) == -1) {
				printf("%s ioctl fb1 FBIOPUT_VSCREENINFO fail!\n",__FUNCTION__);
				err = -1;
				goto exit;
			}
			
			//printf("preview_w = %d,preview_h =%d,panelsize[1] = %d,panelsize[0] = %d\n",preview_w,preview_h,panelsize[1],panelsize[0]);
			var.xres_virtual = preview_w;	//win0 memery x size
			var.yres_virtual = preview_h;	 //win0 memery y size
			var.xoffset = 0;   //win0 start x in memery
			var.yoffset = 0;   //win0 start y in memery
			var.nonstd = ((cory<<20)&0xfff00000) + (( corx<<8)&0xfff00) +FB_NONSTAND;
			var.grayscale = ((preview_h<<20)&0xfff00000) + (( preview_w<<8)&0xfff00) + 0;   //win0 xsize & ysize
			var.xres = preview_w;	 //win0 show x size
			var.yres = preview_h;	 //win0 show y size
			var.bits_per_pixel = 16;
			var.activate = FB_ACTIVATE_FORCE;
			if (ioctl(iDispFd, FBIOPUT_VSCREENINFO, &var) == -1) {
				printf("%s ioctl fb1 FBIOPUT_VSCREENINFO fail!\n",__FUNCTION__);
				err = -1;
				goto exit;
			}
			if (ioctl(iDispFd,RK_FBIOSET_CONFIG_DONE, NULL) < 0) {
        			perror("set config done failed");
    			}

			//usleep(30000); 

		}
	if (ioctl(iCamFd, VIDIOC_QBUF, &cfilledbuffer1) < 0) {
		printf("%s VIDIOC_QBUF Failed!!!\n",__FUNCTION__);
		err = -1;
		goto exit;
	}

	    fps++;
	}
//	hasstoped = 1;

exit:
	return err;
}
// the func is a while loop func , MUST  run in a single thread.
int startCameraTest(){
	int ret = 0;
	int cameraId = 0;
	int preWidth;
	int preHeight;
	int corx ;
	int cory;

	get_camera_size();
	
	if(iCamFd > 0){
		printf(" %s has been opened! can't switch camera!\n",videodevice);
		return -1;
	}

	isstoped = 0;
	hasstoped = 0;
	cameraId = cam_id%2;
	cam_id++;
	preWidth = camera_w;
	preHeight = camera_h;
	corx = camera_x;
	cory = camera_y;
	sprintf(videodevice,"/dev/video%d",cameraId);
	preview_w = preWidth;
	preview_h = preHeight;
	printf("start test camera %d ....\n",cameraId);
	
    if(access(videodevice, O_RDWR) <0 ){
	   printf("access %s failed\n",videodevice);
	   hasstoped = 1;
	   return -1;
     }
	  
	if (CameraCreate() == 0)
	{
		if (CameraStart(v4l2Buffer_phy_addr, 4, preview_w,preview_h) == 0)
		{
			if (DispCreate(corx ,cory,preWidth,preHeight) == 0)
			{
				TaskRuning(1,corx,cory);
			}
			else
			{
				tc_info->result = -1;
				printf("%s display create wrong!\n",__FUNCTION__);
			}
		}
		else
		{
			tc_info->result = -1;
			printf("%s camera start erro\n",__FUNCTION__);
		}
	}
	else
	{
		tc_info->result = -1;
		printf("%s camera create erro\n",__FUNCTION__);
	}
	//isstoped = 1;
	hasstoped = 1;
	printf("camrea%d test over\n",cameraId);
	return 0;
}

int stopCameraTest(){
	
	sprintf(videodevice,"/dev/video%d",(cam_id%2));
	if(access(videodevice, O_RDWR) <0 ){
	   printf("access %s failed,so dont't switch to camera %d\n",videodevice,(cam_id%2));
	   //recover videodevice
	   sprintf(videodevice,"/dev/video%d",(1-(cam_id%2)));
	   return 0;
	 }
	printf("%s enter stop -----\n",__func__);
	return TaskStop();
}
void finishCameraTest(){
	int i;
		TaskStop();
		if (iPmemFd > 0) {
			close(iPmemFd);
			iPmemFd = -1;
		}
	
		if(iIonFd > 0){
			munmap(m_v4l2Buffer[0], ionAllocData.len);
	
			close(iIonFd);
			iIonFd = -1;
			}
		if (iDispFd > 0) {
			int disable = 0;
			printf("Close disp\n");
			ioctl(iDispFd, FBIOSET_ENABLE,&disable);
			close(iDispFd);
			iDispFd = -1;
		}
		//added by zyl 2013/09/16
		if(CAMERA_IS_UVC_CAMERA())
		{
			for(i=0; i<4; i++)
				munmap(mCamDriverV4l2Buffer[i], mCamDriverV4l2BufferLen);	
		}
		
}

int get_camera_size()
{
	if(camera_x>0 && camera_y>0 && camera_w>0 && camera_h >0)
		return 0;	

	if(gr_fb_width() > gr_fb_height()){
		camera_w = ((gr_fb_width() >> 1) & ~0x03);//camera_msg->w;
		camera_h = ((gr_fb_height()*2/3) & ~0x03);// camera_msg->h;
	}
	else{
		camera_h = ((gr_fb_width() >> 1) & ~0x03);//camera_msg->w;
		camera_w = ((gr_fb_height()*2/3) & ~0x03);// camera_msg->h;
	}

	if(camera_w > 640)
		camera_w = 640;
	if(camera_h > 480)
		camera_h=480;			

	
	camera_x = gr_fb_width() >> 1; 	
	camera_y = 0;

	return 0;
}


void * camera_test(void *argc)
{
	int ret,num;

	tc_info = (struct testcase_info *)argc; 

	if (script_fetch("camera", "number",&num, 1) == 0) {
		printf("camera_test num:%d\r\n",num);
		camera_num = num;	
	}

	startCameraTest();
	
	return argc;
}


int cameraFormatConvert(int v4l2_fmt_src, int v4l2_fmt_dst, const char *android_fmt_dst, 
                            char *srcbuf, char *dstbuf,int srcphy,int dstphy,
                            int src_w, int src_h, int srcbuf_w,
                            int dst_w, int dst_h, int dstbuf_w,
                            bool mirror)
{
    int y_size,i,j;
    int ret = -1;
    /*
    if (v4l2_fmt_dst) { 
        LOGD("cameraFormatConvert '%c%c%c%c'@(0x%x,0x%x,%dx%d)->'%c%c%c%c'@(0x%x,0x%x,%dx%d) ",
    				v4l2_fmt_src & 0xFF, (v4l2_fmt_src >> 8) & 0xFF,
    				(v4l2_fmt_src >> 16) & 0xFF, (v4l2_fmt_src >> 24) & 0xFF,
    				(int)srcbuf, srcphy,src_w,src_h,
    				v4l2_fmt_dst & 0xFF, (v4l2_fmt_dst >> 8) & 0xFF,
    				(v4l2_fmt_dst >> 16) & 0xFF, (v4l2_fmt_dst >> 24) & 0xFF,
    				 (int)dstbuf,dstphy,dst_w,dst_h);
    } else if (android_fmt_dst) {
        LOGD("cameraFormatConvert '%c%c%c%c'@(0x%x,0x%x,%dx%d)->%s@(0x%x,0x%x,%dx%d)",
    				v4l2_fmt_src & 0xFF, (v4l2_fmt_src >> 8) & 0xFF,
    				(v4l2_fmt_src >> 16) & 0xFF, (v4l2_fmt_src >> 24) & 0xFF
    				, (int)srcbuf, srcphy,src_w,src_h,android_fmt_dst, (int)dstbuf,dstphy,
    				 dst_w,dst_h);
    }
    */  
    
    y_size = src_w*src_h;
    switch (v4l2_fmt_src)
    {
        case V4L2_PIX_FMT_YUYV:
        {
            char *srcbuf_begin;
            int *dstint_y, *dstint_uv, *srcint;
            
            if ((v4l2_fmt_dst == V4L2_PIX_FMT_NV12) || 
                ((v4l2_fmt_dst == V4L2_PIX_FMT_YUV420)/* && CAMERA_IS_RKSOC_CAMERA() 
                && (mCamDriverCapability.version == KERNEL_VERSION(0, 0, 1))*/)) { 
                if ((src_w == dst_w) && (src_h == dst_h)) {
                    dstint_y = (int*)dstbuf;                
                    srcint = (int*)srcbuf;
                    for(i=0;i<(y_size>>2);i++) {
                        *dstint_y++ = ((*(srcint+1)&0x00ff0000)<<8)|((*(srcint+1)&0x000000ff)<<16)
                                    |((*srcint&0x00ff0000)>>8)|(*srcint&0x000000ff);
                        
                        srcint += 2;
                    }
                    dstint_uv =  (int*)(dstbuf + y_size);
                    srcint = (int*)srcbuf;
                    for(i=0;i<src_h/2; i++) {
                        for (j=0; j<(src_w>>2); j++) {
                            *dstint_uv++ = (*(srcint+1)&0xff000000)|((*(srcint+1)&0x0000ff00)<<8)
                                        |((*srcint&0xff000000)>>16)|((*srcint&0x0000ff00)>>8); 
                            srcint += 2;
                        }
                        srcint += (src_w>>1);  
                    }
                    ret = 0;
                } else {
                    if (v4l2_fmt_dst) {    
                        printf("cameraFormatConvert '%c%c%c%c'@(0x%x,0x%x)->'%c%c%c%c'@(0x%x,0x%x), %dx%d->%dx%d "
                             "scale isn't support",
                    				v4l2_fmt_src & 0xFF, (v4l2_fmt_src >> 8) & 0xFF,
                   				(v4l2_fmt_src >> 16) & 0xFF, (v4l2_fmt_src >> 24) & 0xFF,
                    				v4l2_fmt_dst & 0xFF, (v4l2_fmt_dst >> 8) & 0xFF,
                    				(v4l2_fmt_dst >> 16) & 0xFF, (v4l2_fmt_dst >> 24) & 0xFF,
                    				(int)srcbuf, srcphy, (int)dstbuf,dstphy,src_w,src_h,dst_w,dst_h);
                    } else if (android_fmt_dst) {
                        printf("cameraFormatConvert '%c%c%c%c'@(0x%x,0x%x)->%s@(0x%x,0x%x) %dx%d->%dx%d "
                           "scale isn't support",
                    				v4l2_fmt_src & 0xFF, (v4l2_fmt_src >> 8) & 0xFF,
                    				(v4l2_fmt_src >> 16) & 0xFF, (v4l2_fmt_src >> 24) & 0xFF
                    				, (int)srcbuf, srcphy,android_fmt_dst, (int)dstbuf,dstphy,
                    				 src_w,src_h,dst_w,dst_h);
                    }
                }

            }
			else if ((v4l2_fmt_dst == V4L2_PIX_FMT_NV21)/*|| 
                       (android_fmt_dst && (strcmp(android_fmt_dst,CameraParameters::PIXEL_FORMAT_YUV420SP)==0))*/) {
                if ((src_w==dst_w) && (src_h==dst_h)) {
                    dstint_y = (int*)dstbuf;                
                    srcint = (int*)srcbuf;
                    for(i=0;i<(y_size>>2);i++) {
                        *dstint_y++ = ((*(srcint+1)&0x00ff0000)<<8)|((*(srcint+1)&0x000000ff)<<16)
                                    |((*srcint&0x00ff0000)>>8)|(*srcint&0x000000ff);
                        srcint += 2;
                    }
                    dstint_uv =  (int*)(dstbuf + y_size);
                    srcint = (int*)srcbuf;
                    for(i=0;i<src_h/2; i++) {
                        for (j=0; j<(src_w>>2); j++) {
                            *dstint_uv++ = ((*(srcint+1)&0xff000000)>>8)|((*(srcint+1)&0x0000ff00)<<16)
                                        |((*srcint&0xff000000)>>24)|(*srcint&0x0000ff00); 
                            srcint += 2;
                        }
                        srcint += (src_w>>1);  
                    } 
                    ret = 0;
                } else {
                    if (v4l2_fmt_dst) {    
                        printf("cameraFormatConvert '%c%c%c%c'@(0x%x,0x%x)->'%c%c%c%c'@(0x%x,0x%x), %dx%d->%dx%d "
                             "scale isn't support",
                    				v4l2_fmt_src & 0xFF, (v4l2_fmt_src >> 8) & 0xFF,
                    				(v4l2_fmt_src >> 16) & 0xFF, (v4l2_fmt_src >> 24) & 0xFF,
                    				v4l2_fmt_dst & 0xFF, (v4l2_fmt_dst >> 8) & 0xFF,
                    				(v4l2_fmt_dst >> 16) & 0xFF, (v4l2_fmt_dst >> 24) & 0xFF,
                    				(int)srcbuf, srcphy, (int)dstbuf,dstphy,src_w,src_h,dst_w,dst_h);
                    } else if (android_fmt_dst) {
                        printf("cameraFormatConvert '%c%c%c%c'@(0x%x,0x%x)->%s@(0x%x,0x%x) %dx%d->%dx%d "
                             "scale isn't support",
                    				v4l2_fmt_src & 0xFF, (v4l2_fmt_src >> 8) & 0xFF,
                    				(v4l2_fmt_src >> 16) & 0xFF, (v4l2_fmt_src >> 24) & 0xFF
                    				, (int)srcbuf, srcphy,android_fmt_dst, (int)dstbuf,dstphy,
                    				 src_w,src_h,dst_w,dst_h);
                    }
                }
            }            
            break;
        }
	
cameraFormatConvert_default:        
        default:
            if (android_fmt_dst) {
                printf("%s(%d): CameraHal is not support (%c%c%c%c -> %s)",__FUNCTION__,__LINE__,
                    v4l2_fmt_src & 0xFF, (v4l2_fmt_src >> 8) & 0xFF,
    			    (v4l2_fmt_src >> 16) & 0xFF, (v4l2_fmt_src >> 24) & 0xFF, android_fmt_dst);
            } else if (v4l2_fmt_dst) {
                printf("%s(%d): CameraHal is not support (%c%c%c%c -> %c%c%c%c)",__FUNCTION__,__LINE__,
                    v4l2_fmt_src & 0xFF, (v4l2_fmt_src >> 8) & 0xFF,
    			    (v4l2_fmt_src >> 16) & 0xFF, (v4l2_fmt_src >> 24) & 0xFF,
    			    v4l2_fmt_dst & 0xFF, (v4l2_fmt_dst >> 8) & 0xFF,
                    (v4l2_fmt_dst >> 16) & 0xFF, (v4l2_fmt_dst >> 24) & 0xFF);
            }
            break;
    }
    return ret;

}

