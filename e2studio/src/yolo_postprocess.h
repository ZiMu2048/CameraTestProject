/*
 * yolo_postprocess.h
 *
 *  Created on: 2026年7月11日
 *      Author: lingk
 */

#ifndef YOLO_POSTPROCESS_H_
#define YOLO_POSTPROCESS_H_

#include <stdint.h>

#define YOLO_INPUT_SIZE             (128)
#define YOLO_CLASS_COUNT            (1)
#define YOLO_OUTPUT_BOX_COUNT       (336)
#define YOLO_OUTPUT_ATTRS           (5)
#define YOLO_MAX_DETECTIONS         (64)

typedef struct st_yolo_detection
{
    float x1;            /* 模型坐标，左上角 x，范围 0~352 */
    float y1;            /* 模型坐标，左上角 y，范围 0~352 */
    float x2;            /* 模型坐标，右下角 x，范围 0~352 */
    float y2;            /* 模型坐标，右下角 y，范围 0~352 */
    float score;         /* objectness × 类别概率，范围 0~1 */
    uint8_t class_id;    /* 0~3，对应训练 YAML 的 names 顺序 */
} yolo_detection_t;

extern const char * const g_yolo_class_names[YOLO_CLASS_COUNT];

/**
 * 解码一个 YOLO 输出头。
 *
 * output 布局必须是 NCHW：[1, 27, grid_size, grid_size]。
 * anchors 单位必须是 352x352 模型输入坐标。
 *
 * 返回写入 detections 后的总数量。
 */
int yolo_decode_int8_output(const int8_t * output,
                            yolo_detection_t * detections,
                            int capacity,
                            float confidence_threshold);

/**
 * 按类别执行非极大值抑制。
 *
 * 输入 detections 会原地排序并压缩，返回 NMS 后的数量。
 */
int yolo_nms(yolo_detection_t * detections,
             int count,
             float iou_threshold);

#endif /* YOLO_POSTPROCESS_H_ */
