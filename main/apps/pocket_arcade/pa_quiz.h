#pragma once

/* 一道常识题:题干最多两行,三个选项里第一个永远是正确答案,
   显示时由游戏把顺序打乱,所以题库里不需要再记答案下标。 */
typedef struct {
    const char *ask1;
    const char *ask2;
    const char *choice[3];
} pa_quiz_item_t;

extern const pa_quiz_item_t PA_QUIZ[];
extern const unsigned PA_QUIZ_COUNT;
