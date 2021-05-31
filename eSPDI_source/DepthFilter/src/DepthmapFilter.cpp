//#include "stdafx.h"
#include <cstdio>
#include <cstring>
#include <math.h>
#include <algorithm>
#include "DepthmapFilter.h"
#include "CL/opencl.h"

#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif


#define VERSION_MAJOR_NUMBER 1
#define VERSION_MINOR_NUMBER 1
#define VERSION_REVISION_NUMBER 1

#define VERSION_NUMBER_TO_STR_INTERNAL(num) #num
#define VERSION_NUMBER_TO_STR(num) VERSION_NUMBER_TO_STR_INTERNAL(num)

using namespace std;

//DepthmapFilter* DepthmapFilter::instance = 0;

const int ReservedLeftX = 100;

BufferPool::BufferPool()
{
	curr_pos = 0;
	curr_count = 0;
	total_count = 0;

	for (int i = 0; i < MAX_HISTORY_BUF_SIZE; i++)
		buf[i] = NULL;
}

BufferPool::~BufferPool()
{
	curr_pos = 0;
	curr_count = 0;
	total_count = 0;

	for (int i = 0; i < MAX_HISTORY_BUF_SIZE; i++)
	{
		if (buf[i] != NULL)
			free(buf[i]);
	}
}

void BufferPool::init(int bufferSize, int bufferCount)
{
	total_count = bufferCount;
	
	for (int i = 0; i < total_count; i++)
	{
		if (buf[i] != NULL)
			free(buf[i]);
		buf[i] = (unsigned char*)malloc(bufferSize);
	}

	curr_pos   = 0;
	curr_count = 0;
}

void BufferPool::release(void)
{
	for (int i = 0; i < total_count; i++)
	{
		if (buf[i] != NULL)
			free(buf[i]);
		buf[i] = NULL;
	}
}

void BufferPool::copyToBuffer(unsigned char *data, int size)
{
	memcpy(buf[curr_pos], data, size);
	
	curr_pos++;
	if (curr_pos >= total_count)
		curr_pos = 0;

	curr_count++;
	if (curr_pos >= total_count)
		curr_count = total_count;
}

unsigned char *BufferPool::getBuffer(int offset)
{
	int pos;
	
	pos = curr_pos + offset;
	
	if (pos >= total_count)
		pos -= total_count;
	else if(pos < 0)
		pos += total_count;

	return buf[pos];
}

int BufferPool::getBufferCount(void)
{
	return curr_count;
}


DepthmapFilter::DepthmapFilter()
{
	temproalMap = 0;
	uvMap = 0;
	img2dnormalize = 0;
	sub_disparity_buf = 0;
	sub_width  = 0;
	sub_height = 0;
	sub_bpp    = 0;
	deltaZ = 64;
	reset = false;
	useGPU = true;
	clparam.mem1 = 0;
	clparam.mem2 = 0;
	clparam.mem3 = 0;
	clparam.context = 0;
	clparam.cq = 0;
	clparam.prog = 0;
	clparam.kernel = 0;
	e_buf_size = 0;
	t_buf_size = 0;
}


DepthmapFilter::~DepthmapFilter()
{
	if (sub_disparity_buf != NULL)
		free(sub_disparity_buf);

	FreeTemporalFilter();
	FreeEdgePreServingFilter();
}

/*
DepthmapFilter* DepthmapFilter::getInstance()
{
	if (instance == 0)
	{
		instance = new DepthmapFilter();
	}

	return instance;
}
*/


/*
void DepthmapFilter::HoleFill(unsigned char* depthBuf, int bytesPerPixel, int kernel_size, int width, int height)
{
	int type;
	switch (bytesPerPixel)
	{
	case 1:
		type = CV_8UC1;
		break;
	case 2:
		type = CV_16UC1;
		break;
	}
	kernel_size = 1;
	int down_size = 1;
	int resized_w = width / down_size;
	int resized_h = height / down_size;
	cv::Mat src(cv::Size(width, height), type, depthBuf);
	cv::Mat dst(cv::Size(width, height), type);
	cv::Mat downsample(cv::Size(resized_w, resized_h), type);
	if (down_size > 1)
		resize(src, downsample, cv::Size(resized_w, resized_h), 0, 0, INTER_AREA);


	vector<int> sorted_disparity;
	int index = 0;
	sorted_disparity.resize(((kernel_size * 2) + 1) * ((kernel_size * 2) + 1));

	if (type == CV_16UC1)
	{
		for (int j = 0; j < resized_h-1; j++)
		{
			for (int i = 80; i < resized_w-1; i++)
			{

				int pixel_disparity;
				if (down_size > 1)
					pixel_disparity = (int)downsample.at<unsigned short>(j, i);
				else
					pixel_disparity = (int)src.at<unsigned short>(j, i);

				if (pixel_disparity == 0)  // if is black hole
				{
					//int maxValue = 0;
					int count = 0;
					int avg = 0;
					//vector<int> sorted_disparity;
					index = 0;
					//for (int d = 1; d < kernel_size; d++)
					int d = kernel_size;
					{
						for (int k = i - d; k <= i + d && k < resized_w && k >= 0; k++)
						{
							for (int l = j - d; l <= j + d && l < resized_h && l >= 0; l++)
							{
								int neighbor_disparity;
								if (down_size > 1)
									neighbor_disparity = (int)downsample.at<unsigned short>(l, k);
								else
									neighbor_disparity = (int)src.at<unsigned short>(l, k);

								sorted_disparity[index++] = neighbor_disparity;
								if (neighbor_disparity > 256)
								{
									count++;
									//avg += neighbor_disparity;
								}

								//if (neighbor_disparity > maxValue)
								//	maxValue = neighbor_disparity;
							}
						}
					}

					int k = (kernel_size * 2) + 1;
					int th = k * kernel_size;

					if (count > th )
					{
						int mid_pos = count / 2;
						sort(sorted_disparity.begin(), sorted_disparity.begin() + count);
						int mid = sorted_disparity[mid_pos];
						if (mid > 0)
						{
							if (down_size > 1)
								downsample.at<unsigned short>(j, i) = mid;
							else
								src.at<unsigned short>(j, i) = mid;
						}
					}

				}
			}
		}
	}
	else if (type == CV_8UC1)
	{
		for (int j = 0; j < resized_h; j++)
		{
			for (int i = 80; i < resized_w; i++)
			{
				int pixel_disparity = (int)downsample.at<unsigned char>(j, i); // ptr[i + j * resized_w];

				if (pixel_disparity == 0)  // if is black hole
				{
					//int maxValue = 0;
					int count = 0;
					int avg = 0;
					int index = 0;

					for (int d = 1; d <= kernel_size; d++)
						//int d = kernel_size;
					{
						for (int k = i - d; k <= i + d && k < resized_w && k >= 0; k++)
						{
							for (int l = j - d; l <= j + d && l < resized_h && l >= 0; l++)
							{
								int neighbor_disparity = (int)downsample.at<unsigned char>(l, k); //.ptr[k + l * resized_w];

								sorted_disparity[index++] = neighbor_disparity;
								if (neighbor_disparity > 2)
								{
									count++;
									//avg += neighbor_disparity;
								}

								//if (neighbor_disparity > maxValue)
								//	maxValue = neighbor_disparity;
							}
						}
					}

					int k = (kernel_size * 2) + 1;
					int th = k * kernel_size;

					if (count > th)
					{
						int mid_pos = count / 2;
						sort(sorted_disparity.begin(), sorted_disparity.begin() + count);
						int mid = sorted_disparity[mid_pos];
						if (mid > 0)
						{
							if (down_size > 1)
								downsample.at<unsigned short>(j, i) = mid;
							else
								src.at<unsigned short>(j, i) = mid;
						}
					}
				}
			}
		}
	}

	if (down_size > 1)
	{
		resize(downsample, dst, cv::Size(width, height), 0, 0, INTER_AREA);
		//src = dst.clone();
		memcpy(depthBuf, dst.ptr(0), width * height * 2);
	}
	//else
	//	memcpy(depthBuf, src.ptr(0), width * height * 2);

}
*/

/*
void DepthmapFilter::HoleFill_Median(unsigned char* depthBuf, int bytesPerPixel, int width, int height)
{
	vector<int> sorted_disparity;
	int index = 0;
	sorted_disparity.resize(9);

	if (bytesPerPixel == 2)
	{
		unsigned short * ptr;
		ptr = (unsigned short *)depthBuf;

		for (int j = 0; j < height - 1; j++)
		{
			for (int i = 80; i < width - 1; i++)
			{

				int pixel_disparity = ptr[j * width + i];
				//pixel_disparity = (int)src.at<unsigned short>(j, i);

				if (pixel_disparity == 0)  // if is black hole
				{
					//int maxValue = 0;
					int count = 0;
					int avg = 0;
					//vector<int> sorted_disparity;
					index = 0;
					//for (int d = 1; d < kernel_size; d++)
					int d = 1;
					{
						for (int k = i - d; k <= i + d && k < width && k >= 0; k++)
						{
							for (int l = j - d; l <= j + d && l < height && l >= 0; l++)
							{
								int neighbor_disparity = ptr[l * width + k];

								sorted_disparity[index++] = neighbor_disparity;
								if (neighbor_disparity > 256)
								{
									count++;
									//avg += neighbor_disparity;
								}

								//if (neighbor_disparity > maxValue)
								//	maxValue = neighbor_disparity;
							}
						}
					}

					int k = 3;
					int th = k;

					if (count > th)
					{
						int mid_pos = count / 2;
						sort(sorted_disparity.begin(), sorted_disparity.begin() + count);
						int mid = sorted_disparity[mid_pos];
						if (mid > 0)
						{
							ptr[j * width + i] = mid;
						}
					}

				}
			}
		}
	}
	else if (bytesPerPixel == 1)
	{
		for (int j = 0; j < height - 1; j++)
		{
			for (int i = 80; i < width - 1; i++)
			{

				int pixel_disparity = depthBuf[j * width + i];
				//pixel_disparity = (int)src.at<unsigned short>(j, i);

				if (pixel_disparity == 0)  // if is black hole
				{
					//int maxValue = 0;
					int count = 0;
					int avg = 0;
					//vector<int> sorted_disparity;
					index = 0;
					//for (int d = 1; d < kernel_size; d++)
					int d = 1;
					{
						for (int k = i - d; k <= i + d && k < width && k >= 0; k++)
						{
							for (int l = j - d; l <= j + d && l < height && l >= 0; l++)
							{
								int neighbor_disparity = depthBuf[l * width + k];

								sorted_disparity[index++] = neighbor_disparity;
								if (neighbor_disparity > 0)
								{
									count++;
									//avg += neighbor_disparity;
								}

								//if (neighbor_disparity > maxValue)
								//	maxValue = neighbor_disparity;
							}
						}
					}

					int k = 3;
					int th = k;

					if (count > th)
					{
						int mid_pos = count / 2;
						sort(sorted_disparity.begin(), sorted_disparity.begin() + count);
						int mid = sorted_disparity[mid_pos];
						if (mid > 0)
						{
							depthBuf[j * width + i] = mid;
						}
					}

				}
			}
		}
	}
}
*/

static void median_filter(unsigned char* depthBuf, int bytesPerPixel, int x, int y, int width, int height, vector<int>& neighbor, int level)
{
	if (bytesPerPixel == 2)
	{
		unsigned short * ptr;
		ptr = (unsigned short *)depthBuf;

		//unsigned char * uvptr;
		//uvptr = (unsigned char *)uvmapBuf;

				//unsigned short disparity = ptr[y * width + x];
				//int pixel_disparity = endian_swap(disparity);
				//pixel_disparity = (int)src.at<unsigned short>(j, i);
		        int pixel_disparity = ptr[y * width + x];

				if (pixel_disparity == 0)  // if is black hole
				{
					//int maxValue = 0;
					int count = 0;
					//int avg = 0;
					//vector<int> sorted_disparity;
					int index = 0;
					//for (int d = 1; d < kernel_size; d++)
					int d = 1;
					{
						for (int k = x - d; k <= x + d ; k++)
						{
							for (int l = y - d; l <= y + d ; l++)
							{
								//disparity = ptr[l * width + k];
								//int neighbor_disparity = endian_swap(disparity);								
								//if (useUVMap)
								//{
								//	int uv = uvptr[l * width + k];
								//	if (uv == 1)
								//		continue;
								//}

								int neighbor_disparity = ptr[l * width + k];

								neighbor[index++] = neighbor_disparity;
								if (neighbor_disparity > 127)
								//if (neighbor_disparity > 0)
								{
									count++;
									//neighbor[count++] = neighbor_disparity;
									//avg += neighbor_disparity;
								}

							}
						}
					}

					int th = 3;

					if (count > th-level )
					{
						int mid_pos = count / 2;
						sort(neighbor.begin(), neighbor.begin() + count);
						int mid = neighbor[mid_pos+1];
						if (mid > 0)
						{
							//ptr[y * width + x] = endian_swap((unsigned short)mid);
							ptr[y * width + x] = mid;
						}
					}

				}
	}
	else if (bytesPerPixel == 1)
	{
		/*
				int pixel_disparity = depthBuf[y * width + x];
				//pixel_disparity = (int)src.at<unsigned short>(j, i);

				if (pixel_disparity == 0)  // if is black hole
				{
					//int maxValue = 0;
					int count = 0;
					//int avg = 0;
					vector<int> sorted_disparity;
					sorted_disparity.resize(9);
					int index = 0;
					//for (int d = 1; d < kernel_size; d++)
					int d = 1;
					{
						for (int k = y - d; k <= x + d ; k++)
						{
							for (int l = y - d; l <= y + d ; l++)
							{
								int neighbor_disparity = depthBuf[l * width + k];

								sorted_disparity[index++] = neighbor_disparity;
								if (neighbor_disparity > 0)
								{
									count++;
									//avg += neighbor_disparity;
								}

								//if (neighbor_disparity > maxValue)
								//	maxValue = neighbor_disparity;
							}
						}
					}

					int th = 3;

					if (count > th)
					{
						int mid_pos = count / 2;
						sort(sorted_disparity.begin(), sorted_disparity.begin() + count);
						int mid = sorted_disparity[mid_pos+1];
						if (mid > 0)
						{
							depthBuf[y * width + x] = mid;
						}
					}

				}
				*/


						int pixel_disparity = depthBuf[y * width + x];
						//pixel_disparity = (int)src.at<unsigned short>(j, i);

						if (pixel_disparity == 0)  // if is black hole
						{
							//int maxValue = 0;
							int count = 0;
							int avg = 0;
							//vector<int> sorted_disparity;
							int index = 0;
							//for (int d = 1; d < kernel_size; d++)
							int d = 1;
							{
								for (int k = x - d; k <= x + d ; k++)
								{
									for (int l = y - d; l <= y + d; l++)
									{
										int neighbor_disparity = depthBuf[l * width + k];
										neighbor[index++] = neighbor_disparity;
										if (neighbor_disparity > 32)
										//if (neighbor_disparity > 0)
										{
											count++;
											//avg += neighbor_disparity;
											//neighbor[count++] = neighbor_disparity;
										}

										//if (neighbor_disparity > maxValue)
										//	maxValue = neighbor_disparity;
									}
								}
							}

							int th = 3;

							/*
							if (count > th-level)
							{
								depthBuf[y * width + x] = avg / count;
							}
							*/

							if (count > th)
							{
								int mid_pos = count / 2;
								sort(neighbor.begin(), neighbor.begin() + count);
								int mid = neighbor[mid_pos + 1];
								if (mid > 0)
								{
									depthBuf[y * width + x] = mid;
								}
							}

						}
	}
}

void occlusion_filter(unsigned char* depthBuf, int bytesPerPixel, int width, int height, unsigned char* uvmap)
{
	/* Intel RealSense implementation
	float occZTh = 0.1f; //meters
	int occDilationSz = 1;
	auto pixels_ptr = pix_coord.data();
	auto points_width = _depth_intrinsics->width;
	auto points_height = _depth_intrinsics->height;

	for (size_t y = 0; y < points_height; ++y)
	{
		float maxInLine = -1;
		float maxZ = 0;
		int occDilationLeft = 0;

		for (size_t x = 0; x < points_width; ++x)
		{
			if (points->z)
			{
				// Occlusion detection
				if (pixels_ptr->x < maxInLine || (pixels_ptr->x == maxInLine && (points->z - maxZ) > occZTh))
				{
					uv_map->x = 0.f;
					uv_map->y = 0.f;
					occDilationLeft = occDilationSz;
				}
				else
				{
					maxInLine = pixels_ptr->x;
					maxZ = points->z;
					if (occDilationLeft > 0)
					{
						uv_map->x = 0.f;
						uv_map->y = 0.f;
						occDilationLeft--;
					}
				}
			}

			++points;
			++uv_map;
			++pixels_ptr;
		}
	}
	*/

	if (bytesPerPixel == 2)
	{
		unsigned short * ptr;
		ptr = (unsigned short *)depthBuf;

		unsigned char * uvptr;
		uvptr = (unsigned char *)uvmap;

		int occZTh = 640; //640;
		int occDilationSz = 1;

		for (int y = 0; y < height; ++y)
		{
			//float maxInLine = -1;
			int maxZ = 0;
			int occDilationLeft = 0;

			for (int x = width - 1; x > 0; --x)
			{
				int pixel_disparity = ptr[y * width + x];
				if (pixel_disparity)
				{
					// Occlusion detection
					//if (x < maxInLine || (x == maxInLine && (pixel_disparity - maxZ) > occZTh))
					if ((pixel_disparity - maxZ) > occZTh)
					{
						uvptr[y * width + x] = 1;
						occDilationLeft = occDilationSz;
					}
					else
					{
						//maxInLine = x;
						maxZ = pixel_disparity;
						if (occDilationLeft > 0)
						{
							uvptr[y * width + x] = 1;
							occDilationLeft--;
						}
						else
						{
							uvptr[y * width + x] = 0;
						}
					}

				}
			}
		}
	}
	else if(bytesPerPixel == 1)
	{
		unsigned char * uvptr;
		uvptr = (unsigned char *)uvmap;

		int occZTh = 160;
		int occDilationSz = 1;

		for (int y = 0; y < height; ++y)
		{
			//float maxInLine = -1;
			int maxZ = 0;
			int occDilationLeft = 0;

			for (int x = width - 1; x > 0; --x)
			{
				int pixel_disparity = depthBuf[y * width + x];
				if (pixel_disparity)
				{
					// Occlusion detection
					//if (x < maxInLine || (x == maxInLine && (pixel_disparity - maxZ) > occZTh))
					if ((pixel_disparity - maxZ) > occZTh)
					{
						uvptr[y * width + x] = 1;
						occDilationLeft = occDilationSz;
					}
					else
					{
						//maxInLine = x;
						maxZ = pixel_disparity;
						if (occDilationLeft > 0)
						{
							uvptr[y * width + x] = 1;
							occDilationLeft--;
						}
						else
						{
							uvptr[y * width + x] = 0;
						}
					}

				}
			}
		}
	}
}

void DepthmapFilter::HoleFill(unsigned char* depthBuf, int bytesPerPixel, int kernel_size, int width, int height, int level, bool horizontal)
{
	if (level > 4)
		return;

	vector<int> sorted_disparity;
	sorted_disparity.resize(9);

	level = level - 1;
	int x = width  / 2 ;
	int y = height / 2;
	int r = height / 2;
	int h = width / 2;

	/*
	for (int i = y - 10 ; i < y + 10; i++)
	{
		//for (int j = x; (j - x)*(j - x) + (i - y)*(i - y) <= r * r; j--)
		for (int j = x; j > 80; j--)
		{
			median_filter(depthBuf, bytesPerPixel, j, i, width, height, sorted_disparity, level);
		}

		//for (int j = x + 1; (j - x)*(j - x) + (i - y)*(i - y) <= r * r; j++)
		for (int j = x + 1; j < width; j++)
		{
			median_filter(depthBuf, bytesPerPixel, j, i, width, height, sorted_disparity, level);
		}
	}
	*/

	if (horizontal)
	{
		for (int i = 1; i < height - 1; i++)
		{
			for (int j = 1; j < width - 1; j++)
			{
				//int edge_estimation = edgemap[i * width + j];
				//if (edge_estimation == 0)
				    median_filter(depthBuf, bytesPerPixel, j, i, width, height, sorted_disparity, level);
			}

			/*
			for (int j = x + 1; j < width; j++)
			{
				median_filter(depthBuf, uvmapBuf, bytesPerPixel, j, i, width, height, sorted_disparity, level);
			}
			*/
		}
	}
	else
	{
		//for (int i = y - r + 1; i < y + r; i++)
			//for (int i = y + r - 1; i > y - r; i--)
			//for (int i = x + h - 1; i > x - h; i--)
		for (int i = x; i > x - h + 1; i--)  //vertical part 
		{
			for (int j = 1; j < height - 1; j++)
			{
				//int edge_estimation = edgemap[j * width + i];
				//if (edge_estimation == 0)
				    median_filter(depthBuf, bytesPerPixel, i, j, width, height, sorted_disparity, level);  // vertical part 1
			}
		}
		// vertical scan part 2
		for (int i = x; i < x + h - 1; i++)
		{
			for (int j = 1; j < height - 1; j++)
			{
				//int edge_estimation = edgemap[j * width + i];
				//if (edge_estimation == 0)
				    median_filter(depthBuf, bytesPerPixel, i, j, width, height, sorted_disparity, level);
			}
		}
	}

	/*
	int X = width - 80;
	int Y = height;
	int x, y, dx, dy;
	x = y = dx = 0;
	dy = -1;
	int t = std::max(X, Y);
	int maxI = t * t;
	x = y = X/2;
	for (int i = 0; i < maxI; i++) {
		if ((-X / 2 < x) && (x <= X / 2) && (-Y / 2 < y) && (y <= Y / 2)) {
			// DO STUFF...
			median_filter(depthBuf, bytesPerPixel, x, y, width, height, sorted_disparity, level);
		}
		if ((x == y) || ((x < 0) && (x == -y)) || ((x > 0) && (x == 1 - y))) {
			t = dx;
			dx = -dy;
			dy = t;
		}
		x += dx;
		y += dy;
	}
	*/

	//delete uvmapBuf;
}

void DepthmapFilter::InitTemporalFilter(int width, int height, int bytesPerPixel)
{
	if (temproalMap == 0)
		temproalMap = (unsigned char*)malloc(width * height * sizeof(unsigned char));

	historyDepthBufferPool.init(width * height * bytesPerPixel, 3);
}

void DepthmapFilter::FreeTemporalFilter(void)
{
	if (temproalMap)
		free(temproalMap);
	temproalMap = 0;

	historyDepthBufferPool.release();
}

void DepthmapFilter::TemporalFilter(unsigned char* depthBuf, int bytesPerPixel, int width, int height, float alpha, int history)
{
	if (history <= 1)
		return;

	if (alpha <= 0.f || alpha > 1.f)
		return;

	int bufsize = width * height * bytesPerPixel;
	if (t_buf_size != bufsize)
	{
		if (t_buf_size != 0)
			FreeTemporalFilter();

		InitTemporalFilter(width, height, bytesPerPixel);

		t_buf_size = bufsize;
	}

	if(historyDepthBufferPool.getBufferCount() >= history)
	{
		processTemporalFilter(depthBuf, bytesPerPixel, width, height, alpha, history);
	}
	else
	{
		historyDepthBufferPool.copyToBuffer(depthBuf, bufsize);
	}
}

void DepthmapFilter::processTemporalFilter(unsigned char* depthBuf, int bytesPerPixel, int width, int height, float alpha, int history)
{
	if (bytesPerPixel == 2)
	{
		float filtered = 0.f;
		unsigned short * ptr;
		ptr = (unsigned short *)depthBuf;

		unsigned short *prev;
		prev = (unsigned short *)historyDepthBufferPool.getBuffer(0);
		float _one_minus_alpha = 1.f - alpha;

		unsigned short *oldest;
		oldest = (unsigned short *)historyDepthBufferPool.getBuffer(-1);

		unsigned short *older1;
		older1 = (unsigned short *)historyDepthBufferPool.getBuffer(-2);

		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				int pixel_disparity = ptr[y * width + x];
				/*
				if (pixel_disparity == 0 && temporal_disparity == 0)
					continue;

				if (pixel_disparity == 0)
				{
					// consider black holes
					//ptr[y * width + x] = temporal_disparity;
				}
				else {
				*/
				if (pixel_disparity > 0)
				{
					int temporal_disparity = prev[y * width + x];
					int diff = fabs(pixel_disparity - temporal_disparity);
					if (diff <= 64)  //Dmax: original 64
					{
						// apply temporal weights
						filtered = alpha * pixel_disparity + _one_minus_alpha * temporal_disparity;
						ptr[y * width + x] = ceil(filtered);
					}

					temproalMap[y * width + x] = 0;
				}
				else
				{
					int old_temporal_disparity = oldest[y * width + x];
					int temporal_disparity = prev[y * width + x];
					int older1_disparity = older1[y * width + x];
					//if (oldest > 0)						
					//	old_temporal_disparity = oldest[y * width + x];
						
					//if (prev > 0)
					//	temporal_disparity = prev[y * width + x];

					//if (old_temporal_disparity > 0 && old_temporal_disparity < 65536 && temporal_disparity > 0 && temporal_disparity < 65536
					//	&& older1_disparity > 0 && older1_disparity < 65536)
					if (old_temporal_disparity > 0 && temporal_disparity > 0 
							&& older1_disparity > 0 )
					{
						if (temproalMap[y * width + x])
						{
							temproalMap[y * width + x] = 0;
						}
						else
						{
							// has enough confindence in temporal history
							filtered = alpha * old_temporal_disparity + _one_minus_alpha * temporal_disparity;
							ptr[y * width + x] = ceil(filtered);
							temproalMap[y * width + x] = 1;
						}
					}
				}
			}
		}
			
		int bufsize = width * height * bytesPerPixel;
		historyDepthBufferPool.copyToBuffer(depthBuf, bufsize);
	}
	else if (bytesPerPixel == 1)
	{
		unsigned char *prev;
		prev = (unsigned char *)historyDepthBufferPool.getBuffer(0);
		float _one_minus_alpha = 1.f - alpha;

		unsigned char *oldest;
		oldest = (unsigned char *)historyDepthBufferPool.getBuffer(-1);

		unsigned char *older1;
		older1 = (unsigned char *)historyDepthBufferPool.getBuffer(-2);

		float filtered = 0.f;

		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				int pixel_disparity = depthBuf[y * width + x];

				/*
				if (pixel_disparity == 0 && twemporal_disparity == 0)
					continue;

				if (pixel_disparity == 0)
				{
					// consider black holes
					//ptr[y * width + x] = temporal_disparity;
				}
				else {
				*/
				if (pixel_disparity > 0) {
					int temporal_disparity = prev[y * width + x];
					int diff = fabs(pixel_disparity - temporal_disparity);
					if (diff <= 64)
					{
						// apply temporal weights
						filtered = alpha * pixel_disparity + _one_minus_alpha * temporal_disparity;
						depthBuf[y * width + x] = ceil(filtered);
					}

					temproalMap[y * width + x] = 0;
				}
				else
				{
					int old_temporal_disparity = oldest[y * width + x];
					int temporal_disparity = prev[y * width + x];
					int older1_disparity = older1[y * width + x];
					//if (oldest > 0)						
					//	old_temporal_disparity = oldest[y * width + x];

					//if (prev > 0)
					//	temporal_disparity = prev[y * width + x];

					//if (old_temporal_disparity > 0 && old_temporal_disparity < 256 && temporal_disparity > 0 && temporal_disparity < 256)
					if (old_temporal_disparity > 0 && temporal_disparity > 0 && older1_disparity > 0)
					{
						if (temproalMap[y * width + x])
						{
							temproalMap[y * width + x] = 0;
						}
						else
						{
							// has enough confindence in temporal history
							filtered = alpha * old_temporal_disparity + _one_minus_alpha * temporal_disparity;
							depthBuf[y * width + x] = ceil(filtered);
							temproalMap[y * width + x] = 1;
						}
					}
				}
			}
		}

		int bufsize = width * height * bytesPerPixel;
		historyDepthBufferPool.copyToBuffer(depthBuf, bufsize);
	}
}

void DepthmapFilter::InitEdgePreServingFilter(int width, int height)
{
	if (uvMap == 0)
	{
		uvMap = (unsigned char*)malloc(width * height * sizeof(unsigned char));

		uvMapWidth = width;
		uvMapHeight = height;
	}

	if (img2dnormalize == 0)
		img2dnormalize = (float*)malloc(width * height * sizeof(float));
	
	if (useGPU)
		initGaussianLikeFilter_OpenCL(width, height);
}

void DepthmapFilter::FreeEdgePreServingFilter(void)
{
	if (uvMap)
		free(uvMap);
	uvMap = 0;

	if (img2dnormalize)
		free(img2dnormalize);
	img2dnormalize = 0;

	if (useGPU)
		freeGaussianLikeFilter_OpenCL();
}

void DepthmapFilter::EdgePreServingFilter(unsigned char* depthBuf, int type, int width, int height, int level, float sigma, float lumda)
{
	int bytesPerPixel = 2;
	switch (type)
	{
	case 1:
		bytesPerPixel = 1;
		break;
	case 2:
		bytesPerPixel = 2;
		break;
	case 3:
		bytesPerPixel = 2;
		break;
	}

	int bufsize = width * height * bytesPerPixel;
	if (e_buf_size != bufsize)
	{
		if (e_buf_size != 0)
			FreeEdgePreServingFilter();

		InitEdgePreServingFilter(width, height);

		e_buf_size = bufsize;
	}

	occlusion_filter(depthBuf, bytesPerPixel, width, height, uvMap);
	
	if (useGPU)
		processGaussianLikeFilter_OpenCL(depthBuf, uvMap, type, width, height, level, sigma, lumda);
	else
		processGaussianLikeFilter(depthBuf, uvMap, type, width, height, level, sigma, lumda);
}

void DepthmapFilter::processGaussianLikeFilter(unsigned char* depthBuf, unsigned char* uvMap, int type, int width, int height, int level, float sigma, float lumda)
{
	unsigned short * ptr;
	ptr = (unsigned short *)depthBuf;

	float scaling_factor;

	if (type == 1)
		scaling_factor = 256.0f;
	else if (type == 2)
		scaling_factor = 2048.0f;
	else if (type == 3)
		scaling_factor = 16384.0f;

	if (type == 1)
	{
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				img2dnormalize[i * width + j] = depthBuf[i * width + j] * 1.0 / scaling_factor;
			}
		}

		float sigma2 = sigma * sigma;
		for (int k = 0; k < level; k++)
		{
			for (int i = 1; i < height - 1; i++)
			{
				for (int j = 1; j < width - 1; j++)
				{
					if (uvMap[i * width + j] == 1)
					{
						float cN, cS, cE, cW;
						float deltacN, deltacS, deltacE, deltacW;
						float centerD = img2dnormalize[i * width + j];
						float leftD = img2dnormalize[i * width + j - 1];
						float rightD = img2dnormalize[i * width + j + 1];
						float downD = img2dnormalize[(i + 1) * width + j];
						float upD = img2dnormalize[(i - 1) * width + j];

						deltacN = leftD - centerD;
						deltacS = rightD - centerD;
						deltacE = downD - centerD;
						deltacW = upD - centerD;

						cN = abs(exp(-1 * (deltacN * deltacN / sigma2)));
						cS = abs(exp(-1 * (deltacS * deltacS / sigma2)));
						cE = abs(exp(-1 * (deltacE * deltacE / sigma2)));
						cW = abs(exp(-1 * (deltacW * deltacW / sigma2)));

						img2dnormalize[i * width + j] = centerD * (1 - lumda * (cN + cS + cE + cW)) +
							lumda * (cN * leftD + cS * rightD
								+ cE * downD + cW * upD);

						depthBuf[i * width + j] = ceil(img2dnormalize[i * width + j] * scaling_factor);
					}
				}
			}
		}
	}
	else
	{
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				img2dnormalize[i * width + j] = ptr[i * width + j] * 1.0 / scaling_factor;
			}
		}

		float sigma2 = sigma * sigma;
		for (int k = 0; k < level; k++)
		{
			for (int i = 1; i < height - 1; i++)
			{
				for (int j = 1; j < width - 1; j++)
				{
					if (uvMap[i * width + j] == 1)
					{
						float cN, cS, cE, cW;
						float deltacN, deltacS, deltacE, deltacW;
						float centerD = img2dnormalize[i * width + j];
						float leftD = img2dnormalize[i * width + j - 1];
						float rightD = img2dnormalize[i * width + j + 1];
						float downD = img2dnormalize[(i + 1) * width + j];
						float upD = img2dnormalize[(i - 1) * width + j];

						deltacN = leftD - centerD;
						deltacS = rightD - centerD;
						deltacE = downD - centerD;
						deltacW = upD - centerD;

						cN = abs(exp(-1 * (deltacN * deltacN / sigma2)));
						cS = abs(exp(-1 * (deltacS * deltacS / sigma2)));
						cE = abs(exp(-1 * (deltacE * deltacE / sigma2)));
						cW = abs(exp(-1 * (deltacW * deltacW / sigma2)));

						img2dnormalize[i * width + j] = centerD * (1 - lumda * (cN + cS + cE + cW)) +
							lumda * (cN * leftD + cS * rightD
								+ cE * downD + cW * upD);

						ptr[i * width + j] = ceil(img2dnormalize[i * width + j] * scaling_factor);
					}
				}
			}
		}
	}
}

#define KERNEL(...)#__VA_ARGS__

const char gaussian_cl[] = KERNEL(

__kernel void gaussianlike(__global float * input, __global float * output, __global uchar * uvMap, int width, int height,
		int type, int level, float sigma, float lambda)
{
	int gid = get_global_id(0); // unique id for the current work item
	int i = gid / width;        // y position in the depth image
	int j = gid % width;        // x position in the depth image
	float scaling_factor;

	if (type == 1)
		scaling_factor = 256.0f;
	else if (type == 2)
		scaling_factor = 2048.0f;
	else if (type == 3)
		scaling_factor = 16384.0f;

	// Normalize the input depth data
	input[i * width + j] /= scaling_factor;
	
	barrier(CLK_GLOBAL_MEM_FENCE);

	for (int k = 0; k < level; k++)
	{
		if ((i >= 1) && (i < (height - 1)) && (j >= 1) && (j < (width - 1)))
		{
			if (uvMap[i * width + j] == 1)
			{
				float centerD = input[i * width + j];
				float leftD = input[i * width + j - 1];
				float rightD = input[i * width + j + 1];
				float downD = input[(i + 1) * width + j];
				float upD = input[(i - 1) * width + j];

				float deltacN = leftD - centerD;
				float deltacS = rightD - centerD;
				float deltacE = downD - centerD;
				float deltacW = upD - centerD;

				float sigma2 = sigma * sigma;
				float cN = fabs(exp(-1 * (deltacN * deltacN / sigma2)));
				float cS = fabs(exp(-1 * (deltacS * deltacS / sigma2)));
				float cE = fabs(exp(-1 * (deltacE * deltacE / sigma2)));
				float cW = fabs(exp(-1 * (deltacW * deltacW / sigma2)));

				output[i * width + j] = centerD * (1 - lambda * (cN + cS + cE + cW)) + lambda * (cN * leftD + cS * rightD + cE * downD + cW * upD);
			}
			else
				output[i * width + j] = input[i * width + j];
		}
		else
			output[i * width + j] = input[i * width + j];

		barrier(CLK_GLOBAL_MEM_FENCE);
		
		// Copy the result back to input buffer for the next iteration
		input[i * width + j] = output[i * width + j];
		
		barrier(CLK_GLOBAL_MEM_FENCE);
	}

	// Convert the result from normalized range to original dynamic range
	output[i * width + j] = ceil(output[i * width + j] * scaling_factor);
}
);

void DepthmapFilter::initGaussianLikeFilter_OpenCL(int width, int height)
{
	cl_int err_num;
	cl_platform_id pids[100];
	cl_device_id dids[100];
	cl_uint num_platforms, num_devices;

	// Get the number of the platforms in computer
	err_num = clGetPlatformIDs(0, NULL, &num_platforms);
	printf("num_platforms: %d\n", num_platforms);

	// Get the IDs of available platforms
	err_num = clGetPlatformIDs(num_platforms, pids, &num_platforms);
	if (err_num != CL_SUCCESS)
	{
		printf("%s faild, error num: %d\n", "clGetPlatformIDs", err_num);
		exit(-1);
	}

	int flag_device_found = 0;
	cl_platform_id target_pid = NULL;
	cl_device_id target_did = NULL;

	// Get an available GPU-type device
	for (int i = 0; i < num_platforms; i++)
	{
		printf("pids[%d]: %d\n", i, pids[i]);

		err_num = clGetDeviceIDs(pids[i], CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices);
		printf("num_devices: %d\n", num_devices);

		err_num = clGetDeviceIDs(pids[i], CL_DEVICE_TYPE_GPU, num_devices, dids, &num_devices);
		if (err_num != CL_SUCCESS)
		{
			printf("%s faild, error num: %d\n", "clGetDeviceIDs", err_num);
			exit(-1);
		}

		for (int j = 0; j < num_devices; j++)
		{
			char cInfo[100];
			printf("dids[%d]: %d\n", j, dids[j]);
			clGetDeviceInfo(dids[j], CL_DEVICE_OPENCL_C_VERSION, sizeof(cInfo), cInfo, NULL);

			if (strstr(cInfo, "OpenCL C"))
			{
				target_pid = pids[i];
				target_did = dids[j];
				flag_device_found = 1;
			}
		}
	}
	if (flag_device_found)
	{
		printf("target pid: %d\n", target_pid);
		printf("target did: %d\n", target_did);
	}
	else
	{
		puts("No available device!\n");
		exit(-1);
	}


	// Create a context from a GPU-type device
	cl_context_properties properties[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)target_pid, 0 };

	cl_context context = clCreateContextFromType(properties, CL_DEVICE_TYPE_GPU, NULL, NULL, &err_num);
	if (err_num != CL_SUCCESS)
	{
		printf("%s faild, error num: %d\n", "clCreateContextFromType", err_num);
		exit(-1);
	}

	// Create a command queue
	cl_command_queue cq = clCreateCommandQueue(context, target_did, CL_QUEUE_PROFILING_ENABLE, &err_num);
	if (err_num != CL_SUCCESS)
	{
		printf("%s faild, error num: %d\n", "clCreateCommandQueue", err_num);
		exit(-1);
	}

	// Set local work size to 256 and calculate the correspoding global work size
	size_t local_worksize = 256;
	size_t imagesize = width * height;
	size_t global_worksize = ((imagesize + (local_worksize - 1)) / local_worksize) * local_worksize;

	// Allocate buffers for data exchange between the device and the host
	cl_mem mem1, mem2, mem3;
	mem1 = clCreateBuffer(context, CL_MEM_READ_WRITE, global_worksize * sizeof(float), NULL, &err_num);
	mem2 = clCreateBuffer(context, CL_MEM_READ_WRITE, global_worksize * sizeof(float), NULL, &err_num);
	mem3 = clCreateBuffer(context, CL_MEM_READ_WRITE, global_worksize * sizeof(unsigned char), NULL, &err_num);

	// Submit the source code of the kernel to OpenCL
	const char *src = gaussian_cl;
	size_t srcsize = strlen(gaussian_cl);
	const char *srcptr[] = { src };

	cl_program prog = clCreateProgramWithSource(context, 1, srcptr, &srcsize, &err_num);
	if (err_num != CL_SUCCESS)
	{
		printf("%s faild, error num: %d\n", "clCreateProgramWithSource", err_num);
		exit(-1);
	}

	// Build the OpenCL program
	err_num = clBuildProgram(prog, 1, &target_did, "", NULL, NULL);
	if (err_num != CL_SUCCESS)
	{
		printf("%s faild, error num: %d\n", "clBuildProgram", err_num);
		char buffer[100];
		size_t len;
		clGetProgramBuildInfo(prog, target_did, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
		printf("%s\n", buffer);

		exit(-1);
	}

	// Get the kernel from the compiled OpenCL program
	cl_kernel kernel = clCreateKernel(prog, "gaussianlike", &err_num);
	if (err_num != CL_SUCCESS)
	{
		printf("%s faild, error num: %d\n", "clCreateKernel", err_num);
		exit(-1);
	}

	// Save OpenCL parameters and buffers
	clparam.pid = target_pid;
	clparam.did = target_did;
	clparam.context = context;
	clparam.cq = cq;
	clparam.prog = prog;
	clparam.kernel = kernel;
	clparam.mem1 = mem1;
	clparam.mem2 = mem2;
	clparam.mem3 = mem3;
}

void DepthmapFilter::freeGaussianLikeFilter_OpenCL(void)
{
	// Release buffers created by OpenCL
	if (clparam.mem1) clReleaseMemObject(clparam.mem1);
	if (clparam.mem2) clReleaseMemObject(clparam.mem2);
	if (clparam.mem3) clReleaseMemObject(clparam.mem3);
	clparam.mem1 = 0;
	clparam.mem2 = 0;
	clparam.mem3 = 0;

	// Release kernel, program, command queue, and context
	if (clparam.kernel) clReleaseKernel(clparam.kernel);
	if (clparam.prog) clReleaseProgram(clparam.prog);
	if (clparam.cq) clReleaseCommandQueue(clparam.cq);
	if (clparam.context) clReleaseContext(clparam.context);
	clparam.kernel  = 0;
	clparam.prog    = 0;
	clparam.cq      = 0;
	clparam.context = 0;
}

void DepthmapFilter::processGaussianLikeFilter_OpenCL(unsigned char* depthBuf, unsigned char* uvMap, int type, int width, int height, int level, float sigma, float lambda)
{
	unsigned short * ptr;
	ptr = (unsigned short *)depthBuf;

	float scaling_factor;

	if (type == 1)
		scaling_factor = 256.0f;
	else if (type == 2)
		scaling_factor = 2048.0f;
	else if (type == 3)
		scaling_factor = 16384.0f;
	
	// Convert depth data from integer to floating-point
	if (type == 1)
	{
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				img2dnormalize[i * width + j] = depthBuf[i * width + j];
			}
		}
	}
	else
	{
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				img2dnormalize[i * width + j] = ptr[i * width + j];
			}
		}
	}
	
	// Set arguments for OpenCL kernel function
	cl_int err_num;
	cl_kernel kernel = clparam.kernel;
	cl_mem mem1 = clparam.mem1;
	cl_mem mem2 = clparam.mem2;
	cl_mem mem3 = clparam.mem3;
	clSetKernelArg(kernel, 0, sizeof(mem1), &mem1);
	clSetKernelArg(kernel, 1, sizeof(mem2), &mem2);
	clSetKernelArg(kernel, 2, sizeof(mem3), &mem3);
	clSetKernelArg(kernel, 3, sizeof(width), &width);
	clSetKernelArg(kernel, 4, sizeof(height), &height);
	clSetKernelArg(kernel, 5, sizeof(type), &type);
	clSetKernelArg(kernel, 6, sizeof(level), &level);
	clSetKernelArg(kernel, 7, sizeof(sigma), &sigma);
	clSetKernelArg(kernel, 8, sizeof(lambda), &lambda);

	// Set local work size to 256 and calculate the correspoding global work size
	size_t local_worksize = 256;
	size_t imagesize = width * height;
	size_t global_worksize = ((imagesize + (local_worksize - 1)) / local_worksize) * local_worksize;

	// Write input data to the buffer at device-end
	cl_command_queue cq = clparam.cq;
	err_num = clEnqueueWriteBuffer(cq, mem1, CL_TRUE, 0, imagesize*sizeof(float), img2dnormalize, 0, NULL, NULL);
	if (err_num != CL_SUCCESS)
	{
		printf("%s faild, error num: %d\n", "clEnqueueWriteBuffer", err_num);
		exit(-1);
	}

	err_num = clEnqueueWriteBuffer(cq, mem3, CL_TRUE, 0, imagesize * sizeof(unsigned char), uvMap, 0, NULL, NULL);
	if (err_num != CL_SUCCESS)
	{
		printf("%s faild, error num: %d\n", "clEnqueueWriteBuffer", err_num);
		exit(-1);
	}

	// Perform the operation
	err_num = clEnqueueNDRangeKernel(cq, kernel, 1, NULL, &global_worksize, &local_worksize, 0, NULL, NULL);
	if (err_num != CL_SUCCESS)
	{
		printf("%s faild, error num: %d\n", "clEnqueueNDRangeKernel", err_num);
		exit(-1);
	}



	// Read the result back to the buffer at host-end
	err_num = clEnqueueReadBuffer(cq, mem2, CL_TRUE, 0, imagesize*sizeof(float), img2dnormalize, 0, NULL, NULL);
	if (err_num != CL_SUCCESS)
	{
		printf("%s faild, error num: %d\n", "clEnqueueReadBuffer", err_num);
		exit(-1);
	}
	
	// Convert depth data from floating-point to integer
	if (type == 1)
	{
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				depthBuf[i * width + j] = img2dnormalize[i * width + j];
			}
		}
	}
	else
	{
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				ptr[i * width + j] = img2dnormalize[i * width + j];
			}
		}
	}

}

unsigned char* DepthmapFilter::SubSample(unsigned char* depthBuf, int bytesPerPixel, int width, int height, int& new_width, int& new_height, int mode, int factor)
{
	unsigned char* sub_disparity;

	// check condition is allow to process
	if (mode == 0 && (factor > 3 || factor < 2))
		return NULL;
	if (mode == 1 && (factor > 6 || factor < 3))
		return NULL;
	if (width <= 0 || height <= 0)
		return NULL;
	//allow 8 bit
	//if (bytesPerPixel > 2 || bytesPerPixel <= 1)
	//	return NULL;
	if (depthBuf == NULL)
		return NULL;

	new_width = (width- ReservedLeftX) / factor;   // ex. (1280-128) / 2 = 1152/2 = 576
	new_height = height / factor;                  // ex. (720) / 2 = 340
	
	vector<int> sorted_disparity;
	sorted_disparity.resize(9);

	if ( (sub_width != new_width) || (sub_height != new_height) || (sub_bpp != bytesPerPixel) )
	{
		if (sub_disparity_buf != NULL)
			free(sub_disparity_buf);
		sub_disparity_buf = (unsigned char*)malloc(new_width * new_height * bytesPerPixel);
		memset(sub_disparity_buf, 0, new_width * new_height * bytesPerPixel);
		
		sub_width  = new_width;
		sub_height = new_height;
		sub_bpp    = bytesPerPixel;
	}
	sub_disparity = sub_disparity_buf;
	//memset(&sub_disparity, 0, new_width * new_height * bytesPerPixel);

	//parameter: mode = 0 presents using non-zero median filter to perform sub-sampling with the factor ex.(2, 3)
	//           mode = 1 presents using non-zero mean filter to perform sub-sampling with the factor ex.(4, 5)
	if (mode == 0)
	{
		if (bytesPerPixel == 2)
		{
			unsigned short * ptr;
			ptr = (unsigned short *)depthBuf;

			unsigned short * sub_d;
			sub_d = (unsigned short *)sub_disparity;

			for (int i = 0; i < height - factor; i += factor)
			{
				for (int j = ReservedLeftX; j < width - factor; j += factor)
				{
					int index = 0;
					for (int h = 0; h < factor; h++)
					{
						for (int w = 0; w < factor; w++)
						{
							int disparity = ptr[(i + h) * width + j + w];
							if (disparity > 0)
								sorted_disparity[index++] = disparity;
						}
					}

					if (index > 0)
					{
						sort(sorted_disparity.begin(), sorted_disparity.begin() + index);
						int mid = sorted_disparity[index >> 1];

						sub_d[i / factor * new_width + (j - ReservedLeftX) / factor] = mid;
					}
					else
						sub_d[i / factor * new_width + (j - ReservedLeftX) / factor] = 0;
				}
			}

		}
		else if (bytesPerPixel == 1)
		{
			unsigned char * ptr;
			ptr = (unsigned char *)depthBuf;

			unsigned char * sub_d;
			sub_d = (unsigned char *)sub_disparity;

			for (int i = 0; i < height - factor; i += factor)
			{
				for (int j = ReservedLeftX; j < width - factor; j += factor)
				{
					int index = 0;
					for (int h = 0; h < factor; h++)
					{
						for (int w = 0; w < factor; w++)
						{
							int disparity = ptr[(i + h) * width + j + w];
							if (disparity > 0)
								sorted_disparity[index++] = disparity;
						}
					}

					if (index > 0)
					{
						sort(sorted_disparity.begin(), sorted_disparity.begin() + index);
						int mid = sorted_disparity[index >> 1];

						sub_d[i / factor * new_width + (j - ReservedLeftX) / factor] = mid;
					}
					else
						sub_d[i / factor * new_width + (j - ReservedLeftX) / factor] = 0;
				}
			}
		}
	}
	else if (mode == 1)
	{
		if (bytesPerPixel == 2)
		{
			unsigned short * ptr;
			ptr = (unsigned short *)depthBuf;

			unsigned short * sub_d;
			sub_d = (unsigned short *)sub_disparity;

			for (int i = 0; i < height - factor; i += factor)
			{
				for (int j = ReservedLeftX; j < width - factor; j += factor)
				{
					int count = 0;
					int sum = 0;
					for (int h = 0; h < factor; h++)
					{
						for (int w = 0; w < factor; w++)
						{
							int disparity = ptr[(i + h) * width + j + w];
							if (disparity > 0)
							{
								sum += disparity;
								count++;
							}
						}
					}

					if (count > 0)
					{
						int mean = sum/count;

						sub_d[i / factor * new_width + (j - ReservedLeftX) / factor] = mean;
					}
					else
						sub_d[i / factor * new_width + (j - ReservedLeftX) / factor] = 0;
				}
			}

		}
		else if (bytesPerPixel == 1)
		{
			unsigned char * ptr;
			ptr = (unsigned char *)depthBuf;

			unsigned char * sub_d;
			sub_d = (unsigned char *)sub_disparity;

			for (int i = 0; i < height - factor; i += factor)
			{
				for (int j = ReservedLeftX; j < width - factor; j += factor)
				{
					int count = 0;
					int sum = 0;
					for (int h = 0; h < factor; h++)
					{
						for (int w = 0; w < factor; w++)
						{
							int disparity = ptr[(i + h) * width + j + w];
							if (disparity > 0)
							{
								sum += disparity;
								count++;
							}
						}
					}

					if (count > 0)
					{
						int mean = sum / count;

						sub_d[i / factor * new_width + (j - ReservedLeftX) / factor] = mean;
					}
					else
						sub_d[i / factor * new_width + (j - ReservedLeftX) / factor] = 0;
				}
			}
		}
	}

	return sub_disparity;
}

void DepthmapFilter::ApplyFilters(unsigned char* depthBuf, unsigned char* subDisparity, int bytesPerPixel, int width, int height, int sub_w, int sub_h, int threshold)
{
	int factor = height / sub_h;

	if (bytesPerPixel == 2)
	{
		unsigned short * ptr;
		ptr = (unsigned short *)depthBuf;

		unsigned short * sub_d;
		sub_d = (unsigned short *)subDisparity;
		/*
		for (int y = 0; y < sub_h - factor; y++)
		{
			for (int x = 0; x < sub_w - factor; x++)
			{
				int current_pos = y * sub_w + x;
				int sub_disparity = sub_d[current_pos];
				if (sub_disparity > 0)
				{
					for (int h = 0; h < factor; h++)
					{
						for (int w = 0; w < factor; w++)
						{
							int u = y * factor + h;
							int v = x * factor + w;
							int index = u * width + v + ReservedLeftX;
							int disparity = ptr[index];

							if (disparity == 0)
							{
								ptr[index] = sub_disparity;
							}
							else
							{
								if (u % factor == 0 || (u+v) % 2 == 0)
								{
									int offset = abs(disparity - sub_disparity);
									if (offset < threshold)
										ptr[index] = sub_disparity;
								}

								//int offset = disparity - sub_disparity;
								//if (offset < threshold)
								//{
									//weight = (float)sub_disparity / disparity;
									//ptr[index] = sub_disparity;
								//}
							}
						}
					}
				}
			}
		}
		*/
		for (int i = 0; i < height-factor; i++)
		{
			for (int j = ReservedLeftX; j < width- factor; j++)
			{
				int disparity = ptr[i * width + j];
				int sub_disparity = sub_d[i / factor * sub_w + (j - ReservedLeftX) / factor];
				if (disparity == 0)
				{
					if (sub_disparity > 0)
					{
						ptr[i * width + j] = sub_disparity;
					}
				}
				else
				{
					if (j % factor == 0 || (i+j) % 2 == 0)
					{
						int offset = abs(disparity - sub_disparity);
						if (offset < threshold)
							ptr[i * width + j] = sub_disparity;
					}
				}
			}
		}
		
	}
	else if (bytesPerPixel == 1)
	{
		unsigned char * ptr;
		ptr = (unsigned char *)depthBuf;

		unsigned char * sub_d;
		sub_d = (unsigned char *)subDisparity;
		/*
		for (int y = 0; y < sub_h; y++)
		{
			for (int x = 0; x < sub_w; x++)
			{
				int current_pos = y * sub_w + x;
				int sub_disparity = sub_d[current_pos];
				if (sub_disparity > 0)
				{
					for (int h = 0; h < factor; h++)
					{
						for (int w = 0; w < factor; w++)
						{
							int u = y * factor + h;
							int v = x * factor + w;
							int index = u * width + v + ReservedLeftX;
							int disparity = ptr[index];

							if (disparity == 0)
							{
								ptr[index] = sub_disparity;
							}
							else
							{
								if (u % factor == 0 || (u + v) % 2 == 0)
								{
									int offset = abs(disparity - sub_disparity);
									if (offset < threshold)
										ptr[index] = sub_disparity;
								}
							}
						}
					}
				}
			}
		}
		*/
		for (int i = 0; i < height- factor; i++)
		{
			for (int j = ReservedLeftX; j < width- factor; j++)
			{
				int disparity = ptr[i * width + j];
				int sub_disparity = sub_d[i / factor * sub_w + (j - ReservedLeftX) / factor];
				if (disparity == 0)
				{
					if (sub_disparity > 0)
					{
						ptr[i * width + j] = sub_disparity;
					}
				}
				else
				{
					if (j % factor == 0 || (i+j) % 2 == 0)
					{
						int offset = abs(disparity - sub_disparity);
						if (offset < threshold)
							ptr[i * width + j] = sub_disparity;
					}
				}
			}
		}
		
	}
}

void DepthmapFilter::Reset()
{
	reset = true;
}

void DepthmapFilter::EnableGPUAcceleration(bool enable)
{
	useGPU = enable;
}

std::string DepthmapFilter::GetVersion()
{
	std::string version_str_major;
	std::string version_str_minor;
	std::string version_str_revision;
	std::string version_str_full;

	version_str_major    = VERSION_NUMBER_TO_STR(VERSION_MAJOR_NUMBER);
	version_str_minor    = VERSION_NUMBER_TO_STR(VERSION_MINOR_NUMBER);
	version_str_revision = VERSION_NUMBER_TO_STR(VERSION_REVISION_NUMBER);

	version_str_full = version_str_major + "." + version_str_minor + "." + version_str_revision;

	return version_str_full;
}
