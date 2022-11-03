#include "opencv2/opencv.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>


using namespace std;
using namespace cv;


// GLOBAL VARS
// -----------------------------------------------------------------------
// global write step in all frames
int META_STEP = 10;

// meta of 0 frame (bits indexes): coordinates of step
// 10 - 100
int STEP_START  = 10;
int STEP_MAXLEN = 10;

// meta of 0 frame (bits indexes): coordinates of file name
// 210 - 500
int FILENAME_START  = 210;
int FILENAME_MAXLEN = 30;

// meta of 0 frame (bits indexes): coordinates of number of used frames
// 610 - 700
int USEDFRAMES_START  = 610;
int USEDFRAMES_MAXLEN = 10;

// meta of others frames: mark
int MARK_START  = 10;
int MARK_STOP   = 50;
int MARK_MAXLEN = 5;
string MARK = "abcde";

// meta of others frames: number of symbols in frame
int NUMSYBMOLS_START    = 110;
int NUMSYBMOLS_STOP     = 200;
int NUMSYBMOLS_MAXLEN   = 10;

// meta of others frames: symbols
int SYMBOLS_START   = 1000;
int SYMBOLS_STOP;
int SYMBOLS_MAXLEN;

// minimal number of symbols in frame
int SYMBOLS_FRAME_MIN = 10;

unsigned char SYMBOLS_OUT='*';
// -----------------------------------------------------------------------


// FUNCTIONS
// -----------------------------------------------------------------------
// fail program if only zero video is opened
void die_0(string msg) {
    cerr << msg << endl;
    exit(1);
}


// fail program if only one video is opened
void die_1(string msg, VideoCapture &InputVideo) {
    cerr << msg << endl;
    InputVideo.release();
    exit(1);
}


// fail program if two videos are opened
void die_2(string msg, VideoCapture &InputVideo, VideoWriter &OutputVideo) {
    cerr << msg << endl;
    InputVideo.release();
    OutputVideo.release();
    exit(1);
}


// print info about frame
void MatType(Mat inputMat) {
    int inttype = inputMat.type();

    string r, a;
    uchar depth = inttype & CV_MAT_DEPTH_MASK;
    uchar chans = 1 + (inttype >> CV_CN_SHIFT);

    switch ( depth ) {
        case CV_8U:  r = "8U";   a = "Mat.at<uchar>(y,x)"; break;
        case CV_8S:  r = "8S";   a = "Mat.at<schar>(y,x)"; break;
        case CV_16U: r = "16U";  a = "Mat.at<ushort>(y,x)"; break;
        case CV_16S: r = "16S";  a = "Mat.at<short>(y,x)"; break;
        case CV_32S: r = "32S";  a = "Mat.at<int>(y,x)"; break;
        case CV_32F: r = "32F";  a = "Mat.at<float>(y,x)"; break;
        case CV_64F: r = "64F";  a = "Mat.at<double>(y,x)"; break;
        default:     r = "User"; a = "Mat.at<UKNOWN>(y,x)"; break;
    }

    r += "C";
    r += (chans+'0');
    cout << "Mat is of type " << r << " and should be accessed with " << a << endl;
}


// write info to frame
int write_to_frame(string MetaInfo, Mat &frame, int start, int max_len) {
    int IterMeta = start;
    unsigned char write_char;

    // write info
    for (auto letter = MetaInfo.begin(); letter < MetaInfo.end(); letter++ ) {
        write_char = static_cast<unsigned char>(*letter);
        frame.data[IterMeta] = *letter;

        // limit is very strong for msg
        if (IterMeta > start + max_len * META_STEP)
            return 1;

        IterMeta += META_STEP;
    }

    // write end of info
    for (IterMeta; IterMeta < start + max_len * META_STEP; IterMeta+=META_STEP) {
        write_char = static_cast<unsigned char>(SYMBOLS_OUT);
        frame.at<uchar>(IterMeta) = write_char;
    }
    return 0;
}

// get from frame
string get_from_frame(Mat &frame, int start, int max_len) {
    string info;
    unsigned char get_char;
    char ready_char;

    for (int i = start; i < start + max_len * META_STEP; i+=META_STEP) {
        get_char = static_cast<uchar>(frame.at<uchar>(i));
        ready_char = static_cast<char>(get_char);
        if (ready_char == SYMBOLS_OUT)
            return info;
        info.append(1, ready_char);
    }

    cout << "Returned info: " << info << endl;
    return info;
}


// write data from file to frame
int write_data_to_frame(string MetaInfo, Mat &frame, int start, int stop, int step) {
    int IterMeta = start;
    unsigned char write_char;

    // write info
    for (auto letter = MetaInfo.begin(); letter < MetaInfo.end(); letter++ ) {
        write_char = static_cast<unsigned char>(*letter);
        frame.at<uchar>(IterMeta) = write_char;

        // limit is very strong for msg
        if (IterMeta > stop)
            return 1;

        IterMeta += step;
    }

    return 0;
}

// get data from frame
string get_data_from_frame(Mat &frame, int start, int stop, int step) {
    string info;
    unsigned char get_char;
    char ready_char;

    for (int i = start; i <= stop; i+=step) {
        get_char = frame.at<uchar>(i);
        //ready_char = static_cast<char>(get_char);
        info.append(1, get_char);
    }

    return info;
}

// calc partitions (CASE 1 or CASE 2: return code) and step size
int calc_partitions(size_t FileSize, size_t FrameCount, size_t &UsedFrames,
                    size_t &SymbolNum, size_t &Remainder, size_t &Step) {
    UsedFrames = 0;
    Remainder  = 0;

    size_t x = FileSize / FrameCount + 1;              // +1 just in case

    if (x < FrameCount) {
        Remainder  = FileSize % x;
        SymbolNum  = x;
        UsedFrames = FileSize / x;                  // -2 -> first meta frame and remainder
        Step = SYMBOLS_MAXLEN / SymbolNum - 1;      // dont out our limits

        if (Remainder)
            UsedFrames++;                           // for remainder
        return 1;
    } else {
        Remainder = FileSize % (FrameCount - 2);
        SymbolNum = FileSize / (FrameCount - 2);
        UsedFrames = FrameCount - 2;                // last frame for remainder and for meta frame
        Step = SYMBOLS_MAXLEN / SymbolNum - 1;      // dont out out limits

        if (Remainder)
            UsedFrames++;                           // for remainder
        return 2;
    }
}


std::ifstream::pos_type filesize(const char* filename) {
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}


int get_file(string FileName, size_t &FileSize, string &FileStr) {
    ifstream File(FileName);
    if ( !File.is_open() ) {
        return 1;
    }

    // get info about size
    FileSize = filesize(FileName.c_str());        // get size

    // get file as string
    stringstream buffer;
    buffer << File.rdbuf();
    FileStr = buffer.str();

    File.close();
    return 0;
}


int check_editable(size_t frame_count, size_t AllowedPercent, size_t FileSize) {
    size_t MaxBits = SYMBOLS_MAXLEN * frame_count;
    size_t MaxEditBits = MaxBits / 100 * AllowedPercent;
    cout << "FileSize: " << FileSize << endl;
    cout << "Max editable bits: " << MaxEditBits << endl;
    if (FileSize > MaxEditBits)
        return 1;
    return 0;
    // TODO: ADD PARTS IF LONG INT IS TOO SMALL
}

struct InvalidChar {
    bool operator()(char c) const {
        // return !isprint(static_cast<unsigned char>(c));
        return (c > 127 || c < 0);
    }
};

template <typename C, typename P>
void erase_remove_if(C& c, P predicate) {
    c.erase(std::remove_if(c.begin(), c.end(), predicate), c.end());
}
// -----------------------------------------------------------------------
