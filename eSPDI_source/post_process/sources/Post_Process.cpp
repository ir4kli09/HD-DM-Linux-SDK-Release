#include "Post_Process.h"
#include <opencv2/opencv.hpp>
#include "tcvMedianBlur16U.hpp" // Red_20190720, add for mediam filtering.

using namespace cv;
using namespace tcv;



using std::cout;
using std::endl;
using namespace cv;


#define WPXB_UNIT_PITCH (0.00112763724) //tan(30/180*pi)/512
#define MAX_PIXELS (1024000)
#define N_BAD_NB_THRESHOLD (1) // pixels has bad neighboors more than this number will be set as inactive pixel.
#include "Public_Tool.h"
bool Hole_Filling_Filter::reset_parameters() {

	par.f_switch = false;
	par.spatial_filtering_kernel_size = 15;
	par.outlier_threshold = 2.0f;
	par.weighted_density_threshold = 0.10f;

	return 0;
}

bool Spatial_Smoothing_Filter::reset_parameters() {
	bool f_switch = true;
	int spatial_filtering_kernel_size = 3;
	return 0;
}

bool Median_Filter::reset_parameters() {
	par.f_switch = true;
	par.median_filtering_kernel_size = 3;
	par.f_use_tcv = false;
	par.ds_rate = 1;
	par.n_interations = 5;

	return 0;
}



bool Hole_Filling_Filter::filtering(Mat src, Mat dst, int kernel_size, float outlier_threshold, Mat m_weight_sum) {

	//Spatial_Smoothing_Filter ssf()
	if (kernel_size < 3)
		kernel_size = 3;
	static cv::Mat m_tmp1(src.rows, src.cols, CV_32FC1);
	static cv::Mat m_tmp2(src.rows, src.cols, CV_32FC1);
	static cv::Mat m_tmp(src.rows, src.cols, CV_32FC1);
	p_ssf->filtering(src, m_tmp1, kernel_size, false, m_weight_sum);
	p_ssf->filtering(m_tmp1, m_tmp2, 5, true, m_tmp);

	p_of->filtering(m_tmp1, m_tmp2, outlier_threshold);

	//memcpy(dst.data, src.data, sizeof(float)*src.rows * src.cols);
	for (int i = 0; i < m_tmp2.rows * m_tmp2.cols; i++) {
		float data1 = ((float*)src.data)[i]; // soruce data
		float data2 = ((float*)m_tmp2.data)[i]; // data filled and clear outlier
		float tmp = ((float*)m_weight_sum.data)[i]; // data filled and clear outlier
		if ((data1 == 0)&&(data2 > 0)&&(tmp > par.weighted_density_threshold)) { // can be filled
			((float*)dst.data)[i] = data2;
		}
		else {
			((float*)dst.data)[i] = data1; // eq to the src
		}
	}

	return true;
}


bool Spatial_Smoothing_Filter::filtering(Mat src, Mat dst, int kernel_size, bool f_src_mask, Mat m_weight_sum) {
	if (par.f_switch) {
		if (kernel_size < 3)
			kernel_size = 3;

		//cv::Mat m_tmp(src.rows, src.cols, CV_32FC1);
		for (int i = 0; i < src.rows*src.cols; i++) {
			if (((float*)src.data)[i] > 0)
				((float*)(m_weight_sum.data))[i] = 1.0f;
			else
				((float*)(m_weight_sum.data))[i] = 0.0f;
		}
		GaussianBlur(m_weight_sum, m_weight_sum, Size(kernel_size, kernel_size), 0);

		GaussianBlur(src, dst, Size(kernel_size, kernel_size), 0);

		float* p_div = NULL;
		if (f_src_mask)
			p_div = (float*)src.data;
		else
			p_div = (float*)m_weight_sum.data;

		for (int i = 0; i < src.rows*src.cols; i++) {
			if (p_div[i] > 0) // not div zero
			{
				((float*)(dst.data))[i] /= ((float*)m_weight_sum.data)[i];
			}
			else {
				((float*)(dst.data))[i] = INACTIVE_PIXEL;
			}
		}
	}


	return true;
}

//((float*)(m_tmp.data))[i] = 1;
bool Outlier_Filter::filtering(cv::Mat m_src, cv::Mat m_dst, const float threshold) {

	const int N_COLS = m_src.cols;
	const int N_ROWS = m_src.rows;
	for (int i = 0; i < N_ROWS*N_COLS; i++) {

		float data_a = ((float*)m_src.data)[i];
		float data_b = ((float*)m_dst.data)[i];
		if (!(abs(data_a - data_b) < threshold)) // noise
			((float*)m_dst.data)[i] = INACTIVE_PIXEL;
	};
	return 0;
}




#if 0
bool Post_Process::phase_to_z(
	// in
	Mat Phase, 
	Mat Phase_tmp,
	const Mat Ap,
	const Mat Ac,
	const Mat Rp,
	const Mat tp,
	const float Z_MIN, 
	const float Z_MAX,

	// out
	Mat Z
	) {
	const float UC0 = Ac.at<float>(0, 2);
	const float VC0 = Ac.at<float>(1, 2);
	const float D_FCX = 1.0f / Ac.at<float>(0, 0);
	const float D_FCY = 1.0f / Ac.at<float>(1, 1);
	const float R00 = Rp.at<float>(0, 0);
	const float R20 = Rp.at<float>(2, 0);
	const float R01 = Rp.at<float>(0, 1);
	const float R21 = Rp.at<float>(2, 1);
	const float R02 = Rp.at<float>(0, 2);
	const float R22 = Rp.at<float>(2, 2);
	const float TP_X = tp.at<float>(0, 0);
	const float TP_Z = tp.at<float>(2, 0);

	float tmp = 0;
	if (p_temp_smooting_filter->par.f_switch)
		 tmp = p_temp_smooting_filter->par.temporal_smoothing_coherent_threshold_factor * WPXB_UNIT_PITCH;
	else
		tmp = 0;
	const float TEMP_SMOOTHING_COHERENT_TH = tmp;
	const float TEMP_SMOOTHING_ALFA = p_temp_smooting_filter->par.temporal_smoothing_forgetting_factor;
	const float MOTION_NOISE_COHERENT_TH = p_motion_noise_filter->par.motion_noise_filtering_coherent_threshold_factor * WPXB_UNIT_PITCH;
	const bool F_DO_MOTION_NOISE_FILTERING = p_motion_noise_filter->par.f_switch;
	const bool F_RECOVER_INVALID_PIXELS_AFTER_SMOOTHED = f_recover_invalid_pixels_after_smoothed&p_spatial_smoothing_filter->par.f_switch;

	const int n_ROWS = Phase.rows;
	const int n_COLS = Phase.cols;
	const float WPXB_HALF_FILED_LENGTH = wpbx_effective_half_filed_length;
	const bool DO_SPATIAL_SMOOTHING = p_spatial_smoothing_filter->par.f_switch;

	const float* p_phase = (float*)(Phase.data);
	const float* p_phase_before_sm = (float*)(Phase_tmp.data);

	//const float* p_Ap = (float*)(Ap.data);
	float* p_wpx_b = (float*)(m_Wpx_b.data);
	float* p_last_wpx_b = (float*)(m_Wpx_b_last.data);
	float* p_z = (float*)(Z.data);
	const float p_Ap[7] = {
		((float*)Ap.data)[0],
		((float*)Ap.data)[1],
		((float*)Ap.data)[2],
		((float*)Ap.data)[3],
		((float*)Ap.data)[4],
		((float*)Ap.data)[5],
		((float*)Ap.data)[6] };

#pragma omp parallel num_threads(n_threads)//private(tid)
	{

#pragma omp for
		for (int vc = 0; vc < n_ROWS; vc++) { // parallel.
			float tmp = 0;
			float Wcx_b = 0, Wcy_b = 0, Wpx_b = 0;
			register int n = 0;
			register float z_tmp;
			float z;
			float Wpx_b_tmp = 0;
			register float up = 0;
			float up_tmp = 0;
			float up2, up3, up4;
			for (int uc = 0; uc < n_COLS; uc++) { // 4 elements as one batch
				n = vc * n_COLS + uc;

				//0. reset to NAN
				z_tmp = INACTIVE_PIXEL;
				z = INACTIVE_PIXEL;
				Wpx_b = INACTIVE_PIXEL;

				up = p_phase[n];
				//if (F_RECOVER_INVALID_PIXELS_AFTER_SMOOTHED) {
				//	up = p_phase[n];
				//	if (up < 0) {
				//		up = p_phase_before_sm[n];
				//	}
				//}
				//else {
				//	up = p_phase_before_sm[n];
				//}

				if ((up >= 0)&(up<=1023)) {
					up2 = up * up;
					up3 = up2 * up;
					up4 = up3 * up;
					Wpx_b_tmp =
						up4 * p_Ap[0] +
						up3 * p_Ap[1] +
						up2 * p_Ap[2] +
						up * p_Ap[3] +
						p_Ap[4] +
						vc * p_Ap[5] +
						(1.0f / up)*p_Ap[6];

					//if ((Wpx_b_tmp < WPXB_HALF_FILED_LENGTH)&(Wpx_b_tmp > -WPXB_HALF_FILED_LENGTH))
					{
						////cout << "Wpx_b_tmp" << Wpx_b_tmp << endl;
						if (abs(Wpx_b_tmp - p_wpx_b[n]) < TEMP_SMOOTHING_COHERENT_TH) {
							Wpx_b_tmp = p_wpx_b[n] * (1.0 - TEMP_SMOOTHING_ALFA) + Wpx_b_tmp * TEMP_SMOOTHING_ALFA;
						}

						Wcx_b = ((float)uc - UC0) * D_FCX;
						Wcy_b = ((float)vc - VC0) * D_FCY;
						//tmp = ;
						z_tmp = (Wpx_b_tmp * TP_Z - TP_X) / (Wcx_b * (R00 - Wpx_b_tmp * R20) + Wcy_b * (R01 - Wpx_b_tmp * R21) + (R02 - Wpx_b_tmp * R22));

						if ((z_tmp > Z_MIN) & (z_tmp < Z_MAX)) { // valid point cloud
							
							// Motion noise
							if (F_DO_MOTION_NOISE_FILTERING) {
								if ((abs(p_last_wpx_b[n] - Wpx_b_tmp)) < MOTION_NOISE_COHERENT_TH)
								{
									Wpx_b = Wpx_b_tmp;
									z = z_tmp;
								}
							}
							else {
								Wpx_b = Wpx_b_tmp;
								z = z_tmp;
							}
						}//z
					}// utmp
				}// valid up
				p_wpx_b[n] = Wpx_b;
				p_z[n] = z;
				p_last_wpx_b[n] = Wpx_b_tmp; //tmp
			}
		}
	} // opnemp


	return true;
}

bool Post_Process::z_to_pcdxyz_and_depthmap(
	// in
	Mat Z,
	Mat Ac,
	Mat R,
	Mat T,
	const float DEPTH_MAPPING_Z_START,
	const float DEPTH_MAPPING_Z_END,

	// out
	Mat Pcd_xyz,
	Mat Depth_map
) {
	//cout << "z_to_pcdxyz_and_depthmap: " << endl;
	const int N_COLS = Z.cols;
	const int N_ROWS = Z.rows;
	float* p_z = (float*)Z.data;

	const float Z_RANGE_DIV = 65535.0f / (DEPTH_MAPPING_Z_END - DEPTH_MAPPING_Z_START);

	const float UC0 = Ac.at<float>(0, 2);
	const float VC0 = Ac.at<float>(1, 2);
	const float D_FCX = 1.0f / Ac.at<float>(0, 0);
	const float D_FCY = 1.0f / Ac.at<float>(1, 1);

	float* p_xyz = (float*)Pcd_xyz.data;
	unsigned short* p_depth = (unsigned short*)Depth_map.data;
	memset(p_xyz, 0, sizeof(float)*N_COLS * N_ROWS * 3);
	memset(p_depth, 0, sizeof(unsigned short) *N_ROWS *N_COLS);

	//const float DEF_MF_DEPTH_MAPPING_FACTOR = (1.0 / 2000 * 65535);
	//static float* p_z_tmp = (float*)malloc(sizeof(float) * N_COLS * N_ROWS);
#pragma omp parallel num_threads(n_threads)//private(tid)
	{
#pragma omp for
		for (int vc = 0; vc < N_ROWS; vc++) {
			int n = 0;
			int idx = 0;
			float z = 0;
			float z_tmp = 0;

			//float tmp = 0;
			//if (p_temp_smooting_filter->par.f_switch)
			//	tmp = p_temp_smooting_filter->par.temporal_smoothing_coherent_threshold_factor * WPXB_UNIT_PITCH;
			//else
			//	tmp = 0;
			//const float TEMP_SMOOTHING_COHERENT_TH = tmp;
			//const float temporal_smoothing_coherent_threshold_factor = p_temp_smooting_filter->par.temporal_smoothing_coherent_threshold_factor*0.005;
			//const float TEMP_SMOOTHING_ALFA = p_temp_smooting_filter->par.temporal_smoothing_forgetting_factor;


			for (int uc = 0; uc < N_COLS; uc++) { // 4 elements as one batch
				n = vc * N_COLS + uc;
				z = p_z[n];

				if (z > 0) { //valid
					//float th = z * temporal_smoothing_coherent_threshold_factor;
					//if (th < 0.0001)
					//	th = 0.0001;
					//else if (th > 0.05)
					//	th = 0.05;

					//if (abs(z - p_z_tmp[n]) < th)
					//	z = p_z_tmp[n] * (1.0 - TEMP_SMOOTHING_ALFA) + z * TEMP_SMOOTHING_ALFA;

					idx = n * 3;
					p_xyz[idx++] = ((float)uc - UC0) * D_FCX * z;
					p_xyz[idx++] = ((float)vc - VC0) * D_FCY * z;
					p_xyz[idx] = z;
					//p_depth[n] = (unsigned short)(65535 - z * DEF_MF_DEPTH_MAPPING_FACTOR + 0.5);// uint16_t
					p_depth[n] = (unsigned short)((z - DEPTH_MAPPING_Z_START)* Z_RANGE_DIV + 0.5);// uint16_t
				}
				//p_z_tmp[n] = z;
			}//uc
		}//vc

	}

	return true;
}
#endif

bool Edge_Noise_Filter::filtering(
	// in
	Mat Data, 
	const float GAP_VALUE_RATIO,
	const int WIN_SIZE,
	int n_threads
) {
	if (par.f_switch) {
		// cout << "edge_noise_filtering: " << endl;
		bool* p_edge_noise_mask = (bool*)(Edge_noise_mask.data);
		float* p_data = (float*)(Data.data);
		const int N_COLS = Data.cols;
		const int N_ROWS = Data.rows;

		//2. filtering:
		memset(p_edge_noise_mask, false, N_COLS * N_ROWS * sizeof(bool));

#pragma omp parallel num_threads(n_threads)//private(tid)
		{
#pragma omp for
			for (int vc = 0; vc < N_ROWS; vc++) { // parallel.
				float value_L, value_R;
				int n = 0;
				float d = 0;
				register float th = 0;

				for (int uc = WIN_SIZE; uc < N_COLS - WIN_SIZE; uc++) { // 4 elements as one batch
					n = vc * N_COLS + uc;

					if ((uc < WIN_SIZE) | (uc >= N_COLS - WIN_SIZE)) {
						p_edge_noise_mask[n] = true;
					}
					else {
						value_L = p_data[n - WIN_SIZE];
						value_R = p_data[n + WIN_SIZE];

						if (value_L > value_R) {
							th = value_L * GAP_VALUE_RATIO;
							d = value_L - value_R;
						}
						else {
							th = value_R * GAP_VALUE_RATIO;
							d = value_R - value_L;
						}

						if (d > th) { // edge
							p_edge_noise_mask[n] = true;
						}
					}
				} //uc

				// invalid pixles
				n = vc * N_COLS;
				for (int uc = 0; uc < N_COLS; uc++) { // edge noise
					if (p_edge_noise_mask[n]) {
						p_data[n] = INACTIVE_PIXEL;
					}

					n++;
				} // uc
			} //vc
		}// open-mp

	}


	return true;
}



bool Spatial_Noise_Filter::filtering(
	Mat Data, //wpxb
	Mat Dst, //z
	const int N_ITERATION,
	const float DIST_THRESHOLD
) {
	if (par.f_switch) {
		//cout << "spatial_noise_filtering: " << endl;

		const int N_COLS = Data.cols;
		const int N_ROWS = Data.rows;
		//static unsigned char N_bad_neighboors[MAX_PIXELS] = { 0 };
		//static Mat Data_tmp(1, MAX_PIXELS, CV_32FC1);
		unsigned char* p_n_bad_neighboors = ((unsigned char*)m_N_bad_neighboors.data);//p_abs_phase_h;
		float* p_data = ((float*)Data.data);//p_abs_phase_h;
		float* p_data_tmp = (float*)(m_data_tmp.data);
		float* p_dst = (float*)(Dst.data);
		memcpy(p_data_tmp, p_data, N_COLS * N_ROWS * sizeof(float));

		int n = 0;
		int s = 0, e = 0;

		for (int k = 0; k < N_ITERATION; k++) {

			// reset
			memset(p_n_bad_neighboors, 0, N_COLS * N_ROWS * sizeof(unsigned char));

			for (int vc = 0; vc < N_ROWS; vc++) { // parallel.

				if (vc == 0) { // 1st row, boundry
					for (n = 1; n < N_COLS; n++) { // 4 elements as one batch
						if (abs(p_data[n] - p_data[n - 1]) > DIST_THRESHOLD) {
							p_n_bad_neighboors[n] ++;
							p_n_bad_neighboors[n - 1] ++;
						}
					}
				}
				else { // >2nd row
					int const e = vc * N_COLS + N_COLS;
					for (n = vc * N_COLS + 1; n < e; n++) { // 4 elements as one batch
						if (abs(p_data[n] - p_data[n - 1]) > DIST_THRESHOLD) {
							p_n_bad_neighboors[n] ++;
							p_n_bad_neighboors[n - 1] ++;
						}

						if (abs(p_data[n] - p_data[n - N_COLS]) > DIST_THRESHOLD) {
							p_n_bad_neighboors[n] ++;
							p_n_bad_neighboors[n - N_COLS] ++;
						}

						// Red_20191016 
						if (p_n_bad_neighboors[n - N_COLS] > N_BAD_NB_THRESHOLD) { // bad pixel?
							p_data_tmp[n - N_COLS] = INACTIVE_PIXEL;
							//p_dst[n] = INACTIVE_PIXEL;
						}
					} // for uc
					// last row ,boundry
					if (vc == N_ROWS) {
						int const e = vc * N_COLS + N_COLS;
						for (n = e - N_COLS; n < e; n++) { // 4 elements as one batch
							if (p_n_bad_neighboors[n] > N_BAD_NB_THRESHOLD) { // bad pixel?
								p_data_tmp[n] = INACTIVE_PIXEL;
								//p_dst[n] = INACTIVE_PIXEL;
							}
						}
					} // in uc
				}  // vc>0
			}// for vc
			memcpy(p_data, p_data_tmp, N_COLS * N_ROWS * sizeof(float));
		} // iteration
		
	}


	return true;
}



bool Median_Filter::filtering(Mat src, Mat dst, int kernel_size, bool  f_use_tcv) {

	if (par.f_switch) {
		// cout << "median_filtering: " << endl;
		if (f_use_tcv)
			MedianBlur(src, dst, kernel_size);
		else
			medianBlur(src, dst, kernel_size);
	}


	return true;
}



void Post_Process::set_utilized_threads_num(int _n_threads) {
	n_threads = _n_threads;
}

bool Temporal_Smoothing_Filter::filtering( // +++Red@20201018

	cv::Mat m_data_new,
	cv::Mat m_data,
	const  float temporal_smoothing_coherent_threshold_factor,
	const  float temporal_smoothing_forgetting_factor, 
	cv::Mat m_ref1,
	cv::Mat m_ref2
) {

	if (par.f_switch) {
		//imshow("m_data_last", m_data_last);
		//cv::waitKey(1);
		//PublicTool::float_im_show(m_data_last, m_data_last, 255, "m_data_last", 1, 0);
		

		const int n_ROWS = m_data.rows;
		const int n_COLS = m_data.cols;
		//cv::Mat m_tmp1(n_ROWS, n_COLS, CV_8UC1);
		//cv::Mat m_tmp2(n_ROWS, n_COLS, CV_8UC1);
		//cv::Mat m_tmp3(n_ROWS, n_COLS, CV_8UC1);
		const float one_minus_alfa = 1.0f - temporal_smoothing_forgetting_factor;
		const float* p_data_new = ((float*)m_data_new.data);
		float* p_data = ((float*)m_data.data);
		float data_new = 0;
		float data = 0;
		//float* p_data_last = (float*)m_data_last.data;
		float* p_mean_depth = (float*)m_depth_mean.data;
		float* p_mean_intensity = (float*)m_intensity_mean.data;
		//int* p_stable_cnt = (int*)m_stable_cnt.data;
		//int* p_flip_cost = (int*)m_flip_cost.data;
		unsigned char* p_ref1 = (unsigned char*)m_ref1.data;
		unsigned char* p_ref2 = (unsigned char*)m_ref2.data;
		float* p_post_neg_rate = (float*)m_post_neg_rate.data;
		//bool f_do_data_recovery = true;
		float intensity_diff = 0;
		float mean_depth_update_rate = 1.0f;
		//float confidence = 0;
		for (int i = 0; i < n_ROWS*n_COLS; i++) {
			data_new = p_data_new[i];
			
			if (par.f_do_data_recovery){
			
				//m_tmp1.data[i] = 0;
				//m_tmp2.data[i] = 0;
				//m_tmp1.data[i] = abs(p_ref1[i] - p_mean_intensity[i]);
				intensity_diff = abs(p_ref1[i] - p_mean_intensity[i]);
				p_mean_intensity[i] = p_mean_intensity[i] * 0.75 + p_ref1[i] * 0.25;

				//if (p_mean_intensity[i] > par.threshold_saturation)
				//	data_new = 0;

				float confidence_update_rate = par.confidence_update_rate + intensity_diff / par.threshold_new_data_intensity*(1 - par.confidence_update_rate);
				if (confidence_update_rate > 1.0f)
					confidence_update_rate = 1.0f;

				if (data_new > 0) {

					if (abs(data_new - p_mean_depth[i]) > 3 * temporal_smoothing_coherent_threshold_factor)
						confidence_update_rate = 1.0f;

					p_post_neg_rate[i] = p_post_neg_rate[i] * (1 - confidence_update_rate) + confidence_update_rate;
					mean_depth_update_rate = p_post_neg_rate[i]; // 1: confidence,  -->1: high rate --> faster update rate of new data , 0. low confidence, slower update rate
					p_mean_depth[i] = p_mean_depth[i] * (1 - mean_depth_update_rate) + data_new * mean_depth_update_rate;
				}
				else
					p_post_neg_rate[i] = p_post_neg_rate[i] * (1 - confidence_update_rate);


				if (p_post_neg_rate[i] < par.threshold_confidence) {
					data_new = 0;
				}
				else {
					data_new = p_mean_depth[i];
				}

			}


			data = p_data[i];
			if (abs(data_new - data) < temporal_smoothing_coherent_threshold_factor) { // smoothing
				p_data[i] = data * one_minus_alfa + data_new * temporal_smoothing_forgetting_factor;
			}
			else {
				p_data[i] = data_new;
			}
		}

		
		//memcpy(m_data_last.data, p_data_new, n_ROWS*n_COLS * sizeof(float));
		//imshow("m_flip_cnt", m_flip_cnt);
		//imshow("Filtered Data", m_tmp1);

		//imshow("Confidence_Factor", m_tmp1);
		//imshow("m_tmp2", m_tmp2);
		//imshow("m_tmp1", m_tmp1);
		//waitKey(1);
		//PublicTool::int32_im_show(m_flip_cost, m_flip_cost, 255, "error_confidence", 1, 0);
		//PublicTool::float_im_show(m_intensity_mean, m_intensity_mean, 255, "p_intensity_mean", 1, 0);
		//PublicTool::float_im_show(m_post_neg_rate, m_post_neg_rate, 255, "m_post_neg_rate", 1, 0);
		//PublicTool::float_im_show(m_depth_mean, m_depth_mean, 255, "m_depth_mean", 1, 0);
		
	}
	

	return true;
}

//if (abs((float)m_ref1.data[i] - (float)m_ref2.data[i]) < ) // color also changed.
bool Temporal_Smoothing_Filter::reset_parameters(){ // +++Red@20201018

	par.f_switch = true;
	par.f_do_data_recovery = true;
	par.temporal_smoothing_coherent_threshold_factor = 0.50;
	par.temporal_smoothing_forgetting_factor = 0.01;
	par.threshold_new_data_intensity = 5;
	par.threshold_confidence = 0.20;
	par.confidence_update_rate = 0.40;

	return 0;
}

bool Temporal_Smoothing_Filter::filtering( // +++Red@20201018

	cv::Mat m_data_new,
	cv::Mat m_data,
	const  float temporal_smoothing_coherent_threshold_factor,
	const  float temporal_smoothing_forgetting_factor
	) {

	if (par.f_switch) {
		const int n_ROWS = m_data.rows;
		const int n_COLS = m_data.cols;
		const float one_minus_alfa = 1.0f - temporal_smoothing_forgetting_factor;
		const float* p_data_new = ((float*)m_data_new.data);
		float* p_data = ((float*)m_data.data);
		float data_new = 0;
		float data = 0;
		for (int i = 0; i < n_ROWS*n_COLS; i++) {

			data_new = p_data_new[i];
			if (isnan(data_new)){
				p_data[i] = NAN;
				continue;
			}
			else 
			{
				
				data = p_data[i];

				// 1. last data is nan, and new non-nan data
				// 2. where depth has flash effect.
				if (isnan(data)) {  
					p_data[i] = data_new; // new data, huge motion
					continue;
				}
				else { // both new and last data are not nan, pure signal and data.
					if (abs(data_new - data) < temporal_smoothing_coherent_threshold_factor) { // smoothing
						p_data[i] = data * one_minus_alfa + data_new * temporal_smoothing_forgetting_factor;
					}
					else { 
						p_data[i] = data_new; // new data, 
					}
				}
			}
		}

	}

	return true;
}
