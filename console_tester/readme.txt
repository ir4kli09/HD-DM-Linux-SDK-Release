1. How to build console tool:

sh build.sh (For x86-64, select '1'. For NVIDIA TX2 and NVIDIA NANO, select '2'. )

2. How to run console tool:

(1) For x86-64:
sh run_x86.sh

(2) For NVIDIA TX2 and NVIDIA NANO:
sh run_arm64_tx2.sh


3. There are the macros in main.cpp to enable/disable the demo:


(1) _ENABLE_INTERACTIVE_UI_ : Configure the camera device through step-by-step instructions. By default, it is enabled.
(2) _ENABLE_PROFILE_UI_ : Profile the frame rate of camera device. By default, it is disabled.
(3) _ENABLE_FILESAVING_DEMO_UI_ : Save rgb files of color stream and yuyv files of depth stream. By default, it is disabled.
(4) _ENABLE_FILESAVING_DEMO_POINT_CLOUD_UI_: Run the point cloud demo. By default, it is disabled.

