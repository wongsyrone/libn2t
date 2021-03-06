/*
 * This file is part of the libn2t project.
 * Libn2t is a C++ library transforming network layer into transport layer.
 * Copyright (C) 2018  GreaterFire
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _N2T_UTILS_H_
#define _N2T_UTILS_H_

#include <cstdio>
#include <string>

#if defined(ANDROID) || defined(__ANDROID__)
#include <android/log.h>
#define N2T_LOG(error_code)                                                     \
    do {                                                                        \
        if (error_code.value() != boost::asio::error::operation_aborted         \
            && error_code.value() != boost::asio::error::eof) {                 \
            Utils::log(__FILE__, __func__, __LINE__, error_code.message());     \
        }                                                                       \
    } while(0)
#else
#define N2T_LOG(error_code)
#endif

struct pbuf;

namespace Net2Tr {
    class Utils {
    public:
        static pbuf *str_to_pbuf(const std::string &str);
        static std::string pbuf_to_str(pbuf *p);
        static std::string addrport_to_socks5(const std::string &addr, uint16_t port);
        static int socks5_to_addrport(const std::string &socks5, std::string &addr, uint16_t &port);
        static void log(const char *file, const char *func, int line, const std::string &msg);
    };
}

#endif // _N2T_UTILS_H_
