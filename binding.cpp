/*
    Copyright (c) 2013 250bpm s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#include "nn.hpp"

#include <nanomsg/pair.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <functional>

int a = 0;

void handle(nn::socket * s){
    while (true) {
//        try{
//            if(nn::poll(*s,0,true,false) == 0) //判断读状态
//                continue;
//            auto msg = s->recv();
            nn::msgctl ctl{};
            auto msg = s->recvmsg(&ctl);
            std::string h{"ret "};
            h += std::string((const char *)msg.buf,msg.length);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::cout << std::this_thread::get_id() << "  handle : " << h << std::endl;

            s->sendmsg(h.data(),h.size(),ctl);
//        } catch(nn::exception & e) {
//           std::cout << std::this_thread::get_id() << " out : " << e.what() << std::endl;
//        }

    }
}

void client(){
    nn::socket s2 (AF_SP, NN_REQ);
    s2.connect ("tcp://127.0.0.1:10888");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    char aa[64];
    for(int  i = 0; i < 200 ; ++i){
         ++ a;
        int ta = a;
        memset(aa,0,64);
        sprintf(aa,"hello word %d, index %d",ta,i);
        s2.send(aa,strlen(aa));
        auto m = s2.recv();
        std::cout << std::this_thread::get_id() << "  client : " << reinterpret_cast<char *>(m.buf) <<
                  "  ta = " << ta << "   i = " << i <<std::endl;
    }
     std::cout << std::this_thread::get_id()  << " end " << std::endl;
}

int main ()
{
    nn::socket s1 (AF_SP_RAW, NN_REP); //多线程需要RAW 模式
    s1.bind ("tcp://127.0.0.1:10888");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::thread t1(std::bind(handle,&s1));
    std::thread t2(std::bind(handle,&s1));
    std::thread t5(std::bind(client));
    std::thread t6(std::bind(client));
    std::thread t7(std::bind(client));
    std::thread t8(std::bind(client));
    std::thread t9(std::bind(client));

    t5.join();
    t6.join();


    std::cout << "client end!" << std::endl;
    t1.join();
//    t2.join();
    return 0;
}
