#pragma once
#include <vector>
#include <string>

#ifdef __ANDROID__
#include <objOpenCL.h>
#else
#include "CL/opencl.h"
#endif

typedef struct _opencl_param
{
	cl_platform_id pid;
	cl_device_id did;
	cl_context context;
	cl_command_queue cq;
	cl_program prog;
	cl_kernel kernel;
	cl_mem mem1, mem2, mem3;
} opencl_param;

#define MAX_HISTORY_BUF_SIZE 3

class BufferPool
{
public:
	void init(int bufferSize, int bufferCount);
	void copyToBuffer(unsigned char *data, int size);
	unsigned char *getBuffer(int offset);
	int getBufferCount(void);
	void release(void);
	BufferPool();
	~BufferPool();

private:
	int curr_pos;
	int curr_count;
	int total_count;
	unsigned char *buf[MAX_HISTORY_BUF_SIZE];
};

class DepthmapFilter
{
public:
	unsigned char* SubSample(unsigned char* depthBuf, int bytesPerPixel, int width, int height, int& new_width, int& new_height, int mode = 0, int factor = 3);
	void HoleFill(unsigned char* depthBuf, int bytesPerPixel, int kernel_size, int width, int height, int level, bool horizontal);
	void InitTemporalFilter(int width, int height, int bytesPerPixel);
	void FreeTemporalFilter(void);
	void TemporalFilter(unsigned char* depthBuf, int bytesPerPixel, int width, int height, float alpha, int history);
	void InitEdgePreServingFilter(int width, int height);
	void FreeEdgePreServingFilter(void);
	void EdgePreServingFilter(unsigned char* depthBuf, int type, int width, int height, int level, float sigma, float lumda);
	void ApplyFilters(unsigned char* depthBuf, unsigned char* subDisparity, int bytesPerPixel, int width, int height, int sub_w, int sub_h, int threshold=64);
	void Reset();
	void EnableGPUAcceleration(bool enable);
	std::string GetVersion();
	//static DepthmapFilter* getInstance();
	DepthmapFilter();
	~DepthmapFilter();

private:
	//DepthmapFilter();
	void processTemporalFilter(unsigned char* depthBuf, int bytesPerPixel, int width, int height, float alpha, int history);
	void processGaussianLikeFilter(unsigned char* depthBuf, unsigned char* uvMap, int type, int width, int height, int level, float sigma, float lumda);
	void processGaussianLikeFilter_OpenCL(unsigned char* depthBuf, unsigned char* uvMap, int type, int width, int height, int level, float sigma, float lumda);
	void initGaussianLikeFilter_OpenCL(int width, int height);
	void freeGaussianLikeFilter_OpenCL(void);

private:
	//static DepthmapFilter* instance;
	//std::vector<unsigned char*> historyDepthBuffer;
	BufferPool historyDepthBufferPool;
	unsigned char* temproalMap;
	unsigned char* uvMap;
	float *img2dnormalize;
	unsigned char* sub_disparity_buf;
	int sub_width;
	int sub_height;
	int sub_bpp;
	int deltaZ;
	bool reset;
	int uvMapWidth;
	int uvMapHeight;
	opencl_param clparam;
	bool useGPU;
	int e_buf_size;
	int t_buf_size;
};
