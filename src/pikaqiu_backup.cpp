#include "network.h"
#include "mtcnn.h"
#include <time.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <map>
#include <experimental/filesystem>

namespace Runner {
    Mat last_frame;
    Mat temp_frame;

    mutex last_lock;
    atomic_flag last_used;

    VideoCapture cap;
    mtcnn *cnn;

    int init() {
        cap = VideoCapture(0);
        Mat image;
        if (!cap.isOpened()) {
            cout << "Failed to open!" << endl; 
            return -1;
        }
        cap >> image;
        if (!image.data){
            cout << "No image data!" << endl;  
            return -1;
        }
        cnn = new mtcnn(image.rows, image.cols);
        cap >> last_frame;
        return 0;
    }

    void read_camera() {
        while (true) {
            cap >> temp_frame;
            if (!last_lock.try_lock()) continue;
            cv::swap(last_frame, temp_frame);
            last_used.clear();
            last_lock.unlock();
        }
    }
    void run() {
        while (true) {
            last_lock.lock();
            if (!last_used.test_and_set()) {
                cnn->findFace(last_frame);
                imshow("result", last_frame);
                if (waitKey(1) >= 0) break;   
            }
            last_lock.unlock();
        }
    }

    int main() {
        assert(init() == 0);

        thread camera(read_camera);
        thread runner(run);

        runner.join();
        camera.~thread();

        return 0;
    }
};

void draw_5_points(Mat &image, vector< BoundingBox > &boxes) {
    for (auto &box : boxes) {
        bool is_frontal = box.is_frontal();
        printf("is_frontal=%d\n", is_frontal);
        int radius = 3;
        for (auto p : box.points) {
            circle(image,Point((int)p.x, (int)p.y), radius, Scalar(0,255,255), -1);
        }
    }
}

int old_main(int argc, char **argv) {
    //Size sz = Size(1280, 720);
    //Size sz = Size(640, 480);

    Mat image;
    VideoCapture cap;
    VideoWriter video_writer;
    if (argc > 1) {
        cap = VideoCapture(argv[1]);
    } else {
        cap = VideoCapture(0);
    }
    cap >> image;
    cout << "Original size = " << image.rows << "x" << image.cols << endl;
    //Rect ROI = Rect(image.cols / 6, image.rows / 3, image.cols / 2, image.rows / 3 * 2);
    Rect ROI = Rect(0, 0, image.cols, image.rows); // full image
    //resize(image, image, sz, 0, 0);
    
    video_writer.open("output_phuongnam.avi", VideoWriter::fourcc('X', 'V', 'I', 'D'), cap.get(CAP_PROP_FPS),
        image(ROI).size()
        //image.size()
    );
    

    if(!cap.isOpened())  
        cout<<"fail to open!"<<endl; 
    //cap>>image;
    if(!image.data){
        cout<<"fuck"<<endl;  
        return -1;
    }

    

    mtcnn find(ROI.height, ROI.width);

    clock_t start;

    int num_frame = 0;
    double total_time = 0;
    int frame_id = 0;

    SORT sorter(15);

    int shift_width = 0;
    int last_frame_shifted = 0;

    map<int, int> id_count;
    vector<int> compression_params;
    compression_params.push_back(IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(5);

    while (cap.read(image)) {
        try {
            //start = clock();
            if (image.empty()) break;
            //resize(image, image, sz);

            image = image(ROI);

            auto start_time = std::chrono::system_clock::now();
            vector< BoundingBox > boxes = find.findFace(image);
            auto diff1 = std::chrono::system_clock::now() - start_time;
            auto t1 = std::chrono::duration_cast<std::chrono::nanoseconds>(diff1);

            vector<TrackingBox> detFrameData;
            for (int i = 0; i < boxes.size(); ++i) {
                //cerr << boxes[i].x << ' ' << boxes[i].y << ' ' << boxes[i].width << ' ' << boxes[i].height << endl;
                TrackingBox cur_box;
                cur_box.box = boxes[i].rect;
                cur_box.id = i;
                cur_box.frame = frame_id;
                detFrameData.push_back(cur_box);
            }
            ++frame_id;

            auto start_track_time = std::chrono::system_clock::now();
            vector<TrackingBox> tracking_results = sorter.update(detFrameData);
            //vector<TrackingBox> tracking_results = detFrameData;
            auto diff2 = std::chrono::system_clock::now() - start_track_time;
            auto t2 = std::chrono::duration_cast<std::chrono::nanoseconds>(diff2);
            
            for (TrackingBox it : tracking_results) {
                rectangle(image, Point(it.box.y, it.box.x), Point(it.box.height, it.box.width), sorter.randColor[it.id % 20], 2,8,0);
                Mat face_photo = image(Rect(it.box.y, it.box.x, it.box.height - it.box.y, it.box.width - it.box.x));
                //resize(face_photo, face_photo, Size(0, 0), 2, 2);
                
                string file_name = string("./img_out_phuongnam/") + to_string(it.id) + "_" + to_string(++id_count[it.id]) + ".png";
                //imshow(file_name, face_photo);
                imwrite(file_name, face_photo, compression_params);
            }

            draw_5_points(image, boxes);

            //cout<<"time is  "<<start/10e3<<endl;
            imshow("result", image);
            video_writer << image;
            if( waitKey(1)>=0 ) break;
            //start = clock() -start;
             
            cerr << num_frame << ' ' << t1.count()/1e6 << ' ' << t2.count()/1e6 << " (ms) " << endl;
            if (num_frame < 100) {
                num_frame += 1;
                total_time += double(t1.count());
                total_time += double(t2.count());
            } else {
                printf("Time=%.2f, Frame=%d, FPS=%.2f\n", total_time / 1e9, num_frame, num_frame * 1e9 / total_time);
                num_frame = 0;
                total_time = 0;
            }
            /*
            if (argc > 1) {
                int key = waitKey(30);
                //cerr << "key= " << key << endl;
                if (key != -1) {
                    cerr << "User Input: " << key << endl;
                    if (key == 81) { // left arrow
                        if (last_frame_shifted + 10 < cap.get(CAP_PROP_POS_FRAMES) || shift_width >= 0) {
                            shift_width = -200;
                        } else {
                            shift_width = shift_width * 1.5;
                        }
                        cap.set(CAP_PROP_POS_FRAMES, cap.get(CAP_PROP_POS_FRAMES) + shift_width);
                        last_frame_shifted = cap.get(CAP_PROP_POS_FRAMES);
                    } else if (key == 83) { // right arrow
                        if (last_frame_shifted + 10 < cap.get(CAP_PROP_POS_FRAMES) || shift_width <= 0) {
                            shift_width = +200;
                        } else {
                            shift_width = shift_width * 1.5;
                        }
                        cap.set(CAP_PROP_POS_FRAMES, cap.get(CAP_PROP_POS_FRAMES) + shift_width);
                        last_frame_shifted = cap.get(CAP_PROP_POS_FRAMES);
                    } else if (key == 13) { // ENTER
                        break;
                    }
                }
            }
            */
            
        } catch (cv::Exception e) {
            cerr << "Warning: an exception occured!" << endl;
            continue;
        }
    }

    image.release();
    video_writer.release();

    return 0;
}

int image_main(int argc, char **argv) {
    mtcnn find(480, 640);
    vector< string > a[2];

    for (const auto &entry : experimental::filesystem::directory_iterator("/home/lad/Downloads/face_pose_dataset/Subject01_10/Subject01")) {
        string path = entry.path();
        cout << path << endl;
        if (path.substr(path.size() - 3) != "Jpg") continue;
        Mat image;
        image = imread(path);
        vector< BoundingBox > boxes = find.findFace(image);
        if (path == "/home/lad/Downloads/face_pose_dataset/Subject01_10/Subject01/A_01_-75.Jpg") {
            draw_5_points(image, boxes);
            imshow("result", image);
            waitKey(0);
        }
        a[boxes.size() > 0 && boxes[0].is_frontal()].push_back(path);
    }

    for (int i = 0; i <= 1; ++i) {
        cout << "is_frontal=" << i << " list: " << endl;
        for (auto it : a[i]) {
            cout << it << endl;
        }
        cout << endl;
    }
    return 0;   
}

int main(int argc, char **argv)
{
    //Mat image = imread("4.jpg");
    //mtcnn find(image.rows, image.cols);
    /*
    clock_t start;
    start = clock();
    find.findFace(image);
    imshow("result", image);
    imwrite("result.jpg",image);
    start = clock() -start;
    cout<<"time is  "<<start/10e3<<endl;*/
    

    //return Runner::main();
    //return image_main(argc, argv);
    return old_main(argc, argv);
    
    return 0;
}