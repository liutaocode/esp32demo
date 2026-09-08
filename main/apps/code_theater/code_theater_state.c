#include "code_theater_state.h"
#include <stdio.h>
#include <string.h>
static const char *PROJECTS[CT_TASKS] = {
    "个人博客", "数据看板", "天气助手", "记账工具", "相册管理", "待办清单", "文档检索", "课程日历",
    "库存管理", "消息中心", "订单后台", "阅读笔记", "预约系统", "活动页面", "文件管理", "设备面板"
};
static const char *TASKS[CT_TASKS] = {
    "完善登录校验", "修复列表分页", "调整夜间主题", "优化搜索速度", "补齐异常提示", "整理导航菜单",
    "修复日期格式", "增加导出功能", "优化图片加载", "补充单元测试", "整理重复逻辑", "修复提交按钮",
    "调整表单布局", "处理请求超时", "增加本地缓存", "完善空白页面"
};
static const char *OLD[CT_TASKS] = {
    "- 校验 = 跳过", "- 页码 = 0", "- 背景 = 纯白", "- 搜索 = 全量",
    "- 异常 = 忽略", "- 菜单 = 重复", "- 日期 = 原始值", "- 导出 = 未实现",
    "- 图片 = 同步加载", "- 测试 = 3项", "- 逻辑 = 重复调用", "- 按钮 = 始终禁用",
    "- 间距 = 0", "- 超时 = 1秒", "- 缓存 = 关闭", "- 空白页 = 无提示"
};
static const char *NEW[CT_TASKS] = {
    "+ 校验 = 完整检查", "+ 页码 = 1", "+ 背景 = 跟随主题", "+ 搜索 = 使用索引",
    "+ 异常 = 友好提示", "+ 菜单 = 自动去重", "+ 日期 = 统一格式", "+ 导出 = 已实现",
    "+ 图片 = 按需加载", "+ 测试 = 12项", "+ 逻辑 = 合并复用", "+ 按钮 = 校验后启用",
    "+ 间距 = 16", "+ 超时 = 8秒", "+ 缓存 = 自动更新", "+ 空白页 = 引导说明"
};
static const char *BUGS[CT_CARDS] = {
    "  └ 返回值与预期不符", "  └ 列表出现重复条目", "  └ 请求超过等待时间",
    "  └ 缓存没有及时更新", "  └ 缺少边界条件校验", "  └ 空值处理还不完整",
    "  └ 日期偏移了一天", "  └ 页面出现布局偏移", "  └ 按钮状态没有同步",
    "  └ 读取到了旧配置", "  └ 图片加载顺序异常", "  └ 测试依赖未准备好"
};
static const char *CARDS[CT_CARDS] = {
    "耐心侦探", "深夜搭子", "灵感火花", "小小架构师", "边界守护者", "温柔修补匠",
    "时间管理员", "像素整理师", "默契搭档", "配置检查员", "光影搭子", "测试好朋友"
};
static const char *REMARKS[] = {
    "我在，慢慢来", "这段交给我", "快想明白了", "陪你写完它", "我找到线索了", "这里有点意思",
    "好呀，听你的", "收到，换个思路", "不急，我还在", "让我再试一次", "一起歇一会", "今天配合真好",
    "我也被拦住了", "申诉通过啦", "再等一小会", "回来继续陪你", "我在认真写呢", "看我露一手",
    "差一点就好啦", "摸摸头，收到", "这里我来收尾", "这次稳稳的", "等你一句话", "陪你再写一点"
};
static const char *VERBS[CT_PHASES] = {
    "正在思考", "搜索代码", "读取文件", "规划实现", "编写代码", "应用修改", "运行测试",
    "定位异常", "等待确认", "修复问题", "验证结果", "整理提交", "任务完成", "歇一小会",
    "整理上下文", "已被打断", "恢复会话"
};
static const uint16_t DELAYS[CT_PHASES] = {
    2600, 1700, 1900, 2200, 2600, 1800, 2400, 2200, 6000,
    2400, 2200, 1800, 3000, 3200, 2200, 6000, 1600
};
static const ct_pose_t POSES[CT_PHASES] = {
    CT_PET_THINK, CT_PET_READ, CT_PET_READ, CT_PET_THINK, CT_PET_TYPE,
    CT_PET_TYPE, CT_PET_THINK, CT_PET_WORRY, CT_PET_WAIT, CT_PET_TYPE,
    CT_PET_READ, CT_PET_TYPE, CT_PET_HAPPY, CT_PET_SLEEP, CT_PET_THINK,
    CT_PET_WAVE, CT_PET_HAPPY
};
static const uint8_t PHRASES[CT_PHASES] = {0,4,5,2,3,17,18,9,22,8,21,20,11,10,1,6,15};
const char *ct_project(unsigned i) { return PROJECTS[i % CT_TASKS]; }
const char *ct_task(unsigned i) { return TASKS[i % CT_TASKS]; }
const char *ct_card(unsigned i) { return CARDS[i % CT_CARDS]; }
const char *ct_verb(const ct_state_t *s)
{
    if (s->access != CT_ACCESS_OK) return s->access == CT_BANNED ? "访问权限已停止" : "封禁维持";
    return s->paused ? "陪你歇一会" : VERBS[s->phase];
}
const char *ct_speech(const ct_state_t *s)
{
    if (s->access != CT_ACCESS_OK) return REMARKS[s->access == CT_BANNED ? 12 : 14];
    return REMARKS[s->remark_left ? s->remark : PHRASES[s->phase]];
}
ct_pose_t ct_pose(const ct_state_t *s)
{
    return s->access != CT_ACCESS_OK ? CT_PET_SAD : s->paused ? CT_PET_WAIT : POSES[s->phase];
}
static uint32_t random_next(ct_state_t *s)
{
    uint32_t x = s->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x;
}
static void say(ct_state_t *s, unsigned remark)
{
    s->remark = remark; s->remark_left = 2400;
}
static void line(ct_state_t *s, const char *v, uint8_t color)
{
    if (s->count == CT_ROWS) {
        memmove(s->lines, s->lines + 1, sizeof s->lines - sizeof s->lines[0]);
        memmove(s->colors, s->colors + 1, CT_ROWS - 1); s->count--;
    }
    snprintf(s->lines[s->count], CT_LINE_BYTES, "%s", v);
    s->colors[s->count++] = color;
}
static void phase(ct_state_t *s, ct_phase_t p)
{
    s->phase = p; s->elapsed = 0;
    char b[CT_LINE_BYTES];
    unsigned n = random_next(s);
    switch (p) {
    case CT_THINK: line(s, n & 1 ? "• 我先梳理一下思路。" : "• 好的，我来看看。", 0); break;
    case CT_SEARCH:
        line(s, n & 1 ? "• 搜索(相关调用)" : "• 搜索(项目文件)", 0);
        snprintf(b, sizeof b, "  └ 找到 %u 处相关位置", 3 + n % 12); line(s,b,4); break;
    case CT_READ:
        line(s, n & 1 ? "• 读取(页面/入口)" : "• 读取(服务/配置)", 0);
        snprintf(b, sizeof b, "  └ 已读取 %u 行", 24 + n % 180); line(s,b,4); break;
    case CT_PLAN:
        line(s, n & 1 ? "• 先调整逻辑，再验证。" : "• 找到了，分两步处理。", 0); break;
    case CT_EDIT: line(s, n & 1 ? "• 修改(页面/主逻辑)" : "• 修改(服务/处理流程)",0); line(s,OLD[s->task],2); break;
    case CT_DIFF:
        line(s,NEW[s->task],3); snprintf(b,sizeof b,"  └ 新增 %u 行，移除 %u 行",2+n%9,1+n%4); line(s,b,4); break;
    case CT_TEST: line(s,"• 运行(项目测试)",0); line(s,"  └ 检查中，请稍等",4); break;
    case CT_ERROR: line(s,"• 有一处需要再看一下。",1); line(s,BUGS[s->bug],2); break;
    case CT_WAIT: line(s,"• 你想选哪一种修法？",1); break;
    case CT_REPAIR:
        line(s,s->choice == 2 ? "• 修改(重写这段逻辑)" : "• 修改(补齐边界条件)",0);
        line(s,n&1 ? "+ 异常路径已补齐" : "+ 状态检查已补齐",3); break;
    case CT_VERIFY: line(s,"• 运行(再次验证)",0); line(s,"  └ 12 项检查全部通过",3); break;
    case CT_COMMIT: line(s,"• 整理(本次修改)",0); line(s,"  └ 变更摘要已生成",4); break;
    case CT_DONE:
        if (s->completed < 9999) s->completed++;
        s->last_card = s->bug; s->cards |= (uint16_t)(1u << s->last_card);
        line(s,n&1 ? "• 好了，这次一起搞定。" : "• 完成，辛苦你陪我了。",3);
        snprintf(b,sizeof b,"  留下记录：%s",ct_card(s->last_card)); line(s,b,1); break;
    case CT_REST: line(s,n&1 ? "• 歇一下，马上回来。" : "• 我在这里陪着你。",4); break;
    case CT_COMPACT: line(s,"• 整理上下文，马上继续。",4); break;
    case CT_INTERRUPTED: line(s,"> 先停一下",1); line(s,n&1 ? "• 收到，我听你说。" : "• 好，先不往下写。",0); say(s,6+n%2); break;
    case CT_RECOVER: line(s,"• 访问已恢复，继续工作。",3); break;
    default: break;
    }
}
void ct_next(ct_state_t *s)
{
    if (s->access != CT_ACCESS_OK) return;
    s->project = (s->project + 1 + random_next(s) % (CT_TASKS-1)) % CT_TASKS;
    s->task = (s->task + 1 + random_next(s) % (CT_TASKS-1)) % CT_TASKS;
    s->bug = random_next(s) % CT_CARDS;
    s->elapsed = s->count = s->choice = s->work_ms = 0;
    s->paused = false; phase(s,CT_THINK); say(s,random_next(s)%6);
    /* Deliberately retain the physical-click streak across project changes. */
}
void ct_init(ct_state_t *s, uint32_t seed)
{
    memset(s,0,sizeof *s); s->rng = seed ? seed : 1; ct_next(s);
}
static void step(ct_state_t *s)
{
    ct_phase_t p = (ct_phase_t)s->phase;
    switch (p) {
    case CT_TEST: phase(s,random_next(s)%3 ? CT_ERROR : CT_VERIFY); break;
    case CT_PLAN: phase(s,random_next(s)%4 ? CT_EDIT : CT_COMPACT); break;
    case CT_COMPACT: phase(s,CT_EDIT); break;
    case CT_REST: ct_next(s); break;
    case CT_INTERRUPTED: phase(s,random_next(s)&1 ? CT_PLAN : CT_READ); break;
    case CT_RECOVER: phase(s,CT_PLAN); break;
    default: phase(s,(ct_phase_t)(p+1)); break;
    }
}
static void restore(ct_state_t *s, bool appeal)
{
    s->access = CT_ACCESS_OK; s->ban_left = s->burst = 0; s->has_press = false;
    s->paused = false; phase(s,CT_RECOVER); say(s,appeal ? 13 : 15);
    if (appeal) line(s,"• 申诉通过，继续开工。",3);
}
void ct_tick(ct_state_t *s, uint32_t ms)
{
    /* Ban recovery uses real elapsed time, independently of pause and scene pacing. */
    if (s->access != CT_ACCESS_OK) {
        if (ms >= s->ban_left) restore(s,false);
        else s->ban_left -= ms;
        return;
    }
    if (s->paused) return;
    if (ms > 1000) ms = 1000;
    s->remark_left = ms >= s->remark_left ? 0 : s->remark_left-ms;
    s->elapsed += ms;
    if (s->work_ms < 999000) s->work_ms += ms;
    if (s->elapsed >= DELAYS[s->phase]) step(s);
}
void ct_boost(ct_state_t *s)
{
    if (s->paused || s->access != CT_ACCESS_OK) return;
    if (s->boosts < 9999) s->boosts++;
    unsigned n = random_next(s);
    say(s,16+n%8);
    if (s->phase == CT_INTERRUPTED) step(s);
    else if (n%3 == 0 && s->phase != CT_DONE && s->phase != CT_REST)
        line(s,n&1 ? "• 好，我换个思路试试。" : "• 在写啦，陪我一小会。",0);
    else step(s);
}
void ct_interrupt(ct_state_t *s)
{
    if (s->access != CT_ACCESS_OK || s->paused) return;
    if (s->phase == CT_INTERRUPTED) step(s); else phase(s,CT_INTERRUPTED);
}
void ct_choose(ct_state_t *s, bool bold)
{
    if (s->paused || s->access != CT_ACCESS_OK || s->phase != CT_WAIT) return;
    s->choice = bold ? 2 : 1; phase(s,CT_REPAIR); say(s,bold ? 17 : 21);
}
bool ct_press(ct_state_t *s, uint32_t at)
{
    if (s->access != CT_ACCESS_OK) return false;
    s->burst = s->has_press && (uint32_t)(at-s->last_press) <= CT_BURST_GAP_MS ? s->burst+1 : 1;
    s->last_press = at; s->has_press = true;
    if (s->burst < CT_BURST_LIMIT) return false;
    s->access = CT_BANNED; s->ban_left = CT_BAN_MS; s->paused = false;
    return true;
}
bool ct_appeal(ct_state_t *s, uint32_t coin)
{
    if (s->access != CT_BANNED) return false;
    if ((coin & 1u) == 0) { restore(s,true); return true; }
    s->access = CT_APPEAL_FAILED; return false;
}
unsigned ct_card_count(const ct_state_t *s)
{
    unsigned n=0; for (unsigned i=0;i<CT_CARDS;i++) n+=!!(s->cards&(1u<<i)); return n;
}
