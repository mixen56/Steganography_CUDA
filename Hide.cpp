#include "libhide.hpp"

// g++ -c Hide.cpp -o Hide.o -I/usr/include/opencv4
// g++ -o Hide Hide.o libhide.hpp libhidecuda.o `pkg-config opencv4 --cflags --libs` -L/usr/local/cuda/lib64 -lcuda -lcudart


// CUDA function
void cuda_edit_frames(unsigned char *DATA, size_t UsedFramesNumber, size_t FrameSize,
                      string &FileStr, size_t SymbolNum,
                      string SymbolNumStr, size_t Step);

int main(int argc, char *argv[]){
    string InputVideoName, FileName, AllowedPercentName, BlockSizeStr;

    // ARGS PROCESSING
    // -----------------------------------------------------------------------
    // check args
    if (argc < 5) {
        cout << "Usage: " << argv[0] << " <InputVideoFilePath> <File> <PercentBit> <BlockSize (GB)>" << endl;
        exit(1);
    }

    InputVideoName      = argv[1];
    FileName            = argv[2];
    AllowedPercentName  = argv[3];
    BlockSizeStr        = argv[4];

    // input video
    VideoCapture InputVideo(InputVideoName);
    if(!InputVideo.isOpened())
        die_0("Error opening video stream or file: " + InputVideoName);

    // File Info
    size_t FileSize;       // file size
    string FileStr;     // file as string
    if (get_file(FileName, FileSize, FileStr))
        die_1("Error opening file: " + FileName, InputVideo);

    // input % bit
    int AllowedPercent = stoi(AllowedPercentName);
    cout << "Allowed percent editable bits: " << AllowedPercent << endl;
    if ( AllowedPercent > 100 || AllowedPercent < 0 )
        die_1("PercentBit is from 0 to 100 integer value", InputVideo);

    size_t BlockSize = stoi(BlockSizeStr);
    cout << "Block size, GB: " << BlockSize << endl;
    // -----------------------------------------------------------------------

    // Video info
    // -----------------------------------------------------------------------
    // frame size
    size_t frame_width = InputVideo.get(cv::CAP_PROP_FRAME_WIDTH);
    size_t frame_height = InputVideo.get(cv::CAP_PROP_FRAME_HEIGHT);
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
    size_t frame_count = InputVideo.get(CAP_PROP_FRAME_COUNT);
    cout << "Frame count: " << frame_count << endl;
    // -----------------------------------------------------------------------

    // Meta info
    // -----------------------------------------------------------------------
    // read first frame
    Mat frame;
    InputVideo >> frame;

    // step (width multiply number of element size -> 1920 * 3 = 5760 - step)
    size_t FrameStep = frame.step;
    size_t FrameSize = frame_height * FrameStep;

    // GLOBAL VARS EDIT
    SYMBOLS_STOP = FrameStep * frame_height - 10000; // - 10000 just in case
    SYMBOLS_MAXLEN  = SYMBOLS_STOP - SYMBOLS_START;

    // check that we can use our algo (only after detecting SYMBOLS_MAXLEN)
    if (check_editable(frame_count, AllowedPercent, FileSize))
        die_1("Input file is too big: " + FileName, InputVideo);

    // Output video
    string OutputVideoName = "Hided_" + InputVideoName + ".avi";
    cout << "Output video name: " << OutputVideoName << endl;
    // write AVI not to lose bits
    VideoWriter OutputVideo(OutputVideoName,
        cv::VideoWriter::fourcc('F', 'F', 'V', '1'),
        fps, Size(frame_width,frame_height));

    // write meta info about file name
    if (write_to_frame(FileName, frame, FILENAME_START, FILENAME_MAXLEN))
        die_2("Size of " + InputVideoName + " is very big", InputVideo, OutputVideo);

    // calc symbols of every frame, used frames and step between symbols in frames
    size_t UsedFramesNumber, SymbolNum, Remainder, Step;
    // if remainder exists -> use last frame of UsedFramesNumber for remainder else
    // use all frames without remainder
    calc_partitions(FileSize, frame_count, UsedFramesNumber, SymbolNum, Remainder, Step);
    // write* methods use strings
    string SymbolNumStr         = to_string(SymbolNum);
    string RemainderStr         = to_string(Remainder);
    string UsedFramesNumberStr  = to_string(UsedFramesNumber);

    string MetaStepStr = to_string(Step);
    // write meta info about step in frame
    if (write_to_frame(MetaStepStr, frame, STEP_START, STEP_MAXLEN))
        die_2("Size of " + MetaStepStr + " is very big", InputVideo, OutputVideo);

    // write meta info about used frames
    if (write_to_frame(UsedFramesNumberStr, frame, USEDFRAMES_START, USEDFRAMES_MAXLEN))
        die_2("Size of " + UsedFramesNumberStr + " is very big", InputVideo, OutputVideo);

    OutputVideo.write(frame);       // write frame to output video
    // -----------------------------------------------------------------------

    // NOW WE ARE ON THE 2nd FRAME (1st FRAME  is for FULL META)

    // DETECT PARTS NUMBER FOR MEMORY USAGE OPTIMIZATION
    // -----------------------------------------------------------------------
    size_t NeededDATASize           = (UsedFramesNumber - 1) * FrameSize;
    size_t AvailableDATASize        = BlockSize * 1024 * 1024 * 1024;

    size_t PartSize                 = NeededDATASize / AvailableDATASize;
    if (!PartSize)
        PartSize = 1;
    cout << "Parts number: " << PartSize << endl;

    size_t TMPUsedFramesNumberREM   = UsedFramesNumber - 1;
    size_t TMPUsedFramesNumber      = TMPUsedFramesNumberREM / PartSize;

    size_t TmpRemainder_or_SymbolNum    = SymbolNum;
    string TmpRemainder_or_SymbolNumStr = SymbolNumStr;
    if (Remainder) {
        TmpRemainder_or_SymbolNum       = Remainder;
        TmpRemainder_or_SymbolNumStr    = RemainderStr;
    }
    // -----------------------------------------------------------------------

    // BITE OFF ALL DATA
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


        // calcs

        // CREATE ARRAY OF ALL BITS
        // -----------------------------------------------------------------------
        cout << "Prepare lowlevel array\n";
        unsigned char *DATA = (unsigned char*)malloc(sizeof(unsigned char) * TMPUsedFramesNumber * FrameSize);
        if (DATA == nullptr) {
            die_2("Cant allocate lowlevel array", InputVideo, OutputVideo);
        }

        for (size_t i = 1; i <= TMPUsedFramesNumber; i++) {
            InputVideo >> frame;
            memcpy(&DATA[(i-1) * FrameSize], frame.data, sizeof(unsigned char) * FrameSize);
        }
        //-----------------------------------------------------------------------

        // transfer control to cuda functions
        cout << "Start CUDA calculations\n";
        string tmpFileStr = FileStr.substr(0, SymbolNum * TMPUsedFramesNumber);
        cuda_edit_frames(DATA, TMPUsedFramesNumber, FrameSize, tmpFileStr,
            SymbolNum, SymbolNumStr, Step);

        // erase processed bytes of file
        FileStr.erase(0, SymbolNum * TMPUsedFramesNumber);

        // write edites frames to OutputVideo
        cout << "Write edited frames\n";
        for (int i = 1; i <= TMPUsedFramesNumber; i++) {
            memcpy(frame.data, &DATA[(i-1) * FrameSize], sizeof(unsigned char) * FrameSize);
            OutputVideo.write(frame);                               // write frame to output video
        }

        // free lowlevel array
        free(DATA);
    }

    // detect remainder or last common frame
    string TmpNumStr;
    int TmpNum;
    if (Remainder) {
        TmpNum = Remainder;
        TmpNumStr = RemainderStr;
    } else {
        TmpNum = SymbolNum;
        TmpNumStr = SymbolNumStr;
    }

    // write to last frame
    // -----------------------------------------------------------------------
    // string part = FileStr.substr((UsedFramesNumber-1) * SymbolNum, TmpNum);         // get part of file
    string part = FileStr.substr(0, TmpNum);         // get part of file
    InputVideo >> frame;

    // mark
    write_data_to_frame(MARK, frame, MARK_START, MARK_STOP, META_STEP);
    // number of symbols
    write_to_frame(TmpNumStr, frame, NUMSYBMOLS_START, NUMSYBMOLS_MAXLEN);
    // symbols
    write_data_to_frame(part, frame, SYMBOLS_START, SYMBOLS_STOP, Step);

    OutputVideo.write(frame);       // write frame to output video
    // -----------------------------------------------------------------------

    // Write remainder frames
    // -----------------------------------------------------------------------
    for (int i = UsedFramesNumber; i < frame_count; i++) {
        InputVideo >> frame;
        OutputVideo.write(frame);       // write frame to output video
    }
    // -----------------------------------------------------------------------

    // Release the video capture objects
    InputVideo.release();
    OutputVideo.release();

    return 0;
}
