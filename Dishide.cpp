#include "libhide.hpp"

// g++ -c -o Dishide.o Dishide.cpp -I/usr/include/opencv4
// g++ -o Dishide Dishide.o libhide.hpp libhidecuda.o `pkg-config opencv4 --cflags --libs` -L/usr/local/cuda/lib64 -lcuda -lcudart


void cuda_read_from_frames(unsigned char *DATA, size_t UsedFramesNumber, size_t FrameSize,
                      char *FileStr, size_t SymbolNum,
                      size_t Step);


int main(int argc, char *argv[]){
    string InputVideoName;

    // ARGS PROCESSING
    // -----------------------------------------------------------------------
    // check args
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <InputVideoFilePath> <BlockSize (GB)>" << endl;
        exit(1);
    }

    InputVideoName = argv[1];
    VideoCapture InputVideo(InputVideoName);
    if(!InputVideo.isOpened())
        die_0("Error opening video stream or file: " + InputVideoName);

    string BlockSizeStr = argv[2];
    size_t BlockSize = stoi(BlockSizeStr);
    cout << "Block size, GB: " << BlockSize << endl;
    // -----------------------------------------------------------------------

    // Video info
    // -----------------------------------------------------------------------
    // frame size
    int frame_width = InputVideo.get(cv::CAP_PROP_FRAME_WIDTH);
    int frame_height = InputVideo.get(cv::CAP_PROP_FRAME_HEIGHT);
    cout << "Frame Size: " << frame_width << " x " << frame_height << endl;

    // FPS
    double fps = InputVideo.get(CAP_PROP_FPS);
    cout << "FPS: " << fps << endl;

    // FourCC - codec vars
    int fourcc = InputVideo.get(CAP_PROP_FOURCC);
    char fourcc_1, fourcc_2, fourcc_3, fourcc_4;
    fourcc_1 = fourcc & 255;         fourcc_2 = (fourcc >> 8) & 255;
    fourcc_3 = (fourcc >> 16) & 255; fourcc_4 = (fourcc >> 24) & 255;
    string fourcc_str = format("%c%c%c%c", fourcc_1, fourcc_2, fourcc_3, fourcc_4);
    cout << "FourCC literas: " << fourcc_str << endl;

    // frame count of video
    int frame_count = InputVideo.get(CAP_PROP_FRAME_COUNT);
    cout << "Frame count: " << frame_count << endl;
    // -----------------------------------------------------------------------

    // Get Meta info
    // -----------------------------------------------------------------------
    // read first frame
    Mat frame;
    InputVideo >> frame;

    int FrameStep = frame.step;
    size_t FrameSize = FrameStep * frame_height;

    // get step
    string StepStr = get_from_frame(frame, STEP_START, STEP_MAXLEN);
    int Step = stoi(StepStr);

    // get file name
    string FileName = get_from_frame(frame, FILENAME_START, FILENAME_MAXLEN);
    // remove slashes
    string::size_type pos = 0;                  // позиция разделителя в строке
    pos = FileName.find_first_of("/", pos);     // ищем первый символ, начиная с pos = 0, являющийся разделителем
    FileName.erase(0, pos + 1);                 // удаляем все что перед найденным символом

    // get number of used frames
    string UsedFrameNumberStr = get_from_frame(frame, USEDFRAMES_START, USEDFRAMES_MAXLEN);
    int UsedFrameNumber = stoi(UsedFrameNumberStr);
    // -----------------------------------------------------------------------

    // output file
    FileName = "Hided_" + FileName;
    ofstream File;
    File.open(FileName, ofstream::out | ofstream::trunc);
    if ( !File.is_open() ) {
        cerr << "File: " << FileName << " can not be opened" << endl;
        return 1;
    }
    cout << "Output file: " << FileName << endl;

    // get info from 1 frame
    // -----------------------------------------------------------------------
    InputVideo >> frame;

    // check mark -> stop circle
    if (get_data_from_frame(frame, MARK_START, MARK_STOP, META_STEP) != MARK) {
        InputVideo.release();
        cerr << "Wrong hided video format\n";
        exit(1);
    }

    // get number of symbols
    string SymbolNumberStr = get_from_frame(frame, NUMSYBMOLS_START, NUMSYBMOLS_MAXLEN);
    int SymbolNumber = stoi(SymbolNumberStr);

    // get symbols
    string data = get_data_from_frame(frame, SYMBOLS_START, SYMBOLS_START + SymbolNumber * Step - 1, Step);
    File << data;
    // -----------------------------------------------------------------------

    // DETECT PARTS NUMBER FOR MEMORY USAGE OPTIMIZATION
    // -----------------------------------------------------------------------
    size_t NeededDATASize           = (UsedFrameNumber - 1) * FrameSize;
    size_t AvailableDATASize        = BlockSize * 1024 * 1024 * 1024;

    size_t PartSize                 = NeededDATASize / AvailableDATASize;
    if (!PartSize)
        PartSize = 1;

    cout << "Parts number: " << PartSize << endl;

    size_t TMPUsedFramesNumberREM   = UsedFrameNumber - 1;
    size_t TMPUsedFramesNumber      = TMPUsedFramesNumberREM / PartSize;
    // -----------------------------------------------------------------------

    while (TMPUsedFramesNumberREM) {
        // allocate current data area and file area
        if (TMPUsedFramesNumberREM < TMPUsedFramesNumber) {
            // remainder (LAST PART)
            TMPUsedFramesNumber         = TMPUsedFramesNumberREM;
            TMPUsedFramesNumberREM      = 0;
            cout << "Oops, one more part" << endl;
        } else {
            // cut a piece
            TMPUsedFramesNumberREM      -= TMPUsedFramesNumber;
        }

        // CREATE LOWLEVEL ARRAYS
        // -----------------------------------------------------------------------
        cout << "Prepare lowlevel arrays\n";
        size_t FrameSize = frame_height * FrameStep;
        unsigned char *DATA = (unsigned char*)malloc(sizeof(unsigned char) * TMPUsedFramesNumber * FrameSize);
        for (int i = 1; i <= TMPUsedFramesNumber; i++) {
            InputVideo >> frame;
            memcpy(&DATA[(i-1) * FrameSize], frame.data, sizeof(unsigned char) * FrameSize);
        }

        // CREATE ARRAY OF ALL SYMBOLS
        // this one provides parallel calcs, because we know filesize
        char *FILE_ARRAY = (char*)malloc(sizeof(char) * TMPUsedFramesNumber * SymbolNumber);
        //-----------------------------------------------------------------------

        // transfer control to cuda functions
        cout << "Start CUDA calculations\n";
        cout << "TMPUsedFramesNumber: " << TMPUsedFramesNumber << endl;
        cuda_read_from_frames(DATA, TMPUsedFramesNumber, FrameSize, FILE_ARRAY, SymbolNumber, Step);

        // write file to FILE_ARRAY
        cout << "Write info to file\n";
        string FILE_ARRAY_STRING(FILE_ARRAY);
        File << FILE_ARRAY_STRING;

        free(DATA);
        free(FILE_ARRAY);
    }

    // Release the video capture objects
    InputVideo.release();
    File.close();

    return 0;
}
