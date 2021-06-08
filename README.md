# HD-DM-Linux-SDK-Release
## I.EtronDI Enviroment Setting (Ubuntu x86_64)
Please run below command:
```sh 
cd Buid_Environment
sudo sh setup_env.sh 
```

## II.Build & Run DMPreview
There are two ways to compile the program:  
###### (1) Using QtCreator
-  1.1 Open QtCreator.  
-  1.2 Click "Open Project".  
![l-1](https://user-images.githubusercontent.com/13328289/120995299-dae02f80-c7b7-11eb-9ee5-f498787dcdd7.png)

-  1.3  Choose "./DMPreview/DMPreview.pro".  
![l-2](https://user-images.githubusercontent.com/13328289/120995336-e5022e00-c7b7-11eb-9a66-cb441a7be4e4.png)

- 1.4 Click "Configure Project".  
![l-4](https://user-images.githubusercontent.com/13328289/120995378-ed5a6900-c7b7-11eb-9a8c-ac3fdd1deb6b.png)

-  1.5 Select  Build/Clean All & Build/Rebuild All
![l-6](https://user-images.githubusercontent.com/13328289/120995539-124edc00-c7b8-11eb-9b54-3876490f6375.png)

-  1.6 Select Build/Run to launch program
![l-7](https://user-images.githubusercontent.com/13328289/120996352-d2d4bf80-c7b8-11eb-95a7-2ed7debe9af9.png)


###### (2) Using QMake
-  Build & Run   
```sh 
#  Build:   
cd DMPreview  
qmake
make
```
  ![ll-2](https://user-images.githubusercontent.com/13328289/120999313-8f2f8500-c7bb-11eb-82fc-daf44acf87f3.png)
    
```sh   
#  Run:  
cd bin
sudo sh run_DMPreview.sh  
```
![ll-3](https://user-images.githubusercontent.com/13328289/120999631-edf4fe80-c7bb-11eb-88ca-1a31418287e0.png)
![ll-4](https://user-images.githubusercontent.com/13328289/120999644-f0efef00-c7bb-11eb-81a5-5fa57874439e.png)


