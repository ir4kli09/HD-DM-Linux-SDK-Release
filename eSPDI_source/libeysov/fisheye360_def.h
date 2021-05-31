/**
  * @file  fisheye360_def.h
  * @title Definition of fisheye360
  * @version 2.1.2
  */
#ifndef __EYS_FISHEYE360_DEF_H__
#define __EYS_FISHEYE360_DEF_H__
#define BYTE unsigned char

/**
  * @defgroup fisheye360 FishEye360
  * Fisheye360 Project
  */

namespace eys
{
namespace fisheye360
{

/**
  * @addtogroup fisheye360
  * @{
  */

/**
  * Parameters for LUT table for eYsGlobeK
  */
typedef struct ParaLUT
{
	long long file_ID_header;       //!< [00]-[000] File ID header : 2230
    long long file_ID_version;      //!< [01]-[008] File ID version : 5
	double    FOV;                  //!< [02]-[016] Field of view with degree
    long long semi_FOV_pixels;      //!< [03]-[024] Pixels for semi-FOV
    long long img_src_cols;         //!< [04]-[032] Width for source image (single image)
    long long img_src_rows;         //!< [05]-[040] Height for source image
	double    img_L_src_col_center; //!< [06]-[048] Center of width for L side source image
    double    img_L_src_row_center; //!< [07]-[056] Center of height for L side source image
	double    img_R_src_col_center; //!< [08]-[064] Center of width for R side source image
	double    img_R_src_row_center; //!< [09]-[072] Center of height for R side source image
	double    img_L_rotation;       //!< [10]-[080] Rotation for L side image
	double    img_R_rotation;       //!< [11]-[088] Rotation for R side image
	double    spline_control_v1;    //!< [12]-[096] Spline control value for row = DIV x 0 pixel, DIV = rows/6
	double    spline_control_v2;    //!< [13]-[104] Spline control value for row = DIV x 1 pixel, DIV = rows/6
	double    spline_control_v3;    //!< [14]-[112] Spline control value for row = DIV x 2 pixel, DIV = rows/6
	double    spline_control_v4;    //!< [15]-[120] Spline control value for row = DIV x 3 pixel, DIV = rows/6
	double    spline_control_v5;    //!< [16]-[128] Spline control value for row = DIV x 4 pixel, DIV = rows/6
	double    spline_control_v6;    //!< [17]-[136] Spline control value for row = DIV x 5 pixel, DIV = rows/6
	double    spline_control_v7;    //!< [18]-[144] Spline control value for row = DIV x 6 pixel, DIV = rows/6
	long long img_dst_cols;         //!< [19]-[152] Width for output image (single image), according to "Original" parameters  
	long long img_dst_rows;         //!< [20]-[160] Height for output image, according to "Original" parameters
	long long img_L_dst_shift;      //!< [21]-[168] Output L side image shift in row 
	long long img_R_dst_shift;      //!< [22]-[176] Output R side image shift in row  
	long long img_overlay_LR;       //!< [23]-[184] Overlay between L/R in pixels, far field, (YUV must be even) 
	long long img_overlay_RL;       //!< [24]-[192] Overlay between R/L in pixels, far field, (YUV must be even) 
	long long img_stream_cols;      //!< [25]-[200] Output image stream of cols 
	long long img_stream_rows;      //!< [26]-[208] Output image stream of rows 
	long long video_stream_cols;    //!< [27]-[216] Output video stream of cols, not used 
	long long video_stream_rows;    //!< [28]-[224] Output video stream of rows, not used 
	long long usb_type;             //!< [29]-[232] 2 for usb2, 3 for usb3, not used, 5 tmp for old far-field overlay value calculation
	long long img_type;             //!< [30]-[240] 1 for yuv422, 2 for BGR, 3 for RGB
	long long lut_type;             //!< [31]-[248] Output LUT tye @ref eys::LutModes
	long long blending_type;        //!< [32]-[256] 0 for choosed by function, 1 for alpha-blending, 2 for Laplacian pyramid blending, not used
	double    overlay_ratio;        //!< [33]-[264] far field overlay value is equal to img_overlay_LR(RL)  = overlay_value + overlay_ratio 
		
	long long serial_number_date0;  //!< [34]-[272] 8 bytes, yyyy-mm-dd
	long long serial_number_date1;  //!< [35]-[280] 8 bytes, hh-mm-ss-xxx, xxx for machine number

	double    unit_sphere_radius;   //!< [36]-[288] Original : Unit spherical radius for dewarping get x and y
	double    min_col;              //!< [37]-[296] Original : Parameters of min position of image width
	double    max_col;              //!< [38]-[304] Original : Parameters of max position of image width
	double    min_row;              //!< [39]-[312] Original : Parameters of min position of image height
	double    max_row;              //!< [40]-[320] Original : Parameters of max position of image height

	long long AGD_LR;               //!< [41]-[328] Err : Average gray-level value discrepancy at LR boundary
    long long AGD_RL;               //!< [42]-[336] Err : Average gray-level value discrepancy at RL boundary

	long long out_img_resolution;   //!< [43]-[344] Set output resolution @ref eys::ImgResolutionModes 
	long long out_lut_cols;         //!< [44]-[352] Output side-by-side lut width, according to the set of out_img_resolution
	long long out_lut_rows;         //!< [45]-[360] Output lut height, according to the set of out_img_resolution
	long long out_lut_cols_eff;     //!< [46]-[368] Output effective pixels in out_lut_cols, 0 is for all
	long long out_lut_rows_eff;     //!< [47]-[376] Output effecitve pixels in out_lut_rows, 0 is for all
	long long out_img_cols;         //!< [48]-[384] Output side-by-side image width after dewarping and stitching, according to the set of out_img_resolution
    long long out_img_rows;         //!< [49]-[392] Output image height, according to the set of out_img_resolution
	long long out_overlay_LR;       //!< [50]-[340] Output L/R overlay value, according to the set of out_img_resolution
	long long out_overlay_RL;       //!< [51]-[408] Output R/L overlay value, according to the set of out_img_resolution

	double    distortion_fix_v0;    //!< [52]-[416] Parameter for lens distortion fix, y = v6*x^6 + v5*x^5 + v4*x^4 + v3*x^3 + v2*x^2 + v1*x + v0 
	double    distortion_fix_v1;    //!< [53]-[424] Parameter for lens distortion fix
	double    distortion_fix_v2;    //!< [54]-[432] Parameter for lens distortion fix
	double    distortion_fix_v3;    //!< [55]-[440] Parameter for lens distortion fix
	double    distortion_fix_v4;    //!< [56]-[448] Parameter for lens distortion fix
	double    distortion_fix_v5;    //!< [57]-[456] Parameter for lens distortion fix
	double    distortion_fix_v6;    //!< [58]-[464] Parameter for lens distortion fix
	double    sensor_pixel_size;    //!< [59]-[472] Sensor pixel size in um
	long long lens_projection_type; //!< [60]-[480] Lens projection type, @ref eys::LensProjectionModes

	long long reserve[35];          //!< [61]-[488] Reserve 44 parameter to use
	BYTE      serial_number[256];
}sParaLUT;

/**
  * Parameters for chessboard for eYsGlobeK
  */
typedef struct ParaChessboard
{
	int    lens_type;             //!< For small and wide angel camera lens, @ref eys::LensModes
	int    distortion_type;       //!< Distortion parameters for lens, @ref eys::LensDistortionPara
	bool   draw_corner;           //!< "true" for draw corner, "false" is disable draw corner
	int    block_num_rows_center; //!< Chessboard block "x" number in row for center 
	int    block_num_cols_center; //!< Chessboard block "x" number in column for center
	double square_size_center;    //!< Square size in mm for center
}sParaChessboard;

/**
  * Parameters for image mask for eYsGlobeK
  */
typedef struct ParaMask
{
	int    scan_FOV_pixels;      //!< Scan range of sem_FOV_pixels, ex. (sem_FOV_pixels + scan_FOV_pixels) ~ (sem_FOV_pixels - scan_FOV_pixels)
	int    scan_FOV_num ;        //!< Number of scan_FOV_pixels
	double scan_rotation_ang ;   //!< Scan angle of img_L[R]_rotation, ex. (0 + scan_rotation_ang) ~ (0 - scan_rotation_ang)
	int    scan_rotation_num;    //!< Numbers in scan_rotaton_ang
	int    compress_rate ;       //!< Jpeg compress rate
	bool   on_note ;             // Turn-on or turn-off console mode std::cout
	bool   on_log;               // Turn-on or turn-off imgLog.txt
	bool   on_pic;               // Turn-on or turn-off save pictures  

	int    chessboard_col1;      //!< Chessboard left-top col position
	int    chessboard_row1;      //!< Chessboard left-top row position
	int    chessboard_col2;      //!< Chessboard right-down col position
	int    chessboard_row2;      //!< Chessboard right-down row position
	int    chessboard_color_L;   //!< Chessboard L-side mask color, support R and B @ref eys::ImgColorModes
	int    chessboard_color_R;   //!< Chessboard R-side mask color, support R and B @ref eys::ImgColorModes
	bool   chessboard_work_L;    //!< "true" for chessboard L is work, "false" for chessbord L is not work
	bool   chessboard_work_R;    //!< "true" for chessboard R is work, "false" for chessbord R is not work

	int    alignment_col1;       //!< Alignment for rotation and shift, which left-top col position
	int    alignment_row1;       //!< Alignment for rotation and shift, which left-top row position
	int    alignment_col2;       //!< Alignment for rotation and shift, which right-down col position
	int    alignment_row2;       //!< Alignment for rotation and shift, which right-down row position
	int    alignment_err;        //!< Alignment error for line rotation in pixel of left side image
	double alignment_ratio;      //!< Alignment bar should be in rows x ratio / 100
	int    alignment_color_L1;   //!< Alignment color for L side image and left edge
	int    alignment_color_L2;   //!< Alignment color for L side image and right edge
	int    alignment_color_R1;   //!< Alignment color for R side image and left edge
	int    alignment_color_R2;   //!< Alignment color for R side image and right edge
	bool   alignment_work_L1;    //!< "true" for work and "false" for not work
	bool   alignment_work_L2;    //!< "true" for work and "false" for not work
	bool   alignment_work_R1;    //!< "true" for work and "false" for not work
	bool   alignment_work_R2;    //!< "true" for work and "false" for not work

	int    alignment2_col1;     //!< Alignment2 for rotation and shift, which left-top col position, eYsGlobeS
	int    alignment2_row1;     //!< Alignment2 for rotation and shift, which left-top row position, eYsGlobeS
	int    alignment2_col2;     //!< Alignment2 for rotation and shift, which right-down col position, eYsGlobeS
	int    alignment2_row2;     //!< Alignment2 for rotation and shift, which right-down row position, eYsGlobeS

	int    overlay_col1;         //!< Overlay mask and which left-top col position
	int    overlay_row1;         //!< Overlay mask and which left-top row position
	int    overlay_col2;         //!< Overlay mask and which right-down col position
	int    overlay_row2;         //!< Overlay mask and which right-down row position
	int    overlay_color_L1;     //!< Overlay color for L side image and left edge
	int    overlay_color_L2;     //!< Overlay color for L side image and right edge
	int    overlay_color_R1;     //!< Overlay color for R side image and left edge
	int    overlay_color_R2;     //!< Overlay color for R side image and right edge
	bool   overlay_work_L1;      //!< "true" for work and "false" for not work
	bool   overlay_work_L2;      //!< "true" for work and "false" for not work
	bool   overlay_work_R1;      //!< "true" for work and "false" for not work
	bool   overlay_work_R2;      //!< "true" for work and "false" for not work
	int    overlay_err_LR;       //!< Overlay error of L/R boundary
	int    overlay_err_RL;       //!< Overlay error of R/L boundary

	int    overlay_top_col1;     //!< Overlay mask for top and which left-top col position
	int    overlay_top_row1;     //!< Overlay mask for top and which left-top row position
	int    overlay_top_col2;     //!< Overlay mask for top and which right-down col position
	int    overlay_top_row2;     //!< Overlay mask for top and which right-down row position
	int    overlay_top_color_L1; //!< Overlay color for L side image and left edge
	int    overlay_top_color_L2; //!< Overlay color for L side image and right edge
	int    overlay_top_color_R1; //!< Overlay color for R side image and left edge
	int    overlay_top_color_R2; //!< Overlay color for R side image and right edge
	bool   overlay_top_work_L1;  //!< "true" for work and "false" for not work
	bool   overlay_top_work_L2;  //!< "true" for work and "false" for not work
	bool   overlay_top_work_R1;  //!< "true" for work and "false" for not work
	bool   overlay_top_work_R2;  //!< "true" for work and "false" for not work
	int    overlay_top_err_LR;   //!< Overlay error of L/R boundary
	int    overlay_top_err_RL;   //!< Overlay error of R/L boundary
}sParaMask;

/**
  * Parameters for quality check mask for eYsGlobeQ
  */
typedef struct ParaQuality
{
	int   file_ID_header;      //!< [000]-[000] File ID header : 2230
	int   file_ID_version;     //!< [001]-[004] File ID version : 2

	// para - chart
	int   circle0_cx0;         //!< [002]-[008] X0 position of circle of L chart
	int   circle0_cy0;         //!< [003]-[012] Y0 position of circle of L chart
	int   circle0_cx1;         //!< [004]-[016] X1 position of circle of L chart
	int   circle0_cy1;         //!< [005]-[020] Y1 position of circle of L chart
	int   circle0_cx2;         //!< [006]-[024] X2 position of circle of L chart
	int   circle0_cy2;         //!< [007]-[028] Y2 position of circle of L chart
	int   circle0_cx3;         //!< [008]-[032] X3 position of circle of L chart
	int   circle0_cy3;         //!< [009]-[036] Y3 position of circle of L chart

	int   circle1_cx0;         //!< [010]-[040] X0 position of circle of R chart
	int   circle1_cy0;         //!< [011]-[044] Y0 position of circle of R chart
	int   circle1_cx1;         //!< [012]-[048] X1 position of circle of R chart
	int   circle1_cy1;         //!< [013]-[052] Y1 position of circle of R chart
	int   circle1_cx2;         //!< [014]-[056] X2 position of circle of R chart
	int   circle1_cy2;         //!< [015]-[060] Y2 position of circle of R chart
	int   circle1_cx3;         //!< [016]-[064] X3 position of circle of R chart
	int   circle1_cy3;         //!< [017]-[068] Y3 position of circle of R chart

	int   chart0_cx;           //!< [018]-[072] X position of center of L chart
	int   chart0_cy;           //!< [019]-[076] Y position of center of L chart
	int   chart0_lx;           //!< [020]-[080] Width of L chart
	int   chart0_ly;           //!< [021]-[084] Height of L chart

	int   chart1_cx;           //!< [022]-[088] X position of center of R chart
	int   chart1_cy;           //!< [023]-[092] Y position of center of R chart
	int   chart1_lx;           //!< [024]-[096] Width of R chart
	int   chart1_ly;           //!< [025]-[100] Height of R chart

	// para - white
	int   white_RGB_R0;        //!< [026]-[104] White chart(L) average r value in RGB color domain
	int   white_RGB_G0;        //!< [027]-[108] White chart(L) average g value in RGB color domain
	int   white_RGB_B0;        //!< [028]-[112] White chart(L) average b value in RGB color domain
	int   white_Lab_L0;        //!< [029]-[116] White chart(L) average L value in CIELAB color domain, L = L*255/100 
	int   white_Lab_a0;        //!< [030]-[120] White chart(L) average a value in CIELAB color domain, a = a+128 
	int   white_Lab_b0;        //!< [031]-[124] White chart(L) average b value in CIELAB color domain, b = b+128
	float white_Lab_dE0;       //!< [032]-[128] White chart(L) deta E of CIELAB color domain
	int   white_gray0;         //!< [033]-[132] White chart(L) average gray value

	int   white_RGB_R1;        //!< [034]-[136] White chart(R) average r value in RGB color domain
	int   white_RGB_G1;        //!< [035]-[140] White chart(R) average g value in RGB color domain
	int   white_RGB_B1;        //!< [036]-[144] White chart(R) average b value in RGB color domain
	int   white_Lab_L1;        //!< [037]-[148] White chart(R) average L value in CIELAB color domain, L = L*255/100 
	int   white_Lab_a1;        //!< [038]-[152] White chart(R) average a value in CIELAB color domain, a = a+128 
	int   white_Lab_b1;        //!< [039]-[156] White chart(R) average b value in CIELAB color domain, b = b+128
	float white_Lab_dE1;       //!< [040]-[160] White chart(R) deta E of CIELAB color domain
	int   white_gray1;         //!< [041]-[164] White chart(R) average gray value

	// para - black
	int   black_RGB_R0;        //!< [042]-[168] Black chart(L) average r value in RGB color domain
	int   black_RGB_G0;        //!< [043]-[172] Black chart(L) average g value in RGB color domain
	int   black_RGB_B0;        //!< [044]-[176] Black chart(L) average b value in RGB color domain
	int   black_Lab_L0;        //!< [045]-[180] Black chart(L) average L value in CIELAB color domain, L = L*255/100  
	int   black_Lab_a0;        //!< [046]-[184] Black chart(L) average a value in CIELAB color domain, a = a+128  
	int   black_Lab_b0;        //!< [047]-[188] Black chart(L) average b value in CIELAB color domain, b = b+128 
	float black_Lab_dE0;       //!< [048]-[192] Black chart(L) deta E of CIELAB color domain
	int   black_gray0;         //!< [049]-[196] Black chart(L) average gray value

	int   black_RGB_R1;        //!< [050]-[200] Black chart(R) average r value in RGB color domain
	int   black_RGB_G1;        //!< [051]-[204] Black chart(R) average g value in RGB color domain
	int   black_RGB_B1;        //!< [052]-[208] Black chart(R) average b value in RGB color domain
	int   black_Lab_L1;        //!< [053]-[212] Black chart(R) average L value in CIELAB color domain, L = L*255/100  
	int   black_Lab_a1;        //!< [054]-[216] Black chart(R) average a value in CIELAB color domain, a = a+128  
	int   black_Lab_b1;        //!< [055]-[220] Black chart(R) average b value in CIELAB color domain, b = b+128 
	float black_Lab_dE1;       //!< [056]-[224] Black chart(R) deta E of CIELAB color domain
	int   black_gray1;         //!< [057]-[228] Black chart(R) average gray value

	// para - gray
	int   gray_RGB_R0;         //!< [058]-[232] Gray chart(L) average r value in RGB color domain
	int   gray_RGB_G0;         //!< [059]-[236] Gray chart(L) average g value in RGB color domain
	int   gray_RGB_B0;         //!< [060]-[240] Gray chart(L) average b value in RGB color domain
	int   gray_Lab_L0;         //!< [061]-[244] Gray chart(L) average L value in CIELAB color domain, L = L*255/100  
	int   gray_Lab_a0;         //!< [062]-[248] Gray chart(L) average a value in CIELAB color domain, a = a+128  
	int   gray_Lab_b0;         //!< [063]-[252] Gray chart(L) average b value in CIELAB color domain, b = b+128 
	float gray_Lab_dE0;        //!< [064]-[256] Gray chart(L) deta E of CIELAB color domain
	int   gray_gray0;          //!< [065]-[260] Gray chart(L) average gray value

	int   gray_RGB_R1;         //!< [066]-[264] Gray chart(R) average r value in RGB color domain
	int   gray_RGB_G1;         //!< [067]-[268] Gray chart(R) average g value in RGB color domain
	int   gray_RGB_B1;         //!< [068]-[272] Gray chart(R) average b value in RGB color domain
	int   gray_Lab_L1;         //!< [069]-[276] Gray chart(R) average L value in CIELAB color domain, L = L*255/100  
	int   gray_Lab_a1;         //!< [070]-[280] Gray chart(R) average a value in CIELAB color domain, a = a+128  
	int   gray_Lab_b1;         //!< [071]-[284] Gray chart(R) average b value in CIELAB color domain, b = b+128 
	float gray_Lab_dE1;        //!< [072]-[288] Gray chart(R) deta E of CIELAB color domain
	int   gray_gray1;          //!< [073]-[292] Gray chart(R) average gray value

	// para - red
	int   red_RGB_R0;          //!< [074]-[296] Red chart(L) average r value in RGB color domain
	int   red_RGB_G0;          //!< [075]-[300] Red chart(L) average g value in RGB color domain
	int   red_RGB_B0;          //!< [076]-[304] Red chart(L) average b value in RGB color domain
	int   red_Lab_L0;          //!< [077]-[308] Red chart(L) average L value in CIELAB color domain, L = L*255/100  
	int   red_Lab_a0;          //!< [078]-[312] Red chart(L) average a value in CIELAB color domain, a = a+128  
	int   red_Lab_b0;          //!< [079]-[316] Red chart(L) average b value in CIELAB color domain, b = b+128
	float red_Lab_dE0;         //!< [080]-[320] Red chart(L) deta E of CIELAB color domain

	int   red_RGB_R1;          //!< [081]-[324] Red chart(R) average r value in RGB color domain
	int   red_RGB_G1;          //!< [082]-[328] Red chart(R) average g value in RGB color domain
	int   red_RGB_B1;          //!< [083]-[332] Red chart(R) average b value in RGB color domain
	int   red_Lab_L1;          //!< [084]-[336] Red chart(R) average L value in CIELAB color domain, L = L*255/100  
	int   red_Lab_a1;          //!< [085]-[340] Red chart(R) average a value in CIELAB color domain, a = a+128  
	int   red_Lab_b1;          //!< [086]-[344] Red chart(R) average b value in CIELAB color domain, b = b+128
	float red_Lab_dE1;         //!< [087]-[348] Red chart(R) deta E of CIELAB color domain

	// para - green
	int   green_RGB_R0;        //!< [088]-[352] Green chart(L) average r value in RGB color domain
	int   green_RGB_G0;        //!< [089]-[356] Green chart(L) average g value in RGB color domain
	int   green_RGB_B0;        //!< [090]-[360] Green chart(L) average b value in RGB color domain
	int   green_Lab_L0;        //!< [091]-[364] Green chart(L) average L value in CIELAB color domain, L = L*255/100  
	int   green_Lab_a0;        //!< [092]-[368] Green chart(L) average a value in CIELAB color domain, a = a+128  
	int   green_Lab_b0;        //!< [093]-[372] Green chart(L) average b value in CIELAB color domain, b = b+128 
	float green_Lab_dE0;       //!< [094]-[376] Green chart(L) deta E of CIELAB color domain

	int   green_RGB_R1;        //!< [095]-[380] Green chart(R) average r value in RGB color domain
	int   green_RGB_G1;        //!< [096]-[384] Green chart(R) average g value in RGB color domain
	int   green_RGB_B1;        //!< [097]-[388] Green chart(R) average b value in RGB color domain
	int   green_Lab_L1;        //!< [098]-[392] Green chart(R) average L value in CIELAB color domain, L = L*255/100  
	int   green_Lab_a1;        //!< [099]-[396] Green chart(R) average a value in CIELAB color domain, a = a+128  
	int   green_Lab_b1;        //!< [100]-[400] Green chart(R) average b value in CIELAB color domain, b = b+128 
	float green_Lab_dE1;       //!< [101]-[404] Green chart(R) deta E of CIELAB color domain

	// para - blue
	int   blue_RGB_R0;         //!< [102]-[408] Blue chart(L) average r value in RGB color domain
	int   blue_RGB_G0;         //!< [103]-[412] Blue chart(L) average g value in RGB color domain
	int   blue_RGB_B0;         //!< [104]-[416] Blue chart(L) average b value in RGB color domain
	int   blue_Lab_L0;         //!< [105]-[420] Blue chart(L) average L value in CIELAB color domain, L = L*255/100  
	int   blue_Lab_a0;         //!< [106]-[424] Blue chart(L) average a value in CIELAB color domain, a = a+128  
	int   blue_Lab_b0;         //!< [107]-[428] Blue chart(L) average b value in CIELAB color domain, b = b+128
	float blue_Lab_dE0;        //!< [108]-[432] Blue chart(L) deta E of CIELAB color domain

	int   blue_RGB_R1;         //!< [109]-[436] Blue chart(R) average r value in RGB color domain
	int   blue_RGB_G1;         //!< [110]-[440] Blue chart(R) average g value in RGB color domain
	int   blue_RGB_B1;         //!< [111]-[444] Blue chart(R) average b value in RGB color domain
	int   blue_Lab_L1;         //!< [112]-[448] Blue chart(R) average L value in CIELAB color domain, L = L*255/100  
	int   blue_Lab_a1;         //!< [113]-[452] Blue chart(R) average a value in CIELAB color domain, a = a+128  
	int   blue_Lab_b1;         //!< [114]-[456] Blue chart(R) average b value in CIELAB color domain, b = b+128
	float blue_Lab_dE1;        //!< [115]-[460] Blue chart(R) deta E of CIELAB color domain

	// para - MTF1
	int   MTF1_max_value0;     //!< [116]-[464] Max gray value(L)
	int   MTF1_min_value0;     //!< [117]-[468] Min gray value(L)
	int   MTF1_tv_line_num0;   //!< [118]-[472] TV line number(L)
	float MTF1_tv_line_val0;   //!< [119]-[476] TV line value(L)

	int   MTF1_max_value1;     //!< [120]-[480] Max gray value(R)
	int   MTF1_min_value1;     //!< [121]-[484] Min gray value(R)
	int   MTF1_tv_line_num1;   //!< [122]-[488] TV line number(R)
	float MTF1_tv_line_val1;   //!< [123]-[492] TV line value(R)

	// para - MTF2
	int   MTF2_max_value0;     //!< [124]-[496] Max gray value(L)
	int   MTF2_min_value0;     //!< [125]-[500] Min gray value(L)
	int   MTF2_tv_line_num0;   //!< [126]-[504] TV line number(L)
	float MTF2_tv_line_val0;   //!< [127]-[512] TV line value(L)

	int   MTF2_max_value1;     //!< [128]-[516] Max gray value(R)
	int   MTF2_min_value1;     //!< [129]-[520] Min gray value(R)
	int   MTF2_tv_line_num1;   //!< [130]-[524] TV line number(R)
	float MTF2_tv_line_val1;   //!< [131]-[528] TV line value(R)

	// para - MTF3
	int   MTF3_max_value0;     //!< [132]-[532] Max gray value(L)
	int   MTF3_min_value0;     //!< [133]-[536] Min gray value(L)
	int   MTF3_tv_line_num0;   //!< [134]-[540] TV line number(L)
	float MTF3_tv_line_val0;   //!< [135]-[544] TV line value(L)

	int   MTF3_max_value1;     //!< [136]-[548] Max gray value(R)
	int   MTF3_min_value1;     //!< [137]-[552] Min gray value(R)
	int   MTF3_tv_line_num1;   //!< [138]-[556] TV line number(R)
	float MTF3_tv_line_val1;   //!< [139]-[560] TV line value(R)

	// para - MTF4
	int   MTF4_max_value0;     //!< [140]-[564] Max gray value(L)
	int   MTF4_min_value0;     //!< [141]-[568] Min gray value(L)
	int   MTF4_tv_line_num0;   //!< [142]-[572] TV line number(L)
	float MTF4_tv_line_val0;   //!< [143]-[576] TV line value(L)

	int   MTF4_max_value1;     //!< [144]-[580] Max gray value(R)
	int   MTF4_min_value1;     //!< [145]-[584] Min gray value(R)
	int   MTF4_tv_line_num1;   //!< [146]-[588] TV line number(R)
	float MTF4_tv_line_val1;   //!< [147]-[592] TV line value(R)

	// para - stitching 
	float stitch_LR_shift;     //!< [148]-[596] LR overlay pixel shift
	float stitch_RL_shift;     //!< [149]-[600] RL overlay pixel shift

	float stitch_LR_L_RGB_R;   //!< [150]-[604] chart(LR_L) average r value in RGB color domain
	float stitch_LR_L_RGB_G;   //!< [151]-[608] chart(LR_L) average g value in RGB color domain
	float stitch_LR_L_RGB_B;   //!< [152]-[612] chart(LR_L) average b value in RGB color domain
	float stitch_LR_L_Lab_L;   //!< [153]-[616] chart(LR_L) average L value in CIELAB color domain, L = L*255/100 (CV) -> 100 (EYS) 
	float stitch_LR_L_Lab_a;   //!< [154]-[620] chart(LR_L) average a value in CIELAB color domain, a = a+128  
	float stitch_LR_L_Lab_b;   //!< [155]-[624] chart(LR_L) average b value in CIELAB color domain, b = b+128 
	float stitch_LR_Lab_dE0;   //!< [156]-[628] LB chart(L) deta E of CIELAB color domain

	float stitch_LR_R_RGB_R;   //!< [157]-[632] chart(LR_R) average r value in RGB color domain
	float stitch_LR_R_RGB_G;   //!< [158]-[636] chart(LR_R) average g value in RGB color domain
	float stitch_LR_R_RGB_B;   //!< [159]-[640] chart(LR_R) average b value in RGB color domain
	float stitch_LR_R_Lab_L;   //!< [160]-[644] chart(LR_R) average L value in CIELAB color domain, L = L*255/100  
	float stitch_LR_R_Lab_a;   //!< [161]-[648] chart(LR_R) average a value in CIELAB color domain, a = a+128  
	float stitch_LR_R_Lab_b;   //!< [162]-[652] chart(LR_R) average b value in CIELAB color domain, b = b+128 
	float stitch_LR_Lab_dE1;   //!< [163]-[656] LB chart(R) deta E of CIELAB color domain

	float stitch_RL_L_RGB_R;   //!< [164]-[660] chart(RL_L) average r value in RGB color domain
	float stitch_RL_L_RGB_G;   //!< [165]-[664] chart(RL_L) average g value in RGB color domain
	float stitch_RL_L_RGB_B;   //!< [166]-[668] chart(RL_L) average b value in RGB color domain
	float stitch_RL_L_Lab_L;   //!< [167]-[672] chart(RL_L) average L value in CIELAB color domain, L = L*255/100  
	float stitch_RL_L_Lab_a;   //!< [168]-[676] chart(RL_L) average a value in CIELAB color domain, a = a+128  
	float stitch_RL_L_Lab_b;   //!< [169]-[680] chart(RL_L) average b value in CIELAB color domain, b = b+128 
	float stitch_RL_Lab_dE0;   //!< [170]-[684] LB chart(L) deta E of CIELAB color domain

	float stitch_RL_R_RGB_R;   //!< [171]-[688] chart(RL_R) average r value in RGB color domain
	float stitch_RL_R_RGB_G;   //!< [172]-[692] chart(RL_R) average g value in RGB color domain
	float stitch_RL_R_RGB_B;   //!< [173]-[696] chart(RL_R) average b value in RGB color domain
	float stitch_RL_R_Lab_L;   //!< [174]-[700] chart(RL_R) average L value in CIELAB color domain, L = L*255/100  
	float stitch_RL_R_Lab_a;   //!< [175]-[704] chart(RL_R) average a value in CIELAB color domain, a = a+128  
	float stitch_RL_R_Lab_b;   //!< [176]-[708] chart(RL_R) average b value in CIELAB color domain, b = b+128 
	float stitch_RL_Lab_dE1;   //!< [177]-[712] LB chart(R) deta E of CIELAB color domain

	// criterion
	int   criterion_b01;       //!< [178]-[716] Criterion 1
	int   criterion_b02;       //!< [179]-[720] Criterion 2
	int   criterion_b03;       //!< [180]-[724] Criterion 3
	int   criterion_b04;       //!< [181]-[728] Criterion 4
	int   criterion_b05;       //!< [182]-[732] Criterion 5
	int   criterion_b06;       //!< [183]-[736] Criterion 6
	int   criterion_b07;       //!< [184]-[740] Criterion 7
	int   criterion_b08;       //!< [185]-[744] Criterion 8
	int   criterion_b09;       //!< [186]-[748] Criterion 9
	int   criterion_b10;       //!< [187]-[752] Criterion 10
	int   criterion_b11;       //!< [188]-[756] Criterion 11
	int   criterion_b12;       //!< [189]-[760] Criterion 12
	int   criterion_b13;       //!< [190]-[764] Criterion 13
	int   criterion_b14;       //!< [191]-[768] Criterion 14
	int   criterion_b15;       //!< [192]-[772] Criterion 15
	int   criterion_b16;       //!< [193]-[776] Criterion 16
	int   criterion_b17;       //!< [194]-[780] Criterion 17
	int   criterion_b18;       //!< [195]-[784] Criterion 18
	int   criterion_b19;       //!< [196]-[788] Criterion 19
	int   criterion_b20;       //!< [197]-[792] Criterion 20
}sParaQuality;

/**
  * Parameters for quality check mask for eYsGlobeQ
  */
typedef struct ParaQualityMask
{
	// image para
	int    img_src_cols;              //!< Width of source image (single)
	int    img_src_rows;              //!< Height of source image
	int    img_lut_cols;              //!< Number of cols in LUT
	int    img_lut_rows;              //!< Number of rows in LUT
	double img_sensor_pixel_sz;       //!< Sensor pixel size (um)
	bool   img_show;                  //!< "true" for show image, "false" for disable show image
	bool   inf_show;                  //!< "true" for show information, "false" for disable show image

	// check mask
	bool   iq_chart_L_check;          //!< True for check ok, false for not ok
	bool   iq_chart_R_check;          //!< True for check ok, false for not ok
	bool   lb_chart_LR_check;         //!< Check LR stitching image
	bool   lb_chart_RL_check;         //!< Check RL stitching image

	// iq chart para
	int    iq_chart_fov_h;            //!< Field of view (degree) of IQ chart in col direction
	int    iq_chart_fov_v;            //!< Field of view (degree) of IQ chart in row direction
	int    iq_chart_cols;             //!< Number of pixels in IQ chart col direction
	int    iq_chart_rows;             //!< Number of pixels in IQ chart row direction
	int    iq_chart_L_theta;          //!< Theta (degree) of view point in L side IQ chart, it is from 0 ~ 180 degree
	int    iq_chart_L_phi;            //!< Phi (degree) of view point in L side IQ chart, it is from -180 ~ +180 degree
	int    iq_chart_R_theta;          //!< Theta (degree) of view point in R side IQ chart, it is from 0 ~ 180 degree
	int    iq_chart_R_phi;            //!< Phi (degree) of view point in R side IQ chart, it is from -180 ~ +180 degree
	bool   iq_chart_L_work;           //!< "true" for L-side IQ check enable, "false" is disable
	bool   iq_chart_R_work;           //!< "true" for L-side IQ check enable, "false" is disable

	// lighting board para
	int    lb_chart_cols;             //!< Number of pixels in lighting board chart col direction
	int    lb_chart_rows;             //!< Number of pixels in lighting board chart row direction
	int    lb_chart_row_center;       //!< Center (pixel) of lighting board chart in row direction
	int    lb_chart_line_shift;       //!< Stitching line shift pixel
	bool   lb_stitching_mode;         //!< "true" for enable stitching check, "false" for disable stitching check        

	// criterion
	float  criterion_v01;             //!< Criterion 1  : Max distance between L/R image-Red   color in Lab domain   
	float  criterion_v02;             //!< Criterion 2  : Max distance between L/R image-Green color in Lab domain  
	float  criterion_v03;             //!< Criterion 3  : Max distance between L/R image-Blue  color in Lab domain  
	float  criterion_v04;             //!< Criterion 4  : Max distance between L/R image-Black color in Lab domain  
	float  criterion_v05;             //!< Criterion 5  : Max distance between L/R image-White color in Lab domain 
	float  criterion_v06;             //!< Criterion 6  : Max distance between L/R image-Gray  color in Lab domain 
	float  criterion_v07;             //!< Criterion 7  : Min value of MTF1-400H0 
	float  criterion_v08;             //!< Criterion 8  : Min value of MTF2-400V0
	float  criterion_v09;             //!< Criterion 9  : Min value of MTF3-800H0
	float  criterion_v10;             //!< Criterion 10 : Min value of MTF4-800V0
	float  criterion_v11;             //!< Criterion 11 : Min value of MTF1-400H1 
	float  criterion_v12;             //!< Criterion 12 : Min value of MTF2-400V1
	float  criterion_v13;             //!< Criterion 13 : Min value of MTF3-800H1
	float  criterion_v14;             //!< Criterion 14 : Min value of MTF4-800V1
	float  criterion_v15;             //!< Criterion 15 : Max stitching error in LR boundary
	float  criterion_v16;             //!< Criterion 16 : Max stitching error in RL boundary
	float  criterion_v17;             //!< Criterion 17 : Max distance between L/R stitching image in Lab domain
	float  criterion_v18;             //!< Criterion 18 : Max distance between R/L stitching image in Lab domain
	float  criterion_v19;             //!< Criterion 19 : Non-used
	float  criterion_v20;             //!< Criterion 20 : Non-used

	// iq chart - circles para
	int    iq_chart_circles;          //!< Number of circles in chart
	double iq_chart_dp;               //!< Inverse ratio of accumulator resolution to the image resolution   
	double iq_chart_min_dist;         //!< Minimum distance between the centers of the detected circles
	double iq_chart_para1;            //!< It is the higher threshold of the two passed to the Canny() edge detector 
	double iq_chart_para2;            //!< It is the accumulator threshold for the circle centers at the detection stage
	int    iq_chart_min_radius;       //!< Half-diameter of min circle [pixel]
	int    iq_chart_max_radius;       //!< Half-diameter of max circle [pixel]
	
	// iq chart - white chart para
	double iq_chart_white_fx;         //!< White chart x position, px = chart_cx + chart_lx*fx
	double iq_chart_white_fy;         //!< White chart y position, py = chart_cy + chart_ly*fy
	int    iq_chart_white_cols;       //!< White chart width
	int    iq_chart_white_rows;       //!< White chart height
	bool   iq_chart_white_work;       //!< "true" for work, "false" for disable

	// iq chart - black chart para
	double iq_chart_black_fx;         //!< Black chart x position, px = chart_cx + chart_lx*fx
	double iq_chart_black_fy;         //!< Black chart y position, py = chart_cy + chart_ly*fy
	int    iq_chart_black_cols;       //!< Black chart width
	int    iq_chart_black_rows;       //!< Black chart height
	bool   iq_chart_black_work;       //!< "true" for work, "false" for disable

	// iq chart - gray chart para
	double iq_chart_gray_fx;          //!< Gray chart x position, px = chart_cx + chart_lx*fx
	double iq_chart_gray_fy;          //!< Gray chart y position, py = chart_cy + chart_ly*fy
	int    iq_chart_gray_cols;        //!< Gray chart width
	int    iq_chart_gray_rows;        //!< Gray chart height
	bool   iq_chart_gray_work;        //!< "true" for work, "false" for disable

	// iq chart - red chart para
	double iq_chart_red_fx;           //!< Red chart x position, px = chart_cx + chart_lx*fx
	double iq_chart_red_fy;           //!< Red chart y position, py = chart_cy + chart_ly*fy
	int    iq_chart_red_cols;         //!< Red chart width
	int    iq_chart_red_rows;         //!< Red chart height
	bool   iq_chart_red_work;         //!< "true" for work, "false" for disable

	// iq chart - green chart para
	double iq_chart_green_fx;         //!< Green chart x position, px = chart_cx + chart_lx*fx
	double iq_chart_green_fy;         //!< Green chart y position, py = chart_cy + chart_ly*fy
	int    iq_chart_green_cols;       //!< Green chart width
	int    iq_chart_green_rows;       //!< Green chart height
	bool   iq_chart_green_work;       //!< "true" for work, "false" for disable

	// iq chart - blue chart para
	double iq_chart_blue_fx;          //!< Blue chart x position, px = chart_cx + chart_lx*fx
	double iq_chart_blue_fy;          //!< Blue chart y position, py = chart_cy + chart_ly*fy
	int    iq_chart_blue_cols;        //!< Blue chart width
	int    iq_chart_blue_rows;        //!< Blue chart height
	bool   iq_chart_blue_work;        //!< "true" for work, "false" for disable

	// iq chart - TV Line H 400 para
	double iq_chart_MTF1_fx;          //!< MTF1 chart x position, px = chart_cx + chart_lx*fx
	double iq_chart_MTF1_fy;          //!< MTF1 chart y position, py = chart_cy + chart_ly*fy
	double iq_chart_MTF1_lp_area_ft;  //!< MTF1 line pairs width factor = chart_lx[mm]/ (line pair width[mm]) 
	int    iq_chart_MTF1_lp_num;      //!< MTF1 line pairs number
	int    iq_chart_MTF1_cols;        //!< MTF1 chart width
	int    iq_chart_MTF1_rows;        //!< MTF1 chart height
	bool   iq_chart_MTF1_work;        //!< "true" for work, "false" for disable

	// iq chart - TV Line V 400 para
	double iq_chart_MTF2_fx;          //!< MTF2 chart x position, px = chart_cx + chart_lx*fx
	double iq_chart_MTF2_fy;          //!< MTF2 chart y position, py = chart_cy + chart_ly*fy
	double iq_chart_MTF2_lp_area_ft;  //!< MTF2 line pairs width factor = chart_lx[mm]/ (line pair width[mm]) 
	int    iq_chart_MTF2_lp_num;      //!< MTF2 line pairs number
	int    iq_chart_MTF2_cols;        //!< MTF2 chart width
	int    iq_chart_MTF2_rows;        //!< MTF2 chart height
	bool   iq_chart_MTF2_work;        //!< "true" for work, "false" for disable

	// iq chart - TV Line H 800 para
	double iq_chart_MTF3_fx;          //!< MTF3 chart x position, px = chart_cx + chart_lx*fx
	double iq_chart_MTF3_fy;          //!< MTF3 chart y position, py = chart_cy + chart_ly*fy
	double iq_chart_MTF3_lp_area_ft;  //!< MTF3 line pairs width factor = chart_lx[mm]/ (line pair width[mm]) 
	int    iq_chart_MTF3_lp_num;      //!< MTF3 line pairs number
	int    iq_chart_MTF3_cols;        //!< MTF3 chart width
	int    iq_chart_MTF3_rows;        //!< MTF3 chart height
	bool   iq_chart_MTF3_work;        //!< "true" for work, "false" for disable

	// iq chart - TV Line V 800 para
	double iq_chart_MTF4_fx;          //!< MTF4 chart x position, px = chart_cx + chart_lx*fx
	double iq_chart_MTF4_fy;          //!< MTF4 chart y position, py = chart_cy + chart_ly*fy
	double iq_chart_MTF4_lp_area_ft;  //!< MTF4 line pairs width factor = chart_lx[mm]/ (line pair width[mm]) 
	int    iq_chart_MTF4_lp_num;      //!< MTF4 line pairs number
	int    iq_chart_MTF4_cols;        //!< MTF4 chart width
	int    iq_chart_MTF4_rows;        //!< MTF4 chart height
	bool   iq_chart_MTF4_work;        //!< "true" for work, "false" for disable
}sParaQualityMask;

/**
  * Parameters for focus check mask for eYsGlobeF
  */
typedef struct ParaFocusMask
{
	int  img_src_cols;           //!< Width of source image (single)
	int  img_src_rows;           //!< Height of source image
	int  img_src_col_center_L;   //!< Center of width in L side source image
	int  img_src_row_center_L;   //!< Center of height in L side source image
	int  img_src_col_center_R;   //!< Center of width in R side source image
	int  img_src_row_center_R;   //!< Center of height in R side source image
	int  img_src_cols_crop;      //!< Width of source image (single) after cropping
	int  img_src_rows_crop;      //!< Height of source image after cropping
	int  circle_semi_radius_0;   //!< Semi radius (inner) in pixels  
	int  circle_semi_radius_1;   //!< Semi radius (outter) in pixels
	int  FM_max_L;               //!< Camera max value of focus metric L lens
	int  FM_min_L;               //!< Camera available min value of focus metric L lens
	int  FM_cur_L;               //!< Camera current value of focus metric L lens
	int  FM_max_R;               //!< Camera max value of focus metric R lens
	int  FM_min_R;               //!< Camera available min value of focus metric R lens
	int  FM_cur_R;               //!< Camera current value of focus metric R lens
	bool focus_work_L;           //!< "true" for L side focus is ok, "false" for L side focus is not ok
	bool focus_work_R;           //!< "true" for R side focus is ok, "false" for R side focus is not ok
}sParaFocusMask;

/**
  * Parameters for Lsc check mask for eYsGlobeS
  */
typedef struct ParaLSC
{
	int    nCenterLX[3];            //!< LSC CenterLX, for now, equal to fisheye calibration center RGB
	int    nCenterLY[3];			//!< LSC CenterLY, for now, equal to fisheye calibration center RGB
	int    nCenterRX[3];            //!< LSC CenterRX, for now, equal to fisheye calibration center RGB
	int    nCenterRY[3];			//!< LSC CenterRY, for now, equal to fisheye calibration center RGB
	int    nCenterLX_Old[3];        //!< LSC Old CenterLX, 
	int    nCenterLY_Old[3];		//!< LSC Old CenterLY, 
	int    nCenterRX_Old[3];        //!< LSC Old CenterRX, 
	int    nCenterRY_Old[3];		//!< LSC Old CenterRY, 
	int    nCenterLX_Best[3];       //!< LSC Best CenterLX,
	int    nCenterLY_Best[3];		//!< LSC Best CenterLY, 
	int    nCenterRX_Best[3];       //!< LSC Best CenterRX,
	int    nCenterRY_Best[3];		//!< LSC Best CenterRY,
	int    nCenterLX_Final[3];      //!< LSC Final CenterLX, 
	int    nCenterLY_Final[3];		//!< LSC Final CenterLY, 
	int    nCenterRX_Final[3];      //!< LSC Final CenterRX, 
	int    nCenterRY_Final[3];		//!< LSC Final CenterRY, 
	int    nIsCenterAdjustLeft_L[3];//!< LSC Center Shift direction, Left or Right
	int    nIsCenterAdjustUp_L[3];	//!< LSC Center Shift direction, Up or Down
	int    nIsCenterAdjustLeft_R[3];//!< LSC Center Shift direction, Left or Right
	int    nIsCenterAdjustUp_R[3];	//!< LSC Center Shift direction, Up or Down
	int	   nCenter_range;			//!< LSC Center vary range
	int	   nCenter_stepL[3];		//!< LSC Center vary stepL			1
	int	   nCenter_stepR[3];		//!< LSC Center vary stepR			1
	double fCenterEpison_L[3];		//!< LSC fEpison value				256
	double fCenterEpison_R[3];		//!< LSC fEpison value				256

	int	   nGain1_L[3];				//!< LSC default Gain1_L RGB	100	100	100
	int	   nGain2_L[3];				//!< LSC default Gain2_L RGB	255	255	145
	int	   nGain1_R[3];				//!< LSC default Gain1_R RGB	100	100	100
	int	   nGain2_R[3];				//!< LSC default Gain2_R RGB	255	255	145
	int	   nGain1_L_Old[3];			//!< LSC old Gain1_L RGB
	int	   nGain2_L_Old[3];			//!< LSC old Gain2_L RGB
	int	   nGain1_R_Old[3];			//!< LSC old Gain1_R RGB
	int	   nGain2_R_Old[3];			//!< LSC old Gain2_R RGB
	int	   nGain1_L_Best[3];		//!< LSC new Gain1_L RGB
	int	   nGain2_L_Best[3];		//!< LSC new Gain2_L RGB
	int	   nGain1_R_Best[3];		//!< LSC new Gain1_R RGB
	int	   nGain2_R_Best[3];		//!< LSC new Gain2_R RGB
	int	   nGain1_L_Final[3];		//!< LSC Final Gain1_L RGB
	int	   nGain2_L_Final[3];		//!< LSC Final Gain2_L RGB 
	int	   nGain1_R_Final[3];		//!< LSC Final Gain1_R RGB
	int	   nGain2_R_Final[3];		//!< LSC Final Gain2_R RGB

	bool   bIsRightAdjust;			//!< LSC alternative adjust R	
	int    nIsGainAdjustBigger[3];	//!< LSC Gain vary direction, bigger,  smaller or equal
	int    nLsc_Res_L;				//!< LSC Resolution L
	int    nLsc_Res_R;				//!< LSC Resolution R
	int    nGain1_range;			//!< LSC Gain vary range			100
	int    nGain1_step[3];			//!< LSC Gain vary step				1
	int    nGain1_step2[3];			//!< LSC Gain vary step				1
	int    nGain2_range;			//!< LSC Gain vary range			100
	int    nGain2_step[3];			//!< LSC Gain vary step				1
	int    nGain2_step2[3];			//!< LSC Gain vary step				1
	double fGainEpison[3];			//!< LSC fEpison value				512
	int    nLRLuminLimit;			//!< LSC LR luminance cannot match  20
	int	   nLuminLimit;				//!< LSC ROI region limit			80
	double fCenterConvergentValue;	//!< LSC nConvergence Value			1
	double fGainConvergentValue;	//!< LSC nConvergence Value			3
	double fNFThrdRatio;			//!< LSC Noise Filter sigma ratio   2

	double fAver_L1[3][30];			//!< LSC ROI Average value in L1 region
	double fAver_R1[3][30];			//!< LSC ROI Average value in R1 region
	double fAver_L2[3][30];			//!< LSC ROI Average value in L2 region
	double fAver_R2[3][30];			//!< LSC ROI Average value in R2 region
	double fAver_L12[3][30];		//!< LSC ROI Average value in L12 region for test CenterY shift
	double fAver_R12[3][30];		//!< LSC ROI Average value in R12 region for test CenterY shift
	double fAver_L22[3][30];		//!< LSC ROI Average value in L22 region for test CenterY shift
	double fAver_R22[3][30];		//!< LSC ROI Average value in R22 region for test CenterY shift

	int	   nTotalFrame;				//!< LSC one times using total frame
	int	   nIterationTimes;			//!< LSC Calibration Center times
	int	   nIterationTimes2;		//!< LSC Calibration GainV1 times
	int	   nIterationTimes3;		//!< LSC Calibration GainV2 times
	int	   nIterationTimes4;		//!< LSC Calibration times
	int    nLRMutualAdjust;			//!< LSC dynammic adjust LR based on grayscale	0 
	int    nCenterLConvergentTimes;	//!< LSC Center L Convergent times
	int    nCenterRConvergentTimes;	//!< LSC Center R Convergent times
	int    nGainConvergentTimes;	//!< LSC Gain Convergent times
	int    nSkipK;					//!< LSC Block Skip number
	bool   bSkipFrame;				//!< LSC Skip Frame
	double fDeltaGainL_Curve[200];	//!< LSC Delta Gain CurveL
	double fDeltaGainR_Curve[200];	//!< LSC Delta Gain CurveR
	double fDeltaGain_Curve[200];	//!< LSC Delta Gain CurveLR
	double fDeltaCenterL_Curve[200];//!< LSC Delta Center Curve
	double fDeltaCenterR_Curve[200];//!< LSC Delta Center Curve
	int    nROI_Work_Value;			//!< LSC ROI check 

	int	   nUseResetValue;			//!< LSC Reset initial start point
	bool   bFastMode;				//!< Enable Fast mode
	double fPanelWbRatio[4][3];		//!< When Light Panel Luminance decays, we must calibrate panel
}sParaLSC;

/**
  * Parameters for Lsc check mask for eYsGlobeS
  */
typedef struct ParaAWB
{
	int	   nAWBGain_L[3];			//!< AWB default Gain1_L RGB	256 256 256
	int	   nAWBGain_R[3];			//!< AWB default Gain1_R RGB	256 256 256
	int	   nAWBGain_L_Old[3];		//!< AWB old Gain1_L RGB
	int	   nAWBGain_R_Old[3];		//!< AWB old Gain2_L RGB
	int	   nAWBGain_L_Best[3];		//!< AWB new Gain1_L RGB
	int	   nAWBGain_R_Best[3];		//!< AWB new Gain2_L RGB
	int	   nAWBGain_L_Final[3];		//!< AWB final Gain1_L RGB
	int	   nAWBGain_R_Final[3];		//!< AWB final Gain2_L RGB

	bool   bIsRightAdjust;			//!< AWB alternative adjust R	
	int    nIsGainAdjustBigger[3];	//!< AWB Gain vary direction, bigger,  smaller or equal
	int    nAWBGain_range_up;		//!< AWB Gain vary range			100
	int    nAWBGain_range_down;		//!< AWB Gain vary range			100
	int    nAWBGain_step[3];		//!< AWB Gain vary step				1

	double fAWBGainEpison[3];		//!< AWB fEpison value				512

	int	   nLuminLimit;				//!< AWB ROI region limit			80
	double fAWBGainConvergentValue;	//!< AWB nConvergence Value			3
	double fNFThrdRatio;			//!< AWB Noise Filter sigma ratio   2

	double fAver_L[3][30];			//!< AWB ROI Average value in L1 region
	double fAver_R[3][30];			//!< AWB ROI Average value in R1 region

	int	   nTotalFrame;				//!< AWB one times using total frame
	int	   nIterationTimes;			//!< AWB Calibration times
	int    nLRMutualAdjust;			//!< AWB dynammic adjust LR based on grayscale	0 
	int    nConvergentTimes;		//!< AWB Convergent times
	bool   bSkipFrame;				//!< AWB Skip Frame
	bool   bAEDelayTime;			//!< AE Convergence Time
	double fDeltaGain_Curve[200];	//!< AWB Delta Curve
	int   nROI_Work_Value;			//!< AWB ROI check 

	bool   bAWBGain_Adjust;			//!< Open AWBGain Tuning function
	bool   bLSCCenter_Adjust;		//!< Open LSCCenter Tuning function
	bool   bLSCGainV1_Adjust;		//!< Open LSCGain Tuning function
	bool   bLSCGainV2_Adjust;		//!< Open LSCGain Tuning function
	bool   bAETarget_Adjust;		//!< Enable revising AE target
	bool   bAutoAwb_Enable;			//!< Enalbe Auto Awb function
	bool   bAutoAE_Enable;			//!< Enalbe Auto AE function
	bool   bGammaOGHS_Enable;		//!< Enable ISP Gamma, OGHS
	bool   bSkip_Lut_K;				//!< Enable Lut Skip K value
	bool   bFastMode;				//!< Enable Fast mode
	double fPanelWbRatio[4][3];		//!< When Light Panel Luminance decays, we must calibrate panel

	int	   nYtargetToEnterOutdoor;	//!< AE Target Parameter
	int	   nAETargetL;				//!< AE Target L
	int    nAETargetR;				//!< AE Target R
}sParaAWB;

/*
 * NG code for eYsGlobeK
 */
enum NGCodeGlobeK
{
	NG_CODE_K_HEADER      = 10000,     //!< Header of NG code
	NG_CODE_K_FUNC        = 1,         //!< It for calibration function error
	NG_CODE_K_LENS_OFFSET = 10,        //!< It for lens x-y offset error
	NG_CODE_K_ROTATION    = 100,       //!< It for sensor rotation error
	NG_CODE_K_AGD         = 1000,      //!< It for image stitching error
};

/*
 * NG code for eYsGlobeQ
 */
enum NGCodeGlobeQ
{
	NG_CODE_Q_HEADER      = 10000,     //!< Header of NG code
	NG_CODE_Q_FUNC        = 1,         //!< It for calibration function error
	NG_CODE_Q_MTF         = 10,        //!< It for MTF error
	NG_CODE_Q_STITCHING   = 100,       //!< It for stitching error
	NG_CODE_Q_COLOR       = 1000       //!< It for color difference
};

/*
 * NG code for eYsGlobeS
 */
enum NGCodeGlobeS
{
	NG_CODE_S_HEADER      = 10000,     //!< Header of NG code
	NG_CODE_S_FUNC        = 1,         //!< It for calibration function error
	NG_CODE_S_ERROR       = 10,        //!< It for color difference error
};

/**
  * @} fisheye360
  */

} // fisheye360
} // eys



#endif // __EYS_FISHEYE360_DEF_H__