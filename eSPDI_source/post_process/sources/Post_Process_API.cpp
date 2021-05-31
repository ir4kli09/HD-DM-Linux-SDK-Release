#include "Post_Process_API.h"
#include "Public_Tool.h"
//#define INACTIVE_PIXEL NAN //-999999.0f// RedKuo@20210309, eys3d definition.

int POST_PROCESS_API::ResetAllParameters() {

	p_post->p_hole_filling_filter->reset_parameters();
	p_post->p_spatial_smoothing_filter->reset_parameters();
	p_post->p_temp_smooting_filter->reset_parameters();
	p_post->p_median_filter->reset_parameters();

	return 0;
}


int POST_PROCESS_API::SetPresetMode(POST_PROCESS_API_PRESET_MODES preset_mode) {


	switch (preset_mode) {

	case POST_PROCESS_API_PRESET_MODE_DEFAULT:
		ResetAllParameters();
		break;
	case POST_PROCESS_API_PRESET_MODE_BASIC:
		ResetAllParameters();
		p_post->p_hole_filling_filter->par.f_switch = false;

		p_post->p_spatial_smoothing_filter->par.f_switch = true;
		p_post->p_spatial_smoothing_filter->par.spatial_filtering_kernel_size = 3;

		p_post->p_temp_smooting_filter->par.f_switch = true;
		p_post->p_temp_smooting_filter->par.f_do_data_recovery = false;

		p_post->p_median_filter->par.f_switch = true;
		p_post->p_median_filter->par.median_filtering_kernel_size = 3;
		p_post->p_median_filter->par.n_interations = 2;

		break;
	case POST_PROCESS_API_PRESET_MODE_HIGH_ACCURACY:
		ResetAllParameters();
		p_post->p_hole_filling_filter->par.f_switch = false;

		p_post->p_spatial_smoothing_filter->par.f_switch = true;
		p_post->p_spatial_smoothing_filter->par.spatial_filtering_kernel_size = 5;

		p_post->p_temp_smooting_filter->par.f_switch = true;
		p_post->p_temp_smooting_filter->par.f_do_data_recovery = true;
		p_post->p_temp_smooting_filter->par.threshold_confidence = 0.35;
		p_post->p_temp_smooting_filter->par.confidence_update_rate = 0.4;

		p_post->p_median_filter->par.f_switch = true;
		p_post->p_median_filter->par.median_filtering_kernel_size = 3;
		p_post->p_median_filter->par.n_interations = 7;

		break;
	case POST_PROCESS_API_PRESET_MODE_HAND:
		ResetAllParameters();
		p_post->p_hole_filling_filter->par.f_switch = false;

		p_post->p_spatial_smoothing_filter->par.f_switch = true;
		p_post->p_spatial_smoothing_filter->par.spatial_filtering_kernel_size = 3;

		p_post->p_temp_smooting_filter->par.f_switch = true;
		p_post->p_temp_smooting_filter->par.f_do_data_recovery = false;

		p_post->p_median_filter->par.f_switch = true;
		p_post->p_median_filter->par.median_filtering_kernel_size = 3;
		p_post->p_median_filter->par.n_interations = 2;
		break;
	case POST_PROCESS_API_PRESET_MODE_OBSTACLE_AVOIDANCE:
		ResetAllParameters();
		p_post->p_hole_filling_filter->par.f_switch = false;

		p_post->p_spatial_smoothing_filter->par.f_switch = true;
		p_post->p_spatial_smoothing_filter->par.spatial_filtering_kernel_size = 15;

		p_post->p_temp_smooting_filter->par.f_switch = true;
		p_post->p_temp_smooting_filter->par.f_do_data_recovery = true;
		p_post->p_temp_smooting_filter->par.threshold_new_data_intensity = 5;
		p_post->p_temp_smooting_filter->par.threshold_confidence = 0.35;
		p_post->p_temp_smooting_filter->par.confidence_update_rate = 0.4;

		p_post->p_median_filter->par.f_switch = true;
		p_post->p_median_filter->par.median_filtering_kernel_size = 3;
		p_post->p_median_filter->par.n_interations = 9;
		break;
	default:
		ResetAllParameters();
		break;
	}

	return  0;

}



int POST_PROCESS_API::PostProcessing() {
	// 0. 

	float data;
	const float inv_data_unit = 1.0f / data_unit;
	const float SCALING_RATIO = focus_x * baseline;
	//cv::Mat m_src(n_src_rows, n_src_cols, CV_16UC1, p_src_data);
	//cv::Mat m_scaled(n_rows, n_cols, CV_16UC1);
	//cv::resize(m_src, m_scaled, cv::Size(n_cols, n_rows), 0, 0, cv::INTER_NEAREST);
	//p_post->p_median_filter->filtering(
	//	m_scaled, // inplace
	//	m_scaled,
	//	3,
	//	false);
	//if (depth_data_type == POST_PROCESS_API_DATA_TYPE_DEPTH_Z) {
	//	for (int i = 0; i < n_rows * n_cols; i++) {
	//		data = (float)((unsigned short*)m_scaled.data)[i] ;
	//		if (data > 0)
	//			((float*)p_m_data[0].data)[i] = SCALING_RATIO / (data * data_unit);
	//		else
	//			((float*)p_m_data[0].data)[i] = INACTIVE_PIXEL;
	//	}
	//}
	//else {
	//	for (int i = 0; i < n_rows * n_cols; i++) {
	//		data = (float)((unsigned short*)m_scaled.data)[i] ;
	//		if (data > 0)
	//				((float*)p_m_data[0].data)[i] = data * data_unit;
	//		else
	//			((float*)p_m_data[0].data)[i] = INACTIVE_PIXEL;
	//	}
	//}


	int i_ds = 0;
	for (int iy = 0; iy < n_src_rows; iy += ds_rate) {
		int iY = iy * n_src_cols;
		for (int ix = 0; ix < n_src_cols; ix += ds_rate) {

			data = (float)((unsigned short*)p_src_data)[iY + ix] * data_unit;
			if (data > 0) {
				if (depth_data_type == POST_PROCESS_API_DATA_TYPE_DEPTH_Z)
					((float*)p_m_data[0].data)[i_ds] = SCALING_RATIO / data;
				else
					((float*)p_m_data[0].data)[i_ds] = data;
			}
			else {
				((float*)p_m_data[0].data)[i_ds] = INACTIVE_PIXEL;
			}

			i_ds++;
		}
	}


	p_m_data[1].data = (unsigned char*)pp_data[1]; // next
	p_m_data[2].data = (unsigned char*)pp_data[2]; // next
	p_m_data[3].data = (unsigned char*)pp_data[3]; // next

	if (p_post->p_hole_filling_filter->par.f_switch) {

		// denoise before hole filling
		if (p_post->p_median_filter->par.f_switch) {
			p_post->p_median_filter->filtering(
				p_m_data[0], // inplace
				p_m_data[0],
				p_post->p_median_filter->par.median_filtering_kernel_size,
				false);
		}

		p_post->p_hole_filling_filter->filtering(
			p_m_data[0], // in
			p_m_data[0], //out
			p_post->p_hole_filling_filter->par.spatial_filtering_kernel_size,
			p_post->p_hole_filling_filter->par.outlier_threshold,
			m_weight_sum
		);
	}

	// 2. smoothing / anti-aliasing
	if (p_post->p_spatial_smoothing_filter->par.f_switch) {
		p_post->p_spatial_smoothing_filter->filtering(
			p_m_data[0], // in
			p_m_data[1], //out
			p_post->p_spatial_smoothing_filter->par.spatial_filtering_kernel_size,
			true,
			m_weight_sum // in
		);

		if (p_post->p_outlier_filter->par.f_switch) {
			p_post->p_outlier_filter->filtering(
				p_m_data[0], // before
				p_m_data[1], //after
				p_post->p_outlier_filter->par.threshold
			);
		}
	}
	else {
		p_m_data[1].data = p_m_data[0].data; // bypass
	}




	// 3. 
	if (p_post->p_temp_smooting_filter->par.f_switch) {
		if (p_post->p_temp_smooting_filter->par.f_do_data_recovery) {
			if (p_intensity == NULL) {
				p_post->p_temp_smooting_filter->par.f_do_data_recovery = false;
			}
			else {
				m_intensity.data = (unsigned char*)p_intensity;
				cv::resize(m_intensity, m_intensity_ds, cv::Size(n_cols, n_rows), 0, 0, cv::INTER_LINEAR);
			}
		}
			

		p_post->p_temp_smooting_filter->filtering(
			p_m_data[1], // in
			p_m_data[2], // out
			p_post->p_temp_smooting_filter->par.temporal_smoothing_coherent_threshold_factor,
			p_post->p_temp_smooting_filter->par.temporal_smoothing_forgetting_factor,
			m_intensity_ds,
			m_intensity_ds_last
		);

		if (p_post->p_temp_smooting_filter->par.f_do_data_recovery)
			memcpy(m_intensity_ds_last.data, m_intensity_ds.data, n_cols*n_rows);
	}
	else {
		p_m_data[2].data = p_m_data[1].data; // bypass
	}


	//cv::imshow("Diff", Diff);
	//PublicTool::float_im_show(Diff_d, Diff_d, 255, "Diff_d", 1, 0);



	// Z
	if (p_post->p_median_filter->par.f_switch) {

		// 1. D to Z
		//cv::Mat m_tmp = p_m_data[2];
		for (int i = 0; i < n_cols*n_rows; i++) {

			((float*)p_m_data[3].data)[i] = SCALING_RATIO / ((float*)p_m_data[2].data)[i];// * inv_data_unit;
		}

		for (int n = 0; n < p_post->p_median_filter->par.n_interations; n++) {
			p_post->p_median_filter->filtering(
				p_m_data[3], // inplace
				p_m_data[3],
				p_post->p_median_filter->par.median_filtering_kernel_size,
				false);
		}
	}
	else {
		p_m_data[3].data = p_m_data[2].data; // bypass
	}

	// 4. output stage.
	cv::resize(p_m_data[3], m_data_output, cv::Size(n_src_cols, n_src_rows), 0, 0, cv::INTER_NEAREST);

	if (p_post->p_median_filter->par.f_switch) {
		if (depth_data_type == POST_PROCESS_API_DATA_TYPE_DEPTH_Z) {

			// output Z
			for (int i = 0; i < n_src_cols*n_src_rows; i++) {
				((unsigned short*)p_dst_data)[i] = (unsigned short)(((float*)m_data_output.data)[i] * inv_data_unit + 0.5);
			}
		}
		else {
			// Z to D
			for (int i = 0; i < n_src_cols*n_src_rows; i++) {

				//((unsigned short*)p_dst_data)[i] = ((float*)m_data_output.data)[i];
				float data = ((float*)m_data_output.data)[i];
				if (data > 0)
					((unsigned short*)p_dst_data)[i] = (unsigned short)(SCALING_RATIO / data * inv_data_unit + 0.5);
				else
					((unsigned short*)p_dst_data)[i] = 0;
			}
		}
	}
	else {
		for (int i = 0; i < n_src_cols*n_src_rows; i++) {
			data = ((float*)m_data_output.data)[i];
			if (data > 0)
				if (depth_data_type == POST_PROCESS_API_DATA_TYPE_DEPTH_Z)
					((unsigned short*)p_dst_data)[i] = (unsigned short)(SCALING_RATIO / data * inv_data_unit + 0.5);
				else
					((unsigned short*)p_dst_data)[i] = (unsigned short)(data * inv_data_unit + 0.5);
			else
				((unsigned short*)p_dst_data)[i] = 0;
		}
	}




	//cv::Mat m_IN(n_src_rows, n_src_cols,CV_16UC1, p_src_data);
	//cv::Mat m_OUT(n_src_rows, n_src_cols, CV_16UC1, p_dst_data);
	//PublicTool::float_im_show(m_IN, m_IN, 4000, "Before", 1, 0);
	//PublicTool::float_im_show(m_OUT, m_OUT, 4000, "After_Filtering", 1, 0);
	//PublicTool::float_im_show(p_m_data[0], p_m_data[0], 0, 255, "Before", 1, 0);
	//PublicTool::float_im_show(p_m_data[2], p_m_data[2], 0, 255,"After_Filtering", 1, 0);


	//cout << "in : " << p_dst_data << endl;
	//cv::Mat m_tmp(n_src_rows, n_src_cols,CV_16UC1,p_dst_data);
	//PublicTool::int16_im_show(m_tmp, m_tmp, 255*8, "p_tmp", 1, 0);


	return 0;
}


int POST_PROCESS_API::DataTransformDecimation(
	cv::Mat m_Src,
	cv::Mat m_Dst,
	int ds_rate,
	float src2dst_scaling_rate,
	float invalid_value
) {



	return 0;
}