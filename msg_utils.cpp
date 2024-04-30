// 在上升沿时执行所有msg的入队操作，在下降沿时执行所有msg的出队操作;
// 如果在下一个上升沿时到来时，req队列仍然不为空，证明此时数据处于阻塞状态，后来的req加入队列等待;

// 1. Generate the message: rsp/req/fwd;
// 2. Write msg to its own fifo;
// 3. Send msg to the client's fifo;
// 4. the receiver generates message in its own fifo.

// It's a fuxking bus-based system, I should send msg to the BUS first instead of point-to-point.

// using namespace std;
#include "msg_utils.hpp"

// 将下面几个设置为工程全局变量;
extern std::vector<MSG> bus;
extern DEV devs[MAX_DEVS];
extern TU tus[MAX_DEVS];
extern LLC llc;

// void MsgInit()
// {
// }

int find_conflict(std::vector<MSG> &req_buf)
{
    int len = req_buf.size() - 1; // 不包括新入队的req自身;
    MSG new_msg = req_buf.back();
    if (!req_buf.empty())
    {
        for (int i = 0; i < len; i++)
        {
            if (new_msg.addr == req_buf[i].addr)
                return i;
        }
    }
    return 0;
}

// bool is_unstable_ok(MSG req)
// {

//     return 0;
// }

void rcv_rsp_single(MSG &rsp_in, unsigned long offset, DATA_LINE &data_line)
{
    DATA_WORD data;
    WordExt(data, data_line, offset);

    switch (rsp_in.msg)
    {
    case RSP_V:
    {
        data.state = SPX_V;
        break;
    }
    case RSP_S:
    {
        data.state = SPX_S;
        break;
    }
    case RSP_WTdata:
    {
        data.state = SPX_I;
        break;
    }
    case RSP_Odata:
    {
        data.state = SPX_O;
        break;
    }
    }

    WordIns(data, data_line, offset);
}

void get_rsp(MSG rsp, std::vector<MSG> &req_buf, id_num_t id)
// get a rsp, find match in req_buf;
{
    MSG gen = rsp;
    int req_len = req_buf.size();
    for (int i = 0; i < req_len; i++)
    {
        if (rsp.id == req_buf[i].id)
        {
            if (rsp.gran == GRAN_LINE)
            { // 同时释放req和rsp;
                // 但是rsp已经在总线释放了？
                req_buf.erase(req_buf.begin() + i);
                get_rsp(gen, devs[id.to_ulong()].req_buf, id);
            }
            else
            {
                req_buf[i].ok_mask |= rsp.mask;
                if (req_buf[i].ok_mask.all()) // 收集到了所有rsp;
                {
                    gen.gran = GRAN_LINE;
                    req_buf.erase(req_buf.begin() + i);
                    get_rsp(gen, devs[id.to_ulong()].req_buf, id);
                    // 无需释放gen, 因为 DEV 和 TU 的通信不经过总线;
                }
            }
        }
    }
}

void get_msg()
{
    MSG tmp;
    tmp = bus.front();
    // for (int i = 0; i < MAX_DEVS; i++)
    {
        if (tmp.msg < RSP_S) // req or fwd
        {
            if (tmp.dest.test(0))
            {
                llc.req_buf.push_back(tmp);
                std::cout << "LLC get a req from bus---" << std::endl;
            }
            if (tmp.dest.test(1))
            {
                tus[CPU].req_buf.push_back(tmp);
                std::cout << "TU_CPU get a req from bus---" << std::endl;
            }
            if (tmp.dest.test(2))
            {
                tus[GPU].req_buf.push_back(tmp);
                std::cout << "TU_GPU get a req from bus---" << std::endl;
            }
            if (tmp.dest.test(3))
            {
                tus[ACC].req_buf.push_back(tmp);
                std::cout << "TU_ACC get a req from bus---" << std::endl;
            }
        }
        else // rsp;
        {
            // if (tmp.dest.test(0))
            //     llc.rsp_buf.push_back(tmp);
            // if (tmp.dest.test(1))
            //     tcpu.rsp_buf.push_back(tmp);
            // if (tmp.dest.test(2))
            //     tgpu.rsp_buf.push_back(tmp);
            // if (tmp.dest.test(3))
            //     tacc.rsp_buf.push_back(tmp);

            // 无需将rsp_in入队，直接对对碰消掉req和rsp_in;
            if (tmp.dest.test(0))
            {
                get_rsp(tmp, llc.req_buf, SPX);
                std::cout << "LLC get a rsp from bus---" << std::endl;
            }
            if (tmp.dest.test(1))
            {
                get_rsp(tmp, tus[CPU].req_buf, CPU);
                std::cout << "TU_CPU get a rsp from bus---" << std::endl;
            }
            if (tmp.dest.test(2))
            {
                get_rsp(tmp, tus[GPU].req_buf, GPU);
                std::cout << "TU_GPU get a rsp from bus---" << std::endl;
            }
            if (tmp.dest.test(3))
            {
                get_rsp(tmp, tus[ACC].req_buf, ACC);
                std::cout << "TU_ACC get a rsp from bus---" << std::endl;
            }
        }
    }
    bus.erase(bus.begin());
    buf_display();
}

// void put_req(std::vector<MSG> &req)
// // Put all the req_buf to the bus.
// {
//     int req_len = req.size();
//     for (int i = 0; i < req_len; i++)
//     // while (!req.empty())
//     {
//         MSG tmp;
//         tmp = req.front();
//         bus.push_back(tmp);
//         // all req stays at the vector until its rsp in;
//     }
// }
// void put_rsp_inner(std::vector<MSG> &rsp)
void put_rsp(std::vector<MSG> &rsp)
// Put all the rsp_buf to the bus.
{
    int rsp_len = rsp.size();
    for (int i = 0; i < rsp_len; i++)
    // while (!rsp.empty())
    {
        MSG tmp;
        tmp = rsp.front();
        std::cout << "PUT_RSP_HERE!!!!!!" << std::endl;
        tmp.msg_display();
        bus.push_back(tmp);
        // if (tmp.msg < FWD_REQ_S) // rsp, not fwd;
        // {
        rsp.erase(rsp.begin());
        //}
    }
    buf_display();
}

// void put_rsp(int id)
// {
//     switch (id)
//     {
//     case SPX:
//         put_rsp_inner(llc.rsp_buf);
//         std::cout << "LLC put rsp to bus---" << std::endl;
//         break;
//     case CPU:
//         put_rsp_inner(tus[CPU].rsp_buf);
//         std::cout << "TU_CPU put rsp to bus---" << std::endl;
//         break;
//     case GPU:
//         put_rsp_inner(tus[GPU].rsp_buf);
//         std::cout << "TU_GPU put rsp to bus---" << std::endl;
//         break;
//     case ACC:
//         put_rsp_inner(tus[ACC].rsp_buf);
//         std::cout << "TU_ACC put rsp to bus---" << std::endl;
//         break;
//     default:
//         break;
//     }
// }

// void get_req(std::vector<MSG> &req)
// // get a req, insert it in req_buf;
// {
//     req.push_back(bus.front()); // req_buf入队;
//     bus.erase(bus.begin());     // bus出队;
// }

// bool is_single(MSG &msg, DATA_LINE &line)
// {
//     if (msg.gran == GRAN_LINE)
//         return 1;
//     else
//     {
//         if (msg.mask.all())
//         {
//             if (line.word_state.none() || line.word_state.all())
//                 return 1;
//         }
//     }
//     return 0;
// }

void RspCoalesce(std::vector<MSG> &buf)
{
    // int len = buf.size();
    // 不应该用固定长度的len!!!!!!!!!!!!!!!!!!!!
    for (int i = 0; i < buf.size() - 1; i++)
    {
        for (int j = i + 1; j < buf.size(); j++)
        {
            if ((buf[i].id == buf[j].id) && (buf[i].dest == buf[j].dest) && (buf[i].addr == buf[j].addr) && (buf[i].msg == buf[j].msg))
            {
                buf[i].mask |= buf[j].mask;
                buf.erase(buf.begin() + j);
            }
        }
    }
    for (int i = 0; i < buf.size(); i++)
    {
        buf[i].ok_mask = ~buf[i].mask;
    }
}

void buf_brief(std::vector<MSG> &buf)
{
    int len = buf.size();
    for (int i = 0; i < len; i++)
    {
        std::cout << buf[i].id << " " << msg_which(buf[i].msg) << " ";
    }
    std::cout << std::endl;
}

void buf_display()
{
    std::cout << "-------------BUF_DISPLAY---------------" << std::endl;
    // std::cout << "--BUS--|--LLC--|--CPU--|--ACC--|--GPU--" << std::endl;
    std::cout << "--BUS--" << std::endl;
    buf_brief(bus);
    std::cout << "--LLC--" << std::endl;
    buf_brief(llc.req_buf);
    std::cout << "--CPU--" << std::endl;
    buf_brief(tus[CPU].req_buf);
    std::cout << "--GPU--" << std::endl;
    buf_brief(tus[GPU].req_buf);
    std::cout << "--ACC--" << std::endl;
    buf_brief(tus[ACC].req_buf);
    std::cout << "---------------------------------------" << std::endl;
}

void buf_detailed(std::vector<MSG> &buf)
{
    std::cout << "---------------BUF_DETAILED_INFO---------------" << std::endl;
    int len = buf.size();
    if (!len)
    {
        std::cout << "Nothing in the buffer!" << std::endl;
    }
    for (int i = 0; i < len; i++)
    {
        buf[i].msg_display();
    }
    std::cout << "-----------------------------------------------" << std::endl;
}