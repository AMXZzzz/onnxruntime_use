#include<windows.h>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

using namespace Ort;
using namespace std;
using namespace cv;

struct boxs
{
    cv::Rect_<float> rect;
    int label;
    float prob;
};

struct GridAndStride
{
    int gh;
    int gw;
    int stride;
};
HANDLE hcom;
void generate_grids_and_stride(int target_size, std::vector<int>& strides, std::vector<GridAndStride>& grid_strides)
{
    for (auto stride : strides)
    {
        int num_grid = target_size / stride;
        for (int g1 = 0; g1 < num_grid; g1++)
        {
            for (int g0 = 0; g0 < num_grid; g0++)
            {
                GridAndStride gs;
                gs.gh = g0;
                gs.gw = g1;
                gs.stride = stride;
                grid_strides.push_back(gs);
            }
        }
    }
}

int main()
{
    vector<Value> input_tensors, output_tensors;        //�������봴��
    vector<int> strides = { 8, 16, 32};           //����
    vector<GridAndStride> grid_strides;            //����
    vector<cv::Rect> boxes;                        //����
    vector<int> classIds;                          //���
    vector<float> confidences;                     //���Ŷ�
    vector<int> indices;
    HBITMAP	BitMap, hOld;
    Mat img, img0;
    float conf = 0.4;


    //��ַ
    //string img_path = "C:/Users/Zzzz/Desktop/Z/000102.png";
    const wchar_t* modelFilepath = L"C:/Users/Zzzz/Desktop/Z/yolox.onnx";  //ģ�͵�ַ   

    //����ͼƬ
    //Mat img = imread(img_path);

    // ������ѡ��
    Ort::Env env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "SuperResolution");    //ʵ��������
    Ort::SessionOptions session_options;    //ʵ��������

    //����ͼ�Ż�������߳�
    session_options.SetIntraOpNumThreads(4);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // ����ģ�Ͳ������Ự,�Լ�һЩ��������
    Ort::Session session(env, modelFilepath, session_options);   //����ģ��
    Ort::AllocatorWithDefaultOptions allocator;                 //ʵ�����Ự
    auto memory_info = MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault); //����CPU
    std::vector<const char*> input_names{ session.GetInputName(0, allocator) };     //�﷨��
    std::vector<const char*> output_names{ session.GetOutputName(0, allocator) };
    auto input_dims = session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();   //��ȡ����ά�� [1 3 640 640]
    auto output_dims = session.GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape(); //��ȡ���ά�� [1 3549 7]

    // �������ά��һ���Ŀվ���
    Mat output;
    output.create(Size(output_dims[1], output_dims[2]), CV_32F); 
    const int num_class = output_dims[2] - 5;  //���
    const float img_size = (float)input_dims[2];

    //1.��ȡ��Ļ���
    HWND hwnd = GetDesktopWindow();
    RECT rect;
    GetWindowRect(hwnd, &rect);
    int cx = (rect.right - rect.left) * 0.5;//800
    int cy = (rect.bottom - rect.top) * 0.5; //450

    //Ҫ��ȡ�Ŀ��
    int width = int((640 * 0.5));    //��ȡ�Ŀ� 320
    int  height = int((640 * 0.5));     //320
    //���ݽ�ȡ�Ŀ�߼����ͼԭ��
    int x = int(cx - (width * 0.5)); //��ȡ��ԭ��x   480
    int y = int(cy - (height * 0.5));//��ȡ��ԭ��y   130

    //2.��ȡ��ĻDC
    HDC hdc = GetWindowDC(hwnd);
    //3.��������DC(�ڴ�DC)
    HDC	mfdc = CreateCompatibleDC(hdc);
    //5.����λͼBitmap����
    BitMap = CreateCompatibleBitmap(hdc, width, height);
    //6.��λͼ��������ڴ�dc(Ҳ�����ǰ�)
    SelectObject(mfdc, BitMap);
    //7.����һ���̶�ά�ȵĿվ���,
    img0.create(Size(width, height), CV_8UC4);
    int i = 0;
    while (true)
    {
        i++;


        double t1 = getTickCount();
        //get_img
        BitBlt(mfdc, 0, 0, width, height, hdc, x, y, SRCCOPY);
        //��BitBlt��λͼ��Ϣ����
        GetBitmapBits(BitMap, height * width * 4, img0.data);//λͼ������,�ֽ���,��Ҫ�������ĵط�
        cvtColor(img0, img, COLOR_BGRA2BGR);  // 4->3,img0��ת�����ͼƬ,����������

        //Ԥ����
        float sx = static_cast<float>(img.cols) / img_size; //����
        float sy = static_cast<float>(img.rows) / img_size;
        Mat blob = dnn::blobFromImage(img, 1.0, Size(input_dims[2], input_dims[2]), NULL, true, false);      //Ԥ����

        //���������������
        input_tensors.clear();
        output_tensors.clear();
        input_tensors.emplace_back(Value::CreateTensor<float>(memory_info, blob.ptr<float>(), blob.total(), input_dims.data(), input_dims.size()));
        output_tensors.emplace_back(Value::CreateTensor<float>(memory_info, output.ptr<float>(), output.total(), output_dims.data(), output_dims.size()));

        // ����   
        session.Run(RunOptions{ nullptr }, input_names.data(), input_tensors.data(), input_tensors.size(), output_names.data(), output_tensors.data(), output_tensors.size());

        //��ȡ������ݵ�ָ��
        float* floatarr = output_tensors[0].GetTensorMutableData<float>();

        // ��������������������ê����Ϣ
        grid_strides.clear();
        generate_grids_and_stride(img_size, strides, grid_strides);

        // �������
        boxes.clear();
        classIds.clear();
        confidences.clear();

        for (int anchor_idx = 0; anchor_idx < grid_strides.size(); anchor_idx++)
        {
            const int grid0 = grid_strides[anchor_idx].gh; // H
            const int grid1 = grid_strides[anchor_idx].gw; // W
            const int stride = grid_strides[anchor_idx].stride; // stride
            const int basic_pos = anchor_idx * output_dims[2];       //[,?......] 7

            float x_center = (floatarr[basic_pos + 0] + grid0) * stride * sx;
            float y_center = (floatarr[basic_pos + 1] + grid1) * stride * sy;
            float w = exp(floatarr[basic_pos + 2]) * stride * sx;
            float h = exp(floatarr[basic_pos + 3]) * stride * sy;
            float x0 = x_center - w * 0.5f;     //�����ȳ˷���
            float y0 = y_center - h * 0.5f;
            float box_objectness = floatarr[basic_pos + 4];

            for (int class_idx = 0; class_idx < num_class; class_idx++)
            {
                float box_cls_score = floatarr[basic_pos + 5 + class_idx];
                float box_prob = box_objectness * box_cls_score;

                if (box_prob > conf)
                {
                    cv::Rect rect;
                    rect.x = x0;
                    rect.y = y0;
                    rect.width = w;
                    rect.height = h;

                    classIds.push_back(class_idx);
                    confidences.push_back((float)box_prob);
                    boxes.push_back(rect);
                }
            }
        }


        //����
        indices.clear();
   
        cv::dnn::NMSBoxes(boxes, confidences, 0.1, 0.1, indices);

        for (size_t i = 0; i < indices.size(); ++i)
        {
            int idx = indices[i];
            rectangle(img, boxes[idx], cv::Scalar(0, 255, 0), 2, 8, 0);
        }

        ostringstream ss;
        double time = (getTickCount() - t1) * 1000 / (getTickFrequency());
        ss << "FPS: " << fixed << setprecision(2) << 1000 / time << " ;TIME: " << time << "ms"; //fixed << setprecision(3);���ƺ��������С����λ
        putText(img, ss.str(), cv::Point(20, 40), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(255, 255, 0), 2, 8);
        //��ʾ
        //resize(img, img, Size(960, 540));
        imshow("test", img);
        waitKey(1);
    }
    //�ͷ�
    DeleteDC(hdc);
    DeleteDC(mfdc);
    DeleteObject(BitMap);
    //system("pause");
    return 0;
}

