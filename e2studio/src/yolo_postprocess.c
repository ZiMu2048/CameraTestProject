/*
 * yolo_postprocess.c
 *
 *  Created on: 2026年7月11日
 *      Author: lingk
 */
#include "yolo_postprocess.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

const char * const g_yolo_class_names[YOLO_CLASS_COUNT] =
{
    "BirdDrop",
    "Cracked",
    "Dusty",
    "Panel",
};

static float yolo_clampf(float value, float lower, float upper)
{
    if (value < lower)
    {
        return lower;
    }

    if (value > upper)
    {
        return upper;
    }

    return value;
}

static float yolo_sigmoid(float value)
{
    /* 防止 expf() 因异常输出溢出。 */
    value = yolo_clampf(value, -16.0f, 16.0f);
    return 1.0f / (1.0f + expf(-value));
}

static float yolo_safe_exp(float value)
{
    value = yolo_clampf(value, -10.0f, 10.0f);
    return expf(value);
}

static float yolo_iou(const yolo_detection_t *a,
                      const yolo_detection_t *b)
{
    float x1 = (a->x1 > b->x1) ? a->x1 : b->x1;
    float y1 = (a->y1 > b->y1) ? a->y1 : b->y1;
    float x2 = (a->x2 < b->x2) ? a->x2 : b->x2;
    float y2 = (a->y2 < b->y2) ? a->y2 : b->y2;

    float inter_w = x2 - x1;
    float inter_h = y2 - y1;

    if ((inter_w <= 0.0f) || (inter_h <= 0.0f))
    {
        return 0.0f;
    }

    float intersection = inter_w * inter_h;
    float area_a = (a->x2 - a->x1) * (a->y2 - a->y1);
    float area_b = (b->x2 - b->x1) * (b->y2 - b->y1);
    float union_area = area_a + area_b - intersection;

    if (union_area <= 0.0f)
    {
        return 0.0f;
    }

    return intersection / union_area;
}

static void yolo_add_candidate(yolo_detection_t *detections,
                               int capacity,
                               int *p_count,
                               const yolo_detection_t *candidate)
{
    if (*p_count < capacity)
    {
        detections[*p_count] = *candidate;
        (*p_count)++;
        return;
    }

    /* 满了以后替换最低分候选，保留全局分数较高的框。 */
    int lowest_index = 0;

    for (int i = 1; i < capacity; i++)
    {
        if (detections[i].score < detections[lowest_index].score)
        {
            lowest_index = i;
        }
    }

    if (candidate->score > detections[lowest_index].score)
    {
        detections[lowest_index] = *candidate;
    }
}

int yolo_decode_head(const float *output,
                     int grid_size,
                     const float anchors[YOLO_ANCHOR_COUNT][2],
                     float confidence_threshold,
                     yolo_detection_t *detections,
                     int capacity,
                     int count)
{
    if ((output == NULL) || (anchors == NULL) || (detections == NULL) ||
        (grid_size <= 0) || (capacity <= 0) || (count < 0))
    {
        return 0;
    }

    if (count > capacity)
    {
        count = capacity;
    }

    const int plane = grid_size * grid_size;
    const float stride = (float) YOLO_INPUT_SIZE / (float) grid_size;

    for (int anchor_id = 0; anchor_id < YOLO_ANCHOR_COUNT; anchor_id++)
    {
        for (int grid_y = 0; grid_y < grid_size; grid_y++)
        {
            for (int grid_x = 0; grid_x < grid_size; grid_x++)
            {
                const int cell = grid_y * grid_size + grid_x;
                const int base_channel = anchor_id * YOLO_ATTRS_PER_ANCHOR;

                /* output 是 [1, 27, H, W]，所以每个 channel 跨一个 H*W 平面。 */
                const float tx = output[(base_channel + 0) * plane + cell];
                const float ty = output[(base_channel + 1) * plane + cell];
                const float tw = output[(base_channel + 2) * plane + cell];
                const float th = output[(base_channel + 3) * plane + cell];

                const float objectness = yolo_sigmoid(
                    output[(base_channel + 4) * plane + cell]);

                int best_class = 0;
                float best_class_probability = 0.0f;

                for (int class_id = 0; class_id < YOLO_CLASS_COUNT; class_id++)
                {
                    float class_probability = yolo_sigmoid(
                        output[(base_channel + 5 + class_id) * plane + cell]);

                    if (class_probability > best_class_probability)
                    {
                        best_class_probability = class_probability;
                        best_class = class_id;
                    }
                }

                const float confidence = objectness * best_class_probability;

                if (confidence < confidence_threshold)
                {
                    continue;
                }

                /* YOLO 原始 head：中心位置为 grid 偏移，宽高相对 anchor 解码。 */
                const float center_x =
                    (yolo_sigmoid(tx) + (float) grid_x) * stride;
                const float center_y =
                    (yolo_sigmoid(ty) + (float) grid_y) * stride;
                const float width = yolo_safe_exp(tw) * anchors[anchor_id][0];
                const float height = yolo_safe_exp(th) * anchors[anchor_id][1];

                yolo_detection_t candidate;

                candidate.x1 = yolo_clampf(center_x - width * 0.5f,
                                           0.0f, (float) YOLO_INPUT_SIZE);
                candidate.y1 = yolo_clampf(center_y - height * 0.5f,
                                           0.0f, (float) YOLO_INPUT_SIZE);
                candidate.x2 = yolo_clampf(center_x + width * 0.5f,
                                           0.0f, (float) YOLO_INPUT_SIZE);
                candidate.y2 = yolo_clampf(center_y + height * 0.5f,
                                           0.0f, (float) YOLO_INPUT_SIZE);
                candidate.score = confidence;
                candidate.class_id = (uint8_t) best_class;

                if ((candidate.x2 <= candidate.x1) ||
                    (candidate.y2 <= candidate.y1))
                {
                    continue;
                }

                yolo_add_candidate(detections, capacity, &count, &candidate);
            }
        }
    }

    return count;
}

int yolo_nms(yolo_detection_t *detections,
             int count,
             float iou_threshold)
{
    if ((detections == NULL) || (count <= 0))
    {
        return 0;
    }

    iou_threshold = yolo_clampf(iou_threshold, 0.0f, 1.0f);

    /* 选择排序：把高分框放在前面。检测框最多 64 个，复杂度足够低。 */
    for (int i = 0; i < count - 1; i++)
    {
        int best = i;

        for (int j = i + 1; j < count; j++)
        {
            if (detections[j].score > detections[best].score)
            {
                best = j;
            }
        }

        if (best != i)
        {
            yolo_detection_t temp = detections[i];
            detections[i] = detections[best];
            detections[best] = temp;
        }
    }

    /* 只抑制同一类别中 IoU 过高的低分框。 */
    int kept_count = 0;

    for (int i = 0; i < count; i++)
    {
        bool suppressed = false;

        for (int j = 0; j < kept_count; j++)
        {
            if ((detections[i].class_id == detections[j].class_id) &&
                (yolo_iou(&detections[i], &detections[j]) > iou_threshold))
            {
                suppressed = true;
                break;
            }
        }

        if (!suppressed)
        {
            detections[kept_count] = detections[i];
            kept_count++;
        }
    }

    return kept_count;
}
