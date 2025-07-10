#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <drv_matrix_led.h>
#include <string.h>
#include <ctype.h>

/* 引脚定义（使用标准GET_PIN格式） */
#define PIN_SET              GET_PIN(F, 5)
#define PIN_RESET            GET_PIN(F, 6)
#define PIN_MOD0             GET_PIN(B, 11)
#define PIN_MOD1             GET_PIN(B, 10)
#define PIN_MOD2             GET_PIN(D, 11)
#define PIN_PIN0             GET_PIN(E, 3)
#define PIN_PIN1             GET_PIN(E, 5)
#define PIN_PIN2             GET_PIN(E, 2)

/* LED矩阵编号定义 */
enum{
    EXTERN_LED_0, // 盲文点位1
    EXTERN_LED_1, // 盲文点位2
    EXTERN_LED_2, // 盲文点位3
    EXTERN_LED_3, // 盲文点位4
    EXTERN_LED_4, // 盲文点位5
    EXTERN_LED_5, // 盲文点位6
    EXTERN_LED_6, // 保留
    EXTERN_LED_7, // 字符编号显示LED（最高位）
    EXTERN_LED_8, // 字符编号显示LED（中间位）
    EXTERN_LED_9, // 字符编号显示LED（最低位）
    EXTERN_LED_10,
    EXTERN_LED_11,
    EXTERN_LED_12,
    EXTERN_LED_13,
    EXTERN_LED_14,
    EXTERN_LED_15,
    EXTERN_LED_16,
    EXTERN_LED_17,
    EXTERN_LED_18,
};

/* 使用RT-Thread的bool类型定义 */
typedef rt_bool_t bool;
#define true  RT_TRUE
#define false RT_FALSE

/* 盲文点符与LED矩阵映射 */
static const rt_uint8_t braille_led_map[6] = {
    EXTERN_LED_0, EXTERN_LED_1, EXTERN_LED_2,
    EXTERN_LED_3, EXTERN_LED_4, EXTERN_LED_5
};

/* 字符编号与LED矩阵映射 */
static const rt_uint8_t char_id_led_map[3] = {
    EXTERN_LED_7, EXTERN_LED_8, EXTERN_LED_9
};

/* 英文字母、数字、符号盲文编码映射表（仅小写字母） */
struct braille_matrix_map {
    char c;         // 仅存储小写字母
    char code[7];   // 6位二进制编码
};

struct braille_matrix_map braille_maps[] = {
    // 字母（仅小写，大小写统一使用此编码）
    {'a', "100000"}, {'b', "110000"}, {'c', "100100"}, {'d', "100110"}, {'e', "100010"},
    {'f', "110100"}, {'g', "110110"}, {'h', "110010"}, {'i', "010100"}, {'j', "010110"},
    {'k', "101000"}, {'l', "111000"}, {'m', "101100"}, {'n', "101110"}, {'o', "101010"},
    {'p', "111100"}, {'q', "111110"}, {'r', "111010"}, {'s', "011100"}, {'t', "011110"},
    {'u', "101001"}, {'v', "111001"}, {'w', "010111"}, {'x', "101101"}, {'y', "101111"}, {'z', "101011"},

    // 数字（需要数字前缀"3456"）
    {'0', "010110"}, {'1', "100000"}, {'2', "110000"}, {'3', "100100"}, {'4', "100110"},
    {'5', "100010"}, {'6', "110100"}, {'7', "110110"}, {'8', "110010"}, {'9', "010100"},

    // 常用符号
    {'.', "011000"}, {',', "010000"}, {';', "011000"}, {':', "010010"}, {'!', "011010"},
    {'?', "011011"}, {'\'', "001000"}, {'"', "001010"}, {'-', "000110"}, {'(', "011110"},
    {')', "011110"}, {'/', "011101"}, {'*', "001110"}, {'+', "001101"}, {'=', "001111"},
    {'$', "111111"}, {'%', "001100"}, {'&', "111011"}, {'@', "110011"},

    // 空格
    {' ', "000000"},
    // 数字前缀符号（内部使用）
    {'#', "001111"}
};

/* 引脚控制函数 */
static void set_pin_level(rt_base_t pin, rt_base_t level)
{
    rt_pin_write(pin, level);
}

/* 清除LED矩阵显示 */
void clear_led_matrix(void)
{
    for (int i = EXTERN_LED_0; i <= EXTERN_LED_18; i++)
    {
        led_matrix_set_color(i, DARK);
    }
    led_matrix_reflash();
}

/* 组合遍历MOD和PIN，拉高RESET，拉低SET */
static void traverse_mod_pin(void)
{
    // 拉低SET，拉高RESET
    set_pin_level(PIN_SET, PIN_LOW);
    set_pin_level(PIN_RESET, PIN_HIGH);

    // 遍历MOD和PIN组合
    rt_kprintf("遍历MOD和PIN组合...\n");
    for (int mod = 0; mod < 8; mod++) {
        // 设置MOD引脚电平
        set_pin_level(PIN_MOD0, (mod & 0x01) ? PIN_HIGH : PIN_LOW);
        set_pin_level(PIN_MOD1, (mod & 0x02) ? PIN_HIGH : PIN_LOW);
        set_pin_level(PIN_MOD2, (mod & 0x04) ? PIN_HIGH : PIN_LOW);

        for (int pin = 0; pin < 8; pin++) {
            // 设置PIN引脚电平
            set_pin_level(PIN_PIN0, (pin & 0x01) ? PIN_HIGH : PIN_LOW);
            set_pin_level(PIN_PIN1, (pin & 0x02) ? PIN_HIGH : PIN_LOW);
            set_pin_level(PIN_PIN2, (pin & 0x04) ? PIN_HIGH : PIN_LOW);
            rt_thread_mdelay(1);  // 短暂延时
        }
    }
}

/* 设置位置编码对应的MOD引脚电平 */
static void set_position_code(int position_code)
{
    // 拉高SET，拉低RESET
    set_pin_level(PIN_SET, PIN_HIGH);
    set_pin_level(PIN_RESET, PIN_LOW);

    // 设置MOD引脚电平
    rt_kprintf("设置位置编码: %d (MOD电平: %d%d%d)\n",
              position_code,
              (position_code & 0x04) ? 1 : 0,
              (position_code & 0x02) ? 1 : 0,
              (position_code & 0x01) ? 1 : 0);

    set_pin_level(PIN_MOD0, (position_code & 0x01) ? PIN_HIGH : PIN_LOW);
    set_pin_level(PIN_MOD1, (position_code & 0x02) ? PIN_HIGH : PIN_LOW);
    set_pin_level(PIN_MOD2, (position_code & 0x04) ? PIN_HIGH : PIN_LOW);
}

/* 发送字符编码对应的PIN脉冲 */
static void send_character_code(const char* code)
{
    if (!code || strlen(code) != 6) return;

    // 字符编码位置到三位二进制编码的映射表（修正版）
    const char* pin_code_map[6] = {
        "001",  // 位置0 (二进制000 → 编码001，确保至少有一个PIN被拉高)
        "010",  // 位置1 (二进制001)
        "011",  // 位置2 (二进制010)
        "100",  // 位置3 (二进制011)
        "101",  // 位置4 (二进制100)
        "110"   // 位置5 (二进制101)
    };

    rt_kprintf("发送字符编码: %s\n", code);

    // 遍历字符编码中的每一位
    for (int i = 0; i < 6; i++) {
        if (code[i] == '1') {
            const char* pin_code = pin_code_map[i];

            rt_kprintf("  位置%d → 编码: %s\n", i, pin_code);

            // 按位调整PIN电平（注意：PIN2对应编码第0位，PIN1对应第1位，PIN0对应第2位）
            for (int j = 0; j < 3; j++) {
                rt_base_t level = (pin_code[j] == '1') ? PIN_HIGH : PIN_LOW;

                switch(j) {
                    case 0:  // 调整PIN2
                        rt_kprintf("    调整PIN2为: %d\n", level);
                        set_pin_level(PIN_PIN2, level);
                        break;
                    case 1:  // 调整PIN1
                        rt_kprintf("    调整PIN1为: %d\n", level);
                        set_pin_level(PIN_PIN1, level);
                        break;
                    case 2:  // 调整PIN0
                        rt_kprintf("    调整PIN0为: %d\n", level);
                        set_pin_level(PIN_PIN0, level);
                        break;
                }
                rt_thread_mdelay(5);  // 脉冲宽度
            }

            // 重置所有PIN引脚
            set_pin_level(PIN_PIN0, PIN_LOW);
            set_pin_level(PIN_PIN1, PIN_LOW);
            set_pin_level(PIN_PIN2, PIN_LOW);
            rt_thread_mdelay(1);  // 脉冲间隔
        }
    }
}

/* 同时显示盲文编码和字符编号 */
void show_braille_and_id(const char* code, int char_id)
{
    clear_led_matrix();

    // 显示盲文点位 (LED 0-6)
    if (strlen(code) == 6) {
        for (int i = 0; i < 6; i++)
        {
            if (code[i] == '1')
            {
                led_matrix_set_color(braille_led_map[i], RED);
            }
        }
    }

    // 显示3位二进制编号 (LED 7-9)
    for (int i = 0; i < 3; i++)
    {
        if (char_id & (1 << (2 - i)))
        {
            led_matrix_set_color(char_id_led_map[i], GREEN);
        }
    }

    led_matrix_reflash();
}

/* 查找字符对应的盲文编码（统一转换为小写） */
struct braille_matrix_map* find_braille_map(char c)
{
    // 将大写字母转换为小写，实现大小写统一编码
    char lower_c = tolower(c);

    for (int i = 0; i < sizeof(braille_maps)/sizeof(braille_maps[0]); i++)
    {
        if (braille_maps[i].c == lower_c)
        {
            return &braille_maps[i];
        }
    }
    return RT_NULL;
}

/* 输出盲文点位位置说明 */
void print_braille_position(const char* code)
{
    rt_kprintf("  点位位置: ");
    bool has_position = false;

    if (code[0] == '1') { rt_kprintf("1 "); has_position = true; }
    if (code[1] == '1') { rt_kprintf("2 "); has_position = true; }
    if (code[2] == '1') { rt_kprintf("3 "); has_position = true; }
    if (code[3] == '1') { rt_kprintf("4 "); has_position = true; }
    if (code[4] == '1') { rt_kprintf("5 "); has_position = true; }
    if (code[5] == '1') { rt_kprintf("6 "); has_position = true; }

    if (!has_position) {
        rt_kprintf("无（空格）");
    }
    rt_kprintf("\n");
}

/* 显示数字前缀，占用一个位置编号 */
void show_number_prefix(int char_id)
{
    struct braille_matrix_map* map = find_braille_map('#');
    if (!map) return;

    rt_kprintf("字符编号: %d (二进制: %c%c%c)\n",
              char_id,
              (char_id & 4) ? '1' : '0',
              (char_id & 2) ? '1' : '0',
              (char_id & 1) ? '1' : '0');
    rt_kprintf("数字前缀: %s\n", map->code);
    print_braille_position(map->code);

    // 设置位置编码
    set_position_code(char_id);

    // 发送字符编码
    send_character_code(map->code);

    // 显示LED矩阵（可选，仅用于调试）
    show_braille_and_id(map->code, char_id);
    rt_thread_mdelay(1500);  // 前缀显示1.5秒
}

/* 显示文本内容的通用函数 */
void display_text(const char* text)
{
    if (!text || !*text) {
        rt_kprintf("错误: 文本为空\n");
        return;
    }

    rt_kprintf("正在显示盲文: %s\n", text);

    // 接收MSH指令后，先组合遍历MOD和PIN
    traverse_mod_pin();

    int display_index = 0;  // 总显示位置索引（包括前缀）
    bool is_prev_digit = false;

    // 记录上一个位置编码，用于检测回到000的情况
    int prev_position_code = -1;

    // 遍历所有字符
    for (int i = 0; i < strlen(text); i++)
    {
        char c = text[i];
        rt_kprintf("原始字符: '%c' ", c);

        // 处理数字前缀
        if (isdigit(c)) {
            if (!is_prev_digit) {
                // 计算位置编码（0-7）
                int position_code = display_index % 8;

                // 当位置编码回到000时，重新遍历MOD和PIN
                if (prev_position_code != -1 && position_code == 0) {
                    traverse_mod_pin();
                }
                prev_position_code = position_code;

                // 显示数字前缀
                show_number_prefix(position_code);

                display_index++;
                rt_thread_mdelay(200);  // 间隔0.2秒
            }
            is_prev_digit = true;
        } else {
            is_prev_digit = false;
        }

        // 当前字符的编号
        int position_code = display_index % 8;

        // 当位置编码回到000时，重新遍历MOD和PIN
        if (prev_position_code != -1 && position_code == 0) {
            traverse_mod_pin();
        }
        prev_position_code = position_code;

        // 显示字符编号信息
        char binary_str[4] = {0};
        for (int j = 0; j < 3; j++)
        {
            binary_str[j] = (position_code & (1 << (2 - j))) ? '1' : '0';
        }
        rt_kprintf("字符编号: %d (二进制: %s)\n", position_code, binary_str);

        // 设置位置编码
        set_position_code(position_code);

        struct braille_matrix_map* map = find_braille_map(c);

        if (map)
        {
            rt_kprintf("字符编码: %s (原始字符为'%c'，使用小写编码)\n", map->code, c);
            print_braille_position(map->code);

            // 发送字符编码
            send_character_code(map->code);

            // 显示LED矩阵（可选，仅用于调试）
            show_braille_and_id(map->code, position_code);
        }
        else
        {
            rt_kprintf("字符 '%c' 无对应盲文编码\n", c);
        }

        rt_thread_mdelay(1500);  // 每个字符显示1.5秒
        display_index++;  // 增加总显示索引
    }
}

/* MSH命令处理函数：从命令行参数显示盲文 */
static void cmd_braille(int argc, char *argv[])
{
    if (argc < 2)
    {
        rt_kprintf("用法: braille [文本]\n");
        rt_kprintf("示例: braille Hello World\n");
        return;
    }

    // 合并所有参数为完整文本
    char text[256] = {0};
    for (int i = 1; i < argc; i++)
    {
        if (i > 1) {
            strncat(text, " ", sizeof(text) - strlen(text) - 1);
        }
        strncat(text, argv[i], sizeof(text) - strlen(text) - 1);
    }

    display_text(text);
}
MSH_CMD_EXPORT_ALIAS(cmd_braille, braille, 显示英文文本的盲文编码（带引脚控制）);

/* 主函数：初始化LED矩阵和引脚 */
int main(void)
{
    // 初始化LED矩阵
    clear_led_matrix();

    // 初始化引脚为输出模式
    rt_pin_mode(PIN_SET, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_RESET, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_MOD0, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_MOD1, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_MOD2, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_PIN0, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_PIN1, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_PIN2, PIN_MODE_OUTPUT);

    // 初始状态：拉低SET，拉高RESET
    set_pin_level(PIN_SET, PIN_LOW);
    set_pin_level(PIN_RESET, PIN_HIGH);

    rt_kprintf("盲文LED矩阵控制器启动（带引脚控制）\n");
    rt_kprintf("支持命令:\n");
    rt_kprintf("  braille [文本] - 显示命令行输入的文本\n");
    return 0;
}
