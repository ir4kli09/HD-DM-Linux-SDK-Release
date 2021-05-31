// Post_Process.h

#ifndef _POST_PROCESS_H_
#define _POST_PROCESS_H_

#include <omp.h>
#include <iostream>
#include <stdlib.h>

// OpenCV include
#include <opencv2/opencv.hpp>

#define INACTIVE_PIXEL  0//NAN // (-99999.0f)

using namespace std;

#define  PI  (3.14159265358979323846)
#define PROJECTOR_PIXEL_COUNT (1024)


class Interpreter { // transform pcdxyz to various of pointcloud formats.
public:



};

class Filter {
public:
	bool f_switch;
	virtual bool reset_parameters();
	virtual bool filtering();

	
	Filter() {
		// cout << "Filter()" << endl;
	}

	virtual ~Filter() {
		// cout << "~Filter()" << endl;
	}
};



class Spatial_Smoothing_Filter{
public:
	typedef struct {
		bool f_switch;
		int spatial_filtering_kernel_size;
	}Par, *p_Par;
	Par par;

	bool reset_parameters();
	bool filtering(cv::Mat src, cv::Mat dst, int kernel_size, bool f_src_mask, cv::Mat m_weight_sum);

	Spatial_Smoothing_Filter(){
		par.f_switch = true;
		//sigma = 0.3*((ksize - 1)*0.5 - 1) + 0.8;
		par.spatial_filtering_kernel_size = 5;
	}

	~Spatial_Smoothing_Filter() {
		// cout << "~Spatial_Smoothing_Filter()" << endl;
	}
};

class Outlier_Filter {
public:
	typedef struct {
		bool f_switch;
		float threshold;
	}Par, *p_Par;
	Par par;

	bool reset_parameters();
	bool filtering(cv::Mat m_befroe, cv::Mat m_after, const float threshold);

	Outlier_Filter() {
		par.f_switch = true;
		par.threshold = 2.0f; // var = 0.5, th = 4*var ~ >99%, for disparity
	}

	~Outlier_Filter() {
		// cout << "~Spatial_Smoothing_Filter()" << endl;
	}
};

class Hole_Filling_Filter {
public:
	typedef struct {
		bool f_switch = true;
		int spatial_filtering_kernel_size = 15;
		float outlier_threshold = 2.0f;
		float weighted_density_threshold = 0.10f;
	}Par, *p_Par;
	Par par;

	bool reset_parameters();
	bool filtering(cv::Mat src, cv::Mat dst, int kernel_size, float outlier_threshold, cv::Mat m_weight_sum);

	Hole_Filling_Filter() {
		par.f_switch = false;
		//sigma = 0.3*((ksize - 1)*0.5 - 1) + 0.8;
		par.spatial_filtering_kernel_size = 9;
		par.outlier_threshold = 2.0f;
		par.weighted_density_threshold = 0.50;

		p_ssf = new Spatial_Smoothing_Filter();
		p_of = new Outlier_Filter();
	}

	~Hole_Filling_Filter() {
		delete p_ssf;
		delete p_of;
		// cout << "~Spatial_Smoothing_Filter()" << endl;
	}

private:
	Spatial_Smoothing_Filter* p_ssf;
	Outlier_Filter* p_of;
};

class Saturation_Filter {
public:
	typedef struct {
		bool f_switch;
		int margin;
	}Par, *p_Par;
	Par par;

	bool reset_parameters();
	bool filtering(cv::Mat m_data, float margin);

	Saturation_Filter() {
		par.f_switch = true;
		par.margin = 15;
	}

	~Saturation_Filter() {
		// cout << "~Spatial_Smoothing_Filter()" << endl;
	}
};


class Temporal_Smoothing_Filter{
public:
	typedef struct {
		bool f_switch = true;
		bool f_do_data_recovery = true;
		float temporal_smoothing_coherent_threshold_factor;
		float temporal_smoothing_forgetting_factor;
		float threshold_new_data_intensity;
		float threshold_confidence;
		float confidence_update_rate;
	}Par, *p_Par;
	Par par;

	bool reset_parameters();
	bool filtering(cv::Mat src, cv::Mat dst,  float a, float b);
	bool filtering(cv::Mat src, cv::Mat dst, float a, float b, cv::Mat m_ref1, cv::Mat m_ref2 );
	//bool filtering(cv::Mat src, cv::Mat dst, float a, float b, float base_line, float fx);

	//cv::Mat m_color;

	Temporal_Smoothing_Filter(int n_rows, int n_cols) {

		reset_parameters();
		m_depth_mean.create(n_rows, n_cols, CV_32FC1);
		m_intensity_mean.create(n_rows, n_cols, CV_32FC1);
		m_post_neg_rate.create(n_rows, n_cols, CV_32FC1);
	}

	~Temporal_Smoothing_Filter() {
		m_depth_mean.release();
		m_intensity_mean.release();
		m_post_neg_rate.release();
	}

private:
	cv::Mat m_depth_mean;
	cv::Mat m_intensity_mean;
	cv::Mat m_post_neg_rate;
};


class Motion_Noise_Filter{
public:

	typedef struct {
		bool f_switch = true;
		float motion_noise_filtering_coherent_threshold_factor = 30;
	}Par, *p_Par;
	Par par;

	bool reset_parameters();
	bool filtering (cv::Mat src, cv::Mat dst, const float threshold, const float forgetting_factor);

	Motion_Noise_Filter() {
		// cout << "Motion_Noise_Filter()" << endl;
		par.f_switch = true;
		par.motion_noise_filtering_coherent_threshold_factor = 30;
	}

	~Motion_Noise_Filter() {
		// cout << "~Motion_Noise_Filter()" << endl;
	}

};

class Edge_Noise_Filter{
public:

	typedef struct {
		bool f_switch = true;
		float edge_noise_filtering_gap_threshold_factor; //z
		int edge_noise_filtering_window_size;
	}Par, *p_Par;
	Par par;

	cv::Mat Edge_noise_mask;

	bool reset_parameters();
	bool filtering(
		// in
		cv::Mat Data, // z
		const float GAP_VALUE_RATIO,
		const int WIN_SIZE,
		int n_threads
	);

	Edge_Noise_Filter() {
		// cout << "Edge_Noise_Filter() " << endl;
		par.f_switch = true;
		par.edge_noise_filtering_gap_threshold_factor = 0.038;
		par.edge_noise_filtering_window_size = 3;
	}

	Edge_Noise_Filter(int n_rows, int n_cols) {
		// cout << "Edge_Noise_Filter(int n_rows, int n_cols) " << endl;
		par.f_switch = true;
		par.edge_noise_filtering_gap_threshold_factor = 0.038;
		par.edge_noise_filtering_window_size = 2;
		allocate_resource(n_rows, n_cols);
	}

	~Edge_Noise_Filter() {
		// cout << "~Edge_Noise_Filter() " << endl;
		release_resource();
	}

	friend class Post_Process;

private:
	bool allocate_resource(int n_rows, int n_cols) {
		// cout << "Edge_Noise_Filter::allocate_resource()" << endl;
		Edge_noise_mask.create(n_rows, n_cols, CV_8U);
		return true;
	}

	bool release_resource() {
		// cout<<"Edge_Noise_Filter::release_resource()"<<endl;
		Edge_noise_mask.release();
		return true;
	}



};

class Spatial_Noise_Filter{
public:

	typedef struct {
		bool f_switch = false;
		float spatial_noise_filtering_neighboor_dist_threshold_factor = 5; // in z 
		int spatiale_noise_filtering_n_iteration =2 ;
	}Par, *p_Par;

	Par par;

	bool reset_parameters();
	bool filtering(
		cv::Mat Data, //wpxb
		cv::Mat Dst, //z
		const int N_ITERATION,
		const float DIST_THRESHOLD
	);

	Spatial_Noise_Filter(int n_rows, int n_cols) {
		// cout << "Spatial_Noise_Filter()" << endl;

		par.f_switch = true;
		par.spatial_noise_filtering_neighboor_dist_threshold_factor = 5;
		par.spatiale_noise_filtering_n_iteration = 2;
		m_data_tmp.create(n_rows, n_cols, CV_32FC1);
		m_N_bad_neighboors.create(n_rows, n_cols, CV_8UC1);

	}

	~Spatial_Noise_Filter() {
		// cout << "~Spatial_Noise_Filter()" << endl;
		m_data_tmp.release();
		m_N_bad_neighboors.release();
	}


	friend class Post_Process;

private:

	cv::Mat m_data_tmp;
	cv::Mat m_N_bad_neighboors;

	bool allocate_resource(int n_rows, int n_cols) {
		// cout << "Spatial_Noise_Filter::allocate_resource()" << endl;
		m_data_tmp.create(n_rows, n_cols, CV_32FC1);
		m_N_bad_neighboors.create(n_rows, n_cols, CV_8UC1);
		return true;
	}

	bool release_resource() {
		// cout << "Spatial_Noise_Filter::release_resource()" << endl;
		m_data_tmp.release();
		m_N_bad_neighboors.release();
		return true;
	}



};

class Median_Filter{
public:
	typedef struct {
		bool f_switch = true;
		int median_filtering_kernel_size = 3;
		bool  f_use_tcv = false;
		bool ds_rate = 1;
		int n_interations = 5;
	}Par, *p_Par;
	Par par;

	bool reset_parameters();
	bool filtering(cv::Mat src, cv::Mat dst, int kernel_size, bool  f_use_tcv);

	Median_Filter() {
		// cout << "Median_Filter()" << endl;
		par.f_switch = true;
		par.median_filtering_kernel_size = 3;
		par.f_use_tcv = false;
	}

	~Median_Filter() {
		// cout << "~Median_Filter()" << endl;
	}
};


class Post_Process {

public:
	// input parameters
	int n_rows;
	int n_cols;
	float utilized_threading_ratio;

	bool f_rx_based_depthmap;
	bool f_recover_invalid_pixels_after_smoothed;

	int n_rxCam_blanking_pixels;
	int n_txProj_blanking_pixels;

	// internel
	
	cv::Mat m_Wpx_b;
	cv::Mat m_Wpx_b_last;
	cv::Mat m_phase_last; // +++Red@20201023: PI
	cv::Mat m_phase_internal; // +++Red@20201023: PI
	cv::Mat m_Z;

	float wpbx_pitch; //mm
	float wpbx_effective_half_filed_length; //mm


	typedef struct {
		// Inputs
	}Inputs, *p_Inputs;

	typedef struct {
		// Outputs
		cv::Mat m_depth_map;
		cv::Mat m_PCD_XYZ;
	}Outputs, *p_Outputs;

	Inputs inputs;
	Outputs outputs;

	// Modules
	Spatial_Smoothing_Filter* p_spatial_smoothing_filter;
	Temporal_Smoothing_Filter* p_temp_smooting_filter;
	Edge_Noise_Filter* p_edge_noise_filter;
	Motion_Noise_Filter* p_motion_noise_filter;
	Spatial_Noise_Filter* p_spatial_noise_filter;
	Median_Filter* p_median_filter;
	Outlier_Filter* p_outlier_filter;
	Saturation_Filter* p_saturation_filter;
	Hole_Filling_Filter* p_hole_filling_filter;

	Post_Process(int n_rows, int n_cols, float field_angle) {
		// cout<<" Post_Process(int n_rows, int n_cols, float field_angle) "<<endl;

		this->n_rows = n_rows;
		this->n_cols = n_cols;
		utilized_threading_ratio = 0.5;
		//f_rx_based_depthmap = true;
		f_recover_invalid_pixels_after_smoothed = true;
		n_rxCam_blanking_pixels = 5;
		n_txProj_blanking_pixels = 24;

		//n_threads = int(omp_get_num_procs() * utilized_threading_ratio + 0.5);
		wpbx_effective_half_filed_length = tan(field_angle/2 / 180 * PI);
		wpbx_pitch = wpbx_effective_half_filed_length / (PROJECTOR_PIXEL_COUNT / 2);
		allocate_resource();

		// create modules instance //+++Red@20200911
		p_spatial_smoothing_filter = new Spatial_Smoothing_Filter();
		p_temp_smooting_filter = new Temporal_Smoothing_Filter(n_rows, n_cols);
		p_edge_noise_filter = new Edge_Noise_Filter(n_rows, n_cols);
		p_motion_noise_filter = new Motion_Noise_Filter();
		p_spatial_noise_filter = new Spatial_Noise_Filter(n_rows, n_cols);
		p_median_filter = new Median_Filter();
		p_outlier_filter = new Outlier_Filter();
		p_saturation_filter = new Saturation_Filter();
		p_hole_filling_filter = new Hole_Filling_Filter();
	}

	~Post_Process() {
		// cout<<"~Post_Process"<<endl;
		release_resource();

		delete  p_spatial_smoothing_filter;
		delete  p_temp_smooting_filter;
		delete  p_edge_noise_filter;
		delete  p_motion_noise_filter;
		delete  p_spatial_noise_filter;
		delete  p_median_filter;
		delete  p_outlier_filter;
		delete  p_saturation_filter;
		delete  p_hole_filling_filter;
		//delete  p_pointcloud_statistics;
	}

	bool set_utilized_threading_ratio(float utilized_threading_ratio) {
		
		return true;
	}

	bool allocate_resource() {
		m_Wpx_b.create(n_rows, n_cols, CV_32FC1);
		m_Wpx_b_last.create(n_rows, n_cols, CV_32FC1);
		m_phase_last.create(n_rows, n_cols, CV_32FC1);
		m_phase_internal.create(n_rows, n_cols, CV_32FC1);
		m_Z.create(n_rows, n_cols, CV_32FC1);
		//p_edge_noise_filter->allocate_resource(n_rows, n_cols);

		outputs.m_depth_map.create(n_rows, n_cols, CV_16UC1);
		outputs.m_PCD_XYZ.create(n_rows, n_cols, CV_32FC3);

		return true;
	}

	bool release_resource() {
		m_Wpx_b.release();
		m_Wpx_b_last.release();
		m_phase_last.release();
		m_phase_internal.release();
		m_Z.release();
		//c_edge_noise_filter.release_resource();

		outputs.m_depth_map.release();
		outputs.m_PCD_XYZ.release();

		return true;
	}



	void set_utilized_threads_num(int n_threads);



	friend class SLA_API;

private: // internel, calculated parameters
	int n_threads;

	// Methods
#if 0
	bool phase_to_z(
		// in
		cv::Mat Phase,
		cv::Mat Phase_tmp,
		const cv::Mat Ap,
		const cv::Mat Ac,
		const cv::Mat Rp,
		const cv::Mat tp,
		const float Z_MIN,
		const float Z_MAX,

		// out
		cv::Mat Z
	);


	bool z_to_pcdxyz_and_depthmap(
		// in
		cv::Mat Z,
		cv::Mat Ac,
		cv::Mat R,
		cv::Mat T,
		const float Z_MIN,
		const float Z_MAX,
		//bool f_rx_based_depthmap,

		// out
		cv::Mat Pcd_xyz,
		cv::Mat Depth_map
	);
#endif

	bool temporal_filtering( // +++Red@20201018

		cv::Mat m_data_new,
		cv::Mat m_data,
		const bool f_switch_temporal_smoothing,
		const float temporal_smoothing_coherent_threshold_factor,
		const float temporal_smoothing_forgetting_factor
		);


};



#endif