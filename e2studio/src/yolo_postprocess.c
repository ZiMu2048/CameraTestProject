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
    "Dusty",
    "PhysicalDamage",
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

#if 0 /* Raw-head helpers are not used by the current decoded [1344,6] output. */
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

#endif

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

int yolo_decode_int8_output(const int8_t * output,
                            yolo_detection_t * detections,
                            int capacity,
                            float confidence_threshold)
{
    const float output_scale = 0.0058933664f;
    const int output_zero_point = -93;
    int count = 0;

    if ((output == NULL) || (detections == NULL) || (capacity <= 0))
    {
        return 0;
    }

    for (int i = 0; i < YOLO_OUTPUT_BOX_COUNT; i++)
    {
        const int8_t * row = &output[i * YOLO_OUTPUT_ATTRS];

        float x1 = (float) ((int) row[0] - output_zero_point) * output_scale;
        float y1 = (float) ((int) row[1] - output_zero_point) * output_scale;
        float x2 = (float) ((int) row[2] - output_zero_point) * output_scale;
        float y2 = (float) ((int) row[3] - output_zero_point) * output_scale;
        float dusty = (float) ((int) row[4] - output_zero_point) * output_scale;
        float damage = (float) ((int) row[5] - output_zero_point) * output_scale;

        int class_id = (damage > dusty) ? 1 : 0;
        float score = (damage > dusty) ? damage : dusty;

        if (score < confidence_threshold)
        {
            continue;
        }

        yolo_detection_t candidate =
        {
            .x1 = x1 * YOLO_INPUT_SIZE,
            .y1 = y1 * YOLO_INPUT_SIZE,
            .x2 = x2 * YOLO_INPUT_SIZE,
            .y2 = y2 * YOLO_INPUT_SIZE,
            .score = score,
            .class_id = (uint8_t) class_id,
        };

        candidate.x1 = yolo_clampf(candidate.x1, 0.0f, (float) YOLO_INPUT_SIZE);
        candidate.y1 = yolo_clampf(candidate.y1, 0.0f, (float) YOLO_INPUT_SIZE);
        candidate.x2 = yolo_clampf(candidate.x2, 0.0f, (float) YOLO_INPUT_SIZE);
        candidate.y2 = yolo_clampf(candidate.y2, 0.0f, (float) YOLO_INPUT_SIZE);

        if ((candidate.x2 <= candidate.x1) || (candidate.y2 <= candidate.y1))
        {
            continue;
        }

        /* 复用你现有的候选框限流逻辑即可。 */
        yolo_add_candidate(detections, capacity, &count, &candidate);
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
