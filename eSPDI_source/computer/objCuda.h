#include <cuda_runtime.h>
#include <list>

class objCuda
{
public:
    objCuda() = default;
    ~objCuda()
    {
        for (void *buffer : m_bufferList)
        {
            cudaFree(buffer);
        }
    }

    template <typename T>
    T* AllocateDeviceMemory(unsigned int size, T *src)
    {
        if (!size || !src)
            return nullptr;

        T *new_buffer = (T *)AllocateDeviceMemory(size);

        cudaMemcpy((void *)new_buffer, (void *)src, size, cudaMemcpyHostToDevice);

        return new_buffer;
    }

    void *AllocateDeviceMemory(unsigned int size)
    {
        if (!size)
            return nullptr;

        void *new_buffer;
        cudaMalloc(&new_buffer, size);
        m_bufferList.push_back(new_buffer);

        return new_buffer;
    }

    void CopyCudaDeviceMemoryToHost(void *host_buffer, void *cuda_buffer, unsigned int size)
    {
        if (!host_buffer || !cuda_buffer || !size) return;

        cudaMemcpy(host_buffer, cuda_buffer, size, cudaMemcpyDeviceToHost);
    }

    template <typename... Args>
    void Lanuch(dim3 size, void (*kernel)(Args...), Args... args)
    {
        int N = 1 << 4;
        dim3 threadsPerBlock(N, N);
        dim3 numBlocks((size.x + N - 1) / N, (size.y + N - 1) / N);

        kernel<<<numBlocks, threadsPerBlock>>>(args...);
    }

    int GetLanuchTimeMs()
    { 
        // TODO: implement lanuch time measure.
        return 0;
    }

private:
    std::list<void *> m_bufferList;
};