#!/bin/sh

echo "Project list : "
echo "=================="
echo "1. x86-64 & Nvidia TX2"
echo "2. x86-64"
echo "3. Hypatia"
echo "4. TI"
echo "5. NVIDIA TX2"
echo "=================="
echo 

read -p "Please select project: " project
read -p "Please enter version:  " version

if [ -z $version ]; then
    version=$(git rev-parse --short HEAD)
fi

case $project in
        [1]* ) 
        echo "build x86_64 & Nvidia TX2 SDK"
                sh ./project/x86_64/ReleaseSDK.sh $version FALSE 2>&1 | tee /tmp/log-$(date '+%Y%m%d-%H%M%S')
                break;;
        [2]* ) 
        echo "build x86_64 SDK"
                sh ./project/x86_64/ReleaseSDK.sh $version TRUE 2>&1 | tee /tmp/log-$(date '+%Y%m%d-%H%M%S')
                break;;
        [3]* ) 
        echo "build Hypatia SDK"
		sh ./project/hypatia/ReleaseHypatiaSDK.sh $version 2>&1 | tee /tmp/log-$(date '+%Y%m%d-%H:%M:%S')
		break;;
        [4]* ) 
        echo "build TI SDK"
		sh ./project/ti/ReleaseTISDK.sh $version 2>&1 | tee /tmp/log-$(date '+%Y%m%d-%H:%M:%S')
		break;;
        [5]* ) 
        echo "build NVIDIA TX2 SDK"
		sh ./project/nvidia/ReleaseTx2SDK.sh $version 2>&1 | tee /tmp/log-$(date '+%Y%m%d-%H:%M:%S')
		break;;
        * ) 
        echo "Please selet project no.";;
esac

