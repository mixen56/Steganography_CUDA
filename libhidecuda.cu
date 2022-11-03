// nvcc -c libhidecuda.cu -o libhidecuda.o

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

// CUDA
#include <cuda.h>
#include <cuda_runtime_api.h>


using namespace std;
// GLOBAL VARS
// -----------------------------------------------------------------------
// global write step in all frames
__device__ const int META_STEP = 10;

// meta of 0 frame (bits indexes): coordinates of step
// 10 - 100
__device__ const int STEP_START  = 10;
__device__ const int STEP_MAXLEN = 10;

// meta of 0 frame (bits indexes): coordinates of file name
// 210 - 500
__device__ const int CUDA_FILENAME_START  = 210;
__device__ const int FILENAME_MAXLEN = 30;

// meta of 0 frame (bits indexes): coordinates of number of used frames
// 610 - 700
__device__ const int USEDFRAMES_START  = 610;
__device__ const int USEDFRAMES_MAXLEN = 10;

// meta of others frames: mark
__device__ const int MARK_START  = 10;
__device__ const int MARK_STOP   = 50;
__device__ const int MARK_MAXLEN = 5;
__device__ char MARK[] = "abcde";

// meta of others frames: number of symbols in frame
__device__ const int NUMSYBMOLS_START    = 110;
__device__ const int NUMSYBMOLS_STOP     = 200;
__device__ const int NUMSYBMOLS_MAXLEN   = 10;

// meta of others frames: symbols
__device__ const int SYMBOLS_START   = 1000;
__device__ int SYMBOLS_STOP;
__device__ int SYMBOLS_MAXLEN;

// minimal number of symbols in frame
__device__ int SYMBOLS_FRAME_MIN = 10;

__device__ unsigned char SYMBOLS_OUT='*';
// -----------------------------------------------------------------------


__global__
void cuda_kernel_hide(unsigned char *DATA_CUDA, size_t FrameSize,
                 char *FILE_CUDA, size_t FileSize,
                 size_t SymbolNum, char *SymbolNumStr, size_t SymbolNumStrSize, size_t Step, int SYMBOLS_STOP) {
    size_t frame_index = blockIdx.x * FrameSize;                        // start of frame
    size_t file_index_start = blockIdx.x * SymbolNum;                   // start of file part

    size_t IterMeta;            // iterator index
    unsigned char write_char;   // char to write into frame
    char *FirstChar, *LastChar;  // start and stop of writing

    // write mark
    // -----------------------------------------------------------------------
    // cuda_write_data_to_frame(0, MARK, 5, DATA_CUDA, frame_index, 10, 50, 10);
    IterMeta = MARK_START + frame_index;
    FirstChar = MARK;
    LastChar  = FirstChar + MARK_MAXLEN;

    // write info
    for (char *letter = FirstChar; letter < LastChar; letter++ ) {
        write_char = static_cast<unsigned char>(*letter);
        DATA_CUDA[IterMeta] = write_char;
        IterMeta += META_STEP;
    }
    // -----------------------------------------------------------------------

    // write number of symbols
    // -----------------------------------------------------------------------
    // cuda_write_to_frame(SymbolNumStr, SymbolNumStrSize, DATA_CUDA, frame_index, 110, 10);
    IterMeta = NUMSYBMOLS_START + frame_index;
    LastChar = SymbolNumStr + SymbolNumStrSize;  // last symbol of metainfo

    // write info
    for (char *letter = SymbolNumStr; letter < LastChar; letter++ ) {
        write_char = static_cast<unsigned char>(*letter);
        DATA_CUDA[IterMeta] = *letter;
        IterMeta += META_STEP;
    }

    // write end of info
    for (int i = IterMeta; i < NUMSYBMOLS_START + frame_index + NUMSYBMOLS_MAXLEN * META_STEP; i+=META_STEP) {
        write_char = static_cast<unsigned char>('*');
        DATA_CUDA[IterMeta] = write_char;
    }
    // -----------------------------------------------------------------------

    // writer file symbols
    // -----------------------------------------------------------------------
    // cuda_write_data_to_frame(file_index_start, FILE_CUDA, SymbolNum, DATA_CUDA, frame_index, 1000, SYMBOLS_STOP, Step);
    IterMeta = SYMBOLS_START + frame_index;

    // file indexes
    FirstChar = FILE_CUDA + file_index_start;
    LastChar  = FirstChar + SymbolNum;


    // write info
    for (char *letter = FirstChar; letter < LastChar; letter++ ) {
        write_char = static_cast<unsigned char>(*letter);
        DATA_CUDA[IterMeta] = write_char;
        IterMeta += Step;
    }
    // -----------------------------------------------------------------------
}


// edit frames
void cuda_edit_frames(unsigned char *DATA, size_t UsedFramesNumber, size_t FrameSize,
                      string &FileStr, size_t SymbolNum,
                      string SymbolNumStr, size_t Step) {

    // GLOBAL VARS EDIT
    int SYMBOLS_STOP = FrameSize - 10000; // - 10000 just in case

    // copy memory from HOST to GPU
    unsigned char *DATA_CUDA;
    cudaMallocManaged(&DATA_CUDA, sizeof(unsigned char) * UsedFramesNumber * FrameSize);
    cudaMemcpy(DATA_CUDA, DATA, sizeof(unsigned char) * UsedFramesNumber * FrameSize, cudaMemcpyHostToDevice);

    // load file to gpu mem
    char *FILE_CUDA;
    size_t FileSize = FileStr.size();
    cudaMallocManaged(&FILE_CUDA, sizeof(char) * FileSize);
    cudaMemcpy(FILE_CUDA, FileStr.data(), sizeof(char) * FileSize, cudaMemcpyHostToDevice);

    // prepare SymbolNumStr as c string
    char *SymbolNumStrC;
    size_t SymbolNumStrCSize = SymbolNumStr.size();
    cudaMallocManaged(&SymbolNumStrC, sizeof(char) * SymbolNumStrCSize);
    cudaMemcpy(SymbolNumStrC, SymbolNumStr.data(), sizeof(char) * SymbolNumStrCSize, cudaMemcpyHostToDevice);

    cuda_kernel_hide<<<UsedFramesNumber, 1>>>(DATA_CUDA, FrameSize, FILE_CUDA, FileSize, SymbolNum, SymbolNumStrC, SymbolNumStrCSize, Step, SYMBOLS_STOP);
    cudaError_t cudaStatus = cudaDeviceSynchronize();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaDeviceSynchronize returned error code %d after launching cuda_kernel!\n", cudaStatus);
    }

    // copy memory from GPU to HOST
    cudaMemcpy(DATA, DATA_CUDA, sizeof(unsigned char) * UsedFramesNumber * FrameSize, cudaMemcpyDeviceToHost);

    cudaFree(DATA_CUDA);
    cudaFree(FILE_CUDA);
}


__global__
void cuda_kernel_dishide(unsigned char *DATA_CUDA, size_t FrameSize,
                 char *FILE_CUDA, size_t FileSize,
                 size_t SymbolNum, size_t Step, int SYMBOLS_STOP) {
    size_t frame_index = blockIdx.x * FrameSize;                        // start of frame
    size_t file_index_start = blockIdx.x * SymbolNum;                   // pointer to current file index

    unsigned char get_char;     // get char from frame
    char ready_char;            // get char from frame and convert to char
    size_t j;                      // iter index

    // get MARK and check existance
    // -----------------------------------------------------------------------
    char mark[MARK_MAXLEN + 1];
    j = 0;

    for (int i = MARK_START + frame_index; i <= MARK_STOP + frame_index; i+=META_STEP) {
        get_char = DATA_CUDA[i];
        ready_char = static_cast<char>(get_char);

        mark[j] = ready_char;
        j++;
    }
    mark[MARK_MAXLEN] = '\0';

    // if no mark
    bool go = true;
    for (j = 0; j < MARK_MAXLEN; j++) {
        if (mark[j] != MARK[j]) {
            printf ("CUDA %ld: COMAPARE (%c %c)\n", blockIdx.x, mark[j], MARK[j]);
            go = false;
            break;
        }
    }
    // -----------------------------------------------------------------------

    // get symbol num
    // -----------------------------------------------------------------------
    if (go) {
        char numsymbols_str[NUMSYBMOLS_MAXLEN + 1];

        j = 0;
        for (int i = NUMSYBMOLS_START + frame_index; i < NUMSYBMOLS_STOP + frame_index; i+=META_STEP) {
            get_char = DATA_CUDA[i];;
            ready_char = static_cast<char>(get_char);
            if (ready_char == SYMBOLS_OUT)
                break;

            numsymbols_str[j] = ready_char;
            j++;
        }
        numsymbols_str[j] = '\0';

        // size_t numsymbols = stoi(numsymbols_str);
        // my stoi
        int numsymbols = 0;
        char ch;
        int cur_power = 1;

        for (int i = j - 1; i >= 0; i--) {
            ch = numsymbols_str[i];
            switch (ch) {
                case '0':
                    numsymbols = numsymbols + 0 * cur_power;
                    break;
                case '1':
                    numsymbols = numsymbols + 1 * cur_power;
                    break;
                case '2':
                    numsymbols = numsymbols + 2 * cur_power;
                    break;
                case '3':
                    numsymbols = numsymbols + 3 * cur_power;
                    break;
                case '4':
                    numsymbols = numsymbols + 4 * cur_power;
                    break;
                case '5':
                    numsymbols = numsymbols + 5 * cur_power;
                    break;
                case '6':
                    numsymbols = numsymbols + 6 * cur_power;
                    break;
                case '7':
                    numsymbols = numsymbols + 7 * cur_power;
                    break;
                case '8':
                    numsymbols = numsymbols + 8 * cur_power;
                    break;
                case '9':
                    numsymbols = numsymbols + 9 * cur_power;
                    break;
            }
            cur_power*=10;  // increase power
        }
    // -----------------------------------------------------------------------

    // get data from frame and write to file
    // -----------------------------------------------------------------------
    //if (go) {
        j = file_index_start;
        for (int i = SYMBOLS_START + frame_index; i <= SYMBOLS_START + frame_index + numsymbols * Step - 1; i+=Step) {
            get_char = DATA_CUDA[i];
            ready_char = static_cast<char>(get_char);

            FILE_CUDA[j] = ready_char;
            j++;
        }
    }
    // last frame
    if (gridDim.x == (blockIdx.x + 1)) {
        FILE_CUDA[j] = '\0';
    }
    // -----------------------------------------------------------------------
}


// fill FILE
void cuda_read_from_frames(unsigned char *DATA, size_t UsedFramesNumber, size_t FrameSize,
                      char *FileStr, size_t SymbolNum,
                      size_t Step) {

    // GLOBAL VARS EDIT
    int SYMBOLS_STOP = FrameSize - 10000; // - 10000 just in case

    // copy memory from HOST to GPU
    unsigned char *DATA_CUDA;
    cudaMallocManaged(&DATA_CUDA, sizeof(unsigned char) * UsedFramesNumber  * FrameSize);
    cudaMemcpy(DATA_CUDA, DATA, sizeof(unsigned char) * UsedFramesNumber * FrameSize, cudaMemcpyHostToDevice);

    // load file to gpu mem
    char *FILE_CUDA;
    size_t FileSize = UsedFramesNumber * SymbolNum;
    cudaMallocManaged(&FILE_CUDA, sizeof(char) * FileSize);
    cudaMemcpy(FILE_CUDA, FileStr, sizeof(char) * FileSize, cudaMemcpyHostToDevice);

    cuda_kernel_dishide<<<UsedFramesNumber, 1>>>(DATA_CUDA, FrameSize, FILE_CUDA, FileSize, SymbolNum, Step, SYMBOLS_STOP);
    cudaError_t cudaStatus = cudaDeviceSynchronize();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaDeviceSynchronize returned error code %d after launching cuda_kernel!\n", cudaStatus);
    }

    // copy memory from GPU to HOST
    cudaMemcpy(FileStr, FILE_CUDA, sizeof(char) * UsedFramesNumber * SymbolNum, cudaMemcpyDeviceToHost);
    // thanks father for memcpy bug
    FileStr[UsedFramesNumber * SymbolNum] = '\0';

    cudaFree(DATA_CUDA);
    cudaFree(FILE_CUDA);
}
