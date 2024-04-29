#include "headers.hpp"
#include "consts.hpp"
#include "blocks.hpp"
#include "classes.hpp"
#include "bit_utils.hpp"
#include "msg_utils.hpp"
// #include <vector>
using namespace std;

// TODO: 用 fetch_line() 和 back_line() 函数初始化而不是手动;

// 将下面几个设置为工程全局变量;
std::vector<MSG> bus;
DEV devs[MAX_DEVS];
TU tus[MAX_DEVS];
LLC llc;

MSG Req_001, Req_002, Req_003, Req_004, Req_005;

void init()
{
    for (int i = 0; i < MAX_DEVS; i++)
    {
        devs[i].dev_id = i;
        tus[i].tu_id = i;
    }
}

void init_a()
{
    // ------------- Fig 1(a) --------------
    // Handling word granularity ReqO and ReqWT. (Avoiding false sharing)
    // addr: 0x66CCFF = 0b 0110 0110 11/00 1100 1111 /11/11

    // Data init;
    line_t data_CF = {0x00, 0x66, 0xCC, 0xFF, 0x01, 0x66, 0xCC, 0xFF, 0x10, 0x66, 0xCC, 0xFF, 0x11, 0x66, 0xCC, 0xFF};
    LineCopy(llc.cache[0xCF], data_CF);
    llc.line_state_buf[0xCF] = LLC_V;
    llc.tag_buf[0xCF] = 0x19B; // 0b 0110_0110_11 -> 01_1001_1011

    // Req init;
    // ReqO;
    Req_001.id = 1;
    Req_001.mask = 0xA; // 0b1010
    // no need for a DEV req to determine dest; always LLC; determined in tu.req_mapping();
    Req_001.addr = 0x66CCFF;
    Req_001.msg = WRITE;
    // gran/ok_mask also determined in tu.req_mapping();
    // u_state/retry_times determined in ?;
    // no data_line/data_word needed;
    // Req_001.msg_display();

    // ReqWT;
    Req_002.id = 2;
    Req_002.mask = 0x5; // 0b0101
    Req_002.addr = 0x66CCFF;
    Req_002.msg = WRITE;
    // Req_002.msg_display();
    // ------------------------------------
}

void do_a()
{
    init_a();
    int time = 0;
    for (time = 1; time < 3; time++)
    {
        cout << "Timing: " << time << endl;
        if (time == 1)
        {
            devs[ACC].req_buf.push_back(Req_001);
            tus[ACC].mapping_wrapper(devs[ACC]);
        }
        else if (time == 2)
        {
            devs[GPU].req_buf.push_back(Req_002);
            tus[GPU].mapping_wrapper(devs[GPU]);
        }
        get_msg(); // 上升沿get_msg();
        // llc.req_buf.front().msg_display();
        // llc.rcv_req(bitset<MAX_DEVS_BITS>(ACC), mapped);
        llc.rcv_req();
        get_msg(); // 下降沿get_msg();
    }
}

void init_b()
{
    // ------------- Fig 1(b) --------------
    // Handling word granularity ReqWT+data for remotely owned data.
    // addr: 0x39C5BB = 0b 0011 1001 11/00 0101 1011 /10/11

    // Data init;
    // LLC;
    line_t data_5B = {ACC, 0x00, 0x00, 0x00, ACC, 0x00, 0x00, 0x00, ACC, 0x00, 0x00, 0x00, ACC, 0x00, 0x00, 0x00};
    // Storing owners' id.
    LineCopy(llc.cache[0x5B], data_5B);
    llc.line_state_buf[0x5B] = LLC_V;
    llc.word_state_buf[0x5B].set(); // set all to O state;
    llc.tag_buf[0x5B] = 0xE7;       // 0b 0011_1001_11 -> 00_1110_0111
    // ACC's cache;
    line_t data_ACC = {0x00, 0x39, 0xC5, 0xBB, 0x01, 0x39, 0xC5, 0xBB, 0x10, 0x39, 0xC5, 0xBB, 0x11, 0x39, 0xC5, 0xBB};
    LineCopy(devs[ACC].cache[0x1B], data_ACC);
    devs[ACC].state_buf[0x1B] = DEV_O;
    devs[ACC].tag_buf[0x1B] = 0xE71; // 0b 0011_1001_1100_01 -> 00_1110_0111_0001

    // Req init;
    // ReqWTdata;
    Req_003.id = 3;
    Req_003.mask = 0x5; // 0b0101
    // no need for a DEV req to determine dest; always LLC; determined in tu.req_mapping();
    Req_003.addr = 0x39C5BB;
    Req_003.msg = RMW;
    // gran/ok_mask also determined in tu.req_mapping();
    // u_state/retry_times determined in ?;
    // no data_line/data_word needed;
    // Req_003.msg_display();
    // ------------------------------------
}

void do_b()
{
    init_b();
    int time = 0;
    for (time = 1; time < 3; time++)
    {
        cout << "Timing: " << time << endl;
        if (time == 1)
        {
            devs[GPU].req_buf.push_back(Req_003);
            tus[GPU].mapping_wrapper(devs[GPU]);
        }
        get_msg(); // 上升沿get_msg();
        llc.rcv_req();
        cout << "------ LLC TEST -------" << endl;
        int len = llc.req_buf.size();
        for (int i = 0; i < len; i++)
        {
            llc.req_buf[i].msg_display();
        }
        get_msg(); // 下降沿get_msg();
        tus[ACC].rcv_fwd();
    }
}

// Must
void init_c()
{
    // ------------- Fig 1(c) --------------
    // Handling line granularity ReqV.
    // Using 0x66CCFF in init_a();

    // Req init;
    // ReqV;
    Req_001.id = 4;
    Req_001.addr = 0x66CCFF;
    Req_001.msg = READ;
    // Req_001.msg_display();
    // ------------------------------------
}

void do_c()
{
    do_a();
    init_c();
    int time = 0;
    for (time = 1; time < 3; time++)
    {
        cout << "Timing: " << time << endl;
        if (time == 1)
        {
            devs[GPU].req_buf.push_back(Req_001);
            tus[GPU].mapping_wrapper(devs[GPU]);
        }
        get_msg(); // 上升沿get_msg();
        llc.rcv_req();
        tus[ACC].rcv_fwd();
        get_msg(); // 下降沿get_msg();
    }
}

void init_d()
{
    // ------------- Fig 1(d) --------------
    // Handling word granularity ReqWT with line granularity owner.
    // addr: 0xDEADBEEF = 0b 1101 1110 1010 1101 10/11 1110 1110 /11/11

    // Data init;
    line_t data_3EE = {CPU, 0x00, 0x00, 0x00, CPU, 0x00, 0x00, 0x00, CPU, 0x00, 0x00, 0x00, CPU, 0x00, 0x00, 0x00};
    LineCopy(llc.cache[0x3EE], data_3EE);
    llc.line_state_buf[0x3EE] = LLC_V;
    llc.word_state_buf[0x3EE].set(); // set all to O state;
    llc.tag_buf[0x3EE] = 0x37AB6;    // 0b 1101 1110 1010 1101 10 -> 11 0111 1010 1011 0110

    // Req init;
    // ReqWTdata;
    Req_001.id = 5;
    Req_001.mask = 0x5; // 0b0101
    // no need for a DEV req to determine dest; always LLC; determined in tu.req_mapping();
    Req_001.addr = 0xDEADBEEF;
    Req_001.msg = WRITE;
    // Req_001.msg_display();
    // ------------------------------------
}

void do_d()
{
    init_d();
    int time = 0;
    for (time = 1; time < 5; time++)
    {
        cout << "Timing: " << time << endl;
        if (time == 1)
        {
            devs[GPU].req_buf.push_back(Req_001);
            tus[GPU].mapping_wrapper(devs[GPU]);
        }
        get_msg(); // 上升沿get_msg();
        llc.rcv_req();
        tus[CPU].rcv_fwd();
        get_msg(); // 下降沿get_msg();
    }
}

int main()
{
    // reset;
    init();

    // do_a();
    do_b();
    // do_c();
    // do_d();
    return 0;
}