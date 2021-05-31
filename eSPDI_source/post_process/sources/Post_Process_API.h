#ifndef _POST_PROCESS_API_H_
#define _POST_PROCESS_API_H_

#include <iostream>
#include <opencv2/opencv.hpp>
#include "Post_Process.h"






class POST_PROCESS_API_Post_Para {
public:
	Spatial_Smoothing_Filter::p_Par p_spatial_smoothing_par;
	Temporal_Smoothing_Filter::p_Par p_temporal_smoothing_par;
	//Motion_Noise_Filter::p_Par p_motion_noise_par;
	//Edge_Noise_Filter::p_Par p_edge_noise_par;
	Spatial_Noise_Filter::p_Par p_spatial_noise_par;
	Median_Filter::p_Par p_median_par;
	Outlier_Filter::p_Par p_outlier_par;
	Saturation_Filter::p_Par p_saturation_par;
	Hole_Filling_Filter::p_Par p_hole_filling_par;

	POST_PROCESS_API_Post_Para(Post_Process* p_post) {

		// post modules
		p_spatial_smoothing_par = &p_post->p_spatial_smoothing_filter->par;
		p_temporal_smoothing_par = &p_post->p_temp_smooting_filter->par;
		//p_motion_noise_par = &p_post->p_motion_noise_filter->par;
		//p_edge_noise_par = &p_post->p_edge_noise_filter->par;
		p_spatial_noise_par = &p_post->p_spatial_noise_filter->par;
		p_median_par = &p_post->p_median_filter->par;
		p_outlier_par = &p_post->p_outlier_filter->par;
		p_saturation_par = &p_post->p_saturation_filter->par;
		p_hole_filling_par = &p_post->p_hole_filling_filter->par;
	}
	~POST_PROCESS_API_Post_Para() {}
};

enum POST_PROCESS_API_DATA_TYPE {
	POST_PROCESS_API_DATA_TYPE_DEPTH_D = 0,
	POST_PROCESS_API_DATA_TYPE_DEPTH_Z = 1,
};

enum POST_PROCESS_API_PRESET_MODES {
	POST_PROCESS_API_PRESET_MODE_DEFAULT = 0, // generalized:Hf off, sm5x1, tf on, dr on, med3 x 3
	POST_PROCESS_API_PRESET_MODE_HAND = 1,
	POST_PROCESS_API_PRESET_MODE_OBSTACLE_AVOIDANCE = 2, // low flying pixels,// Hf off, sm11x1, tf on, dr on, med3 x 7
	POST_PROCESS_API_PRESET_MODE_BASIC = 3, // Hf off, sm3x1, tf on, dr off, med3 x 3
	POST_PROCESS_API_PRESET_MODE_HIGH_ACCURACY = 4, // 720p, Hf off, sm9x1, tf on, dr off, med3 x 1
};

class POST_PROCESS_API {
public:
	POST_PROCESS_API_Post_Para* p_filter_pars;


	// Attributes
	// =================================
	POST_PROCESS_API_DATA_TYPE depth_data_type = POST_PROCESS_API_DATA_TYPE_DEPTH_D;
	//cv::Mat* p_m_intensity;
	//cv::Mat* p_m_depth_src;
	//cv::Mat* p_m_depth_dst;


	void* p_src_data = NULL;
	void* p_dst_data = NULL;
	void* p_intensity = NULL;

	float data_unit = 0.125; // 
	float baseline = 60; //mm
	float focus_x = 556; // 100FOV@720p camera pxiels
	cv::Mat m_intensity;

	POST_PROCESS_API_PRESET_MODES preset_mode = POST_PROCESS_API_PRESET_MODE_DEFAULT;




	// Methods
	// =================================
	int PostProcessing();
	int ResetAllParameters();
	int SetPresetMode(POST_PROCESS_API_PRESET_MODES preset_mode);



	POST_PROCESS_API(
		int n_src_rows, 
		int n_src_cols
		) {

		this->n_src_cols = n_src_cols;
		this->n_src_rows = n_src_rows;

		n_rows = n_src_rows / ds_rate; 
		n_cols = n_src_cols / ds_rate;

		p_post = new Post_Process(n_rows, n_cols, 60);
		p_filter_pars = new POST_PROCESS_API_Post_Para(p_post);

		pp_data = new float*[n_filtering_stages];
		p_m_data = new cv::Mat[n_filtering_stages];
		for (int i = 0; i < n_filtering_stages; i++) {
			p_m_data[i].create(n_rows, n_cols, CV_32FC1); // internal use
			pp_data[i] = (float*)p_m_data[i].data;//new float[n_rows*n_cols];
		}
		m_data_output.create(n_src_rows, n_src_cols, CV_32FC1); // temporal
		m_intensity.create(n_src_rows, n_src_cols, CV_8UC1); // temporal

		m_intensity_ds.create(n_rows, n_cols, CV_8UC1); // temporal
		m_intensity_ds_last.create(n_rows, n_cols, CV_8UC1); // temporal
		m_weight_sum.create(n_rows, n_cols, CV_32FC1);
	}

	~POST_PROCESS_API() {
		delete p_filter_pars;
		delete p_post;

		// Resource
		for (int i = 0; i < n_filtering_stages; i++) {
			p_m_data[i].release();
		}
		delete [] p_m_data;
		delete [] pp_data;
		m_data_output.release();

		m_intensity_ds_last.release();
		m_intensity_ds.release();
		m_intensity.release();
		m_weight_sum.release();
	}

//protected:
	// parameters must be init in constructor();


private: // internal
	Post_Process* p_post;
	const int n_internal_data_bits = 8; // 1byte -- > 256
	int ds_rate = 1; // down sampling rate
	float src2Inter_scaling_rate = 1.0;
	int n_rows = 360;
	int n_cols = 640;
	int n_src_rows = 720;
	int n_src_cols = 1280;
	int n_src_data_bytes = 2;
	const int n_filtering_stages = 4;


	// Resource
	cv::Mat m_data_src; // src
	cv::Mat* p_m_data; // smoothing
	float** pp_data;
	cv::Mat m_data_output; // output
	cv::Mat m_weight_sum;
	cv::Mat m_intensity_ds;
	cv::Mat m_intensity_ds_last;


	// methods
	int DataTransformDecimation(cv::Mat m_Src, cv::Mat m_Dst, int ds_rate, float src2dst_scaling_rate, float invalid_value);

};





#endif